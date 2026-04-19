// main entry for simple_http_template_hs (Phase 4 migration).

#include "fox-http/http_server.h"
#include "router.generated.h"
#include "db/db.h"

#include <cstdlib>
#include <exception>
#include <iostream>

int main() {
    constexpr unsigned short port = 19876;

    try {
        try {
            DB::instance().init();
        } catch (const std::exception& e) {
            std::cerr << "WARN: DB init failed (" << e.what()
                      << "); DB-backed routes will error at request time." << std::endl;
        }

        std::size_t io_threads = 0;  // 0 = hardware_concurrency
        if (const char* env = std::getenv("HTTP_IO_THREADS")) {
            std::size_t n = std::strtoul(env, nullptr, 10);
            if (n > 0) io_threads = n;
        }

        fox::http::HttpServer server(port, io_threads);
        Router my_router;
        server.set_handler(&my_router);

        std::cout << "simple_http_template_hs listening on port " << port << std::endl;
        return server.run();
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << std::endl;
        return -1;
    }
}
