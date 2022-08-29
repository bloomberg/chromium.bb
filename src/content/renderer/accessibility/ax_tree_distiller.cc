// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/ax_tree_distiller.h"

#include <memory>
#include <queue>
#include <vector>

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/accessibility/ax_tree_snapshotter_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

namespace {

static constexpr int kMaxNodes = 5000;

// TODO: Consider moving this to AXNodeProperties.
static const ax::mojom::Role kContentRoles[]{
    ax::mojom::Role::kHeading,
    ax::mojom::Role::kParagraph,
};

// TODO: Consider moving this to AXNodeProperties.
static const ax::mojom::Role kRolesToSkip[]{
    ax::mojom::Role::kAudio,
    ax::mojom::Role::kBanner,
    ax::mojom::Role::kButton,
    ax::mojom::Role::kComplementary,
    ax::mojom::Role::kContentInfo,
    ax::mojom::Role::kFooter,
    ax::mojom::Role::kFooterAsNonLandmark,
    ax::mojom::Role::kHeader,
    ax::mojom::Role::kHeaderAsNonLandmark,
    ax::mojom::Role::kImage,
    ax::mojom::Role::kLabelText,
    ax::mojom::Role::kNavigation,
};

int32_t GetNumberOfChildParagraphs(const ui::AXNode* node) {
  int n = 0;
  for (auto iter = node->UnignoredChildrenBegin();
       iter != node->UnignoredChildrenEnd(); ++iter) {
    if (iter.get()->GetRole() == ax::mojom::Role::kParagraph)
      n++;
  }
  return n;
}

// TODO(crbug.com/1266555): Replace this with a call to
// OneShotAccessibilityTreeSearch.
// The article node is the one which contains the most number of paragraphs.
// TODO(crbug.com/1266555): Ensure that this also includes the header node.
const ui::AXNode* GetArticleNode(const ui::AXNode* node) {
  const ui::AXNode* article = nullptr;
  int32_t max = 0;
  std::queue<const ui::AXNode*> queue;
  queue.push(node);

  while (!queue.empty()) {
    const ui::AXNode* popped = queue.front();
    queue.pop();
    int32_t n = GetNumberOfChildParagraphs(popped);
    if (n > max) {
      max = n;
      article = popped;
    }

    if (!base::Contains(kContentRoles, node->GetRole())) {
      for (auto iter = popped->UnignoredChildrenBegin();
           iter != popped->UnignoredChildrenEnd(); ++iter) {
        queue.push(iter.get());
      }
    }
  }

  return article;
}

// Recurse through the root node, searching for content nodes (any node whose
// role is in kContentRoles). Skip branches which begin with a node with role
// in kRolesToSkip. Once a content node is identified, add it to the vector
// |content_node_ids|, whose pointer is passed through the recursion.
void AddContentNodesToVector(const ui::AXNode* node,
                             std::vector<ui::AXNodeID>* content_node_ids) {
  if (base::Contains(kContentRoles, node->GetRole())) {
    content_node_ids->emplace_back(node->id());
    return;
  }
  if (base::Contains(kRolesToSkip, node->GetRole()))
    return;
  for (auto iter = node->UnignoredChildrenBegin();
       iter != node->UnignoredChildrenEnd(); ++iter) {
    AddContentNodesToVector(iter.get(), content_node_ids);
  }
}

}  // namespace

namespace content {

AXTreeDistiller::AXTreeDistiller(RenderFrameImpl* render_frame)
    : render_frame_(render_frame) {
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (features::IsReadAnythingWithScreen2xEnabled()) {
    render_frame_->GetBrowserInterfaceBroker()->GetInterface(
        main_content_extractor_.BindNewPipeAndPassReceiver());
  }
#endif
}

AXTreeDistiller::~AXTreeDistiller() = default;

void AXTreeDistiller::Distill() {
  SnapshotAXTree();
  DistillAXTree();

  // TODO(https://crbug.com/1278249): Move the call to a proper place.
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (features::IsReadAnythingWithScreen2xEnabled())
    ScheduleScreen2xRun();
#endif
}

void AXTreeDistiller::SnapshotAXTree() {
  // If snapshot_ is already cached, do nothing.
  if (snapshot_)
    return;
  snapshot_ = std::make_unique<ui::AXTreeUpdate>();

  // Get page contents (via snapshot of a11y tree) for reader generation.
  // |ui::AXMode::kHTML| is needed for URL information.
  // |ui::AXMode::kScreenReader| is needed for heading level information.
  ui::AXMode ax_mode =
      ui::AXMode::kWebContents | ui::AXMode::kHTML | ui::AXMode::kScreenReader;
  AXTreeSnapshotterImpl snapshotter(render_frame_, ax_mode);
  snapshotter.Snapshot(
      /* exclude_offscreen= */ false, kMaxNodes,
      /* timeout= */ {}, snapshot_.get());
}

void AXTreeDistiller::DistillAXTree() {
  // If content_node_ids_ is already cached, do nothing.
  if (content_node_ids_)
    return;
  content_node_ids_ = std::make_unique<std::vector<ui::AXNodeID>>();

  DCHECK(snapshot_);
  ui::AXTree tree;
  bool success = tree.Unserialize(*snapshot_);
  if (!success)
    return;

  const ui::AXNode* article_node = GetArticleNode(tree.root());
  // If this page does not have an article node, this means it is not
  // distillable.
  if (!article_node) {
    is_distillable_ = false;
    return;
  }

  AddContentNodesToVector(article_node, content_node_ids_.get());
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void AXTreeDistiller::ScheduleScreen2xRun() {
  DCHECK(main_content_extractor_.is_bound());
  DCHECK(snapshot_);
  main_content_extractor_->ExtractMainContent(
      *snapshot_, base::BindOnce(&AXTreeDistiller::ProcessScreen2xResult,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void AXTreeDistiller::ProcessScreen2xResult(
    const std::vector<ui::AXNodeID>& content_node_ids) {
  // TODO(https://crbug.com/1278249): Use |content_node_ids|.
}

#endif

}  // namespace content
