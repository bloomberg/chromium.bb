/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// How ownership works
// -------------------
//
// Big oh represents a refcounted relationship: owner O--- ownee
//
// WebView (for the toplevel frame only)
//    O
//    |           WebFrame
//    |              O
//    |              |
//   Page O------- LocalFrame (main_frame_) O-------O LocalFrameView
//                   ||
//                   ||
//               FrameLoader
//
// FrameLoader and LocalFrame are formerly one object that was split apart
// because it got too big. They basically have the same lifetime, hence the
// double line.
//
// From the perspective of the embedder, WebFrame is simply an object that it
// allocates by calling WebFrame::create() and must be freed by calling close().
// Internally, WebFrame is actually refcounted and it holds a reference to its
// corresponding LocalFrame in blink.
//
// Oilpan: the middle objects + Page in the above diagram are Oilpan heap
// allocated, WebView and LocalFrameView are currently not. In terms of
// ownership and control, the relationships stays the same, but the references
// from the off-heap WebView to the on-heap Page is handled by a Persistent<>,
// not a scoped_refptr<>. Similarly, the mutual strong references between the
// on-heap LocalFrame and the off-heap LocalFrameView is through a RefPtr (from
// LocalFrame to LocalFrameView), and a Persistent refers to the LocalFrame in
// the other direction.
//
// From the embedder's point of view, the use of Oilpan brings no changes.
// close() must still be used to signal that the embedder is through with the
// WebFrame.  Calling it will bring about the release and finalization of the
// frame object, and everything underneath.
//
// How frames are destroyed
// ------------------------
//
// The main frame is never destroyed and is re-used. The FrameLoader is re-used
// and a reference to the main frame is kept by the Page.
//
// When frame content is replaced, all subframes are destroyed. This happens
// in Frame::detachChildren for each subframe in a pre-order depth-first
// traversal. Note that child node order may not match DOM node order!
// detachChildren() (virtually) calls Frame::detach(), which again calls
// LocalFrameClient::detached(). This triggers WebFrame to clear its reference
// to LocalFrame. LocalFrameClient::detached() also notifies the embedder via
// WebLocalFrameClient that the frame is detached. Most embedders will invoke
// close() on the WebFrame at this point, triggering its deletion unless
// something else is still retaining a reference.
//
// The client is expected to be set whenever the WebLocalFrameImpl is attached
// to the DOM.

#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "base/macros.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_double_size.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_isolated_world_info.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_content_capture_client.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_dom_event.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_icon_url.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_manifest_manager.h"
#include "third_party/blink/public/web/web_media_player_action.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_performance.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_print_preset_options.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/public/web/web_text_direction.h"
#include "third_party/blink/public/web/web_tree_scope_type.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/isolated_world_csp.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/icon_url.h"
#include "third_party/blink/renderer/core/dom/ignore_opens_during_unload_count_incrementer.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_in_page_coordinates.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/ime_text_span_vector_builder.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/writing_direction.h"
#include "third_party/blink/renderer/core/events/after_print_event.h"
#include "third_party/blink/renderer/core/events/before_print_event.h"
#include "third_party/blink/renderer/core/events/portal_activate_event.h"
#include "third_party/blink/renderer/core/exported/local_frame_client_impl.h"
#include "third_party/blink/renderer/core/exported/web_associated_url_loader_impl.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/exported/web_document_loader_impl.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/find_in_page.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/pausable_script_executor.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_owner.h"
#include "third_party/blink/renderer/core/frame/screen_orientation_controller.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/smart_clip.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/html/portal/document_portals.h"
#include "third_party/blink/renderer/core/html/portal/dom_window_portal_host.h"
#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"
#include "third_party/blink/renderer/core/html/portal/portal_host.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/print_context.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer_client.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

static int g_frame_count = 0;

namespace {

HeapVector<ScriptSourceCode> CreateSourcesVector(
    const WebScriptSource* sources_in,
    unsigned num_sources) {
  HeapVector<ScriptSourceCode> sources;
  sources.Append(sources_in, num_sources);
  return sources;
}

}  // namespace

// Simple class to override some of PrintContext behavior. Some of the methods
// made virtual so that they can be overridden by ChromePluginPrintContext.
class ChromePrintContext : public PrintContext {
 public:
  ChromePrintContext(LocalFrame* frame, bool use_printing_layout)
      : PrintContext(frame, use_printing_layout), printed_page_width_(0) {}

  ~ChromePrintContext() override = default;

  void BeginPrintMode(float width, float height) override {
    DCHECK(!printed_page_width_);
    printed_page_width_ = width;
    printed_page_height_ = height;
    PrintContext::BeginPrintMode(printed_page_width_, height);
  }

  virtual float GetPageShrink(int page_number) const {
    IntRect page_rect = page_rects_[page_number];
    return printed_page_width_ / page_rect.Width();
  }

  float SpoolSinglePage(cc::PaintCanvas* canvas, int page_number) {
    DispatchEventsForPrintingOnAllFrames();
    if (!GetFrame()->GetDocument() ||
        !GetFrame()->GetDocument()->GetLayoutView())
      return 0;

    GetFrame()->View()->UpdateLifecyclePhasesForPrinting();
    if (!GetFrame()->GetDocument() ||
        !GetFrame()->GetDocument()->GetLayoutView())
      return 0;

    // The page rect gets scaled and translated, so specify the entire
    // print content area here as the recording rect.
    FloatRect bounds(0, 0, printed_page_height_, printed_page_width_);
    PaintRecordBuilder builder(canvas->GetPrintingMetafile());
    builder.Context().SetPrinting(true);
    builder.Context().BeginRecording(bounds);
    float scale = SpoolPage(builder.Context(), page_number);
    canvas->drawPicture(builder.Context().EndRecording());
    return scale;
  }

  void SpoolAllPagesWithBoundariesForTesting(
      cc::PaintCanvas* canvas,
      const FloatSize& page_size_in_pixels) {
    DispatchEventsForPrintingOnAllFrames();
    if (!GetFrame()->GetDocument() ||
        !GetFrame()->GetDocument()->GetLayoutView())
      return;

    GetFrame()->View()->UpdateLifecyclePhasesForPrinting();
    if (!GetFrame()->GetDocument() ||
        !GetFrame()->GetDocument()->GetLayoutView())
      return;

    ComputePageRects(page_size_in_pixels);

    const float page_width = page_size_in_pixels.Width();
    wtf_size_t num_pages = PageRects().size();
    int total_height = num_pages * (page_size_in_pixels.Height() + 1) - 1;
    FloatRect all_pages_rect(0, 0, page_width, total_height);

    PaintRecordBuilder builder(canvas->GetPrintingMetafile());
    GraphicsContext& context = builder.Context();
    context.SetPrinting(true);
    context.BeginRecording(all_pages_rect);

    // Fill the whole background by white.
    context.FillRect(all_pages_rect, Color::kWhite);

    int current_height = 0;
    for (wtf_size_t page_index = 0; page_index < num_pages; page_index++) {
      // Draw a line for a page boundary if this isn't the first page.
      if (page_index > 0) {
        context.Save();
        context.SetStrokeThickness(1);
        context.SetStrokeColor(Color(0, 0, 255));
        context.DrawLine(IntPoint(0, current_height - 1),
                         IntPoint(page_width, current_height - 1));
        context.Restore();
      }

      AffineTransform transform;
      transform.Translate(0, current_height);
#if defined(OS_WIN) || defined(OS_MACOSX)
      // Account for the disabling of scaling in spoolPage. In the context of
      // SpoolAllPagesWithBoundariesForTesting the scale HAS NOT been
      // pre-applied.
      float scale = GetPageShrink(page_index);
      transform.Scale(scale, scale);
#endif
      context.Save();
      context.ConcatCTM(transform);

      SpoolPage(context, page_index);

      context.Restore();

      current_height += page_size_in_pixels.Height() + 1;
    }
    canvas->drawPicture(context.EndRecording());
  }

 protected:
  // Spools the printed page, a subrect of frame(). Skip the scale step.
  // NativeTheme doesn't play well with scaling. Scaling is done browser side
  // instead. Returns the scale to be applied.
  // On Linux, we don't have the problem with NativeTheme, hence we let WebKit
  // do the scaling and ignore the return value.
  virtual float SpoolPage(GraphicsContext& context, int page_number) {
    IntRect page_rect = page_rects_[page_number];
    float scale = printed_page_width_ / page_rect.Width();

    AffineTransform transform;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    transform.Scale(scale);
#endif
    transform.Translate(static_cast<float>(-page_rect.X()),
                        static_cast<float>(-page_rect.Y()));
    context.Save();
    context.ConcatCTM(transform);
    context.ClipRect(page_rect);

    auto* frame_view = GetFrame()->View();
    DCHECK(frame_view);
    PropertyTreeState property_tree_state =
        frame_view->GetLayoutView()->FirstFragment().LocalBorderBoxProperties();

    PaintRecordBuilder builder(context.Canvas()->GetPrintingMetafile(),
                               &context);

    frame_view->PaintContentsOutsideOfLifecycle(
        builder.Context(), kGlobalPaintNormalPhase, CullRect(page_rect));
    {
      ScopedPaintChunkProperties scoped_paint_chunk_properties(
          builder.Context().GetPaintController(), property_tree_state, builder,
          DisplayItem::kPrintedContentDestinationLocations);
      DrawingRecorder line_boundary_recorder(
          builder.Context(), builder,
          DisplayItem::kPrintedContentDestinationLocations);
      OutputLinkedDestinations(builder.Context(), page_rect);
    }

    context.DrawRecord(builder.EndRecording(property_tree_state));
    context.Restore();

    return scale;
  }

 private:
  void DispatchEventsForPrintingOnAllFrames() {
    HeapVector<Member<Document>> documents;
    for (Frame* current_frame = GetFrame(); current_frame;
         current_frame = current_frame->Tree().TraverseNext(GetFrame())) {
      if (auto* current_local_frame = DynamicTo<LocalFrame>(current_frame))
        documents.push_back(current_local_frame->GetDocument());
    }

    for (auto& doc : documents)
      doc->DispatchEventsForPrinting();
  }

  // Set when printing.
  float printed_page_width_;
  float printed_page_height_;

  DISALLOW_COPY_AND_ASSIGN(ChromePrintContext);
};

// Simple class to override some of PrintContext behavior. This is used when
// the frame hosts a plugin that supports custom printing. In this case, we
// want to delegate all printing related calls to the plugin.
class ChromePluginPrintContext final : public ChromePrintContext {
 public:
  ChromePluginPrintContext(LocalFrame* frame,
                           WebPluginContainerImpl* plugin,
                           const WebPrintParams& print_params)
      : ChromePrintContext(frame, print_params.use_printing_layout),
        plugin_(plugin),
        print_params_(print_params) {}

  ~ChromePluginPrintContext() override = default;

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(plugin_);
    ChromePrintContext::Trace(visitor);
  }

  void BeginPrintMode(float width, float height) override {}

  void EndPrintMode() override { plugin_->PrintEnd(); }

  float GetPageShrink(int page_number) const override {
    // We don't shrink the page (maybe we should ask the widget ??)
    return 1.0;
  }

  void ComputePageRects(const FloatSize& print_size) override {
    IntRect rect(IntPoint(0, 0), FlooredIntSize(print_size));
    print_params_.print_content_area = rect;
    page_rects_.Fill(rect, plugin_->PrintBegin(print_params_));
  }

  void ComputePageRectsWithPageSize(
      const FloatSize& page_size_in_pixels) override {
    NOTREACHED();
  }

 protected:
  // Spools the printed page, a subrect of frame(). Skip the scale step.
  // NativeTheme doesn't play well with scaling. Scaling is done browser side
  // instead. Returns the scale to be applied.
  float SpoolPage(GraphicsContext& context, int page_number) override {
    PaintRecordBuilder builder(context.Canvas()->GetPrintingMetafile());
    plugin_->PrintPage(page_number, builder.Context());
    context.DrawRecord(builder.EndRecording());

    return 1.0;
  }

 private:
  // Set when printing.
  Member<WebPluginContainerImpl> plugin_;
  WebPrintParams print_params_;
};

static WebDocumentLoader* DocumentLoaderForDocLoader(DocumentLoader* loader) {
  return loader ? WebDocumentLoaderImpl::FromDocumentLoader(loader) : nullptr;
}

// WebFrame -------------------------------------------------------------------

int WebFrame::InstanceCount() {
  return g_frame_count;
}

WebLocalFrame* WebLocalFrame::FrameForCurrentContext() {
  v8::Local<v8::Context> context =
      v8::Isolate::GetCurrent()->GetCurrentContext();
  if (context.IsEmpty())
    return nullptr;
  return FrameForContext(context);
}

WebLocalFrame* WebLocalFrame::FrameForContext(v8::Local<v8::Context> context) {
  return WebLocalFrameImpl::FromFrame(ToLocalFrameIfNotDetached(context));
}

WebLocalFrame* WebLocalFrame::FromFrameOwnerElement(const WebElement& element) {
  return WebLocalFrameImpl::FromFrameOwnerElement(element);
}

bool WebLocalFrameImpl::IsWebLocalFrame() const {
  return true;
}

WebLocalFrame* WebLocalFrameImpl::ToWebLocalFrame() {
  return this;
}

bool WebLocalFrameImpl::IsWebRemoteFrame() const {
  return false;
}

WebRemoteFrame* WebLocalFrameImpl::ToWebRemoteFrame() {
  NOTREACHED();
  return nullptr;
}

void WebLocalFrameImpl::Close() {
  WebLocalFrame::Close();

  client_ = nullptr;

  if (dev_tools_agent_)
    dev_tools_agent_.Clear();

  self_keep_alive_.Clear();

  if (print_context_)
    PrintEnd();
#if DCHECK_IS_ON()
  is_in_printing_ = false;
#endif
}

WebString WebLocalFrameImpl::AssignedName() const {
  return GetFrame()->Tree().GetName();
}

void WebLocalFrameImpl::SetName(const WebString& name) {
  GetFrame()->Tree().SetName(name, FrameTree::kReplicate);
}

WebVector<WebIconURL> WebLocalFrameImpl::IconURLs(int icon_types_mask) const {
  // The URL to the icon may be in the header. As such, only
  // ask the loader for the icon if it's finished loading.
  if (GetFrame()->GetDocument()->LoadEventFinished())
    return GetFrame()->GetDocument()->IconURLs(icon_types_mask);
  return WebVector<WebIconURL>();
}

void WebLocalFrameImpl::SetContentSettingsClient(
    WebContentSettingsClient* client) {
  content_settings_client_ = client;
}

ScrollableArea* WebLocalFrameImpl::LayoutViewport() const {
  if (LocalFrameView* view = GetFrameView())
    return view->LayoutViewport();
  return nullptr;
}

bool WebLocalFrameImpl::IsFocused() const {
  if (!ViewImpl() || !ViewImpl()->GetPage())
    return false;

  return this ==
         WebFrame::FromFrame(
             ViewImpl()->GetPage()->GetFocusController().FocusedFrame());
}

bool WebLocalFrameImpl::UsePrintingLayout() const {
  return print_context_ ? print_context_->use_printing_layout() : false;
}

WebSize WebLocalFrameImpl::GetScrollOffset() const {
  if (ScrollableArea* scrollable_area = LayoutViewport())
    return scrollable_area->ScrollOffsetInt();
  return WebSize();
}

void WebLocalFrameImpl::SetScrollOffset(const WebSize& offset) {
  if (ScrollableArea* scrollable_area = LayoutViewport()) {
    scrollable_area->SetScrollOffset(ScrollOffset(offset.width, offset.height),
                                     kProgrammaticScroll);
  }
}

WebSize WebLocalFrameImpl::DocumentSize() const {
  if (!GetFrameView() || !GetFrameView()->GetLayoutView())
    return WebSize();

  return GetFrameView()->GetLayoutView()->DocumentRect().Size();
}

bool WebLocalFrameImpl::HasVisibleContent() const {
  auto* layout_object = GetFrame()->OwnerLayoutObject();
  if (layout_object &&
      layout_object->StyleRef().Visibility() != EVisibility::kVisible) {
    return false;
  }

  if (LocalFrameView* view = GetFrameView())
    return view->Width() > 0 && view->Height() > 0;
  return false;
}

WebRect WebLocalFrameImpl::VisibleContentRect() const {
  if (LocalFrameView* view = GetFrameView())
    return view->LayoutViewport()->VisibleContentRect();
  return WebRect();
}

WebView* WebLocalFrameImpl::View() const {
  return ViewImpl();
}

WebDocument WebLocalFrameImpl::GetDocument() const {
  if (!GetFrame() || !GetFrame()->GetDocument())
    return WebDocument();
  return WebDocument(GetFrame()->GetDocument());
}

WebPerformance WebLocalFrameImpl::Performance() const {
  if (!GetFrame())
    return WebPerformance();
  return WebPerformance(
      DOMWindowPerformance::performance(*(GetFrame()->DomWindow())));
}

bool WebLocalFrameImpl::IsAdSubframe() const {
  DCHECK(GetFrame());
  return GetFrame()->IsAdSubframe();
}

void WebLocalFrameImpl::SetIsAdSubframe(
    blink::mojom::AdFrameType ad_frame_type) {
  DCHECK(GetFrame());
  GetFrame()->SetIsAdSubframe(ad_frame_type);
}

void WebLocalFrameImpl::DispatchUnloadEvent() {
  if (!GetFrame())
    return;
  SubframeLoadingDisabler disabler(GetFrame()->GetDocument());
  // https://html.spec.whatwg.org/C/browsing-the-web.html#unload-a-document
  // The ignore-opens-during-unload counter of a Document must be incremented
  // when unloading itself.
  IgnoreOpensDuringUnloadCountIncrementer ignore_opens_during_unload(
      GetFrame()->GetDocument());
  GetFrame()->Loader().DispatchUnloadEvent();
}

void WebLocalFrameImpl::ExecuteScript(const WebScriptSource& source) {
  DCHECK(GetFrame());
  v8::HandleScope handle_scope(ToIsolate(GetFrame()));
  GetFrame()->GetScriptController().ExecuteScriptInMainWorld(
      source, KURL(), SanitizeScriptErrors::kSanitize);
}

void WebLocalFrameImpl::ExecuteScriptInIsolatedWorld(
    int world_id,
    const WebScriptSource& source_in) {
  DCHECK(GetFrame());
  CHECK_GT(world_id, DOMWrapperWorld::kMainWorldId);
  CHECK_LT(world_id, DOMWrapperWorld::kDOMWrapperWorldEmbedderWorldIdLimit);

  // Note: An error event in an isolated world will never be dispatched to
  // a foreign world.
  v8::HandleScope handle_scope(ToIsolate(GetFrame()));
  GetFrame()->GetScriptController().ExecuteScriptInIsolatedWorld(
      world_id, source_in, KURL(), SanitizeScriptErrors::kDoNotSanitize);
}

v8::Local<v8::Value>
WebLocalFrameImpl::ExecuteScriptInIsolatedWorldAndReturnValue(
    int world_id,
    const WebScriptSource& source_in) {
  DCHECK(GetFrame());
  CHECK_GT(world_id, DOMWrapperWorld::kMainWorldId);
  CHECK_LT(world_id, DOMWrapperWorld::kDOMWrapperWorldEmbedderWorldIdLimit);

  // Note: An error event in an isolated world will never be dispatched to
  // a foreign world.
  return GetFrame()->GetScriptController().ExecuteScriptInIsolatedWorld(
      world_id, source_in, KURL(), SanitizeScriptErrors::kDoNotSanitize);
}

void WebLocalFrameImpl::ClearIsolatedWorldCSPForTesting(int world_id) {
  if (!GetFrame())
    return;
  if (world_id <= DOMWrapperWorld::kMainWorldId ||
      world_id >= DOMWrapperWorld::kDOMWrapperWorldEmbedderWorldIdLimit) {
    return;
  }

  GetFrame()->GetDocument()->ClearIsolatedWorldCSPForTesting(world_id);
}

void WebLocalFrameImpl::SetIsolatedWorldInfo(int world_id,
                                             const WebIsolatedWorldInfo& info) {
  DCHECK(GetFrame());
  CHECK_GT(world_id, DOMWrapperWorld::kMainWorldId);
  CHECK_LT(world_id, DOMWrapperWorld::kDOMWrapperWorldEmbedderWorldIdLimit);

  scoped_refptr<SecurityOrigin> security_origin =
      info.security_origin.Get() ? info.security_origin.Get()->IsolatedCopy()
                                 : nullptr;

  CHECK(info.content_security_policy.IsNull() || security_origin);

  DOMWrapperWorld::SetIsolatedWorldSecurityOrigin(world_id, security_origin);
  DOMWrapperWorld::SetNonMainWorldHumanReadableName(world_id,
                                                    info.human_readable_name);
  IsolatedWorldCSP::Get().SetContentSecurityPolicy(
      world_id, info.content_security_policy, security_origin);
}

void WebLocalFrameImpl::Alert(const WebString& message) {
  DCHECK(GetFrame());
  ScriptState* script_state = ToScriptStateForMainWorld(GetFrame());
  DCHECK(script_state);
  GetFrame()->DomWindow()->alert(script_state, message);
}

bool WebLocalFrameImpl::Confirm(const WebString& message) {
  DCHECK(GetFrame());
  ScriptState* script_state = ToScriptStateForMainWorld(GetFrame());
  DCHECK(script_state);
  return GetFrame()->DomWindow()->confirm(script_state, message);
}

WebString WebLocalFrameImpl::Prompt(const WebString& message,
                                    const WebString& default_value) {
  DCHECK(GetFrame());
  ScriptState* script_state = ToScriptStateForMainWorld(GetFrame());
  DCHECK(script_state);
  return GetFrame()->DomWindow()->prompt(script_state, message, default_value);
}

void WebLocalFrameImpl::CollectGarbageForTesting() {
  if (!GetFrame())
    return;
  if (!GetFrame()->GetSettings()->GetScriptEnabled())
    return;
  V8GCController::CollectAllGarbageForTesting(v8::Isolate::GetCurrent());
}

v8::Local<v8::Value> WebLocalFrameImpl::ExecuteScriptAndReturnValue(
    const WebScriptSource& source) {
  DCHECK(GetFrame());

  return GetFrame()
      ->GetScriptController()
      .ExecuteScriptInMainWorldAndReturnValue(source, KURL(),
                                              SanitizeScriptErrors::kSanitize);
}

void WebLocalFrameImpl::RequestExecuteScriptAndReturnValue(
    const WebScriptSource& source,
    bool user_gesture,
    WebScriptExecutionCallback* callback) {
  DCHECK(GetFrame());

  scoped_refptr<DOMWrapperWorld> main_world = &DOMWrapperWorld::MainWorld();
  auto* executor = MakeGarbageCollected<PausableScriptExecutor>(
      GetFrame(), std::move(main_world), CreateSourcesVector(&source, 1),
      user_gesture, callback);
  executor->Run();
}

void WebLocalFrameImpl::RequestExecuteV8Function(
    v8::Local<v8::Context> context,
    v8::Local<v8::Function> function,
    v8::Local<v8::Value> receiver,
    int argc,
    v8::Local<v8::Value> argv[],
    WebScriptExecutionCallback* callback) {
  DCHECK(GetFrame());
  PausableScriptExecutor::CreateAndRun(GetFrame(), ToIsolate(GetFrame()),
                                       context, function, receiver, argc, argv,
                                       callback);
}

void WebLocalFrameImpl::RequestExecuteScriptInIsolatedWorld(
    int world_id,
    const WebScriptSource* sources_in,
    unsigned num_sources,
    bool user_gesture,
    ScriptExecutionType option,
    WebScriptExecutionCallback* callback) {
  DCHECK(GetFrame());
  CHECK_GT(world_id, DOMWrapperWorld::kMainWorldId);
  CHECK_LT(world_id, DOMWrapperWorld::kDOMWrapperWorldEmbedderWorldIdLimit);

  scoped_refptr<DOMWrapperWorld> isolated_world =
      DOMWrapperWorld::EnsureIsolatedWorld(ToIsolate(GetFrame()), world_id);
  auto* executor = MakeGarbageCollected<PausableScriptExecutor>(
      GetFrame(), std::move(isolated_world),
      CreateSourcesVector(sources_in, num_sources), user_gesture, callback);
  switch (option) {
    case kAsynchronousBlockingOnload:
      executor->RunAsync(PausableScriptExecutor::kOnloadBlocking);
      break;
    case kAsynchronous:
      executor->RunAsync(PausableScriptExecutor::kNonBlocking);
      break;
    case kSynchronous:
      executor->Run();
      break;
  }
}

v8::MaybeLocal<v8::Value> WebLocalFrameImpl::CallFunctionEvenIfScriptDisabled(
    v8::Local<v8::Function> function,
    v8::Local<v8::Value> receiver,
    int argc,
    v8::Local<v8::Value> argv[]) {
  DCHECK(GetFrame());
  return V8ScriptRunner::CallFunction(
      function, GetFrame()->GetDocument(), receiver, argc,
      static_cast<v8::Local<v8::Value>*>(argv), ToIsolate(GetFrame()));
}

v8::Local<v8::Context> WebLocalFrameImpl::MainWorldScriptContext() const {
  ScriptState* script_state = ToScriptStateForMainWorld(GetFrame());
  DCHECK(script_state);
  return script_state->GetContext();
}

v8::Local<v8::Object> WebLocalFrameImpl::GlobalProxy() const {
  return MainWorldScriptContext()->Global();
}

bool WebFrame::ScriptCanAccess(WebFrame* target) {
  return BindingSecurity::ShouldAllowAccessToFrame(
      CurrentDOMWindow(V8PerIsolateData::MainThreadIsolate()),
      ToCoreFrame(*target), BindingSecurity::ErrorReportOption::kDoNotReport);
}

void WebLocalFrameImpl::StartReload(WebFrameLoadType frame_load_type) {
  // TODO(clamy): Remove this function once RenderFrame calls StartNavigation
  // for all requests.
  DCHECK(GetFrame());
  DCHECK(IsReloadLoadType(frame_load_type));
  ResourceRequest request =
      GetFrame()->Loader().ResourceRequestForReload(frame_load_type);
  if (request.IsNull())
    return;
  request.SetRequestorOrigin(GetFrame()->GetDocument()->GetSecurityOrigin());
  if (GetTextFinder())
    GetTextFinder()->ClearActiveFindMatch();

  GetFrame()->Loader().StartNavigation(FrameLoadRequest(nullptr, request),
                                       frame_load_type);
}

void WebLocalFrameImpl::ReloadImage(const WebNode& web_node) {
  Node* node = web_node;  // Use implicit WebNode->Node* cast.
  HitTestResult hit_test_result;
  hit_test_result.SetInnerNode(node);
  hit_test_result.SetToShadowHostIfInRestrictedShadowRoot();
  node = hit_test_result.InnerNodeOrImageMapImage();
  if (auto* image_element = ToHTMLImageElementOrNull(*node))
    image_element->ForceReload();
}

void WebLocalFrameImpl::ReloadLoFiImages() {
  GetFrame()->GetDocument()->Fetcher()->ReloadLoFiImages();
}

void WebLocalFrameImpl::StartNavigation(const WebURLRequest& request) {
  // TODO(clamy): Remove this function once RenderFrame calls CommitNavigation
  // for all requests.
  DCHECK(GetFrame());
  DCHECK(!request.IsNull());
  DCHECK(!request.Url().ProtocolIs("javascript"));
  if (GetTextFinder())
    GetTextFinder()->ClearActiveFindMatch();

  GetFrame()->Loader().StartNavigation(
      FrameLoadRequest(nullptr, request.ToResourceRequest()),
      WebFrameLoadType::kStandard);
}

void WebLocalFrameImpl::CheckCompleted() {
  GetFrame()->GetDocument()->CheckCompleted();
}

void WebLocalFrameImpl::StopLoading() {
  if (!GetFrame())
    return;
  // FIXME: Figure out what we should really do here. It seems like a bug
  // that FrameLoader::stopLoading doesn't call stopAllLoaders.
  GetFrame()->Loader().StopAllLoaders();
}

WebDocumentLoader* WebLocalFrameImpl::GetDocumentLoader() const {
  DCHECK(GetFrame());
  return DocumentLoaderForDocLoader(GetFrame()->Loader().GetDocumentLoader());
}

void WebLocalFrameImpl::EnableViewSourceMode(bool enable) {
  if (GetFrame())
    GetFrame()->SetInViewSourceMode(enable);
}

bool WebLocalFrameImpl::IsViewSourceModeEnabled() const {
  if (!GetFrame())
    return false;
  return GetFrame()->InViewSourceMode();
}

void WebLocalFrameImpl::SetReferrerForRequest(WebURLRequest& request,
                                              const WebURL& referrer_url) {
  String referrer = referrer_url.IsEmpty()
                        ? GetFrame()->GetDocument()->OutgoingReferrer()
                        : String(referrer_url.GetString());
  request.ToMutableResourceRequest().SetHttpReferrer(
      SecurityPolicy::GenerateReferrer(
          GetFrame()->GetDocument()->GetReferrerPolicy(), request.Url(),
          referrer));
}

WebAssociatedURLLoader* WebLocalFrameImpl::CreateAssociatedURLLoader(
    const WebAssociatedURLLoaderOptions& options) {
  return new WebAssociatedURLLoaderImpl(GetFrame()->GetDocument(), options);
}

void WebLocalFrameImpl::ReplaceSelection(const WebString& text) {
  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  GetFrame()->GetEditor().ReplaceSelection(text);
}

void WebLocalFrameImpl::SetMarkedText(const WebString& text,
                                      unsigned location,
                                      unsigned length) {
  Vector<ImeTextSpan> decorations;
  GetFrame()->GetInputMethodController().SetComposition(text, decorations,
                                                        location, length);
}

void WebLocalFrameImpl::UnmarkText() {
  GetFrame()->GetInputMethodController().CancelComposition();
}

bool WebLocalFrameImpl::HasMarkedText() const {
  return GetFrame()->GetInputMethodController().HasComposition();
}

WebRange WebLocalFrameImpl::MarkedRange() const {
  return GetFrame()->GetInputMethodController().CompositionEphemeralRange();
}

bool WebLocalFrameImpl::FirstRectForCharacterRange(
    unsigned location,
    unsigned length,
    WebRect& rect_in_viewport) const {
  if ((location + length < location) && (location + length))
    length = 0;

  Element* editable =
      GetFrame()->Selection().RootEditableElementOrDocumentElement();
  if (!editable)
    return false;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  editable->GetDocument().UpdateStyleAndLayout();

  const EphemeralRange range =
      PlainTextRange(location, location + length).CreateRange(*editable);
  if (range.IsNull())
    return false;
  IntRect int_rect = FirstRectForRange(range);
  rect_in_viewport = WebRect(int_rect);
  rect_in_viewport = GetFrame()->View()->FrameToViewport(rect_in_viewport);
  return true;
}

size_t WebLocalFrameImpl::CharacterIndexForPoint(
    const WebPoint& point_in_viewport) const {
  if (!GetFrame())
    return kNotFound;

  HitTestLocation location(
      GetFrame()->View()->ViewportToFrame(point_in_viewport));
  HitTestResult result = GetFrame()->GetEventHandler().HitTestResultAtLocation(
      location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  return GetFrame()->Selection().CharacterIndexForPoint(
      result.RoundedPointInInnerNodeFrame());
}

bool WebLocalFrameImpl::ExecuteCommand(const WebString& name) {
  DCHECK(GetFrame());

  if (name.length() <= 2)
    return false;

  // Since we don't have NSControl, we will convert the format of command
  // string and call the function on Editor directly.
  String command = name;

  // Make sure the first letter is upper case.
  command.replace(0, 1, command.Substring(0, 1).UpperASCII());

  // Remove the trailing ':' if existing.
  if (command[command.length() - 1] == UChar(':'))
    command = command.Substring(0, command.length() - 1);

  Node* plugin_lookup_context_node = nullptr;
  if (WebPluginContainerImpl::SupportsCommand(name))
    plugin_lookup_context_node = ContextMenuNodeInner();

  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      LocalFrame::NotifyUserActivation(GetFrame(),
                                       UserGestureToken::kNewGesture);

  WebPluginContainerImpl* plugin_container =
      GetFrame()->GetWebPluginContainer(plugin_lookup_context_node);
  if (plugin_container && plugin_container->ExecuteEditCommand(name))
    return true;

  return GetFrame()->GetEditor().ExecuteCommand(command);
}

bool WebLocalFrameImpl::ExecuteCommand(const WebString& name,
                                       const WebString& value) {
  DCHECK(GetFrame());

  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      LocalFrame::NotifyUserActivation(GetFrame(),
                                       UserGestureToken::kNewGesture);

  WebPluginContainerImpl* plugin_container =
      GetFrame()->GetWebPluginContainer();
  if (plugin_container && plugin_container->ExecuteEditCommand(name, value))
    return true;

  return GetFrame()->GetEditor().ExecuteCommand(name, value);
}

bool WebLocalFrameImpl::IsCommandEnabled(const WebString& name) const {
  DCHECK(GetFrame());
  return GetFrame()->GetEditor().IsCommandEnabled(name);
}

bool WebLocalFrameImpl::SelectionTextDirection(WebTextDirection& start,
                                               WebTextDirection& end) const {
  FrameSelection& selection = frame_->Selection();
  if (!selection.IsAvailable()) {
    // plugins/mouse-capture-inside-shadow.html reaches here
    return false;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame_->GetDocument()->UpdateStyleAndLayout();

  if (selection.ComputeVisibleSelectionInDOMTree()
          .ToNormalizedEphemeralRange()
          .IsNull())
    return false;
  start = ToWebTextDirection(PrimaryDirectionOf(
      *selection.ComputeVisibleSelectionInDOMTree().Start().AnchorNode()));
  end = ToWebTextDirection(PrimaryDirectionOf(
      *selection.ComputeVisibleSelectionInDOMTree().End().AnchorNode()));
  return true;
}

bool WebLocalFrameImpl::IsSelectionAnchorFirst() const {
  FrameSelection& selection = frame_->Selection();
  if (!selection.IsAvailable()) {
    // plugins/mouse-capture-inside-shadow.html reaches here
    return false;
  }

  return selection.GetSelectionInDOMTree().IsBaseFirst();
}

void WebLocalFrameImpl::SetTextDirection(WebTextDirection direction) {
  // The Editor::SetBaseWritingDirection() function checks if we can change
  // the text direction of the selected node and updates its DOM "dir"
  // attribute and its CSS "direction" property.
  // So, we just call the function as Safari does.
  Editor& editor = frame_->GetEditor();
  if (!editor.CanEdit())
    return;

  switch (direction) {
    case kWebTextDirectionDefault:
      editor.SetBaseWritingDirection(WritingDirection::kNatural);
      break;

    case kWebTextDirectionLeftToRight:
      editor.SetBaseWritingDirection(WritingDirection::kLeftToRight);
      break;

    case kWebTextDirectionRightToLeft:
      editor.SetBaseWritingDirection(WritingDirection::kRightToLeft);
      break;

    default:
      NOTIMPLEMENTED();
      break;
  }
}

void WebLocalFrameImpl::ReplaceMisspelledRange(const WebString& text) {
  // If this caret selection has two or more markers, this function replace the
  // range covered by the first marker with the specified word as Microsoft Word
  // does.
  if (GetFrame()->GetWebPluginContainer())
    return;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  GetFrame()->GetSpellChecker().ReplaceMisspelledRange(text);
}

void WebLocalFrameImpl::RemoveSpellingMarkers() {
  GetFrame()->GetSpellChecker().RemoveSpellingMarkers();
}

void WebLocalFrameImpl::RemoveSpellingMarkersUnderWords(
    const WebVector<WebString>& words) {
  Vector<String> converted_words;
  converted_words.Append(words.Data(), SafeCast<wtf_size_t>(words.size()));
  GetFrame()->RemoveSpellingMarkersUnderWords(converted_words);
}

bool WebLocalFrameImpl::HasSelection() const {
  DCHECK(GetFrame());
  WebPluginContainerImpl* plugin_container =
      GetFrame()->GetWebPluginContainer();
  if (plugin_container)
    return plugin_container->Plugin()->HasSelection();

  // frame()->selection()->isNone() never returns true.
  const auto& selection =
      GetFrame()->Selection().ComputeVisibleSelectionInDOMTreeDeprecated();
  return selection.Start() != selection.End();
}

WebRange WebLocalFrameImpl::SelectionRange() const {
  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  return GetFrame()
      ->Selection()
      .ComputeVisibleSelectionInDOMTreeDeprecated()
      .ToNormalizedEphemeralRange();
}

WebString WebLocalFrameImpl::SelectionAsText() const {
  DCHECK(GetFrame());
  WebPluginContainerImpl* plugin_container =
      GetFrame()->GetWebPluginContainer();
  if (plugin_container)
    return plugin_container->Plugin()->SelectionAsText();

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  String text = GetFrame()->Selection().SelectedText(
      TextIteratorBehavior::EmitsObjectReplacementCharacterBehavior());
#if defined(OS_WIN)
  ReplaceNewlinesWithWindowsStyleNewlines(text);
#endif
  ReplaceNBSPWithSpace(text);
  return text;
}

WebString WebLocalFrameImpl::SelectionAsMarkup() const {
  WebPluginContainerImpl* plugin_container =
      GetFrame()->GetWebPluginContainer();
  if (plugin_container)
    return plugin_container->Plugin()->SelectionAsMarkup();

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // Selection normalization and markup generation require clean layout.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  return GetFrame()->Selection().SelectedHTMLForClipboard();
}

bool WebLocalFrameImpl::SelectWordAroundCaret() {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::selectWordAroundCaret");

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();
  return GetFrame()->Selection().SelectWordAroundCaret();
}

void WebLocalFrameImpl::SelectRange(const WebPoint& base_in_viewport,
                                    const WebPoint& extent_in_viewport) {
  MoveRangeSelection(base_in_viewport, extent_in_viewport);
}

void WebLocalFrameImpl::SelectRange(
    const WebRange& web_range,
    HandleVisibilityBehavior handle_visibility_behavior,
    blink::mojom::SelectionMenuBehavior selection_menu_behavior) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::selectRange");

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  const EphemeralRange& range = web_range.CreateEphemeralRange(GetFrame());
  if (range.IsNull())
    return;

  FrameSelection& selection = GetFrame()->Selection();
  const bool show_handles =
      handle_visibility_behavior == kShowSelectionHandle ||
      (handle_visibility_behavior == kPreserveHandleVisibility &&
       selection.IsHandleVisible());
  using blink::mojom::SelectionMenuBehavior;
  selection.SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(range)
          .SetAffinity(TextAffinity::kDefault)
          .Build(),
      SetSelectionOptions::Builder()
          .SetShouldShowHandle(show_handles)
          .SetShouldShrinkNextTap(selection_menu_behavior ==
                                  SelectionMenuBehavior::kShow)
          .Build());

  if (selection_menu_behavior == SelectionMenuBehavior::kShow) {
    ContextMenuAllowedScope scope;
    GetFrame()->GetEventHandler().ShowNonLocatedContextMenu(
        nullptr, kMenuSourceAdjustSelection);
  }
}

WebString WebLocalFrameImpl::RangeAsText(const WebRange& web_range) {
  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetFrame()->GetDocument()->Lifecycle());

  return PlainText(
      web_range.CreateEphemeralRange(GetFrame()),
      TextIteratorBehavior::EmitsObjectReplacementCharacterBehavior());
}

void WebLocalFrameImpl::MoveRangeSelectionExtent(const WebPoint& point) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::moveRangeSelectionExtent");

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  GetFrame()->Selection().MoveRangeSelectionExtent(
      GetFrame()->View()->ViewportToFrame(point));
}

void WebLocalFrameImpl::MoveRangeSelection(
    const WebPoint& base_in_viewport,
    const WebPoint& extent_in_viewport,
    WebFrame::TextGranularity granularity) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::moveRangeSelection");

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  blink::TextGranularity blink_granularity = blink::TextGranularity::kCharacter;
  if (granularity == WebFrame::kWordGranularity)
    blink_granularity = blink::TextGranularity::kWord;
  GetFrame()->Selection().MoveRangeSelection(
      GetFrame()->View()->ViewportToFrame(base_in_viewport),
      GetFrame()->View()->ViewportToFrame(extent_in_viewport),
      blink_granularity);
}

void WebLocalFrameImpl::MoveCaretSelection(const WebPoint& point_in_viewport) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::moveCaretSelection");

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();
  const IntPoint point_in_contents =
      GetFrame()->View()->ViewportToFrame(point_in_viewport);
  GetFrame()->Selection().MoveCaretSelection(point_in_contents);
}

bool WebLocalFrameImpl::SetEditableSelectionOffsets(int start, int end) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::setEditableSelectionOffsets");

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  return GetFrame()->GetInputMethodController().SetEditableSelectionOffsets(
      PlainTextRange(start, end));
}

bool WebLocalFrameImpl::SetCompositionFromExistingText(
    int composition_start,
    int composition_end,
    const WebVector<WebImeTextSpan>& ime_text_spans) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::setCompositionFromExistingText");
  if (!GetFrame()->GetEditor().CanEdit())
    return false;

  InputMethodController& input_method_controller =
      GetFrame()->GetInputMethodController();

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  input_method_controller.SetCompositionFromExistingText(
      ImeTextSpanVectorBuilder::Build(ime_text_spans), composition_start,
      composition_end);

  return true;
}

void WebLocalFrameImpl::ExtendSelectionAndDelete(int before, int after) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::extendSelectionAndDelete");
  if (WebPlugin* plugin = FocusedPluginIfInputMethodSupported()) {
    plugin->ExtendSelectionAndDelete(before, after);
    return;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  GetFrame()->GetInputMethodController().ExtendSelectionAndDelete(before,
                                                                  after);
}

void WebLocalFrameImpl::DeleteSurroundingText(int before, int after) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::deleteSurroundingText");
  if (WebPlugin* plugin = FocusedPluginIfInputMethodSupported()) {
    plugin->DeleteSurroundingText(before, after);
    return;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  GetFrame()->GetInputMethodController().DeleteSurroundingText(before, after);
}

void WebLocalFrameImpl::DeleteSurroundingTextInCodePoints(int before,
                                                          int after) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::deleteSurroundingTextInCodePoints");
  if (WebPlugin* plugin = FocusedPluginIfInputMethodSupported()) {
    plugin->DeleteSurroundingTextInCodePoints(before, after);
    return;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  GetFrame()->GetInputMethodController().DeleteSurroundingTextInCodePoints(
      before, after);
}

void WebLocalFrameImpl::SetCaretVisible(bool visible) {
  GetFrame()->Selection().SetCaretVisible(visible);
}

WebPlugin* WebLocalFrameImpl::FocusedPluginIfInputMethodSupported() {
  WebPluginContainerImpl* container = GetFrame()->GetWebPluginContainer();
  if (container && container->SupportsInputMethod())
    return container->Plugin();
  return nullptr;
}

void WebLocalFrameImpl::DispatchBeforePrintEvent() {
#if DCHECK_IS_ON()
  DCHECK(!is_in_printing_) << "DispatchAfterPrintEvent() should have been "
                              "called after the previous "
                              "DispatchBeforePrintEvent() call.";
  is_in_printing_ = true;
#endif

  DispatchPrintEventRecursively(event_type_names::kBeforeprint);
}

void WebLocalFrameImpl::DispatchAfterPrintEvent() {
#if DCHECK_IS_ON()
  DCHECK(is_in_printing_) << "DispatchBeforePrintEvent() should be called "
                             "before DispatchAfterPrintEvent().";
  is_in_printing_ = false;
#endif

  if (View())
    DispatchPrintEventRecursively(event_type_names::kAfterprint);
}

void WebLocalFrameImpl::DispatchPrintEventRecursively(
    const AtomicString& event_type) {
  DCHECK(event_type == event_type_names::kBeforeprint ||
         event_type == event_type_names::kAfterprint);

  HeapVector<Member<Frame>> frames;
  for (Frame* frame = frame_; frame; frame = frame->Tree().TraverseNext(frame_))
    frames.push_back(frame);

  for (auto& frame : frames) {
    if (frame->IsRemoteFrame()) {
      // TODO(tkent): Support remote frames. crbug.com/455764.
      continue;
    }
    if (!frame->Tree().IsDescendantOf(frame_))
      continue;
    Event* event =
        event_type == event_type_names::kBeforeprint
            ? static_cast<Event*>(MakeGarbageCollected<BeforePrintEvent>())
            : static_cast<Event*>(MakeGarbageCollected<AfterPrintEvent>());
    To<LocalFrame>(frame.Get())->DomWindow()->DispatchEvent(*event);
  }
}

int WebLocalFrameImpl::PrintBegin(const WebPrintParams& print_params,
                                  const WebNode& constrain_to_node) {
  WebPluginContainerImpl* plugin_container = nullptr;
  if (constrain_to_node.IsNull()) {
    // If this is a plugin document, check if the plugin supports its own
    // printing. If it does, we will delegate all printing to that.
    plugin_container = GetFrame()->GetWebPluginContainer();
  } else {
    // We only support printing plugin nodes for now.
    plugin_container =
        ToWebPluginContainerImpl(constrain_to_node.PluginContainer());
  }

  if (plugin_container && plugin_container->SupportsPaginatedPrint()) {
    print_context_ = MakeGarbageCollected<ChromePluginPrintContext>(
        GetFrame(), plugin_container, print_params);
  } else {
    print_context_ = MakeGarbageCollected<ChromePrintContext>(
        GetFrame(), print_params.use_printing_layout);
  }

  FloatSize size(static_cast<float>(print_params.print_content_area.width),
                 static_cast<float>(print_params.print_content_area.height));
  print_context_->BeginPrintMode(size.Width(), size.Height());
  print_context_->ComputePageRects(size);

  return static_cast<int>(print_context_->PageCount());
}

float WebLocalFrameImpl::GetPrintPageShrink(int page) {
  DCHECK(print_context_);
  DCHECK_GE(page, 0);
  return print_context_->GetPageShrink(page);
}

float WebLocalFrameImpl::PrintPage(int page, cc::PaintCanvas* canvas) {
  DCHECK(print_context_);
  DCHECK_GE(page, 0);
  DCHECK(GetFrame());
  DCHECK(GetFrame()->GetDocument());

  return print_context_->SpoolSinglePage(canvas, page);
}

void WebLocalFrameImpl::PrintEnd() {
  DCHECK(print_context_);
  print_context_->EndPrintMode();
  print_context_.Clear();
}

bool WebLocalFrameImpl::GetPrintPresetOptionsForPlugin(
    const WebNode& node,
    WebPrintPresetOptions* preset_options) {
  WebPluginContainerImpl* plugin_container =
      node.IsNull() ? GetFrame()->GetWebPluginContainer()
                    : ToWebPluginContainerImpl(node.PluginContainer());

  if (!plugin_container || !plugin_container->SupportsPaginatedPrint())
    return false;

  return plugin_container->GetPrintPresetOptionsFromDocument(preset_options);
}

bool WebLocalFrameImpl::HasCustomPageSizeStyle(int page_index) {
  return GetFrame()->GetDocument()->StyleForPage(page_index)->PageSizeType() !=
         EPageSizeType::kAuto;
}

bool WebLocalFrameImpl::IsPageBoxVisible(int page_index) {
  return GetFrame()->GetDocument()->IsPageBoxVisible(page_index);
}

void WebLocalFrameImpl::PageSizeAndMarginsInPixels(int page_index,
                                                   WebDoubleSize& page_size,
                                                   int& margin_top,
                                                   int& margin_right,
                                                   int& margin_bottom,
                                                   int& margin_left) {
  DoubleSize size = page_size;
  GetFrame()->GetDocument()->PageSizeAndMarginsInPixels(
      page_index, size, margin_top, margin_right, margin_bottom, margin_left);
  page_size = size;
}

WebString WebLocalFrameImpl::PageProperty(const WebString& property_name,
                                          int page_index) {
  DCHECK(print_context_);
  return print_context_->PageProperty(GetFrame(), property_name.Utf8().data(),
                                      page_index);
}

void WebLocalFrameImpl::PrintPagesForTesting(
    cc::PaintCanvas* canvas,
    const WebSize& page_size_in_pixels) {
  DCHECK(print_context_);

  print_context_->SpoolAllPagesWithBoundariesForTesting(
      canvas, FloatSize(page_size_in_pixels.width, page_size_in_pixels.height));
}

WebRect WebLocalFrameImpl::GetSelectionBoundsRectForTesting() const {
  return HasSelection()
             ? WebRect(PixelSnappedIntRect(
                   GetFrame()->Selection().AbsoluteUnclippedBounds()))
             : WebRect();
}

WebString WebLocalFrameImpl::GetLayerTreeAsTextForTesting(
    bool show_debug_info) const {
  if (!GetFrame())
    return WebString();

  return WebString(GetFrame()->GetLayerTreeAsTextForTesting(
      show_debug_info ? kLayerTreeIncludesDebugInfo : kLayerTreeNormal));
}

// WebLocalFrameImpl public --------------------------------------------------

WebLocalFrame* WebLocalFrame::CreateMainFrame(
    WebView* web_view,
    WebLocalFrameClient* client,
    InterfaceRegistry* interface_registry,
    mojo::ScopedMessagePipeHandle document_interface_broker_handle,
    WebFrame* opener,
    const WebString& name,
    WebSandboxFlags sandbox_flags,
    const FeaturePolicy::FeatureState& opener_feature_state) {
  return WebLocalFrameImpl::CreateMainFrame(
      web_view, client, interface_registry,
      std::move(document_interface_broker_handle), opener, name, sandbox_flags,
      opener_feature_state);
}

WebLocalFrame* WebLocalFrame::CreateProvisional(
    WebLocalFrameClient* client,
    InterfaceRegistry* interface_registry,
    mojo::ScopedMessagePipeHandle document_interface_broker_handle,
    WebFrame* previous_frame,
    const FramePolicy& frame_policy) {
  return WebLocalFrameImpl::CreateProvisional(
      client, interface_registry, std::move(document_interface_broker_handle),
      previous_frame, frame_policy);
}

WebLocalFrameImpl* WebLocalFrameImpl::CreateMainFrame(
    WebView* web_view,
    WebLocalFrameClient* client,
    InterfaceRegistry* interface_registry,
    mojo::ScopedMessagePipeHandle document_interface_broker_handle,
    WebFrame* opener,
    const WebString& name,
    WebSandboxFlags sandbox_flags,
    const FeaturePolicy::FeatureState& opener_feature_state) {
  WebLocalFrameImpl* frame = MakeGarbageCollected<WebLocalFrameImpl>(
      WebTreeScopeType::kDocument, client, interface_registry,
      std::move(document_interface_broker_handle));
  frame->SetOpener(opener);
  Page& page = *static_cast<WebViewImpl*>(web_view)->GetPage();
  DCHECK(!page.MainFrame());
  frame->InitializeCoreFrame(page, nullptr, name);
  if (RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled())
    frame->GetFrame()->SetOpenerFeatureState(opener_feature_state);
  // Can't force sandbox flags until there's a core frame.
  frame->GetFrame()->Loader().ForceSandboxFlags(
      static_cast<SandboxFlags>(sandbox_flags));
  return frame;
}

WebLocalFrameImpl* WebLocalFrameImpl::CreateProvisional(
    WebLocalFrameClient* client,
    blink::InterfaceRegistry* interface_registry,
    mojo::ScopedMessagePipeHandle document_interface_broker_handle,
    WebFrame* previous_web_frame,
    const FramePolicy& frame_policy) {
  DCHECK(client);
  auto* web_frame = MakeGarbageCollected<WebLocalFrameImpl>(
      previous_web_frame->InShadowTree() ? WebTreeScopeType::kShadow
                                         : WebTreeScopeType::kDocument,
      client, interface_registry, std::move(document_interface_broker_handle));
  Frame* previous_frame = ToCoreFrame(*previous_web_frame);
  web_frame->SetParent(previous_web_frame->Parent());
  web_frame->SetOpener(previous_web_frame->Opener());
  // Note: this *always* temporarily sets a frame owner, even for main frames!
  // When a core Frame is created with no owner, it attempts to set itself as
  // the main frame of the Page. However, this is a provisional frame, and may
  // disappear, so Page::m_mainFrame can't be updated just yet.
  // Note 2: Becuase the dummy owner is still the owner when the initial empty
  // document is created, the initial empty document will not inherit the
  // correct sandbox flags. However, since the provisional frame is inivisible
  // to the rest of the page, the initial document is also invisible and
  // unscriptable. Once the provisional frame gets properly attached and is
  // observable, it will have the real FrameOwner, and any subsequent real
  // documents will correctly inherit sandbox flags from the owner.
  web_frame->InitializeCoreFrame(*previous_frame->GetPage(),
                                 MakeGarbageCollected<DummyFrameOwner>(),
                                 previous_frame->Tree().GetName());

  LocalFrame* new_frame = web_frame->GetFrame();
  new_frame->SetOwner(previous_frame->Owner());
  if (auto* remote_frame_owner =
          DynamicTo<RemoteFrameOwner>(new_frame->Owner())) {
    remote_frame_owner->SetFramePolicy(frame_policy);
  } else if (!new_frame->Owner()) {
    // Provisional main frames need to force sandbox flags.  This is necessary
    // to inherit sandbox flags when a sandboxed frame does a window.open()
    // which triggers a cross-process navigation.
    new_frame->Loader().ForceSandboxFlags(frame_policy.sandbox_flags);
    // If there is an opener (even disowned), the opener policies must be
    // inherited the same way as sandbox flag.
    new_frame->SetOpenerFeatureState(previous_frame->OpenerFeatureState());
  }

  return web_frame;
}

WebLocalFrameImpl* WebLocalFrameImpl::CreateLocalChild(
    WebTreeScopeType scope,
    WebLocalFrameClient* client,
    blink::InterfaceRegistry* interface_registry,
    mojo::ScopedMessagePipeHandle document_interface_broker_handle) {
  WebLocalFrameImpl* frame = MakeGarbageCollected<WebLocalFrameImpl>(
      scope, client, interface_registry,
      std::move(document_interface_broker_handle));
  AppendChild(frame);
  return frame;
}

WebLocalFrameImpl::WebLocalFrameImpl(
    WebTreeScopeType scope,
    WebLocalFrameClient* client,
    blink::InterfaceRegistry* interface_registry,
    mojo::ScopedMessagePipeHandle document_interface_broker_handle)
    : WebNavigationControl(scope),
      client_(client),
      local_frame_client_(MakeGarbageCollected<LocalFrameClientImpl>(
          this,
          std::move(document_interface_broker_handle))),
      autofill_client_(nullptr),
      find_in_page_(
          MakeGarbageCollected<FindInPage>(*this, interface_registry)),
      interface_registry_(interface_registry),
      input_method_controller_(*this),
      spell_check_panel_host_client_(nullptr),
      self_keep_alive_(this) {
  DCHECK(client_);
  g_frame_count++;
  client_->BindToFrame(this);
}

WebLocalFrameImpl::~WebLocalFrameImpl() {
  // The widget for the frame, if any, must have already been closed.
  DCHECK(!frame_widget_);
  g_frame_count--;
}

void WebLocalFrameImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(local_frame_client_);
  visitor->Trace(find_in_page_);
  visitor->Trace(frame_);
  visitor->Trace(dev_tools_agent_);
  visitor->Trace(frame_widget_);
  visitor->Trace(print_context_);
  visitor->Trace(input_method_controller_);
  WebFrame::TraceFrames(visitor, this);
}

void WebLocalFrameImpl::SetCoreFrame(LocalFrame* frame) {
  frame_ = frame;
}

void WebLocalFrameImpl::InitializeCoreFrame(Page& page,
                                            FrameOwner* owner,
                                            const AtomicString& name) {
  SetCoreFrame(MakeGarbageCollected<LocalFrame>(local_frame_client_.Get(), page,
                                                owner, interface_registry_));
  frame_->Tree().SetName(name);
  // We must call init() after frame_ is assigned because it is referenced
  // during init().
  frame_->Init();
  CHECK(frame_);
  CHECK(frame_->Loader().StateMachine()->IsDisplayingInitialEmptyDocument());
  if (!Parent() && !Opener() &&
      frame_->GetSettings()->GetShouldReuseGlobalForUnownedMainFrame()) {
    frame_->GetDocument()->GetMutableSecurityOrigin()->GrantUniversalAccess();
  }

  if (!owner) {
    // This trace event is needed to detect the main frame of the
    // renderer in telemetry metrics. See crbug.com/692112#c11.
    TRACE_EVENT_INSTANT1("loading", "markAsMainFrame", TRACE_EVENT_SCOPE_THREAD,
                         "frame", ToTraceValue(frame_));
  }
}

LocalFrame* WebLocalFrameImpl::CreateChildFrame(
    const AtomicString& name,
    HTMLFrameOwnerElement* owner_element) {
  DCHECK(client_);
  TRACE_EVENT0("blink", "WebLocalFrameImpl::createChildframe");
  WebTreeScopeType scope =
      GetFrame()->GetDocument() == owner_element->GetTreeScope()
          ? WebTreeScopeType::kDocument
          : WebTreeScopeType::kShadow;
  WebFrameOwnerProperties owner_properties(
      owner_element->BrowsingContextContainerName(),
      owner_element->ScrollingMode(), owner_element->MarginWidth(),
      owner_element->MarginHeight(), owner_element->AllowFullscreen(),
      owner_element->AllowPaymentRequest(), owner_element->IsDisplayNone(),
      owner_element->RequiredCsp());
  // FIXME: Using subResourceAttributeName as fallback is not a perfect
  // solution. subResourceAttributeName returns just one attribute name. The
  // element might not have the attribute, and there might be other attributes
  // which can identify the element.
  WebLocalFrameImpl* webframe_child = To<WebLocalFrameImpl>(
      client_->CreateChildFrame(this, scope, name,
                                owner_element->getAttribute(
                                    owner_element->SubResourceAttributeName()),
                                owner_element->GetFramePolicy(),
                                owner_properties, owner_element->OwnerType()));
  if (!webframe_child)
    return nullptr;

  webframe_child->InitializeCoreFrame(*GetFrame()->GetPage(), owner_element,
                                      name);

  DCHECK(webframe_child->Parent());
  return webframe_child->GetFrame();
}

std::pair<RemoteFrame*, base::UnguessableToken> WebLocalFrameImpl::CreatePortal(
    HTMLPortalElement* portal,
    mojom::blink::PortalAssociatedRequest request,
    mojom::blink::PortalClientAssociatedPtrInfo client) {
  auto pair = client_->CreatePortal(request.PassHandle(), client.PassHandle());
  WebRemoteFrameImpl* portal_frame = ToWebRemoteFrameImpl(pair.first);
  portal_frame->InitializeCoreFrame(*GetFrame()->GetPage(), portal,
                                    g_null_atom);
  return std::pair<RemoteFrame*, base::UnguessableToken>(
      portal_frame->GetFrame(), pair.second);
}

RemoteFrame* WebLocalFrameImpl::AdoptPortal(HTMLPortalElement* portal) {
  WebRemoteFrameImpl* portal_frame =
      ToWebRemoteFrameImpl(client_->AdoptPortal(portal->GetToken()));
  portal_frame->InitializeCoreFrame(*GetFrame()->GetPage(), portal,
                                    g_null_atom);
  return portal_frame->GetFrame();
}

void WebLocalFrameImpl::DidChangeContentsSize(const IntSize& size) {
  if (GetTextFinder() && GetTextFinder()->TotalMatchCount() > 0)
    GetTextFinder()->IncreaseMarkerVersion();
}

bool WebLocalFrameImpl::HasDevToolsOverlays() const {
  return dev_tools_agent_ && dev_tools_agent_->HasOverlays();
}

void WebLocalFrameImpl::UpdateDevToolsOverlaysPrePaint() {
  if (dev_tools_agent_)
    dev_tools_agent_->UpdateOverlaysPrePaint();
}

void WebLocalFrameImpl::PaintDevToolsOverlays(GraphicsContext& context) {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  if (dev_tools_agent_)
    dev_tools_agent_->PaintOverlays(context);
}

void WebLocalFrameImpl::CreateFrameView() {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::createFrameView");

  DCHECK(GetFrame());  // If frame() doesn't exist, we probably didn't init
                       // properly.

  WebViewImpl* web_view = ViewImpl();

  // Check if we're shutting down.
  if (!web_view->GetPage())
    return;

  bool is_main_frame = !Parent();
  // TODO(dcheng): Can this be better abstracted away? It's pretty ugly that
  // only local roots are special-cased here.
  IntSize initial_size = (is_main_frame || !frame_widget_)
                             ? web_view->MainFrameSize()
                             : static_cast<IntSize>(frame_widget_->Size());
  Color base_background_color = web_view->BaseBackgroundColor();
  if (!is_main_frame && Parent()->IsWebRemoteFrame())
    base_background_color = Color::kTransparent;

  GetFrame()->CreateView(initial_size, base_background_color);
  if (is_main_frame) {
    GetFrame()->View()->SetInitialViewportSize(
        web_view->GetPageScaleConstraintsSet().InitialViewportSize());
  }
  if (web_view->ShouldAutoResize() && GetFrame()->IsLocalRoot()) {
    GetFrame()->View()->EnableAutoSizeMode(web_view->MinAutoSize(),
                                           web_view->MaxAutoSize());
  }

  GetFrame()->View()->SetDisplayMode(web_view->DisplayMode());

  if (frame_widget_)
    frame_widget_->DidCreateLocalRootView();
}

WebLocalFrameImpl* WebLocalFrameImpl::FromFrame(LocalFrame* frame) {
  if (!frame)
    return nullptr;
  return FromFrame(*frame);
}

WebLocalFrameImpl* WebLocalFrameImpl::FromFrame(LocalFrame& frame) {
  LocalFrameClient* client = frame.Client();
  if (!client || !client->IsLocalFrameClientImpl())
    return nullptr;
  return To<WebLocalFrameImpl>(client->GetWebFrame());
}

WebLocalFrameImpl* WebLocalFrameImpl::FromFrameOwnerElement(Element* element) {
  auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(element);
  if (!frame_owner_element)
    return nullptr;
  return FromFrame(To<LocalFrame>(frame_owner_element->ContentFrame()));
}

WebViewImpl* WebLocalFrameImpl::ViewImpl() const {
  if (!GetFrame())
    return nullptr;
  return GetFrame()->GetPage()->GetChromeClient().GetWebView();
}

void WebLocalFrameImpl::DidFailLoad(const ResourceError& error,
                                    WebHistoryCommitType web_commit_type) {
  if (!Client())
    return;
  WebURLError web_error = error;
  if (WebPluginContainerImpl* plugin = GetFrame()->GetWebPluginContainer())
    plugin->DidFailLoading(error);
  Client()->DidFailLoad(web_error, web_commit_type);
}

void WebLocalFrameImpl::DidFailProvisionalLoad(
    const ResourceError& error,
    const AtomicString& http_method) {
  if (!Client())
    return;
  WebURLError web_error = error;
  if (WebPluginContainerImpl* plugin = GetFrame()->GetWebPluginContainer())
    plugin->DidFailLoading(error);
  Client()->DidFailProvisionalLoad(web_error, http_method);
}

void WebLocalFrameImpl::DidFinish() {
  if (!Client())
    return;

  if (WebPluginContainerImpl* plugin = GetFrame()->GetWebPluginContainer())
    plugin->DidFinishLoading();

  Client()->DidFinishLoad();
}

HitTestResult WebLocalFrameImpl::HitTestResultForVisualViewportPos(
    const IntPoint& pos_in_viewport) {
  IntPoint root_frame_point(
      GetFrame()->GetPage()->GetVisualViewport().ViewportToRootFrame(
          pos_in_viewport));
  HitTestLocation location(
      GetFrame()->View()->ConvertFromRootFrame(root_frame_point));
  HitTestResult result = GetFrame()->GetEventHandler().HitTestResultAtLocation(
      location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  result.SetToShadowHostIfInRestrictedShadowRoot();
  return result;
}

void WebLocalFrameImpl::SetAutofillClient(WebAutofillClient* autofill_client) {
  autofill_client_ = autofill_client;
}

WebAutofillClient* WebLocalFrameImpl::AutofillClient() {
  return autofill_client_;
}

void WebLocalFrameImpl::SetContentCaptureClient(
    WebContentCaptureClient* content_capture_client) {
  content_capture_client_ = content_capture_client;
}

WebContentCaptureClient* WebLocalFrameImpl::ContentCaptureClient() const {
  return content_capture_client_;
}

bool WebLocalFrameImpl::IsLocalRoot() const {
  return frame_->IsLocalRoot();
}

bool WebLocalFrameImpl::IsProvisional() const {
  return frame_->IsProvisional();
}

WebLocalFrameImpl* WebLocalFrameImpl::LocalRoot() {
  // This can't use the LocalFrame::localFrameRoot, since it may be called
  // when the WebLocalFrame exists but the core LocalFrame does not.
  // TODO(alexmos, dcheng): Clean this up to only calculate this in one place.
  WebLocalFrameImpl* local_root = this;
  while (auto* web_local_frame =
             DynamicTo<WebLocalFrameImpl>(local_root->Parent()))
    local_root = web_local_frame;
  return local_root;
}

WebFrame* WebLocalFrameImpl::FindFrameByName(const WebString& name) {
  return WebFrame::FromFrame(GetFrame()->Tree().FindFrameByName(name));
}

bool WebLocalFrameImpl::ScrollTo(const gfx::Point& scrollPosition,
                                 bool animate,
                                 base::OnceClosure on_finish) {
  if (!GetFrame())
    return false;
  ScrollableArea* area = GetFrame()->View()->GetScrollableArea();
  ScrollOffset offset = area->ScrollPositionToOffset(
      FloatPoint(scrollPosition.x(), scrollPosition.y()));
  area->SetScrollOffset(
      offset, kProgrammaticScroll,
      animate ? kScrollBehaviorSmooth : kScrollBehaviorInstant,
      ScrollableArea::ScrollCallback(std::move(on_finish)));
  return true;
}

void WebLocalFrameImpl::SendPings(const WebURL& destination_url) {
  DCHECK(GetFrame());
  if (Node* node = ContextMenuNodeInner()) {
    Element* anchor = node->EnclosingLinkEventParentOrSelf();
    if (auto* html_anchor = ToHTMLAnchorElementOrNull(anchor))
      html_anchor->SendPings(destination_url);
  }
}

bool WebLocalFrameImpl::DispatchBeforeUnloadEvent(bool is_reload) {
  if (!GetFrame())
    return true;

  return GetFrame()->Loader().ShouldClose(is_reload);
}

void WebLocalFrameImpl::CommitNavigation(
    std::unique_ptr<WebNavigationParams> navigation_params,
    std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) {
  DCHECK(GetFrame());
  DCHECK(!navigation_params->url.ProtocolIs("javascript"));
  if (GetTextFinder())
    GetTextFinder()->ClearActiveFindMatch();
  GetFrame()->Loader().CommitNavigation(
      std::move(navigation_params), std::move(extra_data));
}

blink::mojom::CommitResult WebLocalFrameImpl::CommitSameDocumentNavigation(
    const WebURL& url,
    WebFrameLoadType web_frame_load_type,
    const WebHistoryItem& item,
    bool is_client_redirect,
    std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) {
  DCHECK(GetFrame());
  DCHECK(!url.ProtocolIs("javascript"));

  HistoryItem* history_item = item;
  return GetFrame()->Loader().GetDocumentLoader()->CommitSameDocumentNavigation(
      url, web_frame_load_type, history_item,
      is_client_redirect ? ClientRedirectPolicy::kClientRedirect
                         : ClientRedirectPolicy::kNotClientRedirect,
      nullptr, /* origin_document */
      false,   /* has_event */
      std::move(extra_data));
}

void WebLocalFrameImpl::LoadJavaScriptURL(const WebURL& url) {
  DCHECK(GetFrame());
  // Protect privileged pages against bookmarklets and other javascript
  // manipulations.
  if (SchemeRegistry::ShouldTreatURLSchemeAsNotAllowingJavascriptURLs(
          GetFrame()->GetDocument()->Url().Protocol()))
    return;

  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      LocalFrame::NotifyUserActivation(GetFrame(),
                                       UserGestureToken::kNewGesture);
  GetFrame()->GetScriptController().ExecuteJavaScriptURL(
      url, kDoNotCheckContentSecurityPolicy);
}

WebNavigationControl::FallbackContentResult
WebLocalFrameImpl::MaybeRenderFallbackContent(const WebURLError& error) const {
  DCHECK(GetFrame());

  if (!GetFrame()->Owner() || !GetFrame()->Owner()->CanRenderFallbackContent())
    return NoFallbackContent;

  return GetFrame()->Loader().MaybeRenderFallbackContent() ? FallbackRendered
                                                           : NoLoadInProgress;
}

void WebLocalFrameImpl::RenderFallbackContent() const {
  // TODO(ekaramad): If the owner renders its own content, then the current
  // ContentFrame() should detach (see https://crbug.com/850223).
  auto* owner = frame_->DeprecatedLocalOwner();
  DCHECK(IsHTMLObjectElement(owner));
  owner->RenderFallbackContent(frame_);
}

// Called when a navigation is blocked because a Content Security Policy (CSP)
// is infringed.
void WebLocalFrameImpl::ReportContentSecurityPolicyViolation(
    const blink::WebContentSecurityPolicyViolation& violation) {
  AddMessageToConsole(blink::WebConsoleMessage(
      mojom::ConsoleMessageLevel::kError, violation.console_message,
      violation.source_location.url, violation.source_location.line_number,
      violation.source_location.column_number));

  std::unique_ptr<SourceLocation> source_location =
      std::make_unique<SourceLocation>(
          violation.source_location.url, violation.source_location.line_number,
          violation.source_location.column_number, nullptr);

  DCHECK(GetFrame() && GetFrame()->GetDocument());
  Document* document = GetFrame()->GetDocument();
  Vector<String> report_endpoints;
  for (const WebString& end_point : violation.report_endpoints)
    report_endpoints.push_back(end_point);
  document->GetContentSecurityPolicy()->ReportViolation(
      violation.directive,
      ContentSecurityPolicy::GetDirectiveType(violation.effective_directive),
      violation.console_message, violation.blocked_url, report_endpoints,
      violation.use_reporting_api, violation.header,
      static_cast<ContentSecurityPolicyHeaderType>(violation.disposition),
      ContentSecurityPolicy::ViolationType::kURLViolation,
      std::move(source_location), nullptr /* LocalFrame */,
      violation.after_redirect ? RedirectStatus::kFollowedRedirect
                               : RedirectStatus::kNoRedirect,
      nullptr /* Element */);
}

bool WebLocalFrameImpl::IsLoading() const {
  if (!GetFrame() || !GetFrame()->GetDocument())
    return false;
  return GetFrame()
             ->Loader()
             .StateMachine()
             ->IsDisplayingInitialEmptyDocument() ||
         GetFrame()->Loader().HasProvisionalNavigation() ||
         !GetFrame()->GetDocument()->LoadEventFinished();
}

bool WebLocalFrameImpl::IsNavigationScheduledWithin(
    double interval_in_seconds) const {
  if (!GetFrame())
    return false;
  return GetFrame()->Loader().HasProvisionalNavigation() ||
         GetFrame()->GetDocument()->IsHttpRefreshScheduledWithin(
             interval_in_seconds);
}

void WebLocalFrameImpl::SetCommittedFirstRealLoad() {
  DCHECK(GetFrame());
  GetFrame()->Loader().StateMachine()->AdvanceTo(
      FrameLoaderStateMachine::kCommittedMultipleRealLoads);
  GetFrame()->SetShouldSendResourceTimingInfoToParent(false);
}

bool WebLocalFrameImpl::HasCommittedFirstRealLoad() {
  DCHECK(GetFrame());
  return GetFrame()->Loader().StateMachine()->CommittedFirstRealDocumentLoad();
}

void WebLocalFrameImpl::NotifyUserActivation() {
  LocalFrame::NotifyUserActivation(GetFrame(), UserGestureToken::kNewGesture);
}

void WebLocalFrameImpl::BlinkFeatureUsageReport(const std::set<int>& features) {
  DCHECK(!features.empty());
  // Assimilate all features used/performed by the browser into UseCounter.
  for (int feature : features) {
    UseCounter::Count(GetFrame()->GetDocument(),
                      static_cast<WebFeature>(feature));
  }
}

void WebLocalFrameImpl::MixedContentFound(
    const WebURL& main_resource_url,
    const WebURL& mixed_content_url,
    mojom::RequestContextType request_context,
    bool was_allowed,
    bool had_redirect,
    const WebSourceLocation& source_location) {
  DCHECK(GetFrame());
  std::unique_ptr<SourceLocation> source;
  if (!source_location.url.IsNull()) {
    source = std::make_unique<SourceLocation>(
        source_location.url, source_location.line_number,
        source_location.column_number, nullptr);
  }
  MixedContentChecker::MixedContentFound(
      GetFrame(), main_resource_url, mixed_content_url, request_context,
      was_allowed, had_redirect, std::move(source));
}

void WebLocalFrameImpl::DidDropNavigation() {
  GetFrame()->Loader().DidDropNavigation();
}

void WebLocalFrameImpl::MarkAsLoading() {
  GetFrame()->Loader().MarkAsLoading();
}

bool WebLocalFrameImpl::IsClientNavigationInitialHistoryLoad() {
  return GetFrame()->Loader().IsClientNavigationInitialHistoryLoad();
}

bool WebLocalFrameImpl::WillStartNavigation(
    const WebNavigationInfo& info,
    bool is_history_navigation_in_new_child_frame) {
  DCHECK(!info.url_request.IsNull());
  DCHECK(!info.url_request.Url().ProtocolIs("javascript"));
  return GetFrame()->Loader().WillStartNavigation(
      info, is_history_navigation_in_new_child_frame);
}

void WebLocalFrameImpl::SendOrientationChangeEvent() {
  if (!GetFrame())
    return;

  // Screen Orientation API
  if (ScreenOrientationController::From(*GetFrame()))
    ScreenOrientationController::From(*GetFrame())->NotifyOrientationChanged();

  // Legacy window.orientation API
  if (RuntimeEnabledFeatures::OrientationEventEnabled() &&
      GetFrame()->DomWindow())
    GetFrame()->DomWindow()->SendOrientationChangeEvent();
}

void WebLocalFrameImpl::DidCallAddSearchProvider() {
  UseCounter::Count(GetFrame()->GetDocument(),
                    WebFeature::kExternalAddSearchProvider);
}

void WebLocalFrameImpl::DidCallIsSearchProviderInstalled() {
  UseCounter::Count(GetFrame()->GetDocument(),
                    WebFeature::kExternalIsSearchProviderInstalled);
}

void WebLocalFrameImpl::DispatchMessageEventWithOriginCheck(
    const WebSecurityOrigin& intended_target_origin,
    const WebDOMEvent& event,
    bool has_user_gesture) {
  DCHECK(!event.IsNull());

  // If this postMessage was sent from another renderer process while having a
  // user gesture, synthesize the user gesture in this process, and restrict it
  // from being forwarded cross-process again.  This stops unbounded user
  // gesture usage by chaining postMessages across multiple processes.
  std::unique_ptr<UserGestureIndicator> gesture_indicator;
  if (!RuntimeEnabledFeatures::UserActivationV2Enabled() && has_user_gesture) {
    gesture_indicator = LocalFrame::NotifyUserActivation(GetFrame());
    UserGestureIndicator::SetWasForwardedCrossProcess();
  }

  // Transfer user activation state in the target's renderer when
  // |transferUserActivation| is true.
  MessageEvent* msg_event = static_cast<MessageEvent*>((Event*)event);
  Frame* source_frame = nullptr;
  if (msg_event->source() && msg_event->source()->ToDOMWindow())
    source_frame = msg_event->source()->ToDOMWindow()->GetFrame();
  if (RuntimeEnabledFeatures::UserActivationPostMessageTransferEnabled() &&
      msg_event->transferUserActivation()) {
    GetFrame()->TransferUserActivationFrom(source_frame);
  }

  GetFrame()->DomWindow()->DispatchMessageEventWithOriginCheck(
      intended_target_origin.Get(), event,
      std::make_unique<SourceLocation>(String(), 0, 0, nullptr));
}

WebNode WebLocalFrameImpl::ContextMenuNode() const {
  return ContextMenuNodeInner();
}

void WebLocalFrameImpl::WillBeDetached() {
  if (dev_tools_agent_)
    dev_tools_agent_->WillBeDestroyed();
  if (find_in_page_)
    find_in_page_->Dispose();
}

void WebLocalFrameImpl::WillDetachParent() {
  // Do not expect string scoping results from any frames that got detached
  // in the middle of the operation.
  if (GetTextFinder() && GetTextFinder()->ScopingInProgress()) {
    // There is a possibility that the frame being detached was the only
    // pending one. We need to make sure final replies can be sent.
    GetTextFinder()->FlushCurrentScoping();

    GetTextFinder()->CancelPendingScopingEffort();
  }
}

void WebLocalFrameImpl::SetFrameWidget(WebFrameWidgetBase* frame_widget) {
  frame_widget_ = frame_widget;
}

WebFrameWidget* WebLocalFrameImpl::FrameWidget() const {
  return frame_widget_;
}

void WebLocalFrameImpl::CopyImageAt(const WebPoint& pos_in_viewport) {
  HitTestResult result = HitTestResultForVisualViewportPos(pos_in_viewport);
  if (!IsHTMLCanvasElement(result.InnerNodeOrImageMapImage()) &&
      result.AbsoluteImageURL().IsEmpty()) {
    // There isn't actually an image at these coordinates.  Might be because
    // the window scrolled while the context menu was open or because the page
    // changed itself between when we thought there was an image here and when
    // we actually tried to retreive the image.
    //
    // FIXME: implement a cache of the most recent HitTestResult to avoid having
    //        to do two hit tests.
    return;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout();

  GetFrame()->GetEditor().CopyImage(result);
}

void WebLocalFrameImpl::SaveImageAt(const WebPoint& pos_in_viewport) {
  Node* node = HitTestResultForVisualViewportPos(pos_in_viewport)
                   .InnerNodeOrImageMapImage();
  if (!node || !(IsHTMLCanvasElement(*node) || IsHTMLImageElement(*node)))
    return;

  String url = ToElement(*node).ImageSourceURL();
  if (!KURL(NullURL(), url).ProtocolIsData())
    return;

  client_->SaveImageFromDataURL(url);
}

WebSandboxFlags WebLocalFrameImpl::EffectiveSandboxFlagsForTesting() const {
  if (!GetFrame())
    return WebSandboxFlags::kNone;
  SandboxFlags flags = GetFrame()->Loader().EffectiveSandboxFlags();
  if (RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled()) {
    // When some of sandbox flags set in the 'sandbox' attribute are implemented
    // as policies they are removed form the FrameOwner's sandbox flags to avoid
    // being considered again as part of inherited or CSP sandbox.
    // Note: if the FrameOwner is remote then the effective flags would miss the
    // part of sandbox converted to FeaturePolicies. That said, with
    // FeaturePolicyForSandbox all such flags should be part of the document's
    // FeaturePolicy. For certain flags such as "downloads", dedicated API
    // should be used (see IsAllowedToDownloadWithoutUserActivation()).
    auto* local_owner = GetFrame()->DeprecatedLocalOwner();
    if (local_owner &&
        local_owner->OwnerType() == FrameOwnerElementType::kIframe) {
      flags |= ToHTMLIFrameElement(local_owner)
                   ->sandbox_flags_converted_to_feature_policies();
    }
  }
  return static_cast<WebSandboxFlags>(flags);
}

bool WebLocalFrameImpl::IsAllowedToDownloadWithoutUserActivation() const {
  if (!GetFrame())
    return true;

  if (RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled()) {
    // Downloads could be disabled if the parent frame's FeaturePolicy does not
    // allow "downloads-without-user-activation".
    if (GetFrame()->Tree().Parent() &&
        !GetFrame()->Tree().Parent()->GetSecurityContext()->IsFeatureEnabled(
            mojom::FeaturePolicyFeature::kDownloadsWithoutUserActivation)) {
      return false;
    }
    return !GetFrame()->Owner() ||
           GetFrame()
               ->Owner()
               ->GetFramePolicy()
               .allowed_to_download_without_user_activation;
  } else {
    return (GetFrame()->Loader().EffectiveSandboxFlags() &
            WebSandboxFlags::kDownloads) == WebSandboxFlags::kNone;
  }
}

void WebLocalFrameImpl::UsageCountChromeLoadTimes(const WebString& metric) {
  WebFeature feature = WebFeature::kChromeLoadTimesUnknown;
  if (metric == "requestTime") {
    feature = WebFeature::kChromeLoadTimesRequestTime;
  } else if (metric == "startLoadTime") {
    feature = WebFeature::kChromeLoadTimesStartLoadTime;
  } else if (metric == "commitLoadTime") {
    feature = WebFeature::kChromeLoadTimesCommitLoadTime;
  } else if (metric == "finishDocumentLoadTime") {
    feature = WebFeature::kChromeLoadTimesFinishDocumentLoadTime;
  } else if (metric == "finishLoadTime") {
    feature = WebFeature::kChromeLoadTimesFinishLoadTime;
  } else if (metric == "firstPaintTime") {
    feature = WebFeature::kChromeLoadTimesFirstPaintTime;
  } else if (metric == "firstPaintAfterLoadTime") {
    feature = WebFeature::kChromeLoadTimesFirstPaintAfterLoadTime;
  } else if (metric == "navigationType") {
    feature = WebFeature::kChromeLoadTimesNavigationType;
  } else if (metric == "wasFetchedViaSpdy") {
    feature = WebFeature::kChromeLoadTimesWasFetchedViaSpdy;
  } else if (metric == "wasNpnNegotiated") {
    feature = WebFeature::kChromeLoadTimesWasNpnNegotiated;
  } else if (metric == "npnNegotiatedProtocol") {
    feature = WebFeature::kChromeLoadTimesNpnNegotiatedProtocol;
  } else if (metric == "wasAlternateProtocolAvailable") {
    feature = WebFeature::kChromeLoadTimesWasAlternateProtocolAvailable;
  } else if (metric == "connectionInfo") {
    feature = WebFeature::kChromeLoadTimesConnectionInfo;
  }
  Deprecation::CountDeprecation(GetFrame()->GetDocument(), feature);
}

FrameScheduler* WebLocalFrameImpl::Scheduler() const {
  return GetFrame()->GetFrameScheduler();
}

scoped_refptr<base::SingleThreadTaskRunner> WebLocalFrameImpl::GetTaskRunner(
    TaskType task_type) {
  return GetFrame()->GetTaskRunner(task_type);
}

WebInputMethodController* WebLocalFrameImpl::GetInputMethodController() {
  return &input_method_controller_;
}

// TODO(editing-dev): We should move |CreateMarkupInRect()| to
// "core/editing/serializers/Serialization.cpp".
static String CreateMarkupInRect(LocalFrame*, const IntPoint&, const IntPoint&);

void WebLocalFrameImpl::ExtractSmartClipData(WebRect rect_in_viewport,
                                             WebString& clip_text,
                                             WebString& clip_html,
                                             WebRect& clip_rect) {
  // TODO(mahesh.ma): Check clip_data even after use-zoom-for-dsf is enabled.
  SmartClipData clip_data = SmartClip(GetFrame()).DataForRect(rect_in_viewport);
  clip_text = clip_data.ClipData();
  clip_rect = clip_data.RectInViewport();

  WebPoint start_point(rect_in_viewport.x, rect_in_viewport.y);
  WebPoint end_point(rect_in_viewport.x + rect_in_viewport.width,
                     rect_in_viewport.y + rect_in_viewport.height);
  clip_html = CreateMarkupInRect(
      GetFrame(), GetFrame()->View()->ViewportToFrame(start_point),
      GetFrame()->View()->ViewportToFrame(end_point));
}

// TODO(editing-dev): We should move |CreateMarkupInRect()| to
// "core/editing/serializers/Serialization.cpp".
static String CreateMarkupInRect(LocalFrame* frame,
                                 const IntPoint& start_point,
                                 const IntPoint& end_point) {
  VisiblePosition start_visible_position = CreateVisiblePosition(
      PositionForContentsPointRespectingEditingBoundary(start_point, frame));
  VisiblePosition end_visible_position = CreateVisiblePosition(
      PositionForContentsPointRespectingEditingBoundary(end_point, frame));

  Position start_position = start_visible_position.DeepEquivalent();
  Position end_position = end_visible_position.DeepEquivalent();

  // document() will return null if -webkit-user-select is set to none.
  if (!start_position.GetDocument() || !end_position.GetDocument())
    return String();

  if (start_position.CompareTo(end_position) <= 0) {
    return CreateMarkup(start_position, end_position, kAnnotateForInterchange,
                        ConvertBlocksToInlines::kNotConvert,
                        kResolveNonLocalURLs);
  }
  return CreateMarkup(end_position, start_position, kAnnotateForInterchange,
                      ConvertBlocksToInlines::kNotConvert,
                      kResolveNonLocalURLs);
}

void WebLocalFrameImpl::AdvanceFocusInForm(WebFocusType focus_type) {
  DCHECK(GetFrame()->GetDocument());
  Element* element = GetFrame()->GetDocument()->FocusedElement();
  if (!element)
    return;

  Element* next_element =
      GetFrame()->GetPage()->GetFocusController().NextFocusableElementInForm(
          element, focus_type);
  if (!next_element)
    return;

  next_element->scrollIntoViewIfNeeded(true /*centerIfNeeded*/);
  next_element->focus();
}

bool WebLocalFrameImpl::CanFocusedFieldBeAutofilled() const {
  DCHECK(GetFrame()->GetDocument());
  auto* focused_form_control_element = ToHTMLFormControlElementOrNull(
      GetFrame()->GetDocument()->FocusedElement());

  if (!focused_form_control_element)
    return false;

  return autofill_client_->HasFillData(focused_form_control_element);
}

void WebLocalFrameImpl::PerformMediaPlayerAction(
    const WebPoint& location,
    const WebMediaPlayerAction& action) {
  HitTestResult result = HitTestResultForVisualViewportPos(location);
  Node* node = result.InnerNode();
  if (!IsHTMLVideoElement(*node) && !IsHTMLAudioElement(*node))
    return;

  HTMLMediaElement* media_element = ToHTMLMediaElement(node);
  switch (action.type) {
    case WebMediaPlayerAction::Type::kPlay:
      if (action.enable)
        media_element->Play();
      else
        media_element->pause();
      break;
    case WebMediaPlayerAction::Type::kMute:
      media_element->setMuted(action.enable);
      break;
    case WebMediaPlayerAction::Type::kLoop:
      media_element->SetLoop(action.enable);
      break;
    case WebMediaPlayerAction::Type::kControls:
      media_element->SetBooleanAttribute(html_names::kControlsAttr,
                                         action.enable);
      break;
    case WebMediaPlayerAction::Type::kPictureInPicture:
      DCHECK(media_element->IsHTMLVideoElement());
      if (action.enable) {
        PictureInPictureController::From(node->GetDocument())
            .EnterPictureInPicture(ToHTMLVideoElement(media_element),
                                   nullptr /* promise */,
                                   nullptr /* options */);
      } else {
        PictureInPictureController::From(node->GetDocument())
            .ExitPictureInPicture(ToHTMLVideoElement(media_element), nullptr);
      }
      break;
  }
}

void WebLocalFrameImpl::OnPortalActivated(
    const base::UnguessableToken& portal_token,
    mojo::ScopedInterfaceEndpointHandle portal_pipe,
    mojo::ScopedInterfaceEndpointHandle portal_client_pipe,
    TransferableMessage data,
    OnPortalActivatedCallback callback) {
  LocalDOMWindow* window = GetFrame()->DomWindow();

  DOMWindowPortalHost::portalHost(*window)->OnPortalActivated();
  GetFrame()->GetPage()->SetInsidePortal(false);

  auto blink_data = ToBlinkTransferableMessage(std::move(data));
  DCHECK(!blink_data.locked_agent_cluster_id)
      << "portal activation is always cross-agent-cluster and should be "
         "diagnosed early";
  MessagePortArray* ports = MessagePort::EntanglePorts(
      *window->document(), std::move(blink_data.ports));

  PortalActivateEvent* event = PortalActivateEvent::Create(
      frame_.Get(), portal_token,
      mojom::blink::PortalAssociatedPtr(mojom::blink::PortalAssociatedPtrInfo(
          std::move(portal_pipe), mojom::blink::Portal::Version_)),
      mojom::blink::PortalClientAssociatedRequest(
          std::move(portal_client_pipe)),
      std::move(blink_data.message), ports, std::move(callback));

  ThreadDebugger* debugger = MainThreadDebugger::Instance();
  if (debugger)
    debugger->ExternalAsyncTaskStarted(blink_data.sender_stack_trace_id);
  GetFrame()->DomWindow()->DispatchEvent(*event);
  if (debugger)
    debugger->ExternalAsyncTaskFinished(blink_data.sender_stack_trace_id);
  event->DetachPortalIfNotAdopted();
}

void WebLocalFrameImpl::ForwardMessageFromHost(
    TransferableMessage message,
    const WebSecurityOrigin& source_origin,
    const base::Optional<WebSecurityOrigin>& target_origin) {
  scoped_refptr<const SecurityOrigin> target;
  if (target_origin)
    target = target_origin.value();

  PortalHost::From(*(GetFrame()->DomWindow()))
      .ReceiveMessage(ToBlinkTransferableMessage(std::move(message)),
                      source_origin, target);
}

void WebLocalFrameImpl::AddMessageToConsoleImpl(
    const WebConsoleMessage& message,
    bool discard_duplicates) {
  DCHECK(GetFrame());
  GetFrame()->GetDocument()->AddConsoleMessage(
      ConsoleMessage::CreateFromWebConsoleMessage(message, GetFrame()),
      discard_duplicates);
}

void WebLocalFrameImpl::SetTextCheckClient(
    WebTextCheckClient* text_check_client) {
  text_check_client_ = text_check_client;
}

void WebLocalFrameImpl::SetSpellCheckPanelHostClient(
    WebSpellCheckPanelHostClient* spell_check_panel_host_client) {
  spell_check_panel_host_client_ = spell_check_panel_host_client;
}

WebFrameWidgetBase* WebLocalFrameImpl::LocalRootFrameWidget() {
  CHECK(LocalRoot());
  return LocalRoot()->FrameWidgetImpl();
}

Node* WebLocalFrameImpl::ContextMenuNodeInner() const {
  if (!ViewImpl() || !ViewImpl()->GetPage())
    return nullptr;
  return ViewImpl()
      ->GetPage()
      ->GetContextMenuController()
      .ContextMenuNodeForFrame(GetFrame());
}

void WebLocalFrameImpl::SetDevToolsAgentImpl(WebDevToolsAgentImpl* agent) {
  DCHECK(!dev_tools_agent_);
  dev_tools_agent_ = agent;
}

WebDevToolsAgentImpl* WebLocalFrameImpl::DevToolsAgentImpl() {
  if (!frame_->IsLocalRoot())
    return nullptr;
  if (!dev_tools_agent_)
    dev_tools_agent_ = WebDevToolsAgentImpl::CreateForFrame(this);
  return dev_tools_agent_;
}

void WebLocalFrameImpl::BindDevToolsAgent(
    mojo::ScopedInterfaceEndpointHandle devtools_agent_host_ptr_info,
    mojo::ScopedInterfaceEndpointHandle devtools_agent_request) {
  WebDevToolsAgentImpl* agent = DevToolsAgentImpl();
  if (!agent)
    return;
  agent->BindRequest(mojom::blink::DevToolsAgentHostAssociatedPtrInfo(
                         std::move(devtools_agent_host_ptr_info),
                         mojom::blink::DevToolsAgentHost::Version_),
                     mojom::blink::DevToolsAgentAssociatedRequest(
                         std::move(devtools_agent_request)));
}

void WebLocalFrameImpl::SetLifecycleState(mojom::FrameLifecycleState state) {
  DCHECK(GetFrame());
  GetFrame()->SetLifecycleState(state);
}

void WebLocalFrameImpl::WasHidden() {
  if (frame_)
    frame_->WasHidden();
}

void WebLocalFrameImpl::WasShown() {
  if (frame_)
    frame_->WasShown();
}

}  // namespace blink
