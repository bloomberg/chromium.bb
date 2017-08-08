/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/exported/WebDevToolsFrontendImpl.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8DevToolsHost.h"
#include "core/exported/WebViewImpl.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/inspector/DevToolsHost.h"
#include "core/page/Page.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebString.h"
#include "public/web/WebDevToolsFrontendClient.h"

namespace blink {

WebDevToolsFrontend* WebDevToolsFrontend::Create(
    WebLocalFrame* frame,
    WebDevToolsFrontendClient* client) {
  return new WebDevToolsFrontendImpl(ToWebLocalFrameImpl(frame), client);
}

WebDevToolsFrontendImpl::WebDevToolsFrontendImpl(
    WebLocalFrameImpl* web_frame,
    WebDevToolsFrontendClient* client)
    : web_frame_(web_frame), client_(client) {
  web_frame_->SetDevToolsFrontend(this);
  web_frame_->GetFrame()->GetPage()->SetDefaultPageScaleLimits(1.f, 1.f);
}

WebDevToolsFrontendImpl::~WebDevToolsFrontendImpl() {
  if (devtools_host_)
    devtools_host_->DisconnectClient();
}

void WebDevToolsFrontendImpl::DidClearWindowObject(WebLocalFrameImpl* frame) {
  if (web_frame_ != frame)
    return;
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  // Use higher limit for DevTools isolate so that it does not OOM when
  // profiling large heaps.
  isolate->IncreaseHeapLimitForDebugging();
  ScriptState* script_state = ToScriptStateForMainWorld(web_frame_->GetFrame());
  DCHECK(script_state);
  ScriptState::Scope scope(script_state);

  if (devtools_host_)
    devtools_host_->DisconnectClient();
  devtools_host_ = DevToolsHost::Create(this, web_frame_->GetFrame());
  v8::Local<v8::Object> global = script_state->GetContext()->Global();
  v8::Local<v8::Value> devtools_host_obj =
      ToV8(devtools_host_.Get(), global, script_state->GetIsolate());
  DCHECK(!devtools_host_obj.IsEmpty());
  global->Set(V8AtomicString(isolate, "DevToolsHost"), devtools_host_obj);
}

void WebDevToolsFrontendImpl::SendMessageToEmbedder(const String& message) {
  if (client_)
    client_->SendMessageToEmbedder(message);
}

bool WebDevToolsFrontendImpl::IsUnderTest() {
  return client_ ? client_->IsUnderTest() : false;
}

void WebDevToolsFrontendImpl::ShowContextMenu(
    LocalFrame* target_frame,
    float x,
    float y,
    ContextMenuProvider* menu_provider) {
  WebLocalFrameImpl::FromFrame(target_frame)
      ->ViewImpl()
      ->ShowContextMenuAtPoint(x, y, menu_provider);
}

}  // namespace blink
