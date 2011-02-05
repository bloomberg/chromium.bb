// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_DELEGATE_H_
#define WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_DELEGATE_H_

#include <string>
#include <vector>

#include "base/string16.h"
#include "build/build_config.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npapi_extensions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCanvas.h"
#include "ui/gfx/native_widget_types.h"
#include "webkit/plugins/npapi/webplugin_2d_device_delegate.h"
#include "webkit/plugins/npapi/webplugin_3d_device_delegate.h"
#include "webkit/plugins/npapi/webplugin_audio_device_delegate.h"
#include "webkit/plugins/npapi/webplugin_file_delegate.h"
#include "webkit/plugins/npapi/webplugin_print_delegate.h"

class FilePath;
class GURL;
struct NPObject;

namespace WebKit {
class WebInputEvent;
struct WebCursorInfo;
}

namespace gfx {
class Rect;
}

namespace webkit {
namespace npapi {

class WebPlugin;
class WebPluginResourceClient;

// This is the interface that a plugin implementation needs to provide.
class WebPluginDelegate : public WebPlugin2DDeviceDelegate,
                          public WebPlugin3DDeviceDelegate,
                          public WebPluginAudioDeviceDelegate,
                          public WebPluginPrintDelegate,
                          public WebPluginFileDelegate {
 public:
  virtual ~WebPluginDelegate() {}

  // Initializes the plugin implementation with the given (UTF8) arguments.
  // Note that the lifetime of WebPlugin must be longer than this delegate.
  // If this function returns false the plugin isn't started and shouldn't be
  // called again.  If this method succeeds, then the WebPlugin is valid until
  // PluginDestroyed is called.
  // The load_manually parameter if true indicates that the plugin data would
  // be passed from webkit. if false indicates that the plugin should download
  // the data. This also controls whether the plugin is instantiated as a full
  // page plugin (NP_FULL) or embedded (NP_EMBED).
  virtual bool Initialize(const GURL& url,
                          const std::vector<std::string>& arg_names,
                          const std::vector<std::string>& arg_values,
                          WebPlugin* plugin,
                          bool load_manually) = 0;

  // Called when the WebPlugin is being destroyed.  This is a signal to the
  // delegate that it should tear-down the plugin implementation and not call
  // methods on the WebPlugin again.
  virtual void PluginDestroyed() = 0;

  // Update the geometry of the plugin.  This is a request to move the
  // plugin, relative to its containing window, to the coords given by
  // window_rect.  Its contents should be clipped to the coords given
  // by clip_rect, which are relative to the origin of the plugin
  // window.  The clip_rect is in plugin-relative coordinates.
  virtual void UpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect) = 0;

  // Tells the plugin to paint the damaged rect.  |canvas| is only used for
  // windowless plugins.
  virtual void Paint(WebKit::WebCanvas* canvas, const gfx::Rect& rect) = 0;

  // Tells the plugin to print itself.
  virtual void Print(gfx::NativeDrawingContext hdc) = 0;

  // Informs the plugin that it has gained or lost focus. This is only called in
  // windowless mode.
  virtual void SetFocus(bool focused) = 0;

  // For windowless plugins, gives them a user event like mouse/keyboard.
  // Returns whether the event was handled. This is only called in windowsless
  // mode. See NPAPI NPP_HandleEvent for more information.
  virtual bool HandleInputEvent(const WebKit::WebInputEvent& event,
                                WebKit::WebCursorInfo* cursor) = 0;

  // Gets the NPObject associated with the plugin for scripting.
  virtual NPObject* GetPluginScriptableObject() = 0;

  // Receives notification about a resource load that the plugin initiated
  // for a frame.
  virtual void DidFinishLoadWithReason(const GURL& url, NPReason reason,
                                       int notify_id) = 0;

  // Returns the process id of the process that is running the plugin.
  virtual int GetProcessId() = 0;

  // The result, UTF-8 encoded, of the script execution is returned via this
  // function.
  virtual void SendJavaScriptStream(const GURL& url,
                                    const std::string& result,
                                    bool success,
                                    int notify_id) = 0;

  // Receives notification about data being available.
  virtual void DidReceiveManualResponse(const GURL& url,
                                        const std::string& mime_type,
                                        const std::string& headers,
                                        uint32 expected_length,
                                        uint32 last_modified) = 0;

  // Receives the data.
  virtual void DidReceiveManualData(const char* buffer, int length) = 0;

  // Indicates end of data load.
  virtual void DidFinishManualLoading() = 0;

  // Indicates a failure in data receipt.
  virtual void DidManualLoadFail() = 0;

  // Only supported when the plugin is the default plugin.
  virtual void InstallMissingPlugin() = 0;

  // Creates a WebPluginResourceClient instance and returns the same.
  virtual WebPluginResourceClient* CreateResourceClient(
      unsigned long resource_id,
      const GURL& url,
      int notify_id) = 0;

  // Creates a WebPluginResourceClient instance for an existing stream that is
  // has become seekable.
  virtual WebPluginResourceClient* CreateSeekableResourceClient(
      unsigned long resource_id, int range_request_id) = 0;

  // See WebPluginContainerImpl's description of the interface.
  virtual bool StartFind(const string16& search_text,
                         bool case_sensitive,
                         int identifier);
  virtual void SelectFindResult(bool forward) {}
  virtual void StopFind() {}
  virtual void NumberOfFindResultsChanged(int total, bool final_result) {}
  virtual void SelectedFindResultChanged(int index) {}
  virtual NPWidgetExtensions* GetWidgetExtensions();
  virtual bool SetCursor(NPCursorType type);
  virtual NPFontExtensions* GetFontExtensions();

  // Used for zooming of full page plugins.  0 means reset, while -1 means zoom
  // out and +1 means zoom in.
  virtual void SetZoomFactor(float scale, bool text_only) {}
  // Gets the selected text, if any.
  virtual bool HasSelection() const;
  virtual string16 GetSelectionAsText() const;
  virtual string16 GetSelectionAsMarkup() const;
};

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_DELEGATE_H_
