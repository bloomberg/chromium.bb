// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/url_selection_operation_handler.h"

#include "content/services/shared_storage_worklet/worklet_v8_helper.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-promise.h"
#include "v8/include/v8-value-serializer.h"

namespace shared_storage_worklet {

struct UrlSelectionOperationHandler::PendingRequest {
  explicit PendingRequest(
      mojom::SharedStorageWorkletService::RunURLSelectionOperationCallback
          callback);

  ~PendingRequest();

  mojom::SharedStorageWorkletService::RunURLSelectionOperationCallback callback;
};

UrlSelectionOperationHandler::PendingRequest::PendingRequest(
    mojom::SharedStorageWorkletService::RunURLSelectionOperationCallback
        callback)
    : callback(std::move(callback)) {}

UrlSelectionOperationHandler::PendingRequest::~PendingRequest() = default;

UrlSelectionOperationHandler::UrlSelectionOperationHandler() = default;

UrlSelectionOperationHandler::~UrlSelectionOperationHandler() = default;

void UrlSelectionOperationHandler::RegisterOperation(gin::Arguments* args) {
  std::string name;
  if (!args->GetNext(&name)) {
    args->ThrowTypeError("Missing \"name\" argument in operation registration");
    return;
  }

  if (name.empty()) {
    args->ThrowTypeError("Operation name cannot be empty");
    return;
  }

  if (operation_definition_map_.count(name)) {
    args->ThrowTypeError("Operation name already registered");
    return;
  }

  v8::Local<v8::Object> class_definition;
  if (!args->GetNext(&class_definition)) {
    args->ThrowTypeError(
        "Missing class name argument in operation registration");
    return;
  }

  if (!class_definition->IsConstructor()) {
    args->ThrowTypeError("Unexpected class argument: not a constructor");
    return;
  }

  v8::Isolate* isolate = args->isolate();
  v8::Local<v8::Context> context = args->GetHolderCreationContext();

  v8::Local<v8::Value> class_prototype =
      class_definition->Get(context, gin::StringToV8(isolate, "prototype"))
          .ToLocalChecked();

  v8::Local<v8::Value> run_function =
      class_prototype.As<v8::Object>()
          ->Get(context, gin::StringToV8(isolate, "run"))
          .ToLocalChecked();

  if (run_function->IsUndefined() || !run_function->IsFunction()) {
    args->ThrowTypeError("Missing \"run()\" function in the class");
    return;
  }

  operation_definition_map_.emplace(
      name, v8::Global<v8::Function>(isolate, run_function.As<v8::Function>()));
}

void UrlSelectionOperationHandler::RunOperation(
    v8::Local<v8::Context> context,
    const std::string& name,
    const std::vector<std::string>& urls,
    const std::vector<uint8_t>& serialized_data,
    mojom::SharedStorageWorkletService::RunURLSelectionOperationCallback
        callback) {
  auto it = operation_definition_map_.find(name);
  if (it == operation_definition_map_.end()) {
    std::move(callback).Run(/*success=*/false, "Cannot find operation name.",
                            /*index=*/0);
    return;
  }

  v8::Isolate* isolate = context->GetIsolate();

  v8::Context::Scope context_scope(context);

  v8::Local<v8::Function> run_function = it->second.Get(isolate);

  v8::Local<v8::Array> js_urls =
      gin::Converter<std::vector<std::string>>::ToV8(isolate, urls)
          .As<v8::Array>();

  v8::Local<v8::Object> js_data;

  if (serialized_data.empty()) {
    js_data = v8::Object::New(isolate);
  } else {
    v8::ValueDeserializer deserializer(isolate, serialized_data.data(),
                                       serialized_data.size());
    v8::Local<v8::Value> value =
        deserializer.ReadValue(context).ToLocalChecked();
    js_data = value->ToObject(context).ToLocalChecked();
  }

  std::vector<v8::Local<v8::Value>> args{js_urls, js_data};

  std::string error_message;
  v8::MaybeLocal<v8::Value> result = WorkletV8Helper::InvokeFunction(
      context, run_function, args, &error_message);

  if (result.IsEmpty()) {
    std::move(callback).Run(
        /*success=*/false, error_message, /*index=*/0);
    return;
  }

  if (!result.ToLocalChecked()->IsPromise()) {
    std::move(callback).Run(/*success=*/false,
                            "run() did not return a promise.",
                            /*index=*/0);
    return;
  }

  v8::Local<v8::Promise> result_promise =
      result.ToLocalChecked().As<v8::Promise>();

  // If the promise is already completed, retrieve and handle the result
  // directly.
  if (result_promise->State() == v8::Promise::PromiseState::kFulfilled) {
    v8::Local<v8::Value> result_value = result_promise->Result();
    if (!result_value->IsUint32()) {
      std::move(callback).Run(/*success=*/false,
                              "Promise did not resolve to an uint32 number.",
                              /*index=*/0);
      return;
    }
    uint32_t result_index = result_value->Uint32Value(context).FromJust();
    std::move(callback).Run(/*success=*/true,
                            /*error_message=*/{}, result_index);
    return;
  }

  if (result_promise->State() == v8::Promise::PromiseState::kRejected) {
    error_message = gin::V8ToString(
        isolate,
        result_promise->Result()->ToDetailString(context).ToLocalChecked());

    std::move(callback).Run(
        /*success=*/false, error_message, /*index=*/0);
    return;
  }

  // If the promise is pending, install callback functions that will be
  // triggered when it completes.
  auto pending_request = std::make_unique<PendingRequest>(std::move(callback));
  PendingRequest* pending_request_raw = pending_request.get();
  pending_requests_.emplace(pending_request_raw, std::move(pending_request));

  v8::Local<v8::Function> fulfilled_callback =
      gin::CreateFunctionTemplate(
          isolate, base::BindRepeating(
                       &UrlSelectionOperationHandler::OnPromiseFulfilled,
                       weak_ptr_factory_.GetWeakPtr(), pending_request_raw))
          ->GetFunction(context)
          .ToLocalChecked();

  v8::Local<v8::Function> rejected_callback =
      gin::CreateFunctionTemplate(
          isolate, base::BindRepeating(
                       &UrlSelectionOperationHandler::OnPromiseRejected,
                       weak_ptr_factory_.GetWeakPtr(), pending_request_raw))
          ->GetFunction(context)
          .ToLocalChecked();

  result_promise->Then(context, fulfilled_callback, rejected_callback)
      .ToLocalChecked();
}

void UrlSelectionOperationHandler::OnPromiseFulfilled(PendingRequest* request,
                                                      gin::Arguments* args) {
  uint32_t result_index = 0;
  if (!args->GetNext(&result_index)) {
    std::move(request->callback)
        .Run(/*success=*/false, "Promise did not resolve to an uint32 number.",
             /*index=*/0);
    pending_requests_.erase(request);
    return;
  }

  std::move(request->callback)
      .Run(/*success=*/true,
           /*error_message=*/{}, result_index);
  pending_requests_.erase(request);
}

void UrlSelectionOperationHandler::OnPromiseRejected(PendingRequest* request,
                                                     gin::Arguments* args) {
  std::string error_message;
  if (!args->GetNext(&error_message)) {
    std::move(request->callback)
        .Run(/*success=*/false,
             "Promise is rejected without an explicit error message.",
             /*index=*/0);
    pending_requests_.erase(request);
    return;
  }

  std::move(request->callback)
      .Run(/*success=*/false, error_message, /*index=*/0);

  pending_requests_.erase(request);
}

}  // namespace shared_storage_worklet
