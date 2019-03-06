// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/layout_custom.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_fragment.h"
#include "third_party/blink/renderer/core/layout/custom/fragment_result_options.h"
#include "third_party/blink/renderer/core/layout/custom/layout_worklet.h"
#include "third_party/blink/renderer/core/layout/custom/layout_worklet_global_scope.h"
#include "third_party/blink/renderer/core/layout/custom/layout_worklet_global_scope_proxy.h"
#include "third_party/blink/renderer/core/layout/layout_state.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"

namespace blink {

// This scope should be added when about to perform a web-developer defined
// layout. This sets the phase_ flag which changes how children are sized.
class LayoutCustomPhaseScope {
  STACK_ALLOCATED();

 public:
  explicit LayoutCustomPhaseScope(LayoutCustom& layout_custom)
      : layout_custom_(layout_custom) {
    layout_custom_.phase_ = LayoutCustomPhase::kCustom;
  }

  ~LayoutCustomPhaseScope() {
    layout_custom_.phase_ = LayoutCustomPhase::kFallback;
  }

 private:
  LayoutCustom& layout_custom_;
};

LayoutCustom::LayoutCustom(Element* element)
    : LayoutBlockFlow(element), phase_(LayoutCustomPhase::kFallback) {
  DCHECK(element);
}

SerializedScriptValue* LayoutCustom::GetConstraintData() const {
  return constraint_data_.get();
}

void LayoutCustom::SetConstraintData(
    scoped_refptr<SerializedScriptValue> data) {
  constraint_data_ = std::move(data);
}

void LayoutCustom::ClearConstraintData() {
  constraint_data_ = nullptr;
}

SerializedScriptValue* LayoutCustom::GetFragmentResultData() const {
  return fragment_result_data_.get();
}

void LayoutCustom::AddChild(LayoutObject* new_child,
                            LayoutObject* before_child) {
  // Only use the block-flow AddChild logic when we are unloaded, i.e. we
  // should behave exactly like a block-flow.
  if (state_ == kUnloaded) {
    LayoutBlockFlow::AddChild(new_child, before_child);
    return;
  }

  LayoutBlock::AddChild(new_child, before_child);
}

void LayoutCustom::RemoveChild(LayoutObject* child) {
  // Only use the block-flow RemoveChild logic when we are unloaded, i.e. we
  // should behave exactly like a block-flow.
  if (state_ == kUnloaded) {
    LayoutBlockFlow::RemoveChild(child);
    return;
  }

  LayoutBlock::RemoveChild(child);
}

void LayoutCustom::StyleDidChange(StyleDifference diff,
                                  const ComputedStyle* old_style) {
  // TODO(ikilpatrick): This function needs to look at the web developer
  // specified "inputProperties" to potentially invalidate the layout.
  // We will also need to investigate reducing the properties which
  // LayoutBlockFlow::StyleDidChange invalidates upon. (For example margins).

  LayoutWorklet* worklet = LayoutWorklet::From(*GetDocument().domWindow());
  const AtomicString& name = StyleRef().DisplayLayoutCustomName();
  state_ =
      worklet->GetDocumentDefinitionMap()->Contains(name) ? kBlock : kUnloaded;

  // Make our children "block-level" before invoking StyleDidChange. As the
  // current multi-col logic may invoke a call to AddChild, failing a DCHECK.
  if (state_ != kUnloaded)
    SetChildrenInline(false);

  LayoutBlockFlow::StyleDidChange(diff, old_style);

  // Register if we'll need to reattach the layout tree when a matching
  // "layout()" is registered.
  if (state_ == kUnloaded)
    worklet->AddPendingLayout(name, GetNode());
}

void LayoutCustom::UpdateBlockLayout(bool relayout_children) {
  DCHECK(NeedsLayout());

  if (!relayout_children && SimplifiedLayout())
    return;

  // We may end up with multiple SubtreeLayoutScopes on the stack for the same
  // layout object. However we can't create one inside PerformLayout as it may
  // not succeed.
  SubtreeLayoutScope layout_scope(*this);

  // TODO(ikilpatrick): We may need to clear the floating objects.
  // TODO(ikilpatrick): We may need a RAII checker to ensure that if we fail,
  // the children are put back into their initial state from before the custom
  // layout was run.

  // Attempt to run the custom layout, this may fail, and if so we'll have to
  // fall back onto regular block layout.
  bool success = PerformLayout(relayout_children, &layout_scope);

  if (!success) {
    LayoutBlockFlow::UpdateBlockLayout(relayout_children);
    DCHECK_EQ(fragment_result_data_, nullptr);
  }
}

bool LayoutCustom::PerformLayout(bool relayout_children,
                                 SubtreeLayoutScope* layout_scope) {
  LayoutCustomPhaseScope phase_scope(*this);

  // We clear the fragment result data, so that if we fallback to block layout,
  // we don't pass invalid data up the tree to a custom layout parent.
  fragment_result_data_ = nullptr;

  // We need to fallback to block layout if we don't have a registered
  // definition yet.
  if (state_ == kUnloaded)
    return false;

  UpdateLogicalWidth();
  LayoutUnit previous_height = LogicalHeight();

  {
    TextAutosizer::LayoutScope text_autosizer_layout_scope(this, layout_scope);
    LayoutState state(*this);
    ScriptForbiddenScope::AllowUserAgentScript allow_script;

    const AtomicString& name = StyleRef().DisplayLayoutCustomName();
    LayoutWorklet* worklet = LayoutWorklet::From(*GetDocument().domWindow());
    CSSLayoutDefinition* definition = worklet->Proxy()->FindDefinition(name);

    // TODO(ikilpatrick): Decide on a policy to refresh the layout instance.
    if (!instance_)
      instance_ = definition->CreateInstance();

    if (!instance_) {
      GetDocument().AddConsoleMessage(ConsoleMessage::Create(
          kJSMessageSource, mojom::ConsoleMessageLevel::kInfo,
          "Unable to create an instance of layout class '" + name +
              "', falling back to block layout."));
      return false;
    }

    FragmentResultOptions* fragment_result_options =
        FragmentResultOptions::Create();
    scoped_refptr<SerializedScriptValue> fragment_result_data;
    if (!instance_->Layout(*this, fragment_result_options,
                           &fragment_result_data))
      return false;

    size_t index = 0;
    const HeapVector<Member<CustomLayoutFragment>>& child_fragments =
        fragment_result_options->childFragments();
    for (LayoutBox* child = FirstChildBox(); child;
         child = child->NextSiblingBox()) {
      if (child->IsOutOfFlowPositioned()) {
        child->ContainingBlock()->InsertPositionedObject(child);

        PaintLayer* child_layer = child->Layer();
        child_layer->SetStaticInlinePosition(LayoutUnit(BorderStart()));
        child_layer->SetStaticBlockPosition(LayoutUnit(BorderBefore()));

        continue;
      }

      if (index >= child_fragments.size()) {
        GetDocument().AddConsoleMessage(ConsoleMessage::Create(
            kJSMessageSource, mojom::ConsoleMessageLevel::kInfo,
            "Chrome currently requires exactly one "
            "LayoutFragment per LayoutChild, "
            "falling back to block layout."));
        return false;
      }

      CustomLayoutFragment* fragment = child_fragments[index++];
      if (!fragment->IsValid()) {
        GetDocument().AddConsoleMessage(ConsoleMessage::Create(
            kJSMessageSource, mojom::ConsoleMessageLevel::kInfo,
            "An invalid LayoutFragment was returned from the "
            "layout, falling back to block layout."));
        return false;
      }

      // TODO(ikilpatrick): Implement paint order. This should abort this loop,
      // and go into a "slow" loop which allows developers to control the paint
      // order of the children.
      if (child != fragment->GetLayoutBox()) {
        return false;
      }

      // TODO(ikilpatrick): At this stage we may need to perform a re-layout on
      // the given child. (The LayoutFragment may have been produced from a
      // different LayoutFragmentRequest).

      // Update the position of the child.
      bool is_parallel_writing_mode = IsParallelWritingMode(
          StyleRef().GetWritingMode(), child->StyleRef().GetWritingMode());
      LayoutUnit inline_size = is_parallel_writing_mode
                                   ? child->LogicalWidth()
                                   : child->LogicalHeight();

      LayoutUnit inline_offset =
          LayoutUnit::FromDoubleRound(fragment->inlineOffset());
      LayoutUnit block_offset =
          LayoutUnit::FromDoubleRound(fragment->blockOffset());
      LayoutUnit logical_left =
          StyleRef().IsLeftToRightDirection()
              ? inline_offset
              : LogicalWidth() - inline_size - inline_offset;
      child->SetLocationAndUpdateOverflowControlsIfNeeded(
          IsHorizontalWritingMode() ? LayoutPoint(logical_left, block_offset)
                                    : LayoutPoint(block_offset, logical_left));
    }

    // Currently we only support exactly one LayoutFragment per LayoutChild.
    if (index != child_fragments.size()) {
      GetDocument().AddConsoleMessage(ConsoleMessage::Create(
          kJSMessageSource, mojom::ConsoleMessageLevel::kInfo,
          "Chrome currently requires exactly one "
          "LayoutFragment per LayoutChild, "
          "falling back to block layout."));
      return false;
    }

    // We aren't able to fallback to block layout now, it's safe to set the
    // result data.
    fragment_result_data_ = std::move(fragment_result_data);

    SetLogicalHeight(
        LayoutUnit::FromDoubleRound(fragment_result_options->autoBlockSize()));

    LayoutUnit old_client_after_edge = ClientLogicalBottom();
    UpdateLogicalHeight();

    if (LogicalHeight() != previous_height)
      relayout_children = true;

    LayoutPositionedObjects(relayout_children || IsDocumentElement());
    ComputeLayoutOverflow(old_client_after_edge);
  }

  UpdateAfterLayout();
  ClearNeedsLayout();

  return true;
}

}  // namespace blink
