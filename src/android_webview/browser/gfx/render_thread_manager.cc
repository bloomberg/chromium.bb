// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/render_thread_manager.h"

#include <utility>

#include "android_webview/browser/gfx/compositor_frame_producer.h"
#include "android_webview/browser/gfx/compositor_id.h"
#include "android_webview/browser/gfx/deferred_gpu_command_service.h"
#include "android_webview/browser/gfx/scoped_app_gl_state_restore.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/viz/common/quads/compositor_frame.h"

namespace android_webview {

RenderThreadManager::RenderThreadManager(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_loop)
    : ui_loop_(ui_loop),
      mark_hardware_release_(false),
      weak_factory_on_ui_thread_(this) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  ui_thread_weak_ptr_ = weak_factory_on_ui_thread_.GetWeakPtr();
}

RenderThreadManager::~RenderThreadManager() {
  DCHECK(!hardware_renderer_.get());
  DCHECK(child_frames_.empty());
}

void RenderThreadManager::UpdateParentDrawConstraintsOnUI() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  CheckUiCallsAllowed();
  if (producer_weak_ptr_) {
    producer_weak_ptr_->OnParentDrawDataUpdated(this);
  }
}

void RenderThreadManager::ViewTreeForceDarkStateChangedOnUI(
    bool view_tree_force_dark_state) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  CheckUiCallsAllowed();
  if (producer_weak_ptr_) {
    producer_weak_ptr_->OnViewTreeForceDarkStateChanged(
        view_tree_force_dark_state);
  }
}

void RenderThreadManager::SetScrollOffsetOnUI(gfx::Vector2d scroll_offset) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  CheckUiCallsAllowed();
  base::AutoLock lock(lock_);
  scroll_offset_ = scroll_offset;
}

gfx::Vector2d RenderThreadManager::GetScrollOffsetOnRT() {
  base::AutoLock lock(lock_);
  return scroll_offset_;
}

std::unique_ptr<ChildFrame> RenderThreadManager::SetFrameOnUI(
    std::unique_ptr<ChildFrame> new_frame) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  CheckUiCallsAllowed();
  DCHECK(new_frame);

  base::AutoLock lock(lock_);
  if (child_frames_.empty()) {
    child_frames_.emplace_back(std::move(new_frame));
    return nullptr;
  }
  std::unique_ptr<ChildFrame> uncommitted_frame;
  DCHECK_LE(child_frames_.size(), 2u);
  ChildFrameQueue pruned_frames =
      HardwareRenderer::WaitAndPruneFrameQueue(&child_frames_);
  DCHECK_LE(pruned_frames.size(), 1u);
  if (pruned_frames.size())
    uncommitted_frame = std::move(pruned_frames.front());
  child_frames_.emplace_back(std::move(new_frame));
  return uncommitted_frame;
}

ChildFrameQueue RenderThreadManager::PassFramesOnRT() {
  base::AutoLock lock(lock_);
  ChildFrameQueue returned_frames;
  returned_frames.swap(child_frames_);
  return returned_frames;
}

ChildFrameQueue RenderThreadManager::PassUncommittedFrameOnUI() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  CheckUiCallsAllowed();
  base::AutoLock lock(lock_);
  for (auto& frame_ptr : child_frames_)
    frame_ptr->WaitOnFutureIfNeeded();
  ChildFrameQueue returned_frames;
  returned_frames.swap(child_frames_);
  return returned_frames;
}

void RenderThreadManager::PostParentDrawDataToChildCompositorOnRT(
    const ParentCompositorDrawConstraints& parent_draw_constraints,
    const CompositorID& compositor_id,
    viz::PresentationFeedbackMap presentation_feedbacks) {
  {
    base::AutoLock lock(lock_);
    parent_draw_constraints_ = parent_draw_constraints;
    // Presentation feedbacks are a sequence and it's ok to drop something in
    // the middle of the sequence. This also means its ok to drop the feedbacks
    // from early returned frames from WaitAndPruneFrameQueue as well.
    presentation_feedbacks_ = std::move(presentation_feedbacks);
    compositor_id_for_presentation_feedbacks_ = compositor_id;
  }

  // No need to hold the lock_ during the post task.
  ui_loop_->PostTask(
      FROM_HERE,
      base::BindOnce(&RenderThreadManager::UpdateParentDrawConstraintsOnUI,
                     ui_thread_weak_ptr_));
}

void RenderThreadManager::TakeParentDrawDataOnUI(
    ParentCompositorDrawConstraints* constraints,
    CompositorID* compositor_id,
    viz::PresentationFeedbackMap* presentation_feedbacks) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  DCHECK(presentation_feedbacks->empty());
  CheckUiCallsAllowed();
  base::AutoLock lock(lock_);
  *constraints = parent_draw_constraints_;
  *compositor_id = compositor_id_for_presentation_feedbacks_;
  presentation_feedbacks_.swap(*presentation_feedbacks);
}

void RenderThreadManager::SetInsideHardwareRelease(bool inside) {
  base::AutoLock lock(lock_);
  mark_hardware_release_ = inside;
}

bool RenderThreadManager::IsInsideHardwareRelease() const {
  base::AutoLock lock(lock_);
  return mark_hardware_release_;
}

void RenderThreadManager::InsertReturnedResourcesOnRT(
    const std::vector<viz::ReturnedResource>& resources,
    const CompositorID& compositor_id,
    uint32_t layer_tree_frame_sink_id) {
  if (resources.empty())
    return;
  ui_loop_->PostTask(
      FROM_HERE, base::BindOnce(&CompositorFrameProducer::ReturnUsedResources,
                                producer_weak_ptr_, resources, compositor_id,
                                layer_tree_frame_sink_id));
}

void RenderThreadManager::CommitFrameOnRT() {
  if (hardware_renderer_)
    hardware_renderer_->CommitFrame();
}

void RenderThreadManager::UpdateViewTreeForceDarkStateOnRT(
    bool view_tree_force_dark_state) {
  if (view_tree_force_dark_state_ == view_tree_force_dark_state)
    return;
  view_tree_force_dark_state_ = view_tree_force_dark_state;
  ui_loop_->PostTask(
      FROM_HERE,
      base::BindOnce(&RenderThreadManager::ViewTreeForceDarkStateChangedOnUI,
                     ui_thread_weak_ptr_, view_tree_force_dark_state_));
}

void RenderThreadManager::DrawOnRT(bool save_restore,
                                   HardwareRendererDrawParams* params) {
  // Force GL binding init if it's not yet initialized.
  DeferredGpuCommandService::GetInstance();
  ScopedAppGLStateRestore state_restore(ScopedAppGLStateRestore::MODE_DRAW,
                                        save_restore);
  ScopedAllowGL allow_gl;
  if (!hardware_renderer_ && !IsInsideHardwareRelease() &&
      HasFrameForHardwareRendererOnRT()) {
    hardware_renderer_.reset(new HardwareRenderer(this));
    hardware_renderer_->CommitFrame();
  }

  if (hardware_renderer_)
    hardware_renderer_->Draw(params);
}

void RenderThreadManager::DestroyHardwareRendererOnRT(bool save_restore) {
  DeferredGpuCommandService::GetInstance();
  ScopedAppGLStateRestore state_restore(
      ScopedAppGLStateRestore::MODE_RESOURCE_MANAGEMENT, save_restore);
  ScopedAllowGL allow_gl;
  hardware_renderer_.reset();
}

void RenderThreadManager::RemoveFromCompositorFrameProducerOnUI() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  CheckUiCallsAllowed();
  if (producer_weak_ptr_)
    producer_weak_ptr_->RemoveCompositorFrameConsumer(this);
  weak_factory_on_ui_thread_.InvalidateWeakPtrs();
#if DCHECK_IS_ON()
  ui_calls_allowed_ = false;
#endif  // DCHECK_IS_ON()
}

void RenderThreadManager::SetCompositorFrameProducer(
    CompositorFrameProducer* compositor_frame_producer) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  CheckUiCallsAllowed();
  producer_weak_ptr_ = compositor_frame_producer->GetWeakPtr();
}

bool RenderThreadManager::HasFrameForHardwareRendererOnRT() const {
  base::AutoLock lock(lock_);
  return !child_frames_.empty();
}

RenderThreadManager::InsideHardwareReleaseReset::InsideHardwareReleaseReset(
    RenderThreadManager* render_thread_manager)
    : render_thread_manager_(render_thread_manager) {
  DCHECK(!render_thread_manager_->IsInsideHardwareRelease());
  render_thread_manager_->SetInsideHardwareRelease(true);
}

RenderThreadManager::InsideHardwareReleaseReset::~InsideHardwareReleaseReset() {
  render_thread_manager_->SetInsideHardwareRelease(false);
}

}  // namespace android_webview
