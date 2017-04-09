/*
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "modules/plugins/DOMPlugin.h"

#include "platform/plugins/PluginData.h"
#include "wtf/text/AtomicString.h"

namespace blink {

DOMPlugin::DOMPlugin(PluginData* plugin_data, LocalFrame* frame, unsigned index)
    : ContextClient(frame), plugin_data_(plugin_data), index_(index) {}

DOMPlugin::~DOMPlugin() {}

DEFINE_TRACE(DOMPlugin) {
  ContextClient::Trace(visitor);
}

String DOMPlugin::name() const {
  return GetPluginInfo().name;
}

String DOMPlugin::filename() const {
  return GetPluginInfo().file;
}

String DOMPlugin::description() const {
  return GetPluginInfo().desc;
}

unsigned DOMPlugin::length() const {
  return GetPluginInfo().mimes.size();
}

DOMMimeType* DOMPlugin::item(unsigned index) {
  if (index >= GetPluginInfo().mimes.size())
    return nullptr;

  const MimeClassInfo& mime = GetPluginInfo().mimes[index];

  const Vector<MimeClassInfo>& mimes = plugin_data_->Mimes();
  for (unsigned i = 0; i < mimes.size(); ++i) {
    if (mimes[i] == mime && plugin_data_->MimePluginIndices()[i] == index_)
      return DOMMimeType::Create(plugin_data_.Get(), GetFrame(), i);
  }
  return nullptr;
}

DOMMimeType* DOMPlugin::namedItem(const AtomicString& property_name) {
  const Vector<MimeClassInfo>& mimes = plugin_data_->Mimes();
  for (unsigned i = 0; i < mimes.size(); ++i) {
    if (mimes[i].type == property_name)
      return DOMMimeType::Create(plugin_data_.Get(), GetFrame(), i);
  }
  return nullptr;
}

}  // namespace blink
