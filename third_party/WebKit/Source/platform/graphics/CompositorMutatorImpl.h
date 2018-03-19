// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutatorImpl_h
#define CompositorMutatorImpl_h

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "platform/graphics/CompositorAnimator.h"
#include "platform/graphics/CompositorMutator.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Persistent.h"
#include "platform/heap/Visitor.h"
#include "platform/wtf/HashSet.h"

namespace blink {

class CompositorMutatorClient;
class WaitableEvent;

// Fans out requests from the compositor to all of the registered
// CompositorAnimators which can then mutate layers through their respective
// mutate interface. Requests for animation frames are received from
// CompositorAnimators and sent to the compositor to generate a new compositor
// frame.
//
// Unless otherwise noted, this should be accessed only on the compositor
// thread.
class PLATFORM_EXPORT CompositorMutatorImpl final : public CompositorMutator {
 public:
  // There are three outputs for the two interface surfaces of the created
  // class blob. The returned owning pointer to the Client, which
  // also owns the rest of the structure. |mutatee| and |mutatee_runner| form a
  // pair for referencing the CompositorMutatorImpl. i.e. Put tasks on the
  // TaskRunner using the WeakPtr to get to the methods.
  static std::unique_ptr<CompositorMutatorClient> CreateClient(
      base::WeakPtr<CompositorMutatorImpl>* mutatee,
      scoped_refptr<base::SingleThreadTaskRunner>* mutatee_runner);

  CompositorMutatorImpl();
  ~CompositorMutatorImpl();

  // CompositorMutator implementation.
  void Mutate(std::unique_ptr<CompositorMutatorInputState>) override;
  // TODO(majidvp): Remove when timeline inputs are known.
  bool HasAnimators() override;

  // Interface for use by the AnimationWorklet Thread(s) to request calls.
  // (To the given Animator on the given TaskRunner.)
  void RegisterCompositorAnimator(
      CrossThreadPersistent<CompositorAnimator>,
      scoped_refptr<base::SingleThreadTaskRunner> animator_runner);

  void UnregisterCompositorAnimator(CrossThreadPersistent<CompositorAnimator>);

  void SetClient(CompositorMutatorClient* client) { client_ = client; }

 private:
  using CompositorAnimators =
      HashSet<CrossThreadPersistent<CompositorAnimator>>;

  class AutoSignal {
    WTF_MAKE_NONCOPYABLE(AutoSignal);

   public:
    explicit AutoSignal(WaitableEvent*);
    ~AutoSignal();

   private:
    WaitableEvent* event_;
  };

  // The AnimationWorkletProxyClientImpls are also owned by the WorkerClients
  // dictionary.
  CompositorAnimators animators_;

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
    return mutator_queue_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> mutator_queue_;

  // Currently we only support a single queue for animators.
  scoped_refptr<base::SingleThreadTaskRunner> animator_queue_;

  // The CompositorMutatorClient owns (std::unique_ptr) us, so this pointer is
  // valid as long as this class exists.
  CompositorMutatorClient* client_;

  base::WeakPtrFactory<CompositorMutatorImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CompositorMutatorImpl);
};

}  // namespace blink

#endif  // CompositorMutatorImpl_h
