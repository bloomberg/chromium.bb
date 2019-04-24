// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_ANIMATION_WORKLET_MUTATOR_DISPATCHER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_ANIMATION_WORKLET_MUTATOR_DISPATCHER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/mutator_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

class CompositorMutatorClient;
class MainThreadMutatorClient;

// Fans out requests to all of the registered AnimationWorkletMutators which can
// then run worklet animations to produce mutation updates. Requests for
// animation frames are received from AnimationWorkletMutators and generate a
// new frame.
class PLATFORM_EXPORT AnimationWorkletMutatorDispatcherImpl final
    : public AnimationWorkletMutatorDispatcher {
 public:
  // There are three outputs for the two interface surfaces of the created
  // class blob. The returned owning pointer to the Client, which
  // also owns the rest of the structure. |mutatee| and |mutatee_runner| form a
  // pair for referencing the AnimationWorkletMutatorDispatcherImpl. i.e. Put
  // tasks on the TaskRunner using the WeakPtr to get to the methods.
  static std::unique_ptr<CompositorMutatorClient> CreateCompositorThreadClient(
      base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>* mutatee,
      scoped_refptr<base::SingleThreadTaskRunner>* mutatee_runner);
  static std::unique_ptr<MainThreadMutatorClient> CreateMainThreadClient(
      base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>* mutatee,
      scoped_refptr<base::SingleThreadTaskRunner>* mutatee_runner);

  explicit AnimationWorkletMutatorDispatcherImpl(bool main_thread_task_runner);
  ~AnimationWorkletMutatorDispatcherImpl() override;

  // AnimationWorkletMutatorDispatcher implementation.
  void MutateSynchronously(
      std::unique_ptr<AnimationWorkletDispatcherInput>) override;

  bool MutateAsynchronously(std::unique_ptr<AnimationWorkletDispatcherInput>,
                            MutateQueuingStrategy,
                            AsyncMutationCompleteCallback) override;

  // TODO(majidvp): Remove when timeline inputs are known.
  bool HasMutators() override;

  // Interface for use by the AnimationWorklet Thread(s) to request calls.
  // (To the given Mutator on the given TaskRunner.)
  void RegisterAnimationWorkletMutator(
      CrossThreadPersistent<AnimationWorkletMutator>,
      scoped_refptr<base::SingleThreadTaskRunner> mutator_runner);

  void UnregisterAnimationWorkletMutator(
      CrossThreadPersistent<AnimationWorkletMutator>);

  void SetClient(MutatorClient* client) { client_ = client; }

  void SynchronizeAnimatorName(const String& animator_name);

  MutatorClient* client() { return client_; }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
    return host_queue_;
  }

  base::WeakPtr<AnimationWorkletMutatorDispatcherImpl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  class OutputVectorRef;

  using InputMap = HashMap<int, std::unique_ptr<AnimationWorkletInput>>;

  using AnimationWorkletMutatorToTaskRunnerMap =
      HashMap<CrossThreadPersistent<AnimationWorkletMutator>,
              scoped_refptr<base::SingleThreadTaskRunner>>;

  InputMap CreateInputMap(AnimationWorkletDispatcherInput& mutator_input) const;

  // Dispatches mutation update requests. The callback is triggered once all
  // mutation updates have been computed and it runs on the animation worklet
  // thread associated with the last mutation to complete.
  void RequestMutations(WTF::CrossThreadClosure done_callback);

  void MutateAsynchronouslyInternal(AsyncMutationCompleteCallback);

  void AsyncMutationsDone(int async_mutation_id);

  // Returns true if any updates were applied.
  bool ApplyMutationsOnHostThread();

  // The AnimationWorkletProxyClients are also owned by the WorkerClients
  // dictionary.
  AnimationWorkletMutatorToTaskRunnerMap mutator_map_;

  template <typename ClientType>
  static std::unique_ptr<ClientType> CreateClient(
      base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>* weak_interface,
      scoped_refptr<base::SingleThreadTaskRunner>* queue,
      bool create_main_thread_client);

  scoped_refptr<base::SingleThreadTaskRunner> host_queue_;

  // The MutatorClient owns (std::unique_ptr) us, so this pointer is
  // valid as long as this class exists.
  MutatorClient* client_;

  // Map of mutator scope IDs to mutator input. The Mutate methods safeguards
  // against concurrent calls (important once async mutations are introduced) by
  // checking that the map has been reset on entry. For this reason, it is
  // important to reset the map at the end of the mutation cycle.
  InputMap mutator_input_map_;

  // Reference to a vector for collecting mutation output. The vector is
  // accessed across threads, thus it must be guaranteed to persist until the
  // last mutation update is complete, and updates must be done in a thread-safe
  // manner. The Mutate method guards against concurrent calls (important once
  // async mutations are introduced)  by checking that the output vector is
  // empty. For this reason, it is important to clear the output at the end of
  // the mutation cycle.
  scoped_refptr<OutputVectorRef> outputs_;

  // Input queued for the next mutation cycle, which will automatically be
  // triggered at the completion of the current cycle. Only one collection of
  // inputs can be queued at any point in time. In a typical frame, we expect
  // one request for the active tree, and one for the pending tree. The active
  // tree mutation is best effort will be dropped when busy. A pending tree
  // mutation is queued to ensure initial values are resolved. The pending tree
  // activation is blocked until the previous request completes preventing the
  // need to queue multiple requests.
  std::unique_ptr<AnimationWorkletDispatcherInput> queued_mutator_input_;

  // Active and queued callbacks for the completion of an async mutation cycle.
  AsyncMutationCompleteCallback on_async_mutation_complete_;
  AsyncMutationCompleteCallback queued_on_async_mutation_complete_;

  base::WeakPtrFactory<AnimationWorkletMutatorDispatcherImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AnimationWorkletMutatorDispatcherImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_ANIMATION_WORKLET_MUTATOR_DISPATCHER_IMPL_H_
