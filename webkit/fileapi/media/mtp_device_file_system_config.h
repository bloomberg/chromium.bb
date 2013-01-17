// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_MEDIA_MTP_DEVICE_FILE_SYSTEM_CONFIG_H_
#define WEBKIT_FILEAPI_MEDIA_MTP_DEVICE_FILE_SYSTEM_CONFIG_H_

#include "build/build_config.h"

// Support MTP device file system for Windows, Linux and ChromeOS. Note that
// OS_LINUX implies OS_CHROMEOS.
#if defined(OS_WIN) || defined(OS_LINUX)
#define SUPPORT_MTP_DEVICE_FILESYSTEM
#endif

#endif  // WEBKIT_FILEAPI_MEDIA_MTP_DEVICE_FILE_SYSTEM_CONFIG_H_
