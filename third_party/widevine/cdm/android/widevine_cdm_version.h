// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIDEVINE_CDM_VERSION_H_
#define WIDEVINE_CDM_VERSION_H_

#include "third_party/widevine/cdm/widevine_cdm_common.h"

// Indicates that the Widevine CDM is available.
#define WIDEVINE_CDM_AVAILABLE

// TODO(ddorwin): Remove when we have CDM availability detection
// (http://crbug.com/224793).
#define DISABLE_WIDEVINE_CDM_CANPLAYTYPE

// Indicates that ISO BMFF CENC support is available in the Widevine CDM.
// Must be enabled if any of the codecs below are enabled.
#define WIDEVINE_CDM_CENC_SUPPORT_AVAILABLE
// Indicates that AVC1 decoding is available for ISO BMFF CENC.
#define WIDEVINE_CDM_AVC1_SUPPORT_AVAILABLE
// Indicates that AAC decoding is available for ISO BMFF CENC.
#define WIDEVINE_CDM_AAC_SUPPORT_AVAILABLE

#endif  // WIDEVINE_CDM_VERSION_H_
