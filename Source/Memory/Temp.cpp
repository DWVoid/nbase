#include <atomic>
#include <cstddef>
#include "Temp.h"
#include "Blocks.h"
#include "Synchronization/SpinLock.h"

using namespace neworld::memory;
using namespace neworld::synchronization;

namespace {
    constexpr uintptr_t g0_vacate_threshold = 2ull;
    constexpr uintptr_t g1_vacate_threshold_thread = 2ull;
    constexpr uintptr_t amd64_cpu_cache_size = 64ull;

    struct block_header {
        uint32_t count_allocated;
        block_header *next;
        alignas(amd64_cpu_cache_size) std::atomic_uint32_t count_released;
    };

    class bin final {
    public:
        // Workaround for Clang BUG
        constexpr bin() noexcept {} // NOLINT

        bin(bin &&r) noexcept
                : m_count(r.m_count), m_first(r.m_first), m_last(r.m_last) { r.reset(); }

        bin &operator=(bin &&r) noexcept {
            if (this != std::addressof(r)) {
                (m_count = r.m_count, m_first = r.m_first, m_last = r.m_last, r.reset());
            }
            return *this;
        }

        bin(const bin &) = delete;

        bin &operator=(const bin &) = delete;

        ~bin() noexcept = default;

        void add(block_header *const blk) noexcept {
            ++m_count;
            (m_first ? m_last->next : m_first) = blk;
            (m_last = blk)->next = nullptr; // Same as bellow
        }

        void merge(bin &&bin) noexcept {
            m_count += bin.m_count;
            (m_first ? m_last->next : m_first) = bin.m_first;
            (m_last = bin.m_last)->next = nullptr; // For noting the end of chain
            bin.reset();
        }

        [[nodiscard]] bin tidy() && noexcept {
            bin result{};
            for (auto x = m_first; x;) {
                const auto next = x->next;
                if (could_recycle(x)) return_block(reinterpret_cast<uintptr_t>(x)); else result.add(x);
                x = next;
            }
            reset();
            return result;
        }

        [[nodiscard]] uintptr_t size() const noexcept { return m_count; }

        [[nodiscard]] bool empty() const noexcept { return !m_count; }

    private:
        uintptr_t m_count{0};
        block_header *m_first{nullptr}, *m_last{nullptr};

        void reset() noexcept {
            m_count = 0;
            m_first = m_last = nullptr;
        }

        static bool could_recycle(const block_header *const blk) noexcept {
            return blk->count_allocated == blk->count_released.load();
        }
    };

    class central final {
    public:
        ~central() noexcept { collect(); }

        void collect() noexcept { return (collect_g1(), collect_eden()); }

        void merge(bin &&bin) noexcept { merge_g1(std::move(bin)); }

        static central &instance() noexcept {
            static central instance{};
            return instance;
        }

    private:
        bin m_bin_g1, m_bin_eden;
        spin_lock m_g1_lock, m_eden_lock;
        uintptr_t m_g1_vacate_threshold = g1_vacate_threshold_thread * std::thread::hardware_concurrency();

        bin vacate_check() noexcept { return m_bin_g1.size() >= m_g1_vacate_threshold ? std::move(m_bin_g1) : bin(); }

        void collect_g1() noexcept {
            m_g1_lock.lock();
            auto bin = std::move(m_bin_g1);
            m_g1_lock.unlock();
            merge_eden(std::move(bin));
        }

        void collect_eden() noexcept {
            m_eden_lock.lock();
            auto bin = std::move(m_bin_eden);
            m_eden_lock.unlock();
            merge_eden(std::move(bin));
        }

        void merge_g1(bin &&bin) noexcept {
            auto tidy = std::move(bin).tidy();
            if (!tidy.empty()) {
                m_g1_lock.lock();
                m_bin_g1.merge(std::move(tidy));
                auto vacated = vacate_check();
                m_g1_lock.unlock();
                if (!vacated.empty()) merge_eden(std::move(vacated));
            }
        }

        void merge_eden(bin &&bin) noexcept {
            auto tidy = std::move(bin).tidy();
            if (!tidy.empty()) {
                std::lock_guard scope{m_eden_lock};
                m_bin_eden.merge(std::move(tidy));
            }
        }
    };

    class g0 {
    public:
        ~g0() noexcept { collect(); }

        void enter(const uintptr_t blk) noexcept {
            push(reinterpret_cast<block_header *>(blk));
            collect_on_demand();
        }

        void collect() noexcept { vacate(); }

    private:
        bin m_bin;

        void push(block_header *blk) noexcept { m_bin.add(blk); }

        void collect_on_demand() noexcept { if (m_bin.size() >= g0_vacate_threshold) vacate(); }

        void vacate() noexcept { central::instance().merge(std::move(m_bin)); }
    };

    class thread_allocator final {
    public:
        thread_allocator() noexcept { setup_block(); }

        thread_allocator(thread_allocator &&) = delete;

        thread_allocator(const thread_allocator &) = delete;

        thread_allocator &operator=(thread_allocator &&) = delete;

        thread_allocator &operator=(const thread_allocator &) = delete;

        ~thread_allocator() noexcept { m_g0.enter(m_block); }

        void collect() noexcept { m_g0.collect(); }

        void *do_allocate(uintptr_t size) noexcept {
            static constexpr auto align_mask = alignof(std::max_align_t) - 1;
            static constexpr auto align_rev = ~align_mask;
            if (size & align_mask) size = (size & align_rev) + alignof(std::max_align_t);
            for (;;) {
                if (const auto new_head = m_head + size; new_head >= memory_block_size) {
                    swap_block();
                } else {
                    mark(m_block);
                    const auto res = m_block + m_head;
                    m_head = new_head;
                    return reinterpret_cast<void *>(res);
                }
            }
        }

        static thread_allocator &instance() noexcept {
            static thread_local const auto trd = std::make_unique<thread_allocator>();
            return *trd;
        }

    private:
        uintptr_t m_block{}, m_head{};
        g0 m_g0;

        static constexpr uintptr_t alloc_start = amd64_cpu_cache_size * 2;

        static uintptr_t obtain() noexcept { return neworld::memory::fetch_block(); }

        static void mark(const uintptr_t blk) noexcept { ++reinterpret_cast<block_header *>(blk)->count_allocated; }

        void setup_block() noexcept { (m_block = obtain(), m_head = alloc_start); }

        void swap_block() noexcept { return (m_g0.enter(m_block), setup_block()); }
    };

    class temp_source : public std::pmr::memory_resource {
    public:
        static temp_source &instance() noexcept {
            static temp_source instance{};
            return instance;
        }

    private:
        temp_source(): m_sweeper([this] {
            while (m_running) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                central::instance().collect();
            }
        }) {}

        ~temp_source() override {
            m_running = false;
            if (m_sweeper.joinable()) m_sweeper.join();
        }

        static constexpr uintptr_t alloc_threshold = 1u << 18u;

        std::thread m_sweeper;
        bool m_running = true;
        memory_resource &default_resource = *std::pmr::get_default_resource();

        void *do_allocate(size_t bytes, size_t align) override {
            if (align > alignof(std::max_align_t) || bytes > alloc_threshold) {
                return default_resource.allocate(bytes, align);
            } else thread_allocator::instance().do_allocate(bytes);
        }

        void do_deallocate(void *ptr, size_t bytes, size_t align) override {
            if (align <= alignof(std::max_align_t) && bytes <= alloc_threshold) {
                static constexpr uintptr_t align_rev = 0b11'1111'1111'1111'1111'1111;
                static constexpr uintptr_t align_mask = ~align_rev;
                const auto block = reinterpret_cast<block_header *>((reinterpret_cast<uintptr_t>(ptr) & align_mask));
                block->count_released.fetch_add(1);
            } else default_resource.deallocate(ptr, bytes, align);
        }

        [[nodiscard]] bool do_is_equal(const memory_resource &r) const noexcept override { return &r == &instance(); }
    };
}

namespace neworld::memory {
    std::pmr::memory_resource &temp_resource() noexcept { return temp_source::instance(); }
}