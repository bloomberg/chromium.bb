// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_TEST_WEBKIT_PLATFORM_SUPPORT_H_
#define WEBKIT_SUPPORT_TEST_WEBKIT_PLATFORM_SUPPORT_H_

#include "base/compiler_specific.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGamepads.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebUnitTestSupport.h"
#include "webkit/glue/webfileutilities_impl.h"
#include "webkit/glue/webkitplatformsupport_impl.h"
#include "webkit/mocks/mock_webhyphenator.h"
#include "webkit/support/simple_database_system.h"
#include "webkit/support/weburl_loader_mock_factory.h"
#include "webkit/tools/test_shell/mock_webclipboard_impl.h"
#include "webkit/tools/test_shell/simple_appcache_system.h"
#include "webkit/tools/test_shell/simple_dom_storage_system.h"
#include "webkit/tools/test_shell/simple_file_system.h"
#include "webkit/tools/test_shell/simple_webcookiejar_impl.h"
#include "webkit/tools/test_shell/test_shell_webmimeregistry_impl.h"

class TestShellWebBlobRegistryImpl;

namespace cc {
class ContextProvider;
}

namespace WebKit {
class WebAudioDevice;
class WebLayerTreeView;
}

// An implementation of WebKitPlatformSupport for tests.
class TestWebKitPlatformSupport :
    public WebKit::WebUnitTestSupport,
    public webkit_glue::WebKitPlatformSupportImpl {
 public:
  TestWebKitPlatformSupport(bool unit_test_mode,
                            WebKit::Platform* shadow_platform_delegate);
  virtual ~TestWebKitPlatformSupport();

  virtual WebKit::WebMimeRegistry* mimeRegistry();
  virtual WebKit::WebClipboard* clipboard();
  virtual WebKit::WebFileUtilities* fileUtilities();
  virtual WebKit::WebSandboxSupport* sandboxSupport();
  virtual WebKit::WebCookieJar* cookieJar();
  virtual WebKit::WebBlobRegistry* blobRegistry();
  virtual WebKit::WebFileSystem* fileSystem();
  virtual WebKit::WebHyphenator* hyphenator();

  virtual bool sandboxEnabled();
  virtual WebKit::Platform::FileHandle databaseOpenFile(
      const WebKit::WebString& vfs_file_name, int desired_flags);
  virtual int databaseDeleteFile(const WebKit::WebString& vfs_file_name,
                                 bool sync_dir);
  virtual long databaseGetFileAttributes(
      const WebKit::WebString& vfs_file_name);
  virtual long long databaseGetFileSize(
      const WebKit::WebString& vfs_file_name);
  virtual long long databaseGetSpaceAvailableForOrigin(
      const WebKit::WebString& origin_identifier);
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
  virtual WebKit::WebStorageNamespace* createLocalStorageNamespace(
      const WebKit::WebString& path, unsigned quota);

#if defined(OS_WIN) || defined(OS_MACOSX)
  void SetThemeEngine(WebKit::WebThemeEngine* engine);
  virtual WebKit::WebThemeEngine *themeEngine();
#endif

  virtual WebKit::WebGraphicsContext3D* createOffscreenGraphicsContext3D(
      const WebKit::WebGraphicsContext3D::Attributes&);
  virtual WebKit::WebGraphicsContext3D* sharedOffscreenGraphicsContext3D();
  virtual GrContext* sharedOffscreenGrContext();
  virtual bool canAccelerate2dCanvas();
  virtual bool isThreadedCompositingEnabled();

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

  virtual string16 GetLocalizedString(int message_id) OVERRIDE;
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

  void set_threaded_compositing_enabled(bool enabled) {
    threaded_compositing_enabled_ = enabled;
  }

 private:
  TestShellWebMimeRegistryImpl mime_registry_;
  MockWebClipboardImpl mock_clipboard_;
  webkit_glue::WebFileUtilitiesImpl file_utilities_;
  base::ScopedTempDir appcache_dir_;
  SimpleAppCacheSystem appcache_system_;
  SimpleDatabaseSystem database_system_;
  SimpleDomStorageSystem dom_storage_system_;
  SimpleWebCookieJarImpl cookie_jar_;
  scoped_refptr<TestShellWebBlobRegistryImpl> blob_registry_;
  SimpleFileSystem file_system_;
  base::ScopedTempDir file_system_root_;
  webkit_glue::MockWebHyphenator hyphenator_;
  WebURLLoaderMockFactory url_loader_factory_;
  bool unit_test_mode_;
  WebKit::WebGamepads gamepad_data_;
  WebKit::Platform* shadow_platform_delegate_;
  bool threaded_compositing_enabled_;

  scoped_refptr<cc::ContextProvider> main_thread_contexts_;

#if defined(OS_WIN) || defined(OS_MACOSX)
  WebKit::WebThemeEngine* active_theme_engine_;
#endif
  DISALLOW_COPY_AND_ASSIGN(TestWebKitPlatformSupport);
};

#endif  // WEBKIT_SUPPORT_TEST_WEBKIT_PLATFORM_SUPPORT_H_
