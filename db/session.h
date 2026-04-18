#pragma once
#include <string>
#include <cstdint>
#include <optional>
#include <vector>
#include "yxmysql/yxmysql.h"

class Session {
public:

  // 默认构造函数
  Session() = default;
  
  // 通过 ResultSet 构造 Session
  Session(yxmysql::ResultSet& rs) {
    id_ = rs.get_int64(0);
    user_id_ = rs.get_int64(1);
    token_ = rs.get_string(2);
    expires_at_ = rs.get_string(3);
    created_at_ = rs.get_string(4);
    clear_changes();
  }

  long long id() const { return id_; }
  void set_id(long long v) { id_ = v; }

  long long user_id() const { return user_id_; }
  void set_user_id(long long v) { user_id_ = v; user_id_changed_ = true; }

  const std::string& token() const { return token_; }
  void set_token(std::string v) { token_ = std::move(v); token_changed_ = true; }

  const std::string& expires_at() const { return expires_at_; }
  void set_expires_at(std::string v) { expires_at_ = std::move(v); expires_at_changed_ = true; }

  const std::string& created_at() const { return created_at_; }
  void set_created_at(std::string v) { created_at_ = std::move(v); created_at_changed_ = true; }

private:
  long long id_ = 0; // 会话ID
  long long user_id_ = 0; // 用户ID
  std::string token_; // 会话令牌
  std::string expires_at_; // 过期时间
  std::string created_at_; // 创建时间

  bool user_id_changed_ = false;
  bool token_changed_ = false;
  bool expires_at_changed_ = false;
  bool created_at_changed_ = false;

  void clear_changes() {
    user_id_changed_ = false;
    token_changed_ = false;
    expires_at_changed_ = false;
    created_at_changed_ = false;
  }

  friend class SessionRepo;
};

class SessionRepo {
public:
  explicit SessionRepo(yxmysql::Connection& conn) : conn_(conn) {}

  std::optional<Session> get_one_by_id(long long id);
  bool save(Session& s);

  std::vector<Session> find_all(int limit = 100);

  // 登录：验证用户名与密码，成功则创建并返回新的会话；失败返回 nullopt
  // 注意：当前实现直接比较 password 与 user.password_hash 字段，
  // 如需哈希校验请在上层对 password 进行哈希后传入或在此处接入哈希库。
  std::optional<Session> login(const std::string& username, const std::string& password);

private:
  bool insert(Session& s);
  bool update(Session& s);

private:
  yxmysql::Connection& conn_;
};
