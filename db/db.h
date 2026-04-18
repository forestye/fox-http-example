#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "yxmysql/pool.h"
#include "yxmysql/yxmysql.h"

class DB {
public:
    static DB& instance();

    // 获取一个连接（智能指针，自动归还）
    yxmysql_pool::PooledConn acquire();

    // 初始化连接池
    void init();

private:
    DB() = default;
    ~DB() = default;
    DB(const DB&) = delete;
    DB& operator=(const DB&) = delete;

    std::unique_ptr<yxmysql_pool::ConnectionPool> pool_;
    std::mutex mutex_;
};
