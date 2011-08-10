// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_scrollbar_impl.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "ppapi/c/dev/ppp_scrollbar_dev.h"
#include "ppapi/thunk/thunk.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScrollbar.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/event_conversion.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
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

namespace {

// Version 0.3 implementation --------------------------------------------------
//
// TODO(brettw) remove this when we remove support for version 0.3 interface.
// This just forwards everything to the new version of the interface except for
// the GetThickness call which has no parameters.

PP_Resource Create(PP_Instance instance, PP_Bool vertical) {
  return ::ppapi::thunk::GetPPB_Scrollbar_Thunk()->Create(instance, vertical);
}

PP_Bool IsScrollbar(PP_Resource resource) {
  return ::ppapi::thunk::GetPPB_Scrollbar_Thunk()->IsScrollbar(resource);
}

uint32_t GetThickness3() {
  return WebScrollbar::defaultThickness();
}

uint32_t GetThickness4(PP_Resource resource) {
  return ::ppapi::thunk::GetPPB_Scrollbar_Thunk()->GetThickness(resource);
}

uint32_t GetValue(PP_Resource resource) {
  return ::ppapi::thunk::GetPPB_Scrollbar_Thunk()->GetValue(resource);
}

void SetValue(PP_Resource resource, uint32_t value) {
  return ::ppapi::thunk::GetPPB_Scrollbar_Thunk()->SetValue(resource, value);
}

void SetDocumentSize(PP_Resource resource, uint32_t size) {
  return ::ppapi::thunk::GetPPB_Scrollbar_Thunk()->SetDocumentSize(resource,
                                                                   size);
}

void SetTickMarks(PP_Resource resource,
                  const PP_Rect* tick_marks,
                  uint32_t count) {
  return ::ppapi::thunk::GetPPB_Scrollbar_Thunk()->SetTickMarks(resource,
                                                                tick_marks,
                                                                count);
}

void ScrollBy(PP_Resource resource, PP_ScrollBy_Dev unit, int32_t multiplier) {
  return ::ppapi::thunk::GetPPB_Scrollbar_Thunk()->ScrollBy(resource,
                                                            unit,
                                                            multiplier);
}

const PPB_Scrollbar_0_3_Dev ppb_scrollbar_0_3 = {
  &Create,
  &IsScrollbar,
  &GetThickness3,
  &GetValue,
  &SetValue,
  &SetDocumentSize,
  &SetTickMarks,
  &ScrollBy
};

const PPB_Scrollbar_0_4_Dev ppb_scrollbar_0_4 = {
  &Create,
  &IsScrollbar,
  &GetThickness4,
  &GetValue,
  &SetValue,
  &SetDocumentSize,
  &SetTickMarks,
  &ScrollBy
};

}  // namespace

// static
PP_Resource PPB_Scrollbar_Impl::Create(PluginInstance* instance,
                                       bool vertical) {
  scoped_refptr<PPB_Scrollbar_Impl> scrollbar(
      new PPB_Scrollbar_Impl(instance));
  scrollbar->Init(vertical);
  return scrollbar->GetReference();
}

PPB_Scrollbar_Impl::PPB_Scrollbar_Impl(PluginInstance* instance)
    : PPB_Widget_Impl(instance) {
}

PPB_Scrollbar_Impl::~PPB_Scrollbar_Impl() {
}

void PPB_Scrollbar_Impl::Init(bool vertical) {
#if defined(WEBSCROLLBAR_SUPPORTS_OVERLAY)
  scrollbar_.reset(WebScrollbar::createForPlugin(
      vertical ? WebScrollbar::Vertical : WebScrollbar::Horizontal,
      instance()->container(),
      static_cast<WebKit::WebScrollbarClient*>(this)));
#else
  scrollbar_.reset(WebScrollbar::create(
      static_cast<WebKit::WebScrollbarClient*>(this),
      vertical ? WebScrollbar::Vertical : WebScrollbar::Horizontal));
#endif
}

PPB_Scrollbar_API* PPB_Scrollbar_Impl::AsPPB_Scrollbar_API() {
  return this;
}

// static
const PPB_Scrollbar_0_3_Dev* PPB_Scrollbar_Impl::Get0_3Interface() {
  return &ppb_scrollbar_0_3;
}

// static
const PPB_Scrollbar_0_4_Dev* PPB_Scrollbar_Impl::Get0_4Interface() {
  return &ppb_scrollbar_0_4;
}

uint32_t PPB_Scrollbar_Impl::GetThickness() {
  return WebScrollbar::defaultThickness();
}

bool PPB_Scrollbar_Impl::IsOverlay() {
// TODO(jam): take this out once WebKit is rolled.
#if defined(WEBSCROLLBAR_SUPPORTS_OVERLAY)
  return scrollbar_->isOverlay();
#else
  return false;
#endif
}

uint32_t PPB_Scrollbar_Impl::GetValue() {
  return scrollbar_->value();
}

void PPB_Scrollbar_Impl::SetValue(uint32_t value) {
  scrollbar_->setValue(value);
}

void PPB_Scrollbar_Impl::SetDocumentSize(uint32_t size) {
  scrollbar_->setDocumentSize(size);
}

void PPB_Scrollbar_Impl::SetTickMarks(const PP_Rect* tick_marks,
                                      uint32_t count) {
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
  if (!canvas)
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
  if (!web_input_event.get())
    return PP_FALSE;

  return PP_FromBool(scrollbar_->handleInputEvent(*web_input_event.get()));
}

void PPB_Scrollbar_Impl::SetLocationInternal(const PP_Rect* location) {
  scrollbar_->setLocation(WebRect(location->point.x,
                                  location->point.y,
                                  location->size.width,
                                  location->size.height));
}

void PPB_Scrollbar_Impl::valueChanged(WebKit::WebScrollbar* scrollbar) {
  const PPP_Scrollbar_Dev* ppp_scrollbar =
      static_cast<const PPP_Scrollbar_Dev*>(instance()->module()->
          GetPluginInterface(PPP_SCROLLBAR_DEV_INTERFACE));
  if (!ppp_scrollbar) {
    // Try the old version. This is ok because the old interface is a subset of
    // the new one, and ValueChanged didn't change.
    ppp_scrollbar =
      static_cast<const PPP_Scrollbar_Dev*>(instance()->module()->
          GetPluginInterface(PPP_SCROLLBAR_DEV_INTERFACE_0_2));
    if (!ppp_scrollbar)
      return;
  }
  ScopedResourceId resource(this);
  ppp_scrollbar->ValueChanged(
      instance()->pp_instance(), resource.id, scrollbar_->value());
}

void PPB_Scrollbar_Impl::overlayChanged(WebScrollbar* scrollbar) {
  const PPP_Scrollbar_Dev* ppp_scrollbar =
      static_cast<const PPP_Scrollbar_Dev*>(instance()->module()->
          GetPluginInterface(PPP_SCROLLBAR_DEV_INTERFACE));
  if (!ppp_scrollbar)
    return;
  ScopedResourceId resource(this);
  ppp_scrollbar->OverlayChanged(
      instance()->pp_instance(), resource.id,
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
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &PPB_Scrollbar_Impl::NotifyInvalidate));
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

