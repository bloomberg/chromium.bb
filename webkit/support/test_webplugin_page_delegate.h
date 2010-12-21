// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_TEST_WEBPLUGIN_PAGE_DELEGATE_H_
#define WEBKIT_SUPPORT_TEST_WEBPLUGIN_PAGE_DELEGATE_H_

#include <string>

#include "webkit/glue/plugins/webplugin_delegate_impl.h"
#include "webkit/glue/plugins/webplugin_page_delegate.h"

namespace webkit_support {

class TestWebPluginPageDelegate : public webkit_glue::WebPluginPageDelegate {
 public:
  TestWebPluginPageDelegate() {}
  virtual ~TestWebPluginPageDelegate() {}

  virtual webkit_glue::WebPluginDelegate* CreatePluginDelegate(
      const FilePath& file_path,
      const std::string& mime_type) {
    // We don't need a valid native window handle in layout tests.
    // So just passing 0.
    return WebPluginDelegateImpl::Create(file_path, mime_type, 0);
  }
  virtual void CreatedPluginWindow(gfx::PluginWindowHandle handle) {}
  virtual void WillDestroyPluginWindow(gfx::PluginWindowHandle handle) {}
  virtual void DidMovePlugin(const webkit_glue::WebPluginGeometry& move) {}
  virtual void DidStartLoadingForPlugin() {}
  virtual void DidStopLoadingForPlugin() {}
  virtual void ShowModalHTMLDialogForPlugin(
      const GURL& url,
      const gfx::Size& size,
      const std::string& json_arguments,
      std::string* json_retval) {}
  virtual WebKit::WebCookieJar* GetCookieJar() {
    return WebKit::webKitClient()->cookieJar();
  }
};

}  // namespace webkit_support
#endif  // WEBKIT_SUPPORT_TEST_WEBPLUGIN_PAGE_DELEGATE_H_
