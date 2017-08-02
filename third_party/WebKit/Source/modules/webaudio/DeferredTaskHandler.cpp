/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "modules/webaudio/DeferredTaskHandler.h"
#include "modules/webaudio/AudioNode.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/OfflineAudioContext.h"
#include "platform/CrossThreadFunctional.h"
#include "public/platform/Platform.h"

namespace blink {

void DeferredTaskHandler::lock() {
  // Don't allow regular lock in real-time audio thread.
  DCHECK(!IsAudioThread());
  context_graph_mutex_.lock();
}

bool DeferredTaskHandler::TryLock() {
  // Try to catch cases of using try lock on main thread
  // - it should use regular lock.
  DCHECK(IsAudioThread());
  if (!IsAudioThread()) {
    // In release build treat tryLock() as lock() (since above
    // DCHECK(isAudioThread) never fires) - this is the best we can do.
    lock();
    return true;
  }
  return context_graph_mutex_.TryLock();
}

void DeferredTaskHandler::unlock() {
  context_graph_mutex_.unlock();
}

void DeferredTaskHandler::OfflineLock() {
  // CHECK is here to make sure to explicitly crash if this is called from
  // other than the offline render thread, which is considered as the audio
  // thread in OfflineAudioContext.
  CHECK(IsAudioThread()) << "DeferredTaskHandler::offlineLock() must be called "
                            "within the offline audio thread.";

  context_graph_mutex_.lock();
}

bool DeferredTaskHandler::IsGraphOwner() {
#if DCHECK_IS_ON()
  return context_graph_mutex_.Locked();
#else
  // The method is only used inside of DCHECK() so it must be no-op in the
  // release build. Returning false so we can catch when it happens.
  return false;
#endif
}

void DeferredTaskHandler::AddDeferredBreakConnection(AudioHandler& node) {
  DCHECK(IsAudioThread());
  deferred_break_connection_list_.push_back(&node);
}

void DeferredTaskHandler::BreakConnections() {
  DCHECK(IsAudioThread());
  DCHECK(IsGraphOwner());

  for (unsigned i = 0; i < deferred_break_connection_list_.size(); ++i)
    deferred_break_connection_list_[i]->BreakConnectionWithLock();
  deferred_break_connection_list_.clear();
}

void DeferredTaskHandler::MarkSummingJunctionDirty(
    AudioSummingJunction* summing_junction) {
  DCHECK(IsGraphOwner());
  dirty_summing_junctions_.insert(summing_junction);
}

void DeferredTaskHandler::RemoveMarkedSummingJunction(
    AudioSummingJunction* summing_junction) {
  DCHECK(IsMainThread());
  AutoLocker locker(*this);
  dirty_summing_junctions_.erase(summing_junction);
}

void DeferredTaskHandler::MarkAudioNodeOutputDirty(AudioNodeOutput* output) {
  DCHECK(IsGraphOwner());
  DCHECK(IsMainThread());
  dirty_audio_node_outputs_.insert(output);
}

void DeferredTaskHandler::RemoveMarkedAudioNodeOutput(AudioNodeOutput* output) {
  DCHECK(IsGraphOwner());
  DCHECK(IsMainThread());
  dirty_audio_node_outputs_.erase(output);
}

void DeferredTaskHandler::HandleDirtyAudioSummingJunctions() {
  DCHECK(IsGraphOwner());

  for (AudioSummingJunction* junction : dirty_summing_junctions_)
    junction->UpdateRenderingState();
  dirty_summing_junctions_.clear();
}

void DeferredTaskHandler::HandleDirtyAudioNodeOutputs() {
  DCHECK(IsGraphOwner());

  HashSet<AudioNodeOutput*> dirty_outputs;
  dirty_audio_node_outputs_.swap(dirty_outputs);

  // Note: the updating of rendering state may cause output nodes
  // further down the chain to be marked as dirty. These will not
  // be processed in this render quantum.
  for (AudioNodeOutput* output : dirty_outputs)
    output->UpdateRenderingState();
}

void DeferredTaskHandler::AddAutomaticPullNode(AudioHandler* node) {
  DCHECK(IsGraphOwner());

  if (!automatic_pull_nodes_.Contains(node)) {
    automatic_pull_nodes_.insert(node);
    automatic_pull_nodes_need_updating_ = true;
  }
}

void DeferredTaskHandler::RemoveAutomaticPullNode(AudioHandler* node) {
  DCHECK(IsGraphOwner());

  if (automatic_pull_nodes_.Contains(node)) {
    automatic_pull_nodes_.erase(node);
    automatic_pull_nodes_need_updating_ = true;
  }
}

void DeferredTaskHandler::UpdateAutomaticPullNodes() {
  DCHECK(IsGraphOwner());

  if (automatic_pull_nodes_need_updating_) {
    CopyToVector(automatic_pull_nodes_, rendering_automatic_pull_nodes_);
    automatic_pull_nodes_need_updating_ = false;
  }
}

void DeferredTaskHandler::ProcessAutomaticPullNodes(size_t frames_to_process) {
  DCHECK(IsAudioThread());

  for (unsigned i = 0; i < rendering_automatic_pull_nodes_.size(); ++i)
    rendering_automatic_pull_nodes_[i]->ProcessIfNecessary(frames_to_process);
}

void DeferredTaskHandler::AddChangedChannelCountMode(AudioHandler* node) {
  DCHECK(IsGraphOwner());
  DCHECK(IsMainThread());
  deferred_count_mode_change_.insert(node);
}

void DeferredTaskHandler::RemoveChangedChannelCountMode(AudioHandler* node) {
  DCHECK(IsGraphOwner());

  deferred_count_mode_change_.erase(node);
}

void DeferredTaskHandler::AddChangedChannelInterpretation(AudioHandler* node) {
  DCHECK(IsGraphOwner());
  DCHECK(IsMainThread());
  deferred_channel_interpretation_change_.insert(node);
}

void DeferredTaskHandler::RemoveChangedChannelInterpretation(
    AudioHandler* node) {
  DCHECK(IsGraphOwner());

  deferred_channel_interpretation_change_.erase(node);
}

void DeferredTaskHandler::UpdateChangedChannelCountMode() {
  DCHECK(IsGraphOwner());

  for (AudioHandler* node : deferred_count_mode_change_)
    node->UpdateChannelCountMode();
  deferred_count_mode_change_.clear();
}

void DeferredTaskHandler::UpdateChangedChannelInterpretation() {
  DCHECK(IsGraphOwner());

  for (AudioHandler* node : deferred_channel_interpretation_change_)
    node->UpdateChannelInterpretation();
  deferred_channel_interpretation_change_.clear();
}

DeferredTaskHandler::DeferredTaskHandler()
    : automatic_pull_nodes_need_updating_(false), audio_thread_(0) {}

PassRefPtr<DeferredTaskHandler> DeferredTaskHandler::Create() {
  return AdoptRef(new DeferredTaskHandler());
}

DeferredTaskHandler::~DeferredTaskHandler() {
  DCHECK(!automatic_pull_nodes_.size());
  if (automatic_pull_nodes_need_updating_)
    rendering_automatic_pull_nodes_.resize(automatic_pull_nodes_.size());
  DCHECK(!rendering_automatic_pull_nodes_.size());
}

void DeferredTaskHandler::HandleDeferredTasks() {
  UpdateChangedChannelCountMode();
  UpdateChangedChannelInterpretation();
  HandleDirtyAudioSummingJunctions();
  HandleDirtyAudioNodeOutputs();
  UpdateAutomaticPullNodes();
}

void DeferredTaskHandler::ContextWillBeDestroyed() {
  for (auto& handler : rendering_orphan_handlers_)
    handler->ClearContext();
  for (auto& handler : deletable_orphan_handlers_)
    handler->ClearContext();
  ClearHandlersToBeDeleted();
  // Some handlers might live because of their cross thread tasks.
}

DeferredTaskHandler::AutoLocker::AutoLocker(BaseAudioContext* context)
    : handler_(context->GetDeferredTaskHandler()) {
  handler_.lock();
}

DeferredTaskHandler::OfflineGraphAutoLocker::OfflineGraphAutoLocker(
    OfflineAudioContext* context)
    : handler_(context->GetDeferredTaskHandler()) {
  handler_.OfflineLock();
}

void DeferredTaskHandler::AddRenderingOrphanHandler(
    PassRefPtr<AudioHandler> handler) {
  DCHECK(handler);
  DCHECK(!rendering_orphan_handlers_.Contains(handler));
  rendering_orphan_handlers_.push_back(std::move(handler));
}

void DeferredTaskHandler::RequestToDeleteHandlersOnMainThread() {
  DCHECK(IsGraphOwner());
  DCHECK(IsAudioThread());
  if (rendering_orphan_handlers_.IsEmpty())
    return;
  deletable_orphan_handlers_.AppendVector(rendering_orphan_handlers_);
  rendering_orphan_handlers_.clear();
  Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&DeferredTaskHandler::DeleteHandlersOnMainThread,
                      PassRefPtr<DeferredTaskHandler>(this)));
}

void DeferredTaskHandler::DeleteHandlersOnMainThread() {
  DCHECK(IsMainThread());
  AutoLocker locker(*this);
  deletable_orphan_handlers_.clear();
}

void DeferredTaskHandler::ClearHandlersToBeDeleted() {
  DCHECK(IsMainThread());
  AutoLocker locker(*this);
  rendering_orphan_handlers_.clear();
  deletable_orphan_handlers_.clear();
}

void DeferredTaskHandler::SetAudioThreadToCurrentThread() {
  DCHECK(!IsMainThread());
  ThreadIdentifier thread = CurrentThread();
  ReleaseStore(&audio_thread_, thread);
}

}  // namespace blink
