// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_WEBIT_SUPPORT_H_
#define WEBKIT_SUPPORT_WEBIT_SUPPORT_H_

#include <string>
#include <vector>

#include "app/keyboard_codes.h"
#include "base/basictypes.h"
#include "base/string16.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsAgentClient.h"

class WebURLLoaderMockFactory;
namespace WebKit {
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebCString;
class WebFrame;
class WebKitClient;
class WebMediaPlayer;
class WebMediaPlayerClient;
class WebPlugin;
class WebString;
class WebThemeEngine;
class WebURL;
class WebURLRequest;
class WebURLResponse;
struct WebPluginParams;
struct WebURLError;
}

// This package provides functions used by DumpRenderTree/chromium.
// DumpRenderTree/chromium uses some code in webkit/ of Chromium. In
// order to minimize the dependency from WebKit to Chromium, the
// following functions uses WebKit API classes as possible and hide
// implementation classes.
namespace webkit_support {

// Initializes or terminates a test environment.
// |unit_test_mode| should be set to true when running in a TestSuite, in which
// case no AtExitManager is created and ICU is not initialized (as it is already
// done by the TestSuite).
// SetUpTestEnvironment() and SetUpTestEnvironmentForUnitTests() calls
// WebKit::initialize().
// TearDownTestEnvironment() calls WebKit::shutdown().
// SetUpTestEnvironmentForUnitTests() should be used when running in a
// TestSuite, in which case no AtExitManager is created and ICU is not
// initialized (as it is already done by the TestSuite).
void SetUpTestEnvironment();
void SetUpTestEnvironmentForUnitTests();
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

// This is used by WebFrameClient::createApplicationCacheHost().
WebKit::WebApplicationCacheHost* CreateApplicationCacheHost(
    WebKit::WebFrame* frame, WebKit::WebApplicationCacheHostClient* client);

// Returns the root directory of the WebKit code.
WebKit::WebString GetWebKitRootDir();

// ------- URL load mocking.
// Registers the file at |file_path| to be served when |url| is requested.
// |response| is the response provided with the contents.
void RegisterMockedURL(const WebKit::WebURL& url,
                       const WebKit::WebURLResponse& response,
                       const WebKit::WebString& file_path);

// Unregisters URLs so they are no longer mocked.
void UnregisterMockedURL(const WebKit::WebURL& url);
void UnregisterAllMockedURLs();

// Causes all pending asynchronous requests to be served.  When this method
// returns all the pending requests have been processed.
void ServeAsynchronousMockedRequests();

// Wrappers to minimize dependecy.

// -------- Debugging
bool BeingDebugged();

// -------- Message loop and task
void RunMessageLoop();
void QuitMessageLoop();
void RunAllPendingMessages();
void DispatchMessageLoop();
WebKit::WebDevToolsAgentClient::WebKitClientMessageLoop*
    CreateDevToolsMessageLoop();
void PostDelayedTask(void (*func)(void*), void* context, int64 delay_ms);

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

// Set the directory of specified file: URL as the current working directory.
bool SetCurrentDirectoryForFileURL(const WebKit::WebURL& fileUrl);

// -------- Time
int64 GetCurrentTimeInMillisecond();

// -------- Net
// A wrapper of net::EscapePath().
std::string EscapePath(const std::string& path);
// Make an error description for layout tests.
std::string MakeURLErrorDescription(const WebKit::WebURLError& error);
// Creates WebURLError for an aborted request.
WebKit::WebURLError CreateCancelledError(const WebKit::WebURLRequest& request);

// - Database
void SetDatabaseQuota(int quota);
void ClearAllDatabases();

// - Resource loader
void SetAcceptAllCookies(bool accept);

// - Theme engine
#if defined(OS_WIN)
void SetThemeEngine(WebKit::WebThemeEngine* engine);
WebKit::WebThemeEngine* GetThemeEngine();
#endif

// - DevTools
WebKit::WebCString GetDevToolsDebuggerScriptSource();
WebKit::WebURL GetDevToolsPathAsURL();

// -------- Keyboard code
enum {
    VKEY_LEFT = app::VKEY_LEFT,
    VKEY_RIGHT = app::VKEY_RIGHT,
    VKEY_UP = app::VKEY_UP,
    VKEY_DOWN = app::VKEY_DOWN,
    VKEY_RETURN = app::VKEY_RETURN,
    VKEY_INSERT = app::VKEY_INSERT,
    VKEY_DELETE = app::VKEY_DELETE,
    VKEY_PRIOR = app::VKEY_PRIOR,
    VKEY_NEXT = app::VKEY_NEXT,
    VKEY_HOME = app::VKEY_HOME,
    VKEY_END = app::VKEY_END,
    VKEY_SNAPSHOT = app::VKEY_SNAPSHOT,
    VKEY_F1 = app::VKEY_F1,
};

}  // namespace webkit_support

#endif  // WEBKIT_SUPPORT_WEBIT_CLIENT_IMPL_H_
