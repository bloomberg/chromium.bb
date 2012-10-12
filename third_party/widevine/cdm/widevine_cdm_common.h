// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIDEVINE_CDM_COMMON_H_INCLUDED_
#define WIDEVINE_CDM_COMMON_H_INCLUDED_

#include "base/file_path.h"

// This file defines constants common to all Widevine CDM versions.

// "alpha" is a temporary name until a convention is defined.
const char kWidevineKeySystem[] = "com.widevine.alpha";

const char kWidevineCdmPluginName[] = "Widevine HTML CDM";
// Will be parsed as HTML.
const char kWidevineCdmPluginDescription[] =
    "This plugin enables Widevine licenses for playback of HTML audio/video "
    "content.";
const char kWidevineCdmPluginMimeType[] ="application/x-ppapi-widevine-cdm";
const char kWidevineCdmPluginMimeTypeDescription[] = "Widevine HTML CDM";

// File name of the plugin on different platforms.
const FilePath::CharType kWidevineCdmPluginFileName[] =
#if defined(OS_MACOSX)
    FILE_PATH_LITERAL("widevinecdmplugin.plugin");
#elif defined(OS_WIN)
    FILE_PATH_LITERAL("widevinecdmplugin.dll");
#else  // OS_LINUX, etc.
    FILE_PATH_LITERAL("libwidevinecdmplugin.so");
#endif

#endif  // WIDEVINE_CDM_COMMON_H_INCLUDED_
