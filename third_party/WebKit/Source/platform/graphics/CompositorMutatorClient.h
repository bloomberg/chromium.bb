// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutatorClient_h
#define CompositorMutatorClient_h

#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebCompositorMutatorClient.h"
#include <memory>

namespace blink {

class CompositorMutator;
struct CompositorMutations;
class CompositorMutationsTarget;

class PLATFORM_EXPORT CompositorMutatorClient
    : public WebCompositorMutatorClient {
 public:
  CompositorMutatorClient(CompositorMutator*, CompositorMutationsTarget*);
  virtual ~CompositorMutatorClient();

  void SetNeedsMutate();

  // cc::LayerTreeMutator
  bool Mutate(base::TimeTicks monotonic_time) override;
  void SetClient(cc::LayerTreeMutatorClient*) override;
  base::Closure TakeMutations() override;

  CompositorMutator* Mutator() { return mutator_.Get(); }

  void SetMutationsForTesting(std::unique_ptr<CompositorMutations>);

 private:
  cc::LayerTreeMutatorClient* client_;
  CompositorMutationsTarget* mutations_target_;
  // Accessed by main and compositor threads.
  CrossThreadPersistent<CompositorMutator> mutator_;
  std::unique_ptr<CompositorMutations> mutations_;
};

}  // namespace blink

#endif  // CompositorMutatorClient_h
