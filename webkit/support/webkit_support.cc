// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/webkit_support.h"

#include "base/at_exit.h"
#include "base/debug_util.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/weak_ptr.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/glue/media/buffered_data_source.h"
#include "webkit/glue/media/media_resource_loader_bridge_factory.h"
#include "webkit/glue/media/simple_data_source.h"
#include "webkit/glue/media/video_renderer_impl.h"
#include "webkit/glue/plugins/webplugin_impl.h"
#include "webkit/glue/plugins/webplugin_page_delegate.h"
#include "webkit/glue/webkitclient_impl.h"
#include "webkit/glue/webmediaplayer_impl.h"
#include "webkit/support/platform_support.h"
#include "webkit/support/test_webplugin_page_delegate.h"
#include "webkit/support/test_webkit_client.h"
#include "webkit/tools/test_shell/simple_database_system.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"

using WebKit::WebFrame;
using WebKit::WebMediaPlayerClient;
using WebKit::WebPlugin;
using WebKit::WebPluginParams;
using WebKit::WebString;
using WebKit::WebURL;

namespace {

class TestEnvironment {
 public:
  explicit TestEnvironment() {}

  ~TestEnvironment() {
    SimpleResourceLoaderBridge::Shutdown();
  }

  WebKit::WebKitClient* webkit_client() { return &webkit_client_; }

 private:
  base::AtExitManager at_exit_manager_;
  MessageLoopForUI main_message_loop_;
  // TestWebKitClient must be instantiated after the MessageLoopForUI.
  TestWebKitClient webkit_client_;
};

class WebPluginImplWithPageDelegate
    : public webkit_support::TestWebPluginPageDelegate,
      public base::SupportsWeakPtr<WebPluginImplWithPageDelegate>,
      public webkit_glue::WebPluginImpl {
 public:
  WebPluginImplWithPageDelegate(WebFrame* frame,
                                const WebPluginParams& params)
      : webkit_support::TestWebPluginPageDelegate(),
        webkit_glue::WebPluginImpl(frame, params, AsWeakPtr()) {}
  virtual ~WebPluginImplWithPageDelegate() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(WebPluginImplWithPageDelegate);
};

}  // namespace

namespace webkit_support {

static TestEnvironment* test_environment;

void SetUpTestEnvironment() {
  base::EnableTerminationOnHeapCorruption();
  // Load ICU data tables
  icu_util::Initialize();
  BeforeInitialize();
  test_environment = new TestEnvironment;
  AfterIniitalize();
}

void TearDownTestEnvironment() {
  // Flush any remaining messages before we kill ourselves.
  // http://code.google.com/p/chromium/issues/detail?id=9500
  MessageLoop::current()->RunAllPending();

  BeforeShutdown();
  WebKit::shutdown();
  delete test_environment;
  test_environment = NULL;
  AfterShutdown();
}

WebKit::WebKitClient* GetWebKitClient() {
  DCHECK(test_environment);
  return test_environment->webkit_client();
}

WebPlugin* CreateWebPlugin(WebFrame* frame,
                           const WebPluginParams& params) {
  return new WebPluginImplWithPageDelegate(frame, params);
}

WebKit::WebMediaPlayer* CreateMediaPlayer(WebFrame* frame,
                                          WebMediaPlayerClient* client) {
  scoped_refptr<media::FilterFactoryCollection> factory =
      new media::FilterFactoryCollection();

  appcache::WebApplicationCacheHostImpl* appcache_host =
      appcache::WebApplicationCacheHostImpl::FromFrame(frame);

  // TODO(hclam): this is the same piece of code as in RenderView, maybe they
  // should be grouped together.
  webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory =
      new webkit_glue::MediaResourceLoaderBridgeFactory(
          GURL(),  // referrer
          "null",  // frame origin
          "null",  // main_frame_origin
          base::GetCurrentProcId(),
          appcache_host ? appcache_host->host_id() : appcache::kNoHostId,
          0);
  // A simple data source that keeps all data in memory.
  media::FilterFactory* simple_data_source_factory =
      webkit_glue::SimpleDataSource::CreateFactory(MessageLoop::current(),
                                                   bridge_factory);
  // A sophisticated data source that does memory caching.
  media::FilterFactory* buffered_data_source_factory =
      webkit_glue::BufferedDataSource::CreateFactory(MessageLoop::current(),
                                                     bridge_factory);
  factory->AddFactory(buffered_data_source_factory);
  factory->AddFactory(simple_data_source_factory);
  return new webkit_glue::WebMediaPlayerImpl(
      client, factory,
      new webkit_glue::VideoRendererImpl::FactoryFactory(false));
}

// Wrapper for debug_util
bool BeingDebugged() {
  return DebugUtil::BeingDebugged();
}

// Wrappers for MessageLoop

void RunMessageLoop() {
  MessageLoop::current()->Run();
}

void QuitMessageLoop() {
  MessageLoop::current()->Quit();
}

void RunAllPendingMessages() {
  MessageLoop::current()->RunAllPending();
}

void DispatchMessageLoop() {
  MessageLoop* current = MessageLoop::current();
  bool old_state = current->NestableTasksAllowed();
  current->SetNestableTasksAllowed(true);
  current->RunAllPending();
  current->SetNestableTasksAllowed(old_state);
}

void PostTaskFromHere(Task* task) {
  MessageLoop::current()->PostTask(FROM_HERE, task);
}

void PostDelayedTaskFromHere(Task* task, int64 delay_ms) {
  MessageLoop::current()->PostDelayedTask(FROM_HERE, task, delay_ms);
}

// Wrappers for FilePath and file_util

WebString GetAbsoluteWebStringFromUTF8Path(const std::string& utf8_path) {
#if defined(OS_WIN)
  FilePath path(UTF8ToWide(utf8_path));
  file_util::AbsolutePath(&path);
  return WebString(path.value());
#else
  FilePath path(base::SysWideToNativeMB(base::SysUTF8ToWide(utf8_path)));
  file_util::AbsolutePath(&path);
  return WideToUTF16(base::SysNativeMBToWide(path.value()));
#endif
}

WebURL CreateURLForPathOrURL(const std::string& path_or_url_in_nativemb) {
  // NativeMB to UTF-8
  std::wstring wide_path_or_url
      = base::SysNativeMBToWide(path_or_url_in_nativemb);
  std::string path_or_url_in_utf8 = WideToUTF8(wide_path_or_url);

  GURL url(path_or_url_in_utf8);
  if (url.is_valid() && url.has_scheme())
    return WebURL(url);
#if defined(OS_WIN)
  return net::FilePathToFileURL(FilePath(wide_path_or_url));
#else
  return net::FilePathToFileURL(FilePath(path_or_url_in_nativemb));
#endif
}

WebURL RewriteLayoutTestsURL(const std::string& utf8_url) {
  const char kPrefix[] = "file:///tmp/LayoutTests/";
  const int kPrefixLen = arraysize(kPrefix) - 1;

  if (utf8_url.compare(0, kPrefixLen, kPrefix, kPrefixLen))
    return WebURL(GURL(utf8_url));

  FilePath sourcePath;
  PathService::Get(base::DIR_SOURCE_ROOT, &sourcePath);
  FilePath replacePath
      = sourcePath.Append(FILE_PATH_LITERAL("third_party/WebKit/LayoutTests/"));
  if (!file_util::PathExists(replacePath)) {
      replacePath = sourcePath.Append(FILE_PATH_LITERAL("../../LayoutTests/"));
      DCHECK(file_util::PathExists(replacePath));
  }
#if defined(OS_WIN)
  std::string utf8_path = WideToUTF8(replacePath.value());
#else
  std::string utf8_path
      = WideToUTF8(base::SysNativeMBToWide(replacePath.value()));
#endif
  std::string newUrl = std::string("file://") + utf8_path
      + utf8_url.substr(kPrefixLen);
  return WebURL(GURL(newUrl));
}

// Bridge for SimpleDatabaseSystem

void SetDatabaseQuota(int quota) {
  SimpleDatabaseSystem::GetInstance()->SetDatabaseQuota(quota);
}

void ClearAllDatabases() {
  SimpleDatabaseSystem::GetInstance()->ClearAllDatabases();
}

// Bridge for SimpleResourceLoaderBridge

void SetAcceptAllCookies(bool accept) {
  SimpleResourceLoaderBridge::SetAcceptAllCookies(accept);
}

}  // namespace webkit_support
