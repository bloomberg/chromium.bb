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

#include "core/frame/LocalFrame.h"
#include "core/page/Page.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/plugins/PluginData.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

DOMPluginArray::DOMPluginArray(LocalFrame* frame) : ContextClient(frame) {}

DEFINE_TRACE(DOMPluginArray) {
  ContextClient::Trace(visitor);
}

unsigned DOMPluginArray::length() const {
  PluginData* data = GetPluginData();
  if (!data)
    return 0;
  return data->Plugins().size();
}

DOMPlugin* DOMPluginArray::item(unsigned index) {
  PluginData* data = GetPluginData();
  if (!data)
    return nullptr;
  const Vector<PluginInfo>& plugins = data->Plugins();
  if (index >= plugins.size())
    return nullptr;
  return DOMPlugin::Create(data, GetFrame(), index);
}

DOMPlugin* DOMPluginArray::namedItem(const AtomicString& property_name) {
  PluginData* data = GetPluginData();
  if (!data)
    return nullptr;
  const Vector<PluginInfo>& plugins = data->Plugins();
  for (unsigned i = 0; i < plugins.size(); ++i) {
    if (plugins[i].name == property_name)
      return DOMPlugin::Create(data, GetFrame(), i);
  }
  return nullptr;
}

void DOMPluginArray::refresh(bool reload) {
  if (!GetFrame())
    return;
  Page::RefreshPlugins();
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

}  // namespace blink
