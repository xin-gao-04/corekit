#include "log/glog_log_manager.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <algorithm>
#include <chrono>
#include <cctype>
#include <condition_variable>
#include <cerrno>
#include <cstdio>
#include <ctime>
#include <deque>
#include <fstream>
#if defined(_WIN32)
#include <direct.h>
#include <sys/stat.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include <glog/logging.h>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

#include "corekit/api/version.hpp"

namespace corekit {
namespace log {
namespace {

// ── 进程级单例状态 ──────────────────────────────────────────────────────────

std::mutex& GlobalMutex() {
  static std::mutex m;
  return m;
}

LoggingOptions& GlobalOptions() {
  static LoggingOptions opts;
  return opts;
}

std::string& GlobalSessionDir() {
  static std::string dir;
  return dir;
}

std::string& GlobalBaseDir() {
  static std::string dir;
  return dir;
}

std::unique_ptr<google::LogSink>& GlobalSink() {
  static std::unique_ptr<google::LogSink> sink;
  return sink;
}

bool& GlobalInitialized() {
  static bool initialized = false;
  return initialized;
}

bool& GlobalFailureHandlerInstalled() {
  static bool installed = false;
  return installed;
}

// ── 路径工具 ─────────────────────────────────────────────────────────────────

bool IsPathSeparator(char c) { return c == '/' || c == '\\'; }

std::string BaseName(const std::string& path) {
  if (path.empty()) return {};
  size_t end = path.size();
  while (end > 0 && IsPathSeparator(path[end - 1])) --end;
  if (end == 0) return {};
  const size_t pos = path.find_last_of("/\\", end - 1);
  if (pos == std::string::npos) return path.substr(0, end);
  return path.substr(pos + 1, end - pos - 1);
}

std::string JoinPath(const std::string& left, const std::string& right) {
  if (left.empty()) return right;
  if (right.empty()) return left;
  if (IsPathSeparator(left[left.size() - 1])) return left + right;
#if defined(_WIN32)
  return left + "\\" + right;
#else
  return left + "/" + right;
#endif
}

bool DirectoryExists(const std::string& path) {
  if (path.empty()) return false;
#if defined(_WIN32)
  struct _stat info;
  if (_stat(path.c_str(), &info) != 0) return false;
#else
  struct stat info;
  if (stat(path.c_str(), &info) != 0) return false;
#endif
  return (info.st_mode & S_IFDIR) != 0;
}

int MakeDir(const std::string& path) {
#if defined(_WIN32)
  return _mkdir(path.c_str());
#else
  return mkdir(path.c_str(), 0755);
#endif
}

bool CreateDirectories(const std::string& path) {
  if (path.empty()) return false;
  if (DirectoryExists(path)) return true;

  std::string current;
  size_t pos = 0;

#if defined(_WIN32)
  if (path.size() >= 2 && path[1] == ':') {
    current = path.substr(0, 2);
    pos = 2;
    if (pos < path.size() && IsPathSeparator(path[pos])) {
      current += "\\";
      ++pos;
    }
  } else if (IsPathSeparator(path[0])) {
    current = "\\";
    pos = 1;
  }
#else
  if (IsPathSeparator(path[0])) {
    current = "/";
    pos = 1;
  }
#endif

  while (pos <= path.size()) {
    size_t next = path.find_first_of("/\\", pos);
    std::string part =
        next == std::string::npos ? path.substr(pos) : path.substr(pos, next - pos);
    if (!part.empty()) {
      current = current.empty() ? part : JoinPath(current, part);
      if (!DirectoryExists(current)) {
        errno = 0;
        if (MakeDir(current) != 0 && errno != EEXIST) return false;
      }
    }
    if (next == std::string::npos) break;
    pos = next + 1;
  }
  return DirectoryExists(path);
}

int ClampInt(int value, int low, int high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

// ── 配置文件解析 ──────────────────────────────────────────────────────────────

std::string Trim(const std::string& s) {
  const auto start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  const auto end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

std::string ToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

bool ParseBool(const std::string& value, bool* out) {
  const std::string v = ToLower(Trim(value));
  if (v == "1" || v == "true" || v == "yes" || v == "on") { *out = true; return true; }
  if (v == "0" || v == "false" || v == "no" || v == "off") { *out = false; return true; }
  return false;
}

bool ParseInt(const std::string& value, int* out) {
  try { *out = std::stoi(Trim(value)); return true; } catch (...) { return false; }
}

int LevelFromText(const std::string& value) {
  const std::string v = ToLower(Trim(value));
  if (v == "info") return google::GLOG_INFO;
  if (v == "warning" || v == "warn") return google::GLOG_WARNING;
  if (v == "error") return google::GLOG_ERROR;
  if (v == "fatal") return google::GLOG_FATAL;
  int parsed = 0;
  if (ParseInt(v, &parsed)) return parsed;
  return google::GLOG_INFO;
}

// 从 INI 格式配置文件解析 LoggingOptions（仅识别公开字段）。
LoggingOptions ParseConfig(std::istream& input, bool* ok) {
  LoggingOptions options;
  std::string line;
  bool success = true;

  while (std::getline(input, line)) {
    std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == '#' || trimmed.rfind("//", 0) == 0) continue;

    const size_t sep = trimmed.find_first_of("=:");
    if (sep == std::string::npos) continue;

    const std::string key = ToLower(Trim(trimmed.substr(0, sep)));
    std::string value = Trim(trimmed.substr(sep + 1));

    // 去除行尾注释
    auto cut_comment = [](std::string& v) {
      const size_t hash = v.find('#');
      const size_t slash = v.find("//");
      size_t pos = std::string::npos;
      if (hash != std::string::npos) pos = hash;
      if (slash != std::string::npos) pos = std::min(pos, slash);
      if (pos != std::string::npos) v = Trim(v.substr(0, pos));
    };
    cut_comment(value);

    bool bool_value = false;
    int int_value = 0;

    if (key == "log_dir") {
      options.log_dir = value;
    } else if (key == "session_subdir") {
      if (ParseBool(value, &bool_value)) options.session_subdir = bool_value;
      else success = false;
    } else if (key == "simple_format") {
      if (ParseBool(value, &bool_value)) options.simple_format = bool_value;
      else success = false;
    } else if (key == "json_format") {
      if (ParseBool(value, &bool_value)) options.json_format = bool_value;
      else success = false;
    } else if (key == "async_sink") {
      if (ParseBool(value, &bool_value)) options.async_sink = bool_value;
      else success = false;
    } else if (key == "async_queue_size") {
      if (ParseInt(value, &int_value) && int_value > 0) options.async_queue_size = int_value;
      else success = false;
    } else if (key == "async_drop_when_full") {
      if (ParseBool(value, &bool_value)) options.async_drop_when_full = bool_value;
      else success = false;
    } else if (key == "logtostderr") {
      if (ParseBool(value, &bool_value)) options.logtostderr = bool_value;
      else success = false;
    } else if (key == "minloglevel") {
      options.min_log_level = LevelFromText(value);
    } else if (key == "max_log_size") {
      if (ParseInt(value, &int_value)) options.max_log_size_mb = int_value;
      else success = false;
    } else if (key == "stop_logging_if_full_disk") {
      if (ParseBool(value, &bool_value)) options.stop_logging_if_full_disk = bool_value;
      else success = false;
    }
    // 未识别的键直接忽略，保持宽容解析。
  }

  if (ok) *ok = success;
  return options;
}

// ── 时间戳工具 ────────────────────────────────────────────────────────────────

std::tm LocalTime(std::time_t t) {
  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  return tm;
}

std::string TimestampDir() {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto t = system_clock::to_time_t(now);
  const std::tm tm = LocalTime(t);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d%02d",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec);
  return std::string(buf);
}

std::string TimestampPrefix() {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto t = system_clock::to_time_t(now);
  const std::tm tm = LocalTime(t);
  const auto ns = duration_cast<nanoseconds>(now.time_since_epoch()).count() % 1000000000LL;
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%04d%02d%02d %02d:%02d:%02d.%09lld",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<long long>(ns));
  return std::string(buf);
}

// ── JSON 转义 ─────────────────────────────────────────────────────────────────

std::string JsonEscape(const std::string& input) {
  std::ostringstream out;
  for (unsigned char c : input) {
    switch (c) {
      case '\\': out << "\\\\"; break;
      case '"':  out << "\\\""; break;
      case '\n': out << "\\n";  break;
      case '\r': out << "\\r";  break;
      case '\t': out << "\\t";  break;
      default:
        if (c < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<int>(c) << std::dec;
        } else {
          out << static_cast<char>(c);
        }
        break;
    }
  }
  return out.str();
}

char LevelChar(google::LogSeverity severity) {
  const char levels[] = {'I', 'W', 'E', 'F'};
  return levels[ClampInt(static_cast<int>(severity), 0, 3)];
}

std::string FormatSourceLocation(const char* file, int line) {
  if (file == NULL || file[0] == '\0' || line <= 0) return std::string();
  std::ostringstream out;
  out << "[" << file << ":" << line << "]";
  return out.str();
}

// ── 自定义格式化 Sink ─────────────────────────────────────────────────────────

class FormattedSink : public google::LogSink {
 public:
  enum class Mode { kSimple, kJson };

  FormattedSink(const std::string& file_path, Mode mode, bool async_mode,
                int queue_size, bool drop_when_full)
      : stream_(file_path, std::ios::app),
        mode_(mode),
        async_mode_(async_mode),
        queue_size_(std::max(1, queue_size)),
        drop_when_full_(drop_when_full) {
    if (async_mode_) {
      worker_ = std::thread([this]() { this->RunWorker(); });
    }
  }

  ~FormattedSink() override {
    if (!async_mode_) return;
    {
      std::lock_guard<std::mutex> lock(queue_mu_);
      stopping_ = true;
    }
    queue_cv_.notify_all();
    queue_space_cv_.notify_all();
    if (worker_.joinable()) worker_.join();
  }

  void send(google::LogSeverity severity, const char* full_filename, const char*,
            int line,
            const std::tm*, const char* message, size_t) override {
    if (!stream_.is_open()) return;
    std::string msg = message ? message : "";
    while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) msg.pop_back();
    const std::string location = FormatSourceLocation(full_filename, line);
    const std::string formatted_line = FormatLine(severity, location, msg);

    if (!async_mode_) {
      std::lock_guard<std::mutex> lock(stream_mu_);
      stream_ << formatted_line << '\n';
      stream_.flush();
      return;
    }

    std::unique_lock<std::mutex> lock(queue_mu_);
    if (drop_when_full_) {
      if (queue_.size() >= static_cast<size_t>(queue_size_)) { ++dropped_count_; return; }
    } else {
      queue_space_cv_.wait(lock, [this] {
        return stopping_ || queue_.size() < static_cast<size_t>(queue_size_);
      });
      if (stopping_) return;
    }
    queue_.push_back(formatted_line);
    lock.unlock();
    queue_cv_.notify_one();
  }

 private:
  std::string FormatLine(google::LogSeverity severity, const std::string& location,
                         const std::string& msg) const {
    const char level = LevelChar(severity);
    if (mode_ == Mode::kSimple) {
      std::ostringstream out;
      out << TimestampPrefix() << " [" << level << "]";
      if (!location.empty()) out << " " << location;
      out << " " << msg;
      return out.str();
    }
    std::ostringstream out;
    out << "{\"ts\":\"" << TimestampPrefix() << "\",\"level\":\"" << level << "\"";
    if (!location.empty()) {
      out << ",\"source\":\"" << JsonEscape(location) << "\"";
    }
    out << ",\"message\":\"" << JsonEscape(msg) << "\"}";
    return out.str();
  }

  void RunWorker() {
    for (;;) {
      std::string line;
      {
        std::unique_lock<std::mutex> lock(queue_mu_);
        queue_cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
        if (queue_.empty()) { if (stopping_) break; continue; }
        line = std::move(queue_.front());
        queue_.pop_front();
      }
      queue_space_cv_.notify_one();
      std::lock_guard<std::mutex> lock(stream_mu_);
      stream_ << line << '\n';
      stream_.flush();
    }
    if (dropped_count_ > 0) {
      std::lock_guard<std::mutex> lock(stream_mu_);
      stream_ << "{\"ts\":\"" << TimestampPrefix()
              << "\",\"level\":\"W\",\"message\":\"corekit dropped "
              << dropped_count_ << " messages because async queue was full\"}\n";
      stream_.flush();
    }
  }

  std::ofstream stream_;
  Mode mode_;
  bool async_mode_;
  int queue_size_;
  bool drop_when_full_;
  std::mutex stream_mu_;
  std::mutex queue_mu_;
  std::condition_variable queue_cv_;
  std::condition_variable queue_space_cv_;
  std::deque<std::string> queue_;
  std::thread worker_;
  bool stopping_ = false;
  size_t dropped_count_ = 0;
};

// ── 配置应用 ──────────────────────────────────────────────────────────────────

bool ApplyOptions(const LoggingOptions& options) {
  // 目录管理
  std::string output_dir;
  if (!options.log_dir.empty()) {
    std::string target = options.log_dir;
    if (options.session_subdir) {
      if (GlobalSessionDir().empty() || GlobalBaseDir() != options.log_dir) {
        target = JoinPath(options.log_dir, TimestampDir());
        if (!CreateDirectories(target)) return false;
        GlobalSessionDir() = target;
        GlobalBaseDir() = options.log_dir;
      } else {
        target = GlobalSessionDir();
      }
    } else {
      GlobalSessionDir().clear();
      GlobalBaseDir() = options.log_dir;
    }
    if (!CreateDirectories(target)) return false;
    output_dir = target;
  } else {
    GlobalSessionDir().clear();
    GlobalBaseDir().clear();
  }

  // glog 始终不使用原生文件输出，由自定义 Sink 或 stderr 处理
  FLAGS_log_dir.clear();
  FLAGS_logtostderr = options.log_dir.empty() ? true : options.logtostderr;
  FLAGS_alsologtostderr = false;
  FLAGS_colorlogtostderr = true;
  FLAGS_log_prefix = true;
  FLAGS_minloglevel = options.min_log_level;
  FLAGS_stderrthreshold = google::GLOG_ERROR;
  FLAGS_v = 0;
  FLAGS_max_log_size = static_cast<google::uint32>(std::max(0, options.max_log_size_mb));
  FLAGS_logbufsecs = 30;
  FLAGS_stop_logging_if_full_disk = options.stop_logging_if_full_disk;

  if (!GlobalFailureHandlerInstalled()) {
    google::InstallFailureSignalHandler();
    GlobalFailureHandlerInstalled() = true;
  }

  // 重建自定义 Sink（支持 Reload 时切换格式/路径）
  if (GlobalSink()) {
    google::RemoveLogSink(GlobalSink().get());
    GlobalSink().reset();
  }

  const bool custom_format = options.simple_format || options.json_format;
  if (custom_format && !output_dir.empty()) {
    const bool use_json = options.json_format;
    const std::string sink_file =
        JoinPath(output_dir, use_json ? "app.jsonl" : "app.log");
    GlobalSink().reset(new FormattedSink(
        sink_file,
        use_json ? FormattedSink::Mode::kJson : FormattedSink::Mode::kSimple,
        options.async_sink, options.async_queue_size, options.async_drop_when_full));
    google::AddLogSink(GlobalSink().get());
    FLAGS_log_prefix = false;
  }

  GlobalOptions() = options;
  return true;
}

LoggingOptions LoadFromFile(const std::string& path, bool* ok) {
  if (path.empty()) { if (ok) *ok = true; return LoggingOptions{}; }
  std::ifstream input(path);
  if (!input.is_open()) { if (ok) *ok = false; return LoggingOptions{}; }
  return ParseConfig(input, ok);
}

}  // namespace

// ── GlogLogManager 实现 ───────────────────────────────────────────────────────

const char* GlogLogManager::Name() const { return "corekit.log.glog"; }

std::uint32_t GlogLogManager::ApiVersion() const { return api::kApiVersion; }

void GlogLogManager::Release() { delete this; }

api::Status GlogLogManager::Init(const std::string& app_name,
                                  const std::string& config_path) {
  if (app_name.empty()) {
    return api::Status(api::StatusCode::kInvalidArgument, "app_name is empty");
  }
  std::lock_guard<std::mutex> lock(GlobalMutex());
  if (GlobalInitialized()) return api::Status::Ok();

  // 先让 glog 输出到 stderr，避免初始化阶段就创建文件
  FLAGS_logtostderr = true;
  const std::string app = BaseName(app_name).empty() ? "corekit" : BaseName(app_name);
  google::InitGoogleLogging(app.c_str());

  bool ok = true;
  const LoggingOptions options = LoadFromFile(config_path, &ok);
  if (!ok) {
    google::ShutdownGoogleLogging();
    return api::Status(api::StatusCode::kInternalError,
                       "failed to parse config: " + config_path);
  }

  if (!ApplyOptions(options)) {
    if (GlobalSink()) { google::RemoveLogSink(GlobalSink().get()); GlobalSink().reset(); }
    GlobalSessionDir().clear();
    GlobalBaseDir().clear();
    google::ShutdownGoogleLogging();
    return api::Status(api::StatusCode::kInternalError, "failed to apply logging options");
  }

  GlobalInitialized() = true;
  return api::Status::Ok();
}

api::Status GlogLogManager::Reload(const std::string& config_path) {
  if (config_path.empty()) {
    return api::Status(api::StatusCode::kInvalidArgument, "config_path is empty");
  }
  std::lock_guard<std::mutex> lock(GlobalMutex());
  if (!GlobalInitialized()) {
    return api::Status(api::StatusCode::kNotInitialized, "not initialized");
  }
  bool ok = true;
  const LoggingOptions options = LoadFromFile(config_path, &ok);
  if (!ok) {
    return api::Status(api::StatusCode::kInternalError,
                       "failed to parse config: " + config_path);
  }
  if (!ApplyOptions(options)) {
    return api::Status(api::StatusCode::kInternalError, "failed to apply logging options");
  }
  return api::Status::Ok();
}

api::Status GlogLogManager::Log(LogSeverity severity, const std::string& message) {
  return LogWithSource(severity, message, __FILE__, __LINE__);
}

api::Status GlogLogManager::LogWithSource(LogSeverity severity, const std::string& message,
                                          const char* file, int line) {
  const google::LogSeverity gs =
      static_cast<google::LogSeverity>(ClampInt(static_cast<int>(severity), 0, 3));
  const char* actual_file = (file != NULL && file[0] != '\0') ? file : __FILE__;
  const int actual_line = line > 0 ? line : __LINE__;
  google::LogMessage(actual_file, actual_line, gs).stream() << message;
  return api::Status::Ok();
}

api::Result<LoggingOptions> GlogLogManager::CurrentOptions() const {
  std::lock_guard<std::mutex> lock(GlobalMutex());
  return api::Result<LoggingOptions>(GlobalOptions());
}

api::Status GlogLogManager::Shutdown() {
  std::lock_guard<std::mutex> lock(GlobalMutex());
  if (!GlobalInitialized()) return api::Status::Ok();
  if (GlobalSink()) { google::RemoveLogSink(GlobalSink().get()); GlobalSink().reset(); }
  google::ShutdownGoogleLogging();
  GlobalSessionDir().clear();
  GlobalBaseDir().clear();
  GlobalInitialized() = false;
  return api::Status::Ok();
}

}  // namespace log
}  // namespace corekit
