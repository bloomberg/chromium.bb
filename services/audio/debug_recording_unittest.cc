// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/debug_recording.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/test/scoped_task_environment.h"
#include "media/audio/audio_debug_recording_test.h"
#include "media/audio/mock_audio_debug_recording_manager.h"
#include "media/audio/mock_audio_manager.h"
#include "services/audio/public/cpp/debug_recording_session.h"
#include "services/audio/public/mojom/debug_recording.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace audio {

namespace {
const base::FilePath::CharType kBaseFileName[] =
    FILE_PATH_LITERAL("base_file_name");
}

class DebugRecordingTest : public media::AudioDebugRecordingTest {
 public:
  DebugRecordingTest() = default;
  ~DebugRecordingTest() override = default;

  void SetUp() override {
    CreateAudioManager();
    InitializeAudioDebugRecordingManager();
  }

  void TearDown() override { ShutdownAudioManager(); }

 protected:
  void CreateDebugRecording() {
    debug_recording_ = std::make_unique<DebugRecording>(
        mojo::MakeRequest(&debug_recording_ptr_),
        static_cast<media::AudioManager*>(mock_audio_manager_.get()));
    EXPECT_TRUE(debug_recording_ptr_.is_bound());
  }

  void EnableDebugRecording() {
    mojom::DebugRecordingFileProviderPtr file_provider_ptr;
    DebugRecordingSession::DebugRecordingFileProvider file_provider(
        mojo::MakeRequest(&file_provider_ptr), base::FilePath(kBaseFileName));
    ASSERT_TRUE(file_provider_ptr.is_bound());
    debug_recording_ptr_->Enable(std::move(file_provider_ptr));
  }

  void DestroyDebugRecording() { debug_recording_ptr_.reset(); }

  MOCK_METHOD0(OnConnectionError, void());

 private:
  std::unique_ptr<DebugRecording> debug_recording_;
  mojom::DebugRecordingPtr debug_recording_ptr_;

  DISALLOW_COPY_AND_ASSIGN(DebugRecordingTest);
};

TEST_F(DebugRecordingTest, EnableResetEnablesDisablesDebugRecording) {
  CreateDebugRecording();

  EXPECT_CALL(*mock_debug_recording_manager_, EnableDebugRecording(testing::_));
  EnableDebugRecording();

  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording());
  DestroyDebugRecording();
}

TEST_F(DebugRecordingTest, ResetWithoutEnableDoesNotDisableDebugRecording) {
  CreateDebugRecording();

  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording()).Times(0);
  DestroyDebugRecording();
}

}  // namespace audio
