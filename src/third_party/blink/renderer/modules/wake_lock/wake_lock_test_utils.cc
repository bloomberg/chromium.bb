// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_test_utils.h"

#include <utility>

#include "base/logging.h"
#include "base/run_loop.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_type.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionStatus;

namespace {

// Helper class for WaitForPromise{Fulfillment,Rejection}(). It provides a
// function that invokes |callback| when a ScriptPromise is resolved.
class ClosureRunnerFunction final : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(
      ScriptState* script_state,
      base::RepeatingClosure callback) {
    auto* function = MakeGarbageCollected<ClosureRunnerFunction>(
        script_state, std::move(callback));
    return function->BindToV8Function();
  }

  ClosureRunnerFunction(ScriptState* script_state,
                        base::RepeatingClosure callback)
      : ScriptFunction(script_state), callback_(std::move(callback)) {}

 private:
  ScriptValue Call(ScriptValue) override {
    if (callback_)
      std::move(callback_).Run();
    return ScriptValue();
  }

  base::RepeatingClosure callback_;
};

WakeLockType ToBlinkWakeLockType(device::mojom::blink::WakeLockType type) {
  switch (type) {
    case device::mojom::blink::WakeLockType::kPreventDisplaySleep:
      return WakeLockType::kScreen;
    case device::mojom::blink::WakeLockType::kPreventAppSuspension:
      return WakeLockType::kSystem;
    default:
      NOTREACHED();
      return WakeLockType::kMaxValue;
  }
}

}  // namespace

// MockWakeLock

MockWakeLock::MockWakeLock() = default;
MockWakeLock::~MockWakeLock() = default;

void MockWakeLock::Bind(device::mojom::blink::WakeLockRequest request) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(
      WTF::Bind(&MockWakeLock::OnConnectionError, WTF::Unretained(this)));
}

void MockWakeLock::Unbind() {
  OnConnectionError();
}

void MockWakeLock::WaitForRequest() {
  DCHECK(!request_wake_lock_callback_);
  base::RunLoop run_loop;
  request_wake_lock_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void MockWakeLock::WaitForCancelation() {
  DCHECK(!cancel_wake_lock_callback_);
  base::RunLoop run_loop;
  cancel_wake_lock_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void MockWakeLock::OnConnectionError() {
  if (binding_.is_bound())
    binding_.Unbind();
  CancelWakeLock();
}

void MockWakeLock::RequestWakeLock() {
  is_acquired_ = true;
  if (request_wake_lock_callback_)
    std::move(request_wake_lock_callback_).Run();
}

void MockWakeLock::CancelWakeLock() {
  is_acquired_ = false;
  if (cancel_wake_lock_callback_)
    std::move(cancel_wake_lock_callback_).Run();
}

void MockWakeLock::AddClient(device::mojom::blink::WakeLockRequest) {}
void MockWakeLock::ChangeType(device::mojom::blink::WakeLockType,
                              ChangeTypeCallback) {}
void MockWakeLock::HasWakeLockForTests(HasWakeLockForTestsCallback) {}

// MockWakeLockService

MockWakeLockService::MockWakeLockService() = default;
MockWakeLockService::~MockWakeLockService() = default;

void MockWakeLockService::BindRequest(mojo::ScopedMessagePipeHandle handle) {
  bindings_.AddBinding(this,
                       mojom::blink::WakeLockServiceRequest(std::move(handle)));
}

MockWakeLock& MockWakeLockService::get_wake_lock(WakeLockType type) {
  size_t pos = static_cast<size_t>(type);
  return mock_wake_lock_[pos];
}

void MockWakeLockService::GetWakeLock(
    device::mojom::blink::WakeLockType type,
    device::mojom::blink::WakeLockReason reason,
    const String& description,
    device::mojom::blink::WakeLockRequest request) {
  size_t pos = static_cast<size_t>(ToBlinkWakeLockType(type));
  mock_wake_lock_[pos].Bind(std::move(request));
}

// MockPermissionService

MockPermissionService::MockPermissionService() = default;
MockPermissionService::~MockPermissionService() = default;

void MockPermissionService::BindRequest(mojo::ScopedMessagePipeHandle handle) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(mojom::blink::PermissionServiceRequest(std::move(handle)));
  binding_.set_connection_error_handler(WTF::Bind(
      &MockPermissionService::OnConnectionError, WTF::Unretained(this)));
}

void MockPermissionService::SetPermissionResponse(WakeLockType type,
                                                  PermissionStatus status) {
  DCHECK(status == PermissionStatus::GRANTED ||
         status == PermissionStatus::DENIED);
  permission_responses_[static_cast<size_t>(type)] = status;
}

void MockPermissionService::OnConnectionError() {
  binding_.Unbind();
}

bool MockPermissionService::GetWakeLockTypeFromDescriptor(
    const PermissionDescriptorPtr& descriptor,
    WakeLockType* output) {
  if (!descriptor->extension || !descriptor->extension->is_wake_lock())
    return false;
  switch (descriptor->extension->get_wake_lock()->type) {
    case mojom::blink::WakeLockType::kScreen:
      *output = WakeLockType::kScreen;
      return true;
    case mojom::blink::WakeLockType::kSystem:
      *output = WakeLockType::kSystem;
      return true;
    default:
      return false;
  }
}

void MockPermissionService::WaitForPermissionRequest(WakeLockType type) {
  size_t pos = static_cast<size_t>(type);
  DCHECK(!request_permission_callbacks_[pos]);
  base::RunLoop run_loop;
  request_permission_callbacks_[pos] = run_loop.QuitClosure();
  run_loop.Run();
}

void MockPermissionService::HasPermission(PermissionDescriptorPtr permission,
                                          HasPermissionCallback callback) {
  WakeLockType type;
  if (!GetWakeLockTypeFromDescriptor(permission, &type)) {
    std::move(callback).Run(PermissionStatus::DENIED);
    return;
  }
  size_t pos = static_cast<size_t>(type);
  DCHECK(permission_responses_[pos].has_value());
  std::move(callback).Run(
      permission_responses_[pos].value_or(PermissionStatus::DENIED));
}

void MockPermissionService::RequestPermission(
    PermissionDescriptorPtr permission,
    bool user_gesture,
    RequestPermissionCallback callback) {
  WakeLockType type;
  if (!GetWakeLockTypeFromDescriptor(permission, &type)) {
    std::move(callback).Run(PermissionStatus::DENIED);
    return;
  }

  size_t pos = static_cast<size_t>(type);
  DCHECK(permission_responses_[pos].has_value());
  if (request_permission_callbacks_[pos])
    std::move(request_permission_callbacks_[pos]).Run();
  std::move(callback).Run(
      permission_responses_[pos].value_or(PermissionStatus::DENIED));
}

void MockPermissionService::RequestPermissions(
    Vector<PermissionDescriptorPtr> permissions,
    bool user_gesture,
    mojom::blink::PermissionService::RequestPermissionsCallback) {
  NOTREACHED();
}

void MockPermissionService::RevokePermission(PermissionDescriptorPtr permission,
                                             RevokePermissionCallback) {
  NOTREACHED();
}

void MockPermissionService::AddPermissionObserver(
    PermissionDescriptorPtr permission,
    PermissionStatus last_known_status,
    mojom::blink::PermissionObserverPtr) {
  NOTREACHED();
}

// WakeLockTestingContext

WakeLockTestingContext::WakeLockTestingContext(
    MockWakeLockService* mock_wake_lock_service) {
  service_manager::InterfaceProvider::TestApi test_api(
      GetDocument()->GetInterfaceProvider());
  test_api.SetBinderForName(
      mojom::blink::WakeLockService::Name_,
      WTF::BindRepeating(&MockWakeLockService::BindRequest,
                         WTF::Unretained(mock_wake_lock_service)));
  test_api.SetBinderForName(
      mojom::blink::PermissionService::Name_,
      WTF::BindRepeating(&MockPermissionService::BindRequest,
                         WTF::Unretained(&permission_service_)));
}

Document* WakeLockTestingContext::GetDocument() {
  return &testing_scope_.GetDocument();
}

LocalFrame* WakeLockTestingContext::Frame() {
  return &testing_scope_.GetFrame();
}

ScriptState* WakeLockTestingContext::GetScriptState() {
  return testing_scope_.GetScriptState();
}

MockPermissionService& WakeLockTestingContext::GetPermissionService() {
  return permission_service_;
}

ScriptPromise WakeLockTestingContext::WaitForPromiseFulfillment(
    ScriptPromise promise) {
  base::RunLoop run_loop;
  ScriptPromise return_promise =
      promise.Then(ClosureRunnerFunction::CreateFunction(
          GetScriptState(), run_loop.QuitClosure()));
  // Execute pending microtasks, otherwise it can take a few seconds for the
  // promise to resolve.
  v8::MicrotasksScope::PerformCheckpoint(GetScriptState()->GetIsolate());
  run_loop.Run();
  return return_promise;
}

// Synchronously waits for |promise| to be rejected.
void WakeLockTestingContext::WaitForPromiseRejection(ScriptPromise promise) {
  base::RunLoop run_loop;
  promise.Then(v8::Local<v8::Function>(),
               ClosureRunnerFunction::CreateFunction(GetScriptState(),
                                                     run_loop.QuitClosure()));
  // Execute pending microtasks, otherwise it can take a few seconds for the
  // promise to resolve.
  v8::MicrotasksScope::PerformCheckpoint(GetScriptState()->GetIsolate());
  run_loop.Run();
}

// ScriptPromiseUtils

// static
v8::Promise::PromiseState ScriptPromiseUtils::GetPromiseState(
    const ScriptPromise& promise) {
  return promise.V8Value().As<v8::Promise>()->State();
}

// static
String ScriptPromiseUtils::GetPromiseResolutionAsString(
    const ScriptPromise& promise) {
  auto v8_promise = promise.V8Value().As<v8::Promise>();
  if (v8_promise->State() == v8::Promise::kPending) {
    return g_empty_string;
  }
  ScriptValue promise_result(promise.GetScriptValue().GetScriptState(),
                             v8_promise->Result());
  String value;
  if (!promise_result.ToString(value)) {
    return g_empty_string;
  }
  return value;
}

// static
DOMException* ScriptPromiseUtils::GetPromiseResolutionAsDOMException(
    const ScriptPromise& promise) {
  return V8DOMException::ToImplWithTypeCheck(
      promise.GetIsolate(), promise.V8Value().As<v8::Promise>()->Result());
}

}  // namespace blink
