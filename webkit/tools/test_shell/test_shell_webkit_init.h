// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBKIT_INIT_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBKIT_INIT_H_

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "media/base/media.h"
#include "webkit/api/public/WebData.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebStorageArea.h"
#include "webkit/api/public/WebStorageNamespace.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webclipboard_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkitclient_impl.h"
#include "webkit/extensions/v8/gears_extension.h"
#include "webkit/extensions/v8/interval_extension.h"
#include "webkit/tools/test_shell/mock_webclipboard_impl.h"
#include "webkit/tools/test_shell/simple_appcache_system.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "v8/include/v8.h"

class TestShellWebKitInit : public webkit_glue::WebKitClientImpl {
 public:
  TestShellWebKitInit(bool layout_test_mode) {
    v8::V8::SetCounterFunction(StatsTable::FindLocation);

    WebKit::initialize(this);
    WebKit::setLayoutTestMode(layout_test_mode);
    WebKit::registerURLSchemeAsLocal(
        ASCIIToUTF16(webkit_glue::GetUIResourceProtocol()));
    WebKit::registerURLSchemeAsNoAccess(
        ASCIIToUTF16(webkit_glue::GetUIResourceProtocol()));
    WebKit::enableV8SingleThreadMode();
    WebKit::registerExtension(extensions_v8::GearsExtension::Get());
    WebKit::registerExtension(extensions_v8::IntervalExtension::Get());

    // Load libraries for media and enable the media player.
    FilePath module_path;
    if (PathService::Get(base::DIR_MODULE, &module_path) &&
        media::InitializeMediaLibrary(module_path)) {
      WebKit::enableMediaPlayer();
    }

    // Construct and initialize an appcache system for this scope.
    // A new empty temp directory is created to house any cached
    // content during the run. Upon exit that directory is deleted.
    if (appcache_dir_.CreateUniqueTempDir())
      SimpleAppCacheSystem::InitializeOnUIThread(appcache_dir_.path());
  }

  ~TestShellWebKitInit() {
    WebKit::shutdown();
  }

  virtual WebKit::WebMimeRegistry* mimeRegistry() {
    return &mime_registry_;
  }

  WebKit::WebClipboard* clipboard() {
    // Mock out clipboard calls in layout test mode so that tests don't mess
    // with each other's copies/pastes when running in parallel.
    if (TestShell::layout_test_mode()) {
      return &mock_clipboard_;
    } else {
      return &real_clipboard_;
    }
  }

  virtual WebKit::WebSandboxSupport* sandboxSupport() {
    return NULL;
  }

  virtual bool sandboxEnabled() {
    return false;
  }

  virtual bool getFileSize(const WebKit::WebString& path, long long& result) {
    return file_util::GetFileSize(
        FilePath(webkit_glue::WebStringToFilePathString(path)),
                 reinterpret_cast<int64*>(&result));
  }

  virtual unsigned long long visitedLinkHash(const char* canonicalURL,
                                             size_t length) {
    return 0;
  }

  virtual bool isLinkVisited(unsigned long long linkHash) {
    return false;
  }

  virtual WebKit::WebMessagePortChannel* createMessagePortChannel() {
    return NULL;
  }

  virtual void setCookies(const WebKit::WebURL& url,
                          const WebKit::WebURL& first_party_for_cookies,
                          const WebKit::WebString& value) {
    SimpleResourceLoaderBridge::SetCookie(
        url, first_party_for_cookies, value.utf8());
  }

  virtual WebKit::WebString cookies(
      const WebKit::WebURL& url,
      const WebKit::WebURL& first_party_for_cookies) {
    return WebKit::WebString::fromUTF8(SimpleResourceLoaderBridge::GetCookies(
        url, first_party_for_cookies));
  }

  virtual void prefetchHostName(const WebKit::WebString&) {
  }

  virtual WebKit::WebData loadResource(const char* name) {
    if (!strcmp(name, "deleteButton")) {
      // Create a red 30x30 square.
      const char red_square[] =
          "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52"
          "\x00\x00\x00\x1e\x00\x00\x00\x1e\x04\x03\x00\x00\x00\xc9\x1e\xb3"
          "\x91\x00\x00\x00\x30\x50\x4c\x54\x45\x00\x00\x00\x80\x00\x00\x00"
          "\x80\x00\x80\x80\x00\x00\x00\x80\x80\x00\x80\x00\x80\x80\x80\x80"
          "\x80\xc0\xc0\xc0\xff\x00\x00\x00\xff\x00\xff\xff\x00\x00\x00\xff"
          "\xff\x00\xff\x00\xff\xff\xff\xff\xff\x7b\x1f\xb1\xc4\x00\x00\x00"
          "\x09\x70\x48\x59\x73\x00\x00\x0b\x13\x00\x00\x0b\x13\x01\x00\x9a"
          "\x9c\x18\x00\x00\x00\x17\x49\x44\x41\x54\x78\x01\x63\x98\x89\x0a"
          "\x18\x50\xb9\x33\x47\xf9\xa8\x01\x32\xd4\xc2\x03\x00\x33\x84\x0d"
          "\x02\x3a\x91\xeb\xa5\x00\x00\x00\x00\x49\x45\x4e\x44\xae\x42\x60"
          "\x82";
      return WebKit::WebData(red_square, arraysize(red_square));
    }
    return webkit_glue::WebKitClientImpl::loadResource(name);
  }

  virtual WebKit::WebString defaultLocale() {
    return ASCIIToUTF16("en-US");
  }

  virtual WebKit::WebStorageNamespace* createLocalStorageNamespace(
      const WebKit::WebString& path) {
    return WebKit::WebStorageNamespace::createLocalStorageNamespace(path);
  }

  virtual WebKit::WebStorageNamespace* createSessionStorageNamespace() {
    return WebKit::WebStorageNamespace::createSessionStorageNamespace();
  }

  virtual WebKit::WebApplicationCacheHost* createApplicationCacheHost(
        WebKit::WebApplicationCacheHostClient* client) {
    return SimpleAppCacheSystem::CreateApplicationCacheHost(client);
  }

 private:
  webkit_glue::SimpleWebMimeRegistryImpl mime_registry_;
  MockWebClipboardImpl mock_clipboard_;
  webkit_glue::WebClipboardImpl real_clipboard_;
  ScopedTempDir appcache_dir_;
  SimpleAppCacheSystem appcache_system_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBKIT_INIT_H_
