/*
    Copyright (C) 2000 Harri Porten (porten@kde.org)
    Copyright (C) 2000 Daniel Molkentin (molkentin@kde.org)
    Copyright (C) 2000 Stefan Schimanski (schimmi@kde.org)
    Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All Rights Reserved.
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "platform/plugins/PluginData.h"

#include "platform/plugins/PluginListBuilder.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSecurityOrigin.h"

namespace blink {

PluginData::PluginData(SecurityOrigin* main_frame_origin)
    : main_frame_origin_(main_frame_origin) {
  PluginListBuilder builder(&plugins_);
  Platform::Current()->GetPluginList(
      false, WebSecurityOrigin(main_frame_origin_), &builder);

  for (unsigned i = 0; i < plugins_.size(); ++i) {
    const PluginInfo& plugin = plugins_[i];
    for (unsigned j = 0; j < plugin.mimes.size(); ++j) {
      mimes_.push_back(plugin.mimes[j]);
      mime_plugin_indices_.push_back(i);
    }
  }
}

bool PluginData::SupportsMimeType(const String& mime_type) const {
  for (unsigned i = 0; i < mimes_.size(); ++i)
    if (mimes_[i].type == mime_type)
      return true;
  return false;
}

const PluginInfo* PluginData::PluginInfoForMimeType(
    const String& mime_type) const {
  for (unsigned i = 0; i < mimes_.size(); ++i) {
    const MimeClassInfo& info = mimes_[i];

    if (info.type == mime_type)
      return &plugins_[mime_plugin_indices_[i]];
  }

  return 0;
}

String PluginData::PluginNameForMimeType(const String& mime_type) const {
  if (const PluginInfo* info = PluginInfoForMimeType(mime_type))
    return info->name;
  return String();
}

void PluginData::RefreshBrowserSidePluginCache() {
  Vector<PluginInfo> plugins;
  PluginListBuilder builder(&plugins);
  Platform::Current()->GetPluginList(true, WebSecurityOrigin::CreateUnique(),
                                     &builder);
}

}  // namespace blink
