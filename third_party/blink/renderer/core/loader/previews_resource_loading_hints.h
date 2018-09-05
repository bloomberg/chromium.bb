// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PREVIEWS_RESOURCE_LOADING_HINTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PREVIEWS_RESOURCE_LOADING_HINTS_H_

#include <vector>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
class KURL;

// PreviewsResourceLoadingHints stores the resource loading hints that apply to
// a single document.
class CORE_EXPORT PreviewsResourceLoadingHints final
    : public GarbageCollectedFinalized<PreviewsResourceLoadingHints> {
 public:
  static PreviewsResourceLoadingHints* Create(
      ExecutionContext& execution_context,
      const std::vector<WTF::String>& subresource_patterns_to_block);

  ~PreviewsResourceLoadingHints();

  // Returns true if load of resource with URL |resource_url| and priority
  // |resource_load_priority| is allowed as per resource loading hints.
  bool AllowLoad(const KURL& resource_url,
                 ResourceLoadPriority resource_load_priority) const;

  virtual void Trace(blink::Visitor*);

 private:
  PreviewsResourceLoadingHints(
      ExecutionContext* execution_context,
      const std::vector<WTF::String>& subresource_patterns_to_block);

  // Reports to console when loading of |resource_url| is blocked.
  void ReportBlockedLoading(const KURL& resource_url) const;

  Member<ExecutionContext> execution_context_;

  // |subresource_patterns_to_block_| is a collection of subresource patterns
  // for resources whose loading should be blocked. Each pattern is a
  // WTF::String. If a subresource URL contains any of the strings specified in
  // |subresource_patterns_to_block_|, then that subresource's loading could
  // be blocked.
  const std::vector<WTF::String> subresource_patterns_to_block_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PREVIEWS_RESOURCE_LOADING_HINTS_H_
