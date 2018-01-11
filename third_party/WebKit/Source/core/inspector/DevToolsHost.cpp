/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/inspector/DevToolsHost.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/clipboard/Pasteboard.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/dom/events/Event.h"
#include "core/dom/events/EventTarget.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/inspector/InspectorFrontendClient.h"
#include "core/layout/LayoutTheme.h"
#include "core/loader/FrameLoader.h"
#include "core/page/ContextMenuController.h"
#include "core/page/ContextMenuProvider.h"
#include "core/page/Page.h"
#include "platform/ContextMenu.h"
#include "platform/ContextMenuItem.h"
#include "platform/PlatformChromeClient.h"
#include "platform/SharedBuffer.h"
#include "platform/bindings/ScriptForbiddenScope.h"
#include "platform/bindings/ScriptState.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"

namespace blink {

class FrontendMenuProvider final : public ContextMenuProvider {
 public:
  static FrontendMenuProvider* Create(DevToolsHost* devtools_host,
                                      const Vector<ContextMenuItem>& items) {
    return new FrontendMenuProvider(devtools_host, items);
  }

  ~FrontendMenuProvider() override {
    // Verify that this menu provider has been detached.
    DCHECK(!devtools_host_);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(devtools_host_);
    ContextMenuProvider::Trace(visitor);
  }

  void Disconnect() { devtools_host_ = nullptr; }

  void ContextMenuCleared() override {
    if (devtools_host_) {
      devtools_host_->EvaluateScript("DevToolsAPI.contextMenuCleared()");
      devtools_host_->ClearMenuProvider();
      devtools_host_ = nullptr;
    }
    items_.clear();
  }

  void PopulateContextMenu(ContextMenu* menu) override {
    for (size_t i = 0; i < items_.size(); ++i)
      menu->AppendItem(items_[i]);
  }

  void ContextMenuItemSelected(const ContextMenuItem* item) override {
    if (!devtools_host_)
      return;
    int item_number = item->Action() - kContextMenuItemBaseCustomTag;
    devtools_host_->EvaluateScript("DevToolsAPI.contextMenuItemSelected(" +
                                   String::Number(item_number) + ")");
  }

 private:
  FrontendMenuProvider(DevToolsHost* devtools_host,
                       const Vector<ContextMenuItem>& items)
      : devtools_host_(devtools_host), items_(items) {}

  Member<DevToolsHost> devtools_host_;
  Vector<ContextMenuItem> items_;
};

DevToolsHost::DevToolsHost(InspectorFrontendClient* client,
                           LocalFrame* frontend_frame)
    : client_(client),
      frontend_frame_(frontend_frame),
      menu_provider_(nullptr) {}

DevToolsHost::~DevToolsHost() = default;

void DevToolsHost::Trace(blink::Visitor* visitor) {
  visitor->Trace(client_);
  visitor->Trace(frontend_frame_);
  visitor->Trace(menu_provider_);
  ScriptWrappable::Trace(visitor);
}

void DevToolsHost::EvaluateScript(const String& expression) {
  if (ScriptForbiddenScope::IsScriptForbidden())
    return;
  if (!frontend_frame_)
    return;
  ScriptState* script_state = ToScriptStateForMainWorld(frontend_frame_);
  if (!script_state)
    return;
  ScriptState::Scope scope(script_state);
  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      Frame::NotifyUserActivation(frontend_frame_);
  v8::MicrotasksScope microtasks(script_state->GetIsolate(),
                                 v8::MicrotasksScope::kRunMicrotasks);
  ScriptSourceCode source_code(expression, ScriptSourceLocationType::kInternal,
                               nullptr, KURL(), TextPosition());
  V8ScriptRunner::CompileAndRunInternalScript(script_state->GetIsolate(),
                                              script_state, source_code);
}

void DevToolsHost::DisconnectClient() {
  client_ = nullptr;
  if (menu_provider_) {
    menu_provider_->Disconnect();
    menu_provider_ = nullptr;
  }
  frontend_frame_ = nullptr;
}

float DevToolsHost::zoomFactor() {
  if (!frontend_frame_)
    return 1;
  float zoom_factor = frontend_frame_->PageZoomFactor();
  // Cancel the device scale factor applied to the zoom factor in
  // use-zoom-for-dsf mode.
  const PlatformChromeClient* client =
      frontend_frame_->View()->GetChromeClient();
  float window_to_viewport_ratio = client->WindowToViewportScalar(1.0f);
  return zoom_factor / window_to_viewport_ratio;
}

void DevToolsHost::copyText(const String& text) {
  Pasteboard::GeneralPasteboard()->WritePlainText(
      text, Pasteboard::kCannotSmartReplace);
}

static String EscapeUnicodeNonCharacters(const String& str) {
  const UChar kNonChar = 0xD800;

  unsigned i = 0;
  while (i < str.length() && str[i] < kNonChar)
    ++i;
  if (i == str.length())
    return str;

  StringBuilder dst;
  dst.Append(str, 0, i);
  for (; i < str.length(); ++i) {
    UChar c = str[i];
    if (c >= kNonChar) {
      unsigned symbol = static_cast<unsigned>(c);
      String symbol_code = String::Format("\\u%04X", symbol);
      dst.Append(symbol_code);
    } else {
      dst.Append(c);
    }
  }
  return dst.ToString();
}

void DevToolsHost::sendMessageToEmbedder(const String& message) {
  if (client_)
    client_->SendMessageToEmbedder(EscapeUnicodeNonCharacters(message));
}

void DevToolsHost::ShowContextMenu(LocalFrame* target_frame,
                                   float x,
                                   float y,
                                   const Vector<ContextMenuItem>& items) {
  DCHECK(frontend_frame_);
  FrontendMenuProvider* menu_provider =
      FrontendMenuProvider::Create(this, items);
  menu_provider_ = menu_provider;
  float zoom = target_frame->PageZoomFactor();
  if (client_)
    client_->ShowContextMenu(target_frame, x * zoom, y * zoom, menu_provider);
}

String DevToolsHost::getSelectionBackgroundColor() {
  return LayoutTheme::GetTheme().ActiveSelectionBackgroundColor().Serialized();
}

String DevToolsHost::getSelectionForegroundColor() {
  return LayoutTheme::GetTheme().ActiveSelectionForegroundColor().Serialized();
}

String DevToolsHost::getInactiveSelectionBackgroundColor() {
  return LayoutTheme::GetTheme()
      .InactiveSelectionBackgroundColor()
      .Serialized();
}

String DevToolsHost::getInactiveSelectionForegroundColor() {
  return LayoutTheme::GetTheme()
      .InactiveSelectionForegroundColor()
      .Serialized();
}


bool DevToolsHost::isHostedMode() {
  return false;
}

}  // namespace blink
