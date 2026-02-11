#include "liblogkit/api/factory.hpp"

#include "ipc/shared_memory_channel.hpp"
#include "liblogkit/api/version.hpp"
#include "memory/system_allocator.hpp"
#include "log/log_manager_adapter.hpp"
#include "task/simple_task_graph.hpp"
#include "task/thread_pool_executor.hpp"

extern "C" {

std::uint32_t liblogkit_get_api_version() { return liblogkit::api::kApiVersion; }

liblogkit::log::ILogManager* liblogkit_create_log_manager() {
  return new liblogkit::log::LogManagerAdapter();
}

void liblogkit_destroy_log_manager(liblogkit::log::ILogManager* manager) {
  delete manager;
}

liblogkit::ipc::IChannel* liblogkit_create_ipc_channel() {
  return new liblogkit::ipc::SharedMemoryChannel();
}

void liblogkit_destroy_ipc_channel(liblogkit::ipc::IChannel* channel) { delete channel; }

liblogkit::memory::IAllocator* liblogkit_create_allocator() {
  return new liblogkit::memory::SystemAllocator();
}

void liblogkit_destroy_allocator(liblogkit::memory::IAllocator* allocator) {
  delete allocator;
}

liblogkit::task::IExecutor* liblogkit_create_executor() {
  return new liblogkit::task::ThreadPoolExecutor();
}

void liblogkit_destroy_executor(liblogkit::task::IExecutor* executor) { delete executor; }

liblogkit::task::ITaskGraph* liblogkit_create_task_graph() {
  return new liblogkit::task::SimpleTaskGraph();
}

void liblogkit_destroy_task_graph(liblogkit::task::ITaskGraph* graph) { delete graph; }

}
