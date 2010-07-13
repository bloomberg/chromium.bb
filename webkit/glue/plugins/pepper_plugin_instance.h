// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_INSTANCE_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_INSTANCE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "gfx/rect.h"
#include "third_party/ppapi/c/pp_cursor_type.h"
#include "third_party/ppapi/c/pp_instance.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "third_party/ppapi/c/ppb_find.h"
#include "third_party/ppapi/c/ppp_find.h"
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
class URLLoader;

class PluginInstance : public base::RefCounted<PluginInstance> {
 public:
  PluginInstance(PluginDelegate* delegate,
                 PluginModule* module,
                 const PPP_Instance* instance_interface);
  ~PluginInstance();

  static const PPB_Instance* GetInterface();

  // Converts the given instance ID to an actual instance object.
  static PluginInstance* FromPPInstance(PP_Instance instance);

  // Returns a pointer to the interface implementing PPB_Find that is
  // exposed to the plugin.
  static const PPB_Find* GetFindInterface();

  PluginDelegate* delegate() const { return delegate_; }
  PluginModule* module() const { return module_.get(); }

  WebKit::WebPluginContainer* container() const { return container_; }

  const gfx::Rect& position() const { return position_; }
  const gfx::Rect& clip() const { return clip_; }

  int find_identifier() const { return find_identifier_; }

  PP_Instance GetPPInstance();

  // Paints the current backing store to the web page.
  void Paint(WebKit::WebCanvas* canvas,
             const gfx::Rect& plugin_rect,
             const gfx::Rect& paint_rect);

  // Schedules a paint of the page for the given region. The coordinates are
  // relative to the top-left of the plugin. This does nothing if the plugin
  // has not yet been positioned. You can supply an empty gfx::Rect() to
  // invalidate the entire plugin.
  void InvalidateRect(const gfx::Rect& rect);

  // PPB_Instance implementation.
  PP_Var GetWindowObject();
  PP_Var GetOwnerElementObject();
  bool BindGraphicsDeviceContext(PP_Resource device_id);
  bool full_frame() const { return full_frame_; }
  bool SetCursor(PP_CursorType type);

  // PPP_Instance pass-through.
  void Delete();
  bool Initialize(WebKit::WebPluginContainer* container,
                  const std::vector<std::string>& arg_names,
                  const std::vector<std::string>& arg_values,
                  bool full_frame);
  bool HandleDocumentLoad(URLLoader* loader);
  bool HandleInputEvent(const WebKit::WebInputEvent& event,
                        WebKit::WebCursorInfo* cursor_info);
  PP_Var GetInstanceObject();
  void ViewChanged(const gfx::Rect& position, const gfx::Rect& clip);

  // Notifications that the view has rendered the page and that it has been
  // flushed to the screen. These messages are used to send Flush callbacks to
  // the plugin for DeviceContext2D.
  void ViewInitiatedPaint();
  void ViewFlushedPaint();

  string16 GetSelectedText(bool html);
  void Zoom(float factor, bool text_only);
  bool StartFind(const string16& search_text,
                 bool case_sensitive,
                 int identifier);
  void SelectFindResult(bool forward);
  void StopFind();

 private:
  bool LoadFindInterface();

  PluginDelegate* delegate_;
  scoped_refptr<PluginModule> module_;
  const PPP_Instance* instance_interface_;

  // NULL until we have been initialized.
  WebKit::WebPluginContainer* container_;

  // Indicates whether this is a full frame instance, which means it represents
  // an entire document rather than an embed tag.
  bool full_frame_;

  // Position in the viewport (which moves as the page is scrolled) of this
  // plugin. This will be a 0-sized rectangle if the plugin has not yet been
  // laid out.
  gfx::Rect position_;

  // Current clip rect. This will be empty if the plugin is not currently
  // visible. This is in the plugin's coordinate system, so fully visible will
  // be (0, 0, w, h) regardless of scroll position.
  gfx::Rect clip_;

  // The current device context for painting in 2D.
  scoped_refptr<DeviceContext2D> device_context_2d_;

  // The id of the current find operation, or -1 if none is in process.
  int find_identifier_;

  // The plugin find interface.
  const PPP_Find* plugin_find_interface_;

  // Containes the cursor if it's set by the plugin.
  scoped_ptr<WebKit::WebCursorInfo> cursor_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstance);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_INSTANCE_H_
