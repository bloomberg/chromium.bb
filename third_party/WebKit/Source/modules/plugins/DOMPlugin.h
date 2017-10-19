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

#ifndef DOMPlugin_h
#define DOMPlugin_h

#include "core/dom/ContextLifecycleObserver.h"
#include "modules/plugins/DOMMimeType.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class ExceptionState;

class DOMPlugin final : public GarbageCollected<DOMPlugin>,
                        public ScriptWrappable,
                        public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(DOMPlugin);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMPlugin* Create(LocalFrame* frame, const PluginInfo& plugin_info) {
    return new DOMPlugin(frame, plugin_info);
  }

  String name() const;
  String filename() const;
  String description() const;

  unsigned length() const;

  DOMMimeType* item(unsigned index);
  DOMMimeType* namedItem(const AtomicString& property_name);
  void NamedPropertyEnumerator(Vector<String>&, ExceptionState&) const;
  bool NamedPropertyQuery(const AtomicString&, ExceptionState&) const;

  virtual void Trace(blink::Visitor*);

 private:
  DOMPlugin(LocalFrame*, const PluginInfo&);

  Member<const PluginInfo> plugin_info_;
};

}  // namespace blink

#endif  // DOMPlugin_h
