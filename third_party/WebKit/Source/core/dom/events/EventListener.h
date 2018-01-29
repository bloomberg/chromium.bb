/*
 * Copyright (C) 2006, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef EventListener_h
#define EventListener_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Event;
class ExecutionContext;

class CORE_EXPORT EventListener
    : public GarbageCollectedFinalized<EventListener>,
      public TraceWrapperBase {
 public:
  enum ListenerType {
    kJSEventListenerType,
    kImageEventListenerType,
    kCPPEventListenerType,
    kConditionEventListenerType,
    kNativeEventListenerType,
  };

  virtual ~EventListener() = default;
  virtual bool operator==(const EventListener&) const = 0;
  virtual void handleEvent(ExecutionContext*, Event*) = 0;
  virtual const String& Code() const { return g_empty_string; }
  virtual bool WasCreatedFromMarkup() const { return false; }
  virtual bool BelongsToTheCurrentWorld(ExecutionContext*) const {
    return false;
  }
  virtual bool IsAttribute() const { return false; }

  ListenerType GetType() const { return type_; }

  virtual void Trace(blink::Visitor* visitor) {}
  void TraceWrappers(const ScriptWrappableVisitor* visitor) const override {}
  const char* NameInHeapSnapshot() const override { return "EventListener"; }

 protected:
  explicit EventListener(ListenerType type) : type_(type) {}

 private:
  ListenerType type_;
};

}  // namespace blink

#endif
