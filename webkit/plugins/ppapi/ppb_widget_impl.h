// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_WIDGET_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_WIDGET_IMPL_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "ppapi/c/pp_rect.h"
#include "webkit/plugins/ppapi/resource.h"

struct PPB_Widget_Dev;
struct PP_InputEvent;

namespace webkit {
namespace ppapi {

class PPB_ImageData_Impla;
class PluginInstance;

class PPB_Widget_Impl : public Resource  {
 public:
  explicit PPB_Widget_Impl(PluginInstance* instance);
  virtual ~PPB_Widget_Impl();

  // Returns a pointer to the interface implementing PPB_Widget that is
  // exposed to the plugin.
  static const PPB_Widget_Dev* GetInterface();

  // Resource overrides.
  virtual PPB_Widget_Impl* AsPPB_Widget_Impl();

  // PPB_Widget implementation.
  virtual bool Paint(const PP_Rect* rect, PPB_ImageData_Impl* image) = 0;
  virtual bool HandleEvent(const PP_InputEvent* event) = 0;
  bool GetLocation(PP_Rect* location);
  void SetLocation(const PP_Rect* location);

  // Notifies the plugin instance that the given rect needs to be repainted.
  void Invalidate(const PP_Rect* dirty);

 protected:
  virtual void SetLocationInternal(const PP_Rect* location) = 0;
  PP_Rect location() const { return location_; }

 private:
  PP_Rect location_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Widget_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_WIDGET_IMPL_H_
