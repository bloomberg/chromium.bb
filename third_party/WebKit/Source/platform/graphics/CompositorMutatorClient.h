// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutatorClient_h
#define CompositorMutatorClient_h

#include <memory>
#include "cc/trees/layer_tree_mutator.h"
#include "platform/PlatformExport.h"

namespace blink {

class CompositorMutatorImpl;

class PLATFORM_EXPORT CompositorMutatorClient : public cc::LayerTreeMutator {
 public:
  explicit CompositorMutatorClient(std::unique_ptr<CompositorMutatorImpl>);
  virtual ~CompositorMutatorClient();

  void SetMutationUpdate(std::unique_ptr<cc::MutatorOutputState>);

  // cc::LayerTreeMutator
  void SetClient(cc::LayerTreeMutatorClient*);
  void Mutate(std::unique_ptr<cc::MutatorInputState>) override;
  bool HasAnimators() override;

 private:
  std::unique_ptr<CompositorMutatorImpl> mutator_;
  cc::LayerTreeMutatorClient* client_;
};

}  // namespace blink

#endif  // CompositorMutatorClient_h
