// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_scrollbar.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/ppapi/c/dev/ppp_scrollbar_dev.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScrollbar.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
#include "webkit/glue/plugins/pepper_event_conversion.h"
#include "webkit/glue/plugins/pepper_image_data.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using WebKit::WebInputEvent;
using WebKit::WebRect;
using WebKit::WebScrollbar;

namespace pepper {

namespace {

PP_Resource Create(PP_Instance instance_id, bool vertical) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  scoped_refptr<Scrollbar> scrollbar(new Scrollbar(instance, vertical));
  return scrollbar->GetReference();
}

bool IsScrollbar(PP_Resource resource) {
  return !!Resource::GetAs<Scrollbar>(resource);
}

uint32_t GetThickness() {
  return WebScrollbar::defaultThickness();
}

uint32_t GetValue(PP_Resource resource) {
  scoped_refptr<Scrollbar> scrollbar(Resource::GetAs<Scrollbar>(resource));
  if (!scrollbar)
    return 0;
  return scrollbar->GetValue();
}

void SetValue(PP_Resource resource, uint32_t value) {
  scoped_refptr<Scrollbar> scrollbar(Resource::GetAs<Scrollbar>(resource));
  if (scrollbar)
    scrollbar->SetValue(value);
}

void SetDocumentSize(PP_Resource resource, uint32_t size) {
  scoped_refptr<Scrollbar> scrollbar(Resource::GetAs<Scrollbar>(resource));
  if (scrollbar)
    scrollbar->SetDocumentSize(size);
}

void SetTickMarks(PP_Resource resource,
                  const PP_Rect* tick_marks,
                  uint32_t count) {
  scoped_refptr<Scrollbar> scrollbar(Resource::GetAs<Scrollbar>(resource));
  if (scrollbar)
    scrollbar->SetTickMarks(tick_marks, count);
}

void ScrollBy(PP_Resource resource, PP_ScrollBy_Dev unit, int32_t multiplier) {
  scoped_refptr<Scrollbar> scrollbar(Resource::GetAs<Scrollbar>(resource));
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

Scrollbar::Scrollbar(PluginInstance* instance, bool vertical)
    : Widget(instance) {
  scrollbar_.reset(WebScrollbar::create(
      static_cast<WebKit::WebScrollbarClient*>(this),
      vertical ? WebScrollbar::Vertical : WebScrollbar::Horizontal));
}

Scrollbar::~Scrollbar() {
}

// static
const PPB_Scrollbar_Dev* Scrollbar::GetInterface() {
  return &ppb_scrollbar;
}

uint32_t Scrollbar::GetValue() {
  return scrollbar_->value();
}

void Scrollbar::SetValue(uint32_t value) {
  scrollbar_->setValue(value);
}

void Scrollbar::SetDocumentSize(uint32_t size) {
  scrollbar_->setDocumentSize(size);
}

void Scrollbar::SetTickMarks(const PP_Rect* tick_marks, uint32_t count) {
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

void Scrollbar::ScrollBy(PP_ScrollBy_Dev unit, int32_t multiplier) {
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

bool Scrollbar::Paint(const PP_Rect* rect, ImageData* image) {
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
    canvas->getTopPlatformDevice().makeOpaque(
        gfx_rect.x(), gfx_rect.y(), gfx_rect.width(), gfx_rect.height());
  }
#endif

  return true;
}

bool Scrollbar::HandleEvent(const PP_InputEvent* event) {
  scoped_ptr<WebInputEvent> web_input_event(CreateWebInputEvent(*event));
  if (!web_input_event.get())
    return false;

  return scrollbar_->handleInputEvent(*web_input_event.get());
}

void Scrollbar::SetLocationInternal(const PP_Rect* location) {
  scrollbar_->setLocation(WebRect(location->point.x,
                                  location->point.y,
                                  location->size.width,
                                  location->size.height));
}

void Scrollbar::valueChanged(WebKit::WebScrollbar* scrollbar) {
  const PPP_Scrollbar_Dev* ppp_scrollbar =
      static_cast<const PPP_Scrollbar_Dev*>(
          module()->GetPluginInterface(PPP_SCROLLBAR_DEV_INTERFACE));
  if (!ppp_scrollbar)
    return;
  ScopedResourceId resource(this);
  ppp_scrollbar->ValueChanged(
      instance()->pp_instance(), resource.id, scrollbar_->value());
}

void Scrollbar::invalidateScrollbarRect(WebKit::WebScrollbar* scrollbar,
                                        const WebKit::WebRect& rect) {
  gfx::Rect gfx_rect(rect.x,
                     rect.y,
                     rect.width,
                     rect.height);
  dirty_ = dirty_.Union(gfx_rect);
  // Can't call into the client to tell them about the invalidate right away,
  // since the Scrollbar code is still in the middle of updating its internal
  // state.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &Scrollbar::NotifyInvalidate));
}

void Scrollbar::getTickmarks(
    WebKit::WebScrollbar* scrollbar,
    WebKit::WebVector<WebKit::WebRect>* tick_marks) const {
  if (tickmarks_.empty()) {
    WebRect* rects = NULL;
    tick_marks->assign(rects, 0);
  } else {
    tick_marks->assign(&tickmarks_[0], tickmarks_.size());
  }
}

void Scrollbar::NotifyInvalidate() {
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

}  // namespace pepper
