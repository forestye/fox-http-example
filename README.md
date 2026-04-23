# fox-http-example

把 [fox-http](https://github.com/forestye/fox-http) + [fox-route](https://github.com/forestye/fox-route)
\+ [fox-page](https://github.com/forestye/fox-page) 组合起来写一个小型 web 应用
的端到端示例。本项目同时是 fox 生态的回归测试床——文档里讲的每条路由、
每种响应模式、每个 CMake 集成点，都以这里的实际代码为准。

**里面有什么：**

- 12 条路由，覆盖静态、动态参数、catchall、FILESYSTEM 静态文件、JSON API、
  HTML 页面、表单 POST
- 两个 HTML 模板（首页 + 用户详情）用 fox-page 编译成零拷贝 `writev` 渲染函数
- MySQL 连接池（经 yxmysql）+ 用户/会话两张表 + 简单 Repo 类
- Apache 2.0

**不在这里（故意的）：**

- 鉴权只是个占位（密码 `"password"` 就通过）
- 错误处理只演示路径，生产应用需要自己补全

---

## 依赖

- **C++17** 编译器
- **CMake** 3.14+
- **fox-http / fox-route / fox-page**（相邻目录 `../fox-http`、`../fox-route`、
  `../fox-page`）
- **yxmysql** + **libmysqlclient**（数据库路由用到；无此环境时 DB 初始化失败
  会降级为警告，非 DB 路由仍可用）
- **jsoncpp**（`libjsoncpp-dev`）
- **gflags**（`libgflags-dev`）

典型 Ubuntu/Debian：

```bash
sudo apt install cmake libjsoncpp-dev libgflags-dev libmysqlclient-dev
# yxmysql: https://github.com/forestye/yxmysql 自行构建安装
```

---

## 构建与运行

目录结构预期（三个 fox-* 仓和本仓平级）：

```
code/
├── fox-http/       # 先 cmake --build build
├── fox-route/      # 先 cmake --build build
├── fox-page/       # 先 cmake --build build
└── fox-http-example/
```

```bash
git clone git@github.com:forestye/fox-http-example.git
cd fox-http-example
cmake -S . -B build
cmake --build build -j
./build/fox-http-example
# → fox-http-example listening on port 19876
```

另起终端 curl 试试：

```bash
curl -i http://127.0.0.1:19876/hello                         # Buffered 短 API
curl    http://127.0.0.1:19876/                              # weave 生成的 index 页
curl    http://127.0.0.1:19876/user/42                       # 动态参 (若无 DB 会返回 500)
curl    http://127.0.0.1:19876/test/alpha/string/7           # 两个路径参数
curl -X POST -d "username=x&password=password" http://127.0.0.1:19876/login
curl -i http://127.0.0.1:19876/css/styles.css                # FILESYSTEM 静态文件
```

---

## 路由清单（`routes.crdl`）

```crdl
GET  /hello -> hello(resp)
GET  /favicon.ico -> favicon(resp)
GET  / -> index(resp)
GET  /user/{int:id} -> user(resp, id)
POST /login -> login(form, resp)

FILESYSTEM /css -> ../pages/css
FILESYSTEM /images -> ../pages/images
FILESYSTEM /upload -> ../pages/upload

GET /userinfo/{int:id} -> json api_user_info(id)
GET /test/string -> text test_string()
GET /test/string/{int:id} -> text test_id(id)
GET /test/{string:dir}/string/{int:id} -> text test_dir_id(dir, id)
```

从这里就能看到 fox-route 的主要语法：静态路径、`{int:id}` 动态参数、
FILESYSTEM 静态映射、`text`/`json` 返回类型自动序列化。

---

## 代码地图

| 文件 | 角色 |
|---|---|
| `test.cpp` | `main()`——DB 初始化（非致命）+ `fox::http::HttpServer` 启动 |
| `handlers.cpp` | `hello` / `favicon` / `login` / `api_user_info` / `test_*` 这些 handler 的实际实现。`index` / `user` 由 fox-page 从 HTML 模板生成 |
| `routes.crdl` | 路由定义，fox-route 消费 |
| `pages/index.html`、`pages/user.html` | fox-page 的输入模板，构建期编译为 C++ 渲染函数 |
| `pages/css/`、`pages/images/`、`pages/upload/` | FILESYSTEM 静态文件 |
| `db/db.{h,cpp}` | yxmysql 连接池单例 |
| `db/user.{h,cpp}` | `UserRepo`，通过 ID 查 user |
| `db/session.{h,cpp}` | `SessionRepo`，会话 token 管理 |
| `db/tables.sql` | schema 参考 |
| `CMakeLists.txt` | 通过 `add_subdirectory(../fox-http)` 链 fox-http，`find_program` 定位 fox-route/fox-page；自定义命令触发 `.crdl`→C++ 和 `.html`→C++ 代码生成 |

构建时 CMake 会在 `build/` 下生成：

- `handlers.h`、`router.generated.h`、`router.generated.cpp`（由 fox-route 产出）
- `pages/index.cpp`、`pages/user.cpp`（由 fox-page 产出）
- 最终链接出可执行文件 `build/fox-http-example`

---

## 连接 MySQL（可选）

DB 路由 (`/user/{id}`、`/userinfo/{id}`) 需要真实 MySQL。其它路由不需要。

### 1. 起一个本地 MySQL

以 Docker 为例：

```bash
docker run --name fox-mysql -e MYSQL_ROOT_PASSWORD=rootpw -p 3306:3306 -d mysql:8
```

### 2. 建库建表建用户

```bash
mysql -h 127.0.0.1 -uroot -prootpw <<'SQL'
CREATE DATABASE simple_http;
CREATE USER 'simple_http'@'%' IDENTIFIED BY 'dbpassexample';
GRANT ALL ON simple_http.* TO 'simple_http'@'%';
USE simple_http;
SQL
mysql -h 127.0.0.1 -usimple_http -pdbpassexample simple_http < db/tables.sql
mysql -h 127.0.0.1 -usimple_http -pdbpassexample simple_http \
    -e "INSERT INTO user (username, password_hash, email) VALUES ('alice', 'xxx', 'alice@example.com');"
```

### 3. 重跑服务

起起来后 `curl http://127.0.0.1:19876/userinfo/1` 返回 JSON：

```json
{"id":1,"username":"alice","email":"alice@example.com","created_at":"..."}
```

> 账号密码硬编码在 `db/db.cpp`：`simple_http` / `dbpassexample` /
> `localhost:3306` / `simple_http`。想改到别的库，直接改那几个字面量。

---

## 学完这个示例你能学到的

1. **怎么把 fox-* 三件套串起来**：CMakeLists.txt 里 `add_subdirectory(../fox-http)`
   + `find_program(fox-route fox-route-func fox-page)` + `add_custom_command`
   自动生成 Router 和页面 C++。
2. **路由如何定义**：`routes.crdl` 里声明，生成器负责做参数解析、返回值包装。
3. **业务 handler 怎么写**：文本 / HTML / JSON 三种返回类型，分别返回 `std::string` /
   `std::string` / `Json::Value`；需要原始 `HttpResponse` 的手动控制时参数里加 `resp`。
4. **HTML 模板怎么和 handler 参数接上**：fox-route 的 `fox-route-func` 抽出
   handler 签名喂给 fox-page 的 `--func`，模板渲染函数和路由签名天然一致。
5. **FILESYSTEM 路由**：静态资源一行声明映射到本地目录。
6. **DB 连接池 + Repo 模式**：`db.{h,cpp}` 单例 + `*_repo.h` 的连接借用模式。

---

## 遇到问题

- **`fox-route not found — build fox-route first.`**：先去 `../fox-route`
  `cmake --build build`，fox-route 的可执行会落在 `../fox-route/build/fox-route`。
  fox-page、fox-http 同理。
- **`DB pool not initialized` 在所有 DB 路由**：本机没 MySQL 或账号不对。
  非 DB 路由（`/hello`、`/test/*`、FILESYSTEM）仍然可用，test.cpp 里 DB
  初始化失败是 warning，不影响进程启动。
- **端口 19876 被占**：改 `test.cpp` 里的 `constexpr unsigned short port = 19876;`
  或把硬编码改成从 argv 读。
- **`/css/styles.css` 返回 404**：启动目录影响 FILESYSTEM 的相对路径
  （`../pages/css`）。从 `build/` 目录下跑最稳（`cd build && ./fox-http-example`）。

---

## License

Apache 2.0，见 [LICENSE](LICENSE)（如缺失则与上游 fox-http 一致）。
