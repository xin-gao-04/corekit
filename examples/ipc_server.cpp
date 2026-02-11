#include "liblogkit/liblogkit.hpp"

#include <cstdio>
#include <cstring>

int main() {
  liblogkit::ipc::IChannel* ch = liblogkit_create_ipc_channel();
  if (ch == NULL) return 1;

  liblogkit::ipc::ChannelOptions opt;
  opt.name = "demo_channel";
  opt.capacity = 64;
  opt.message_max_bytes = 256;
  opt.drop_when_full = true;

  liblogkit::api::Status st = ch->OpenServer(opt);
  if (!st.ok()) {
    std::fprintf(stderr, "OpenServer failed: %s\n", st.message().c_str());
    liblogkit_destroy_ipc_channel(ch);
    return 1;
  }

  const char* msg = "hello from ipc server";
  st = ch->TrySend(msg, static_cast<std::uint32_t>(std::strlen(msg) + 1));
  if (!st.ok()) {
    std::fprintf(stderr, "TrySend failed: %s\n", st.message().c_str());
  }

  ch->Close();
  liblogkit_destroy_ipc_channel(ch);
  return 0;
}
