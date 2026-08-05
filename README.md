# fox-http-example

把 [fox-http](https://github.com/forestye/fox-http) + [fox-route](https://github.com/forestye/fox-route)
\+ [fox-page](https://github.com/forestye/fox-page) 组合起来写一个小型 web 应用
的端到端示例。本项目同时是 fox 生态的回归测试床——文档里讲的每条路由、
每种响应模式、每个 CMake 集成点，都以这里的实际代码为准。

**里面有什么：**

- 13 条路由，覆盖静态、动态参数、catchall、FILESYSTEM 静态文件、JSON API、
  HTML 页面、表单 POST
- 三个 HTML 模板（首页 + 用户列表 + 用户详情）用 fox-page 编译成零拷贝
  `writev` 渲染函数；其中用户列表页演示 `cpp-for` 在 MySQL 结果集上迭代
- MySQL 连接池（经 fox-mysql）+ 用户/会话两张表 + 简单 Repo 类
- 冒烟回归 `tests/smoke.sh`（已接入 ctest）：每类路由、模板插值、HTML 转义
  各有断言，见 [回归测试](#回归测试)
- Apache 2.0

**不在这里（故意的）：**

- 鉴权只是个占位（密码 `"password"` 就通过）
- 错误处理只演示路径，生产应用需要自己补全
- 请求体没有大小上限（fox-http 目前按 Content-Length 全量读入内存），公网
  部署必须前置 nginx 并设置 `client_max_body_size`

---

## 依赖

- **C++17** 编译器
- **CMake** 3.14+
- **fox-http / fox-route / fox-page** 已安装到系统（默认 `/usr/local`）。
  `fox-http` 走 `find_package(fox-http)`，`fox-route` / `fox-route-func` /
  `fox-page` 三个可执行通过 `find_program` 在 PATH 上查找。
- **fox-mysql** + **libmysqlclient**（数据库路由用到；无此环境时 DB 初始化失败
  会降级为警告，非 DB 路由仍可用）
- **jsoncpp**（`libjsoncpp-dev`）—— 走 `find_package(jsoncpp)`

典型 Ubuntu/Debian：

```bash
sudo apt install cmake libjsoncpp-dev libmysqlclient-dev pkg-config
# fox-http / fox-route / fox-page / fox-mysql：分别 clone 后
#   cmake -S . -B build && cmake --build build -j && sudo cmake --install build
# 装到 /usr/local 之后本项目就能直接 find_package / find_program 找到。
```

---

## 构建与运行

前提：fox-http / fox-route / fox-page / fox-mysql 已安装到系统目录
（见上面 [依赖](#依赖) 一节）。验证一下：

```bash
which fox-route fox-page                     # 应在 /usr/local/bin
ls /usr/local/lib/cmake/fox-http             # 应有 fox-httpConfig.cmake
ls /usr/local/lib/cmake/fox-mysql            # 应有 fox-mysqlConfig.cmake
```

然后：

```bash
git clone git@github.com:forestye/fox-http-example.git
cd fox-http-example
cmake -S . -B build
cmake --build build -j
cd build && ./fox-http-example
# → fox-http-example listening on port 19876
```

注意最后一步是在 `build/` 目录里启动：FILESYSTEM 路由映射的 `../pages/*`
（以及 favicon 的读取路径）都按**进程当前目录**解析，从别的目录启动会拿到
404（见 [遇到问题](#遇到问题)）。

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

## 回归测试

`tests/smoke.sh` 起一个真实服务进程，对 README 里出现的每类路由 curl 一遍并
校验响应内容——包括模板文本/属性插值渲出的是求值结果而非字面
`<< (expr) <<`，以及用户数据里的 `<script>` 到页面上必须已转义
（见 [模板与转义](#模板与转义)）。已接入 ctest：

```bash
cd build && ctest --output-on-failure
```

本机 127.0.0.1:3306 上有 [下文](#连接-mysql可选) 所建的 MySQL 时，DB 路由与
转义回归一并校验（会临时插入并删除一行测试用户）；没有则这部分自动 SKIP，
其余断言照跑。约定：**README 中声称的行为，这个脚本里应有对应断言**——
发现文档与实现脱节，修哪边都行，但要让断言先红后绿。

---

## 模板与转义

fox-page（`ff41757` 起）对动态输出**默认 HTML 转义**：`{{expr}}`、`cpp-text`、
`cpp:<attr>` 与属性值内的 `{{}}`，输出前都会转义 `& < > " '` 五个字符。
所以本项目的模板直接写 `cpp-text="u.username()"` 渲染用户数据即可，
**不要在应用层再自行转义**——那会双重转义，页面上 `&` 显示成 `&amp;`。

需要输出受信任的原始 HTML 时用逃生口：文本节点写三括号 `{{{expr}}}`，
元素内容写 `cpp-html="expr"`；属性值内不提供 raw 豁免。本项目目前没有
这类插值点。

`tests/smoke.sh` 里有双向回归：插入用户名带 `<script>` 的行，断言页面上
渲成 `&lt;script&gt;`（默认转义生效），同时断言页面上没有 `&amp;lt;`
（应用层没有多套一层转义）。

---

## 路由清单（`routes.crdl`）

```crdl
GET  /hello -> hello(resp)
GET  /favicon.ico -> favicon(resp)
GET  / -> index(resp)
GET  /users -> users(resp)
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
| `handlers.cpp` | `hello` / `favicon` / `login` / `api_user_info` / `test_*` 这些 handler 的实际实现。`index` / `users` / `user` 由 fox-page 从 HTML 模板生成 |
| `routes.crdl` | 路由定义，fox-route 消费 |
| `pages/index.html`、`pages/users.html`、`pages/user.html` | fox-page 的输入模板，构建期编译为 C++ 渲染函数 |
| `tests/smoke.sh` | 冒烟回归脚本，`ctest` 调用（见 [回归测试](#回归测试)） |
| `pages/css/`、`pages/images/`、`pages/upload/` | FILESYSTEM 静态文件 |
| `db/db.{h,cpp}` | fox-mysql 连接池单例 |
| `db/user.{h,cpp}` | `UserRepo`，通过 ID 查 user |
| `db/session.{h,cpp}` | `SessionRepo`，会话 token 管理 |
| `db/tables.sql` | schema 参考 |
| `CMakeLists.txt` | `find_package(fox-http / fox-mysql / jsoncpp)` 引入库依赖，`find_program` 在 PATH 上定位 fox-route/fox-route-func/fox-page；自定义命令触发 `.crdl`→C++ 和 `.html`→C++ 代码生成 |

构建时 CMake 会在 `build/` 下生成：

- `handlers.h`、`router.generated.h`、`router.generated.cpp`（由 fox-route 产出）
- `pages/index.cpp`、`pages/users.cpp`、`pages/user.cpp`（由 fox-page 产出）
- 最终链接出可执行文件 `build/fox-http-example`

---

## 连接 MySQL（可选）

DB 相关路由 (`/users`、`/user/{id}`、`/userinfo/{id}`) 需要真实 MySQL。
其它路由不需要。

### 1. 起一个本地 MySQL（已有 MySQL 跳过）

以 Docker 为例：

```bash
docker run --name fox-mysql -e MYSQL_ROOT_PASSWORD=rootpw -p 3306:3306 -d mysql:8
```

### 2. 建库 / 建用户 / 建表 / 写示例数据

下面这块 SQL 是自包含的，整块拷进有权限的客户端（如 root）执行即可。
账号密码与 `db/db.cpp` 里硬编码的一致 (`simple_http` / `dbpassexample`)；
想改库名密码记得两处一起改。

```sql
-- 1) 库 + 账户
--    默认只建 'simple_http'@'localhost'：本机直连即可，攻击面最小。
--    需要 Docker / 远端机器连过来时再单独 `CREATE USER ... @'%'`，
--    并把那条单独 `GRANT`。不要图省事直接全开 '%'。
CREATE DATABASE IF NOT EXISTS simple_http
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;

CREATE USER IF NOT EXISTS 'simple_http'@'localhost' IDENTIFIED BY 'dbpassexample';
GRANT ALL ON simple_http.* TO 'simple_http'@'localhost';
FLUSH PRIVILEGES;

USE simple_http;

-- 2) 表结构（与 db/tables.sql 一致）
CREATE TABLE IF NOT EXISTS user (
    id            INTEGER PRIMARY KEY AUTO_INCREMENT  COMMENT '用户ID',
    username      VARCHAR(64)  NOT NULL UNIQUE        COMMENT '用户名',
    password_hash VARCHAR(128) NOT NULL               COMMENT '密码哈希',
    email         VARCHAR(128)                        COMMENT '邮箱',
    created_at    DATETIME DEFAULT CURRENT_TIMESTAMP  COMMENT '创建时间'
);

CREATE TABLE IF NOT EXISTS session (
    id         INTEGER PRIMARY KEY AUTO_INCREMENT  COMMENT '会话ID',
    user_id    INTEGER NOT NULL                    COMMENT '用户ID',
    token      VARCHAR(128) NOT NULL UNIQUE        COMMENT '会话令牌',
    expires_at DATETIME NOT NULL                   COMMENT '过期时间',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP  COMMENT '创建时间'
);

-- 3) 几条示例用户，让 /users 列表页不为空
INSERT INTO user (username, password_hash, email) VALUES
    ('alice',   'placeholder', 'alice@example.com'),
    ('bowen',   'placeholder', 'bowen@example.com'),
    ('cyril',   'placeholder', 'cyril@example.com'),
    ('dimitri', 'placeholder', 'dimitri@example.com'),
    ('elara',   'placeholder', 'elara@example.com');
```

一行 shell 执行（保存为 `setup.sql` 然后 pipe 进去）：

```bash
mysql -h 127.0.0.1 -uroot -prootpw < setup.sql
```

### 3. 验证

服务启动后：

```bash
curl -s http://127.0.0.1:19876/userinfo/1
# → {"id":1,"username":"alice","email":"alice@example.com","created_at":"..."}

curl -s http://127.0.0.1:19876/users | grep -oE 'user-card__name">[^<]+' | head
# → user-card__name">alice / user-card__name">bowen / ...
```

> 想改到别的库或换密码，直接改 `db/db.cpp` 里那几个字面量；或者在 `init()`
> 里改成读环境变量。

---

## 学完这个示例你能学到的

1. **怎么把 fox-* 三件套串起来**：CMakeLists.txt 里 `find_package(fox-http)`
   引入库 + `find_program(fox-route / fox-route-func / fox-page)` 找代码生成器
   + `add_custom_command` 自动生成 Router 和页面 C++。
2. **路由如何定义**：`routes.crdl` 里声明，生成器负责做参数解析、返回值包装。
3. **业务 handler 怎么写**：文本 / HTML / JSON 三种返回类型，分别返回 `std::string` /
   `std::string` / `Json::Value`；需要原始 `HttpResponse` 的手动控制时参数里加 `resp`。
4. **HTML 模板怎么和 handler 参数接上**：fox-route 的 `fox-route-func` 抽出
   handler 签名喂给 fox-page 的 `--func`，模板渲染函数和路由签名天然一致。
5. **FILESYSTEM 路由**：静态资源一行声明映射到本地目录（相对路径按进程
   cwd 解析）。
6. **DB 连接池 + Repo 模式**：`db.{h,cpp}` 单例 + `*_repo.h` 的连接借用模式。
7. **模板转义语义**：fox-page 默认转义动态输出，受信任的原始 HTML 走
   `{{{expr}}}` / `cpp-html` 逃生口（见 [模板与转义](#模板与转义)）。

---

## 遇到问题

- **`fox-route not found — install fox-route to a directory on PATH.`**：去
  fox-route 仓库 `cmake -S . -B build && cmake --build build -j && sudo
  cmake --install build`，可执行会落在 `/usr/local/bin/fox-route`。
  fox-page 同理。fox-http 是装库，报错信息会是 `fox-http install not found`，
  到 fox-http 仓库做同样的 `cmake --install` 即可。
- **`DB pool not initialized` 在所有 DB 路由**：本机没 MySQL 或账号不对。
  非 DB 路由（`/hello`、`/test/*`、FILESYSTEM）仍然可用，test.cpp 里 DB
  初始化失败是 warning，不影响进程启动。
- **`Access denied for user 'simple_http'@'localhost'`**：MySQL 把
  `'name'@'localhost'` 与 `'name'@'%'` 视为两个不同的账户匹配项，本机直连
  通常按 `localhost` 解析、不会自动回退到 `'%'`。如果之前只建了 `@'%'`，
  补一条 `@'localhost'` 即可。下面这段是幂等的——`@'localhost'` 不存在
  就建，存在就重置成已知状态，不会撞 `ERROR 1396 (HY000): Operation
  CREATE USER failed`：
  ```sql
  DROP USER IF EXISTS 'simple_http'@'localhost';
  CREATE USER 'simple_http'@'localhost' IDENTIFIED BY 'dbpassexample';
  GRANT ALL ON simple_http.* TO 'simple_http'@'localhost';
  FLUSH PRIVILEGES;
  ```
  想先看一眼当前账户情况：
  ```sql
  SELECT user, host FROM mysql.user WHERE user='simple_http';
  ```
- **从 Docker / 远端机器连过来又被拒**：那种场景源 IP 不是 `localhost`，
  需要单独再建 `'simple_http'@'%'`（或具体的源 IP）账户并 GRANT。但
  `@'%'` 等于"任意 IP 都允许"，仅在受信网络下使用，避免直接做默认账户。
- **端口 19876 被占**：改 `test.cpp` 里的 `constexpr unsigned short port = 19876;`
  或把硬编码改成从 argv 读。
- **`/css/styles.css` 返回 404**：启动目录影响 FILESYSTEM 的相对路径
  （`../pages/css`）。从 `build/` 目录下跑最稳（`cd build && ./fox-http-example`）。

---

## License

Apache 2.0，见 [LICENSE](LICENSE)（如缺失则与上游 fox-http 一致）。
