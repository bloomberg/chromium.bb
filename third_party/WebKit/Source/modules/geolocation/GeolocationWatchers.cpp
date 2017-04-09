// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/geolocation/GeolocationWatchers.h"

#include "modules/geolocation/GeoNotifier.h"
#include "wtf/Assertions.h"

namespace blink {

DEFINE_TRACE(GeolocationWatchers) {
  visitor->Trace(id_to_notifier_map_);
  visitor->Trace(notifier_to_id_map_);
}

bool GeolocationWatchers::Add(int id, GeoNotifier* notifier) {
  DCHECK_GT(id, 0);
  if (!id_to_notifier_map_.insert(id, notifier).is_new_entry)
    return false;
  notifier_to_id_map_.Set(notifier, id);
  return true;
}

GeoNotifier* GeolocationWatchers::Find(int id) {
  DCHECK_GT(id, 0);
  IdToNotifierMap::iterator iter = id_to_notifier_map_.Find(id);
  if (iter == id_to_notifier_map_.end())
    return 0;
  return iter->value;
}

void GeolocationWatchers::Remove(int id) {
  DCHECK_GT(id, 0);
  IdToNotifierMap::iterator iter = id_to_notifier_map_.Find(id);
  if (iter == id_to_notifier_map_.end())
    return;
  notifier_to_id_map_.erase(iter->value);
  id_to_notifier_map_.erase(iter);
}

void GeolocationWatchers::Remove(GeoNotifier* notifier) {
  NotifierToIdMap::iterator iter = notifier_to_id_map_.Find(notifier);
  if (iter == notifier_to_id_map_.end())
    return;
  id_to_notifier_map_.erase(iter->value);
  notifier_to_id_map_.erase(iter);
}

bool GeolocationWatchers::Contains(GeoNotifier* notifier) const {
  return notifier_to_id_map_.Contains(notifier);
}

void GeolocationWatchers::Clear() {
  id_to_notifier_map_.Clear();
  notifier_to_id_map_.Clear();
}

bool GeolocationWatchers::IsEmpty() const {
  return id_to_notifier_map_.IsEmpty();
}

void GeolocationWatchers::GetNotifiersVector(
    HeapVector<Member<GeoNotifier>>& copy) const {
  CopyValuesToVector(id_to_notifier_map_, copy);
}

}  // namespace blink
