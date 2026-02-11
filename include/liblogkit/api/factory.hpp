#pragma once

#include <cstdint>

#include "liblogkit/api/export.hpp"

namespace liblogkit {
namespace log {
class ILogManager;
}
namespace ipc {
class IChannel;
}
namespace memory {
class IAllocator;
}
namespace task {
class IExecutor;
class ITaskGraph;
}
}  // namespace liblogkit

extern "C" {

// Return packed API version to allow runtime ABI compatibility checks.
LOGKIT_API std::uint32_t liblogkit_get_api_version();

// Create a log manager instance owned by the caller.
LOGKIT_API liblogkit::log::ILogManager* liblogkit_create_log_manager();

// Destroy a log manager created by liblogkit_create_log_manager.
LOGKIT_API void liblogkit_destroy_log_manager(liblogkit::log::ILogManager* manager);

// Create an IPC channel instance owned by the caller.
LOGKIT_API liblogkit::ipc::IChannel* liblogkit_create_ipc_channel();

// Destroy an IPC channel created by liblogkit_create_ipc_channel.
LOGKIT_API void liblogkit_destroy_ipc_channel(liblogkit::ipc::IChannel* channel);

// Create a memory allocator facade instance.
LOGKIT_API liblogkit::memory::IAllocator* liblogkit_create_allocator();

// Destroy an allocator created by liblogkit_create_allocator.
LOGKIT_API void liblogkit_destroy_allocator(liblogkit::memory::IAllocator* allocator);

// Create an executor instance.
LOGKIT_API liblogkit::task::IExecutor* liblogkit_create_executor();

// Destroy an executor created by liblogkit_create_executor.
LOGKIT_API void liblogkit_destroy_executor(liblogkit::task::IExecutor* executor);

// Create a task graph instance.
LOGKIT_API liblogkit::task::ITaskGraph* liblogkit_create_task_graph();

// Destroy a task graph created by liblogkit_create_task_graph.
LOGKIT_API void liblogkit_destroy_task_graph(liblogkit::task::ITaskGraph* graph);

}
