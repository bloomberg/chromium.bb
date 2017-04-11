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

#include "modules/plugins/DOMMimeType.h"

#include "core/frame/LocalFrame.h"
#include "core/loader/FrameLoader.h"
#include "core/page/Page.h"
#include "modules/plugins/DOMPlugin.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

DOMMimeType::DOMMimeType(PassRefPtr<PluginData> plugin_data,
                         LocalFrame* frame,
                         unsigned index)
    : ContextClient(frame),
      plugin_data_(std::move(plugin_data)),
      index_(index) {}

DOMMimeType::~DOMMimeType() {}

DEFINE_TRACE(DOMMimeType) {
  ContextClient::Trace(visitor);
}

const String& DOMMimeType::type() const {
  return GetMimeClassInfo().type;
}

String DOMMimeType::suffixes() const {
  const Vector<String>& extensions = GetMimeClassInfo().extensions;

  StringBuilder builder;
  for (size_t i = 0; i < extensions.size(); ++i) {
    if (i)
      builder.Append(',');
    builder.Append(extensions[i]);
  }
  return builder.ToString();
}

const String& DOMMimeType::description() const {
  return GetMimeClassInfo().desc;
}

DOMPlugin* DOMMimeType::enabledPlugin() const {
  // FIXME: allowPlugins is just a client call. We should not need
  // to bounce through the loader to get there.
  // Something like: frame()->page()->client()->allowPlugins().
  if (!GetFrame() ||
      !GetFrame()->Loader().AllowPlugins(kNotAboutToInstantiatePlugin))
    return nullptr;

  return DOMPlugin::Create(plugin_data_.Get(), GetFrame(),
                           plugin_data_->MimePluginIndices()[index_]);
}

}  // namespace blink
