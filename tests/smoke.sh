#!/usr/bin/env bash
# fox-http-example 冒烟回归:起服务,对每类路由 curl 一遍,校验渲染输出。
#
# 用法:
#   tests/smoke.sh <path-to-binary>
# 工作目录必须是 build/(FILESYSTEM 的 ../pages/* 相对路径按进程 cwd 解析)。
# 通过 ctest 跑则两者都已安顿好:cd build && ctest --output-on-failure
#
# MySQL 可选:127.0.0.1:3306 上有 README 所建的 simple_http 库时,连同
# /users、/user/{id}、/userinfo/{id} 与 HTML 转义一起校验;没有则 SKIP。

set -u

BIN=${1:-./fox-http-example}
PORT=19876
BASE=http://127.0.0.1:$PORT
LOG=smoke-server.log

if [ ! -x "$BIN" ]; then
    echo "FATAL: 找不到可执行文件 $BIN(先构建,再从 build/ 目录运行本脚本)" >&2
    exit 2
fi

if curl -s -o /dev/null --max-time 1 "$BASE/hello"; then
    echo "FATAL: 端口 $PORT 已被占用,先停掉旧进程再跑" >&2
    exit 2
fi

"$BIN" >"$LOG" 2>&1 &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null; wait "$SERVER_PID" 2>/dev/null' EXIT

up=0
for _ in $(seq 1 50); do
    if curl -s -o /dev/null "$BASE/hello"; then up=1; break; fi
    sleep 0.1
done
if [ "$up" != 1 ]; then
    echo "FATAL: 服务 3 秒内未就绪,日志如下:" >&2
    cat "$LOG" >&2
    exit 2
fi

fail=0
pass=0
skip=0

# assert_contains <名称> <路径> <期望子串>
assert_contains() {
    local name=$1 path=$2 want=$3 body
    body=$(curl -s "$BASE$path")
    if [[ "$body" == *"$want"* ]]; then
        pass=$((pass+1)); echo "PASS  $name"
    else
        fail=$((fail+1)); echo "FAIL  $name — GET $path 输出中未找到: $want"
    fi
}

# assert_absent <名称> <路径> <禁止子串>
assert_absent() {
    local name=$1 path=$2 bad=$3 body
    body=$(curl -s "$BASE$path")
    if [[ "$body" != *"$bad"* ]]; then
        pass=$((pass+1)); echo "PASS  $name"
    else
        fail=$((fail+1)); echo "FAIL  $name — GET $path 输出中出现了不该有的: $bad"
    fi
}

# assert_status <名称> <curl 参数...> <期望状态码>
assert_status() {
    local name=$1; shift
    local want=${!#} code
    code=$(curl -s -o /dev/null -w '%{http_code}' "${@:1:$#-1}")
    if [ "$code" = "$want" ]; then
        pass=$((pass+1)); echo "PASS  $name"
    else
        fail=$((fail+1)); echo "FAIL  $name — 期望 HTTP $want,实得 $code"
    fi
}

# ── 无 DB 依赖的路由 ─────────────────────────────────────────
assert_contains "hello 手写 writev"        /hello                "<h1>Hello</h1>"
assert_contains "text 无参"                /test/string          "This is a test string response."
assert_contains "text int 参数"            /test/string/7        "ID: 7"
assert_contains "text string+int 参数"     /test/alpha/string/7  "dir: alpha and ID: 7"
assert_contains "index 模板渲出"           /                     "hero__title"
assert_contains "index 文本插值({{datestr}})" /                  "年"
assert_absent   "index 插值未劣化为字面量" /                     '<< (datestr) <<'
assert_status   "FILESYSTEM css"           "$BASE/css/styles.css" 200
assert_status   "login 缺参数 400"         -X POST -d "username=x" "$BASE/login" 400
assert_status   "login 错口令 401"         -X POST -d "username=x&password=wrong" "$BASE/login" 401
assert_status   "login 对口令 200"         -X POST -d "username=x&password=password" "$BASE/login" 200

# ── DB 依赖的路由(无 MySQL 时跳过)─────────────────────────
db_ok=0
if [[ $(curl -s "$BASE/userinfo/1") == *'"username"'* ]]; then
    db_ok=1
fi

if [ "$db_ok" = 1 ]; then
    assert_contains "JSON api /userinfo/1"     /userinfo/1  '"username"'
    # 属性内插值回归:href="/user/{{u.id()}}" 必须渲成真实 id,
    # 而不是字面 " << (u.id()) << "(fox-page e245c73 前的 bug)
    assert_contains "users 属性插值 href"      /users       'href="/user/1"'
    assert_absent   "users 属性插值未劣化"     /users       '<< (u.id()) <<'
    assert_contains "users cpp-text 用户名"    /users       'alice'
    assert_contains "user 详情页 cpp-text"     /user/1      'alice'
    assert_contains "user 详情页 404 分支"     /user/999999 '查无此人'

    # HTML 转义回归(双向):插一行带 <script> 的用户名,页面上必须
    # 渲成 &lt;script&gt;(fox-page 默认转义生效),同时不得出现
    # &amp;lt;(应用层不要再套一层转义,否则双重转义)。
    if command -v mysql >/dev/null 2>&1; then
        MYSQL=(mysql -h 127.0.0.1 -u simple_http -pdbpassexample simple_http -N -s)
        "${MYSQL[@]}" -e "INSERT INTO user (username, password_hash, email) VALUES ('smoke<script>t', 'x', 'smoke@e.com<b>');" 2>/dev/null
        xss_id=$("${MYSQL[@]}" -e "SELECT id FROM user WHERE username = 'smoke<script>t';" 2>/dev/null)
        if [ -n "$xss_id" ]; then
            assert_contains "users 列表转义 <script>"  /users          'smoke&lt;script&gt;t'
            assert_absent   "users 列表无原样 <script>" /users         'smoke<script>t'
            assert_absent   "users 列表无双重转义"     /users          '&amp;lt;'
            assert_contains "user 详情转义 <script>"   "/user/$xss_id" 'smoke&lt;script&gt;t'
            assert_absent   "user 详情无双重转义"      "/user/$xss_id" '&amp;lt;'
            "${MYSQL[@]}" -e "DELETE FROM user WHERE id = $xss_id;" 2>/dev/null
        else
            skip=$((skip+1)); echo "SKIP  HTML 转义回归 — 测试行插入失败"
        fi
    else
        skip=$((skip+1)); echo "SKIP  HTML 转义回归 — 本机无 mysql 客户端"
    fi
else
    skip=$((skip+1)); echo "SKIP  DB 相关路由(/users /user/{id} /userinfo/{id})— MySQL 不可达"
fi

echo "----------------------------------------"
echo "通过 $pass,失败 $fail,跳过 $skip"
[ "$fail" = 0 ]
