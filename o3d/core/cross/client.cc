/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
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


// This file contains the implementation for the Client class.

#include "core/cross/client.h"
#include "core/cross/draw_context.h"
#include "core/cross/effect.h"
#include "core/cross/message_queue.h"
#include "core/cross/pack.h"
#include "core/cross/shape.h"
#include "core/cross/transform.h"
#include "core/cross/material.h"
#include "core/cross/renderer.h"
#include "core/cross/viewport.h"
#include "core/cross/clear_buffer.h"
#include "core/cross/state_set.h"
#include "core/cross/tree_traversal.h"
#include "core/cross/draw_pass.h"
#include "core/cross/bounding_box.h"
#include "core/cross/bitmap.h"
#include "core/cross/error.h"
#include "core/cross/evaluation_counter.h"
#include "core/cross/id_manager.h"
#include "core/cross/profiler.h"
#include "utils/cross/string_writer.h"
#include "utils/cross/json_writer.h"
#include "utils/cross/dataurl.h"

#ifdef OS_WIN
#include "core/cross/core_metrics.h"
#endif

using std::map;
using std::vector;
using std::make_pair;

namespace o3d {
// If Renderer::max_fps has been set, rendering is driven by incoming new
// textures. We draw on each new texture as long as not exceeding max_fps.
//
// If we are in RENDERMODE_ON_DEMAND mode, Client::Render() can still set dirty
// specifically.
//
// If we are in RENDERMODE_CONTINUOUS mode, we do NOT set dirty on each tick any
// more (since it is already driven by new textures.).
// There is one problem here: what if new texture don't come in for some reason?
// If that happened, no rendering callback will be invoked and this can cause
// problem sometimes. For example, some UI may depend on the rendering callback
// to work correctly.
// So, in RENDERMODE_CONTINUOUS mode, if we have set max_fps but haven't
// received any new texture for a while, we draw anyway to trigger the rendering
// callback.
// This value defines the minimum number of draws per seconds in
// RENDERMODE_CONTINUOUS mode.
static const float kContinuousModeMinDrawPerSecond = 15;
// TODO(zhurunz) Tuning this value.

// We don't want to handle too much MOUSEMOVE events for perf reasons.
// This is used for limiting one MOUSEMOVE event per N ticks.
//
// Notes:
// Don't limit MOUSEMOVE event on new texture since that don't always come.
// Don't limit MOUSEMOVE event on rendering since RENDERMODE_ON_DEMAND might
// depends on MOUSEMOVE first.
static const int kTicksPerMouseMoveEvent = 4;

// Client constructor.  Creates the default root node for the scenegraph
Client::Client(ServiceLocator* service_locator)
    : service_locator_(service_locator),
      object_manager_(service_locator),
      error_status_(service_locator),
      draw_list_manager_(service_locator),
      counter_manager_(service_locator),
      transformation_context_(service_locator),
      semantic_manager_(service_locator),
      profiler_(service_locator),
      renderer_(service_locator),
      evaluation_counter_(service_locator),
      render_tree_called_(false),
      render_mode_(RENDERMODE_CONTINUOUS),
      texture_on_hold_(false),
      event_manager_(),
      is_ticking_(false),
      last_tick_time_(0),
      tick_count_(0),
      root_(NULL),
#ifdef OS_WIN
      calls_(0),
#endif
      rendergraph_root_(NULL),
      id_(IdManager::CreateId()) {
  // Create and initialize the message queue to allow external code to
  // communicate with the Client via RPC calls.
  message_queue_.reset(new MessageQueue(service_locator_));

  if (!message_queue_->Initialize()) {
    LOG(ERROR) << "Client failed to initialize the message queue";
    message_queue_.reset(NULL);
  }
}

// Frees up all the resources allocated by the Client factory methods but
// does not destroy the "renderer_" object.
Client::~Client() {
  root_.Reset();
  rendergraph_root_.Reset();

  object_manager_->DestroyAllPacks();

  // Unmap the client from the renderer on exit.
  if (renderer_.IsAvailable()) {
    renderer_->UninitCommon();
  }
}

// Assigns a Renderer to the Client, and also assigns the Client
// to the Renderer and sets up the default render graph
void Client::Init() {
  if (!renderer_.IsAvailable())
    return;

  // Create the root node for the scenegraph.  Note that the root lives
  // outside of a pack object.  The root's lifetime is directly bound to that
  // of the client.
  root_ = Transform::Ref(new Transform(service_locator_));
  root_->set_name(O3D_STRING_CONSTANT("root"));

  // Creates the root for the render graph.
  rendergraph_root_ = RenderNode::Ref(new RenderNode(service_locator_));
  rendergraph_root_->set_name(O3D_STRING_CONSTANT("root"));

  // Let the renderer Init a few common things.
  renderer_->InitCommon();
}

void Client::Cleanup() {
  ClearRenderCallback();
  ClearPostRenderCallback();
  ClearTickCallback();
  event_manager_.ClearAll();
  counter_manager_.ClearAllCallbacks();

  // Disable continuous rendering because there is nothing to render after
  // Cleanup is called. This speeds up the the process of unloading the page.
  // It also preserves the last rendered frame so long as it does not become
  // invalid.
  render_mode_ = RENDERMODE_ON_DEMAND;

  // Destroy the packs here if possible. If there are a lot of objects it takes
  // a long time and seems to make Chrome timeout if it happens in NPP_Destroy.
  root_.Reset();
  rendergraph_root_.Reset();
  object_manager_->DestroyAllPacks();
}

Pack* Client::CreatePack() {
  if (!renderer_.IsAvailable()) {
    O3D_ERROR(service_locator_)
        << "No Renderer available, Pack creation not allowed.";
    return NULL;
  }

  return object_manager_->CreatePack();
}

// Tick Methods ----------------------------------------------------------------

void Client::SetTickCallback(
    TickCallback* tick_callback) {
  tick_callback_manager_.Set(tick_callback);
}

void Client::ClearTickCallback() {
  tick_callback_manager_.Clear();
}

bool Client::Tick() {
  is_ticking_ = true;
  ElapsedTimeTimer timer;
  float seconds_elapsed = tick_elapsed_time_timer_.GetElapsedTimeAndReset();
  tick_event_.set_elapsed_time(seconds_elapsed);
  profiler_->ProfileStart("Tick callback");
  tick_callback_manager_.Run(tick_event_);
  profiler_->ProfileStop("Tick callback");

  evaluation_counter_->InvalidateAllParameters();

  counter_manager_.AdvanceCounters(1.0f, seconds_elapsed);

  // Processes any incoming message found in the message queue.  Note that this
  // call does not block if no new messages are found.
  bool message_check_ok = true;
  bool has_new_texture = false;

  if (message_queue_.get()) {
    profiler_->ProfileStart("CheckForNewMessages");
    message_check_ok = message_queue_->CheckForNewMessages(&has_new_texture);
    profiler_->ProfileStop("CheckForNewMessages");
  }

  event_manager_.ProcessQueue();
  event_manager_.ProcessQueue();
  event_manager_.ProcessQueue();
  event_manager_.ProcessQueue();

  last_tick_time_ = timer.GetElapsedTimeAndReset();

  texture_on_hold_ |= has_new_texture;
  if (texture_on_hold_ && renderer_.IsAvailable()) {
    int max_fps = renderer_->max_fps();
    if (max_fps > 0 &&
        render_mode() == RENDERMODE_ON_DEMAND &&
        render_elapsed_time_timer_.GetElapsedTimeWithoutClearing()
        > 1.0/max_fps) {
      renderer_->set_need_to_render(true);
      texture_on_hold_ = false;
    }
  }

  is_ticking_ = false;
  tick_count_++;
  return message_check_ok;
}

// Render Methods --------------------------------------------------------------

void Client::SetLostResourcesCallback(LostResourcesCallback* callback) {
  if (!renderer_.IsAvailable()) {
    O3D_ERROR(service_locator_) << "No Renderer";
  } else {
    renderer_->SetLostResourcesCallback(callback);
  }
}

void Client::ClearLostResourcesCallback() {
  if (renderer_.IsAvailable()) {
    renderer_->ClearLostResourcesCallback();
  }
}

void Client::RenderClientInner(bool present, bool send_callback) {
  ElapsedTimeTimer timer;
  render_tree_called_ = false;
  total_time_to_render_ = 0.0f;

  if (!renderer_.IsAvailable())
    return;

  if (renderer_->StartRendering()) {
    counter_manager_.AdvanceRenderFrameCounters(1.0f);

    profiler_->ProfileStart("Render callback");
    if (send_callback)
      render_callback_manager_.Run(render_event_);

    // Calling back to JavaScript may have caused the plugin to be
    // torn down. Guard carefully against this.
    if (!profiler_.IsAvailable()) {
      if (renderer_.IsAvailable()) {
        renderer_->FinishRendering();
      }
      return;
    }

    profiler_->ProfileStop("Render callback");

    if (!render_tree_called_) {
      RenderNode* rendergraph_root = render_graph_root();
      // If nothing was rendered and there are no render graph nodes then
      // clear the client area.
      if (!rendergraph_root || rendergraph_root->children().empty()) {
          renderer_->Clear(Float4(0.4f, 0.3f, 0.3f, 1.0f),
                           true, 1.0, true, 0, true);
      } else if (rendergraph_root) {
        RenderTree(rendergraph_root);
      }
    }

    renderer_->FinishRendering();
    if (present) {
      renderer_->Present();
      // This has to be called before the POST render callback because
      // the post render callback may call Client::Render.
      renderer_->set_need_to_render(false);
    }

    // Call post render callback.
    profiler_->ProfileStart("Post-render callback");
    post_render_callback_manager_.Run(render_event_);
    profiler_->ProfileStop("Post-render callback");

    // Update Render stats.
    render_event_.set_elapsed_time(
        render_elapsed_time_timer_.GetElapsedTimeAndReset());
    render_event_.set_render_time(total_time_to_render_);
    render_event_.set_transforms_culled(renderer_->transforms_culled());
    render_event_.set_transforms_processed(renderer_->transforms_processed());
    render_event_.set_draw_elements_culled(renderer_->draw_elements_culled());
    render_event_.set_draw_elements_processed(
        renderer_->draw_elements_processed());
    render_event_.set_draw_elements_rendered(
        renderer_->draw_elements_rendered());

    render_event_.set_primitives_rendered(renderer_->primitives_rendered());

    render_event_.set_active_time(
        timer.GetElapsedTimeAndReset() + last_tick_time_);
    last_tick_time_ = 0.0f;

#ifdef OS_WIN
    // Update render metrics
    metric_render_elapsed_time.AddSample(  // Convert to ms.
        static_cast<int>(1000 * render_event_.elapsed_time()));
    metric_render_time_seconds += static_cast<uint64>(
        render_event_.render_time());
    metric_render_xforms_culled.AddSample(render_event_.transforms_culled());
    metric_render_xforms_processed.AddSample(
        render_event_.transforms_processed());
    metric_render_draw_elts_culled.AddSample(
        render_event_.draw_elements_culled());
    metric_render_draw_elts_processed.AddSample(
        render_event_.draw_elements_processed());
    metric_render_draw_elts_rendered.AddSample(
        render_event_.draw_elements_rendered());
    metric_render_prims_rendered.AddSample(render_event_.primitives_rendered());
#endif  // OS_WIN
  }
}

void Client::RenderClient(bool send_callback) {
  if (!renderer_.IsAvailable())
    return;

  bool have_offscreen_surfaces =
      !(offscreen_render_surface_.IsNull() ||
        offscreen_depth_render_surface_.IsNull());

  if (have_offscreen_surfaces) {
    if (!renderer_->StartRendering()) {
      return;
    }
    renderer_->SetRenderSurfaces(offscreen_render_surface_,
                                 offscreen_depth_render_surface_,
                                 true);
  }

  RenderClientInner(!have_offscreen_surfaces, send_callback);

  if (have_offscreen_surfaces) {
    renderer_->SetRenderSurfaces(NULL, NULL, false);
    renderer_->FinishRendering();
  }
}

bool Client::IsRendering() {
  return (renderer_.IsAvailable() && renderer_->rendering());
}

bool Client::NeedsContinuousRender() {
  bool needRender = false;
  // Only may happen in RENDERMODE_CONTINUOUS mode.
  if (render_mode() == RENDERMODE_CONTINUOUS) {
    // Always need a draw in normal RENDERMODE_CONTINUOUS mode.
    needRender = true;

    if (renderer_.IsAvailable()) {
      // If max_fps has been set, only need a draw when "long time no draw".
      int max_fps = renderer_->max_fps();
      if (max_fps > 0 &&
          render_elapsed_time_timer_.GetElapsedTimeWithoutClearing() <
          1.0/kContinuousModeMinDrawPerSecond)
      {
        needRender = false;
      }
    }
  }
  return needRender;
}

// Executes draw calls for all visible shapes in a subtree
void Client::RenderTree(RenderNode *tree_root) {
  if (!renderer_.IsAvailable())
    return;

  if (!renderer_->rendering()) {
    // Render tree can not be called if we are not rendering because all calls
    // to RenderTree must happen inside renderer->StartRendering() /
    // renderer->FinishRendering() calls.
    O3D_ERROR(service_locator_)
        << "RenderTree must not be called outside of rendering.";
    return;
  }

  render_tree_called_ = true;

  // Only render the shapes if BeginDraw() succeeds
  profiler_->ProfileStart("RenderTree");
  ElapsedTimeTimer time_to_render_timer;
  if (renderer_->BeginDraw()) {
    RenderContext render_context(renderer_.Get());

    if (tree_root) {
      tree_root->RenderTree(&render_context);
    }

    draw_list_manager_.Reset();

    // Finish up.
    renderer_->EndDraw();
  }
  total_time_to_render_ += time_to_render_timer.GetElapsedTimeAndReset();
  profiler_->ProfileStop("RenderTree");
}

void Client::SetRenderCallback(RenderCallback* render_callback) {
  render_callback_manager_.Set(render_callback);
}

void Client::ClearRenderCallback() {
  render_callback_manager_.Clear();
}

void Client::SetEventCallback(Event::Type type,
                              EventCallback* event_callback) {
  event_manager_.SetEventCallback(type, event_callback);
}

void Client::SetEventCallback(String type_name,
                              EventCallback* event_callback) {
  Event::Type type = Event::TypeFromString(type_name.c_str());
  if (!Event::ValidType(type)) {
    O3D_ERROR(service_locator_) << "Invalid event type: '" <<
        type_name << "'.";
  } else {
    event_manager_.SetEventCallback(type, event_callback);
  }
}

void Client::ClearEventCallback(Event::Type type) {
  event_manager_.ClearEventCallback(type);
}

void Client::ClearEventCallback(String type_name) {
  Event::Type type = Event::TypeFromString(type_name.c_str());
  if (!Event::ValidType(type)) {
    O3D_ERROR(service_locator_) << "Invalid event type: '" <<
        type_name << "'.";
  } else {
    event_manager_.ClearEventCallback(type);
  }
}

void Client::AddEventToQueue(const Event& event) {
  // Limit one MOUSEMOVE event per N ticks.
  if (event.type() == Event::TYPE_MOUSEMOVE) {
    if (tick_count_ <= kTicksPerMouseMoveEvent) {
      return;
    } else {
      tick_count_ = 0;
    }
  }
  event_manager_.AddEventToQueue(event);
}

void Client::SendResizeEvent(int width, int height, bool fullscreen) {
  Event event(Event::TYPE_RESIZE);
  event.set_size(width, height, fullscreen);
  AddEventToQueue(event);
}

void Client::set_render_mode(RenderMode render_mode) {
  render_mode_ = render_mode;
}

void Client::SetPostRenderCallback(RenderCallback* post_render_callback) {
  post_render_callback_manager_.Set(post_render_callback);
}

void Client::ClearPostRenderCallback() {
  post_render_callback_manager_.Clear();
}

void Client::Render() {
  if (render_mode() == RENDERMODE_ON_DEMAND) {
    if (renderer_.IsAvailable()) {
      renderer_->set_need_to_render(true);
    }
  }
}

void Client::SetErrorTexture(Texture* texture) {
  renderer_->SetErrorTexture(texture);
}

void Client::InvalidateAllParameters() {
  evaluation_counter_->InvalidateAllParameters();
}

String Client::GetScreenshotAsDataURL()  {
  // To take a screenshot we create a render target and render into it
  // then get a bitmap from that.
  int pot_width =
      static_cast<int>(image::ComputePOTSize(renderer_->display_width()));
  int pot_height =
      static_cast<int>(image::ComputePOTSize(renderer_->display_height()));
  if (pot_width == 0 || pot_height == 0) {
    return dataurl::kEmptyDataURL;
  }
  Texture2D::Ref texture = renderer_->CreateTexture2D(
      pot_width,
      pot_height,
      Texture::ARGB8,
      1,
      true);
  if (texture.IsNull()) {
    return dataurl::kEmptyDataURL;
  }
  RenderSurface::Ref surface(texture->GetRenderSurface(0));
  if (surface.IsNull()) {
    return dataurl::kEmptyDataURL;
  }
  RenderDepthStencilSurface::Ref depth(renderer_->CreateDepthStencilSurface(
      pot_width,
      pot_height));
  if (depth.IsNull()) {
    return dataurl::kEmptyDataURL;
  }
  surface->SetClipSize(renderer_->display_width(), renderer_->display_height());
  depth->SetClipSize(renderer_->display_width(), renderer_->display_height());

  const RenderSurface* old_render_surface_;
  const RenderDepthStencilSurface* old_depth_surface_;
  bool is_back_buffer;

  renderer_->GetRenderSurfaces(&old_render_surface_, &old_depth_surface_,
                               &is_back_buffer);
  renderer_->SetRenderSurfaces(surface, depth, true);

  RenderClientInner(false, true);

  renderer_->SetRenderSurfaces(old_render_surface_, old_depth_surface_,
                               is_back_buffer);

  Bitmap::Ref bitmap(surface->GetBitmap());
  if (bitmap.IsNull()) {
    return dataurl::kEmptyDataURL;
  } else {
    bitmap->FlipVertically();
    return bitmap->ToDataURL();
  }
}

String Client::ToDataURL() {
  if (!renderer_.IsAvailable()) {
    O3D_ERROR(service_locator_) << "No Render Device Available";
    return dataurl::kEmptyDataURL;
  }

  if (renderer_->rendering()) {
    O3D_ERROR(service_locator_)
       << "Can not take a screenshot while rendering";
    return dataurl::kEmptyDataURL;
  }

  if (!renderer_->StartRendering()) {
    return dataurl::kEmptyDataURL;
  }

  String data_url(GetScreenshotAsDataURL());
  renderer_->FinishRendering();

  return data_url;
}

String Client::GetMessageQueueAddress() const {
  if (message_queue_.get()) {
    return message_queue_->GetSocketAddress();
  } else {
    O3D_ERROR(service_locator_) << "Message queue not initialized";
    return String("");
  }
}

void Client::SetOffscreenRenderingSurfaces(
    RenderSurface::Ref surface,
    RenderDepthStencilSurface::Ref depth_surface) {
  offscreen_render_surface_ = surface;
  offscreen_depth_render_surface_ = depth_surface;
}

// Error Related methods -------------------------------------------------------

void Client::SetErrorCallback(ErrorCallback* callback) {
  error_status_.SetErrorCallback(callback);
}

void Client::ClearErrorCallback() {
  error_status_.ClearErrorCallback();
}

const String& Client::GetLastError() const {
  return error_status_.GetLastError();
}

void Client::ClearLastError() {
  error_status_.ClearLastError();
}

void Client::ProfileStart(const std::string& key) {
  profiler_->ProfileStart(key);
}

void Client::ProfileStop(const std::string& key) {
  profiler_->ProfileStop(key);
}

void Client::ProfileReset() {
  profiler_->ProfileReset();
}

String Client::ProfileToString() {
  StringWriter string_writer(StringWriter::LF);
  JsonWriter json_writer(&string_writer, 2);
  profiler_->Write(&json_writer);
  json_writer.Close();
  return string_writer.ToString();
}
}  // namespace o3d
