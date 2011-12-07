// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPAPI_PLUGIN_INSTANCE_H_
#define WEBKIT_PLUGINS_PPAPI_PPAPI_PLUGIN_INSTANCE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "ppapi/c/dev/pp_cursor_type_dev.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppp_graphics_3d.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/shared_impl/instance_impl.h"
#include "ppapi/shared_impl/ppp_instance_combined.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCanvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/rect.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/webkit_plugins_export.h"

struct PP_Var;
struct PPP_Find_Dev;
struct PPP_InputEvent;
struct PPP_Instance_Private;
struct PPP_Messaging;
struct PPP_MouseLock;
struct PPP_Pdf;
struct PPP_Selection_Dev;
struct PPP_Zoom_Dev;

class SkBitmap;
class TransportDIB;

namespace WebKit {
class WebInputEvent;
class WebPluginContainer;
struct WebCompositionUnderline;
struct WebCursorInfo;
}

namespace ppapi {
struct InputEventData;
struct PPP_Instance_Combined;
class Resource;
}

namespace webkit {
namespace ppapi {

class FullscreenContainer;
class MessageChannel;
class PluginDelegate;
class PluginModule;
class PluginObject;
class PPB_Graphics2D_Impl;
class PPB_Graphics3D_Impl;
class PPB_ImageData_Impl;
class PPB_URLLoader_Impl;
class PPB_URLRequestInfo_Impl;

// Represents one time a plugin appears on one web page.
//
// Note: to get from a PP_Instance to a PluginInstance*, use the
// ResourceTracker.
class WEBKIT_PLUGINS_EXPORT PluginInstance :
    public base::RefCounted<PluginInstance>,
    public ::ppapi::FunctionGroupBase,
    NON_EXPORTED_BASE(public ::ppapi::thunk::PPB_Instance_FunctionAPI),
    public ::ppapi::InstanceImpl {
 public:
  // Create and return a PluginInstance object which supports the
  // PPP_Instance_1_0 interface.
  static PluginInstance* Create1_0(PluginDelegate* delegate,
                                   PluginModule* module,
                                   const void* ppp_instance_if_1_0);

  // Delete should be called by the WebPlugin before this destructor.
  virtual ~PluginInstance();

  PluginDelegate* delegate() const { return delegate_; }
  PluginModule* module() const { return module_.get(); }
  MessageChannel& message_channel() { return *message_channel_; }

  WebKit::WebPluginContainer* container() const { return container_; }

  const gfx::Rect& position() const { return position_; }
  const gfx::Rect& clip() const { return clip_; }

  void set_always_on_top(bool on_top) { always_on_top_ = on_top; }

  // Returns the PP_Instance uniquely identifying this instance. Guaranteed
  // nonzero.
  PP_Instance pp_instance() const { return pp_instance_; }

  // Does some pre-destructor cleanup on the instance. This is necessary
  // because some cleanup depends on the plugin instance still existing (like
  // calling the plugin's DidDestroy function). This function is called from
  // the WebPlugin implementation when WebKit is about to remove the plugin.
  void Delete();

  // Paints the current backing store to the web page.
  void Paint(WebKit::WebCanvas* canvas,
             const gfx::Rect& plugin_rect,
             const gfx::Rect& paint_rect);

  // Schedules a paint of the page for the given region. The coordinates are
  // relative to the top-left of the plugin. This does nothing if the plugin
  // has not yet been positioned. You can supply an empty gfx::Rect() to
  // invalidate the entire plugin.
  void InvalidateRect(const gfx::Rect& rect);

  // Schedules a scroll of the plugin.  This uses optimized scrolling only for
  // full-frame plugins, as otherwise there could be other elements on top.  The
  // slow path can also be triggered if there is an overlapping frame.
  void ScrollRect(int dx, int dy, const gfx::Rect& rect);

  // If the plugin instance is backed by a texture, return its texture ID in the
  // compositor's namespace. Otherwise return 0. Returns 0 by default.
  unsigned GetBackingTextureId();

  // Commit the backing texture to the screen once the side effects some
  // rendering up to an offscreen SwapBuffers are visible.
  void CommitBackingTexture();

  // Called when the out-of-process plugin implementing this instance crashed.
  void InstanceCrashed();

  // PPB_Instance and PPB_Instance_Private implementation.
  const GURL& plugin_url() const { return plugin_url_; }
  bool full_frame() const { return full_frame_; }
  // If |type| is not PP_CURSORTYPE_CUSTOM, |custom_image| and |hot_spot| are
  // ignored.
  bool SetCursor(PP_CursorType_Dev type,
                 PP_Resource custom_image,
                 const PP_Point* hot_spot);

  // PPP_Instance and PPP_Instance_Private pass-through.
  bool Initialize(WebKit::WebPluginContainer* container,
                  const std::vector<std::string>& arg_names,
                  const std::vector<std::string>& arg_values,
                  const GURL& plugin_url,
                  bool full_frame);
  bool HandleDocumentLoad(PPB_URLLoader_Impl* loader);
  bool HandleInputEvent(const WebKit::WebInputEvent& event,
                        WebKit::WebCursorInfo* cursor_info);
  PP_Var GetInstanceObject();
  void ViewChanged(const gfx::Rect& position, const gfx::Rect& clip);

  // Handlers for composition events.
  bool HandleCompositionStart(const string16& text);
  bool HandleCompositionUpdate(
      const string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  bool HandleCompositionEnd(const string16& text);
  bool HandleTextInput(const string16& text);

  // Implementation of composition API.
  void UpdateCaretPosition(const gfx::Rect& caret,
                           const gfx::Rect& bounding_box);
  void SetTextInputType(ui::TextInputType type);

  // Gets the current text input status.
  ui::TextInputType text_input_type() const { return text_input_type_; }
  gfx::Rect GetCaretBounds() const;
  bool IsPluginAcceptingCompositionEvents() const;

  // Notifications about focus changes, see has_webkit_focus_ below.
  void SetWebKitFocus(bool has_focus);
  void SetContentAreaFocus(bool has_focus);

  // Notifications that the view has rendered the page and that it has been
  // flushed to the screen. These messages are used to send Flush callbacks to
  // the plugin for DeviceContext2D.
  void ViewInitiatedPaint();
  void ViewFlushedPaint();

  // If this plugin can be painted merely by copying the backing store to the
  // screen, and the plugin bounds encloses the given paint bounds, returns
  // true. In this case, the location, clipping, and ID of the backing store
  // will be filled into the given output parameters.
  bool GetBitmapForOptimizedPluginPaint(
      const gfx::Rect& paint_bounds,
      TransportDIB** dib,
      gfx::Rect* dib_bounds,
      gfx::Rect* clip);

  // Tracks all live PluginObjects.
  void AddPluginObject(PluginObject* plugin_object);
  void RemovePluginObject(PluginObject* plugin_object);

  string16 GetSelectedText(bool html);
  string16 GetLinkAtPosition(const gfx::Point& point);
  void Zoom(double factor, bool text_only);
  bool StartFind(const string16& search_text,
                 bool case_sensitive,
                 int identifier);
  void SelectFindResult(bool forward);
  void StopFind();

  bool SupportsPrintInterface();
  bool IsPrintScalingDisabled();
  int PrintBegin(const gfx::Rect& printable_area, int printer_dpi);
  bool PrintPage(int page_number, WebKit::WebCanvas* canvas);
  void PrintEnd();

  void Graphics3DContextLost();

  // There are 2 implementations of the fullscreen interface
  // PPB_FlashFullscreen is used by Pepper Flash.
  // PPB_Fullscreen is intended for other applications including NaCl.
  // The two interface are mutually exclusive.

  // Implementation of PPB_FlashFullscreen.

  // Because going to fullscreen is asynchronous (but going out is not), there
  // are 3 states:
  // - normal            : fullscreen_container_ == NULL
  //                       flash_fullscreen_ == false
  // - fullscreen pending: fullscreen_container_ != NULL
  //                       flash_fullscreen_ == false
  // - fullscreen        : fullscreen_container_ != NULL
  //                       flash_fullscreen_ == true
  //
  // In normal state, events come from webkit and painting goes back to it.
  // In fullscreen state, events come from the fullscreen container, and
  // painting goes back to it.
  // In pending state, events from webkit are ignored, and as soon as we
  // receive events from the fullscreen container, we go to the fullscreen
  // state.
  bool FlashIsFullscreenOrPending();

  // Switches between fullscreen and normal mode. If |delay_report| is set to
  // false, it may report the new state through DidChangeView immediately. If
  // true, it will delay it. When called from the plugin, delay_report should
  // be true to avoid re-entrancy.
  void FlashSetFullscreen(bool fullscreen, bool delay_report);

  FullscreenContainer* fullscreen_container() const {
    return fullscreen_container_;
  }

  // Implementation of PPB_Fullscreen.

  // Because going to/from fullscreen is asynchronous, there are 4 states:
  // - normal            : desired_fullscreen_state_ == false
  //                       fullscreen_ == false
  // - fullscreen pending: desired_fullscreen_state_ == true
  //                       fullscreen_ == false
  // - fullscreen        : desired_fullscreen_state_ == true
  //                       fullscreen_ == true
  // - normal pending    : desired_fullscreen_state_ = false
  //                       fullscreen_ = true
  bool IsFullscreenOrPending();

  // Switches between fullscreen and normal mode. The transition is
  // asynchronous. WebKit will trigger corresponding VewChanged calls.
  // Returns true on success, false on failure (e.g. trying to enter fullscreen
  // when not processing a user gesture or trying to set fullscreen when
  // already in fullscreen mode).
  bool SetFullscreen(bool fullscreen);

  // Implementation of PPB_Flash.
  int32_t Navigate(PPB_URLRequestInfo_Impl* request,
                   const char* target,
                   bool from_user_action);

  // Implementation of PPP_Messaging.
  void HandleMessage(PP_Var message);

  PluginDelegate::PlatformContext3D* CreateContext3D();

  // Returns true iff the plugin is a full-page plugin (i.e. not in an iframe
  // or embedded in a page).
  bool IsFullPagePlugin() const;

  void OnLockMouseACK(int32_t result);
  void OnMouseLockLost();

  // Simulates an input event to the plugin by passing it down to WebKit,
  // which sends it back up to the plugin as if it came from the user.
  void SimulateInputEvent(const ::ppapi::InputEventData& input_event);

  // FunctionGroupBase overrides.
  virtual ::ppapi::thunk::PPB_Instance_FunctionAPI*
      AsPPB_Instance_FunctionAPI() OVERRIDE;

  // PPB_Instance_FunctionAPI implementation.
  virtual PP_Bool BindGraphics(PP_Instance instance,
                               PP_Resource device) OVERRIDE;
  virtual PP_Bool IsFullFrame(PP_Instance instance) OVERRIDE;
  virtual PP_Var GetWindowObject(PP_Instance instance) OVERRIDE;
  virtual PP_Var GetOwnerElementObject(PP_Instance instance) OVERRIDE;
  virtual PP_Var ExecuteScript(PP_Instance instance,
                               PP_Var script,
                               PP_Var* exception) OVERRIDE;
  virtual PP_Var GetDefaultCharSet(PP_Instance instance) OVERRIDE;
  virtual void Log(PP_Instance instance,
                   int log_level,
                   PP_Var value) OVERRIDE;
  virtual void LogWithSource(PP_Instance instance,
                             int log_level,
                             PP_Var source,
                             PP_Var value) OVERRIDE;
  virtual void NumberOfFindResultsChanged(PP_Instance instance,
                                          int32_t total,
                                          PP_Bool final_result) OVERRIDE;
  virtual void SelectedFindResultChanged(PP_Instance instance,
                                         int32_t index) OVERRIDE;
  virtual PP_Bool FlashIsFullscreen(PP_Instance instance) OVERRIDE;
  virtual PP_Bool FlashSetFullscreen(PP_Instance instance,
                                     PP_Bool fullscreen) OVERRIDE;
  virtual PP_Bool FlashGetScreenSize(PP_Instance instance,
                                     PP_Size* size) OVERRIDE;
  virtual PP_Bool IsFullscreen(PP_Instance instance) OVERRIDE;
  virtual PP_Bool SetFullscreen(PP_Instance instance,
                                     PP_Bool fullscreen) OVERRIDE;
  virtual PP_Bool GetScreenSize(PP_Instance instance, PP_Size* size)
      OVERRIDE;
  virtual int32_t RequestInputEvents(PP_Instance instance,
                                     uint32_t event_classes) OVERRIDE;
  virtual int32_t RequestFilteringInputEvents(PP_Instance instance,
                                              uint32_t event_classes) OVERRIDE;
  virtual void ClearInputEventRequest(PP_Instance instance,
                                      uint32_t event_classes) OVERRIDE;
  virtual void ZoomChanged(PP_Instance instance, double factor) OVERRIDE;
  virtual void ZoomLimitsChanged(PP_Instance instance,
                                 double minimum_factor,
                                 double maximium_factor) OVERRIDE;
  virtual void PostMessage(PP_Instance instance, PP_Var message) OVERRIDE;
  virtual int32_t LockMouse(PP_Instance instance,
                            PP_CompletionCallback callback) OVERRIDE;
  virtual void UnlockMouse(PP_Instance instance) OVERRIDE;
  virtual PP_Var ResolveRelativeToDocument(
      PP_Instance instance,
      PP_Var relative,
      PP_URLComponents_Dev* components) OVERRIDE;
  virtual PP_Bool DocumentCanRequest(PP_Instance instance, PP_Var url) OVERRIDE;
  virtual PP_Bool DocumentCanAccessDocument(PP_Instance instance,
                                            PP_Instance target) OVERRIDE;
  virtual PP_Var GetDocumentURL(PP_Instance instance,
                                PP_URLComponents_Dev* components) OVERRIDE;
  virtual PP_Var GetPluginInstanceURL(
      PP_Instance instance,
      PP_URLComponents_Dev* components) OVERRIDE;

 private:
  // See the static Create functions above for creating PluginInstance objects.
  // This constructor is private so that we can hide the PPP_Instance_Combined
  // details while still having 1 constructor to maintain for member
  // initialization.
  PluginInstance(PluginDelegate* delegate,
                 PluginModule* module,
                 ::ppapi::PPP_Instance_Combined* instance_interface);

  bool LoadFindInterface();
  bool LoadInputEventInterface();
  bool LoadMessagingInterface();
  bool LoadMouseLockInterface();
  bool LoadPdfInterface();
  bool LoadPrintInterface();
  bool LoadPrivateInterface();
  bool LoadSelectionInterface();
  bool LoadZoomInterface();

  // Determines if we think the plugin has focus, both content area and webkit
  // (see has_webkit_focus_ below).
  bool PluginHasFocus() const;

  // Reports the current plugin geometry to the plugin by calling
  // DidChangeView.
  void ReportGeometry();

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
#elif defined(OS_MACOSX) && !defined(USE_SKIA)
  // Draws the given kARGB_8888_Config bitmap to the specified canvas starting
  // at the specified destination rect.
  void DrawSkBitmapToCanvas(const SkBitmap& bitmap, WebKit::WebCanvas* canvas,
                            const gfx::Rect& dest_rect, int canvas_height);
#endif  // OS_MACOSX

  // Get the bound graphics context as a concrete 2D graphics context or returns
  // null if the context is not 2D.
  PPB_Graphics2D_Impl* GetBoundGraphics2D() const;

  // Get the bound 3D graphics context.
  // Returns NULL if bound graphics is not a 3D context.
  PPB_Graphics3D_Impl* GetBoundGraphics3D() const;

  // Sets the id of the texture that the plugin draws to. The id is in the
  // compositor space so it can use it to composite with rest of the page.
  // A value of zero indicates the plugin is not backed by a texture.
  void setBackingTextureId(unsigned int id);

  // Internal helper function for PrintPage().
  bool PrintPageHelper(PP_PrintPageNumberRange_Dev* page_ranges,
                       int num_ranges,
                       WebKit::WebCanvas* canvas);

  void DoSetCursor(WebKit::WebCursorInfo* cursor);

  // Internal helper functions for HandleCompositionXXX().
  bool SendCompositionEventToPlugin(
      PP_InputEvent_Type type,
      const string16& text);
  bool SendCompositionEventWithUnderlineInformationToPlugin(
      PP_InputEvent_Type type,
      const string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);

  // Checks if the security origin of the document containing this instance can
  // assess the security origin of the main frame document.
  bool CanAccessMainFrame() const;

  // Returns true if the WebView the plugin is in renders via the accelerated
  // compositing path.
  bool IsViewAccelerated();

  // Remember view parameters that were sent to the plugin.
  void SetSentDidChangeView(const gfx::Rect& position, const gfx::Rect& clip);

  // Track, set and reset size attributes to control the size of the plugin
  // in and out of fullscreen mode.
  void KeepSizeAttributesBeforeFullscreen();
  void SetSizeAttributesForFullscreen();
  void ResetSizeAttributesAfterFullscreen();

  PluginDelegate* delegate_;
  scoped_refptr<PluginModule> module_;
  scoped_ptr< ::ppapi::PPP_Instance_Combined> instance_interface_;

  PP_Instance pp_instance_;

  // NULL until we have been initialized.
  WebKit::WebPluginContainer* container_;

  // Plugin URL.
  GURL plugin_url_;

  // Indicates whether this is a full frame instance, which means it represents
  // an entire document rather than an embed tag.
  bool full_frame_;

  // Indicates if we've ever sent a didChangeView to the plugin. This ensure we
  // always send an initial notification, even if the position and clip are the
  // same as the default values.
  bool sent_did_change_view_;

  // Position in the viewport (which moves as the page is scrolled) of this
  // plugin. This will be a 0-sized rectangle if the plugin has not yet been
  // laid out.
  gfx::Rect position_;

  // Current clip rect. This will be empty if the plugin is not currently
  // visible. This is in the plugin's coordinate system, so fully visible will
  // be (0, 0, w, h) regardless of scroll position.
  gfx::Rect clip_;

  // The current device context for painting in 2D or 3D.
  scoped_refptr< ::ppapi::Resource> bound_graphics_;

  // We track two types of focus, one from WebKit, which is the focus among
  // all elements of the page, one one from the browser, which is whether the
  // tab/window has focus. We tell the plugin it has focus only when both of
  // these values are set to true.
  bool has_webkit_focus_;
  bool has_content_area_focus_;

  // The id of the current find operation, or -1 if none is in process.
  int find_identifier_;

  // The plugin-provided interfaces.
  const PPP_Find_Dev* plugin_find_interface_;
  const PPP_Messaging* plugin_messaging_interface_;
  const PPP_MouseLock* plugin_mouse_lock_interface_;
  const PPP_InputEvent* plugin_input_event_interface_;
  const PPP_Instance_Private* plugin_private_interface_;
  const PPP_Pdf* plugin_pdf_interface_;
  const PPP_Selection_Dev* plugin_selection_interface_;
  const PPP_Zoom_Dev* plugin_zoom_interface_;

  // Flags indicating whether we have asked this plugin instance for the
  // corresponding interfaces, so that we can ask only once.
  bool checked_for_plugin_input_event_interface_;
  bool checked_for_plugin_messaging_interface_;

  // This is only valid between a successful PrintBegin call and a PrintEnd
  // call.
  PP_PrintSettings_Dev current_print_settings_;
#if defined(OS_MACOSX)
  // On the Mac, when we draw the bitmap to the PDFContext, it seems necessary
  // to keep the pixels valid until CGContextEndPage is called. We use this
  // variable to hold on to the pixels.
  scoped_refptr<PPB_ImageData_Impl> last_printed_page_;
#endif  // defined(OS_MACOSX)
#if defined(USE_SKIA)
  // Always when printing to PDF on Linux and when printing for preview on Mac
  // and Win, the entire document goes into one metafile.  However, when users
  // print only a subset of all the pages, it is impossible to know if a call
  // to PrintPage() is the last call. Thus in PrintPage(), just store the page
  // number in |ranges_|. The hack is in PrintEnd(), where a valid |canvas_|
  // is preserved in PrintWebViewHelper::PrintPages. This makes it possible
  // to generate the entire PDF given the variables below:
  //
  // The most recently used WebCanvas, guaranteed to be valid.
  SkRefPtr<WebKit::WebCanvas> canvas_;
  // An array of page ranges.
  std::vector<PP_PrintPageNumberRange_Dev> ranges_;
#endif  // OS_LINUX || OS_WIN

  // The plugin print interface.
  const PPP_Printing_Dev* plugin_print_interface_;

  // The plugin 3D interface.
  const PPP_Graphics3D* plugin_graphics_3d_interface_;

  // Contains the cursor if it's set by the plugin.
  scoped_ptr<WebKit::WebCursorInfo> cursor_;

  // Set to true if this plugin thinks it will always be on top. This allows us
  // to use a more optimized painting path in some cases.
  bool always_on_top_;

  // Implementation of PPB_FlashFullscreen.

  // Plugin container for fullscreen mode. NULL if not in fullscreen mode. Note:
  // there is a transition state where fullscreen_container_ is non-NULL but
  // flash_fullscreen_ is false (see above).
  FullscreenContainer* fullscreen_container_;

  // True if we are in fullscreen mode. False if we are in normal mode or
  // in transition to fullscreen.
  bool flash_fullscreen_;

  // Implementation of PPB_Fullscreen.

  // Since entering fullscreen mode is an asynchronous operation, we set this
  // variable to the desired state at the time we issue the fullscreen change
  // request. The plugin will receive a DidChangeView event when it goes
  // fullscreen.
  bool desired_fullscreen_state_;

  // True if we are in fullscreen mode. False if we are in normal mode.
  // It reflects the previous state when in transition.
  bool fullscreen_;

  // WebKit does not resize the plugin when going into fullscreen mode, so we do
  // this here by modifying the various plugin attributes and then restoring
  // them on exit.
  WebKit::WebString width_before_fullscreen_;
  WebKit::WebString height_before_fullscreen_;
  WebKit::WebString border_before_fullscreen_;
  WebKit::WebString style_before_fullscreen_;
  gfx::Size screen_size_for_fullscreen_;

  // The MessageChannel used to implement bidirectional postMessage for the
  // instance.
  scoped_ptr<MessageChannel> message_channel_;

  // Bitmap for crashed plugin. Lazily initialized, non-owning pointer.
  SkBitmap* sad_plugin_;

  typedef std::set<PluginObject*> PluginObjectSet;
  PluginObjectSet live_plugin_objects_;

  // Classes of events that the plugin has registered for, both for filtering
  // and not. The bits are PP_INPUTEVENT_CLASS_*.
  uint32_t input_event_mask_;
  uint32_t filtered_input_event_mask_;

  // Text composition status.
  ui::TextInputType text_input_type_;
  gfx::Rect text_input_caret_;
  gfx::Rect text_input_caret_bounds_;
  bool text_input_caret_set_;

  PP_CompletionCallback lock_mouse_callback_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstance);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPAPI_PLUGIN_INSTANCE_H_
