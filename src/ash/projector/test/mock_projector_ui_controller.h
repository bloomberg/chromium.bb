// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_TEST_MOCK_PROJECTOR_UI_CONTROLLER_H_
#define ASH_PROJECTOR_TEST_MOCK_PROJECTOR_UI_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/projector/projector_ui_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class ProjectorControllerImpl;

// A mock implementation of ProjectorUiController for use in tests.
class ASH_EXPORT MockProjectorUiController : public ProjectorUiController {
 public:
  explicit MockProjectorUiController(
      ProjectorControllerImpl* projector_controller);

  MockProjectorUiController(const MockProjectorUiController&) = delete;
  MockProjectorUiController& operator=(const MockProjectorUiController&) =
      delete;

  ~MockProjectorUiController() override;

  // ProjectorUiController:
  MOCK_METHOD0(ShowToolbar, void());
  MOCK_METHOD0(CloseToolbar, void());
  MOCK_METHOD0(OnKeyIdeaMarked, void());
  MOCK_METHOD0(OnLaserPointerPressed, void());
  MOCK_METHOD0(OnMarkerPressed, void());
  MOCK_METHOD0(OnClearAllMarkersPressed, void());
  MOCK_METHOD2(OnTranscription,
               void(const std::string& transcription, bool is_final));
  MOCK_METHOD1(OnSelfieCamPressed, void(bool enabled));
  MOCK_METHOD1(OnRecordingStateChanged, void(bool started));
  MOCK_METHOD1(OnChangeMarkerColorPressed, void(SkColor new_color));
  MOCK_METHOD1(SetCaptionBubbleState, void(bool));
  MOCK_METHOD1(OnMagnifierButtonPressed, void(bool));
};

}  // namespace ash
#endif  // ASH_PROJECTOR_TEST_MOCK_PROJECTOR_UI_CONTROLLER_H_
