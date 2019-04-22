// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/page_capture_custom_bindings.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/web/web_blob.h"
#include "v8/include/v8.h"

namespace extensions {

PageCaptureCustomBindings::PageCaptureCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void PageCaptureCustomBindings::AddRoutes() {
  RouteHandlerFunction(
      "CreateBlob", "pageCapture",
      base::BindRepeating(&PageCaptureCustomBindings::CreateBlob,
                          base::Unretained(this)));
  RouteHandlerFunction(
      "SendResponseAck", "pageCapture",
      base::BindRepeating(&PageCaptureCustomBindings::SendResponseAck,
                          base::Unretained(this)));
}

void PageCaptureCustomBindings::CreateBlob(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 2);
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsInt32());
  v8::Isolate* isolate = args.GetIsolate();
  blink::WebString path(
      blink::WebString::FromUTF8(*v8::String::Utf8Value(isolate, args[0])));
  blink::WebBlob blob =
      blink::WebBlob::CreateFromFile(path, args[1].As<v8::Int32>()->Value());
  args.GetReturnValue().Set(
      blob.ToV8Value(context()->v8_context()->Global(), isolate));
}

void PageCaptureCustomBindings::SendResponseAck(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1);
  CHECK(args[0]->IsInt32());

  content::RenderFrame* render_frame = context()->GetRenderFrame();
  if (render_frame) {
    render_frame->Send(new ExtensionHostMsg_ResponseAck(
        render_frame->GetRoutingID(), args[0].As<v8::Int32>()->Value()));
  }
}

}  // namespace extensions
