#include "liblogkit/liblogkit.hpp"

#include <cstdio>

int main() {
  liblogkit::ipc::IChannel* ch = liblogkit_create_ipc_channel();
  if (ch == NULL) return 1;

  liblogkit::ipc::ChannelOptions opt;
  opt.name = "demo_channel";

  liblogkit::api::Status st = ch->OpenClient(opt);
  if (!st.ok()) {
    std::fprintf(stderr, "OpenClient failed: %s\n", st.message().c_str());
    liblogkit_destroy_ipc_channel(ch);
    return 1;
  }

  char buf[256] = {0};
  liblogkit::api::Result<std::uint32_t> res = ch->TryRecv(buf, sizeof(buf));
  if (res.ok()) {
    std::printf("recv: %s\n", buf);
  } else {
    std::fprintf(stderr, "TryRecv failed: %s\n", res.status().message().c_str());
  }

  ch->Close();
  liblogkit_destroy_ipc_channel(ch);
  return 0;
}
