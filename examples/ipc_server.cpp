#include "corekit/corekit.hpp"

#include <cstdio>
#include <cstring>

int main() {
  corekit::ipc::IChannel* ch = corekit_create_ipc_channel();
  if (ch == NULL) return 1;

  corekit::ipc::ChannelOptions opt;
  opt.name = "demo_channel";
  opt.capacity = 64;
  opt.message_max_bytes = 256;
  opt.drop_when_full = true;

  corekit::api::Status st = ch->OpenServer(opt);
  if (!st.ok()) {
    std::fprintf(stderr, "OpenServer failed: %s\n", st.message().c_str());
    corekit_destroy_ipc_channel(ch);
    return 1;
  }

  const char* msg = "hello from ipc server";
  st = ch->TrySend(msg, static_cast<std::uint32_t>(std::strlen(msg) + 1));
  if (!st.ok()) {
    std::fprintf(stderr, "TrySend failed: %s\n", st.message().c_str());
  }

  ch->Close();
  corekit_destroy_ipc_channel(ch);
  return 0;
}
