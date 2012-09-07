// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPAPI_PLUGIN_INSTANCE_H_
#define WEBKIT_PLUGINS_PPAPI_PPAPI_PLUGIN_INSTANCE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "media/base/decryptor.h"
#include "ppapi/c/dev/pp_cursor_type_dev.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/dev/ppp_find_dev.h"
#include "ppapi/c/dev/ppp_selection_dev.h"
#include "ppapi/c/dev/ppp_text_input_dev.h"
#include "ppapi/c/dev/ppp_zoom_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/c/ppp_graphics_3d.h"
#include "ppapi/c/ppp_input_event.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/c/ppp_mouse_lock.h"
#include "ppapi/c/private/ppb_content_decryptor_private.h"
#include "ppapi/c/private/ppp_content_decryptor_private.h"
#include "ppapi/c/private/ppp_instance_private.h"
#include "ppapi/shared_impl/ppb_instance_shared.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "ppapi/thunk/ppb_gamepad_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCanvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPlugin.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/rect.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppb_flash_impl.h"
#include "webkit/plugins/ppapi/ppp_pdf.h"
#include "webkit/plugins/webkit_plugins_export.h"

struct PP_DecryptedBlockInfo;
struct PP_Point;

class SkBitmap;
class TransportDIB;

namespace WebKit {
class WebInputEvent;
class WebMouseEvent;
class WebPluginContainer;
struct WebCompositionUnderline;
struct WebCursorInfo;
struct WebPrintParams;
}

namespace media {
class DecoderBuffer;
class DecryptorClient;
}

namespace ppapi {
struct InputEventData;
struct PPP_Instance_Combined;
class Resource;
}

namespace ui {
class Range;
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
    public base::SupportsWeakPtr<PluginInstance>,
    public ::ppapi::PPB_Instance_Shared {
 public:
  // Create and return a PluginInstance object which supports the most recent
  // version of PPP_Instance possible by querying the given get_plugin_interface
  // function. If the plugin does not support any valid PPP_Instance interface,
  // returns NULL.
  static PluginInstance* Create(PluginDelegate* delegate, PluginModule* module);
  // Delete should be called by the WebPlugin before this destructor.
  virtual ~PluginInstance();

  PluginDelegate* delegate() const { return delegate_; }
  PluginModule* module() const { return module_.get(); }
  MessageChannel& message_channel() { return *message_channel_; }

  WebKit::WebPluginContainer* container() const { return container_; }

  void set_always_on_top(bool on_top) { always_on_top_ = on_top; }

  // Returns the PP_Instance uniquely identifying this instance. Guaranteed
  // nonzero.
  PP_Instance pp_instance() const { return pp_instance_; }

  ::ppapi::thunk::ResourceCreationAPI& resource_creation() {
    return *resource_creation_.get();
  }

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
  const ::ppapi::ViewData& view_data() const { return view_data_; }

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
  void ViewChanged(const gfx::Rect& position, const gfx::Rect& clip,
                   const std::vector<gfx::Rect>& cut_outs_rects);

  // Handlers for composition events.
  bool HandleCompositionStart(const string16& text);
  bool HandleCompositionUpdate(
      const string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  bool HandleCompositionEnd(const string16& text);
  bool HandleTextInput(const string16& text);

  // Gets the current text input status.
  ui::TextInputType text_input_type() const { return text_input_type_; }
  gfx::Rect GetCaretBounds() const;
  bool IsPluginAcceptingCompositionEvents() const;
  void GetSurroundingText(string16* text, ui::Range* range) const;

  // Notifications about focus changes, see has_webkit_focus_ below.
  void SetWebKitFocus(bool has_focus);
  void SetContentAreaFocus(bool has_focus);

  // Notification about page visibility. The default is "visible".
  void PageVisibilityChanged(bool is_visible);

  // Notifications that the view is about to paint, has started painting, and
  // has flushed the painted content to the screen. These messages are used to
  // send Flush callbacks to the plugin for DeviceContext2D/3D.
  void ViewWillInitiatePaint();
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
      gfx::Rect* clip,
      float* scale_factor);

  // Tracks all live PluginObjects.
  void AddPluginObject(PluginObject* plugin_object);
  void RemovePluginObject(PluginObject* plugin_object);

  string16 GetSelectedText(bool html);
  string16 GetLinkAtPosition(const gfx::Point& point);
  void RequestSurroundingText(size_t desired_number_of_characters);
  void Zoom(double factor, bool text_only);
  bool StartFind(const string16& search_text,
                 bool case_sensitive,
                 int identifier);
  void SelectFindResult(bool forward);
  void StopFind();

  bool SupportsPrintInterface();
  bool IsPrintScalingDisabled();
  int PrintBegin(const WebKit::WebPrintParams& print_params);
  bool PrintPage(int page_number, WebKit::WebCanvas* canvas);
  void PrintEnd();

  bool CanRotateView();
  void RotateView(WebKit::WebPlugin::RotationType type);

  void Graphics3DContextLost();

  // Provides access to PPP_ContentDecryptor_Private.
  // TODO(tomfinegan): Move decryptor methods to delegate class.
  void set_decrypt_client(media::DecryptorClient* client);
  bool GenerateKeyRequest(const std::string& key_system,
                          const std::string& init_data);
  bool AddKey(const std::string& session_id,
              const std::string& key,
              const std::string& init_data);
  bool CancelKeyRequest(const std::string& session_id);
  bool Decrypt(const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
               const media::Decryptor::DecryptCB& decrypt_cb);
  // TODO(xhwang): Update this when we need to support decrypt and decode.
  bool DecryptAndDecode(
      const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
      const media::Decryptor::DecryptCB& decrypt_cb);

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

  // Updates |flash_fullscreen_| and sends focus change notification if
  // necessary.
  void UpdateFlashFullscreenState(bool flash_fullscreen);

  FullscreenContainer* fullscreen_container() const {
    return fullscreen_container_;
  }

  // Implementation of PPB_Fullscreen.

  // Because going to/from fullscreen is asynchronous, there are 4 states:
  // - normal            : desired_fullscreen_state_ == false
  //                       view_data_.is_fullscreen == false
  // - fullscreen pending: desired_fullscreen_state_ == true
  //                       view_data_.is_fullscreen == false
  // - fullscreen        : desired_fullscreen_state_ == true
  //                       view_data_.is_fullscreen == true
  // - normal pending    : desired_fullscreen_state_ = false
  //                       view_data_.is_fullscreen = true
  bool IsFullscreenOrPending();

  bool flash_fullscreen() const { return flash_fullscreen_; }

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
  bool IsRectTopmost(const gfx::Rect& rect);

  // Implementation of PPP_Messaging.
  void HandleMessage(PP_Var message);

  PluginDelegate::PlatformContext3D* CreateContext3D();

  // Returns true iff the plugin is a full-page plugin (i.e. not in an iframe
  // or embedded in a page).
  bool IsFullPagePlugin() const;

  // Returns true if the plugin is processing a user gesture.
  bool IsProcessingUserGesture();

  // A mouse lock request was pending and this reports success or failure.
  void OnLockMouseACK(bool succeeded);
  // A mouse lock was in place, but has been lost.
  void OnMouseLockLost();
  // A mouse lock is enabled and mouse events are being delivered.
  void HandleMouseLockedInputEvent(const WebKit::WebMouseEvent& event);

  // Simulates an input event to the plugin by passing it down to WebKit,
  // which sends it back up to the plugin as if it came from the user.
  void SimulateInputEvent(const ::ppapi::InputEventData& input_event);

  // Simulates an IME event at the level of RenderView which sends it back up to
  // the plugin as if it came from the user.
  bool SimulateIMEEvent(const ::ppapi::InputEventData& input_event);
  void SimulateImeSetCompositionEvent(
      const ::ppapi::InputEventData& input_event);

  // PPB_Instance_API implementation.
  virtual PP_Bool BindGraphics(PP_Instance instance,
                               PP_Resource device) OVERRIDE;
  virtual PP_Bool IsFullFrame(PP_Instance instance) OVERRIDE;
  virtual const ::ppapi::ViewData* GetViewData(PP_Instance instance) OVERRIDE;
  virtual PP_Var GetWindowObject(PP_Instance instance) OVERRIDE;
  virtual PP_Var GetOwnerElementObject(PP_Instance instance) OVERRIDE;
  virtual PP_Var ExecuteScript(PP_Instance instance,
                               PP_Var script,
                               PP_Var* exception) OVERRIDE;
  virtual uint32_t GetAudioHardwareOutputSampleRate(PP_Instance instance)
      OVERRIDE;
  virtual uint32_t GetAudioHardwareOutputBufferSize(PP_Instance instance)
      OVERRIDE;
  virtual PP_Var GetDefaultCharSet(PP_Instance instance) OVERRIDE;
  virtual PP_Var GetFontFamilies(PP_Instance instance) OVERRIDE;
  virtual void NumberOfFindResultsChanged(PP_Instance instance,
                                          int32_t total,
                                          PP_Bool final_result) OVERRIDE;
  virtual void SelectedFindResultChanged(PP_Instance instance,
                                         int32_t index) OVERRIDE;
  virtual PP_Bool SetFullscreen(PP_Instance instance,
                                     PP_Bool fullscreen) OVERRIDE;
  virtual PP_Bool GetScreenSize(PP_Instance instance, PP_Size* size)
      OVERRIDE;
  virtual ::ppapi::thunk::PPB_Flash_API* GetFlashAPI() OVERRIDE;
  virtual ::ppapi::thunk::PPB_Gamepad_API* GetGamepadAPI(PP_Instance instance)
      OVERRIDE;
  virtual int32_t RequestInputEvents(PP_Instance instance,
                                     uint32_t event_classes) OVERRIDE;
  virtual int32_t RequestFilteringInputEvents(PP_Instance instance,
                                              uint32_t event_classes) OVERRIDE;
  virtual void ClearInputEventRequest(PP_Instance instance,
                                      uint32_t event_classes) OVERRIDE;
  virtual void ClosePendingUserGesture(PP_Instance instance,
                                       PP_TimeTicks timestamp);
  virtual void ZoomChanged(PP_Instance instance, double factor) OVERRIDE;
  virtual void ZoomLimitsChanged(PP_Instance instance,
                                 double minimum_factor,
                                 double maximium_factor) OVERRIDE;
  virtual void PostMessage(PP_Instance instance, PP_Var message) OVERRIDE;
  virtual PP_Bool SetCursor(PP_Instance instance,
                            PP_MouseCursor_Type type,
                            PP_Resource image,
                            const PP_Point* hot_spot) OVERRIDE;
  virtual int32_t LockMouse(
      PP_Instance instance,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual void UnlockMouse(PP_Instance instance) OVERRIDE;
  virtual void SetTextInputType(PP_Instance instance,
                                PP_TextInput_Type type) OVERRIDE;
  virtual void UpdateCaretPosition(PP_Instance instance,
                                   const PP_Rect& caret,
                                   const PP_Rect& bounding_box) OVERRIDE;
  virtual void CancelCompositionText(PP_Instance instance) OVERRIDE;
  virtual void SelectionChanged(PP_Instance instance) OVERRIDE;
  virtual void UpdateSurroundingText(PP_Instance instance,
                                     const char* text,
                                     uint32_t caret,
                                     uint32_t anchor) OVERRIDE;
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

  // TODO(tomfinegan): Move the next 7 methods to a delegate class.
  virtual void NeedKey(PP_Instance instance,
                       PP_Var key_system,
                       PP_Var session_id,
                       PP_Var init_data) OVERRIDE;
  virtual void KeyAdded(PP_Instance instance,
                        PP_Var key_system,
                        PP_Var session_id) OVERRIDE;
  virtual void KeyMessage(PP_Instance instance,
                          PP_Var key_system,
                          PP_Var session_id,
                          PP_Resource message,
                          PP_Var default_url) OVERRIDE;
  virtual void KeyError(PP_Instance instance,
                        PP_Var key_system,
                        PP_Var session_id,
                        int32_t media_error,
                        int32_t system_code) OVERRIDE;
  virtual void DeliverBlock(PP_Instance instance,
                            PP_Resource decrypted_block,
                            const PP_DecryptedBlockInfo* block_info) OVERRIDE;
  virtual void DeliverFrame(PP_Instance instance,
                            PP_Resource decrypted_frame,
                            const PP_DecryptedBlockInfo* block_info) OVERRIDE;
  virtual void DeliverSamples(PP_Instance instance,
                              PP_Resource decrypted_samples,
                              const PP_DecryptedBlockInfo* block_info) OVERRIDE;

  // Reset this instance as proxied. Resets cached interfaces to point to the
  // proxy and re-sends DidCreate, DidChangeView, and HandleDocumentLoad (if
  // necessary).
  // This is for use with the NaCl proxy.
  bool ResetAsProxied(scoped_refptr<PluginModule> module);

 private:
  // Implements PPB_Gamepad_API. This is just to avoid having an excessive
  // number of interfaces implemented by PluginInstance.
  class GamepadImpl : public ::ppapi::thunk::PPB_Gamepad_API {
   public:
    explicit GamepadImpl(PluginDelegate* delegate);
    virtual void Sample(PP_GamepadsSampleData* data) OVERRIDE;
   private:
    PluginDelegate* delegate_;
  };

  // See the static Create functions above for creating PluginInstance objects.
  // This constructor is private so that we can hide the PPP_Instance_Combined
  // details while still having 1 constructor to maintain for member
  // initialization.
  PluginInstance(PluginDelegate* delegate,
                 PluginModule* module,
                 ::ppapi::PPP_Instance_Combined* instance_interface);

  bool LoadContentDecryptorInterface();
  bool LoadFindInterface();
  bool LoadInputEventInterface();
  bool LoadMessagingInterface();
  bool LoadMouseLockInterface();
  bool LoadPdfInterface();
  bool LoadPrintInterface();
  bool LoadPrivateInterface();
  bool LoadSelectionInterface();
  bool LoadTextInputInterface();
  bool LoadZoomInterface();

  // Determines if we think the plugin has focus, both content area and webkit
  // (see has_webkit_focus_ below).
  bool PluginHasFocus() const;
  void SendFocusChangeNotification();

  // Returns true if the plugin has registered to accept touch events.
  bool IsAcceptingTouchEvents() const;

  void ScheduleAsyncDidChangeView(const ::ppapi::ViewData& previous_view);
  void SendAsyncDidChangeView(const ::ppapi::ViewData& previous_view);
  void SendDidChangeView(const ::ppapi::ViewData& previous_view);

  // Reports the current plugin geometry to the plugin by calling
  // DidChangeView.
  void ReportGeometry();

  // Queries the plugin for supported print formats and sets |format| to the
  // best format to use. Returns false if the plugin does not support any
  // print format that we can handle (we can handle only PDF).
  bool GetPreferredPrintOutputFormat(PP_PrintOutputFormat_Dev* format);
  bool PrintPDFOutput(PP_Resource print_output, WebKit::WebCanvas* canvas);

  // Get the bound graphics context as a concrete 2D graphics context or returns
  // null if the context is not 2D.
  PPB_Graphics2D_Impl* GetBoundGraphics2D() const;

  // Get the bound 3D graphics context.
  // Returns NULL if bound graphics is not a 3D context.
  PPB_Graphics3D_Impl* GetBoundGraphics3D() const;

  // Sets the id of the texture that the plugin draws to. The id is in the
  // compositor space so it can use it to composite with rest of the page.
  // A value of zero indicates the plugin is not backed by a texture.
  // is_opaque is true if the plugin contents are always opaque.
  void setBackingTextureId(unsigned int id, bool is_opaque);

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

  // Track, set and reset size attributes to control the size of the plugin
  // in and out of fullscreen mode.
  void KeepSizeAttributesBeforeFullscreen();
  void SetSizeAttributesForFullscreen();
  void ResetSizeAttributesAfterFullscreen();

  PluginDelegate* delegate_;
  scoped_refptr<PluginModule> module_;
  scoped_ptr< ::ppapi::PPP_Instance_Combined> instance_interface_;
  // If this is the NaCl plugin, we create a new module when we switch to the
  // IPC-based PPAPI proxy. Store the original module and instance interface
  // so we can shut down properly.
  scoped_refptr<PluginModule> original_module_;
  scoped_ptr< ::ppapi::PPP_Instance_Combined> original_instance_interface_;

  PP_Instance pp_instance_;

  // NULL until we have been initialized.
  WebKit::WebPluginContainer* container_;

  // Plugin URL.
  GURL plugin_url_;

  // Indicates whether this is a full frame instance, which means it represents
  // an entire document rather than an embed tag.
  bool full_frame_;

  // Stores the current state of the plugin view.
  ::ppapi::ViewData view_data_;

  // Indicates if we've ever sent a didChangeView to the plugin. This ensure we
  // always send an initial notification, even if the position and clip are the
  // same as the default values.
  bool sent_initial_did_change_view_;

  // We use a weak ptr factory for scheduling DidChangeView events so that we
  // can tell whether updates are pending and consolidate them. When there's
  // already a weak ptr pending (HasWeakPtrs is true), code should update the
  // view_data_ but not send updates. This also allows us to cancel scheduled
  // view change events.
  base::WeakPtrFactory<PluginInstance> view_change_weak_ptr_factory_;

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

  // Helper object that creates resources.
  scoped_ptr< ::ppapi::thunk::ResourceCreationAPI> resource_creation_;

  // The plugin-provided interfaces.
  const PPP_ContentDecryptor_Private* plugin_decryption_interface_;
  const PPP_Find_Dev* plugin_find_interface_;
  const PPP_InputEvent* plugin_input_event_interface_;
  const PPP_Messaging* plugin_messaging_interface_;
  const PPP_MouseLock* plugin_mouse_lock_interface_;
  const PPP_Pdf* plugin_pdf_interface_;
  const PPP_Instance_Private* plugin_private_interface_;
  const PPP_Selection_Dev* plugin_selection_interface_;
  const PPP_TextInput_Dev* plugin_textinput_interface_;
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

  GamepadImpl gamepad_impl_;

  // The plugin print interface.
  const PPP_Printing_Dev* plugin_print_interface_;

  // The plugin 3D interface.
  const PPP_Graphics3D* plugin_graphics_3d_interface_;

  // Contains the cursor if it's set by the plugin.
  scoped_ptr<WebKit::WebCursorInfo> cursor_;

  // Set to true if this plugin thinks it will always be on top. This allows us
  // to use a more optimized painting path in some cases.
  bool always_on_top_;
  // Even if |always_on_top_| is true, the plugin is not fully visible if there
  // are some cut-out areas (occupied by iframes higher in the stacking order).
  // This information is used in the optimized painting path.
  std::vector<gfx::Rect> cut_outs_rects_;

  // Implementation of PPB_FlashFullscreen.

  // Plugin container for fullscreen mode. NULL if not in fullscreen mode. Note:
  // there is a transition state where fullscreen_container_ is non-NULL but
  // flash_fullscreen_ is false (see above).
  FullscreenContainer* fullscreen_container_;

  // True if we are in "flash" fullscreen mode. False if we are in normal mode
  // or in transition to fullscreen. Normal fullscreen mode is indicated in
  // the ViewData.
  bool flash_fullscreen_;

  // Implementation of PPB_Fullscreen.

  // Since entering fullscreen mode is an asynchronous operation, we set this
  // variable to the desired state at the time we issue the fullscreen change
  // request. The plugin will receive a DidChangeView event when it goes
  // fullscreen.
  bool desired_fullscreen_state_;

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

  // Text selection status.
  std::string surrounding_text_;
  size_t selection_caret_;
  size_t selection_anchor_;

  scoped_refptr< ::ppapi::TrackedCallback> lock_mouse_callback_;

  // Track pending user gestures so out-of-process plugins can respond to
  // a user gesture after it has been processed.
  PP_TimeTicks pending_user_gesture_;

  // The Flash proxy is associated with the instance.
  PPB_Flash_Impl flash_impl_;

  // We store the arguments so we can re-send them if we are reset to talk to
  // NaCl via the IPC NaCl proxy.
  std::vector<std::string> argn_;
  std::vector<std::string> argv_;

  // This is NULL unless HandleDocumentLoad has called. In that case, we store
  // the pointer so we can re-send it later if we are reset to talk to NaCl.
  scoped_refptr<PPB_URLLoader_Impl> document_loader_;

  media::DecryptorClient* decryptor_client_;
  uint32_t next_decryption_request_id_;
  typedef std::map<uint32_t, media::Decryptor::DecryptCB> DecryptionCBMap;
  DecryptionCBMap pending_decryption_cbs_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstance);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPAPI_PLUGIN_INSTANCE_H_
