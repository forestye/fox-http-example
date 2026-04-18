#include "db.h"

DB& DB::instance() {
    static DB inst;
    return inst;
}

void DB::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    yxmysql::ConnectionConfig config;
    config.host = "127.0.0.1";
    config.port = 3306;
    config.user = "simple_http";
    config.password = "dbpassexample";
    config.database = "simple_http";
    config.charset = "utf8mb4";
    config.connect_timeout = std::chrono::seconds{10};
    config.read_timeout = std::chrono::seconds{30};
    config.write_timeout = std::chrono::seconds{30};
    config.auto_reconnect = true;
    config.multi_statements = false;

    yxmysql_pool::PoolOptions opts;
    opts.min_size = 2;
    opts.max_size = 16;
    opts.acquire_timeout = std::chrono::seconds(5);

    pool_ = std::make_unique<yxmysql_pool::ConnectionPool>(config, opts);
}

yxmysql_pool::PooledConn DB::acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!pool_) throw std::runtime_error("DB pool not initialized");
    return pool_->acquire();
}
