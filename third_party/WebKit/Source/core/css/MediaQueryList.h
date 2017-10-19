/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef MediaQueryList_h
#define MediaQueryList_h

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/events/EventTarget.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class ExecutionContext;
class MediaQueryListListener;
class MediaQueryMatcher;
class MediaQuerySet;

// MediaQueryList interface is specified at
// http://dev.w3.org/csswg/cssom-view/#the-mediaquerylist-interface
// The objects of this class are returned by window.matchMedia. They may be used
// to retrieve the current value of the given media query and to add/remove
// listeners that will be called whenever the value of the query changes.

class CORE_EXPORT MediaQueryList final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<MediaQueryList>,
      public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(MediaQueryList);
  WTF_MAKE_NONCOPYABLE(MediaQueryList);

 public:
  static MediaQueryList* Create(ExecutionContext*,
                                MediaQueryMatcher*,
                                RefPtr<MediaQuerySet>);
  ~MediaQueryList() override;

  String media() const;
  bool matches();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(change);

  // These two functions are provided for compatibility with JS code
  // written before the change listener became a DOM event.
  void addDeprecatedListener(EventListener*);
  void removeDeprecatedListener(EventListener*);

  // C++ code can use these functions to listen to changes instead of having to
  // use DOM event listeners.
  void AddListener(MediaQueryListListener*);
  void RemoveListener(MediaQueryListListener*);

  // Will return true if a DOM event should be scheduled.
  bool MediaFeaturesChanged(
      HeapVector<Member<MediaQueryListListener>>* listeners_to_notify);

  virtual void Trace(blink::Visitor*);

  // From ScriptWrappable
  bool HasPendingActivity() const final;

  // From ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

 private:
  MediaQueryList(ExecutionContext*, MediaQueryMatcher*, RefPtr<MediaQuerySet>);

  bool UpdateMatches();

  Member<MediaQueryMatcher> matcher_;
  RefPtr<MediaQuerySet> media_;
  using ListenerList = HeapListHashSet<Member<MediaQueryListListener>>;
  ListenerList listeners_;
  bool matches_dirty_;
  bool matches_;
};

}  // namespace blink

#endif  // MediaQueryList_h
