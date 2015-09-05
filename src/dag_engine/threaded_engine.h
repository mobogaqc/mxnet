/*!
 * Copyright (c) 2015 by Contributors
 */
#ifndef MXNET_DAG_ENGINE_THREADED_ENGINE_H_
#define MXNET_DAG_ENGINE_THREADED_ENGINE_H_

#include <dmlc/base.h>
#include <dmlc/concurrency.h>
#include <dmlc/logging.h>
#include <vector>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "dag_engine_impl.h"
#include "thread_pool.h"
#include "../common/object_pool.h"

namespace mxnet {

namespace engine {

/*!
 * \brief Forward declarations.
 */
struct ThreadedOpr;

/*!
 * \brief Operation in the queue.
 */
struct OprBlock : public common::ObjectPoolAllocatable<OprBlock> {
#if DAG_ENGINE_DEBUG
  static std::atomic<std::size_t> counter;
  OprBlock() { LOG(INFO) << __func__ << " " << ++counter; }
  ~OprBlock() { LOG(INFO) << __func__ << " " << --counter; }
#endif  // DAG_ENGINE_DEBUG
  std::atomic<std::size_t> wait{0};
  ThreadedOpr* opr{nullptr};
  Context ctx;
  RunContext rctx;
};  // struct OprBlock

/*!
 * \brief Variable with version information.
 */
struct VersionedVarBlock
    : public common::ObjectPoolAllocatable<VersionedVarBlock> {
#if DAG_ENGINE_DEBUG
  static std::atomic<std::size_t> counter;
  VersionedVarBlock() { LOG(INFO) << __func__ << " " << ++counter; }
  ~VersionedVarBlock() { LOG(INFO) << __func__ << " " << --counter; }
#endif  // DAG_ENGINE_DEBUG
  VersionedVarBlock* next{nullptr};
  OprBlock* trigger{nullptr};
  bool write{false};
};  // struct VersionedVarBlock

/*!
 * \brief Variable implementation.
 */
struct ThreadedVar final : public Var,
                           public common::ObjectPoolAllocatable<ThreadedVar> {
#if DAG_ENGINE_DEBUG
  static std::atomic<std::size_t> counter;
  ThreadedVar() { LOG(INFO) << __func__ << " " << ++counter; }
  ~ThreadedVar() { LOG(INFO) << __func__ << " " << --counter; }
#endif  // DAG_ENGINE_DEBUG
  std::mutex m;
  std::size_t num_pending_reads{0};
  VersionedVarBlock* head{nullptr};
  VersionedVarBlock* pending_write{nullptr};
  /*!
   * If true, then there are no current or future processing of the chain.
   */
  bool ready_to_read{true};
  /*!
   * If true, delete after operation completes.
   */
  bool to_delete{false};

  static ThreadedVar* CastFromBase(Var* ptr);
};  // struct ThreadedVar

/*!
 * \brief Operator implementation.
 */
struct ThreadedOpr final : public Opr,
                           public common::ObjectPoolAllocatable<ThreadedOpr> {
#if DAG_ENGINE_DEBUG
  static std::atomic<std::size_t> counter;
  ThreadedOpr() { LOG(INFO) << __func__ << " " << ++counter; }
  ~ThreadedOpr() { LOG(INFO) << __func__ << " " << --counter; }
#endif  // DAG_ENGINE_DEBUG
  DAGEngine::AsyncFn fn;
  std::vector<ThreadedVar*> use_vars;
  std::vector<ThreadedVar*> mutate_vars;
  bool temporary{false};

  static ThreadedOpr* CastFromBase(Opr* ptr);
  static ThreadedOpr* New();
  static void Delete(ThreadedOpr* ptr);
};  // struct ThreadedOpr

/*!
 * \brief Engine implementation.
 */
class ThreadedEngine final : public DAGEngine {
 public:
  /*!
   * \brief Constructor and destructor.
   */
  ThreadedEngine();
  ~ThreadedEngine() noexcept(false);
  /*!
   * \brief Overriding methods.
   */
  ThreadedVar* NewVar() override;
  ThreadedOpr* NewOperator(AsyncFn fn, std::vector<Variable> const& use_vars,
                           std::vector<Variable> const& mutate_vars) override;
  void DeleteOperator(OprHandle op) override;
  void Push(OprHandle op, Context exec_ctx) override;
  void Push(Fn exec_fun, Context exec_ctx,
            std::vector<Variable> const& use_vars,
            std::vector<Variable> const& mutate_vars) override;
  void PushAsync(AsyncFn exec_fun, Context exec_ctx,
                 std::vector<Variable> const& use_vars,
                 std::vector<Variable> const& mutate_vars) override;
  void PushDelete(Fn delete_fn, Context exec_ctx, Variable var) override;
  void WaitForVar(Variable var) override;
  void WaitForAll() override;
  /*!
   * \brief Callback on operation completion.
   *
   * On operation completion, this will trigger subsequent operations.
   */
  void OnComplete(ThreadedOpr* threaded_opr);
  /*!
   * \brief Worker.
   *
   * The method to pass to thread pool to parallelize.
   */
  void ThreadWorker();

 private:
  /*!
   * \brief Concurrency for thread pool.
   */
  static constexpr std::size_t kNumWorkingThreads = 16;
  /*!
   * \brief Number of pending operations.
   */
  std::atomic<std::size_t> pending_;
  /*!
   * \brief Notify waits for single or all variables.
   */
  std::mutex finished_m_;
  std::condition_variable finished_cv_;
  /*!
   * \brief Task queue.
   */
  dmlc::ConcurrentBlockingQueue<OprBlock*> task_queue_;
  /*!
   * \brief Thread pool.
   */
  ThreadPool<kNumWorkingThreads> thread_pool_;
  /*!
   * \brief Disallow copy construction and assignment.
   */
  DISALLOW_COPY_AND_ASSIGN(ThreadedEngine);
};  // class ThreadedEngine

}  // namespace engine

}  // namespace mxnet

#endif  // MXNET_DAG_ENGINE_THREADED_ENGINE_H_
