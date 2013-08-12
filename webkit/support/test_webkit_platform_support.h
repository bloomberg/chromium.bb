// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_TEST_WEBKIT_PLATFORM_SUPPORT_H_
#define WEBKIT_SUPPORT_TEST_WEBKIT_PLATFORM_SUPPORT_H_

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "third_party/WebKit/public/platform/WebUnitTestSupport.h"
#include "webkit/child/webkitplatformsupport_child_impl.h"
#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webfileutilities_impl.h"
#include "webkit/renderer/compositor_bindings/web_compositor_support_impl.h"
#include "webkit/support/mock_webclipboard_impl.h"
#include "webkit/support/weburl_loader_mock_factory.h"

namespace WebKit {
class WebLayerTreeView;
}

// An implementation of WebKitPlatformSupport for tests.
class TestWebKitPlatformSupport :
    public WebKit::WebUnitTestSupport,
    public webkit_glue::WebKitPlatformSupportChildImpl {
 public:
  TestWebKitPlatformSupport();
  virtual ~TestWebKitPlatformSupport();

  virtual WebKit::WebMimeRegistry* mimeRegistry();
  virtual WebKit::WebClipboard* clipboard();
  virtual WebKit::WebFileUtilities* fileUtilities();
  virtual WebKit::WebIDBFactory* idbFactory();

  virtual WebKit::WebURLLoader* createURLLoader();
  virtual WebKit::WebData loadResource(const char* name);
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name);
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name,
      const WebKit::WebString& value);
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name,
      const WebKit::WebString& value1,
      const WebKit::WebString& value2);
  virtual WebKit::WebString defaultLocale();

#if defined(OS_WIN) || defined(OS_MACOSX)
  void SetThemeEngine(WebKit::WebThemeEngine* engine);
  virtual WebKit::WebThemeEngine *themeEngine();
#endif

  virtual WebKit::WebCompositorSupport* compositorSupport();

  WebURLLoaderMockFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  const base::FilePath& file_system_root() const {
    return file_system_root_.path();
  }

  virtual base::string16 GetLocalizedString(int message_id) OVERRIDE;
  virtual base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) OVERRIDE;
  virtual webkit_glue::ResourceLoaderBridge* CreateResourceLoader(
      const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info)
     OVERRIDE;
  virtual webkit_glue::WebSocketStreamHandleBridge* CreateWebSocketBridge(
      WebKit::WebSocketStreamHandle* handle,
      webkit_glue::WebSocketStreamHandleDelegate* delegate) OVERRIDE;

  virtual WebKit::WebGestureCurve* createFlingAnimationCurve(
      int device_source,
      const WebKit::WebFloatPoint& velocity,
      const WebKit::WebSize& cumulative_scroll);

  virtual WebKit::WebUnitTestSupport* unitTestSupport();

  // WebUnitTestSupport implementation
  virtual void registerMockedURL(const WebKit::WebURL& url,
                                 const WebKit::WebURLResponse& response,
                                 const WebKit::WebString& filePath);
  virtual void registerMockedErrorURL(const WebKit::WebURL& url,
                                      const WebKit::WebURLResponse& response,
                                      const WebKit::WebURLError& error);
  virtual void unregisterMockedURL(const WebKit::WebURL& url);
  virtual void unregisterAllMockedURLs();
  virtual void serveAsynchronousMockedRequests();
  virtual WebKit::WebString webKitRootDir();
  virtual WebKit::WebLayerTreeView* createLayerTreeViewForTesting();
  virtual WebKit::WebLayerTreeView* createLayerTreeViewForTesting(
      TestViewType type);
  virtual WebKit::WebData readFromFile(const WebKit::WebString& path);

 private:
  webkit_glue::SimpleWebMimeRegistryImpl mime_registry_;
  MockWebClipboardImpl mock_clipboard_;
  webkit_glue::WebFileUtilitiesImpl file_utilities_;
  base::ScopedTempDir file_system_root_;
  WebURLLoaderMockFactory url_loader_factory_;
  webkit::WebCompositorSupportImpl compositor_support_;

#if defined(OS_WIN) || defined(OS_MACOSX)
  WebKit::WebThemeEngine* active_theme_engine_;
#endif
  DISALLOW_COPY_AND_ASSIGN(TestWebKitPlatformSupport);
};

#endif  // WEBKIT_SUPPORT_TEST_WEBKIT_PLATFORM_SUPPORT_H_
