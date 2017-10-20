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

class PLATFORM_EXPORT CompositorMutatorClient
    : public WebCompositorMutatorClient {
 public:
  explicit CompositorMutatorClient(CompositorMutator*);
  virtual ~CompositorMutatorClient();

  void SetMutationUpdate(std::unique_ptr<cc::MutatorOutputState>);

  // cc::LayerTreeMutator
  void SetClient(cc::LayerTreeMutatorClient*);
  void Mutate(base::TimeTicks monotonic_time,
              std::unique_ptr<cc::MutatorInputState>) override;
  // TODO(majidvp): Remove this when CC knows about timeline input.
  bool HasAnimators() override;

  CompositorMutator* Mutator() { return mutator_.Get(); }

 private:
  // Accessed by main and compositor threads.
  CrossThreadPersistent<CompositorMutator> mutator_;
  cc::LayerTreeMutatorClient* client_;
};

}  // namespace blink

#endif  // CompositorMutatorClient_h
