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
#include "platform/wtf/text/AtomicString.h"

namespace blink {

DOMPlugin::DOMPlugin(LocalFrame* frame, const PluginInfo& plugin_info)
    : ContextClient(frame), plugin_info_(&plugin_info) {}

DEFINE_TRACE(DOMPlugin) {
  ContextClient::Trace(visitor);
  visitor->Trace(plugin_info_);
}

String DOMPlugin::name() const {
  return plugin_info_->Name();
}

String DOMPlugin::filename() const {
  return plugin_info_->Filename();
}

String DOMPlugin::description() const {
  return plugin_info_->Description();
}

unsigned DOMPlugin::length() const {
  return plugin_info_->GetMimeClassInfoSize();
}

DOMMimeType* DOMPlugin::item(unsigned index) {
  const MimeClassInfo* mime = plugin_info_->GetMimeClassInfo(index);

  if (!mime)
    return nullptr;

  return DOMMimeType::Create(GetFrame(), *mime);
}

DOMMimeType* DOMPlugin::namedItem(const AtomicString& property_name) {
  const MimeClassInfo* mime = plugin_info_->GetMimeClassInfo(property_name);

  if (!mime)
    return nullptr;

  return DOMMimeType::Create(GetFrame(), *mime);
}

}  // namespace blink
