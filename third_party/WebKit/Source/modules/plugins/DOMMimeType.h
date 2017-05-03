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

#ifndef DOMMimeType_h
#define DOMMimeType_h

#include "core/dom/ContextLifecycleObserver.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/plugins/PluginData.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class DOMPlugin;
class LocalFrame;

class DOMMimeType final : public GarbageCollectedFinalized<DOMMimeType>,
                          public ScriptWrappable,
                          public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(DOMMimeType);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMMimeType* Create(PassRefPtr<PluginData> plugin_data,
                             LocalFrame* frame,
                             unsigned index) {
    return new DOMMimeType(std::move(plugin_data), frame, index);
  }
  virtual ~DOMMimeType();

  const String& type() const;
  String suffixes() const;
  const String& description() const;
  DOMPlugin* enabledPlugin() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  DOMMimeType(PassRefPtr<PluginData>, LocalFrame*, unsigned index);

  const MimeClassInfo& GetMimeClassInfo() const {
    return plugin_data_->Mimes()[index_];
  }

  RefPtr<PluginData> plugin_data_;
  unsigned index_;
};

}  // namespace blink

#endif  // DOMMimeType_h
