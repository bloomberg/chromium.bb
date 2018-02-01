// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRFrameProvider_h
#define XRFrameProvider_h

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class ScriptPromiseResolver;
class XRDevice;
class XRSession;
class XRFrameTransport;
class XRWebGLLayer;

// This class manages requesting and dispatching frame updates, which includes
// pose information for a given XRDevice.
class XRFrameProvider final
    : public GarbageCollectedFinalized<XRFrameProvider> {
 public:
  explicit XRFrameProvider(XRDevice*);

  XRSession* exclusive_session() const { return exclusive_session_; }

  void BeginExclusiveSession(XRSession*, ScriptPromiseResolver*);
  void OnExclusiveSessionEnded();

  void RequestFrame(XRSession*);

  void OnNonExclusiveVSync(double timestamp);

  void SubmitWebGLLayer(XRWebGLLayer*);
  void UpdateWebGLLayerViewports(XRWebGLLayer*);

  void Dispose();

  virtual void Trace(blink::Visitor*);

 private:
  void OnExclusiveVSync(
      device::mojom::blink::VRPosePtr,
      WTF::TimeDelta,
      int16_t frame_id,
      device::mojom::blink::VRPresentationProvider::VSyncStatus);
  void OnNonExclusivePose(device::mojom::blink::VRPosePtr);

  void ScheduleExclusiveFrame();
  void ScheduleNonExclusiveFrame();

  void OnPresentComplete(
      bool success,
      device::mojom::blink::VRDisplayFrameTransportOptionsPtr);
  void OnPresentationProviderConnectionError();
  void ProcessScheduledFrame(double timestamp);

  const Member<XRDevice> device_;
  Member<XRSession> exclusive_session_;
  Member<ScriptPromiseResolver> pending_exclusive_session_resolver_;
  Member<XRFrameTransport> frame_transport_;

  // Non-exclusive Sessions which have requested a frame update.
  HeapVector<Member<XRSession>> requesting_sessions_;

  device::mojom::blink::VRPresentationProviderPtr presentation_provider_;
  device::mojom::blink::VRMagicWindowProviderPtr magic_window_provider_;
  device::mojom::blink::VRPosePtr frame_pose_;

  // This frame ID is XR-specific and is used to track when frames arrive at the
  // XR compositor so that it knows which poses to use, when to apply bounds
  // updates, etc.
  int16_t frame_id_ = -1;
  bool pending_exclusive_vsync_ = false;
  bool pending_non_exclusive_vsync_ = false;
  bool vsync_connection_failed_ = false;
};

}  // namespace blink

#endif  // XRFrameProvider_h
