// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_webplugin_impl.h"

#include "base/file_path.h"
#include "base/message_loop.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"

using WebKit::WebCanvas;
using WebKit::WebPluginContainer;
using WebKit::WebPluginParams;
using WebKit::WebRect;
using WebKit::WebVector;

namespace pepper {

WebPluginImpl::WebPluginImpl(
    WebKit::WebFrame* frame,
    const WebPluginParams& params,
    const base::WeakPtr<PluginDelegate>& plugin_delegate)
    : plugin_delegate_(plugin_delegate),
      container_(NULL) {
  for (size_t i = 0; i < params.attributeNames.size(); ++i) {
    arg_names_.push_back(params.attributeNames[i].utf8());
    arg_values_.push_back(params.attributeValues[i].utf8());
  }
}

WebPluginImpl::~WebPluginImpl() {
}

bool WebPluginImpl::initialize(WebPluginContainer* container) {
  // TODO(brettw) don't hardcode this!
  scoped_refptr<PluginModule> module = PluginModule::CreateModule(
      FilePath(FILE_PATH_LITERAL(
          "/usr/local/google/home/src3/src/out/Debug/lib.target/libppapi_example.so")));
  if (!module.get())
    return false;

  if (!plugin_delegate_)
    return false;
  instance_ = module->CreateInstance(plugin_delegate_);
  bool success = instance_->Initialize(arg_names_, arg_values_);
  if (!success) {
    instance_->Delete();
    instance_ = NULL;
    return false;
  }

  container_ = container;
  return true;
}

void WebPluginImpl::destroy() {
  container_ = NULL;

  if (instance_) {
    instance_->Delete();
    instance_ = NULL;
  }

  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

NPObject* WebPluginImpl::scriptableObject() {
  // TODO(brettw): implement getting this from the plugin instance.
  return NULL;
}

void WebPluginImpl::paint(WebCanvas* canvas, const WebRect& rect) {
  instance_->Paint(canvas, plugin_rect_, rect);
}

void WebPluginImpl::updateGeometry(
    const WebRect& window_rect,
    const WebRect& clip_rect,
    const WebVector<WebRect>& cut_outs_rects,
    bool is_visible) {
  plugin_rect_ = window_rect;
  instance_->ViewChanged(plugin_rect_, clip_rect);
}

void WebPluginImpl::updateFocus(bool focused) {
}

void WebPluginImpl::updateVisibility(bool visible) {
}

bool WebPluginImpl::acceptsInputEvents() {
  return true;
}

bool WebPluginImpl::handleInputEvent(const WebKit::WebInputEvent& event,
                                     WebKit::WebCursorInfo& cursor_info) {
  return instance_->HandleInputEvent(event, &cursor_info);
}

void WebPluginImpl::didReceiveResponse(
    const WebKit::WebURLResponse& response) {
}

void WebPluginImpl::didReceiveData(const char* data, int data_length) {
}

void WebPluginImpl::didFinishLoading() {
}

void WebPluginImpl::didFailLoading(const WebKit::WebURLError&) {
}

void WebPluginImpl::didFinishLoadingFrameRequest(const WebKit::WebURL& url,
                                                 void* notify_data) {
}

void WebPluginImpl::didFailLoadingFrameRequest(
    const WebKit::WebURL& url,
    void* notify_data,
    const WebKit::WebURLError& error) {
}

}  // namespace pepper
