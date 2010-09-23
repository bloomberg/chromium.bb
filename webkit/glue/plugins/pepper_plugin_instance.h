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
#include "third_party/ppapi/c/dev/pp_cursor_type_dev.h"
#include "third_party/ppapi/c/dev/ppp_graphics_3d_dev.h"
#include "third_party/ppapi/c/dev/ppp_printing_dev.h"
#include "third_party/ppapi/c/pp_instance.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCanvas.h"

struct PP_Var;
struct PPB_Instance;
struct PPB_Find_Dev;
struct PPB_Fullscreen_Dev;
struct PPP_Find_Dev;
struct PPP_Instance;
struct PPP_Zoom_Dev;

class SkBitmap;

namespace gfx {
class Rect;
}

namespace WebKit {
struct WebCursorInfo;
class WebInputEvent;
class WebPluginContainer;
}

namespace pepper {

class Graphics2D;
class ImageData;
class PluginDelegate;
class PluginModule;
class URLLoader;
class FullscreenContainer;

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
  static const PPB_Find_Dev* GetFindInterface();
  static const PPB_Fullscreen_Dev* GetFullscreenInterface();

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
  bool BindGraphics(PP_Resource device_id);
  bool full_frame() const { return full_frame_; }
  bool SetCursor(PP_CursorType_Dev type);
  PP_Var ExecuteScript(PP_Var script, PP_Var* exception);

  // PPP_Instance pass-through.
  void Delete();
  bool Initialize(WebKit::WebPluginContainer* container,
                  const std::vector<std::string>& arg_names,
                  const std::vector<std::string>& arg_values,
                  bool full_frame);
  bool HandleDocumentLoad(URLLoader* loader);
  bool HandleInputEvent(const WebKit::WebInputEvent& event,
                        WebKit::WebCursorInfo* cursor_info);
  void FocusChanged(bool has_focus);
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

  bool SupportsPrintInterface();
  int PrintBegin(const gfx::Rect& printable_area, int printer_dpi);
  bool PrintPage(int page_number, WebKit::WebCanvas* canvas);
  void PrintEnd();

  void Graphics3DContextLost();

  // Implementation of PPB_Fullscreen_Dev.
  bool IsFullscreen();
  bool SetFullscreen(bool fullscreen);

 private:
  bool LoadFindInterface();
  bool LoadZoomInterface();

  // Queries the plugin for supported print formats and sets |format| to the
  // best format to use. Returns false if the plugin does not support any
  // print format that we can handle (we can handle raster and PDF).
  bool GetPreferredPrintOutputFormat(PP_PrintOutputFormat_Dev* format);
  bool PrintPDFOutput(PP_Resource print_output, WebKit::WebCanvas* canvas);
  bool PrintRasterOutput(PP_Resource print_output, WebKit::WebCanvas* canvas);
#if defined(OS_WIN)
  bool DrawJPEGToPlatformDC(const SkBitmap& bitmap,
                            const gfx::Rect& printable_area,
                            WebKit::WebCanvas* canvas);
#elif defined(OS_MACOSX)
  // Draws the given kARGB_8888_Config bitmap to the specified canvas starting
  // at the specified destination rect.
  void DrawSkBitmapToCanvas(const SkBitmap& bitmap, WebKit::WebCanvas* canvas,
                            const gfx::Rect& dest_rect, int canvas_height);
#endif  // OS_MACOSX

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
  scoped_refptr<Graphics2D> bound_graphics_2d_;

  // The id of the current find operation, or -1 if none is in process.
  int find_identifier_;

  // The plugin find and zoom interfaces.
  const PPP_Find_Dev* plugin_find_interface_;
  const PPP_Zoom_Dev* plugin_zoom_interface_;

  // This is only valid between a successful PrintBegin call and a PrintEnd
  // call.
  PP_PrintSettings_Dev current_print_settings_;
#if defined(OS_MACOSX)
  // On the Mac, when we draw the bitmap to the PDFContext, it seems necessary
  // to keep the pixels valid until CGContextEndPage is called. We use this
  // variable to hold on to the pixels.
  scoped_refptr<ImageData> last_printed_page_;
#elif defined(OS_LINUX)
  // On Linux, we always send all pages from the renderer to the browser.
  // So, if the plugin supports printPagesAsPDF we print the entire output
  // in one shot in the first call to PrintPage.
  // (This is a temporary hack until we change the WebFrame and WebPlugin print
  // interfaces).
  // Specifies the total number of pages to be printed. It it set in PrintBegin.
  int32 num_pages_;
  // Specifies whether we have already output all pages. This is used to ignore
  // subsequent PrintPage requests.
  bool pdf_output_done_;
#endif  // defined(OS_LINUX)

  // The plugin print interface.
  const PPP_Printing_Dev* plugin_print_interface_;

  // The plugin 3D interface.
  const PPP_Graphics3D_Dev* plugin_graphics_3d_interface_;

  // Containes the cursor if it's set by the plugin.
  scoped_ptr<WebKit::WebCursorInfo> cursor_;

  // Plugin container for fullscreen mode. NULL if not in fullscreen mode.
  FullscreenContainer* fullscreen_container_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstance);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_INSTANCE_H_
