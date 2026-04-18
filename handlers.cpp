#include "handlers.h"

#include "httpserver/http_response.h"
#include "httpserver/http_util.h"

#include "db/db.h"
#include "db/session.h"
#include "db/user.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/uio.h>
#include <unordered_map>

using namespace std;
using httpserver::HttpResponse;

void hello(httpserver::HttpResponse& resp) {
    resp.set_status(200);
    resp.headers().content_type("text/html; charset=utf-8");
    static constexpr std::string_view body = "<html><body><h1>Hello</h1></body></html>";
    resp.headers().content_length(body.size());

    struct iovec iov[1];
    iov[0].iov_base = (void*)body.data();
    iov[0].iov_len = body.size();
    if (!resp.writev(iov, 1)) {
        std::cerr << "send body failed for /hello" << std::endl;
    }
}

void favicon(httpserver::HttpResponse& resp) {
    std::ifstream ifs("../pages/images/favicon.ico", std::ios::binary);
    if (!ifs) {
        resp.set_status(404);
        return;
    }
    std::stringstream buf;
    buf << ifs.rdbuf();
    resp.set_status(200);
    resp.headers().content_type("image/x-icon");
    resp.headers().insert("Content-Disposition", "inline; filename=\"favicon.ico\"");
    resp.set_body(buf.str());
}

void login(std::unordered_map<std::string, std::string> form, httpserver::HttpResponse& resp) {
    auto it_u = form.find("username");
    auto it_p = form.find("password");
    if (it_u == form.end() || it_p == form.end()) {
        resp.set_status(400);
        resp.headers().content_type("application/json; charset=utf-8");
        resp.set_body(R"({"error":"username and password required"})");
        return;
    }

    // Placeholder auth (same as original template): password == "password" succeeds.
    const bool ok = (it_p->second == "password");
    if (!ok) {
        resp.set_status(401);
        resp.headers().content_type("application/json; charset=utf-8");
        resp.set_body(R"({"error":"invalid credentials"})");
        return;
    }

    const std::string token = "demo-token";
    resp.set_status(200);
    resp.headers().content_type("application/json; charset=utf-8");
    resp.headers().insert("Set-Cookie",
        "session_token=" + token + "; HttpOnly; Path=/");
    resp.set_body(R"({"ok":true})");
}

Json::Value api_user_info(int64_t id) {
    auto conn = DB::instance().acquire();
    UserRepo userRepo(conn.ref());
    auto user = userRepo.get_one_by_id(id);

    Json::Value user_json;
    if (user) {
        user_json["id"] = (int64_t)user->id();
        user_json["username"] = user->username();
        user_json["email"] = user->email();
        user_json["created_at"] = user->created_at();
    } else {
        user_json["error"] = "User not found";
    }
    return user_json;
}

std::string test_string() {
    return "This is a test string response.";
}
std::string test_id(int64_t id) {
    return "This is a test string response for ID: " + std::to_string(id);
}
std::string test_dir_id(std::string dir, int64_t id) {
    return "This is a test string response for dir: " + dir + " and ID: " + std::to_string(id);
}
