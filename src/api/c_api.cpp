#include "corekit/api/factory.hpp"

#include "ipc/shared_memory_channel.hpp"
#include "corekit/api/version.hpp"
#include "memory/system_allocator.hpp"
#include "log/log_manager_adapter.hpp"
#include "task/simple_task_graph.hpp"
#include "task/thread_pool_executor.hpp"

extern "C" {

std::uint32_t corekit_get_api_version() { return corekit::api::kApiVersion; }

corekit::log::ILogManager* corekit_create_log_manager() {
  return new corekit::log::LogManagerAdapter();
}

void corekit_destroy_log_manager(corekit::log::ILogManager* manager) {
  delete manager;
}

corekit::ipc::IChannel* corekit_create_ipc_channel() {
  return new corekit::ipc::SharedMemoryChannel();
}

void corekit_destroy_ipc_channel(corekit::ipc::IChannel* channel) { delete channel; }

corekit::memory::IAllocator* corekit_create_allocator() {
  return new corekit::memory::SystemAllocator();
}

void corekit_destroy_allocator(corekit::memory::IAllocator* allocator) {
  delete allocator;
}

corekit::task::IExecutor* corekit_create_executor() {
  return new corekit::task::ThreadPoolExecutor();
}

corekit::task::IExecutor* corekit_create_executor_v2(
    const corekit::task::ExecutorOptions* options) {
  if (options == NULL) {
    return new corekit::task::ThreadPoolExecutor();
  }
  return new corekit::task::ThreadPoolExecutor(*options);
}

void corekit_destroy_executor(corekit::task::IExecutor* executor) { delete executor; }

corekit::task::ITaskGraph* corekit_create_task_graph() {
  return new corekit::task::SimpleTaskGraph();
}

void corekit_destroy_task_graph(corekit::task::ITaskGraph* graph) { delete graph; }

}

