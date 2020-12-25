#include <thread>
#include <memory_resource>
#include <boost/context/fiber.hpp>
#include <thread>
#include <vector>

namespace neworld::scheduler {
    // priority design: each priority level has a base level, +1 and -1.
    // with an IDLE level, we have 16 priority levels in total

    enum class base_priority {
        background, below, normal, high, realtime
    };

    class worker {

    };

    class worker_pool {
    public:
    private:
        std::vector<std::unique_ptr<worker>> m_workers;
    };
}