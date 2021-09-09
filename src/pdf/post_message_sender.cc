// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/post_message_sender.h"

#include <memory>

#include "base/check_op.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_dom_message_event.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "v8/include/v8.h"

namespace chrome_pdf {

PostMessageSender::PostMessageSender() : isolate_(blink::MainThreadIsolate()) {}

PostMessageSender::~PostMessageSender() = default;

// TODO(crbug.com/1109796): This method is currently called from the renderer
// main thread, but will be called from a dedicated plugin thread in the future.
// When that happens, the body of this method needs to be posted to the main
// thread as a task because that's where the Blink and V8 interactions need to
// occur.
void PostMessageSender::Post(base::Value message) {
  // Drop messages if there is no container.
  if (!container_)
    return;

  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      container_->GetDocument().GetFrame()->MainWorldScriptContext();
  DCHECK_EQ(isolate_, context->GetIsolate());
  v8::Context::Scope context_scope(context);

  if (!v8_value_converter_)
    v8_value_converter_ = content::V8ValueConverter::Create();

  v8::Local<v8::Value> converted_message =
      v8_value_converter_->ToV8Value(&message, context);

  container_->EnqueueMessageEvent(
      blink::WebSerializedScriptValue::Serialize(isolate_, converted_message));
}

}  // namespace chrome_pdf
