// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_WIDGET_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_WIDGET_H_

#include "base/scoped_ptr.h"
#include "ppapi/c/pp_rect.h"
#include "webkit/glue/plugins/pepper_resource.h"

struct PPB_Widget_Dev;
struct PP_InputEvent;

namespace pepper {

class ImageData;
class PluginInstance;

class Widget : public Resource  {
 public:
  explicit Widget(PluginInstance* instance);
  virtual ~Widget();

  // Returns a pointer to the interface implementing PPB_Widget that is
  // exposed to the plugin.
  static const PPB_Widget_Dev* GetInterface();

  // Resource overrides.
  Widget* AsWidget() { return this; }

  // PPB_Widget implementation.
  virtual bool Paint(const PP_Rect* rect, ImageData* image) = 0;
  virtual bool HandleEvent(const PP_InputEvent* event) = 0;
  bool GetLocation(PP_Rect* location);
  void SetLocation(const PP_Rect* location);

  // Notifies the plugin instance that the given rect needs to be repainted.
  void Invalidate(const PP_Rect* dirty);
  PluginInstance* instance() { return instance_; }

 protected:
  virtual void SetLocationInternal(const PP_Rect* location) = 0;
  PP_Rect location() const { return location_; }

 private:
  scoped_refptr<PluginInstance> instance_;
  PP_Rect location_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_WIDGET_H_
