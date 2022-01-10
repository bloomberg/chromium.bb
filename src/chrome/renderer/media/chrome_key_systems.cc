// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_key_systems.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "components/cdm/renderer/external_clear_key_key_system_properties.h"
#include "components/cdm/renderer/widevine_key_system_properties.h"
#include "content/public/renderer/render_thread.h"
#include "media/base/decrypt_config.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_properties.h"
#include "media/cdm/cdm_capability.h"
#include "media/media_buildflags.h"
#include "third_party/widevine/cdm/buildflags.h"

#if defined(OS_ANDROID)
#include "components/cdm/renderer/android_key_systems.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "base/feature_list.h"
#include "content/public/renderer/key_system_support.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#if BUILDFLAG(ENABLE_WIDEVINE)
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
// TODO(crbug.com/663554): Needed for WIDEVINE_CDM_MIN_GLIBC_VERSION.
// component updated CDM on all desktop platforms and remove this.
#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR. // nogncheck
#if BUILDFLAG(ENABLE_PLATFORM_HEVC) && BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) && BUILDFLAG(IS_CHROMEOS_ASH)
// The following must be after widevine_cdm_version.h.
#if defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
#include <gnu/libc-version.h>
#include "base/version.h"
#endif  // defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
#endif  // BUILDFLAG(ENABLE_WIDEVINE)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

using media::EmeFeatureSupport;
using media::EmeSessionTypeSupport;
using media::KeySystemProperties;
using media::SupportedCodecs;

namespace {

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

// External Clear Key (used for testing).
void AddExternalClearKey(
    std::vector<std::unique_ptr<KeySystemProperties>>* key_systems) {
  static const char kExternalClearKeyKeySystem[] =
      "org.chromium.externalclearkey";

  // TODO(xhwang): Actually use `capability` to determine capabilities.
  media::mojom::KeySystemCapabilityPtr capability;
  if (!content::IsKeySystemSupported(kExternalClearKeyKeySystem, &capability)) {
    DVLOG(1) << "External Clear Key not supported";
    return;
  }

  key_systems->push_back(std::make_unique<cdm::ExternalClearKeyProperties>());
}

#if BUILDFLAG(ENABLE_WIDEVINE)
SupportedCodecs GetVP9Codecs(
    const std::vector<media::VideoCodecProfile>& profiles) {
  if (profiles.empty()) {
    // If no profiles are specified, then all are supported.
    return media::EME_CODEC_VP9_PROFILE0 | media::EME_CODEC_VP9_PROFILE2;
  }

  SupportedCodecs supported_vp9_codecs = media::EME_CODEC_NONE;
  for (const auto& profile : profiles) {
    switch (profile) {
      case media::VP9PROFILE_PROFILE0:
        supported_vp9_codecs |= media::EME_CODEC_VP9_PROFILE0;
        break;
      case media::VP9PROFILE_PROFILE2:
        supported_vp9_codecs |= media::EME_CODEC_VP9_PROFILE2;
        break;
      default:
        DVLOG(1) << "Unexpected " << GetCodecName(media::VideoCodec::kVP9)
                 << " profile: " << GetProfileName(profile);
        break;
    }
  }

  return supported_vp9_codecs;
}

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
SupportedCodecs GetHevcCodecs(
    const std::vector<media::VideoCodecProfile>& profiles) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLacrosEnablePlatformHevc)) {
    return media::EME_CODEC_NONE;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // If no profiles are specified, then all are supported.
  if (profiles.empty()) {
    return media::EME_CODEC_HEVC_PROFILE_MAIN |
           media::EME_CODEC_HEVC_PROFILE_MAIN10;
  }

  SupportedCodecs supported_hevc_codecs = media::EME_CODEC_NONE;
  for (const auto& profile : profiles) {
    switch (profile) {
      case media::HEVCPROFILE_MAIN:
        supported_hevc_codecs |= media::EME_CODEC_HEVC_PROFILE_MAIN;
        break;
      case media::HEVCPROFILE_MAIN10:
        supported_hevc_codecs |= media::EME_CODEC_HEVC_PROFILE_MAIN10;
        break;
      default:
        DVLOG(1) << "Unexpected " << GetCodecName(media::VideoCodec::kHEVC)
                 << " profile: " << GetProfileName(profile);
        break;
    }
  }

  return supported_hevc_codecs;
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)

SupportedCodecs GetSupportedCodecs(const media::CdmCapability& capability) {
  SupportedCodecs supported_codecs = media::EME_CODEC_NONE;

  for (const auto& codec : capability.audio_codecs) {
    switch (codec) {
      case media::AudioCodec::kOpus:
        supported_codecs |= media::EME_CODEC_OPUS;
        break;
      case media::AudioCodec::kVorbis:
        supported_codecs |= media::EME_CODEC_VORBIS;
        break;
      case media::AudioCodec::kFLAC:
        supported_codecs |= media::EME_CODEC_FLAC;
        break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      case media::AudioCodec::kAAC:
        supported_codecs |= media::EME_CODEC_AAC;
        break;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
      default:
        DVLOG(1) << "Unexpected supported codec: " << GetCodecName(codec);
        break;
    }
  }

  // For compatibility with older CDMs different profiles are only used
  // with some video codecs.
  for (const auto& codec : capability.video_codecs) {
    switch (codec.first) {
      case media::VideoCodec::kVP8:
        supported_codecs |= media::EME_CODEC_VP8;
        break;
      case media::VideoCodec::kVP9:
        supported_codecs |= GetVP9Codecs(codec.second);
        break;
      case media::VideoCodec::kAV1:
        supported_codecs |= media::EME_CODEC_AV1;
        break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      case media::VideoCodec::kH264:
        supported_codecs |= media::EME_CODEC_AVC1;
        break;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
      case media::VideoCodec::kHEVC:
        supported_codecs |= GetHevcCodecs(codec.second);
        break;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
      default:
        DVLOG(1) << "Unexpected supported codec: " << GetCodecName(codec.first);
        break;
    }
  }

  return supported_codecs;
}

// Returns persistent-license session support.
EmeSessionTypeSupport GetPersistentLicenseSupport(bool supported_by_the_cdm) {
  // Do not support persistent-license if the process cannot persist data.
  // TODO(crbug.com/457487): Have a better plan on this. See bug for details.
  if (ChromeRenderThreadObserver::is_incognito_process()) {
    DVLOG(2) << __func__ << ": Not supported in incognito process.";
    return EmeSessionTypeSupport::NOT_SUPPORTED;
  }

  if (!supported_by_the_cdm) {
    DVLOG(2) << __func__ << ": Not supported by the CDM.";
    return EmeSessionTypeSupport::NOT_SUPPORTED;
  }

// On ChromeOS, platform verification is similar to CDM host verification.
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION) || defined(OS_CHROMEOS)
  bool cdm_host_verification_potentially_supported = true;
#else
  bool cdm_host_verification_potentially_supported = false;
#endif

  // If we are sure CDM host verification is NOT supported, we should not
  // support persistent-license.
  if (!cdm_host_verification_potentially_supported) {
    DVLOG(2) << __func__ << ": Not supported without CDM host verification.";
    return EmeSessionTypeSupport::NOT_SUPPORTED;
  }

#if defined(OS_CHROMEOS)
  // On ChromeOS, platform verification (similar to CDM host verification)
  // requires identifier to be allowed.
  // TODO(jrummell): Currently the ChromeOS CDM does not require storage ID
  // to support persistent license. Update this logic when the new CDM requires
  // storage ID.
  return EmeSessionTypeSupport::SUPPORTED_WITH_IDENTIFIER;
#elif BUILDFLAG(ENABLE_CDM_STORAGE_ID)
  // On other platforms, we require storage ID to support persistent license.
  return EmeSessionTypeSupport::SUPPORTED;
#else
  // Storage ID not implemented, so no support for persistent license.
  DVLOG(2) << __func__ << ": Not supported without CDM storage ID.";
  return EmeSessionTypeSupport::NOT_SUPPORTED;
#endif  // defined(OS_CHROMEOS)
}

void AddWidevine(
    std::vector<std::unique_ptr<KeySystemProperties>>* key_systems) {
#if defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
  base::Version glibc_version(gnu_get_libc_version());
  DCHECK(glibc_version.IsValid());
  if (glibc_version < base::Version(WIDEVINE_CDM_MIN_GLIBC_VERSION))
    return;
#endif  // defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)

  media::mojom::KeySystemCapabilityPtr capability;
  if (!content::IsKeySystemSupported(kWidevineKeySystem, &capability)) {
    DVLOG(1) << "Widevine CDM is not currently available.";
    return;
  }

  // Codecs and encryption schemes.
  SupportedCodecs codecs = media::EME_CODEC_NONE;
  SupportedCodecs hw_secure_codecs = media::EME_CODEC_NONE;
  base::flat_set<::media::EncryptionScheme> encryption_schemes;
  base::flat_set<::media::EncryptionScheme> hw_secure_encryption_schemes;
  bool cdm_supports_persistent_license = false;

  if (capability->sw_secure_capability) {
    codecs = GetSupportedCodecs(capability->sw_secure_capability.value());
    encryption_schemes = capability->sw_secure_capability->encryption_schemes;
    if (!base::Contains(capability->sw_secure_capability->session_types,
                        media::CdmSessionType::kTemporary)) {
      DVLOG(1) << "Temporary sessions must be supported.";
      return;
    }

    cdm_supports_persistent_license =
        base::Contains(capability->sw_secure_capability->session_types,
                       media::CdmSessionType::kPersistentLicense);
  }

  if (capability->hw_secure_capability) {
    hw_secure_codecs =
        GetSupportedCodecs(capability->hw_secure_capability.value());
    hw_secure_encryption_schemes =
        capability->hw_secure_capability->encryption_schemes;
    if (!base::Contains(capability->hw_secure_capability->session_types,
                        media::CdmSessionType::kTemporary)) {
      DVLOG(1) << "Temporary sessions must be supported.";
      return;
    }

    // TODO(b/186035558): With a single flag we can't distinguish persistent
    // session support between software and hardware CDMs. This should be
    // fixed so that if there is both a software and a hardware CDM, persistent
    // session support can be different between the versions.
  }

  // Robustness.
  using Robustness = cdm::WidevineKeySystemProperties::Robustness;
  auto max_audio_robustness = Robustness::SW_SECURE_CRYPTO;
  auto max_video_robustness = Robustness::SW_SECURE_DECODE;

#if defined(OS_CHROMEOS)
  // On ChromeOS, we support HW_SECURE_ALL even without hardware secure codecs.
  // See WidevineKeySystemProperties::GetRobustnessConfigRule().
  max_audio_robustness = Robustness::HW_SECURE_ALL;
  max_video_robustness = Robustness::HW_SECURE_ALL;
#else
  if (media::IsHardwareSecureDecryptionEnabled()) {
    max_audio_robustness = Robustness::HW_SECURE_CRYPTO;
    max_video_robustness = Robustness::HW_SECURE_ALL;
  }
#endif

  auto persistent_license_support =
      GetPersistentLicenseSupport(cdm_supports_persistent_license);

  // Others.
  auto persistent_state_support = EmeFeatureSupport::REQUESTABLE;
  auto distinctive_identifier_support = EmeFeatureSupport::NOT_SUPPORTED;
#if defined(OS_CHROMEOS) || defined(OS_WIN)
  distinctive_identifier_support = EmeFeatureSupport::REQUESTABLE;
#endif

  key_systems->emplace_back(new cdm::WidevineKeySystemProperties(
      codecs, encryption_schemes, hw_secure_codecs,
      hw_secure_encryption_schemes, max_audio_robustness, max_video_robustness,
      persistent_license_support, persistent_state_support,
      distinctive_identifier_support));
}
#endif  // BUILDFLAG(ENABLE_WIDEVINE)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

}  // namespace

void AddChromeKeySystems(
    std::vector<std::unique_ptr<KeySystemProperties>>* key_systems) {
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  if (base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting))
    AddExternalClearKey(key_systems);

#if BUILDFLAG(ENABLE_WIDEVINE)
  AddWidevine(key_systems);
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if defined(OS_ANDROID)
  cdm::AddAndroidWidevine(key_systems);
#endif  // defined(OS_ANDROID)
}
