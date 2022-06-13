// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/post_message_receiver.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "gin/function_template.h"
#include "gin/handle.h"
#include "gin/interceptor.h"
#include "gin/object_template_builder.h"
#include "gin/public/wrapper_info.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace chrome_pdf {

namespace {

constexpr char kPropertyName[] = "postMessage";

}  // namespace

// static
gin::WrapperInfo PostMessageReceiver::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
v8::Local<v8::Object> PostMessageReceiver::Create(
    v8::Isolate* isolate,
    base::WeakPtr<Client> client,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner) {
  return gin::CreateHandle(
             isolate, new PostMessageReceiver(isolate, std::move(client),
                                              std::move(client_task_runner)))
      .ToV8()
      .As<v8::Object>();
}

PostMessageReceiver::~PostMessageReceiver() = default;

PostMessageReceiver::PostMessageReceiver(
    v8::Isolate* isolate,
    base::WeakPtr<Client> client,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner)
    : gin::NamedPropertyInterceptor(isolate, this),
      isolate_(isolate),
      client_(std::move(client)),
      client_task_runner_(std::move(client_task_runner)) {}

gin::ObjectTemplateBuilder PostMessageReceiver::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  // `gin::ObjectTemplateBuilder::SetMethod()` can't be used here because it
  // would create a function template which expects the first parameter to a
  // member function pointer to be the JavaScript `this` object corresponding
  // to this scriptable object exposed through Blink. However, the actual
  // receiving object for a plugin is an HTMLEmbedElement and Blink internally
  // forwards the parameters to this scriptable object.
  //
  // Also, passing a callback would cause Gin to ignore the target. Because Gin
  // creates the object template of a type only once per isolate, the member
  // method of the first `PostMessageReceiver` instance would get effectively
  // treated like a static method for all other instances.
  //
  // An interceptor allows for the creation of a function template per instance.
  return gin::Wrappable<PostMessageReceiver>::GetObjectTemplateBuilder(isolate)
      .AddNamedPropertyInterceptor();
}

const char* PostMessageReceiver::GetTypeName() {
  return "ChromePdfPostMessageReceiver";
}

v8::Local<v8::Value> PostMessageReceiver::GetNamedProperty(
    v8::Isolate* isolate,
    const std::string& property) {
  DCHECK_EQ(isolate_, isolate);

  if (property != kPropertyName)
    return v8::Local<v8::Value>();

  return GetFunctionTemplate()
      ->GetFunction(isolate->GetCurrentContext())
      .ToLocalChecked();
}

std::vector<std::string> PostMessageReceiver::EnumerateNamedProperties(
    v8::Isolate* isolate) {
  DCHECK_EQ(isolate_, isolate);
  return {kPropertyName};
}

v8::Local<v8::FunctionTemplate> PostMessageReceiver::GetFunctionTemplate() {
  if (function_template_.IsEmpty()) {
    function_template_.Reset(
        isolate_,
        gin::CreateFunctionTemplate(
            isolate_, base::BindRepeating(&PostMessageReceiver::PostMessage,
                                          weak_factory_.GetWeakPtr())));
  }

  return function_template_.Get(isolate_);
}

std::unique_ptr<base::Value> PostMessageReceiver::ConvertMessage(
    v8::Local<v8::Value> message) {
  if (!v8_value_converter_)
    v8_value_converter_ = content::V8ValueConverter::Create();

  return v8_value_converter_->FromV8Value(message,
                                          isolate_->GetCurrentContext());
}

void PostMessageReceiver::PostMessage(v8::Local<v8::Value> message) {
  if (!client_)
    return;

  std::unique_ptr<base::Value> converted_message = ConvertMessage(message);
  DCHECK(converted_message) << "The PDF Viewer UI should not be sending "
                               "messages that cannot be converted.";

  client_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(&Client::OnMessage, client_,
                                               std::move(*converted_message)));
}

}  // namespace chrome_pdf
