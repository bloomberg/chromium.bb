// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_FAKE_ARCORE_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_FAKE_ARCORE_H_

#include <memory>
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "chrome/browser/android/vr/arcore_device/arcore.h"

namespace device {

// Minimal fake ArCore implementation for testing. It can populate
// the camera texture with a GL_TEXTURE_OES image and do UV transform
// calculations.
class FakeArCore : public ArCore {
 public:
  FakeArCore();
  ~FakeArCore() override;

  // ArCore implementation.
  bool Initialize(
      base::android::ScopedJavaLocalRef<jobject> application_context) override;
  void SetCameraTexture(GLuint texture) override;
  void SetDisplayGeometry(const gfx::Size& frame_size,
                          display::Display::Rotation display_rotation) override;
  std::vector<float> TransformDisplayUvCoords(
      const base::span<const float> uvs) override;
  gfx::Transform GetProjectionMatrix(float near, float far) override;
  mojom::VRPosePtr Update(bool* camera_updated) override;
  base::TimeDelta GetFrameTimestamp() override;

  void Pause() override;
  void Resume() override;

  float GetEstimatedFloorHeight() override;

  bool RequestHitTest(const mojom::XRRayPtr& ray,
                      std::vector<mojom::XRHitResultPtr>* hit_results) override;

  base::Optional<uint64_t> SubscribeToHitTest(
      mojom::XRNativeOriginInformationPtr nativeOriginInformation,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray) override;
  base::Optional<uint64_t> SubscribeToHitTestForTransientInput(
      const std::string& profile_name,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray) override;

  mojom::XRHitTestSubscriptionResultsDataPtr GetHitTestSubscriptionResults(
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state) override;

  void UnsubscribeFromHitTest(uint64_t subscription_id) override;

  mojom::XRPlaneDetectionDataPtr GetDetectedPlanesData() override;
  mojom::XRAnchorsDataPtr GetAnchorsData() override;
  mojom::XRLightEstimationDataPtr GetLightEstimationData() override;

  void CreateAnchor(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const mojom::Pose& native_origin_from_anchor,
      CreateAnchorCallback callback) override;
  void CreatePlaneAttachedAnchor(const mojom::Pose& plane_from_anchor,
                                 uint64_t plane_id,
                                 CreateAnchorCallback callback) override;

  void ProcessAnchorCreationRequests(
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state,
      const base::TimeTicks& frame_time) override;

  void DetachAnchor(uint64_t anchor_id) override;

  void SetCameraAspect(float aspect) { camera_aspect_ = aspect; }

 private:
  bool IsOnGlThread() const;

  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;

  float camera_aspect_ = 1.0f;
  display::Display::Rotation display_rotation_ =
      display::Display::Rotation::ROTATE_0;
  gfx::Size frame_size_;

  struct FakeAnchorData {
    gfx::Point3F position;
    gfx::Quaternion orientation;
  };

  std::unordered_map<uint64_t, FakeAnchorData> anchors_;

  DISALLOW_COPY_AND_ASSIGN(FakeArCore);
};

class FakeArCoreFactory : public ArCoreFactory {
 public:
  std::unique_ptr<ArCore> Create() override;
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_FAKE_ARCORE_H_
