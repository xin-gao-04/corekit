#include "corekit/corekit.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

bool RecvUntilOk(corekit::ipc::IChannel* ch,
                 void* buffer,
                 std::uint32_t buffer_size,
                 corekit::api::Result<std::uint32_t>* out) {
  for (int i = 0; i < 2000; ++i) {
    corekit::api::Result<std::uint32_t> r = ch->TryRecv(buffer, buffer_size);
    if (r.ok()) {
      *out = r;
      return true;
    }
    if (r.status().code() != corekit::api::StatusCode::kWouldBlock) {
      *out = r;
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}

bool TestIpcVariableFramesRoundTrip() {
  corekit::ipc::IChannel* server = corekit_create_ipc_channel();
  corekit::ipc::IChannel* client = corekit_create_ipc_channel();
  if (server == NULL || client == NULL) {
    std::printf("alloc channel failed\n");
    return false;
  }

  corekit::ipc::ChannelOptions opt;
  opt.name = "ut_ipc_var_frames";
  opt.capacity = 7;
  opt.message_max_bytes = 128;

  corekit::api::Status sst = server->OpenServer(opt);
  if (!sst.ok()) {
    std::printf("OpenServer failed: %s\n", sst.message().c_str());
    return false;
  }
  corekit::api::Status cst = client->OpenClient(opt);
  if (!cst.ok()) {
    std::printf("OpenClient failed: %s\n", cst.message().c_str());
    return false;
  }

  for (int i = 0; i < 120; ++i) {
    const int payload_size = 1 + (i * 37 % 100);
    std::vector<char> payload(static_cast<std::size_t>(payload_size));
    for (int j = 0; j < payload_size; ++j) {
      payload[static_cast<std::size_t>(j)] = static_cast<char>('a' + ((i + j) % 26));
    }

    corekit::api::Status send_st =
        server->TrySend(payload.data(), static_cast<std::uint32_t>(payload.size()));
    if (!send_st.ok()) {
      std::printf("TrySend failed at i=%d: %s\n", i, send_st.message().c_str());
      return false;
    }

    std::vector<char> recv(128, 0);
    corekit::api::Result<std::uint32_t> got(corekit::api::Status::Ok());
    if (!RecvUntilOk(client, recv.data(), static_cast<std::uint32_t>(recv.size()), &got)) {
      std::printf("RecvUntilOk failed at i=%d\n", i);
      return false;
    }
    if (got.value() != payload.size()) {
      std::printf("size mismatch at i=%d: got=%u expect=%zu\n", i, got.value(), payload.size());
      return false;
    }
    if (std::memcmp(payload.data(), recv.data(), payload.size()) != 0) {
      std::printf("payload mismatch at i=%d\n", i);
      return false;
    }
  }

  server->Close();
  client->Close();
  corekit_destroy_ipc_channel(server);
  corekit_destroy_ipc_channel(client);
  return true;
}

bool TestIpcBufferTooSmallDoesNotConsume() {
  corekit::ipc::IChannel* server = corekit_create_ipc_channel();
  corekit::ipc::IChannel* client = corekit_create_ipc_channel();
  if (server == NULL || client == NULL) return false;

  corekit::ipc::ChannelOptions opt;
  opt.name = "ut_ipc_small_buffer";
  opt.capacity = 4;
  opt.message_max_bytes = 128;

  if (!server->OpenServer(opt).ok()) return false;
  if (!client->OpenClient(opt).ok()) return false;

  const std::string msg = "abcdefghijklmnopqrstuvwxyz0123456789";
  if (!server->TrySend(msg.data(), static_cast<std::uint32_t>(msg.size())).ok()) {
    return false;
  }

  char tiny[8] = {0};
  for (int i = 0; i < 2000; ++i) {
    corekit::api::Result<std::uint32_t> r = client->TryRecv(tiny, sizeof(tiny));
    if (!r.ok()) {
      if (r.status().code() == corekit::api::StatusCode::kWouldBlock) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      if (r.status().code() != corekit::api::StatusCode::kBufferTooSmall) {
        return false;
      }
      break;
    }
  }

  std::vector<char> ok(128, 0);
  corekit::api::Result<std::uint32_t> got(corekit::api::Status::Ok());
  if (!RecvUntilOk(client, ok.data(), static_cast<std::uint32_t>(ok.size()), &got)) {
    return false;
  }
  if (got.value() != msg.size()) return false;
  if (std::memcmp(ok.data(), msg.data(), msg.size()) != 0) return false;

  server->Close();
  client->Close();
  corekit_destroy_ipc_channel(server);
  corekit_destroy_ipc_channel(client);
  return true;
}

bool TestIpcBackpressureAndStats() {
  corekit::ipc::IChannel* server = corekit_create_ipc_channel();
  corekit::ipc::IChannel* client = corekit_create_ipc_channel();
  if (server == NULL || client == NULL) return false;

  corekit::ipc::ChannelOptions opt;
  opt.name = "ut_ipc_backpressure";
  opt.capacity = 2;
  opt.message_max_bytes = 32;
  opt.drop_when_full = true;

  if (!server->OpenServer(opt).ok()) return false;
  if (!client->OpenClient(opt).ok()) return false;

  const char payload[] = "0123456789abcdef";
  bool saw_block = false;
  for (int i = 0; i < 500; ++i) {
    corekit::api::Status st = server->TrySend(payload, static_cast<std::uint32_t>(sizeof(payload)));
    if (!st.ok()) {
      if (st.code() == corekit::api::StatusCode::kWouldBlock) {
        saw_block = true;
        break;
      }
      return false;
    }
  }
  if (!saw_block) return false;

  corekit::ipc::ChannelStats stats = server->GetStats();
  if (stats.would_block_send == 0) return false;
  if (stats.dropped_when_full == 0) return false;

  char recv[64] = {0};
  for (int i = 0; i < 200; ++i) {
    corekit::api::Result<std::uint32_t> r = client->TryRecv(recv, sizeof(recv));
    if (!r.ok() && r.status().code() == corekit::api::StatusCode::kWouldBlock) {
      break;
    }
  }

  server->Close();
  client->Close();
  corekit_destroy_ipc_channel(server);
  corekit_destroy_ipc_channel(client);
  return true;
}

bool TestIpcBurstThroughputSmoke() {
  corekit::ipc::IChannel* server = corekit_create_ipc_channel();
  corekit::ipc::IChannel* client = corekit_create_ipc_channel();
  if (server == NULL || client == NULL) return false;

  corekit::ipc::ChannelOptions opt;
  opt.name = "ut_ipc_burst_smoke";
  opt.capacity = 64;
  opt.message_max_bytes = 64;
  opt.drop_when_full = false;

  if (!server->OpenServer(opt).ok()) return false;
  if (!client->OpenClient(opt).ok()) return false;

  const char payload[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  const int n = 5000;
  int sent = 0;
  int recv = 0;
  char out[128] = {0};
  const auto begin = std::chrono::steady_clock::now();

  while (recv < n) {
    if (sent < n) {
      corekit::api::Status s = server->TrySend(payload, static_cast<std::uint32_t>(sizeof(payload)));
      if (s.ok()) {
        ++sent;
      } else if (s.code() != corekit::api::StatusCode::kWouldBlock) {
        return false;
      }
    }

    corekit::api::Result<std::uint32_t> r = client->TryRecv(out, sizeof(out));
    if (r.ok()) {
      if (r.value() != sizeof(payload) || std::memcmp(out, payload, sizeof(payload)) != 0) {
        return false;
      }
      ++recv;
    } else if (r.status().code() != corekit::api::StatusCode::kWouldBlock) {
      return false;
    }
  }

  const auto end = std::chrono::steady_clock::now();
  const double sec =
      std::chrono::duration_cast<std::chrono::duration<double>>(end - begin).count();
  const double qps = static_cast<double>(n) / (sec > 0.0 ? sec : 1.0);
  std::printf("[INFO] ipc_burst_smoke throughput=%.0f msg/s\n", qps);

  server->Close();
  client->Close();
  corekit_destroy_ipc_channel(server);
  corekit_destroy_ipc_channel(client);
  return true;
}

}  // namespace

int main() {
  struct TestCase {
    const char* name;
    bool (*fn)();
  };

  const TestCase tests[] = {
      {"ipc_variable_frames_roundtrip", TestIpcVariableFramesRoundTrip},
      {"ipc_buffer_too_small_no_consume", TestIpcBufferTooSmallDoesNotConsume},
      {"ipc_backpressure_and_stats", TestIpcBackpressureAndStats},
      {"ipc_burst_throughput_smoke", TestIpcBurstThroughputSmoke},
  };

  int failed = 0;
  for (std::size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
    std::printf("[RUN ] %s\n", tests[i].name);
    const bool ok = tests[i].fn();
    std::printf("%s %s\n", ok ? "[PASS]" : "[FAIL]", tests[i].name);
    if (!ok) {
      ++failed;
    }
  }

  return failed == 0 ? 0 : 1;
}




