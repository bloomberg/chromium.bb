/*
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

#ifndef PluginData_h
#define PluginData_h

#include "platform/PlatformExport.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

struct PluginInfo;

struct MimeClassInfo {
  String type;
  String desc;
  Vector<String> extensions;
};

inline bool operator==(const MimeClassInfo& a, const MimeClassInfo& b) {
  return a.type == b.type && a.desc == b.desc && a.extensions == b.extensions;
}

struct PluginInfo {
  String name;
  String file;
  String desc;
  Vector<MimeClassInfo> mimes;
};

class PLATFORM_EXPORT PluginData : public RefCounted<PluginData> {
  WTF_MAKE_NONCOPYABLE(PluginData);

 public:
  static PassRefPtr<PluginData> Create(SecurityOrigin* main_frame_origin) {
    return AdoptRef(new PluginData(main_frame_origin));
  }

  const Vector<PluginInfo>& Plugins() const { return plugins_; }
  const Vector<MimeClassInfo>& Mimes() const { return mimes_; }
  const Vector<size_t>& MimePluginIndices() const {
    return mime_plugin_indices_;
  }
  const SecurityOrigin* Origin() const { return main_frame_origin_.Get(); }

  bool SupportsMimeType(const String& mime_type) const;
  String PluginNameForMimeType(const String& mime_type) const;

  // refreshBrowserSidePluginCache doesn't update existent instances of
  // PluginData.
  static void RefreshBrowserSidePluginCache();

 private:
  explicit PluginData(SecurityOrigin* main_frame_origin);
  const PluginInfo* PluginInfoForMimeType(const String& mime_type) const;

  Vector<PluginInfo> plugins_;
  Vector<MimeClassInfo> mimes_;
  Vector<size_t> mime_plugin_indices_;
  RefPtr<SecurityOrigin> main_frame_origin_;
};

}  // namespace blink

#endif
