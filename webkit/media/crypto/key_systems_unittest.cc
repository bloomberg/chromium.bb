// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "webkit/media/crypto/key_systems.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

using WebKit::WebString;

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
#define EXPECT_PROPRIETARY EXPECT_TRUE
#else
#define EXPECT_PROPRIETARY EXPECT_FALSE
#endif

#if defined(WIDEVINE_CDM_AVAILABLE)
#define EXPECT_WV EXPECT_TRUE
#else
#define EXPECT_WV EXPECT_FALSE
#endif

#if defined(WIDEVINE_CDM_AVAILABLE) && \
    defined(WIDEVINE_CDM_CENC_SUPPORT_AVAILABLE)
#define EXPECT_WVISO EXPECT_TRUE
#else
#define EXPECT_WVISO EXPECT_FALSE
#endif

namespace webkit_media {

static const char* const kClearKey = "webkit-org.w3.clearkey";
static const char* const kExternalClearKey = "org.chromium.externalclearkey";
static const char* const kWidevineAlpha = "com.widevine.alpha";

class KeySystemsTest : public testing::Test {
 protected:
  KeySystemsTest() {
    vp8_codec_.push_back("vp8");

    vp80_codec_.push_back("vp8.0");

    vorbis_codec_.push_back("vorbis");

    vp8_and_vorbis_codecs_.push_back("vp8");
    vp8_and_vorbis_codecs_.push_back("vorbis");

    avc1_codec_.push_back("avc1");

    avc1_extended_codec_.push_back("avc1.4D400C");

    avc1_dot_codec_.push_back("avc1.");

    avc2_codec_.push_back("avc2");

    aac_codec_.push_back("mp4a");

    avc1_and_aac_codecs_.push_back("avc1");
    avc1_and_aac_codecs_.push_back("mp4a");

    unknown_codec_.push_back("foo");

    mixed_codecs_.push_back("vorbis");
    mixed_codecs_.push_back("avc1");
  }

  typedef std::vector<std::string> CodecVector;

  const CodecVector& no_codecs() const { return no_codecs_; }

  const CodecVector& vp8_codec() const { return vp8_codec_; }
  const CodecVector& vp80_codec() const { return vp80_codec_; }
  const CodecVector& vorbis_codec() const { return vorbis_codec_; }
  const CodecVector& vp8_and_vorbis_codecs() const {
    return vp8_and_vorbis_codecs_;
  }

  const CodecVector& avc1_codec() const { return avc1_codec_; }
  const CodecVector& avc1_extended_codec() const {
    return avc1_extended_codec_;
  }
  const CodecVector& avc1_dot_codec() const { return avc1_dot_codec_; }
  const CodecVector& avc2_codec() const { return avc2_codec_; }
  const CodecVector& aac_codec() const { return aac_codec_; }
  const CodecVector& avc1_and_aac_codecs() const {
    return avc1_and_aac_codecs_;
  }

  const CodecVector& unknown_codec() const { return unknown_codec_; }

  const CodecVector& mixed_codecs() const { return mixed_codecs_; }

 private:
  const CodecVector no_codecs_;

  CodecVector vp8_codec_;
  CodecVector vp80_codec_;
  CodecVector vorbis_codec_;
  CodecVector vp8_and_vorbis_codecs_;

  CodecVector avc1_codec_;
  CodecVector avc1_extended_codec_;
  CodecVector avc1_dot_codec_;
  CodecVector avc2_codec_;
  CodecVector aac_codec_;
  CodecVector avc1_and_aac_codecs_;

  CodecVector unknown_codec_;

  CodecVector mixed_codecs_;

};

TEST_F(KeySystemsTest, ClearKey_Basic) {
  EXPECT_TRUE(IsSupportedKeySystem(WebString::fromUTF8(kClearKey)));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kClearKey));

  // Not yet out from behind the vendor prefix.
  EXPECT_FALSE(IsSupportedKeySystem("org.w3.clearkey"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "org.w3.clearkey"));

  EXPECT_STREQ("ClearKey", KeySystemNameForUMA(std::string(kClearKey)).c_str());
  EXPECT_STREQ("ClearKey",
               KeySystemNameForUMA(WebString::fromUTF8(kClearKey)).c_str());

  EXPECT_TRUE(CanUseAesDecryptor(kClearKey));
  EXPECT_TRUE(GetPluginType(kClearKey).empty());  // Does not use plugin.
}

TEST_F(KeySystemsTest, ClearKey_Parent) {
  const char* const kClearKeyParent = "webkit-org.w3";

  // The parent should be supported but is not. See http://crbug.com/164303.
  EXPECT_FALSE(IsSupportedKeySystem(WebString::fromUTF8(kClearKeyParent)));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kClearKeyParent));

  // The parent is not supported for most things.
  EXPECT_STREQ("Unknown",
               KeySystemNameForUMA(std::string(kClearKeyParent)).c_str());
  EXPECT_STREQ("Unknown",
      KeySystemNameForUMA(WebString::fromUTF8(kClearKeyParent)).c_str());
  EXPECT_FALSE(CanUseAesDecryptor(kClearKeyParent));
  EXPECT_TRUE(GetPluginType(kClearKeyParent).empty());
}

TEST_F(KeySystemsTest, ClearKey_IsSupportedKeySystem_InvalidVariants) {
  // Case sensitive.
  EXPECT_FALSE(IsSupportedKeySystem("webkit-org.w3.ClEaRkEy"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "webkit-org.w3.ClEaRkEy"));

  // TLDs are not allowed.
  EXPECT_FALSE(IsSupportedKeySystem("webkit-org."));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "webkit-org."));
  EXPECT_FALSE(IsSupportedKeySystem("webkit-org"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "webkit-org"));
  EXPECT_FALSE(IsSupportedKeySystem("org."));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "org."));
  EXPECT_FALSE(IsSupportedKeySystem("org"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "org"));

  // Extra period.
  EXPECT_FALSE(IsSupportedKeySystem("webkit-org.w3."));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "webkit-org.w3."));

  // Incomplete.
  EXPECT_FALSE(IsSupportedKeySystem("webkit-org.w3.clearke"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "webkit-org.w3.clearke"));

  // Extra character.
  EXPECT_FALSE(IsSupportedKeySystem("webkit-org.w3.clearkeyz"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "webkit-org.w3.clearkeyz"));

  // There are no child key systems for Clear Key.
  EXPECT_FALSE(IsSupportedKeySystem("webkit-org.w3.clearkey.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "webkit-org.w3.clearkey.foo"));
}

TEST_F(KeySystemsTest, IsSupportedKeySystemWithMediaMimeType_ClearKey_NoType) {
  // These two should be true. See http://crbug.com/164303.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "", no_codecs(), kClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "", no_codecs(), "webkit-org.w3"));

  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "", no_codecs(), "webkit-org.w3.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "", no_codecs(), "webkit-org.w3.clearkey.foo"));
}

TEST_F(KeySystemsTest, IsSupportedKeySystemWithMediaMimeType_ClearKey_WebM) {
  // Valid video types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kClearKey));
  // The parent should be supported but is not. See http://crbug.com/164303.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "webkit-org.w3"));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_codec(), kClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp80_codec(), kClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_and_vorbis_codecs(), kClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vorbis_codec(), kClearKey));

  // Non-Webm codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", avc1_codec(), kClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", unknown_codec(), kClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", mixed_codecs(), kClearKey));

  // Valid audio types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", no_codecs(), kClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vorbis_codec(), kClearKey));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp8_codec(), kClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp8_and_vorbis_codecs(), kClearKey));

  // Non-Webm codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", aac_codec(), kClearKey));
}

TEST_F(KeySystemsTest, IsSupportedKeySystemWithMediaMimeType_ClearKey_MP4) {
  // Valid video types.
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", no_codecs(), kClearKey));
  // The parent should be supported but is not. See http://crbug.com/164303.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", no_codecs(), "webkit-org.w3"));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_codec(), kClearKey));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_and_aac_codecs(), kClearKey));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", aac_codec(), kClearKey));

  // Extended codecs fail because this is handled by SimpleWebMimeRegistryImpl.
  // They should really pass canPlayType().
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_extended_codec(), kClearKey));

  // Invalid codec format.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_dot_codec(), kClearKey));

  // Non-MP4 codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc2_codec(), kClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", vp8_codec(), kClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", unknown_codec(), kClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", mixed_codecs(), kClearKey));

  // Valid audio types.
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", no_codecs(), kClearKey));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", aac_codec(), kClearKey));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_codec(), kClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_and_aac_codecs(), kClearKey));

  // Non-MP4 codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", vorbis_codec(), kClearKey));
}

//
// External Clear Key
//

TEST_F(KeySystemsTest, ExternalClearKey_Basic) {
  EXPECT_TRUE(IsSupportedKeySystem(WebString::fromUTF8(kExternalClearKey)));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kExternalClearKey));

  // External Clear Key does not have a UMA name because it is for testing.
  EXPECT_STREQ(
      "Unknown",
      KeySystemNameForUMA(std::string(kExternalClearKey)).c_str());
  EXPECT_STREQ(
      "Unknown",
      KeySystemNameForUMA(WebString::fromUTF8(kExternalClearKey)).c_str());

  EXPECT_FALSE(CanUseAesDecryptor(kExternalClearKey));
  EXPECT_STREQ("application/x-ppapi-clearkey-cdm",
               GetPluginType(kExternalClearKey).c_str());
}

TEST_F(KeySystemsTest, ExternalClearKey_Parent) {
  const char* const kExternalClearKeyParent = "org.chromium";

  // The parent should be supported but is not. See http://crbug.com/164303.
  EXPECT_FALSE(
      IsSupportedKeySystem(WebString::fromUTF8(kExternalClearKeyParent)));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kExternalClearKeyParent));

  // The parent is not supported for most things.
  EXPECT_STREQ(
      "Unknown",
      KeySystemNameForUMA(std::string(kExternalClearKeyParent)).c_str());
  EXPECT_STREQ("Unknown",
               KeySystemNameForUMA(
                   WebString::fromUTF8(kExternalClearKeyParent)).c_str());
  EXPECT_FALSE(CanUseAesDecryptor(kExternalClearKeyParent));
  EXPECT_TRUE(GetPluginType(kExternalClearKeyParent).empty());
}

TEST_F(KeySystemsTest, ExternalClearKey_IsSupportedKeySystem_InvalidVariants) {
  // Case sensitive.
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium.ExTeRnAlClEaRkEy"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(),
      "org.chromium.ExTeRnAlClEaRkEy"));

  // TLDs are not allowed.
  EXPECT_FALSE(IsSupportedKeySystem("org."));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "org."));
  EXPECT_FALSE(IsSupportedKeySystem("org"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "org"));

  // Extra period.
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium."));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "org.chromium."));

  // Incomplete.
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium.externalclearke"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(),
      "org.chromium.externalclearke"));

  // Extra character.
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium.externalclearkeyz"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(),
      "org.chromium.externalclearkeyz"));

  // There are no child key systems for Clear Key.
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium.externalclearkey.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(),
      "org.chromium.externalclearkey.foo"));
}

TEST_F(KeySystemsTest,
       IsSupportedKeySystemWithMediaMimeType_ExternalClearKey_NoType) {
  // These two should be true. See http://crbug.com/164303.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "", no_codecs(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "", no_codecs(), "org.chromium"));

  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "", no_codecs(), "org.chromium.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "", no_codecs(), "org.chromium.externalclearkey.foo"));
}

TEST_F(KeySystemsTest,
       IsSupportedKeySystemWithMediaMimeType_ExternalClearKey_WebM) {
  // Valid video types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kExternalClearKey));
  // The parent should be supported but is not. See http://crbug.com/164303.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "org.chromium"));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_codec(), kExternalClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp80_codec(), kExternalClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_and_vorbis_codecs(), kExternalClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vorbis_codec(), kExternalClearKey));

  // Non-Webm codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", avc1_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", unknown_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", mixed_codecs(), kExternalClearKey));

  // Valid audio types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", no_codecs(), kExternalClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vorbis_codec(), kExternalClearKey));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp8_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp8_and_vorbis_codecs(), kExternalClearKey));

  // Non-Webm codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", aac_codec(), kExternalClearKey));
}

TEST_F(KeySystemsTest,
       IsSupportedKeySystemWithMediaMimeType_ExternalClearKey_MP4) {
  // Valid video types.
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", no_codecs(), kExternalClearKey));
  // The parent should be supported but is not. See http://crbug.com/164303.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", no_codecs(), "org.chromium"));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_codec(), kExternalClearKey));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_and_aac_codecs(), kExternalClearKey));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", aac_codec(), kExternalClearKey));

  // Extended codecs fail because this is handled by SimpleWebMimeRegistryImpl.
  // They should really pass canPlayType().
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_extended_codec(), kExternalClearKey));

  // Invalid codec format.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_dot_codec(), kExternalClearKey));

  // Non-MP4 codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc2_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", vp8_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", unknown_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", mixed_codecs(), kExternalClearKey));

  // Valid audio types.
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", no_codecs(), kExternalClearKey));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", aac_codec(), kExternalClearKey));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_and_aac_codecs(), kExternalClearKey));

  // Non-MP4 codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", vorbis_codec(), kExternalClearKey));
}

//
// Widevine
//

TEST_F(KeySystemsTest, Widevine_Basic) {
  EXPECT_WV(IsSupportedKeySystem(WebString::fromUTF8(kWidevineAlpha)));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kWidevineAlpha));

#if defined(WIDEVINE_CDM_AVAILABLE)
  const char* const kWidevineUmaName = "Widevine";
#else
  const char* const kWidevineUmaName = "Unknown";
#endif
  EXPECT_STREQ(
      kWidevineUmaName,
      KeySystemNameForUMA(std::string(kWidevineAlpha)).c_str());
  EXPECT_STREQ(
      kWidevineUmaName,
      KeySystemNameForUMA(WebString::fromUTF8(kWidevineAlpha)).c_str());

  EXPECT_FALSE(CanUseAesDecryptor(kWidevineAlpha));
#if defined(WIDEVINE_CDM_AVAILABLE)
  EXPECT_STREQ("application/x-ppapi-widevine-cdm",
               GetPluginType(kWidevineAlpha).c_str());
#else
  EXPECT_TRUE(GetPluginType(kWidevineAlpha).empty());
#endif
}

TEST_F(KeySystemsTest, Widevine_Parent) {
  const char* const kWidevineParent = "com.widevine";

  EXPECT_WV(IsSupportedKeySystem(WebString::fromUTF8(kWidevineParent)));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kWidevineParent));

  // The parent is not supported for most things.
  EXPECT_STREQ("Unknown",
               KeySystemNameForUMA(std::string(kWidevineParent)).c_str());
  EXPECT_STREQ("Unknown",
      KeySystemNameForUMA(WebString::fromUTF8(kWidevineParent)).c_str());
  EXPECT_FALSE(CanUseAesDecryptor(kWidevineParent));
  EXPECT_TRUE(GetPluginType(kWidevineParent).empty());
}

TEST_F(KeySystemsTest, Widevine_IsSupportedKeySystem_InvalidVariants) {
  // Case sensitive.
  EXPECT_FALSE(IsSupportedKeySystem("com.widevine.AlPhA"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "com.widevine.AlPhA"));

  // TLDs are not allowed.
  EXPECT_FALSE(IsSupportedKeySystem("com."));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "com."));
  EXPECT_FALSE(IsSupportedKeySystem("com"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "com"));

  // Extra period.
  EXPECT_FALSE(IsSupportedKeySystem("com.widevine."));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "com.widevine."));

  // Incomplete.
  EXPECT_FALSE(IsSupportedKeySystem("com.widevine.alph"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "com.widevine.alph"));

  // Extra character.
  EXPECT_FALSE(IsSupportedKeySystem("com.widevine.alphab"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "com.widevine.alphab"));

  // There are no child key systems for Widevine Alpha.
  EXPECT_FALSE(IsSupportedKeySystem("com.widevine.alpha.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "com.widevine.alpha.foo"));
}

TEST_F(KeySystemsTest, IsSupportedKeySystemWithMediaMimeType_Widevine_NoType) {
  // These two should be true. See http://crbug.com/164303.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "", no_codecs(), "com.widevine.alpha"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "", no_codecs(), "com.widevine"));

  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "", no_codecs(), "com.widevine.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "", no_codecs(), "com.widevine.alpha.foo"));
}

TEST_F(KeySystemsTest, IsSupportedKeySystemWithMediaMimeType_Widevine_WebM) {
  // Valid video types.
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kWidevineAlpha));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "com.widevine"));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_codec(), kWidevineAlpha));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp80_codec(), kWidevineAlpha));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_and_vorbis_codecs(), kWidevineAlpha));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vorbis_codec(), kWidevineAlpha));

  // Non-Webm codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", avc1_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", unknown_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", mixed_codecs(), kWidevineAlpha));

  // Valid audio types.
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", no_codecs(), kWidevineAlpha));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vorbis_codec(), kWidevineAlpha));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp8_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp8_and_vorbis_codecs(), kWidevineAlpha));

  // Non-Webm codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", aac_codec(), kWidevineAlpha));
}

TEST_F(KeySystemsTest, IsSupportedKeySystemWithMediaMimeType_Widevine_MP4) {
  // Valid video types.
  EXPECT_WVISO(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", no_codecs(), kWidevineAlpha));
  EXPECT_WVISO(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", no_codecs(), "com.widevine"));
  EXPECT_WVISO(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_codec(), kWidevineAlpha));
  EXPECT_WVISO(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_and_aac_codecs(), kWidevineAlpha));
  EXPECT_WVISO(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", aac_codec(), kWidevineAlpha));

  // Extended codecs fail because this is handled by SimpleWebMimeRegistryImpl.
  // They should really pass canPlayType().
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_extended_codec(), kWidevineAlpha));

  // Invalid codec format.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_dot_codec(), kWidevineAlpha));

  // Non-MP4 codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc2_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", vp8_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", unknown_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", mixed_codecs(), kWidevineAlpha));

  // Valid audio types.
  EXPECT_WVISO(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", no_codecs(), kWidevineAlpha));
  EXPECT_WVISO(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", aac_codec(), kWidevineAlpha));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_and_aac_codecs(), kWidevineAlpha));

  // Non-MP4 codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", vorbis_codec(), kWidevineAlpha));
}

}  // namespace webkit_media
