// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_UNVERIFIED_RULESET_DEALER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_UNVERIFIED_RULESET_DEALER_H_

#include "base/macros.h"
#include "components/subresource_filter/content/common/ruleset_dealer.h"
#include "content/public/renderer/render_thread_observer.h"
#include "ipc/ipc_platform_file.h"

namespace IPC {
class Message;
}  // namespace IPC

namespace subresource_filter {

class MemoryMappedRuleset;

// Memory maps the subresource filtering ruleset file received over IPC from the
// RulesetDistributor, and makes it available to all SubresourceFilterAgents
// within the current render process through GetRuleset() method. Does not make
// sure that the file is valid.
//
// See RulesetDealerBase for details on the lifetime of MemoryMappedRuleset, and
// the distribution pipeline diagram in content_ruleset_service.h.
class UnverifiedRulesetDealer : public RulesetDealer,
                                public content::RenderThreadObserver {
 public:
  UnverifiedRulesetDealer();
  ~UnverifiedRulesetDealer() override;

 private:
  // content::RenderThreadObserver:
  bool OnControlMessageReceived(const IPC::Message& message) override;
  void OnSetRulesetForProcess(const IPC::PlatformFileForTransit& file);

  DISALLOW_COPY_AND_ASSIGN(UnverifiedRulesetDealer);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_RULESET_DEALER_H_
