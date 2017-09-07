/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
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

#include "bindings/core/v8/V8DevToolsHost.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8HTMLDocument.h"
#include "bindings/core/v8/V8MouseEvent.h"
#include "bindings/core/v8/V8Window.h"
#include "build/build_config.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/inspector/DevToolsHost.h"
#include "core/inspector/InspectorFrontendClient.h"
#include "platform/ContextMenu.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/Platform.h"

namespace blink {

void V8DevToolsHost::platformMethodCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
#if defined(OS_MACOSX)
  V8SetReturnValue(info, V8AtomicString(info.GetIsolate(), "mac"));
#elif defined(OS_WIN)
  V8SetReturnValue(info, V8AtomicString(info.GetIsolate(), "windows"));
#else  // Unix-like systems
  V8SetReturnValue(info, V8AtomicString(info.GetIsolate(), "linux"));
#endif
}

static bool PopulateContextMenuItems(v8::Isolate* isolate,
                                     const v8::Local<v8::Array>& item_array,
                                     ContextMenu& menu) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  for (size_t i = 0; i < item_array->Length(); ++i) {
    v8::Local<v8::Object> item =
        item_array->Get(context, i).ToLocalChecked().As<v8::Object>();
    v8::Local<v8::Value> type;
    v8::Local<v8::Value> id;
    v8::Local<v8::Value> label;
    v8::Local<v8::Value> enabled;
    v8::Local<v8::Value> checked;
    v8::Local<v8::Value> sub_items;
    if (!item->Get(context, V8AtomicString(isolate, "type")).ToLocal(&type) ||
        !item->Get(context, V8AtomicString(isolate, "id")).ToLocal(&id) ||
        !item->Get(context, V8AtomicString(isolate, "label")).ToLocal(&label) ||
        !item->Get(context, V8AtomicString(isolate, "enabled"))
             .ToLocal(&enabled) ||
        !item->Get(context, V8AtomicString(isolate, "checked"))
             .ToLocal(&checked) ||
        !item->Get(context, V8AtomicString(isolate, "subItems"))
             .ToLocal(&sub_items))
      return false;
    if (!type->IsString())
      continue;
    String type_string = ToCoreStringWithNullCheck(type.As<v8::String>());
    if (type_string == "separator") {
      ContextMenuItem item(ContextMenuItem(
          kSeparatorType, kContextMenuItemCustomTagNoAction, String()));
      menu.AppendItem(item);
    } else if (type_string == "subMenu" && sub_items->IsArray()) {
      ContextMenu sub_menu;
      v8::Local<v8::Array> sub_items_array =
          v8::Local<v8::Array>::Cast(sub_items);
      if (!PopulateContextMenuItems(isolate, sub_items_array, sub_menu))
        return false;
      TOSTRING_DEFAULT(V8StringResource<kTreatNullAsNullString>, label_string,
                       label, false);
      ContextMenuItem item(kSubmenuType, kContextMenuItemCustomTagNoAction,
                           label_string, &sub_menu);
      menu.AppendItem(item);
    } else {
      int32_t int32_id;
      if (!id->Int32Value(context).To(&int32_id))
        return false;
      ContextMenuAction typed_id = static_cast<ContextMenuAction>(
          kContextMenuItemBaseCustomTag + int32_id);
      TOSTRING_DEFAULT(V8StringResource<kTreatNullAsNullString>, label_string,
                       label, false);
      ContextMenuItem menu_item(
          (type_string == "checkbox" ? kCheckableActionType : kActionType),
          typed_id, label_string);
      if (checked->IsBoolean())
        menu_item.SetChecked(checked.As<v8::Boolean>()->Value());
      if (enabled->IsBoolean())
        menu_item.SetEnabled(enabled.As<v8::Boolean>()->Value());
      menu.AppendItem(menu_item);
    }
  }
  return true;
}

void V8DevToolsHost::showContextMenuAtPointMethodCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 3)
    return;

  ExceptionState exception_state(info.GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 "DevToolsHost", "showContextMenuAtPoint");
  v8::Isolate* isolate = info.GetIsolate();

  float x = ToRestrictedFloat(isolate, info[0], exception_state);
  if (exception_state.HadException())
    return;
  float y = ToRestrictedFloat(isolate, info[1], exception_state);
  if (exception_state.HadException())
    return;

  v8::Local<v8::Value> array = v8::Local<v8::Value>::Cast(info[2]);
  if (!array->IsArray())
    return;
  ContextMenu menu;
  if (!PopulateContextMenuItems(isolate, v8::Local<v8::Array>::Cast(array),
                                menu))
    return;

  Document* document = nullptr;
  if (info.Length() >= 4 && v8::Local<v8::Value>::Cast(info[3])->IsObject()) {
    v8::Local<v8::Object> document_wrapper =
        v8::Local<v8::Object>::Cast(info[3]);
    if (!V8HTMLDocument::wrapperTypeInfo.Equals(
            ToWrapperTypeInfo(document_wrapper)))
      return;
    document = V8HTMLDocument::ToImpl(document_wrapper);
  } else {
    v8::Local<v8::Object> window_wrapper =
        V8Window::findInstanceInPrototypeChain(
            isolate->GetEnteredContext()->Global(), isolate);
    if (window_wrapper.IsEmpty())
      return;
    DOMWindow* window = V8Window::ToImpl(window_wrapper);
    document = window ? ToLocalDOMWindow(window)->document() : nullptr;
  }
  if (!document || !document->GetFrame())
    return;

  DevToolsHost* devtools_host = V8DevToolsHost::ToImpl(info.Holder());
  Vector<ContextMenuItem> items = menu.Items();
  devtools_host->ShowContextMenu(document->GetFrame(), x, y, items);
}

}  // namespace blink
