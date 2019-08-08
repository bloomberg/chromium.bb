// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_SYSTEM_MEDIA_CONTROLS_MOCK_SYSTEM_MEDIA_CONTROLS_SERVICE_H_
#define UI_BASE_WIN_SYSTEM_MEDIA_CONTROLS_MOCK_SYSTEM_MEDIA_CONTROLS_SERVICE_H_

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/win/system_media_controls/system_media_controls_service.h"

namespace system_media_controls {

class SystemMediaControlsServiceObserver;

namespace testing {

// Mock implementation of SystemMediaControlsService for testing.
class MockSystemMediaControlsService : public SystemMediaControlsService {
 public:
  MockSystemMediaControlsService();
  ~MockSystemMediaControlsService() override;

  // SystemMediaControlsService implementation.
  MOCK_METHOD1(AddObserver, void(SystemMediaControlsServiceObserver* observer));
  MOCK_METHOD1(RemoveObserver,
               void(SystemMediaControlsServiceObserver* observer));
  MOCK_METHOD1(SetEnabled, void(bool enabled));
  MOCK_METHOD1(SetIsNextEnabled, void(bool value));
  MOCK_METHOD1(SetIsPreviousEnabled, void(bool value));
  MOCK_METHOD1(SetIsPlayEnabled, void(bool value));
  MOCK_METHOD1(SetIsPauseEnabled, void(bool value));
  MOCK_METHOD1(SetIsStopEnabled, void(bool value));
  MOCK_METHOD1(SetPlaybackStatus,
               void(ABI::Windows::Media::MediaPlaybackStatus status));
  MOCK_METHOD1(SetTitle, void(const base::string16& title));
  MOCK_METHOD1(SetArtist, void(const base::string16& artist));
  MOCK_METHOD1(SetThumbnail, void(const SkBitmap& bitmap));
  MOCK_METHOD0(ClearThumbnail, void());
  MOCK_METHOD0(ClearMetadata, void());
  MOCK_METHOD0(UpdateDisplay, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSystemMediaControlsService);
};

}  // namespace testing

}  // namespace system_media_controls

#endif  // UI_BASE_WIN_SYSTEM_MEDIA_CONTROLS_MOCK_SYSTEM_MEDIA_CONTROLS_SERVICE_H_
