// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_webplugin_delegate_impl.h"

#include "base/logging.h"
#include "base/ref_counted.h"
#include "webkit/glue/plugins/pepper_device_context_2d.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/webplugin.h"

namespace pepper {

WebPluginDelegateImpl::WebPluginDelegateImpl(PluginInstance* instance)
    : instance_(instance),
      web_plugin_(NULL) {
}

WebPluginDelegateImpl::~WebPluginDelegateImpl() {
}

// static
WebPluginDelegateImpl* WebPluginDelegateImpl::Create(PluginDelegate* delegate,
                                                     const FilePath& filename) {
  scoped_refptr<PluginModule> module = PluginModule::CreateModule(filename);
  if (!module.get())
    return NULL;

  scoped_refptr<PluginInstance> instance = module->CreateInstance(delegate);
  return new WebPluginDelegateImpl(instance.get());
}

bool WebPluginDelegateImpl::Initialize(
    const GURL& url,
    const std::vector<std::string>& arg_names,
    const std::vector<std::string>& arg_values,
    webkit_glue::WebPlugin* plugin,
    bool load_manually) {
  web_plugin_ = plugin;

  if (!instance_->Initialize(arg_names, arg_values)) {
    LOG(WARNING) << "Plugin instance initialization failed.";
    return false;
  }

  // Declare we're in Windowless mode to WebKit.
  web_plugin_->SetWindow(0);
  return true;
}

void WebPluginDelegateImpl::PluginDestroyed() {
  // TODO(brettw) we may need something like in the NPAPI version that checks
  // for reentrancy and doesn't delete until later.
  instance_->Delete();
  delete this;
}

void WebPluginDelegateImpl::UpdateGeometry(const gfx::Rect& window_rect,
                                           const gfx::Rect& clip_rect) {
  window_rect_ = window_rect;
  instance_->ViewChanged(window_rect, clip_rect);
}

void WebPluginDelegateImpl::Paint(WebKit::WebCanvas* canvas,
                                  const gfx::Rect& rect) {
  instance_->Paint(canvas, window_rect_, rect);
}

void WebPluginDelegateImpl::Print(gfx::NativeDrawingContext hdc) {
}

void WebPluginDelegateImpl::SetFocus(bool focused) {
}

bool WebPluginDelegateImpl::HandleInputEvent(const WebKit::WebInputEvent& event,
                                             WebKit::WebCursorInfo* cursor) {
  return instance_->HandleInputEvent(event, cursor);
}

NPObject* WebPluginDelegateImpl::GetPluginScriptableObject() {
  return NULL;
}

void WebPluginDelegateImpl::DidFinishLoadWithReason(const GURL& url,
                                                    NPReason reason,
                                                    int notify_id) {
}

int WebPluginDelegateImpl::GetProcessId() {
  return -1;  // TODO(brettw) work out what this should do.
}

void WebPluginDelegateImpl::SendJavaScriptStream(const GURL& url,
                                                 const std::string& result,
                                                 bool success,
                                                 int notify_id) {
}

void WebPluginDelegateImpl::DidReceiveManualResponse(
    const GURL& url,
    const std::string& mime_type,
    const std::string& headers,
    uint32 expected_length,
    uint32 last_modified) {
}

void WebPluginDelegateImpl::DidReceiveManualData(const char* buffer,
                                                 int length) {
}

void WebPluginDelegateImpl::DidFinishManualLoading() {
}

void WebPluginDelegateImpl::DidManualLoadFail() {
}

void WebPluginDelegateImpl::InstallMissingPlugin() {
}

webkit_glue::WebPluginResourceClient*
WebPluginDelegateImpl::CreateResourceClient(unsigned long resource_id,
                                            const GURL& url,
                                            int notify_id) {
  return NULL;
}

webkit_glue::WebPluginResourceClient*
WebPluginDelegateImpl::CreateSeekableResourceClient(unsigned long resource_id,
                                                    int range_request_id) {
  return NULL;
}

bool WebPluginDelegateImpl::SupportsFind() {
  return false;
}

void WebPluginDelegateImpl::StartFind(const std::string& search_text,
                                      bool case_sensitive,
                                      int identifier) {
}

void WebPluginDelegateImpl::SelectFindResult(bool forward) {
}

void WebPluginDelegateImpl::StopFind() {
}

void WebPluginDelegateImpl::NumberOfFindResultsChanged(int total,
                                                       bool final_result) {
}

void WebPluginDelegateImpl::SelectedFindResultChanged(int index) {
}

void WebPluginDelegateImpl::Zoom(int factor) {
}

}  // namespace pepper
