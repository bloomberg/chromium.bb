// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "net/base/cookie_monster.h"
#include "net/http/http_cache.h"
#include "net/socket/ssl_test_util.h"
#include "media/base/media.h"
#include "third_party/WebKit/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDatabase.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptController.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/WebKit/chromium/public/WebStorageArea.h"
#include "third_party/WebKit/WebKit/chromium/public/WebStorageEventDispatcher.h"
#include "third_party/WebKit/WebKit/chromium/public/WebStorageNamespace.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/database/vfs_backend.h"
#include "webkit/extensions/v8/gc_extension.h"
#include "webkit/extensions/v8/gears_extension.h"
#include "webkit/extensions/v8/interval_extension.h"
#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webclipboard_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkitclient_impl.h"
#include "webkit/support/test_webkit_client.h"
#include "webkit/tools/test_shell/mock_webclipboard_impl.h"
#include "webkit/tools/test_shell/simple_appcache_system.h"
#include "webkit/tools/test_shell/simple_database_system.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/simple_webcookiejar_impl.h"
#include "webkit/tools/test_shell/test_shell_request_context.h"
#include "v8/include/v8.h"

#if defined(OS_WIN)
#include "third_party/WebKit/WebKit/chromium/public/win/WebThemeEngine.h"
#include "webkit/tools/test_shell/test_shell_webthemeengine.h"
#endif

using WebKit::WebScriptController;

TestWebKitClient::TestWebKitClient() {
  v8::V8::SetCounterFunction(StatsTable::FindLocation);

  WebKit::initialize(this);
  WebKit::setLayoutTestMode(true);
  WebKit::WebSecurityPolicy::registerURLSchemeAsLocal(
      WebKit::WebString::fromUTF8("test-shell-resource"));
  WebKit::WebSecurityPolicy::registerURLSchemeAsNoAccess(
      WebKit::WebString::fromUTF8("test-shell-resource"));
  WebScriptController::enableV8SingleThreadMode();
  WebScriptController::registerExtension(
      extensions_v8::GearsExtension::Get());
  WebScriptController::registerExtension(
      extensions_v8::IntervalExtension::Get());
  WebKit::WebRuntimeFeatures::enableSockets(true);
  WebKit::WebRuntimeFeatures::enableApplicationCache(true);
  WebKit::WebRuntimeFeatures::enableDatabase(true);

  // Load libraries for media and enable the media player.
  FilePath module_path;
  WebKit::WebRuntimeFeatures::enableMediaPlayer(
      PathService::Get(base::DIR_MODULE, &module_path) &&
      media::InitializeMediaLibrary(module_path));
  // TODO(joth): Make a dummy geolocation service implemenation for
  // test_shell, and set this to true. http://crbug.com/36451
  WebKit::WebRuntimeFeatures::enableGeolocation(false);

  // Construct and initialize an appcache system for this scope.
  // A new empty temp directory is created to house any cached
  // content during the run. Upon exit that directory is deleted.
  // If we can't create a tempdir, we'll use in-memory storage.
  if (!appcache_dir_.CreateUniqueTempDir()) {
    LOG(WARNING) << "Failed to create a temp dir for the appcache, "
                    "using in-memory storage.";
    DCHECK(appcache_dir_.path().empty());
  }
  SimpleAppCacheSystem::InitializeOnUIThread(appcache_dir_.path());

  WebKit::WebDatabase::setObserver(&database_system_);

#if defined(OS_WIN)
  // Ensure we pick up the default theme engine.
  SetThemeEngine(NULL);
#endif

  net::HttpCache::Mode cache_mode = net::HttpCache::NORMAL;
  net::CookieMonster::EnableFileScheme();

  // Initializing with a default context, which means no on-disk cookie DB,
  // and no support for directory listings.
  SimpleResourceLoaderBridge::Init(
      new TestShellRequestContext(FilePath(), cache_mode, true));

  // Test shell always exposes the GC.
  webkit_glue::SetJavaScriptFlags(L" --expose-gc");
  // Expose GCController to JavaScript.
  WebScriptController::registerExtension(extensions_v8::GCExtension::Get());
}

TestWebKitClient::~TestWebKitClient() {
  WebKit::shutdown();
}

WebKit::WebMimeRegistry* TestWebKitClient::mimeRegistry() {
  return &mime_registry_;
}

WebKit::WebClipboard* TestWebKitClient::clipboard() {
  // Mock out clipboard calls so that tests don't mess
  // with each other's copies/pastes when running in parallel.
  return &mock_clipboard_;
}

WebKit::WebSandboxSupport* TestWebKitClient::sandboxSupport() {
  return NULL;
}

WebKit::WebCookieJar* TestWebKitClient::cookieJar() {
  return &cookie_jar_;
}

bool TestWebKitClient::sandboxEnabled() {
  return true;
}

WebKit::WebKitClient::FileHandle TestWebKitClient::databaseOpenFile(
    const WebKit::WebString& vfs_file_name, int desired_flags,
    WebKit::WebKitClient::FileHandle* dir_handle) {
  return SimpleDatabaseSystem::GetInstance()->OpenFile(
      vfs_file_name, desired_flags, dir_handle);
}

int TestWebKitClient::databaseDeleteFile(const WebKit::WebString& vfs_file_name,
                                         bool sync_dir) {
  return SimpleDatabaseSystem::GetInstance()->DeleteFile(
      vfs_file_name, sync_dir);
}

long TestWebKitClient::databaseGetFileAttributes(
    const WebKit::WebString& vfs_file_name) {
  return SimpleDatabaseSystem::GetInstance()->GetFileAttributes(vfs_file_name);
}

long long TestWebKitClient::databaseGetFileSize(
    const WebKit::WebString& vfs_file_name) {
  return SimpleDatabaseSystem::GetInstance()->GetFileSize(vfs_file_name);
}

bool TestWebKitClient::getFileSize(const WebKit::WebString& path, long long& result) {
  return file_util::GetFileSize(
      webkit_glue::WebStringToFilePath(path),
      reinterpret_cast<int64*>(&result));
}

unsigned long long TestWebKitClient::visitedLinkHash(const char* canonicalURL,
                                                     size_t length) {
  return 0;
}

bool TestWebKitClient::isLinkVisited(unsigned long long linkHash) {
  return false;
}

WebKit::WebMessagePortChannel* TestWebKitClient::createMessagePortChannel() {
  return NULL;
}

void TestWebKitClient::prefetchHostName(const WebKit::WebString&) {
}

WebKit::WebData TestWebKitClient::loadResource(const char* name) {
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

WebKit::WebString TestWebKitClient::defaultLocale() {
  return ASCIIToUTF16("en-US");
}

WebKit::WebStorageNamespace* TestWebKitClient::createLocalStorageNamespace(
    const WebKit::WebString& path, unsigned quota) {
  return WebKit::WebStorageNamespace::createLocalStorageNamespace(path, quota);
}

void TestWebKitClient::dispatchStorageEvent(const WebKit::WebString& key,
    const WebKit::WebString& old_value, const WebKit::WebString& new_value,
    const WebKit::WebString& origin, const WebKit::WebURL& url,
    bool is_local_storage) {
  // The event is dispatched by the proxy.
}

WebKit::WebApplicationCacheHost* TestWebKitClient::createApplicationCacheHost(
    WebKit::WebApplicationCacheHostClient* client) {
  return SimpleAppCacheSystem::CreateApplicationCacheHost(client);
}

#if defined(OS_WIN)
void TestWebKitClient::SetThemeEngine(WebKit::WebThemeEngine* engine) {
  active_theme_engine_ = engine ? engine : WebKitClientImpl::themeEngine();
}

WebKit::WebThemeEngine* TestWebKitClient::themeEngine() {
  return active_theme_engine_;
}
#endif

WebKit::WebSharedWorkerRepository* TestWebKitClient::sharedWorkerRepository() {
  return NULL;
}
