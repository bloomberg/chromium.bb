// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_SCROLLBAR_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_SCROLLBAR_H_

#include <vector>

#include "gfx/rect.h"
#include "third_party/ppapi/c/ppb_scrollbar.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScrollbarClient.h"
#include "webkit/glue/plugins/pepper_widget.h"

typedef struct _ppb_Scrollbar PPB_Scrollbar;

namespace pepper {

class PluginInstance;

class Scrollbar : public Widget, public WebKit::WebScrollbarClient {
 public:
  Scrollbar(PluginInstance* instance, bool vertical);
  virtual ~Scrollbar();

  // Returns a pointer to the interface implementing PPB_Scrollbar that is
  // exposed to the plugin.
  static const PPB_Scrollbar* GetInterface();

  // Resource overrides.
  Scrollbar* AsScrollbar() { return this; }

  // PPB_Scrollbar implementation.
  uint32_t GetValue();
  void SetValue(uint32_t value);
  void SetDocumentSize(uint32_t size);
  void SetTickMarks(const PP_Rect* tick_marks, uint32_t count);
  void ScrollBy(PP_ScrollBy unit, int32_t multiplier);

  // PPB_Widget implementation.
  virtual bool Paint(const PP_Rect* rect, ImageData* image);
  virtual bool HandleEvent(const PP_Event* event);
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
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_SCROLLBAR_H_
