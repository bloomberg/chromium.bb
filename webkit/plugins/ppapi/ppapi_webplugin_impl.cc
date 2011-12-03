// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppapi_webplugin_impl.h"

#include <cmath>

#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "ppapi/c/pp_var.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/plugins/ppapi/message_channel.h"
#include "webkit/plugins/ppapi/npobject_var.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_url_loader_impl.h"

using ppapi::NPObjectVar;
using WebKit::WebCanvas;
using WebKit::WebPluginContainer;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebRect;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebVector;
using WebKit::WebView;

namespace webkit {
namespace ppapi {

struct WebPluginImpl::InitData {
  scoped_refptr<PluginModule> module;
  base::WeakPtr<PluginDelegate> delegate;
  std::vector<std::string> arg_names;
  std::vector<std::string> arg_values;
  GURL url;
};

WebPluginImpl::WebPluginImpl(
    PluginModule* plugin_module,
    const WebPluginParams& params,
    const base::WeakPtr<PluginDelegate>& plugin_delegate)
    : init_data_(new InitData()),
      full_frame_(params.loadManually) {
  DCHECK(plugin_module);
  init_data_->module = plugin_module;
  init_data_->delegate = plugin_delegate;
  for (size_t i = 0; i < params.attributeNames.size(); ++i) {
    init_data_->arg_names.push_back(params.attributeNames[i].utf8());
    init_data_->arg_values.push_back(params.attributeValues[i].utf8());
  }
  init_data_->url = params.url;
}

WebPluginImpl::~WebPluginImpl() {
}

bool WebPluginImpl::initialize(WebPluginContainer* container) {
  // The plugin delegate may have gone away.
  if (!init_data_->delegate)
    return false;

  instance_ = init_data_->module->CreateInstance(init_data_->delegate);
  if (!instance_)
    return false;

  bool success = instance_->Initialize(container,
                                       init_data_->arg_names,
                                       init_data_->arg_values,
                                       init_data_->url,
                                       full_frame_);
  if (!success) {
    instance_->Delete();
    instance_ = NULL;
    return false;
  }

  init_data_.reset();
  return true;
}

void WebPluginImpl::destroy() {
  if (instance_) {
    instance_->Delete();
    instance_ = NULL;
  }

  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

NPObject* WebPluginImpl::scriptableObject() {
  scoped_refptr<NPObjectVar> object(
      NPObjectVar::FromPPVar(instance_->GetInstanceObject()));
  // GetInstanceObject talked to the plugin which may have removed the instance
  // from the DOM, in which case instance_ would be NULL now.
  if (!instance_)
    return NULL;

  // If there's an InstanceObject, tell the Instance's MessageChannel to pass
  // any non-postMessage calls to it.
  if (object) {
    instance_->message_channel().SetPassthroughObject(object->np_object());
  }
  NPObject* message_channel_np_object(instance_->message_channel().np_object());
  // The object is expected to be retained before it is returned.
  WebKit::WebBindings::retainObject(message_channel_np_object);
  return message_channel_np_object;
}

bool WebPluginImpl::getFormValue(WebString* value) {
  return false;
}

void WebPluginImpl::paint(WebCanvas* canvas, const WebRect& rect) {
  if (!instance_->FlashIsFullscreenOrPending())
    instance_->Paint(canvas, plugin_rect_, rect);
}

void WebPluginImpl::updateGeometry(
    const WebRect& window_rect,
    const WebRect& clip_rect,
    const WebVector<WebRect>& cut_outs_rects,
    bool is_visible) {
  plugin_rect_ = window_rect;
  if (!instance_->FlashIsFullscreenOrPending())
    instance_->ViewChanged(plugin_rect_, clip_rect);
}

void WebPluginImpl::updateFocus(bool focused) {
  instance_->SetWebKitFocus(focused);
}

void WebPluginImpl::updateVisibility(bool visible) {
}

bool WebPluginImpl::acceptsInputEvents() {
  return true;
}

bool WebPluginImpl::handleInputEvent(const WebKit::WebInputEvent& event,
                                     WebKit::WebCursorInfo& cursor_info) {
  if (instance_->FlashIsFullscreenOrPending())
    return false;
  return instance_->HandleInputEvent(event, &cursor_info);
}

void WebPluginImpl::didReceiveResponse(
    const WebKit::WebURLResponse& response) {
  DCHECK(!document_loader_);

  if (instance_->module()->is_crashed()) {
    // Don't create a resource for a crashed plugin.
    instance_->container()->element().document().frame()->stopLoading();
    return;
  }

  document_loader_ = new PPB_URLLoader_Impl(instance_->pp_instance(), true);
  document_loader_->didReceiveResponse(NULL, response);

  if (!instance_->HandleDocumentLoad(document_loader_))
    document_loader_ = NULL;
}

void WebPluginImpl::didReceiveData(const char* data, int data_length) {
  if (document_loader_)
    document_loader_->didReceiveData(NULL, data, data_length, data_length);
}

void WebPluginImpl::didFinishLoading() {
  if (document_loader_) {
    document_loader_->didFinishLoading(NULL, 0);
    document_loader_ = NULL;
  }
}

void WebPluginImpl::didFailLoading(const WebKit::WebURLError& error) {
  if (document_loader_) {
    document_loader_->didFail(NULL, error);
    document_loader_ = NULL;
  }
}

void WebPluginImpl::didFinishLoadingFrameRequest(const WebKit::WebURL& url,
                                                 void* notify_data) {
}

void WebPluginImpl::didFailLoadingFrameRequest(
    const WebKit::WebURL& url,
    void* notify_data,
    const WebKit::WebURLError& error) {
}

bool WebPluginImpl::hasSelection() const {
  return !selectionAsText().isEmpty();
}

WebString WebPluginImpl::selectionAsText() const {
  return instance_->GetSelectedText(false);
}

WebString WebPluginImpl::selectionAsMarkup() const {
  return instance_->GetSelectedText(true);
}

WebURL WebPluginImpl::linkAtPosition(const WebPoint& position) const {
  return GURL(instance_->GetLinkAtPosition(position));
}

void WebPluginImpl::setZoomLevel(double level, bool text_only) {
  instance_->Zoom(WebView::zoomLevelToZoomFactor(level), text_only);
}

bool WebPluginImpl::startFind(const WebKit::WebString& search_text,
                              bool case_sensitive,
                              int identifier) {
  return instance_->StartFind(search_text, case_sensitive, identifier);
}

void WebPluginImpl::selectFindResult(bool forward) {
  instance_->SelectFindResult(forward);
}

void WebPluginImpl::stopFind() {
  instance_->StopFind();
}

bool WebPluginImpl::supportsPaginatedPrint() {
  return instance_->SupportsPrintInterface();
}

bool WebPluginImpl::isPrintScalingDisabled() {
  return instance_->IsPrintScalingDisabled();
}

int WebPluginImpl::printBegin(const WebKit::WebRect& printable_area,
                              int printer_dpi) {
  return instance_->PrintBegin(printable_area, printer_dpi);
}

bool WebPluginImpl::printPage(int page_number,
                              WebKit::WebCanvas* canvas) {
  return instance_->PrintPage(page_number, canvas);
}

void WebPluginImpl::printEnd() {
  return instance_->PrintEnd();
}

}  // namespace ppapi
}  // namespace webkit
