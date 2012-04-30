// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
      const std::string& mime_type) OVERRIDE;
  virtual void CreatedPluginWindow(gfx::PluginWindowHandle handle) OVERRIDE {}
  virtual void WillDestroyPluginWindow(
      gfx::PluginWindowHandle handle) OVERRIDE {}
  virtual void DidMovePlugin(
      const webkit::npapi::WebPluginGeometry& move) OVERRIDE {}
  virtual void DidStartLoadingForPlugin() OVERRIDE {}
  virtual void DidStopLoadingForPlugin() OVERRIDE {}
  virtual WebKit::WebCookieJar* GetCookieJar() OVERRIDE;
};

}  // namespace webkit_support

#endif  // WEBKIT_SUPPORT_TEST_WEBPLUGIN_PAGE_DELEGATE_H_
