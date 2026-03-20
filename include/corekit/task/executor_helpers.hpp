#pragma once

#include "corekit/task/iexecutor.hpp"

namespace corekit {
namespace task {

/// Submit a callable (lambda, functor) to an executor.
/// Convenience wrapper that converts any callable to std::function<void()>.
///
/// Usage:
///   corekit::task::SubmitLambda(exec, [&]{ process(data); });
template <typename Fn>
inline api::Status SubmitLambda(IExecutor* executor, Fn&& fn) {
  return executor->Submit(std::function<void()>(static_cast<Fn&&>(fn)));
}

/// Submit a callable with options, returning a TaskId.
template <typename Fn>
inline api::Result<TaskId> SubmitLambdaEx(IExecutor* executor, Fn&& fn,
                                           const TaskSubmitOptions& options) {
  return executor->SubmitEx(std::function<void()>(static_cast<Fn&&>(fn)), options);
}

/// Submit a callable under a serial key.
template <typename Fn>
inline api::Result<TaskId> SubmitLambdaWithKey(IExecutor* executor,
                                                std::uint64_t serial_key, Fn&& fn) {
  return executor->SubmitWithKey(serial_key, std::function<void()>(static_cast<Fn&&>(fn)));
}

}  // namespace task
}  // namespace corekit
