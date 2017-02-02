// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLMediaElement.h"

#include "core/frame/Settings.h"
#include "core/html/HTMLAudioElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/page/NetworkStateNotifier.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

enum class TestParam { Audio, Video };

class HTMLMediaElementTest : public ::testing::TestWithParam<TestParam> {
 protected:
  void SetUp() {
    m_dummyPageHolder = DummyPageHolder::create();

    if (GetParam() == TestParam::Audio)
      m_media = HTMLAudioElement::create(m_dummyPageHolder->document());
    else
      m_media = HTMLVideoElement::create(m_dummyPageHolder->document());
  }

  HTMLMediaElement* media() { return m_media.get(); }
  void setCurrentSrc(const String& src) {
    KURL url(ParsedURLString, src);
    media()->m_currentSrc = url;
  }

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  Persistent<HTMLMediaElement> m_media;
};

INSTANTIATE_TEST_CASE_P(Audio,
                        HTMLMediaElementTest,
                        ::testing::Values(TestParam::Audio));
INSTANTIATE_TEST_CASE_P(Video,
                        HTMLMediaElementTest,
                        ::testing::Values(TestParam::Video));

TEST_P(HTMLMediaElementTest, effectiveMediaVolume) {
  struct TestData {
    double volume;
    bool muted;
    double effectiveVolume;
  } test_data[] = {
      {0.0, false, 0.0}, {0.5, false, 0.5}, {1.0, false, 1.0},
      {0.0, true, 0.0},  {0.5, true, 0.0},  {1.0, true, 0.0},
  };

  for (const auto& data : test_data) {
    media()->setVolume(data.volume);
    media()->setMuted(data.muted);
    EXPECT_EQ(data.effectiveVolume, media()->effectiveMediaVolume());
  }
}

enum class TestURLScheme {
  kHttp,
  kHttps,
  kFtp,
  kFile,
  kData,
  kBlob,
};

String srcSchemeToURL(TestURLScheme scheme) {
  switch (scheme) {
    case TestURLScheme::kHttp:
      return "http://example.com/foo.mp4";
    case TestURLScheme::kHttps:
      return "https://example.com/foo.mp4";
    case TestURLScheme::kFtp:
      return "ftp://example.com/foo.mp4";
    case TestURLScheme::kFile:
      return "file:///foo/bar.mp4";
    case TestURLScheme::kData:
      return "data:video/mp4;base64,XXXXXXX";
    case TestURLScheme::kBlob:
      return "blob:http://example.com/00000000-0000-0000-0000-000000000000";
    default:
      NOTREACHED();
  }
  return emptyString;
}

TEST_P(HTMLMediaElementTest, preloadType) {
  struct TestData {
    bool dataSaverEnabled;
    bool forcePreloadNoneForMediaElements;
    bool isCellular;
    TestURLScheme srcScheme;
    AtomicString preloadToSet;
    AtomicString preloadExpected;
  } testData[] = {
      // Tests for conditions in which preload type should be overriden to
      // "none".
      {false, true, false, TestURLScheme::kHttp, "auto", "none"},
      {true, true, false, TestURLScheme::kHttps, "auto", "none"},
      {true, true, false, TestURLScheme::kFtp, "metadata", "none"},
      {false, false, false, TestURLScheme::kHttps, "auto", "auto"},
      {false, true, false, TestURLScheme::kFile, "auto", "auto"},
      {false, true, false, TestURLScheme::kData, "metadata", "metadata"},
      {false, true, false, TestURLScheme::kBlob, "auto", "auto"},
      {false, true, false, TestURLScheme::kFile, "none", "none"},
      // Tests for conditions in which preload type should be overriden to
      // "metadata".
      {false, false, true, TestURLScheme::kHttp, "auto", "metadata"},
      {false, false, true, TestURLScheme::kHttp, "scheme", "metadata"},
      {false, false, true, TestURLScheme::kHttp, "none", "none"},
      // Tests that the preload is overriden to "auto"
      {false, false, false, TestURLScheme::kHttp, "foo", "auto"},
  };

  int index = 0;
  for (const auto& data : testData) {
    media()->document().settings()->setDataSaverEnabled(data.dataSaverEnabled);
    media()->document().settings()->setForcePreloadNoneForMediaElements(
        data.forcePreloadNoneForMediaElements);
    if (data.isCellular) {
      networkStateNotifier().setOverride(
          true, WebConnectionType::WebConnectionTypeCellular3G, 2.0);
    } else {
      networkStateNotifier().clearOverride();
    }
    setCurrentSrc(srcSchemeToURL(data.srcScheme));
    media()->setPreload(data.preloadToSet);

    EXPECT_EQ(data.preloadExpected, media()->preload())
        << "preload type differs at index" << index;
    ++index;
  }
}

}  // namespace blink
