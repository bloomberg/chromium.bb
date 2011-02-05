// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_PAGE_DELEGATE_
#define WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_PAGE_DELEGATE_

#include "ui/gfx/native_widget_types.h"

class FilePath;
class GURL;

namespace WebKit {
class WebCookieJar;
}

namespace webkit {
namespace npapi {

class WebPluginDelegate;
struct WebPluginGeometry;

// Used by the WebPlugin to communicate back to the containing page.
class WebPluginPageDelegate {
 public:
  // This method is called to create a WebPluginDelegate implementation when a
  // new plugin is instanced.  See CreateWebPluginDelegateHelper
  // for a default WebPluginDelegate implementation.
  virtual WebPluginDelegate* CreatePluginDelegate(
      const FilePath& file_path,
      const std::string& mime_type) = 0;

  // Called when a windowed plugin is created.
  // Lets the view delegate create anything it is using to wrap the plugin.
  virtual void CreatedPluginWindow(
      gfx::PluginWindowHandle handle) = 0;

  // Called when a windowed plugin is closing.
  // Lets the view delegate shut down anything it is using to wrap the plugin.
  virtual void WillDestroyPluginWindow(
      gfx::PluginWindowHandle handle) = 0;

  // Keeps track of the necessary window move for a plugin window that resulted
  // from a scroll operation.  That way, all plugin windows can be moved at the
  // same time as each other and the page.
  virtual void DidMovePlugin(
      const WebPluginGeometry& move) = 0;

  // Notifies the parent view that a load has begun.
  virtual void DidStartLoadingForPlugin() = 0;

  // Notifies the parent view that all loads are finished.
  virtual void DidStopLoadingForPlugin() = 0;

  // Asks the browser to show a modal HTML dialog.  The dialog is passed the
  // given arguments as a JSON string, and returns its result as a JSON string
  // through json_retval.
  virtual void ShowModalHTMLDialogForPlugin(
      const GURL& url,
      const gfx::Size& size,
      const std::string& json_arguments,
      std::string* json_retval) = 0;

  // The WebCookieJar to use for this plugin.
  virtual WebKit::WebCookieJar* GetCookieJar() = 0;
};

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_PAGE_DELEGATE_H_
