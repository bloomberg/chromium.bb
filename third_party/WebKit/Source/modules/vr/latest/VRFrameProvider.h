// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRFrameProvider_h
#define VRFrameProvider_h

#include "device/vr/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class ScriptPromiseResolver;
class VRDevice;
class VRSession;

// This class manages requesting and dispatching frame updates, which includes
// pose information for a given VRDevice.
class VRFrameProvider final
    : public GarbageCollectedFinalized<VRFrameProvider> {
 public:
  explicit VRFrameProvider(VRDevice*);

  VRSession* exclusive_session() const { return exclusive_session_; }

  void BeginExclusiveSession(VRSession*, ScriptPromiseResolver*);
  void OnExclusiveSessionEnded();

  void RequestFrame(VRSession*);

  void OnExclusiveVSync(double timestamp);
  void OnNonExclusiveVSync(double timestamp);

  void SubmitFrame(gpu::MailboxHolder);

  virtual void Trace(blink::Visitor*);

 private:
  void OnNonExclusivePose(device::mojom::blink::VRPosePtr);

  void ScheduleExclusiveFrame();
  void ScheduleNonExclusiveFrame();

  void OnPresentComplete(bool success);
  void ProcessScheduledFrame(double timestamp);

  const Member<VRDevice> device_;
  Member<VRSession> exclusive_session_;
  Member<ScriptPromiseResolver> pending_exclusive_session_resolver_;

  // Non-exclusive Sessions which have requested a frame update.
  HeapVector<Member<VRSession>> requesting_sessions_;

  device::mojom::blink::VRMagicWindowProviderPtr magic_window_provider_;
  device::mojom::blink::VRPosePtr frame_pose_;

  // This frame ID is VR-specific and is used to track when frames arrive at the
  // VR compositor so that it knows which poses to use, when to apply bounds
  // updates, etc.
  int16_t frame_id_ = -1;
  bool pending_exclusive_vsync_ = false;
  bool pending_non_exclusive_vsync_ = false;
  bool vsync_connection_failed_ = false;
  double timebase_ = -1;

  bool pending_submit_frame_ = false;
  bool pending_previous_frame_render_ = false;
};

}  // namespace blink

#endif  // VRFrameProvider_h
