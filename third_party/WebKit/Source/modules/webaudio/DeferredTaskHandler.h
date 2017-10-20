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

#ifndef DeferredTaskHandler_h
#define DeferredTaskHandler_h

#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/ThreadSafeRefCounted.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/ThreadingPrimitives.h"
#include "platform/wtf/Vector.h"

namespace blink {

class BaseAudioContext;
class OfflineAudioContext;
class AudioHandler;
class AudioNodeOutput;
class AudioSummingJunction;

// DeferredTaskHandler manages the major part of pre- and post- rendering tasks,
// and provides a lock mechanism against the audio rendering graph. A
// DeferredTaskHandler object is created when an BaseAudioContext object is
// created.
//
// DeferredTaskHandler outlives the BaseAudioContext only if all of the
// following conditions match:
// - An audio rendering thread is running,
// - It is requested to stop,
// - The audio rendering thread calls requestToDeleteHandlersOnMainThread(),
// - It posts a task of deleteHandlersOnMainThread(), and
// - GC happens and it collects the BaseAudioContext before the task execution.
//
class MODULES_EXPORT DeferredTaskHandler final
    : public ThreadSafeRefCounted<DeferredTaskHandler> {
 public:
  static scoped_refptr<DeferredTaskHandler> Create();
  ~DeferredTaskHandler();

  void HandleDeferredTasks();
  void ContextWillBeDestroyed();

  // BaseAudioContext can pull node(s) at the end of each render quantum even
  // when they are not connected to any downstream nodes.  These two methods are
  // called by the nodes who want to add/remove themselves into/from the
  // automatic pull lists.
  void AddAutomaticPullNode(AudioHandler*);
  void RemoveAutomaticPullNode(AudioHandler*);
  // Called right before handlePostRenderTasks() to handle nodes which need to
  // be pulled even when they are not connected to anything.
  void ProcessAutomaticPullNodes(size_t frames_to_process);

  // Keep track of AudioNode's that have their channel count mode changed. We
  // process the changes in the post rendering phase.
  void AddChangedChannelCountMode(AudioHandler*);
  void RemoveChangedChannelCountMode(AudioHandler*);

  // Keep track of AudioNode's that have their channel interpretation
  // changed. We process the changes in the post rendering phase.
  void AddChangedChannelInterpretation(AudioHandler*);
  void RemoveChangedChannelInterpretation(AudioHandler*);

  // Only accessed when the graph lock is held.
  void MarkSummingJunctionDirty(AudioSummingJunction*);
  // Only accessed when the graph lock is held. Must be called on the main
  // thread.
  void RemoveMarkedSummingJunction(AudioSummingJunction*);

  void MarkAudioNodeOutputDirty(AudioNodeOutput*);
  void RemoveMarkedAudioNodeOutput(AudioNodeOutput*);

  // In AudioNode::breakConnection() and deref(), a tryLock() is used for
  // calling actual processing, but if it fails keep track here.
  void AddDeferredBreakConnection(AudioHandler&);
  void BreakConnections();

  void AddRenderingOrphanHandler(scoped_refptr<AudioHandler>);
  void RequestToDeleteHandlersOnMainThread();
  void ClearHandlersToBeDeleted();

  //
  // Thread Safety and Graph Locking:
  //
  void SetAudioThreadToCurrentThread();
  ThreadIdentifier AudioThread() const { return AcquireLoad(&audio_thread_); }

  // TODO(hongchan): Use no-barrier load here. (crbug.com/247328)
  //
  // It is okay to use a relaxed (no-barrier) load here. Because the data
  // referenced by m_audioThread is not actually being used, thus we do not
  // need a barrier between the load of m_audioThread and of that data.
  bool IsAudioThread() const {
    return CurrentThread() == AcquireLoad(&audio_thread_);
  }

  void lock();
  bool TryLock();
  void unlock();

  // This locks the audio render thread for OfflineAudioContext rendering.
  // MUST NOT be used in the real-time audio context.
  void OfflineLock();

  // Returns true if this thread owns the context's lock.
  bool IsGraphOwner();

  class MODULES_EXPORT GraphAutoLocker {
    STACK_ALLOCATED();

   public:
    explicit GraphAutoLocker(DeferredTaskHandler& handler) : handler_(handler) {
      handler_.lock();
    }
    explicit GraphAutoLocker(BaseAudioContext*);

    ~GraphAutoLocker() { handler_.unlock(); }

   private:
    DeferredTaskHandler& handler_;
  };

  // This is for locking offline render thread (which is considered as the
  // audio thread) with unlocking on self-destruction at the end of the scope.
  // Also note that it uses lock() rather than tryLock() because the timing
  // MUST be accurate on offline rendering.
  class MODULES_EXPORT OfflineGraphAutoLocker {
    STACK_ALLOCATED();

   public:
    explicit OfflineGraphAutoLocker(OfflineAudioContext*);

    ~OfflineGraphAutoLocker() { handler_.unlock(); }

   private:
    DeferredTaskHandler& handler_;
  };

 private:
  DeferredTaskHandler();
  void UpdateAutomaticPullNodes();
  void UpdateChangedChannelCountMode();
  void UpdateChangedChannelInterpretation();
  void HandleDirtyAudioSummingJunctions();
  void HandleDirtyAudioNodeOutputs();
  void DeleteHandlersOnMainThread();

  // For the sake of thread safety, we maintain a seperate Vector of automatic
  // pull nodes for rendering in m_renderingAutomaticPullNodes.  It will be
  // copied from m_automaticPullNodes by updateAutomaticPullNodes() at the
  // very start or end of the rendering quantum.
  HashSet<AudioHandler*> automatic_pull_nodes_;
  Vector<AudioHandler*> rendering_automatic_pull_nodes_;
  // m_automaticPullNodesNeedUpdating keeps track if m_automaticPullNodes is
  // modified.
  bool automatic_pull_nodes_need_updating_;

  // Collection of nodes where the channel count mode has changed. We want the
  // channel count mode to change in the pre- or post-rendering phase so as
  // not to disturb the running audio thread.
  HashSet<AudioHandler*> deferred_count_mode_change_;

  HashSet<AudioHandler*> deferred_channel_interpretation_change_;

  // These two HashSet must be accessed only when the graph lock is held.
  // These raw pointers are safe because their destructors unregister them.
  HashSet<AudioSummingJunction*> dirty_summing_junctions_;
  HashSet<AudioNodeOutput*> dirty_audio_node_outputs_;

  // Only accessed in the audio thread.
  Vector<AudioHandler*> deferred_break_connection_list_;

  Vector<scoped_refptr<AudioHandler>> rendering_orphan_handlers_;
  Vector<scoped_refptr<AudioHandler>> deletable_orphan_handlers_;

  // Graph locking.
  RecursiveMutex context_graph_mutex_;
  volatile ThreadIdentifier audio_thread_;
};

}  // namespace blink

#endif  // DeferredTaskHandler_h
