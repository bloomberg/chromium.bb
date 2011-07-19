// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_TEST_WEBPLUGIN_PAGE_DELEGATE_H_
#define WEBKIT_SUPPORT_TEST_WEBPLUGIN_PAGE_DELEGATE_H_

#include <string>

#include "webkit/plugins/npapi/webplugin_delegate_impl.h"
#include "webkit/plugins/npapi/webplugin_page_delegate.h"

namespace webkit_support {

class TestWebPluginPageDelegate : public webkit::npapi::WebPluginPageDelegate {
 public:
  TestWebPluginPageDelegate() {}
  virtual ~TestWebPluginPageDelegate() {}

  virtual webkit::npapi::WebPluginDelegate* CreatePluginDelegate(
      const FilePath& file_path,
      const std::string& mime_type);
  virtual void CreatedPluginWindow(gfx::PluginWindowHandle handle) {}
  virtual void WillDestroyPluginWindow(gfx::PluginWindowHandle handle) {}
  virtual void DidMovePlugin(const webkit::npapi::WebPluginGeometry& move) {}
  virtual void DidStartLoadingForPlugin() {}
  virtual void DidStopLoadingForPlugin() {}
  virtual WebKit::WebCookieJar* GetCookieJar();
};

}  // namespace webkit_support

#endif  // WEBKIT_SUPPORT_TEST_WEBPLUGIN_PAGE_DELEGATE_H_
