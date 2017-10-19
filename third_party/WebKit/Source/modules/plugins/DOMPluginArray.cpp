/*
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA
 */

#include "modules/plugins/DOMPluginArray.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/page/Page.h"
#include "modules/plugins/DOMMimeTypeArray.h"
#include "modules/plugins/NavigatorPlugins.h"
#include "platform/plugins/PluginData.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/debug/Alias.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

DOMPluginArray::DOMPluginArray(LocalFrame* frame)
    : ContextLifecycleObserver(frame ? frame->GetDocument() : nullptr),
      PluginsChangedObserver(frame ? frame->GetPage() : nullptr) {
  UpdatePluginData();
}

void DOMPluginArray::Trace(blink::Visitor* visitor) {
  ContextLifecycleObserver::Trace(visitor);
  PluginsChangedObserver::Trace(visitor);
  visitor->Trace(dom_plugins_);
}

unsigned DOMPluginArray::length() const {
  return dom_plugins_.size();
}

DOMPlugin* DOMPluginArray::item(unsigned index) {
  if (index >= dom_plugins_.size())
    return nullptr;

  if (!dom_plugins_[index]) {
    dom_plugins_[index] =
        DOMPlugin::Create(GetFrame(), *GetPluginData()->Plugins()[index]);
  }

  return dom_plugins_[index];
}

DOMPlugin* DOMPluginArray::namedItem(const AtomicString& property_name) {
  PluginData* data = GetPluginData();
  if (!data)
    return nullptr;

  for (const Member<PluginInfo>& plugin_info : data->Plugins()) {
    if (plugin_info->Name() == property_name) {
      size_t index = &plugin_info - &data->Plugins()[0];
      return item(index);
    }
  }
  return nullptr;
}

void DOMPluginArray::NamedPropertyEnumerator(Vector<String>& property_names,
                                             ExceptionState&) const {
  PluginData* data = GetPluginData();
  if (!data)
    return;
  property_names.ReserveInitialCapacity(data->Plugins().size());
  for (const PluginInfo* plugin_info : data->Plugins()) {
    property_names.UncheckedAppend(plugin_info->Name());
  }
}

bool DOMPluginArray::NamedPropertyQuery(const AtomicString& property_name,
                                        ExceptionState& exception_state) const {
  Vector<String> properties;
  NamedPropertyEnumerator(properties, exception_state);
  return properties.Contains(property_name);
}

void DOMPluginArray::refresh(bool reload) {
  if (!GetFrame())
    return;

  Page::RefreshPlugins();
  if (PluginData* data = GetPluginData())
    data->ResetPluginData();

  for (Frame* frame = GetFrame()->GetPage()->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (!frame->IsLocalFrame())
      continue;
    Navigator& navigator = *ToLocalFrame(frame)->DomWindow()->navigator();
    NavigatorPlugins::plugins(navigator)->UpdatePluginData();
    NavigatorPlugins::mimeTypes(navigator)->UpdatePluginData();
  }

  if (reload) {
    GetFrame()->Reload(kFrameLoadTypeReload,
                       ClientRedirectPolicy::kClientRedirect);
  }
}

PluginData* DOMPluginArray::GetPluginData() const {
  if (!GetFrame())
    return nullptr;
  return GetFrame()->GetPluginData();
}

void DOMPluginArray::UpdatePluginData() {
  PluginData* data = GetPluginData();
  if (!data) {
    dom_plugins_.clear();
    return;
  }

  HeapVector<Member<DOMPlugin>> old_dom_plugins(std::move(dom_plugins_));
  dom_plugins_.clear();
  dom_plugins_.resize(data->Plugins().size());

  for (Member<DOMPlugin>& plugin : old_dom_plugins) {
    if (plugin) {
      for (const Member<PluginInfo>& plugin_info : data->Plugins()) {
        if (plugin->name() == plugin_info->Name()) {
          size_t index = &plugin_info - &data->Plugins()[0];
          dom_plugins_[index] = plugin;
        }
      }
    }
  }
}

void DOMPluginArray::ContextDestroyed(ExecutionContext*) {
  dom_plugins_.clear();
}

void DOMPluginArray::PluginsChanged() {
  UpdatePluginData();
}

}  // namespace blink
