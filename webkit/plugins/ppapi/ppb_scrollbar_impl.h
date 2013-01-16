// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_SCROLLBAR_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_SCROLLBAR_IMPL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/thunk/ppb_scrollbar_api.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginScrollbarClient.h"
#include "ui/gfx/rect.h"
#include "webkit/plugins/ppapi/ppb_widget_impl.h"

namespace webkit {
namespace ppapi {

class PPB_Scrollbar_Impl : public PPB_Widget_Impl,
                           public ::ppapi::thunk::PPB_Scrollbar_API,
                           public WebKit::WebPluginScrollbarClient {
 public:
  static PP_Resource Create(PP_Instance instance, bool vertical);

  virtual ~PPB_Scrollbar_Impl();

  // Resource overrides.
  virtual PPB_Scrollbar_API* AsPPB_Scrollbar_API() OVERRIDE;
  virtual void InstanceWasDeleted();

  // PPB_Scrollbar_API implementation.
  virtual uint32_t GetThickness() OVERRIDE;
  virtual bool IsOverlay() OVERRIDE;
  virtual uint32_t GetValue() OVERRIDE;
  virtual void SetValue(uint32_t value) OVERRIDE;
  virtual void SetDocumentSize(uint32_t size) OVERRIDE;
  virtual void SetTickMarks(const PP_Rect* tick_marks, uint32_t count) OVERRIDE;
  virtual void ScrollBy(PP_ScrollBy_Dev unit, int32_t multiplier) OVERRIDE;

 private:
  explicit PPB_Scrollbar_Impl(PP_Instance instance);
  void Init(bool vertical);

  // PPB_Widget private implementation.
  virtual PP_Bool PaintInternal(const gfx::Rect& rect,
                                PPB_ImageData_Impl* image) OVERRIDE;
  virtual PP_Bool HandleEventInternal(
      const ::ppapi::InputEventData& data) OVERRIDE;
  virtual void SetLocationInternal(const PP_Rect* location) OVERRIDE;

  // WebKit::WebPluginScrollbarClient implementation.
  virtual void valueChanged(WebKit::WebPluginScrollbar* scrollbar) OVERRIDE;
  virtual void overlayChanged(WebKit::WebPluginScrollbar* scrollbar) OVERRIDE;
  virtual void invalidateScrollbarRect(WebKit::WebPluginScrollbar* scrollbar,
                                       const WebKit::WebRect& rect) OVERRIDE;
  virtual void getTickmarks(
      WebKit::WebPluginScrollbar* scrollbar,
      WebKit::WebVector<WebKit::WebRect>* tick_marks) const OVERRIDE;

  void NotifyInvalidate();

  gfx::Rect dirty_;
  std::vector<WebKit::WebRect> tickmarks_;
  scoped_ptr<WebKit::WebPluginScrollbar> scrollbar_;

  // Used so that the post task for Invalidate doesn't keep an extra reference.
  base::WeakPtrFactory<PPB_Scrollbar_Impl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Scrollbar_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_SCROLLBAR_IMPL_H_
