// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_anchor_manager.h"

#include "chrome/browser/android/vr/arcore_device/type_converters.h"

namespace device {

ArCoreAnchorManager::ArCoreAnchorManager(util::PassKey<ArCoreImpl> pass_key,
                                         ArSession* arcore_session)
    : arcore_session_(arcore_session) {
  DCHECK(arcore_session_);

  ArAnchorList_create(
      arcore_session_,
      internal::ScopedArCoreObject<ArAnchorList*>::Receiver(arcore_anchors_)
          .get());
  DCHECK(arcore_anchors_.is_valid());

  ArPose_create(
      arcore_session_, nullptr,
      internal::ScopedArCoreObject<ArPose*>::Receiver(ar_pose_).get());
  DCHECK(ar_pose_.is_valid());
}

ArCoreAnchorManager::~ArCoreAnchorManager() = default;

mojom::XRAnchorsDataPtr ArCoreAnchorManager::GetAnchorsData() const {
  DVLOG(3) << __func__ << ": anchor_id_to_anchor_info_.size()="
           << anchor_id_to_anchor_info_.size()
           << ", updated_anchor_ids_.size()=" << updated_anchor_ids_.size();

  std::vector<uint64_t> all_anchors_ids;
  all_anchors_ids.reserve(anchor_id_to_anchor_info_.size());
  for (const auto& anchor_id_and_object : anchor_id_to_anchor_info_) {
    all_anchors_ids.push_back(anchor_id_and_object.first.GetUnsafeValue());
  }

  std::vector<mojom::XRAnchorDataPtr> updated_anchors;
  updated_anchors.reserve(updated_anchor_ids_.size());
  for (const auto& anchor_id : updated_anchor_ids_) {
    const device::internal::ScopedArCoreObject<ArAnchor*>& anchor =
        anchor_id_to_anchor_info_.at(anchor_id).anchor;

    if (anchor_id_to_anchor_info_.at(anchor_id).tracking_state ==
        AR_TRACKING_STATE_TRACKING) {
      // pose
      ArAnchor_getPose(arcore_session_, anchor.get(), ar_pose_.get());
      mojom::Pose pose =
          GetMojomPoseFromArPose(arcore_session_, ar_pose_.get());

      DVLOG(3) << __func__ << ": anchor_id: " << anchor_id.GetUnsafeValue()
               << ", position=" << pose.position.ToString()
               << ", orientation=" << pose.orientation.ToString();

      updated_anchors.push_back(mojom::XRAnchorData::New(
          anchor_id.GetUnsafeValue(), device::mojom::Pose::New(pose)));
    } else {
      DVLOG(3) << __func__ << ": anchor_id: " << anchor_id.GetUnsafeValue()
               << ", position=untracked, orientation=untracked";

      updated_anchors.push_back(
          mojom::XRAnchorData::New(anchor_id.GetUnsafeValue(), nullptr));
    }
  }

  return mojom::XRAnchorsData::New(std::move(all_anchors_ids),
                                   std::move(updated_anchors));
}

template <typename FunctionType>
void ArCoreAnchorManager::ForEachArCoreAnchor(ArAnchorList* arcore_anchors,
                                              FunctionType fn) {
  DCHECK(arcore_anchors);

  int32_t anchor_list_size;
  ArAnchorList_getSize(arcore_session_, arcore_anchors, &anchor_list_size);

  for (int i = 0; i < anchor_list_size; i++) {
    internal::ScopedArCoreObject<ArAnchor*> anchor;
    ArAnchorList_acquireItem(
        arcore_session_, arcore_anchors, i,
        internal::ScopedArCoreObject<ArAnchor*>::Receiver(anchor).get());

    ArTrackingState tracking_state;
    ArAnchor_getTrackingState(arcore_session_, anchor.get(), &tracking_state);

    if (tracking_state == ArTrackingState::AR_TRACKING_STATE_STOPPED) {
      // Skip all anchors that are not currently tracked.
      continue;
    }

    fn(std::move(anchor), tracking_state);
  }
}

void ArCoreAnchorManager::Update(ArFrame* ar_frame) {
  // First, ask ARCore about all Anchors updated in the current frame.
  ArFrame_getUpdatedAnchors(arcore_session_, ar_frame, arcore_anchors_.get());

  // Collect the IDs of updated anchors.
  std::set<AnchorId> updated_anchor_ids;
  ForEachArCoreAnchor(arcore_anchors_.get(), [this, &updated_anchor_ids](
                                                 device::internal::
                                                     ScopedArCoreObject<
                                                         ArAnchor*> ar_anchor,
                                                 ArTrackingState
                                                     tracking_state) {
    // ID
    AnchorId anchor_id;
    bool created;
    std::tie(anchor_id, created) = CreateOrGetAnchorId(ar_anchor.get());

    DVLOG(3) << __func__
             << ": anchor updated, anchor_id=" << anchor_id.GetUnsafeValue()
             << ", tracking_state=" << tracking_state;

    DCHECK(!created)
        << "Anchor creation is app-initiated - we should never encounter an "
           "anchor that was created outside of `ArCoreImpl::CreateAnchor()`.";

    updated_anchor_ids.insert(anchor_id);
  });

  DVLOG(3) << __func__
           << ": updated_anchor_ids.size()=" << updated_anchor_ids.size();

  // Then, ask about all Anchors that are still tracked.
  ArSession_getAllAnchors(arcore_session_, arcore_anchors_.get());

  // Collect the objects of all currently tracked anchors.
  // |ar_plane_address_to_id_| should *not* grow.
  std::map<AnchorId, AnchorInfo> new_anchor_id_to_anchor_info;
  ForEachArCoreAnchor(arcore_anchors_.get(), [this,
                                              &new_anchor_id_to_anchor_info,
                                              &updated_anchor_ids](
                                                 device::internal::
                                                     ScopedArCoreObject<
                                                         ArAnchor*> anchor,
                                                 ArTrackingState
                                                     tracking_state) {
    // ID
    AnchorId anchor_id;
    bool created;
    std::tie(anchor_id, created) = CreateOrGetAnchorId(anchor.get());

    DVLOG(3) << __func__
             << ": anchor present, anchor_id=" << anchor_id.GetUnsafeValue()
             << ", tracking state=" << tracking_state;

    DCHECK(!created)
        << "Anchor creation is app-initiated - we should never encounter an "
           "anchor that was created outside of `ArCoreImpl::CreateAnchor()`.";

    // Inspect the tracking state of this anchor in the previous frame. If it
    // changed, mark the anchor as updated.
    if (base::Contains(anchor_id_to_anchor_info_, anchor_id) &&
        anchor_id_to_anchor_info_.at(anchor_id).tracking_state !=
            tracking_state) {
      updated_anchor_ids.insert(anchor_id);
    }

    AnchorInfo new_anchor_info = AnchorInfo(std::move(anchor), tracking_state);

    new_anchor_id_to_anchor_info.emplace(anchor_id, std::move(new_anchor_info));
  });

  DVLOG(3) << __func__ << ": new_anchor_id_to_anchor_info.size()="
           << new_anchor_id_to_anchor_info.size();

  // Shrink |ar_plane_address_to_id_|, removing all planes that are no longer
  // tracked or were subsumed - if they do not show up in
  // |plane_id_to_plane_object| map, they are no longer tracked.
  base::EraseIf(
      ar_anchor_address_to_id_,
      [&new_anchor_id_to_anchor_info](const auto& anchor_address_and_id) {
        return !base::Contains(new_anchor_id_to_anchor_info,
                               anchor_address_and_id.second);
      });
  anchor_id_to_anchor_info_.swap(new_anchor_id_to_anchor_info);
  updated_anchor_ids_.swap(updated_anchor_ids);
}

std::pair<AnchorId, bool> ArCoreAnchorManager::CreateOrGetAnchorId(
    void* anchor_address) {
  auto it = ar_anchor_address_to_id_.find(anchor_address);
  if (it != ar_anchor_address_to_id_.end()) {
    return std::make_pair(it->second, false);
  }

  CHECK(next_id_ != std::numeric_limits<uint64_t>::max())
      << "preventing ID overflow";

  uint64_t current_id = next_id_++;
  ar_anchor_address_to_id_.emplace(anchor_address, current_id);

  return std::make_pair(AnchorId(current_id), true);
}

base::Optional<AnchorId> ArCoreAnchorManager::CreateAnchor(
    const device::mojom::Pose& pose) {
  auto ar_pose = GetArPoseFromMojomPose(arcore_session_, pose);

  device::internal::ScopedArCoreObject<ArAnchor*> ar_anchor;
  ArStatus status = ArSession_acquireNewAnchor(
      arcore_session_, ar_pose.get(),
      device::internal::ScopedArCoreObject<ArAnchor*>::Receiver(ar_anchor)
          .get());

  if (status != AR_SUCCESS) {
    return base::nullopt;
  }

  AnchorId anchor_id;
  bool created;
  std::tie(anchor_id, created) = CreateOrGetAnchorId(ar_anchor.get());

  DCHECK(created) << "This should always be a new anchor, not something we've "
                     "seen previously.";

  // Mark new anchor as updated to ensure we send its information over to blink:
  updated_anchor_ids_.insert(anchor_id);

  ArTrackingState tracking_state;
  ArAnchor_getTrackingState(arcore_session_, ar_anchor.get(), &tracking_state);

  anchor_id_to_anchor_info_.emplace(
      anchor_id, AnchorInfo(std::move(ar_anchor), tracking_state));

  return anchor_id;
}

base::Optional<AnchorId> ArCoreAnchorManager::CreateAnchor(
    ArCorePlaneManager* plane_manager,
    const device::mojom::Pose& pose,
    PlaneId plane_id) {
  DCHECK(plane_manager);

  DVLOG(2) << __func__ << ": plane_id=" << plane_id;

  auto ar_anchor = plane_manager->CreateAnchor(
      util::PassKey<ArCoreAnchorManager>(), plane_id, pose);
  if (!ar_anchor.is_valid()) {
    return base::nullopt;
  }

  AnchorId anchor_id;
  bool created;
  std::tie(anchor_id, created) = CreateOrGetAnchorId(ar_anchor.get());

  DCHECK(created) << "This should always be a new anchor, not something we've "
                     "seen previously.";

  // Mark new anchor as updated to ensure we send its information over to blink:
  updated_anchor_ids_.insert(anchor_id);

  ArTrackingState tracking_state;
  ArAnchor_getTrackingState(arcore_session_, ar_anchor.get(), &tracking_state);

  anchor_id_to_anchor_info_.emplace(
      anchor_id, AnchorInfo(std::move(ar_anchor), tracking_state));

  return anchor_id;
}

void ArCoreAnchorManager::DetachAnchor(AnchorId anchor_id) {
  auto it = anchor_id_to_anchor_info_.find(anchor_id);
  if (it == anchor_id_to_anchor_info_.end()) {
    return;
  }

  ArAnchor_detach(arcore_session_, it->second.anchor.get());

  anchor_id_to_anchor_info_.erase(it);
}

bool ArCoreAnchorManager::AnchorExists(AnchorId id) const {
  return base::Contains(anchor_id_to_anchor_info_, id);
}

base::Optional<gfx::Transform> ArCoreAnchorManager::GetMojoFromAnchor(
    AnchorId id) const {
  auto it = anchor_id_to_anchor_info_.find(id);
  if (it == anchor_id_to_anchor_info_.end()) {
    return base::nullopt;
  }

  ArAnchor_getPose(arcore_session_, it->second.anchor.get(), ar_pose_.get());
  mojom::Pose mojo_pose =
      GetMojomPoseFromArPose(arcore_session_, ar_pose_.get());

  return mojo::ConvertTo<gfx::Transform>(mojo_pose);
}

ArCoreAnchorManager::AnchorInfo::AnchorInfo(
    device::internal::ScopedArCoreObject<ArAnchor*> anchor,
    ArTrackingState tracking_state)
    : anchor(std::move(anchor)), tracking_state(tracking_state) {}

ArCoreAnchorManager::AnchorInfo::AnchorInfo(AnchorInfo&& other) = default;
ArCoreAnchorManager::AnchorInfo::~AnchorInfo() = default;

}  // namespace device
