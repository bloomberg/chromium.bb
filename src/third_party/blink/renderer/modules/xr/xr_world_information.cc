// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_world_information.h"

#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRWorldInformation::XRWorldInformation(XRSession* session)
    : session_(session) {}

void XRWorldInformation::Trace(blink::Visitor* visitor) {
  visitor->Trace(plane_ids_to_planes_);
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

HeapVector<Member<XRPlane>> XRWorldInformation::detectedPlanes(
    bool& is_null) const {
  DVLOG(3) << __func__;

  HeapVector<Member<XRPlane>> result;

  is_null = is_detected_planes_null_;
  if (is_null)
    return result;

  for (auto& plane_id_and_plane : plane_ids_to_planes_) {
    result.push_back(plane_id_and_plane.value);
  }

  return result;
}

void XRWorldInformation::ProcessPlaneInformation(
    const base::Optional<WTF::Vector<device::mojom::blink::XRPlaneDataPtr>>&
        detected_planes) {
  if (!detected_planes.has_value()) {
    DVLOG(3) << __func__ << ": detected_planes is null";

    // We have received a nullopt - plane detection is not supported or
    // disabled. Mark detected_planes as null & clear stored planes.
    is_detected_planes_null_ = true;
    plane_ids_to_planes_.clear();
    return;
  }

  DVLOG(3) << __func__ << ": detected_planes size=" << detected_planes->size();

  is_detected_planes_null_ = false;

  HeapHashMap<int32_t, Member<XRPlane>> updated_planes;
  for (const auto& plane : *detected_planes) {
    auto it = plane_ids_to_planes_.find(plane->id);
    if (it != plane_ids_to_planes_.end()) {
      updated_planes.insert(plane->id, it->value);
      it->value->Update(plane);
    } else {
      updated_planes.insert(plane->id,
                            MakeGarbageCollected<XRPlane>(session_, plane));
    }
  }

  plane_ids_to_planes_.swap(updated_planes);
}

}  // namespace blink
