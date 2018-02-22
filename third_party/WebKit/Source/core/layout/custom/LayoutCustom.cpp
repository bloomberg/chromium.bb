// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/custom/LayoutCustom.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/layout/LayoutState.h"
#include "core/layout/TextAutosizer.h"
#include "core/layout/custom/FragmentResultOptions.h"
#include "core/layout/custom/LayoutWorklet.h"
#include "core/layout/custom/LayoutWorkletGlobalScope.h"
#include "core/layout/custom/LayoutWorkletGlobalScopeProxy.h"
#include "platform/bindings/ScriptForbiddenScope.h"

namespace blink {

LayoutCustom::LayoutCustom(Element* element) : LayoutBlockFlow(element) {
  DCHECK(element);
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

    // TODO(ikilpatrick): We'll want to store the instance on either the
    // CSSLayoutDefinition or on the LayoutCustom object.
    CSSLayoutDefinition::Instance* instance = definition->CreateInstance();

    if (!instance) {
      GetDocument().AddConsoleMessage(ConsoleMessage::Create(
          kJSMessageSource, kInfoMessageLevel,
          "Unable to create an instance of layout class '" + name +
              "', falling back to block layout."));
      return false;
    }

    FragmentResultOptions fragment_result_options;
    if (!instance->Layout(*this, &fragment_result_options))
      return false;

    // TODO(ikilpatrick): Currently we need to "fail" if we have any children
    // as they won't be laid out yet.
    if (FirstChild())
      return false;

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
