#pragma once

#include "corekit/task/iexecutor.hpp"

namespace corekit {
namespace task {

namespace detail {

template <typename Fn>
struct LambdaContext {
  Fn fn;
  explicit LambdaContext(Fn&& f) : fn(static_cast<Fn&&>(f)) {}
  static void Invoke(void* p) {
    LambdaContext* ctx = static_cast<LambdaContext*>(p);
    ctx->fn();
    delete ctx;
  }
};

template <typename Fn>
struct IndexedLambdaContext {
  Fn fn;
  explicit IndexedLambdaContext(Fn&& f) : fn(static_cast<Fn&&>(f)) {}
  static void Invoke(std::size_t index, void* p) {
    IndexedLambdaContext* ctx = static_cast<IndexedLambdaContext*>(p);
    ctx->fn(index);
    // Note: not deleted here — ParallelFor shares context across invocations.
  }
};

}  // namespace detail

/// Submit a callable (lambda, functor) to an executor.
/// The callable is heap-allocated and freed after execution.
///
/// Usage:
///   corekit::task::SubmitLambda(exec, [&]{ process(data); });
template <typename Fn>
inline api::Status SubmitLambda(IExecutor* executor, Fn&& fn) {
  typedef detail::LambdaContext<Fn> Ctx;
  Ctx* ctx = new (std::nothrow) Ctx(static_cast<Fn&&>(fn));
  if (ctx == NULL) {
    return api::Status(api::StatusCode::kInternalError, "failed to allocate lambda context");
  }
  api::Status st = executor->Submit(&Ctx::Invoke, ctx);
  if (!st.ok()) {
    delete ctx;
  }
  return st;
}

/// Submit a callable with options, returning a TaskId.
template <typename Fn>
inline api::Result<TaskId> SubmitLambdaEx(IExecutor* executor, Fn&& fn,
                                           const TaskSubmitOptions& options) {
  typedef detail::LambdaContext<Fn> Ctx;
  Ctx* ctx = new (std::nothrow) Ctx(static_cast<Fn&&>(fn));
  if (ctx == NULL) {
    return api::Result<TaskId>(
        api::Status(api::StatusCode::kInternalError, "failed to allocate lambda context"));
  }
  api::Result<TaskId> r = executor->SubmitEx(&Ctx::Invoke, ctx, options);
  if (!r.ok()) {
    delete ctx;
  }
  return r;
}

/// Submit a callable under a serial key.
template <typename Fn>
inline api::Result<TaskId> SubmitLambdaWithKey(IExecutor* executor,
                                                std::uint64_t serial_key, Fn&& fn) {
  typedef detail::LambdaContext<Fn> Ctx;
  Ctx* ctx = new (std::nothrow) Ctx(static_cast<Fn&&>(fn));
  if (ctx == NULL) {
    return api::Result<TaskId>(
        api::Status(api::StatusCode::kInternalError, "failed to allocate lambda context"));
  }
  api::Result<TaskId> r = executor->SubmitWithKey(serial_key, &Ctx::Invoke, ctx);
  if (!r.ok()) {
    delete ctx;
  }
  return r;
}

}  // namespace task
}  // namespace corekit
