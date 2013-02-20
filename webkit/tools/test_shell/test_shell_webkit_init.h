// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBKIT_INIT_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBKIT_INIT_H_

#include "base/compiler_specific.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageNamespace.h"
#include "webkit/glue/webclipboard_impl.h"
#include "webkit/glue/webfileutilities_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkitplatformsupport_impl.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_impl.h"
#include "webkit/support/simple_database_system.h"
#include "webkit/tools/test_shell/mock_webclipboard_impl.h"
#include "webkit/tools/test_shell/simple_appcache_system.h"
#include "webkit/tools/test_shell/simple_clipboard_impl.h"
#include "webkit/tools/test_shell/simple_dom_storage_system.h"
#include "webkit/tools/test_shell/simple_file_system.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/simple_webcookiejar_impl.h"
#include "webkit/tools/test_shell/test_shell_webblobregistry_impl.h"
#include "webkit/tools/test_shell/test_shell_webmimeregistry_impl.h"

#if defined(OS_WIN)
#include "webkit/tools/test_shell/test_shell_webthemeengine.h"
#endif

class TestShellWebKitInit : public webkit_glue::WebKitPlatformSupportImpl {
 public:
  explicit TestShellWebKitInit(bool layout_test_mode);
  virtual ~TestShellWebKitInit();

  virtual WebKit::WebMimeRegistry* mimeRegistry() OVERRIDE;
  virtual WebKit::WebClipboard* clipboard() OVERRIDE;
  virtual WebKit::WebFileUtilities* fileUtilities() OVERRIDE;
  virtual WebKit::WebSandboxSupport* sandboxSupport() OVERRIDE;
  virtual WebKit::WebCookieJar* cookieJar() OVERRIDE;
  virtual WebKit::WebBlobRegistry* blobRegistry() OVERRIDE;
  virtual WebKit::WebFileSystem* fileSystem() OVERRIDE;
  virtual bool sandboxEnabled() OVERRIDE;
  virtual WebKit::WebKitPlatformSupport::FileHandle databaseOpenFile(
      const WebKit::WebString& vfs_file_name, int desired_flags) OVERRIDE;
  virtual int databaseDeleteFile(const WebKit::WebString& vfs_file_name,
                                 bool sync_dir) OVERRIDE;
  virtual long databaseGetFileAttributes(
      const WebKit::WebString& vfs_file_name) OVERRIDE;
  virtual long long databaseGetFileSize(
      const WebKit::WebString& vfs_file_name) OVERRIDE;
  virtual long long databaseGetSpaceAvailableForOrigin(
      const WebKit::WebString& origin_identifier) OVERRIDE;
  virtual unsigned long long visitedLinkHash(const char* canonicalURL,
                                             size_t length) OVERRIDE;
  virtual bool isLinkVisited(unsigned long long linkHash) OVERRIDE;
  virtual WebKit::WebMessagePortChannel* createMessagePortChannel() OVERRIDE;
  virtual void prefetchHostName(const WebKit::WebString&) OVERRIDE;
  virtual WebKit::WebData loadResource(const char* name) OVERRIDE;
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name) OVERRIDE;
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name,
      const WebKit::WebString& value) OVERRIDE;
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name,
      const WebKit::WebString& value1,
      const WebKit::WebString& value2) OVERRIDE;

  virtual WebKit::WebString defaultLocale() OVERRIDE;

  virtual WebKit::WebStorageNamespace* createLocalStorageNamespace(
      const WebKit::WebString& path, unsigned quota) OVERRIDE;

  virtual WebKit::WebIDBFactory* idbFactory();

#if defined(OS_WIN)
  void SetThemeEngine(WebKit::WebThemeEngine* engine) {
    active_theme_engine_ = engine ?
        engine : WebKitPlatformSupportImpl::themeEngine();
  }

  virtual WebKit::WebThemeEngine *themeEngine() OVERRIDE {
    return active_theme_engine_;
  }
#endif

  virtual WebKit::WebGraphicsContext3D* createOffscreenGraphicsContext3D(
      const WebKit::WebGraphicsContext3D::Attributes& attributes);

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

 private:
  scoped_ptr<webkit_glue::SimpleWebMimeRegistryImpl> mime_registry_;
  MockWebClipboardImpl mock_clipboard_;
  SimpleClipboardClient clipboard_client_;
  webkit_glue::WebClipboardImpl real_clipboard_;
  webkit_glue::WebFileUtilitiesImpl file_utilities_;
  base::ScopedTempDir appcache_dir_;
  SimpleAppCacheSystem appcache_system_;
  SimpleDatabaseSystem database_system_;
  SimpleDomStorageSystem dom_storage_system_;
  SimpleWebCookieJarImpl cookie_jar_;
  scoped_refptr<TestShellWebBlobRegistryImpl> blob_registry_;
  SimpleFileSystem file_system_;

#if defined(OS_WIN)
  WebKit::WebThemeEngine* active_theme_engine_;
#endif
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBKIT_INIT_H_
