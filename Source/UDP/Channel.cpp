#include <boost/crc.hpp>
#if __has_include(<Windows.h>)
#define _WIN32_WINNT 0x0601
#endif
#include <boost/asio.hpp>
#include <vector>

using namespace boost;

namespace neworld::network {
    class udp_channel {
    public:
        asio::awaitable<void> echo() {
            try {
                char data[1024];
                for (;;) {
                    const auto n = co_await m_socket.async_receive_from(
                            asio::buffer(data),
                            m_endpoint,
                            asio::use_awaitable
                    );
                    co_await m_socket.async_send_to(
                            asio::buffer(data, n),
                            m_endpoint,
                            asio::use_awaitable
                    );
                }
            }
            catch (std::exception &e) {
                std::printf("echo Exception: %s\n", e.what());
            }
        }
    private:
        asio::ip::udp::endpoint m_endpoint;
        asio::ip::udp::socket m_socket;
    };
}