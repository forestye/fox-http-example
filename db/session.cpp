#include "session.h"
#include <sstream>
#include <random>
#include <chrono>
#include <iomanip>
#include <utility>

using namespace std;
using namespace fox::mysql;

optional<Session> SessionRepo::get_one_by_id(long long id) {
  string sql = "SELECT id, user_id, token, expires_at, created_at FROM session WHERE id=" + to_string(id) + " LIMIT 1";
  auto rs = conn_.query(sql);
  if (rs->next()) return Session(*rs);
  return nullopt;
}

bool SessionRepo::save(Session& s) {
  return (s.id_ == 0) ? insert(s) : update(s);
}

bool SessionRepo::insert(Session& s) {
  string esc_token = conn_.escape_string(s.token_);
  string esc_expires_at = conn_.escape_string(s.expires_at_);
  string esc_created_at = conn_.escape_string(s.created_at_);
  string sql = "INSERT INTO session (user_id, token, expires_at, created_at) VALUES (" +
               to_string(s.user_id_) + ", '" + esc_token + "', '" + esc_expires_at + "', '" + esc_created_at + "')";
  conn_.execute(sql);
  s.id_ = static_cast<long long>(conn_.insert_id());
  s.clear_changes();
  return true;
}

bool SessionRepo::update(Session& s) {
  vector<string> sets;
  if (s.user_id_changed_) sets.emplace_back("user_id=" + to_string(s.user_id_));
  if (s.token_changed_) sets.emplace_back("token='" + conn_.escape_string(s.token_) + "'");
  if (s.expires_at_changed_) sets.emplace_back("expires_at='" + conn_.escape_string(s.expires_at_) + "'");
  if (s.created_at_changed_) sets.emplace_back("created_at='" + conn_.escape_string(s.created_at_) + "'");

  if (sets.empty()) return true;
  ostringstream oss;
  oss << "UPDATE session SET ";
  for (size_t i = 0; i < sets.size(); ++i) {
    if (i) oss << ", ";
    oss << sets[i];
  }
  oss << " WHERE id=" << s.id_;
  conn_.execute(oss.str());
  s.clear_changes();
  return true;
}

vector<Session> SessionRepo::find_all(int limit) {
  string sql = "SELECT id, user_id, token, expires_at, created_at FROM session ORDER BY id DESC LIMIT " + to_string(limit);
  auto rs = conn_.query(sql);
  vector<Session> out;
  while (rs->next()) out.emplace_back(Session(*rs));
  return out;
}

// 生成简单的随机 token（32 字符十六进制）
static string gen_token() {
  static thread_local std::mt19937_64 rng(std::random_device{}());
  std::uniform_int_distribution<uint64_t> dist;
  uint64_t a = dist(rng);
  uint64_t b = dist(rng);
  std::ostringstream oss;
  oss << std::hex << std::setw(16) << std::setfill('0') << a
      << std::setw(16) << std::setfill('0') << b;
  return oss.str();
}

// 获取当前时间与过期时间（当前时间 + 24 小时），格式：YYYY-MM-DD HH:MM:SS
static pair<string, string> now_and_expire_24h() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto expire = now + hours(24);

  auto to_string_ts = [](system_clock::time_point tp) {
    time_t t = system_clock::to_time_t(tp);
    tm tm_{};
#if defined(_WIN32)
    localtime_s(&tm_, &t);
#else
    localtime_r(&t, &tm_);
#endif
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_);
    return string(buf);
  };

  return {to_string_ts(now), to_string_ts(expire)};
}

optional<Session> SessionRepo::login(const std::string& username, const std::string& password) {
  // 1) 查找用户
  string esc_username = conn_.escape_string(username);
  string sql_user = "SELECT id, password_hash FROM user WHERE username='" + esc_username + "' LIMIT 1";
  auto rs_user = conn_.query(sql_user);
  if (!rs_user->next()) {
    return nullopt; // 用户不存在
  }

  long long user_id = rs_user->get_int64(0);
  string stored_pass = rs_user->get_string(1);

  // 2) 简单的明文比较（注意：生产环境应使用安全哈希）
  if (stored_pass != password) {
    return nullopt; // 密码不匹配
  }

  // 3) 生成会话并持久化
  auto [created_at, expires_at] = now_and_expire_24h();
  Session s;
  s.set_user_id(user_id);
  s.set_token(gen_token());
  s.set_created_at(created_at);
  s.set_expires_at(expires_at);

  if (!insert(s)) return nullopt;
  return s;
}
