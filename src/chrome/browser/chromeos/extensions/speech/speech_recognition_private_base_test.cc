// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_base_test.h"

#include "content/public/test/fake_speech_recognition_manager.h"

namespace {
const char kBasePath[] = "speech/speech_recognition_private/";
}  // namespace

namespace extensions {

SpeechRecognitionPrivateBaseTest::SpeechRecognitionPrivateBaseTest()
    : test_helper_(GetParam()) {}

SpeechRecognitionPrivateBaseTest::~SpeechRecognitionPrivateBaseTest() = default;

void SpeechRecognitionPrivateBaseTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  std::vector<base::Feature> enabled_features =
      test_helper_.GetEnabledFeatures();
  std::vector<base::Feature> disabled_features =
      test_helper_.GetDisabledFeatures();
  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

  ExtensionApiTest::SetUpCommandLine(command_line);
}

void SpeechRecognitionPrivateBaseTest::SetUpOnMainThread() {
  test_helper_.SetUp(browser()->profile());
  ExtensionApiTest::SetUpOnMainThread();
}

void SpeechRecognitionPrivateBaseTest::TearDownOnMainThread() {
  if (GetParam() == speech::SpeechRecognitionType::kNetwork)
    content::SpeechRecognitionManager::SetManagerForTesting(nullptr);

  ExtensionApiTest::TearDownOnMainThread();
}

bool SpeechRecognitionPrivateBaseTest::RunSpeechRecognitionPrivateTest(
    const std::string& dir_name) {
  const std::string path = kBasePath + dir_name;
  return RunExtensionTest(path.c_str(), {}, {.load_as_component = true});
}

const Extension* SpeechRecognitionPrivateBaseTest::LoadExtensionAsComponent(
    const std::string& dir_name) {
  const std::string path = kBasePath + dir_name;
  return LoadExtension(test_data_dir_.AppendASCII(path.c_str()),
                       {.load_as_component = true});
}

void SpeechRecognitionPrivateBaseTest::WaitForRecognitionStarted() {
  test_helper_.WaitForRecognitionStarted();
}

void SpeechRecognitionPrivateBaseTest::WaitForRecognitionStopped() {
  test_helper_.WaitForRecognitionStopped();
}

void SpeechRecognitionPrivateBaseTest::SendFakeSpeechResultAndWait(
    const std::string& transcript,
    bool is_final) {
  test_helper_.SendFakeSpeechResultAndWait(transcript, is_final);
}

void SpeechRecognitionPrivateBaseTest::SendFinalFakeSpeechResultAndWait(
    const std::string& transcript) {
  test_helper_.SendFinalFakeSpeechResultAndWait(transcript);
}

void SpeechRecognitionPrivateBaseTest::SendFakeSpeechRecognitionErrorAndWait() {
  test_helper_.SendFakeSpeechRecognitionErrorAndWait();
}

}  // namespace extensions
