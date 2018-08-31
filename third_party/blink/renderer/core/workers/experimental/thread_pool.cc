// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/experimental/thread_pool.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/dedicated_worker_factory.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_messaging_proxy.h"
#include "third_party/blink/renderer/core/workers/threaded_messaging_proxy_base.h"
#include "third_party/blink/renderer/core/workers/threaded_object_proxy_base.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {

template <wtf_size_t inlineCapacity, typename Allocator>
struct CrossThreadCopier<
    Vector<scoped_refptr<SerializedScriptValue>, inlineCapacity, Allocator>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type =
      Vector<scoped_refptr<SerializedScriptValue>, inlineCapacity, Allocator>;
  static Type Copy(Type pointer) {
    return pointer;  // This is in fact a move.
  }
};

class ThreadPoolWorkerGlobalScope final : public WorkerGlobalScope {
 public:
  ThreadPoolWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params,
      WorkerThread* thread)
      : WorkerGlobalScope(std::move(creation_params),
                          thread,
                          CurrentTimeTicks()) {}

  ~ThreadPoolWorkerGlobalScope() override = default;

  // EventTarget
  const AtomicString& InterfaceName() const override {
    // TODO(japhet): Replaces this with
    // EventTargetNames::ThreadPoolWorkerGlobalScope.
    return EventTargetNames::DedicatedWorkerGlobalScope;
  }

  // WorkerGlobalScope
  void ImportModuleScript(
      const KURL& module_url_record,
      FetchClientSettingsObjectSnapshot* outside_settings_object,
      network::mojom::FetchCredentialsMode) override {
    // TODO(japhet): Consider whether modules should be supported.
    NOTREACHED();
  }

  void ExceptionThrown(ErrorEvent*) override {}
};

class ThreadPoolObjectProxy final : public ThreadedObjectProxyBase {
 public:
  ThreadPoolObjectProxy(ThreadPoolMessagingProxy* messaging_proxy,
                        ParentExecutionContextTaskRunners* task_runners)
      : ThreadedObjectProxyBase(task_runners),
        messaging_proxy_(messaging_proxy) {}
  ~ThreadPoolObjectProxy() override = default;

  void DidCreateWorkerGlobalScope(
      WorkerOrWorkletGlobalScope* global_scope) override {
    global_scope_ = static_cast<ThreadPoolWorkerGlobalScope*>(global_scope);
  }
  void WillDestroyWorkerGlobalScope() override { global_scope_ = nullptr; }
  CrossThreadWeakPersistent<ThreadedMessagingProxyBase> MessagingProxyWeakPtr()
      override {
    return messaging_proxy_;
  }

  void ProcessTask(scoped_refptr<SerializedScriptValue> task,
                   Vector<scoped_refptr<SerializedScriptValue>> arguments,
                   size_t task_id,
                   const v8_inspector::V8StackTraceId&);

  void AbortTask(size_t task_id) { cancelled_tasks_.insert(task_id); }

 private:
  CrossThreadWeakPersistent<ThreadPoolMessagingProxy> messaging_proxy_;
  CrossThreadPersistent<ThreadPoolWorkerGlobalScope> global_scope_;
  HashSet<size_t> cancelled_tasks_;
};

class ThreadPoolThread final : public WorkerThread {
 public:
  ThreadPoolThread(ExecutionContext* parent_execution_context,
                   ThreadPoolObjectProxy& object_proxy)
      : WorkerThread(object_proxy) {
    FrameOrWorkerScheduler* scheduler =
        parent_execution_context ? parent_execution_context->GetScheduler()
                                 : nullptr;
    worker_backing_thread_ =
        WorkerBackingThread::Create(WebThreadCreationParams(GetThreadType())
                                        .SetFrameOrWorkerScheduler(scheduler));
  }
  ~ThreadPoolThread() override = default;

 private:
  WorkerBackingThread& GetWorkerBackingThread() override {
    return *worker_backing_thread_;
  }
  void ClearWorkerBackingThread() override { worker_backing_thread_ = nullptr; }

  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params) override {
    return new ThreadPoolWorkerGlobalScope(std::move(creation_params), this);
  }

  WebThreadType GetThreadType() const override {
    // TODO(japhet): Replace with WebThreadType::kThreadPoolWorkerThread.
    return WebThreadType::kDedicatedWorkerThread;
  }
  std::unique_ptr<WorkerBackingThread> worker_backing_thread_;
};

class ThreadPoolMessagingProxy final : public ThreadedMessagingProxyBase {
 public:
  ThreadPoolMessagingProxy(ExecutionContext* context, ThreadPool* thread_pool)
      : ThreadedMessagingProxyBase(context), thread_pool_(thread_pool) {
    object_proxy_ = std::make_unique<ThreadPoolObjectProxy>(
        this, GetParentExecutionContextTaskRunners());
  }
  ~ThreadPoolMessagingProxy() override = default;

  void StartWorker(std::unique_ptr<GlobalScopeCreationParams> creation_params) {
    InitializeWorkerThread(std::move(creation_params),
                           WorkerBackingThreadStartupData::CreateDefault());
  }
  std::unique_ptr<WorkerThread> CreateWorkerThread() override {
    return std::make_unique<ThreadPoolThread>(GetExecutionContext(),
                                              *object_proxy_.get());
  }

  void PostTaskToWorkerGlobalScope(
      ExecutionContext* context,
      scoped_refptr<SerializedScriptValue> task,
      const Vector<scoped_refptr<SerializedScriptValue>>& arguments,
      size_t task_id,
      TaskType task_type) {
    v8_inspector::V8StackTraceId stack_id =
        ThreadDebugger::From(ToIsolate(context))
            ->StoreCurrentStackTrace("ThreadPool.postTask");
    PostCrossThreadTask(
        *GetWorkerThread()->GetTaskRunner(task_type), FROM_HERE,
        CrossThreadBind(&ThreadPoolObjectProxy::ProcessTask,
                        CrossThreadUnretained(object_proxy_.get()),
                        std::move(task), std::move(arguments), task_id,
                        stack_id));
  }

  void PostAbortToWorkerGlobalScope(size_t task_id) {
    PostCrossThreadTask(
        *GetWorkerThread()->GetControlTaskRunner(), FROM_HERE,
        CrossThreadBind(&ThreadPoolObjectProxy::AbortTask,
                        CrossThreadUnretained(object_proxy_.get()), task_id));
  }

  void TaskCompleted(size_t task_id,
                     scoped_refptr<SerializedScriptValue> result) {
    if (thread_pool_)
      thread_pool_->TaskCompleted(task_id, std::move(result));
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(thread_pool_);
    ThreadedMessagingProxyBase::Trace(visitor);
  }

 private:
  std::unique_ptr<ThreadPoolObjectProxy> object_proxy_;
  Member<ThreadPool> thread_pool_;
};

void ThreadPoolObjectProxy::ProcessTask(
    scoped_refptr<SerializedScriptValue> task,
    Vector<scoped_refptr<SerializedScriptValue>> arguments,
    size_t task_id,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(global_scope_->IsContextThread());

  if (cancelled_tasks_.Contains(task_id)) {
    cancelled_tasks_.erase(task_id);
    PostCrossThreadTask(
        *GetParentExecutionContextTaskRunners()->Get(TaskType::kPostedMessage),
        FROM_HERE,
        CrossThreadBind(&ThreadPoolMessagingProxy::TaskCompleted,
                        messaging_proxy_, task_id, nullptr));
    return;
  }

  v8::Isolate* isolate = ToIsolate(global_scope_.Get());
  ThreadDebugger* debugger = ThreadDebugger::From(isolate);
  debugger->ExternalAsyncTaskStarted(stack_id);

  ScriptState::Scope scope(global_scope_->ScriptController()->GetScriptState());
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  String core_script =
      "(" + ToCoreString(task->Deserialize(isolate).As<v8::String>()) + ")";
  v8::MaybeLocal<v8::Script> script = v8::Script::Compile(
      isolate->GetCurrentContext(), V8String(isolate, core_script));
  v8::Local<v8::Function> script_function =
      script.ToLocalChecked()->Run(context).ToLocalChecked().As<v8::Function>();

  Vector<v8::Local<v8::Value>> params(arguments.size());
  for (size_t i = 0; i < arguments.size(); i++) {
    params[i] = arguments[i]->Deserialize(isolate);
  }

  v8::MaybeLocal<v8::Value> ret = script_function->Call(
      context, script_function, params.size(), params.data());
  scoped_refptr<SerializedScriptValue> result =
      ret.IsEmpty() ? nullptr
                    : SerializedScriptValue::SerializeAndSwallowExceptions(
                          isolate, ret.ToLocalChecked());

  debugger->ExternalAsyncTaskFinished(stack_id);
  // TODO(japhet): Is it ok to always send the completion notification back on
  // the same task queue, or should this be the task type sent to the worker?
  PostCrossThreadTask(
      *GetParentExecutionContextTaskRunners()->Get(TaskType::kPostedMessage),
      FROM_HERE,
      CrossThreadBind(&ThreadPoolMessagingProxy::TaskCompleted,
                      messaging_proxy_, task_id, std::move(result)));
}

service_manager::mojom::blink::InterfaceProviderPtrInfo
ConnectToWorkerInterfaceProviderForThreadPool(
    ExecutionContext* execution_context,
    scoped_refptr<const SecurityOrigin> script_origin) {
  // TODO(japhet): Implement a proper factory.
  mojom::blink::DedicatedWorkerFactoryPtr worker_factory;
  execution_context->GetInterfaceProvider()->GetInterface(&worker_factory);
  service_manager::mojom::blink::InterfaceProviderPtrInfo
      interface_provider_ptr;
  worker_factory->CreateDedicatedWorker(
      script_origin, mojo::MakeRequest(&interface_provider_ptr));
  return interface_provider_ptr;
}

const char ThreadPool::kSupplementName[] = "ThreadPool";

ThreadPool* ThreadPool::From(Document& document) {
  ThreadPool* thread_pool = Supplement<Document>::From<ThreadPool>(document);
  if (!thread_pool) {
    thread_pool = new ThreadPool(document);
    Supplement<Document>::ProvideTo(document, thread_pool);
  }
  return thread_pool;
}

static const size_t kProxyCount = 2;

ThreadPool::ThreadPool(Document& document)
    : Supplement<Document>(document),
      document_(document),
      context_proxies_(kProxyCount) {}

ThreadPoolMessagingProxy* ThreadPool::GetProxyForTaskType(TaskType task_type) {
  DCHECK(document_->IsContextThread());
  size_t proxy_id = kProxyCount;
  if (task_type == TaskType::kUserInteraction)
    proxy_id = 0u;
  else if (task_type == TaskType::kIdleTask)
    proxy_id = 1u;
  DCHECK_LT(proxy_id, kProxyCount);

  if (!context_proxies_[proxy_id]) {
    base::UnguessableToken devtools_worker_token =
        document_->GetFrame() ? document_->GetFrame()->GetDevToolsFrameToken()
                              : base::UnguessableToken::Create();
    ExecutionContext* context = document_.Get();

    context_proxies_[proxy_id] = new ThreadPoolMessagingProxy(context, this);
    std::unique_ptr<WorkerSettings> settings =
        std::make_unique<WorkerSettings>(document_->GetSettings());

    context_proxies_[proxy_id]->StartWorker(
        std::make_unique<GlobalScopeCreationParams>(
            context->Url(), ScriptType::kClassic, context->UserAgent(),
            context->GetContentSecurityPolicy()->Headers(),
            kReferrerPolicyDefault, context->GetSecurityOrigin(),
            context->IsSecureContext(), WorkerClients::Create(),
            context->GetSecurityContext().AddressSpace(),
            OriginTrialContext::GetTokens(context).get(), devtools_worker_token,
            std::move(settings), kV8CacheOptionsDefault,
            nullptr /* worklet_module_responses_map */,
            ConnectToWorkerInterfaceProviderForThreadPool(
                context, context->GetSecurityOrigin())));
  }
  return context_proxies_[proxy_id];
}

void ThreadPool::PostTask(
    scoped_refptr<SerializedScriptValue> task,
    ScriptPromiseResolver* resolver,
    AbortSignal* signal,
    const Vector<scoped_refptr<SerializedScriptValue>>& arguments,
    TaskType task_type) {
  DCHECK(document_->IsContextThread());

  GetProxyForTaskType(task_type)->PostTaskToWorkerGlobalScope(
      document_.Get(), std::move(task), arguments, next_task_id_, task_type);
  resolvers_.insert(next_task_id_, resolver);
  if (signal) {
    signal->AddAlgorithm(WTF::Bind(&ThreadPool::AbortTask, WrapPersistent(this),
                                   next_task_id_, task_type));
  }
  next_task_id_++;
}

void ThreadPool::AbortTask(size_t task_id, TaskType task_type) {
  DCHECK(document_->IsContextThread());
  GetProxyForTaskType(task_type)->PostAbortToWorkerGlobalScope(task_id);
}

void ThreadPool::TaskCompleted(size_t task_id,
                               scoped_refptr<SerializedScriptValue> result) {
  DCHECK(document_->IsContextThread());
  DCHECK(resolvers_.Contains(task_id));
  ScriptPromiseResolver* resolver = resolvers_.Take(task_id);
  if (!result) {
    resolver->Reject();
    return;
  }
  ScriptState::Scope scope(resolver->GetScriptState());
  resolver->Resolve(
      result->Deserialize(resolver->GetScriptState()->GetIsolate()));
}

void ThreadPool::Trace(blink::Visitor* visitor) {
  Supplement<Document>::Trace(visitor);
  visitor->Trace(document_);
  visitor->Trace(context_proxies_);
  visitor->Trace(resolvers_);
}

}  // namespace blink
