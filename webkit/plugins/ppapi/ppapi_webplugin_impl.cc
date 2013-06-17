// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppapi_webplugin_impl.h"

#include <cmath>

#include "base/debug/crash_logging.h"
#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebPrintParams.h"
#include "third_party/WebKit/public/web/WebPrintScalingOption.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "webkit/plugins/ppapi/message_channel.h"
#include "webkit/plugins/ppapi/npobject_var.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

using ppapi::NPObjectVar;
using WebKit::WebCanvas;
using WebKit::WebPlugin;
using WebKit::WebPluginContainer;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebPrintParams;
using WebKit::WebRect;
using WebKit::WebSize;
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
      full_frame_(params.loadManually),
      instance_object_(PP_MakeUndefined()),
      container_(NULL) {
  DCHECK(plugin_module);
  init_data_->module = plugin_module;
  init_data_->delegate = plugin_delegate;
  for (size_t i = 0; i < params.attributeNames.size(); ++i) {
    init_data_->arg_names.push_back(params.attributeNames[i].utf8());
    init_data_->arg_values.push_back(params.attributeValues[i].utf8());
  }
  init_data_->url = params.url;

  // Set subresource URL for crash reporting.
  base::debug::SetCrashKeyValue("subresource_url", init_data_->url.spec());
}

WebPluginImpl::~WebPluginImpl() {
}

WebKit::WebPluginContainer* WebPluginImpl::container() const {
  return container_;
}

bool WebPluginImpl::initialize(WebPluginContainer* container) {
  // The plugin delegate may have gone away.
  if (!init_data_->delegate.get())
    return false;

  instance_ = init_data_->module
      ->CreateInstance(init_data_->delegate.get(), container, init_data_->url);
  if (!instance_.get())
    return false;

  // Enable script objects for this plugin.
  container->allowScriptObjects();

  bool success = instance_->Initialize(init_data_->arg_names,
                                       init_data_->arg_values,
                                       full_frame_);
  if (!success) {
    instance_->Delete();
    instance_ = NULL;

    WebKit::WebPlugin* replacement_plugin =
        init_data_->delegate->CreatePluginReplacement(
            init_data_->module->path());
    if (!replacement_plugin || !replacement_plugin->initialize(container))
      return false;

    container->setPlugin(replacement_plugin);
    return true;
  }

  init_data_.reset();
  container_ = container;
  return true;
}

void WebPluginImpl::destroy() {
  // Tell |container_| to clear references to this plugin's script objects.
  if (container_)
    container_->clearScriptObjects();

  if (instance_.get()) {
    ::ppapi::PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(instance_object_);
    instance_object_ = PP_MakeUndefined();
    instance_->Delete();
    instance_ = NULL;
  }

  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

NPObject* WebPluginImpl::scriptableObject() {
  // Call through the plugin to get its instance object. The plugin should pass
  // us a reference which we release in destroy().
  if (instance_object_.type == PP_VARTYPE_UNDEFINED)
    instance_object_ = instance_->GetInstanceObject();
  // GetInstanceObject talked to the plugin which may have removed the instance
  // from the DOM, in which case instance_ would be NULL now.
  if (!instance_.get())
    return NULL;

  scoped_refptr<NPObjectVar> object(NPObjectVar::FromPPVar(instance_object_));
  // If there's an InstanceObject, tell the Instance's MessageChannel to pass
  // any non-postMessage calls to it.
  if (object.get()) {
    instance_->message_channel().SetPassthroughObject(object->np_object());
  }
  NPObject* message_channel_np_object(instance_->message_channel().np_object());
  // The object is expected to be retained before it is returned.
  WebKit::WebBindings::retainObject(message_channel_np_object);
  return message_channel_np_object;
}

NPP WebPluginImpl::pluginNPP() {
  return instance_->instanceNPP();
}

bool WebPluginImpl::getFormValue(WebString& value) {
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
  if (!instance_->FlashIsFullscreenOrPending()) {
    std::vector<gfx::Rect> cut_outs;
    for (size_t i = 0; i < cut_outs_rects.size(); ++i)
      cut_outs.push_back(cut_outs_rects[i]);
    instance_->ViewChanged(plugin_rect_, clip_rect, cut_outs);
  }
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
  DCHECK(!instance_->document_loader());
  instance_->HandleDocumentLoad(response);
}

void WebPluginImpl::didReceiveData(const char* data, int data_length) {
  WebKit::WebURLLoaderClient* document_loader = instance_->document_loader();
  if (document_loader)
    document_loader->didReceiveData(NULL, data, data_length, 0);
}

void WebPluginImpl::didFinishLoading() {
  WebKit::WebURLLoaderClient* document_loader = instance_->document_loader();
  if (document_loader)
    document_loader->didFinishLoading(NULL, 0.0);
}

void WebPluginImpl::didFailLoading(const WebKit::WebURLError& error) {
  WebKit::WebURLLoaderClient* document_loader = instance_->document_loader();
  if (document_loader)
    document_loader->didFail(NULL, error);
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

int WebPluginImpl::printBegin(const WebPrintParams& print_params) {
  return instance_->PrintBegin(print_params);
}

bool WebPluginImpl::printPage(int page_number,
                              WebKit::WebCanvas* canvas) {
  return instance_->PrintPage(page_number, canvas);
}

void WebPluginImpl::printEnd() {
  return instance_->PrintEnd();
}

bool WebPluginImpl::canRotateView() {
  return instance_->CanRotateView();
}

void WebPluginImpl::rotateView(RotationType type) {
  instance_->RotateView(type);
}

bool WebPluginImpl::isPlaceholder() {
  return false;
}

}  // namespace ppapi
}  // namespace webkit
