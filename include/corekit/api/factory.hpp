#pragma once

#include <cstdint>

#include "corekit/api/export.hpp"

namespace corekit {
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
}  // namespace corekit

extern "C" {

// Return packed API version to allow runtime ABI compatibility checks.
COREKIT_API std::uint32_t corekit_get_api_version();

// Create a log manager instance owned by the caller.
COREKIT_API corekit::log::ILogManager* corekit_create_log_manager();

// Destroy a log manager created by corekit_create_log_manager.
COREKIT_API void corekit_destroy_log_manager(corekit::log::ILogManager* manager);

// Create an IPC channel instance owned by the caller.
COREKIT_API corekit::ipc::IChannel* corekit_create_ipc_channel();

// Destroy an IPC channel created by corekit_create_ipc_channel.
COREKIT_API void corekit_destroy_ipc_channel(corekit::ipc::IChannel* channel);

// Create a memory allocator facade instance.
COREKIT_API corekit::memory::IAllocator* corekit_create_allocator();

// Destroy an allocator created by corekit_create_allocator.
COREKIT_API void corekit_destroy_allocator(corekit::memory::IAllocator* allocator);

// Create an executor instance.
COREKIT_API corekit::task::IExecutor* corekit_create_executor();

// Destroy an executor created by corekit_create_executor.
COREKIT_API void corekit_destroy_executor(corekit::task::IExecutor* executor);

// Create a task graph instance.
COREKIT_API corekit::task::ITaskGraph* corekit_create_task_graph();

// Destroy a task graph created by corekit_create_task_graph.
COREKIT_API void corekit_destroy_task_graph(corekit::task::ITaskGraph* graph);

}

