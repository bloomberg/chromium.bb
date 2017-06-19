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
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/plugins/PluginData.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/debug/Alias.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

DOMPluginArray::DOMPluginArray(LocalFrame* frame)
    : ContextLifecycleObserver(frame ? frame->GetDocument() : nullptr) {
  UpdatePluginData();
}

DEFINE_TRACE(DOMPluginArray) {
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(dom_plugins_);
}

unsigned DOMPluginArray::length() const {
  return dom_plugins_.size();
}

DOMPlugin* DOMPluginArray::item(unsigned index) {
  if (index >= dom_plugins_.size())
    return nullptr;

  // TODO(lfg): Temporary to track down https://crbug.com/731239.
  if (!GetPluginData()) {
    LocalFrame* frame = GetFrame();
    LocalFrameClient* client = GetFrame()->Client();
    Settings* settings = GetFrame()->GetSettings();
    ExecutionContext* context = GetExecutionContext();
    Page* page = GetFrame()->GetPage();
    bool allow_plugins =
        frame->Loader().AllowPlugins(kNotAboutToInstantiatePlugin);

    CHECK(false);

    WTF::debug::Alias(frame);
    WTF::debug::Alias(client);
    WTF::debug::Alias(settings);
    WTF::debug::Alias(context);
    WTF::debug::Alias(page);
    WTF::debug::Alias(&allow_plugins);
  }
  CHECK(main_frame_origin_->IsSameSchemeHostPort(GetPluginData()->Origin()));

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

  main_frame_origin_ = data->Origin();

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

}  // namespace blink
