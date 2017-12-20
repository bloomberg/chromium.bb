// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeolocationWatchers_h
#define GeolocationWatchers_h

#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"

namespace blink {

class GeoNotifier;

class GeolocationWatchers : public TraceWrapperBase {
  DISALLOW_NEW();

 public:
  GeolocationWatchers() {}
  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

  bool Add(int id, GeoNotifier*);
  GeoNotifier* Find(int id);
  void Remove(int id);
  void Remove(GeoNotifier*);
  bool Contains(GeoNotifier*) const;
  void Clear();
  bool IsEmpty() const;
  void Swap(GeolocationWatchers& other);

  void GetNotifiersVector(HeapVector<Member<GeoNotifier>>&) const;

 private:
  typedef HeapHashMap<int, TraceWrapperMember<GeoNotifier>> IdToNotifierMap;
  typedef HeapHashMap<Member<GeoNotifier>, int> NotifierToIdMap;

  IdToNotifierMap id_to_notifier_map_;
  NotifierToIdMap notifier_to_id_map_;
};

inline void swap(GeolocationWatchers& a, GeolocationWatchers& b) {
  a.Swap(b);
}

}  // namespace blink

#endif  // GeolocationWatchers_h
