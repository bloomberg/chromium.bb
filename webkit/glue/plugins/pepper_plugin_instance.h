// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_INSTANCE_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_INSTANCE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "third_party/ppapi/c/pp_instance.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCanvas.h"

typedef struct _pp_Var PP_Var;
typedef struct _ppb_Instance PPB_Instance;
typedef struct _ppp_Instance PPP_Instance;

namespace gfx {
class Rect;
}

namespace WebKit {
struct WebCursorInfo;
class WebInputEvent;
class WebPluginContainer;
}

namespace pepper {

class DeviceContext2D;
class PluginDelegate;
class PluginModule;

class PluginInstance : public base::RefCounted<PluginInstance> {
 public:
  PluginInstance(PluginDelegate* delegate,
                 PluginModule* module,
                 const PPP_Instance* instance_interface);
  ~PluginInstance();

  static const PPB_Instance* GetInterface();

  // Converts the given instance ID to an actual instance object.
  static PluginInstance* FromPPInstance(PP_Instance instance);

  PluginDelegate* delegate() const { return delegate_; }
  PluginModule* module() const { return module_.get(); }

  PP_Instance GetPPInstance();

  void Paint(WebKit::WebCanvas* canvas,
             const gfx::Rect& plugin_rect,
             const gfx::Rect& paint_rect);

  // PPB_Instance implementation.
  PP_Var GetWindowObject();
  PP_Var GetOwnerElementObject();
  bool BindGraphicsDeviceContext(PP_Resource device_id);

  // PPP_Instance pass-through.
  void Delete();
  bool Initialize(WebKit::WebPluginContainer* container,
                  const std::vector<std::string>& arg_names,
                  const std::vector<std::string>& arg_values);
  bool HandleInputEvent(const WebKit::WebInputEvent& event,
                        WebKit::WebCursorInfo* cursor_info);
  void ViewChanged(const gfx::Rect& position, const gfx::Rect& clip);

 private:
  PluginDelegate* delegate_;
  scoped_refptr<PluginModule> module_;
  const PPP_Instance* instance_interface_;

  // NULL until we have been initialized.
  WebKit::WebPluginContainer* container_;

  // The current device context for painting in 2D.
  scoped_refptr<DeviceContext2D> device_context_2d_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstance);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_INSTANCE_H_
