// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIDEVINE_CDM_WIDEVINE_CDM_COMMON_H_
#define WIDEVINE_CDM_WIDEVINE_CDM_COMMON_H_

#include "base/files/file_path.h"

// This file defines constants common to all Widevine CDM versions.

// "alpha" is a temporary name until a convention is defined.
const char kWidevineKeySystem[] = "com.widevine.alpha";

const char kWidevineCdmPluginName[] = "Widevine Content Decryption Module";
// Will be parsed as HTML.
const char kWidevineCdmPluginDescription[] =
    "Enables Widevine licenses for playback of HTML audio/video content.";
const char kWidevineCdmPluginMimeType[] ="application/x-ppapi-widevine-cdm";
const char kWidevineCdmPluginMimeTypeDescription[] =
    "Widevine Content Decryption Module";

// File name of the plugin on different platforms.
const base::FilePath::CharType kWidevineCdmPluginFileName[] =
#if defined(OS_MACOSX)
    FILE_PATH_LITERAL("widevinecdmadapter.plugin");
#elif defined(OS_WIN)
    FILE_PATH_LITERAL("widevinecdmadapter.dll");
#else  // OS_LINUX, etc.
    FILE_PATH_LITERAL("libwidevinecdmadapter.so");
#endif

#endif  // WIDEVINE_CDM_WIDEVINE_CDM_COMMON_H_
