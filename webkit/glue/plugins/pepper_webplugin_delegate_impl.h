// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_WEBPLUGIN_DELEGATE_IMPL_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_WEBPLUGIN_DELEGATE_IMPL_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "gfx/rect.h"
#include "webkit/glue/plugins/webplugin_delegate.h"

namespace pepper {

class DeviceContext2D;
class PluginDelegate;
class PluginInstance;

class WebPluginDelegateImpl : public webkit_glue::WebPluginDelegate {
 public:
  virtual ~WebPluginDelegateImpl();

  static WebPluginDelegateImpl* Create(PluginDelegate* delegate,
                                       const FilePath& filename);

  // webkit_glue::WebPluginDelegate implementation.
  virtual bool Initialize(const GURL& url,
                          const std::vector<std::string>& arg_names,
                          const std::vector<std::string>& arg_values,
                          webkit_glue::WebPlugin* plugin,
                          bool load_manually);
  virtual void PluginDestroyed();
  virtual void UpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect);
  virtual void Paint(WebKit::WebCanvas* canvas, const gfx::Rect& rect);
  virtual void Print(gfx::NativeDrawingContext hdc);
  virtual void SetFocus(bool focused);
  virtual bool HandleInputEvent(const WebKit::WebInputEvent& event,
                                WebKit::WebCursorInfo* cursor);
  virtual NPObject* GetPluginScriptableObject();
  virtual void DidFinishLoadWithReason(const GURL& url, NPReason reason,
                                       int notify_id);
  virtual int GetProcessId();
  virtual void SendJavaScriptStream(const GURL& url,
                                    const std::string& result,
                                    bool success,
                                    int notify_id);
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
      unsigned long resource_id,
      const GURL& url,
      int notify_id);
  virtual webkit_glue::WebPluginResourceClient* CreateSeekableResourceClient(
      unsigned long resource_id, int range_request_id);
  virtual bool SupportsFind();
  virtual void StartFind(const std::string& search_text,
                         bool case_sensitive,
                         int identifier);
  virtual void SelectFindResult(bool forward);
  virtual void StopFind();
  virtual void NumberOfFindResultsChanged(int total, bool final_result);
  virtual void SelectedFindResultChanged(int index);
  virtual void Zoom(int factor);

 private:
  WebPluginDelegateImpl(PluginInstance* instance);

  scoped_refptr<PluginInstance> instance_;
  webkit_glue::WebPlugin* web_plugin_;

  gfx::Rect window_rect_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginDelegateImpl);
};

}  // namespace

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_WEBPLUGIN_DELEGATE_IMPL_H_
