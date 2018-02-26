// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/debug_recording_session.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_task_environment.h"
#include "media/audio/audio_debug_recording_test.h"
#include "media/audio/mock_audio_debug_recording_manager.h"
#include "media/audio/mock_audio_manager.h"
#include "services/audio/service_factory.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {

namespace {

const base::FilePath::CharType kBaseFileName[] =
    FILE_PATH_LITERAL("debug_recording");
const base::FilePath::CharType kWavExtension[] = FILE_PATH_LITERAL("wav");
const base::FilePath::CharType kValidSuffix[] = FILE_PATH_LITERAL("output.1");
const base::FilePath::CharType kEmptySuffix[] = FILE_PATH_LITERAL("");
const base::FilePath::CharType kInvalidSuffix[] =
    FILE_PATH_LITERAL("/../invalid");

}  // namespace

class DebugRecordingFileProviderTest : public testing::Test {
 public:
  DebugRecordingFileProviderTest() = default;
  ~DebugRecordingFileProviderTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.GetPath().Append(kBaseFileName);
    file_provider_ =
        std::make_unique<DebugRecordingSession::DebugRecordingFileProvider>(
            mojo::MakeRequest(&file_provider_ptr_), file_path_);
    ASSERT_TRUE(file_provider_ptr_.is_bound());
  }

  void TearDown() override { file_provider_.reset(); }

  base::FilePath GetFileName(base::FilePath suffix) {
    return file_path_.AddExtension(suffix.value()).AddExtension(kWavExtension);
  }

  MOCK_METHOD1(OnFileCreated, void(bool));
  void FileCreated(base::File file) { OnFileCreated(file.IsValid()); }

 protected:
  mojom::DebugRecordingFileProviderPtr file_provider_ptr_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  std::unique_ptr<DebugRecordingSession::DebugRecordingFileProvider>
      file_provider_;
  base::ScopedTempDir temp_dir_;
  base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(DebugRecordingFileProviderTest);
};

class DebugRecordingSessionTest : public media::AudioDebugRecordingTest {
 public:
  DebugRecordingSessionTest() = default;
  ~DebugRecordingSessionTest() override = default;

  void SetUp() override {
    CreateAudioManager();
    InitializeAudioDebugRecordingManager();

    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            CreateEmbeddedService(
                static_cast<media::AudioManager*>(mock_audio_manager_.get())));
    scoped_task_environment_.RunUntilIdle();
  }

  void TearDown() override { ShutdownAudioManager(); }

 protected:
  std::unique_ptr<DebugRecordingSession> CreateDebugRecordingSession() {
    std::unique_ptr<DebugRecordingSession> session(
        std::make_unique<DebugRecordingSession>(
            base::FilePath(kBaseFileName),
            connector_factory_->CreateConnector()));
    scoped_task_environment_.RunUntilIdle();
    return session;
  }

  void DestroyDebugRecordingSession(
      std::unique_ptr<DebugRecordingSession> debug_recording_session) {
    debug_recording_session.reset();
    scoped_task_environment_.RunUntilIdle();
  }

 private:
  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;

  DISALLOW_COPY_AND_ASSIGN(DebugRecordingSessionTest);
};

TEST_F(DebugRecordingFileProviderTest, CreateWithValidSuffixReturnsValidFile) {
  base::FilePath suffix(kValidSuffix);
  EXPECT_CALL(*this, OnFileCreated(true));
  file_provider_ptr_->CreateWavFile(
      suffix, base::BindOnce(&DebugRecordingFileProviderTest::FileCreated,
                             base::Unretained(this)));
  scoped_task_environment_.RunUntilIdle();

  base::FilePath file_name(GetFileName(suffix));
  EXPECT_TRUE(base::PathExists(file_name));
  ASSERT_TRUE(base::DeleteFile(file_name, false));
}

TEST_F(DebugRecordingFileProviderTest, CreateWithEmptySuffixReturnsValidFile) {
  base::FilePath suffix(kEmptySuffix);
  EXPECT_CALL(*this, OnFileCreated(true));
  file_provider_ptr_->CreateWavFile(
      suffix, base::BindOnce(&DebugRecordingFileProviderTest::FileCreated,
                             base::Unretained(this)));
  scoped_task_environment_.RunUntilIdle();

  base::FilePath file_name(GetFileName(suffix));
  EXPECT_TRUE(base::PathExists(file_name));
  ASSERT_TRUE(base::DeleteFile(file_name, false));
}

TEST_F(DebugRecordingFileProviderTest,
       CreateWithInvalidSuffixReturnsInvalidFile) {
  base::FilePath suffix(kInvalidSuffix);
  EXPECT_CALL(*this, OnFileCreated(false));
  file_provider_ptr_->CreateWavFile(
      suffix, base::BindOnce(&DebugRecordingFileProviderTest::FileCreated,
                             base::Unretained(this)));
  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::PathExists(GetFileName(suffix)));
}

TEST_F(DebugRecordingSessionTest,
       CreateDestroySessionEnablesDisablesDebugRecording) {
  EXPECT_CALL(*mock_debug_recording_manager_, EnableDebugRecording(testing::_));
  std::unique_ptr<DebugRecordingSession> session =
      CreateDebugRecordingSession();

  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording());
  DestroyDebugRecordingSession(std::move(session));
}

TEST_F(DebugRecordingSessionTest,
       CreateTwoSessionsFirstSessionDestroyedOnSecondSessionCreation) {
  testing::InSequence seq;

  EXPECT_CALL(*mock_debug_recording_manager_, EnableDebugRecording(testing::_));
  std::unique_ptr<DebugRecordingSession> first_session =
      CreateDebugRecordingSession();

  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording());
  EXPECT_CALL(*mock_debug_recording_manager_, EnableDebugRecording(testing::_));
  std::unique_ptr<DebugRecordingSession> second_session =
      CreateDebugRecordingSession();

  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording()).Times(0);
  DestroyDebugRecordingSession(std::move(first_session));
  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording());
  DestroyDebugRecordingSession(std::move(second_session));
}

}  // namespace audio
