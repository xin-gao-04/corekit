#include "logkit/log_manager.hpp"

#include <chrono>
#include <cstdlib>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#if defined(_WIN32)
#include <direct.h>
#include <sys/stat.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {

bool IsPathSeparator(char c) { return c == '/' || c == '\\'; }

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

void RemoveTree(const std::string& path) {
#if defined(_WIN32)
  const std::string cmd = "cmd /c if exist \"" + path + "\" rmdir /s /q \"" + path + "\" >nul 2>nul";
  std::system(cmd.c_str());
#else
  const std::string cmd = "rm -rf \"" + path + "\"";
  std::system(cmd.c_str());
#endif
}

std::string TempDirectory() {
#if defined(_WIN32)
  const char* temp = std::getenv("TEMP");
  if (temp && *temp) return temp;
  const char* tmp = std::getenv("TMP");
  if (tmp && *tmp) return tmp;
  return ".";
#else
  const char* tmp = std::getenv("TMPDIR");
  if (tmp && *tmp) return tmp;
  return "/tmp";
#endif
}

std::string UniqueTestDir(const std::string& name) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return JoinPath(TempDirectory(), "logkit_" + name + "_" + std::to_string(now));
}

bool WriteTextFile(const std::string& path, const std::string& content) {
  std::ofstream out(path);
  if (!out.is_open()) return false;
  out << content;
  return out.good();
}

std::string ReadTextFile(const std::string& path) {
  std::ifstream in(path);
  if (!in.is_open()) return {};
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

bool TestReloadBeforeInitFails() { return !logkit::LogManager::Reload("not_used.conf"); }

bool TestJsonAsyncSinkWritesFile() {
  const std::string root = UniqueTestDir("json_async");
  const std::string logs_dir = JoinPath(root, "logs");
  const std::string cfg = JoinPath(root, "logging.conf");
  if (!CreateDirectories(root)) return false;

  const std::string config =
      "log_dir = " + logs_dir + "\n" + "session_subdir = false\n" +
      "json_format = true\n" + "async_sink = true\n" + "async_queue_size = 256\n" +
      "async_drop_when_full = false\n" + "install_failure_signal_handler = false\n" +
      "bootstrap_stderr = true\n" + "logtostderr = false\n" + "alsologtostderr = false\n";
  if (!WriteTextFile(cfg, config)) return false;

  if (!logkit::LogManager::Init("logkit_tests", cfg)) return false;
  const auto opts = logkit::LogManager::CurrentOptions();
  if (!opts.json_format || !opts.async_sink || opts.async_queue_size != 256) return false;

  logkit::LogManager::Log(logkit::LogSeverity::kInfo, "hello-json");
  logkit::LogManager::Log(logkit::LogSeverity::kError, "error-json");
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  logkit::LogManager::Shutdown();

  const std::string log_file = JoinPath(logs_dir, "app.jsonl");
  const std::string body = ReadTextFile(log_file);
  const bool ok = body.find("\"message\":\"hello-json\"") != std::string::npos &&
                  body.find("\"level\":\"E\"") != std::string::npos;

  RemoveTree(root);
  return ok;
}

bool TestReloadInvalidConfigKeepsOptions() {
  const std::string root = UniqueTestDir("reload_invalid");
  const std::string logs_dir = JoinPath(root, "logs");
  const std::string good_cfg = JoinPath(root, "good.conf");
  const std::string bad_cfg = JoinPath(root, "bad.conf");
  if (!CreateDirectories(root)) return false;

  const std::string good =
      "log_dir = " + logs_dir + "\n" + "session_subdir = false\n" +
      "simple_format = true\n" + "async_sink = false\n" + "v = 2\n" +
      "install_failure_signal_handler = false\n";
  const std::string bad = "v = not_a_number\n";
  if (!WriteTextFile(good_cfg, good) || !WriteTextFile(bad_cfg, bad)) return false;

  if (!logkit::LogManager::Init("logkit_tests", good_cfg)) return false;
  const auto before = logkit::LogManager::CurrentOptions();
  const bool reload_ok = logkit::LogManager::Reload(bad_cfg);
  const auto after = logkit::LogManager::CurrentOptions();
  logkit::LogManager::Shutdown();

  RemoveTree(root);
  if (reload_ok) return false;
  return before.verbosity == after.verbosity && before.simple_format == after.simple_format &&
         before.async_sink == after.async_sink;
}

}  // namespace

int main() {
  struct Case {
    const char* name;
    bool (*fn)();
  };
  const Case cases[] = {{"reload_before_init", TestReloadBeforeInitFails},
                        {"json_async_sink", TestJsonAsyncSinkWritesFile},
                        {"reload_invalid_keep_options", TestReloadInvalidConfigKeepsOptions}};

  int failed = 0;
  for (const auto& c : cases) {
    const bool ok = c.fn();
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << c.name << '\n';
    if (!ok) ++failed;
  }
  return failed == 0 ? 0 : 1;
}
