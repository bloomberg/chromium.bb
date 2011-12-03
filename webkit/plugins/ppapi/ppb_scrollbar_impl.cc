// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_scrollbar_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "ppapi/c/dev/ppp_scrollbar_dev.h"
#include "ppapi/thunk/thunk.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScrollbar.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/event_conversion.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using ppapi::thunk::PPB_Scrollbar_API;
using WebKit::WebInputEvent;
using WebKit::WebRect;
using WebKit::WebScrollbar;

namespace webkit {
namespace ppapi {

// static
PP_Resource PPB_Scrollbar_Impl::Create(PP_Instance instance,
                                       bool vertical) {
  scoped_refptr<PPB_Scrollbar_Impl> scrollbar(
      new PPB_Scrollbar_Impl(instance));
  scrollbar->Init(vertical);
  return scrollbar->GetReference();
}

PPB_Scrollbar_Impl::PPB_Scrollbar_Impl(PP_Instance instance)
    : PPB_Widget_Impl(instance),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

PPB_Scrollbar_Impl::~PPB_Scrollbar_Impl() {
}

void PPB_Scrollbar_Impl::Init(bool vertical) {
  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return;
  scrollbar_.reset(WebScrollbar::createForPlugin(
      vertical ? WebScrollbar::Vertical : WebScrollbar::Horizontal,
      ResourceHelper::GetPluginInstance(this)->container(),
      static_cast<WebKit::WebScrollbarClient*>(this)));
}

PPB_Scrollbar_API* PPB_Scrollbar_Impl::AsPPB_Scrollbar_API() {
  return this;
}

void PPB_Scrollbar_Impl::InstanceWasDeleted() {
  Resource::LastPluginRefWasDeleted();
  scrollbar_.reset();
}

uint32_t PPB_Scrollbar_Impl::GetThickness() {
  return WebScrollbar::defaultThickness();
}

bool PPB_Scrollbar_Impl::IsOverlay() {
  return scrollbar_->isOverlay();
}

uint32_t PPB_Scrollbar_Impl::GetValue() {
  return scrollbar_->value();
}

void PPB_Scrollbar_Impl::SetValue(uint32_t value) {
  if (scrollbar_.get())
    scrollbar_->setValue(value);
}

void PPB_Scrollbar_Impl::SetDocumentSize(uint32_t size) {
  if (scrollbar_.get())
    scrollbar_->setDocumentSize(size);
}

void PPB_Scrollbar_Impl::SetTickMarks(const PP_Rect* tick_marks,
                                      uint32_t count) {
  if (!scrollbar_.get())
    return;
  tickmarks_.resize(count);
  for (uint32 i = 0; i < count; ++i) {
    tickmarks_[i] = WebRect(tick_marks[i].point.x,
                            tick_marks[i].point.y,
                            tick_marks[i].size.width,
                            tick_marks[i].size.height);;
  }
  PP_Rect rect = location();
  Invalidate(&rect);
}

void PPB_Scrollbar_Impl::ScrollBy(PP_ScrollBy_Dev unit, int32_t multiplier) {
  if (!scrollbar_.get())
    return;

  WebScrollbar::ScrollDirection direction = multiplier >= 0 ?
      WebScrollbar::ScrollForward : WebScrollbar::ScrollBackward;
  float fmultiplier = 1.0;

  WebScrollbar::ScrollGranularity granularity;
  if (unit == PP_SCROLLBY_LINE) {
    granularity = WebScrollbar::ScrollByLine;
  } else if (unit == PP_SCROLLBY_PAGE) {
    granularity = WebScrollbar::ScrollByPage;
  } else if (unit == PP_SCROLLBY_DOCUMENT) {
    granularity = WebScrollbar::ScrollByDocument;
  } else {
    granularity = WebScrollbar::ScrollByPixel;
    fmultiplier = static_cast<float>(multiplier);
    if (fmultiplier < 0)
      fmultiplier *= -1;
  }
  scrollbar_->scroll(direction, granularity, fmultiplier);
}

PP_Bool PPB_Scrollbar_Impl::PaintInternal(const gfx::Rect& rect,
                                          PPB_ImageData_Impl* image) {
  ImageDataAutoMapper mapper(image);
  skia::PlatformCanvas* canvas = image->mapped_canvas();
  if (!canvas || !scrollbar_.get())
    return PP_FALSE;
  scrollbar_->paint(webkit_glue::ToWebCanvas(canvas), rect);

#if defined(OS_WIN)
  if (base::win::GetVersion() == base::win::VERSION_XP)
    skia::MakeOpaque(canvas, rect.x(), rect.y(), rect.width(), rect.height());
#endif

  return PP_TRUE;
}

PP_Bool PPB_Scrollbar_Impl::HandleEventInternal(
    const ::ppapi::InputEventData& data) {
  scoped_ptr<WebInputEvent> web_input_event(CreateWebInputEvent(data));
  if (!web_input_event.get() || !scrollbar_.get())
    return PP_FALSE;

  return PP_FromBool(scrollbar_->handleInputEvent(*web_input_event.get()));
}

void PPB_Scrollbar_Impl::SetLocationInternal(const PP_Rect* location) {
  if (!scrollbar_.get())
    return;
  scrollbar_->setLocation(WebRect(location->point.x,
                                  location->point.y,
                                  location->size.width,
                                  location->size.height));
}

void PPB_Scrollbar_Impl::valueChanged(WebKit::WebScrollbar* scrollbar) {
  PluginModule* plugin_module = ResourceHelper::GetPluginModule(this);
  if (!plugin_module)
    return;

  const PPP_Scrollbar_Dev* ppp_scrollbar =
      static_cast<const PPP_Scrollbar_Dev*>(plugin_module->GetPluginInterface(
          PPP_SCROLLBAR_DEV_INTERFACE));
  if (!ppp_scrollbar) {
    // Try the old version. This is ok because the old interface is a subset of
    // the new one, and ValueChanged didn't change.
    ppp_scrollbar =
        static_cast<const PPP_Scrollbar_Dev*>(plugin_module->GetPluginInterface(
            PPP_SCROLLBAR_DEV_INTERFACE_0_2));
    if (!ppp_scrollbar)
      return;
  }
  ppp_scrollbar->ValueChanged(pp_instance(), pp_resource(),
                              scrollbar_->value());
}

void PPB_Scrollbar_Impl::overlayChanged(WebScrollbar* scrollbar) {
  PluginModule* plugin_module = ResourceHelper::GetPluginModule(this);
  if (!plugin_module)
    return;

  const PPP_Scrollbar_Dev* ppp_scrollbar =
      static_cast<const PPP_Scrollbar_Dev*>(plugin_module->GetPluginInterface(
          PPP_SCROLLBAR_DEV_INTERFACE));
  if (!ppp_scrollbar)
    return;
  ppp_scrollbar->OverlayChanged(pp_instance(), pp_resource(),
                                PP_FromBool(IsOverlay()));
}

void PPB_Scrollbar_Impl::invalidateScrollbarRect(
    WebKit::WebScrollbar* scrollbar,
    const WebKit::WebRect& rect) {
  gfx::Rect gfx_rect(rect.x,
                     rect.y,
                     rect.width,
                     rect.height);
  dirty_ = dirty_.Union(gfx_rect);
  // Can't call into the client to tell them about the invalidate right away,
  // since the PPB_Scrollbar_Impl code is still in the middle of updating its
  // internal state.
  // Note: we use a WeakPtrFactory here so that a lingering callback can not
  // modify the lifetime of this object. Otherwise, WebKit::WebScrollbar could
  // outlive WebKit::WebPluginContainer, which is against its contract.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PPB_Scrollbar_Impl::NotifyInvalidate,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PPB_Scrollbar_Impl::getTickmarks(
    WebKit::WebScrollbar* scrollbar,
    WebKit::WebVector<WebKit::WebRect>* tick_marks) const {
  if (tickmarks_.empty()) {
    WebRect* rects = NULL;
    tick_marks->assign(rects, 0);
  } else {
    tick_marks->assign(&tickmarks_[0], tickmarks_.size());
  }
}

void PPB_Scrollbar_Impl::NotifyInvalidate() {
  if (dirty_.IsEmpty())
    return;
  PP_Rect pp_rect;
  pp_rect.point.x = dirty_.x();
  pp_rect.point.y = dirty_.y();
  pp_rect.size.width = dirty_.width();
  pp_rect.size.height = dirty_.height();
  dirty_ = gfx::Rect();
  Invalidate(&pp_rect);
}

}  // namespace ppapi
}  // namespace webkit

