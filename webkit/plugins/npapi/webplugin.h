// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_H_
#define WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "gfx/native_widget_types.h"
#include "gfx/rect.h"

// TODO(port): this typedef is obviously incorrect on non-Windows
// platforms, but now a lot of code now accidentally depends on them
// existing.  #ifdef out these declarations and fix all the users.
typedef void* HANDLE;

class GURL;
struct NPObject;

namespace WebKit {
class WebFrame;
}

namespace webkit {
namespace npapi {

class WebPluginDelegate;
class WebPluginParentView;
class WebPluginResourceClient;
#if defined(OS_MACOSX)
class WebPluginAcceleratedSurface;
#endif

// Describes the new location for a plugin window.
struct WebPluginGeometry {
  WebPluginGeometry();
  ~WebPluginGeometry();

  bool Equals(const WebPluginGeometry& rhs) const;

  // On Windows, this is the plugin window in the plugin process.
  // On X11, this is the XID of the plugin-side GtkPlug containing the
  // GtkSocket hosting the actual plugin window.
  //
  // On Mac OS X, all of the plugin types are currently "windowless"
  // (window == 0) except for the special case of the GPU plugin,
  // which currently performs rendering on behalf of the Pepper 3D API
  // and WebGL. The GPU plugin uses a simple integer for the
  // PluginWindowHandle which is used to map to a side data structure
  // containing information about the plugin. Soon this plugin will be
  // generalized, at which point this mechanism will be rethought or
  // removed.
  gfx::PluginWindowHandle window;
  gfx::Rect window_rect;
  // Clip rect (include) and cutouts (excludes), relative to
  // window_rect origin.
  gfx::Rect clip_rect;
  std::vector<gfx::Rect> cutout_rects;
  bool rects_valid;
  bool visible;
};

// The WebKit side of a plugin implementation.  It provides wrappers around
// operations that need to interact with the frame and other WebCore objects.
class WebPlugin {
 public:
  virtual ~WebPlugin() {}

  // Called by the plugin delegate to let the WebPlugin know if the plugin is
  // windowed (i.e. handle is not NULL) or windowless (handle is NULL).  This
  // tells the WebPlugin to send mouse/keyboard events to the plugin delegate,
  // as well as the information about the HDC for paint operations.
  virtual void SetWindow(gfx::PluginWindowHandle window) = 0;

  // Whether input events should be sent to the delegate.
  virtual void SetAcceptsInputEvents(bool accepts) = 0;

  // Called by the plugin delegate to let it know that the window is being
  // destroyed.
  virtual void WillDestroyWindow(gfx::PluginWindowHandle window) = 0;
#if defined(OS_WIN)
  // The pump_messages_event is a event handle which is valid only for
  // windowless plugins and is used in NPP_HandleEvent calls to pump messages
  // if the plugin enters a modal loop.
  // Cancels a pending request.
  virtual void SetWindowlessPumpEvent(HANDLE pump_messages_event) = 0;
#endif
  virtual void CancelResource(unsigned long id) = 0;
  virtual void Invalidate() = 0;
  virtual void InvalidateRect(const gfx::Rect& rect) = 0;

  // Returns the NPObject for the browser's window object.
  virtual NPObject* GetWindowScriptNPObject() = 0;

  // Returns the DOM element that loaded the plugin.
  virtual NPObject* GetPluginElement() = 0;

  // Cookies
  virtual void SetCookie(const GURL& url,
                         const GURL& first_party_for_cookies,
                         const std::string& cookie) = 0;
  virtual std::string GetCookies(const GURL& url,
                                 const GURL& first_party_for_cookies) = 0;

  // Shows a modal HTML dialog containing the given URL.  json_arguments are
  // passed to the dialog via the DOM 'window.chrome.dialogArguments', and the
  // retval is the string returned by 'window.chrome.send("DialogClose",
  // retval)'.
  virtual void ShowModalHTMLDialog(const GURL& url, int width, int height,
                                   const std::string& json_arguments,
                                   std::string* json_retval) = 0;

  // When a default plugin has downloaded the plugin list and finds it is
  // available, it calls this method to notify the renderer. Also it will update
  // the status when user clicks on the plugin to install.
  virtual void OnMissingPluginStatus(int status) = 0;

  // Handles GetURL/GetURLNotify/PostURL/PostURLNotify requests initiated
  // by plugins.  If the plugin wants notification of the result, notify_id will
  // be non-zero.
  virtual void HandleURLRequest(const char* url,
                                const char* method,
                                const char* target,
                                const char* buf,
                                unsigned int len,
                                int notify_id,
                                bool popups_allowed,
                                bool notify_redirects) = 0;

  // Cancels document load.
  virtual void CancelDocumentLoad() = 0;

  // Initiates a HTTP range request for an existing stream.
  virtual void InitiateHTTPRangeRequest(const char* url,
                                        const char* range_info,
                                        int range_request_id) = 0;

  // Returns true iff in off the record (Incognito) mode.
  virtual bool IsOffTheRecord() = 0;

  // Called when the WebPluginResourceClient instance is deleted.
  virtual void ResourceClientDeleted(
      WebPluginResourceClient* resource_client) {}

  // Defers the loading of the resource identified by resource_id. This is
  // controlled by the defer parameter.
  virtual void SetDeferResourceLoading(unsigned long resource_id,
                                       bool defer) = 0;

#if defined(OS_MACOSX)
  // Called to inform the WebPlugin that the plugin has gained or lost focus.
  virtual void FocusChanged(bool focused) {};

  // Starts plugin IME.
  virtual void StartIme() {};

  // Synthesize a fake window handle for the plug-in to identify the instance
  // to the browser, allowing mapping to a surface for hardware accelleration
  // of plug-in content. The browser generates the handle which is then set on
  // the plug-in. |opaque| indicates whether the content should be treated as
  // opaque or translucent.
  // TODO(stuartmorgan): Move this into WebPluginProxy.
  virtual void BindFakePluginWindowHandle(bool opaque) {}

  // Returns the accelerated surface abstraction for accelerated plugins.
  virtual WebPluginAcceleratedSurface* GetAcceleratedSurface() { return NULL; }
#endif

  // Gets the WebPluginDelegate that implements the interface.
  // This API is only for use with Pepper, and is only overridden
  // by in-renderer implementations.
  virtual WebPluginDelegate* delegate();

  // Handles NPN_URLRedirectResponse calls issued by plugins in response to
  // HTTP URL redirect notifications.
  virtual void URLRedirectResponse(bool allow, int resource_id) = 0;
};

// Simpler version of ResourceHandleClient that lends itself to proxying.
class WebPluginResourceClient {
 public:
  virtual ~WebPluginResourceClient() {}
  virtual void WillSendRequest(const GURL& url, int http_status_code) = 0;
  // The request_is_seekable parameter indicates whether byte range requests
  // can be issued for the underlying stream.
  virtual void DidReceiveResponse(const std::string& mime_type,
                                  const std::string& headers,
                                  uint32 expected_length,
                                  uint32 last_modified,
                                  bool request_is_seekable) = 0;
  virtual void DidReceiveData(const char* buffer, int length,
                              int data_offset) = 0;
  virtual void DidFinishLoading() = 0;
  virtual void DidFail() = 0;
  virtual bool IsMultiByteResponseExpected() = 0;
  virtual int ResourceId() = 0;
};

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_H_
