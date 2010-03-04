// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/webkit_support.h"

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/weak_ptr.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/glue/media/buffered_data_source.h"
#include "webkit/glue/media/media_resource_loader_bridge_factory.h"
#include "webkit/glue/media/simple_data_source.h"
#include "webkit/glue/media/video_renderer_impl.h"
#include "webkit/glue/webkitclient_impl.h"
#include "webkit/glue/webmediaplayer_impl.h"
#include "webkit/glue/webplugin_impl.h"
#include "webkit/glue/webplugin_page_delegate.h"
#include "webkit/support/test_webplugin_page_delegate.h"
#include "webkit/support/test_webkit_client.h"
#include "webkit/tools/test_shell/simple_database_system.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"

using WebKit::WebPlugin;
using WebKit::WebFrame;
using WebKit::WebPluginParams;
using WebKit::WebMediaPlayerClient;

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
  TestWebKitClient webkit_client_;
  MessageLoopForUI main_message_loop_;
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
  test_environment = new TestEnvironment;
}

void TearDownTestEnvironment() {
  delete test_environment;
  test_environment = NULL;
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
      client, factory, new webkit_glue::VideoRendererImpl::FactoryFactory());
}

// Bridge for SimpleDatabaseSystem

void SetDatabaseQuota(int quota) {
  SimpleDatabaseSystem::GetInstance()->SetDatabaseQuota(quota);
}

void ClearAllDatabases() {
  SimpleDatabaseSystem::GetInstance()->ClearAllDatabases();
}

}  // namespace webkit_support
