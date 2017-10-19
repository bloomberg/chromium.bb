// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/media/HTMLMediaElement.h"

#include "core/frame/Settings.h"
#include "core/html/media/HTMLAudioElement.h"
#include "core/html/media/HTMLVideoElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/network/NetworkStateNotifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

enum class MediaTestParam { kAudio, kVideo };

class HTMLMediaElementTest : public ::testing::TestWithParam<MediaTestParam> {
 protected:
  void SetUp() {
    dummy_page_holder_ = DummyPageHolder::Create();

    if (GetParam() == MediaTestParam::kAudio)
      media_ = HTMLAudioElement::Create(dummy_page_holder_->GetDocument());
    else
      media_ = HTMLVideoElement::Create(dummy_page_holder_->GetDocument());
  }

  HTMLMediaElement* Media() { return media_.Get(); }
  void SetCurrentSrc(const String& src) {
    KURL url(src);
    Media()->current_src_ = url;
  }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<HTMLMediaElement> media_;
};

INSTANTIATE_TEST_CASE_P(Audio,
                        HTMLMediaElementTest,
                        ::testing::Values(MediaTestParam::kAudio));
INSTANTIATE_TEST_CASE_P(Video,
                        HTMLMediaElementTest,
                        ::testing::Values(MediaTestParam::kVideo));

TEST_P(HTMLMediaElementTest, effectiveMediaVolume) {
  struct TestData {
    double volume;
    bool muted;
    double effective_volume;
  } test_data[] = {
      {0.0, false, 0.0}, {0.5, false, 0.5}, {1.0, false, 1.0},
      {0.0, true, 0.0},  {0.5, true, 0.0},  {1.0, true, 0.0},
  };

  for (const auto& data : test_data) {
    Media()->setVolume(data.volume);
    Media()->setMuted(data.muted);
    EXPECT_EQ(data.effective_volume, Media()->EffectiveMediaVolume());
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

String SrcSchemeToURL(TestURLScheme scheme) {
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
  return g_empty_string;
}

TEST_P(HTMLMediaElementTest, preloadType) {
  struct TestData {
    bool data_saver_enabled;
    bool force_preload_none_for_media_elements;
    bool is_cellular;
    TestURLScheme src_scheme;
    AtomicString preload_to_set;
    AtomicString preload_expected;
  } test_data[] = {
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
      // Tests that the preload is overriden to "metadata".
      {false, false, false, TestURLScheme::kHttp, "foo", "metadata"},
  };

  int index = 0;
  for (const auto& data : test_data) {
    Media()->GetDocument().GetSettings()->SetDataSaverEnabled(
        data.data_saver_enabled);
    Media()->GetDocument().GetSettings()->SetForcePreloadNoneForMediaElements(
        data.force_preload_none_for_media_elements);
    if (data.is_cellular) {
      GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
          true, WebConnectionType::kWebConnectionTypeCellular3G, 2.0);
    } else {
      GetNetworkStateNotifier().ClearOverride();
    }
    SetCurrentSrc(SrcSchemeToURL(data.src_scheme));
    Media()->setPreload(data.preload_to_set);

    EXPECT_EQ(data.preload_expected, Media()->preload())
        << "preload type differs at index" << index;
    ++index;
  }
}

}  // namespace blink
