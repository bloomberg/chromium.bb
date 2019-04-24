// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_KEYBOARD_DELEGATE_H_
#define CHROME_BROWSER_VR_TEST_MOCK_KEYBOARD_DELEGATE_H_

#include "base/macros.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/model/camera_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/transform.h"

namespace vr {

class MockKeyboardDelegate : public KeyboardDelegate {
 public:
  MockKeyboardDelegate();
  ~MockKeyboardDelegate() override;

  MOCK_METHOD0(ShowKeyboard, void());
  MOCK_METHOD0(HideKeyboard, void());
  MOCK_METHOD1(SetTransform, void(const gfx::Transform&));
  MOCK_METHOD3(HitTest,
               bool(const gfx::Point3F&, const gfx::Point3F&, gfx::Point3F*));
  MOCK_METHOD0(OnBeginFrame, void());
  MOCK_METHOD1(Draw, void(const CameraModel&));
  MOCK_METHOD0(SupportsSelection, bool());
  MOCK_METHOD1(OnHoverEnter, void(const gfx::PointF&));
  MOCK_METHOD0(OnHoverLeave, void());
  MOCK_METHOD1(OnHoverMove, void(const gfx::PointF&));
  MOCK_METHOD1(OnButtonDown, void(const gfx::PointF&));
  MOCK_METHOD1(OnButtonUp, void(const gfx::PointF&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockKeyboardDelegate);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_MOCK_KEYBOARD_DELEGATE_H_
