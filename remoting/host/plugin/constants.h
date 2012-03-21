// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PLUGIN_CONSTANTS_H_
#define REMOTING_HOST_PLUGIN_CONSTANTS_H_

// Warning: If you modify any macro in this file, make sure to modify
// the following files too:
//   - remoting/remoting.gyp
//   - remoting/chromium_branding
//   - remoting/google_chrome_branding
//   - remoting/host/plugin/host_plugin.ver

#define HOST_PLUGIN_DESCRIPTION \
    "Allow another user to access your computer securely over the Internet."
#define HOST_PLUGIN_MIME_TYPE "application/vnd.chromium.remoting-host"

#if defined(GOOGLE_CHROME_BUILD)
#define HOST_PLUGIN_NAME "Chrome Remote Desktop Host"
#else
#define HOST_PLUGIN_NAME "Chromoting Host"
#endif  // defined(GOOGLE_CHROME_BUILD)

#endif  // REMOTING_HOST_PLUGIN_CONSTANTS_H_
