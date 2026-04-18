#pragma once
#include <string>
#include <cstdint>
#include <optional>
#include <vector>
#include "yxmysql/yxmysql.h"

// 对象类：id==0 表示未持久化的新对象
class User {
public:
  // 默认构造保留
  User() = default;

  // 通过 ResultSet 构造，避免外部访问私有字段
  explicit User(yxmysql::ResultSet& rs) {
    id_ = rs.get_int64(0);
    username_ = rs.get_string(1);
    password_hash_ = rs.get_string(2);
    email_ = rs.get_string(3);
    created_at_ = rs.get_string(4);
    clear_changes();
  }
  long long id() const { return id_; }

  const std::string& username() const { return username_; }
  void set_username(std::string v) { username_ = std::move(v); username_changed_ = true; }

  const std::string& password_hash() const { return password_hash_; }
  void set_password_hash(std::string v) { password_hash_ = std::move(v); password_hash_changed_ = true; }

  const std::string& email() const { return email_; }
  void set_email(std::string v) { email_ = std::move(v); email_changed_ = true; }

  const std::string& created_at() const { return created_at_; }
  void set_created_at(std::string v) { created_at_ = std::move(v); created_at_changed_ = true; }

private:
  long long id_ = 0; // 用户ID
  std::string username_; // 用户名
  std::string password_hash_; // 密码哈希
  std::string email_; // 邮箱
  std::string created_at_; // 创建时间

  bool username_changed_ = false;
  bool password_hash_changed_ = false;
  bool email_changed_ = false;
  bool created_at_changed_ = false;

  void clear_changes() {
    username_changed_ = false;
    password_hash_changed_ = false;
    email_changed_ = false;
    created_at_changed_ = false;
  }

  friend class UserRepo;
};

// 仓库类
class UserRepo {
public:
  explicit UserRepo(yxmysql::Connection& conn) : conn_(conn) {}

  std::optional<User> get_one_by_id(long long id);
  bool save(User& u);
  std::vector<User> find_all(int limit = 100);

private:
  bool insert(User& u);
  bool update(User& u);

private:
  yxmysql::Connection& conn_;
};
