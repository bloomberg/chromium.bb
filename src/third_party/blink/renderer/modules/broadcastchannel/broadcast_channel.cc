// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/broadcastchannel/broadcast_channel.h"

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// To ensure proper ordering of messages sent to/from multiple BroadcastChannel
// instances in the same thread, this uses one BroadcastChannelProvider
// connection as basis for all connections to channels from the same thread. The
// actual connections used to send/receive messages are then created using
// associated interfaces, ensuring proper message ordering. Note that this
// approach only works in the case of workers, since each worker has it's own
// thread.
mojo::Remote<mojom::blink::BroadcastChannelProvider>&
GetWorkerThreadSpecificProvider(WorkerGlobalScope& worker_global_scope) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<mojo::Remote<mojom::blink::BroadcastChannelProvider>>,
      provider, ());
  if (!provider.IsSet()) {
    worker_global_scope.GetBrowserInterfaceBroker().GetInterface(
        provider->BindNewPipeAndPassReceiver());
  }
  return *provider;
}

}  // namespace

// static
BroadcastChannel* BroadcastChannel::Create(ExecutionContext* execution_context,
                                           const String& name,
                                           ExceptionState& exception_state) {
  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(execution_context);
  if (window && window->IsCrossSiteSubframe())
    UseCounter::Count(window, WebFeature::kThirdPartyBroadcastChannel);

  if (execution_context->GetSecurityOrigin()->IsOpaque()) {
    // TODO(mek): Decide what to do here depending on
    // https://github.com/whatwg/html/issues/1319
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Can't create BroadcastChannel in an opaque origin");
    return nullptr;
  }
  return MakeGarbageCollected<BroadcastChannel>(execution_context, name);
}

BroadcastChannel::~BroadcastChannel() = default;

void BroadcastChannel::Dispose() {
  CloseInternal();
}

void BroadcastChannel::postMessage(const ScriptValue& message,
                                   ExceptionState& exception_state) {
  // If the receiver is not bound because `close` was called on this
  // BroadcastChannel instance, raise an exception per the spec. Otherwise,
  // in cases like the instance being created in an iframe that is now detached,
  // just ignore the postMessage call.
  if (!receiver_.is_bound()) {
    if (explicitly_closed_) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Channel is closed");
    }
    return;
  }

  // Silently ignore the postMessage call if this BroadcastChannel instance is
  // associated with a closing worker. This case needs to be handled explicitly
  // because the mojo connection to the worker won't be torn down until the
  // worker actually goes away.
  ExecutionContext* execution_context = GetExecutionContext();
  if (execution_context->IsWorkerGlobalScope()) {
    WorkerGlobalScope* worker_global_scope =
        DynamicTo<WorkerGlobalScope>(execution_context);
    DCHECK(worker_global_scope);
    if (worker_global_scope->IsClosing()) {
      return;
    }
  }

  scoped_refptr<SerializedScriptValue> value = SerializedScriptValue::Serialize(
      message.GetIsolate(), message.V8Value(),
      SerializedScriptValue::SerializeOptions(), exception_state);
  if (exception_state.HadException())
    return;

  BlinkCloneableMessage msg;
  msg.message = std::move(value);
  msg.sender_origin = execution_context->GetSecurityOrigin()->IsolatedCopy();
  remote_client_->OnMessage(std::move(msg));
}

void BroadcastChannel::close() {
  explicitly_closed_ = true;
  CloseInternal();
}

void BroadcastChannel::CloseInternal() {
  remote_client_.reset();
  if (receiver_.is_bound())
    receiver_.reset();
  if (associated_remote_.is_bound())
    associated_remote_.reset();
  feature_handle_for_scheduler_.reset();
}

const AtomicString& BroadcastChannel::InterfaceName() const {
  return event_target_names::kBroadcastChannel;
}

bool BroadcastChannel::HasPendingActivity() const {
  return receiver_.is_bound() && HasEventListeners(event_type_names::kMessage);
}

void BroadcastChannel::ContextDestroyed() {
  CloseInternal();
}

void BroadcastChannel::Trace(Visitor* visitor) const {
  ExecutionContextLifecycleObserver::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

void BroadcastChannel::OnMessage(BlinkCloneableMessage message) {
  // Queue a task to dispatch the event.
  MessageEvent* event;
  if (!message.locked_agent_cluster_id ||
      GetExecutionContext()->IsSameAgentCluster(
          *message.locked_agent_cluster_id)) {
    event = MessageEvent::Create(
        nullptr, std::move(message.message),
        GetExecutionContext()->GetSecurityOrigin()->ToString());
  } else {
    event = MessageEvent::CreateError(
        GetExecutionContext()->GetSecurityOrigin()->ToString());
  }
  // <specdef
  // href="https://html.spec.whatwg.org/multipage/web-messaging.html#dom-broadcastchannel-postmessage">
  // <spec>The tasks must use the DOM manipulation task source, and, for
  // those where the event loop specified by the target BroadcastChannel
  // object's BroadcastChannel settings object is a window event loop,
  // must be associated with the responsible document specified by that
  // target BroadcastChannel object's BroadcastChannel settings object.
  // </spec>
  DispatchEvent(*event);
}

void BroadcastChannel::OnError() {
  CloseInternal();
}

BroadcastChannel::BroadcastChannel(ExecutionContext* execution_context,
                                   const String& name)
    : ExecutionContextLifecycleObserver(execution_context),
      name_(name),
      feature_handle_for_scheduler_(
          execution_context->GetScheduler()->RegisterFeature(
              SchedulingPolicy::Feature::kBroadcastChannel,
              {SchedulingPolicy::DisableBackForwardCache()})) {
  // Note: We cannot associate per-frame task runner here, but postTask
  //       to it manually via EnqueueEvent, since the current expectation
  //       is to receive messages even after close for which queued before
  //       close.
  //       https://github.com/whatwg/html/issues/1319
  //       Relying on Mojo binding will cancel the enqueued messages
  //       at close().

  // The BroadcastChannel spec indicates that messages should be delivered to
  // BroadcastChannel objects in the order in which they were created, so it's
  // important that the ordering of ConnectToChannel messages (used to create
  // the corresponding state in the browser process) is preserved. We accomplish
  // this using two approaches, depending on the context:
  //
  //  - In the frame case, we create a new navigation associated remote for each
  //    BroadcastChannel instance and leverage it to ensure in-order delivery
  //    and delivery to the RenderFrameHostImpl object that corresponds to the
  //    current frame.
  //
  //  - In the worker case, since each worker runs in its own thread, we use a
  //    shared remote for all BroadcastChannel objects created on that thread to
  //    ensure in-order delivery of messages to the appropriate *WorkerHost
  //    object.
  if (execution_context->IsWindow()) {
    LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(execution_context);
    DCHECK(window);

    LocalFrame* frame = window->GetFrame();
    if (!frame) {
      return;
    }

    frame->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
        associated_remote_.BindNewEndpointAndPassReceiver());

    associated_remote_->ConnectToChannel(
        name_, receiver_.BindNewEndpointAndPassRemote(),
        remote_client_.BindNewEndpointAndPassReceiver());

  } else if (execution_context->IsWorkerGlobalScope()) {
    WorkerGlobalScope* worker_global_scope =
        DynamicTo<WorkerGlobalScope>(execution_context);
    DCHECK(worker_global_scope);

    if (worker_global_scope->IsClosing()) {
      return;
    }

    mojo::Remote<mojom::blink::BroadcastChannelProvider>& provider =
        GetWorkerThreadSpecificProvider(*worker_global_scope);

    provider->ConnectToChannel(name_, receiver_.BindNewEndpointAndPassRemote(),
                               remote_client_.BindNewEndpointAndPassReceiver());
  } else {
    NOTREACHED();
  }

  receiver_.set_disconnect_handler(
      WTF::Bind(&BroadcastChannel::OnError, WrapWeakPersistent(this)));
  remote_client_.set_disconnect_handler(
      WTF::Bind(&BroadcastChannel::OnError, WrapWeakPersistent(this)));
}

}  // namespace blink
