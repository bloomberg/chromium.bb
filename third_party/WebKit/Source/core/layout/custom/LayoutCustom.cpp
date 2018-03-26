// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/custom/LayoutCustom.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/layout/LayoutState.h"
#include "core/layout/TextAutosizer.h"
#include "core/layout/custom/CustomLayoutFragment.h"
#include "core/layout/custom/FragmentResultOptions.h"
#include "core/layout/custom/LayoutWorklet.h"
#include "core/layout/custom/LayoutWorkletGlobalScope.h"
#include "core/layout/custom/LayoutWorkletGlobalScopeProxy.h"
#include "core/paint/PaintLayer.h"
#include "platform/bindings/ScriptForbiddenScope.h"

namespace blink {

LayoutCustom::LayoutCustom(Element* element) : LayoutBlockFlow(element) {
  DCHECK(element);
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

  LayoutBlockFlow::StyleDidChange(diff, old_style);

  LayoutWorklet* worklet = LayoutWorklet::From(*GetDocument().domWindow());
  const AtomicString& name = StyleRef().DisplayLayoutCustomName();
  state_ =
      worklet->GetDocumentDefinitionMap()->Contains(name) ? kBlock : kUnloaded;

  // Register if we'll need to reattach the layout tree when a matching
  // "layout()" is registered.
  if (state_ == kUnloaded) {
    worklet->AddPendingLayout(name, GetNode());
  } else {
    SetChildrenInline(false);
  }
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

  if (!success)
    LayoutBlockFlow::UpdateBlockLayout(relayout_children);
}

bool LayoutCustom::PerformLayout(bool relayout_children,
                                 SubtreeLayoutScope* layout_scope) {
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
          kJSMessageSource, kInfoMessageLevel,
          "Unable to create an instance of layout class '" + name +
              "', falling back to block layout."));
      return false;
    }

    FragmentResultOptions fragment_result_options;
    if (!instance_->Layout(*this, &fragment_result_options))
      return false;

    size_t index = 0;
    const HeapVector<Member<CustomLayoutFragment>>& child_fragments =
        fragment_result_options.childFragments();
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
        GetDocument().AddConsoleMessage(
            ConsoleMessage::Create(kJSMessageSource, kInfoMessageLevel,
                                   "Chrome currently requires exactly one "
                                   "LayoutFragment per LayoutChild, "
                                   "falling back to block layout."));
        return false;
      }

      CustomLayoutFragment* fragment = child_fragments[index++];
      if (!fragment->IsValid()) {
        GetDocument().AddConsoleMessage(ConsoleMessage::Create(
            kJSMessageSource, kInfoMessageLevel,
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
    }

    // Currently we only support
    if (index != child_fragments.size()) {
      GetDocument().AddConsoleMessage(
          ConsoleMessage::Create(kJSMessageSource, kInfoMessageLevel,
                                 "Chrome currently requires exactly one "
                                 "LayoutFragment per LayoutChild, "
                                 "falling back to block layout."));
      return false;
    }

    SetLogicalHeight(LayoutUnit(fragment_result_options.autoBlockSize()));

    LayoutUnit old_client_after_edge = ClientLogicalBottom();
    UpdateLogicalHeight();

    if (LogicalHeight() != previous_height)
      relayout_children = true;

    LayoutPositionedObjects(relayout_children || IsDocumentElement());
    ComputeOverflow(old_client_after_edge);
  }

  UpdateAfterLayout();
  ClearNeedsLayout();

  return true;
}

}  // namespace blink
