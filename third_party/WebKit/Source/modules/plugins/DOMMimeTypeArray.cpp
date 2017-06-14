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

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/page/Page.h"
#include "platform/plugins/PluginData.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

DOMMimeTypeArray::DOMMimeTypeArray(LocalFrame* frame)
    : ContextLifecycleObserver(frame ? frame->GetDocument() : nullptr) {
  UpdatePluginData();
}

DEFINE_TRACE(DOMMimeTypeArray) {
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(dom_mime_types_);
}

unsigned DOMMimeTypeArray::length() const {
  return dom_mime_types_.size();
}

DOMMimeType* DOMMimeTypeArray::item(unsigned index) {
  if (index >= dom_mime_types_.size())
    return nullptr;
  if (!dom_mime_types_[index]) {
    dom_mime_types_[index] =
        DOMMimeType::Create(GetFrame(), *GetPluginData()->Mimes()[index]);
  }

  return dom_mime_types_[index];
}

DOMMimeType* DOMMimeTypeArray::namedItem(const AtomicString& property_name) {
  PluginData* data = GetPluginData();
  if (!data)
    return nullptr;

  for (const Member<MimeClassInfo>& mime : data->Mimes()) {
    if (mime->Type() == property_name) {
      size_t index = &mime - &data->Mimes()[0];
      return item(index);
    }
  }
  return nullptr;
}

PluginData* DOMMimeTypeArray::GetPluginData() const {
  if (!GetFrame())
    return nullptr;
  return GetFrame()->GetPluginData();
}

void DOMMimeTypeArray::UpdatePluginData() {
  PluginData* data = GetPluginData();
  if (!data) {
    dom_mime_types_.clear();
    return;
  }

  HeapVector<Member<DOMMimeType>> old_dom_mime_types(
      std::move(dom_mime_types_));
  dom_mime_types_.clear();
  dom_mime_types_.resize(data->Mimes().size());

  for (Member<DOMMimeType>& mime : old_dom_mime_types) {
    if (mime) {
      for (const Member<MimeClassInfo>& mime_info : data->Mimes()) {
        if (mime->type() == mime_info->Type()) {
          size_t index = &mime_info - &data->Mimes()[0];
          dom_mime_types_[index] = mime;
        }
      }
    }
  }
}

void DOMMimeTypeArray::ContextDestroyed(ExecutionContext*) {
  dom_mime_types_.clear();
}

}  // namespace blink
