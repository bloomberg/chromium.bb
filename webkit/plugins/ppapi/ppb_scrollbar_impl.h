// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_SCROLLBAR_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_SCROLLBAR_IMPL_H_

#include <vector>

#include "ppapi/c/dev/ppb_scrollbar_dev.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScrollbarClient.h"
#include "ui/gfx/rect.h"
#include "webkit/plugins/ppapi/ppb_widget_impl.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_Scrollbar_Impl : public PPB_Widget_Impl,
                           public WebKit::WebScrollbarClient {
 public:
  PPB_Scrollbar_Impl(PluginInstance* instance, bool vertical);
  virtual ~PPB_Scrollbar_Impl();

  // Returns a pointer to the interface implementing PPB_Scrollbar that is
  // exposed to the plugin.
  static const PPB_Scrollbar_Dev* GetInterface();

  // Resource overrides.
  virtual PPB_Scrollbar_Impl* AsPPB_Scrollbar_Impl();

  // PPB_Scrollbar implementation.
  uint32_t GetValue();
  void SetValue(uint32_t value);
  void SetDocumentSize(uint32_t size);
  void SetTickMarks(const PP_Rect* tick_marks, uint32_t count);
  void ScrollBy(PP_ScrollBy_Dev unit, int32_t multiplier);

  // PPB_Widget implementation.
  virtual bool Paint(const PP_Rect* rect, PPB_ImageData_Impl* image);
  virtual bool HandleEvent(const PP_InputEvent* event);
  virtual void SetLocationInternal(const PP_Rect* location);

 private:
  // WebKit::WebScrollbarClient implementation.
  virtual void valueChanged(WebKit::WebScrollbar* scrollbar);
  virtual void invalidateScrollbarRect(WebKit::WebScrollbar* scrollbar,
                                       const WebKit::WebRect& rect);
  virtual void getTickmarks(
      WebKit::WebScrollbar* scrollbar,
      WebKit::WebVector<WebKit::WebRect>* tick_marks) const;

  void NotifyInvalidate();

  gfx::Rect dirty_;
  std::vector<WebKit::WebRect> tickmarks_;
  scoped_ptr<WebKit::WebScrollbar> scrollbar_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Scrollbar_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_SCROLLBAR_IMPL_H_
