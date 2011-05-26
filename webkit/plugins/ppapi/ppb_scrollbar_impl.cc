// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_scrollbar_impl.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "ppapi/c/dev/ppp_scrollbar_dev.h"
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

using WebKit::WebInputEvent;
using WebKit::WebRect;
using WebKit::WebScrollbar;

namespace webkit {
namespace ppapi {

namespace {

PP_Resource Create(PP_Instance instance_id, PP_Bool vertical) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  scoped_refptr<PPB_Scrollbar_Impl> scrollbar(
      new PPB_Scrollbar_Impl(instance, PPBoolToBool(vertical)));
  return scrollbar->GetReference();
}

PP_Bool IsScrollbar(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_Scrollbar_Impl>(resource));
}

uint32_t GetThickness() {
  return WebScrollbar::defaultThickness();
}

uint32_t GetValue(PP_Resource resource) {
  scoped_refptr<PPB_Scrollbar_Impl> scrollbar(
      Resource::GetAs<PPB_Scrollbar_Impl>(resource));
  if (!scrollbar)
    return 0;
  return scrollbar->GetValue();
}

void SetValue(PP_Resource resource, uint32_t value) {
  scoped_refptr<PPB_Scrollbar_Impl> scrollbar(
      Resource::GetAs<PPB_Scrollbar_Impl>(resource));
  if (scrollbar)
    scrollbar->SetValue(value);
}

void SetDocumentSize(PP_Resource resource, uint32_t size) {
  scoped_refptr<PPB_Scrollbar_Impl> scrollbar(
      Resource::GetAs<PPB_Scrollbar_Impl>(resource));
  if (scrollbar)
    scrollbar->SetDocumentSize(size);
}

void SetTickMarks(PP_Resource resource,
                  const PP_Rect* tick_marks,
                  uint32_t count) {
  scoped_refptr<PPB_Scrollbar_Impl> scrollbar(
      Resource::GetAs<PPB_Scrollbar_Impl>(resource));
  if (scrollbar)
    scrollbar->SetTickMarks(tick_marks, count);
}

void ScrollBy(PP_Resource resource, PP_ScrollBy_Dev unit, int32_t multiplier) {
  scoped_refptr<PPB_Scrollbar_Impl> scrollbar(
      Resource::GetAs<PPB_Scrollbar_Impl>(resource));
  if (scrollbar)
    scrollbar->ScrollBy(unit, multiplier);
}

const PPB_Scrollbar_Dev ppb_scrollbar = {
  &Create,
  &IsScrollbar,
  &GetThickness,
  &GetValue,
  &SetValue,
  &SetDocumentSize,
  &SetTickMarks,
  &ScrollBy
};

}  // namespace

PPB_Scrollbar_Impl::PPB_Scrollbar_Impl(PluginInstance* instance, bool vertical)
    : PPB_Widget_Impl(instance) {
  scrollbar_.reset(WebScrollbar::create(
      static_cast<WebKit::WebScrollbarClient*>(this),
      vertical ? WebScrollbar::Vertical : WebScrollbar::Horizontal));
}

PPB_Scrollbar_Impl::~PPB_Scrollbar_Impl() {
}

// static
const PPB_Scrollbar_Dev* PPB_Scrollbar_Impl::GetInterface() {
  return &ppb_scrollbar;
}

PPB_Scrollbar_Impl* PPB_Scrollbar_Impl::AsPPB_Scrollbar_Impl() {
  return this;
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

bool PPB_Scrollbar_Impl::Paint(const PP_Rect* rect, PPB_ImageData_Impl* image) {
  gfx::Rect gfx_rect(rect->point.x,
                     rect->point.y,
                     rect->size.width,
                     rect->size.height);
  skia::PlatformCanvas* canvas = image->mapped_canvas();
  if (!canvas)
    return false;
  scrollbar_->paint(webkit_glue::ToWebCanvas(canvas), gfx_rect);

#if defined(OS_WIN)
  if (base::win::GetVersion() == base::win::VERSION_XP) {
    skia::MakeOpaque(canvas, gfx_rect.x(), gfx_rect.y(),
                     gfx_rect.width(), gfx_rect.height());
  }
#endif

  return true;
}

bool PPB_Scrollbar_Impl::HandleEvent(const PP_InputEvent* event) {
  scoped_ptr<WebInputEvent> web_input_event(CreateWebInputEvent(*event));
  if (!web_input_event.get())
    return false;

  return scrollbar_->handleInputEvent(*web_input_event.get());
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
  if (!ppp_scrollbar)
    return;
  ScopedResourceId resource(this);
  ppp_scrollbar->ValueChanged(
      instance()->pp_instance(), resource.id, scrollbar_->value());
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

