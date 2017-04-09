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

#include "modules/plugins/DOMMimeTypeArray.h"

#include "core/frame/LocalFrame.h"
#include "core/page/Page.h"
#include "platform/plugins/PluginData.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

namespace blink {

DOMMimeTypeArray::DOMMimeTypeArray(LocalFrame* frame) : ContextClient(frame) {}

DEFINE_TRACE(DOMMimeTypeArray) {
  ContextClient::Trace(visitor);
}

unsigned DOMMimeTypeArray::length() const {
  PluginData* data = GetPluginData();
  if (!data)
    return 0;
  return data->Mimes().size();
}

DOMMimeType* DOMMimeTypeArray::item(unsigned index) {
  PluginData* data = GetPluginData();
  if (!data)
    return nullptr;
  const Vector<MimeClassInfo>& mimes = data->Mimes();
  if (index >= mimes.size())
    return nullptr;
  return DOMMimeType::Create(data, GetFrame(), index);
}

DOMMimeType* DOMMimeTypeArray::namedItem(const AtomicString& property_name) {
  PluginData* data = GetPluginData();
  if (!data)
    return nullptr;
  const Vector<MimeClassInfo>& mimes = data->Mimes();
  for (unsigned i = 0; i < mimes.size(); ++i) {
    if (mimes[i].type == property_name)
      return DOMMimeType::Create(data, GetFrame(), i);
  }
  return nullptr;
}

PluginData* DOMMimeTypeArray::GetPluginData() const {
  if (!GetFrame())
    return nullptr;
  return GetFrame()->GetPluginData();
}

}  // namespace blink
