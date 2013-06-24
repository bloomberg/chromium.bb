// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_TEST_WEBKIT_PLATFORM_SUPPORT_H_
#define WEBKIT_SUPPORT_TEST_WEBKIT_PLATFORM_SUPPORT_H_

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebUnitTestSupport.h"
#include "webkit/child/webkitplatformsupport_child_impl.h"
#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webfileutilities_impl.h"
#include "webkit/mocks/mock_webhyphenator.h"
#include "webkit/renderer/compositor_bindings/web_compositor_support_impl.h"
#include "webkit/support/mock_webclipboard_impl.h"
#include "webkit/support/weburl_loader_mock_factory.h"

class TestShellWebBlobRegistryImpl;

namespace cc {
class ContextProvider;
}

namespace WebKit {
class WebAudioDevice;
class WebGraphicsContext3DProvider;
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
  virtual WebKit::WebSandboxSupport* sandboxSupport();
  virtual WebKit::WebBlobRegistry* blobRegistry();
  virtual WebKit::WebHyphenator* hyphenator();
  virtual WebKit::WebIDBFactory* idbFactory();

  virtual bool sandboxEnabled();
  virtual unsigned long long visitedLinkHash(const char* canonicalURL,
                                             size_t length);
  virtual bool isLinkVisited(unsigned long long linkHash);
  virtual WebKit::WebMessagePortChannel* createMessagePortChannel();
  virtual void prefetchHostName(const WebKit::WebString&);
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

  virtual WebKit::WebGraphicsContext3D* createOffscreenGraphicsContext3D(
      const WebKit::WebGraphicsContext3D::Attributes&);
  virtual WebKit::WebGraphicsContext3DProvider*
      createSharedOffscreenGraphicsContext3DProvider();
  virtual bool canAccelerate2dCanvas();
  virtual bool isThreadedCompositingEnabled();
  virtual WebKit::WebCompositorSupport* compositorSupport();

  WebURLLoaderMockFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  const base::FilePath& file_system_root() const {
    return file_system_root_.path();
  }

  // Mock out the WebAudioDevice since the real one
  // talks with the browser process.
  virtual double audioHardwareSampleRate();
  virtual size_t audioHardwareBufferSize();
  virtual WebKit::WebAudioDevice* createAudioDevice(size_t bufferSize,
      unsigned numberOfInputChannels, unsigned numberOfChannels,
      double sampleRate, WebKit::WebAudioDevice::RenderCallback*,
      const WebKit::WebString& input_device_id);
  virtual WebKit::WebAudioDevice* createAudioDevice(size_t bufferSize,
      unsigned numberOfInputChannels, unsigned numberOfChannels,
      double sampleRate, WebKit::WebAudioDevice::RenderCallback*);
  virtual WebKit::WebAudioDevice* createAudioDevice(size_t bufferSize,
      unsigned numberOfChannels, double sampleRate,
      WebKit::WebAudioDevice::RenderCallback*);

  virtual void sampleGamepads(WebKit::WebGamepads& data);
  void setGamepadData(const WebKit::WebGamepads& data);

  virtual base::string16 GetLocalizedString(int message_id) OVERRIDE;
  virtual base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) OVERRIDE;
  virtual void GetPlugins(bool refresh,
                          std::vector<webkit::WebPluginInfo>* plugins) OVERRIDE;
  virtual webkit_glue::ResourceLoaderBridge* CreateResourceLoader(
      const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info)
     OVERRIDE;
  virtual webkit_glue::WebSocketStreamHandleBridge* CreateWebSocketBridge(
      WebKit::WebSocketStreamHandle* handle,
      webkit_glue::WebSocketStreamHandleDelegate* delegate) OVERRIDE;

  virtual WebKit::WebMediaStreamCenter* createMediaStreamCenter(
      WebKit::WebMediaStreamCenterClient* client);
  virtual WebKit::WebRTCPeerConnectionHandler* createRTCPeerConnectionHandler(
      WebKit::WebRTCPeerConnectionHandlerClient* client);

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

 private:
  webkit_glue::SimpleWebMimeRegistryImpl mime_registry_;
  MockWebClipboardImpl mock_clipboard_;
  webkit_glue::WebFileUtilitiesImpl file_utilities_;
  base::ScopedTempDir appcache_dir_;
  scoped_refptr<TestShellWebBlobRegistryImpl> blob_registry_;
  base::ScopedTempDir file_system_root_;
  webkit_glue::MockWebHyphenator hyphenator_;
  WebURLLoaderMockFactory url_loader_factory_;
  WebKit::WebGamepads gamepad_data_;
  webkit::WebCompositorSupportImpl compositor_support_;

  scoped_refptr<cc::ContextProvider> main_thread_contexts_;

#if defined(OS_WIN) || defined(OS_MACOSX)
  WebKit::WebThemeEngine* active_theme_engine_;
#endif
  DISALLOW_COPY_AND_ASSIGN(TestWebKitPlatformSupport);
};

#endif  // WEBKIT_SUPPORT_TEST_WEBKIT_PLATFORM_SUPPORT_H_
