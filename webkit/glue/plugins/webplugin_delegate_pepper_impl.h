// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGIN_WEBPLUGIN_DELEGATE_PEPPER_IMPL_H_
#define WEBKIT_GLUE_PLUGIN_WEBPLUGIN_DELEGATE_PEPPER_IMPL_H_

#include "build/build_config.h"

#include <list>
#include <string>
#include <vector>

#include "app/gfx/native_widget_types.h"
#include "base/file_path.h"
#include "base/gfx/rect.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "third_party/npapi/bindings/npapi.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webplugin_delegate.h"

namespace NPAPI {
class PluginInstance;
}

// An implementation of WebPluginDelegate for Pepper in-process plugins.
class WebPluginDelegatePepperImpl : public webkit_glue::WebPluginDelegate {
 public:
  static WebPluginDelegatePepperImpl* Create(const FilePath& filename,
      const std::string& mime_type, gfx::PluginWindowHandle containing_view);

  static bool IsPluginDelegateWindow(gfx::NativeWindow window);
  static bool GetPluginNameFromWindow(gfx::NativeWindow window,
                                      std::wstring *plugin_name);

  // Returns true if the window handle passed in is that of the dummy
  // activation window for windowless plugins.
  static bool IsDummyActivationWindow(gfx::NativeWindow window);

  // WebPluginDelegate implementation
  virtual bool Initialize(const GURL& url,
                          const std::vector<std::string>& arg_names,
                          const std::vector<std::string>& arg_values,
                          webkit_glue::WebPlugin* plugin,
                          bool load_manually);
  virtual void PluginDestroyed();
  virtual void UpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect);
  virtual void Paint(gfx::NativeDrawingContext context, const gfx::Rect& rect);
  virtual void Print(gfx::NativeDrawingContext context);
  virtual void SetFocus();
  virtual bool HandleInputEvent(const WebKit::WebInputEvent& event,
                                WebKit::WebCursorInfo* cursor);
  virtual NPObject* GetPluginScriptableObject();
  virtual void DidFinishLoadWithReason(const GURL& url, NPReason reason,
                                       intptr_t notify_data);
  virtual int GetProcessId();
  virtual void SendJavaScriptStream(const GURL& url,
                                    const std::string& result,
                                    bool success, bool notify_needed,
                                    intptr_t notify_data);
  virtual void DidReceiveManualResponse(const GURL& url,
                                        const std::string& mime_type,
                                        const std::string& headers,
                                        uint32 expected_length,
                                        uint32 last_modified);
  virtual void DidReceiveManualData(const char* buffer, int length);
  virtual void DidFinishManualLoading();
  virtual void DidManualLoadFail();
  virtual void InstallMissingPlugin();
  virtual webkit_glue::WebPluginResourceClient* CreateResourceClient(
      int resource_id,
      const GURL& url,
      bool notify_needed,
      intptr_t notify_data,
      intptr_t stream);
  // End of WebPluginDelegate implementation.

  bool IsWindowless() const { return true; }
  gfx::Rect GetRect() const { return window_rect_; }
  gfx::Rect GetClipRect() const { return clip_rect_; }

  // Returns the path for the library implementing this plugin.
  FilePath GetPluginPath();

 private:
  friend class DeleteTask<WebPluginDelegatePepperImpl>;

  WebPluginDelegatePepperImpl(gfx::PluginWindowHandle containing_view,
                        NPAPI::PluginInstance *instance);
  ~WebPluginDelegatePepperImpl();

  //----------------------------
  // used for windowless plugins
  void WindowlessUpdateGeometry(const gfx::Rect& window_rect,
                                const gfx::Rect& clip_rect);
  void WindowlessPaint(gfx::NativeDrawingContext hdc, const gfx::Rect& rect);

  // Tells the plugin about the current state of the window.
  // See NPAPI NPP_SetWindow for more information.
  void WindowlessSetWindow(bool force_set_window);

  //-----------------------------------------
  // used for windowed and windowless plugins

  NPAPI::PluginInstance* instance() { return instance_.get(); }

  // Closes down and destroys our plugin instance.
  void DestroyInstance();

  webkit_glue::WebPlugin* plugin_;
  scoped_refptr<NPAPI::PluginInstance> instance_;

  gfx::PluginWindowHandle parent_;
  NPWindow window_;
  gfx::Rect window_rect_;
  gfx::Rect clip_rect_;
  std::vector<gfx::Rect> cutout_rects_;

  // The url with which the plugin was instantiated.
  std::string plugin_url_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginDelegatePepperImpl);
};

#endif  // WEBKIT_GLUE_PLUGIN_WEBPLUGIN_DELEGATE_PEPPER_IMPL_H_
