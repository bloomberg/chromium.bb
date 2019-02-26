// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_RENDERER_ANDROID_KEY_SYSTEMS_H_
#define COMPONENTS_CDM_RENDERER_ANDROID_KEY_SYSTEMS_H_

#include <memory>
#include <vector>

#include "components/cdm/common/cdm_messages_android.h"
#include "media/base/key_system_properties.h"
#include "third_party/widevine/cdm/buildflags.h"

namespace cdm {

#if BUILDFLAG(ENABLE_WIDEVINE)
void AddAndroidWidevine(
    std::vector<std::unique_ptr<media::KeySystemProperties>>*
        concrete_key_systems);
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

// Add platform-supported key systems which are not explicitly handled
// by Chrome.
void AddAndroidPlatformKeySystems(
    std::vector<std::unique_ptr<media::KeySystemProperties>>*
        concrete_key_systems);

// Query key system property in browser process.
SupportedKeySystemResponse QueryKeySystemSupport(const std::string& key_system);

}  // namespace cdm

#endif  // COMPONENTS_CDM_RENDERER_ANDROID_KEY_SYSTEMS_H_
