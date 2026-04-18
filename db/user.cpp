#include "user.h"
#include <sstream>

using namespace std;
using namespace yxmysql;

optional<User> UserRepo::get_one_by_id(long long id) {
  string sql = "SELECT id, username, password_hash, email, created_at FROM user WHERE id=" + to_string(id) + " LIMIT 1";
  auto rs = conn_.query(sql);
  if (rs->next()) return User(*rs);
  return nullopt;
}

bool UserRepo::save(User& u) {
  return (u.id_ == 0) ? insert(u) : update(u);
}

bool UserRepo::insert(User& u) {
  string esc_username = conn_.escape_string(u.username_);
  string esc_password_hash = conn_.escape_string(u.password_hash_);
  string esc_email = conn_.escape_string(u.email_);
  string sql = "INSERT INTO user (username, password_hash, email) VALUES ('" +
               esc_username + "', '" + esc_password_hash + "', '" + esc_email + "')";
  conn_.execute(sql);
  u.id_ = static_cast<long long>(conn_.insert_id());
  u.clear_changes();
  return true;
}

bool UserRepo::update(User& u) {
  vector<string> sets;
  if (u.username_changed_) sets.emplace_back("username='" + conn_.escape_string(u.username_) + "'");
  if (u.password_hash_changed_) sets.emplace_back("password_hash='" + conn_.escape_string(u.password_hash_) + "'");
  if (u.email_changed_) sets.emplace_back("email='" + conn_.escape_string(u.email_) + "'");
  if (u.created_at_changed_) sets.emplace_back("created_at='" + conn_.escape_string(u.created_at_) + "'");

  if (sets.empty()) return true;
  ostringstream oss;
  oss << "UPDATE user SET ";
  for (size_t i = 0; i < sets.size(); ++i) {
    if (i) oss << ", ";
    oss << sets[i];
  }
  oss << " WHERE id=" << u.id_;
  conn_.execute(oss.str());
  u.clear_changes();
  return true;
}

vector<User> UserRepo::find_all(int limit) {
  string sql = "SELECT id, username, password_hash, email, created_at FROM user ORDER BY id DESC LIMIT " + to_string(limit);
  auto rs = conn_.query(sql);
  vector<User> out;
  while (rs->next()) out.emplace_back(User(*rs));
  return out;
}
