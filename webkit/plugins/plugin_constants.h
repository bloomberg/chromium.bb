// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PLUGIN_CONSTANTS_H_
#define WEBKIT_PLUGINS_PLUGIN_CONSTANTS_H_

#include "base/basictypes.h"
#include "webkit/plugins/webkit_plugins_export.h"

WEBKIT_PLUGINS_EXPORT extern const char kFlashPluginName[];
WEBKIT_PLUGINS_EXPORT extern const char kFlashPluginSwfMimeType[];
WEBKIT_PLUGINS_EXPORT extern const char kFlashPluginSwfExtension[];
WEBKIT_PLUGINS_EXPORT extern const char kFlashPluginSwfDescription[];
WEBKIT_PLUGINS_EXPORT extern const char kFlashPluginSplMimeType[];
WEBKIT_PLUGINS_EXPORT extern const char kFlashPluginSplExtension[];
WEBKIT_PLUGINS_EXPORT extern const char kFlashPluginSplDescription[];

// The maximum plugin width and height.
WEBKIT_PLUGINS_EXPORT extern const uint16 kMaxPluginSideLength;
// The maximum plugin size, defined as the number of pixels occupied by the
// plugin.
WEBKIT_PLUGINS_EXPORT extern const uint32 kMaxPluginSize;

#endif  // WEBKIT_PLUGINS_PLUGIN_CONSTANTS_H_
