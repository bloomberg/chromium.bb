// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_WEBIT_SUPPORT_H_
#define WEBKIT_SUPPORT_WEBIT_SUPPORT_H_

#include <string>

#include "base/basictypes.h"

class Task;
namespace WebKit {
class WebFrame;
class WebKitClient;
class WebMediaPlayer;
class WebMediaPlayerClient;
class WebPlugin;
struct WebPluginParams;
class WebString;
class WebURL;
}

// This package provides functions used by DumpRenderTree/chromium.
// DumpRenderTree/chromium uses some code in webkit/ of Chromium. In
// order to minimize the dependency from WebKit to Chromium, the
// following functions uses WebKit API classes as possible and hide
// implementation classes.
namespace webkit_support {

// Initializes or terminates a test environment.
// SetUpTestEnvironment() calls WebKit::initialize().
// TearDownTestEnvironment() calls WebKit::shutdown().
void SetUpTestEnvironment();
void TearDownTestEnvironment();

// Returns a pointer to a WebKitClient implementation for DumpRenderTree.
// Needs to call SetUpTestEnvironment() before this.
// This returns a pointer to a static instance.  Don't delete it.
WebKit::WebKitClient* GetWebKitClient();

// This is used by WebFrameClient::createPlugin().
WebKit::WebPlugin* CreateWebPlugin(WebKit::WebFrame* frame,
                                   const WebKit::WebPluginParams& params);

// This is used by WebFrameClient::createMediaPlayer().
WebKit::WebMediaPlayer* CreateMediaPlayer(WebKit::WebFrame* frame,
                                          WebKit::WebMediaPlayerClient* client);

// Wrappers to minimize dependecy.

// -------- Debugging
bool BeingDebugged();

// -------- Message loop and task
void RunMessageLoop();
void QuitMessageLoop();
void RunAllPendingMessages();
void DispatchMessageLoop();
void PostTaskFromHere(Task* task);  // TODO(tkent): Eliminate Task.
void PostDelayedTaskFromHere(Task* task, int64 delay_ms);  // ditto.

// -------- File path and PathService
// Converts the specified path string to an absolute path in WebString.
// |utf8_path| is in UTF-8 encoding, not native multibyte string.
WebKit::WebString GetAbsoluteWebStringFromUTF8Path(
    const std::string& utf8_path);

// Create a WebURL from the specified string.
// If |path_or_url_in_nativemb| is a URL starting with scheme, this simply
// returns a WebURL for it.  Otherwise, this returns a file:// URL.
WebKit::WebURL CreateURLForPathOrURL(
    const std::string& path_or_url_in_nativemb);

// Converts file:///tmp/LayoutTests URLs to the actual location on disk.
WebKit::WebURL RewriteLayoutTestsURL(const std::string& utf8_url);

// - Database
void SetDatabaseQuota(int quota);
void ClearAllDatabases();

// - Resource loader
void SetAcceptAllCookies(bool accept);

}  // namespace webkit_support

#endif  // WEBKIT_SUPPORT_WEBIT_CLIENT_IMPL_H_
