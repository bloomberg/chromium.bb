// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_WEBIT_SUPPORT_H_
#define WEBKIT_SUPPORT_WEBIT_SUPPORT_H_

namespace WebKit {
class WebFrame;
class WebKitClient;
class WebMediaPlayer;
class WebMediaPlayerClient;
class WebPlugin;
struct WebPluginParams;
}

// This package provides functions used by DumpRenderTree/chromium.
// DumpRenderTree/chromium uses some code in webkit/ of Chromium. In
// order to minimize the dependency from WebKit to Chromium, the
// following functions uses WebKit API classes as possible and hide
// implementation classes.
namespace webkit_support {

// Initializes or terminates a test environment.
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

// The following functions are used by LayoutTestController.
void SetDatabaseQuota(int quota);
void ClearAllDatabases();
void SetAcceptAllCookies(bool accept);

}  // namespace webkit_support

#endif  // WEBKIT_SUPPORT_WEBIT_CLIENT_IMPL_H_
