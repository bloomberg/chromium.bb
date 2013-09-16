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

#include "config.h"
#include "V8InspectorFrontendHost.h"

#include "V8MouseEvent.h"
#include "bindings/v8/V8Binding.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorFrontendClient.h"
#include "core/inspector/InspectorFrontendHost.h"
#include "core/platform/HistogramSupport.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

void V8InspectorFrontendHost::platformMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
#if OS(MACOSX)
    v8SetReturnValue(args, v8::String::NewSymbol("mac"));
#elif OS(LINUX)
    v8SetReturnValue(args, v8::String::NewSymbol("linux"));
#elif OS(FREEBSD)
    v8SetReturnValue(args, v8::String::NewSymbol("freebsd"));
#elif OS(OPENBSD)
    v8SetReturnValue(args, v8::String::NewSymbol("openbsd"));
#elif OS(WIN)
    v8SetReturnValue(args, v8::String::NewSymbol("windows"));
#else
    v8SetReturnValue(args, v8::String::NewSymbol("unknown"));
#endif
}

void V8InspectorFrontendHost::portMethodCustom(const v8::FunctionCallbackInfo<v8::Value>&)
{
}

static void populateContextMenuItems(v8::Local<v8::Array>& itemArray, ContextMenu& menu)
{
    for (size_t i = 0; i < itemArray->Length(); ++i) {
        v8::Local<v8::Object> item = v8::Local<v8::Object>::Cast(itemArray->Get(i));
        v8::Local<v8::Value> type = item->Get(v8::String::NewSymbol("type"));
        v8::Local<v8::Value> id = item->Get(v8::String::NewSymbol("id"));
        v8::Local<v8::Value> label = item->Get(v8::String::NewSymbol("label"));
        v8::Local<v8::Value> enabled = item->Get(v8::String::NewSymbol("enabled"));
        v8::Local<v8::Value> checked = item->Get(v8::String::NewSymbol("checked"));
        v8::Local<v8::Value> subItems = item->Get(v8::String::NewSymbol("subItems"));
        if (!type->IsString())
            continue;
        String typeString = toWebCoreStringWithNullCheck(type.As<v8::String>());
        if (typeString == "separator") {
            ContextMenuItem item(ContextMenuItem(SeparatorType,
                                 ContextMenuItemCustomTagNoAction,
                                 String()));
            menu.appendItem(item);
        } else if (typeString == "subMenu" && subItems->IsArray()) {
            ContextMenu subMenu;
            v8::Local<v8::Array> subItemsArray = v8::Local<v8::Array>::Cast(subItems);
            populateContextMenuItems(subItemsArray, subMenu);
            ContextMenuItem item(SubmenuType,
                                 ContextMenuItemCustomTagNoAction,
                                 toWebCoreStringWithNullCheck(label),
                                 &subMenu);
            menu.appendItem(item);
        } else {
            ContextMenuAction typedId = static_cast<ContextMenuAction>(ContextMenuItemBaseCustomTag + id->ToInt32()->Value());
            ContextMenuItem menuItem((typeString == "checkbox" ? CheckableActionType : ActionType), typedId, toWebCoreStringWithNullCheck(label));
            if (checked->IsBoolean())
                menuItem.setChecked(checked->ToBoolean()->Value());
            if (enabled->IsBoolean())
                menuItem.setEnabled(enabled->ToBoolean()->Value());
            menu.appendItem(menuItem);
        }
    }
}

void V8InspectorFrontendHost::showContextMenuMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() < 2)
        return;

    v8::Local<v8::Object> eventWrapper = v8::Local<v8::Object>::Cast(args[0]);
    if (!V8MouseEvent::info.equals(toWrapperTypeInfo(eventWrapper)))
        return;

    Event* event = V8Event::toNative(eventWrapper);
    if (!args[1]->IsArray())
        return;

    v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(args[1]);
    ContextMenu menu;
    populateContextMenuItems(array, menu);

    InspectorFrontendHost* frontendHost = V8InspectorFrontendHost::toNative(args.Holder());
    Vector<ContextMenuItem> items = menu.items();
    frontendHost->showContextMenu(event, items);
}

static void histogramEnumeration(const char* name, const v8::FunctionCallbackInfo<v8::Value>& args, int boundaryValue)
{
    if (args.Length() < 1 || !args[0]->IsInt32())
        return;

    int sample = args[0]->ToInt32()->Value();
    if (sample < boundaryValue)
        HistogramSupport::histogramEnumeration(name, sample, boundaryValue);
}

void V8InspectorFrontendHost::recordActionTakenMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    histogramEnumeration("DevTools.ActionTaken", args, 100);
}

void V8InspectorFrontendHost::recordPanelShownMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    histogramEnumeration("DevTools.PanelShown", args, 20);
}

void V8InspectorFrontendHost::recordSettingChangedMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    histogramEnumeration("DevTools.SettingChanged", args, 100);
}

} // namespace WebCore

