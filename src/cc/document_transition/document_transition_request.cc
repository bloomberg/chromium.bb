// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/document_transition/document_transition_request.h"

#include <algorithm>
#include <map>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "cc/document_transition/document_transition_shared_element_id.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/compositor_render_pass.h"

namespace cc {
namespace {

std::string TypeToString(viz::CompositorFrameTransitionDirective::Type type) {
  switch (type) {
    case viz::CompositorFrameTransitionDirective::Type::kSave:
      return "kSave";
    case viz::CompositorFrameTransitionDirective::Type::kAnimate:
      return "kAnimate";
    case viz::CompositorFrameTransitionDirective::Type::kAnimateRenderer:
      return "kAnimateRenderer";
    case viz::CompositorFrameTransitionDirective::Type::kRelease:
      return "kRelease";
  }
  return "<unknown>";
}

}  // namespace

uint32_t DocumentTransitionRequest::s_next_sequence_id_ = 1;

// static
std::unique_ptr<DocumentTransitionRequest>
DocumentTransitionRequest::CreateCapture(
    uint32_t document_tag,
    uint32_t shared_element_count,
    std::vector<viz::SharedElementResourceId> capture_ids,
    base::OnceClosure commit_callback) {
  return base::WrapUnique(new DocumentTransitionRequest(
      Type::kSave, document_tag, shared_element_count, std::move(capture_ids),
      std::move(commit_callback)));
}

// static
std::unique_ptr<DocumentTransitionRequest>
DocumentTransitionRequest::CreateAnimateRenderer(uint32_t document_tag) {
  return base::WrapUnique(new DocumentTransitionRequest(
      Type::kAnimateRenderer, document_tag, 0u, {}, base::DoNothing()));
}

// static
std::unique_ptr<DocumentTransitionRequest>
DocumentTransitionRequest::CreateRelease(uint32_t document_tag) {
  return base::WrapUnique(new DocumentTransitionRequest(
      Type::kRelease, document_tag, 0u, {}, base::DoNothing()));
}

DocumentTransitionRequest::DocumentTransitionRequest(
    Type type,
    uint32_t document_tag,
    uint32_t shared_element_count,
    std::vector<viz::SharedElementResourceId> capture_ids,
    base::OnceClosure commit_callback)
    : type_(type),
      document_tag_(document_tag),
      shared_element_count_(shared_element_count),
      commit_callback_(std::move(commit_callback)),
      sequence_id_(s_next_sequence_id_++),
      capture_resource_ids_(std::move(capture_ids)) {}

DocumentTransitionRequest::~DocumentTransitionRequest() = default;

viz::CompositorFrameTransitionDirective
DocumentTransitionRequest::ConstructDirective(
    const std::map<DocumentTransitionSharedElementId, SharedElementInfo>&
        shared_element_render_pass_id_map) const {
  std::vector<viz::CompositorFrameTransitionDirective::SharedElement>
      shared_elements(shared_element_count_);
  auto capture_resource_ids = capture_resource_ids_;
  for (uint32_t i = 0; i < shared_elements.size(); ++i) {
    auto it = std::find_if(
        shared_element_render_pass_id_map.begin(),
        shared_element_render_pass_id_map.end(),
        [this, i](const std::pair<const DocumentTransitionSharedElementId,
                                  SharedElementInfo>& value) {
          return value.first.Matches(document_tag_, i);
        });
    if (it == shared_element_render_pass_id_map.end())
      continue;
    shared_elements[i].render_pass_id = it->second.render_pass_id;
    shared_elements[i].shared_element_resource_id = it->second.resource_id;

    // Remove the resource id from our capture ids, since we just want to have
    // "empty" resource ids left -- the ones that don't have a render pass
    // associated with them.
    capture_resource_ids.erase(
        std::remove(capture_resource_ids.begin(), capture_resource_ids.end(),
                    it->second.resource_id),
        capture_resource_ids.end());
  }

  // Add invalid render pass id for each empty resource id left in capture ids.
  for (auto& empty_resource_id : capture_resource_ids) {
    shared_elements.emplace_back();
    shared_elements.back().shared_element_resource_id = empty_resource_id;
  }

  // TODO(vmpstr): Clean up the directive parameters.
  return viz::CompositorFrameTransitionDirective(
      sequence_id_, type_, /*is_renderer_driven_animation=*/true,
      viz::CompositorFrameTransitionDirective::Effect::kNone, {},
      std::move(shared_elements));
}

std::string DocumentTransitionRequest::ToString() const {
  std::ostringstream str;
  str << "[type: " << TypeToString(type_) << " sequence_id: " << sequence_id_
      << "]";
  return str.str();
}

DocumentTransitionRequest::SharedElementInfo::SharedElementInfo() = default;
DocumentTransitionRequest::SharedElementInfo::~SharedElementInfo() = default;

}  // namespace cc
