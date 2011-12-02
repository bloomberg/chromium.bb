// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_PLUGIN_CONSTANTS_WIN_H_
#define WEBKIT_PLUGINS_NPAPI_PLUGIN_CONSTANTS_WIN_H_

#include "base/string16.h"
#include "webkit/plugins/webkit_plugins_export.h"

namespace webkit {
namespace npapi {

// The window class name for a plugin window.
extern const char16 kNativeWindowClassName[];

// If property is non-zero window reparenting must add UIPI message filters.
WEBKIT_PLUGINS_EXPORT extern const char16 kNativeWindowClassFilterProp[];

// The name of the window class name for the wrapper HWND around the actual
// plugin window that's used when running in multi-process mode.  This window
// is created on the browser UI thread.
WEBKIT_PLUGINS_EXPORT extern const char16 kWrapperNativeWindowClassName[];

// The name of the custom window message that the browser uses to tell the
// plugin process to paint a window.
WEBKIT_PLUGINS_EXPORT extern const char16 kPaintMessageName[];

// The name of the registry key which NPAPI plugins update on installation.
WEBKIT_PLUGINS_EXPORT extern const char16 kRegistryMozillaPlugins[];

extern const char16 kMozillaActiveXPlugin[];
WEBKIT_PLUGINS_EXPORT extern const char16 kNewWMPPlugin[];
extern const char16 kOldWMPPlugin[];
extern const char16 kYahooApplicationStatePlugin[];
extern const char16 kWanWangProtocolHandlerPlugin[];
extern const char16 kBuiltinFlashPlugin[];
extern const char16 kFlashPlugin[];
extern const char16 kAcrobatReaderPlugin[];
extern const char16 kRealPlayerPlugin[];
extern const char16 kSilverlightPlugin[];
extern const char16 kJavaPlugin1[];
extern const char16 kJavaPlugin2[];

extern const char kGPUPluginMimeType[];

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_PLUGIN_PLUGIN_LIST_H_
