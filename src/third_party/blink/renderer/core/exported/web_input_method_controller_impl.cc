// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/exported/web_input_method_controller_impl.h"

#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/edit_context.h"
#include "third_party/blink/renderer/core/editing/ime/ime_text_span_vector_builder.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

WebInputMethodControllerImpl::WebInputMethodControllerImpl(
    WebLocalFrameImpl& web_frame)
    : web_frame_(&web_frame) {}

WebInputMethodControllerImpl::~WebInputMethodControllerImpl() = default;

void WebInputMethodControllerImpl::Trace(Visitor* visitor) {
  visitor->Trace(web_frame_);
}

bool WebInputMethodControllerImpl::IsEditContextActive() const {
  return GetInputMethodController().GetActiveEditContext();
}

bool WebInputMethodControllerImpl::SetComposition(
    const WebString& text,
    const WebVector<WebImeTextSpan>& ime_text_spans,
    const WebRange& replacement_range,
    int selection_start,
    int selection_end) {
  if (IsEditContextActive()) {
    return GetInputMethodController().GetActiveEditContext()->SetComposition(
        text, ime_text_spans, replacement_range, selection_start,
        selection_end);
  }

  if (WebPlugin* plugin = FocusedPluginIfInputMethodSupported()) {
    return plugin->SetComposition(text, ime_text_spans, replacement_range,
                                  selection_start, selection_end);
  }

  // We should use this |editor| object only to complete the ongoing
  // composition.
  if (!GetFrame()->GetEditor().CanEdit() &&
      !GetInputMethodController().HasComposition())
    return false;

  // Select the range to be replaced with the composition later.
  if (!replacement_range.IsNull()) {
    web_frame_->SelectRange(replacement_range,
                            WebLocalFrame::kHideSelectionHandle,
                            blink::mojom::SelectionMenuBehavior::kHide);
  }

  // We should verify the parent node of this IME composition node are
  // editable because JavaScript may delete a parent node of the composition
  // node. In this case, WebKit crashes while deleting texts from the parent
  // node, which doesn't exist any longer.
  const EphemeralRange range =
      GetInputMethodController().CompositionEphemeralRange();
  if (range.IsNotNull()) {
    Node* node = range.StartPosition().ComputeContainerNode();
    GetFrame()->GetDocument()->UpdateStyleAndLayoutTree();
    if (!node || !HasEditableStyle(*node))
      return false;
  }

  LocalFrame::NotifyUserActivation(GetFrame());

  GetInputMethodController().SetComposition(
      String(text), ImeTextSpanVectorBuilder::Build(ime_text_spans),
      selection_start, selection_end);

  return text.IsEmpty() ||
         (GetFrame() && GetInputMethodController().HasComposition());
}

bool WebInputMethodControllerImpl::FinishComposingText(
    ConfirmCompositionBehavior selection_behavior) {
  // TODO(ekaramad): Here and in other IME calls we should expect the
  // call to be made when our frame is focused. This, however, is not the case
  // all the time. For instance, resetInputMethod call on RenderViewImpl could
  // be after losing the focus on frame. But since we return the core frame
  // in WebViewImpl::focusedLocalFrameInWidget(), we will reach here with
  // |web_frame_| not focused on page.

  if (IsEditContextActive()) {
    return GetInputMethodController()
        .GetActiveEditContext()
        ->FinishComposingText(selection_behavior);
  }

  if (WebPlugin* plugin = FocusedPluginIfInputMethodSupported())
    return plugin->FinishComposingText(selection_behavior);

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kInput);

  return GetInputMethodController().FinishComposingText(
      selection_behavior == WebInputMethodController::kKeepSelection
          ? InputMethodController::kKeepSelection
          : InputMethodController::kDoNotKeepSelection);
}

bool WebInputMethodControllerImpl::CommitText(
    const WebString& text,
    const WebVector<WebImeTextSpan>& ime_text_spans,
    const WebRange& replacement_range,
    int relative_caret_position) {
  LocalFrame::NotifyUserActivation(GetFrame());

  if (IsEditContextActive()) {
    return GetInputMethodController().GetActiveEditContext()->CommitText(
        text, ime_text_spans, replacement_range, relative_caret_position);
  }

  if (WebPlugin* plugin = FocusedPluginIfInputMethodSupported()) {
    return plugin->CommitText(text, ime_text_spans, replacement_range,
                              relative_caret_position);
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kInput);

  if (!replacement_range.IsNull()) {
    return GetInputMethodController().ReplaceText(
        text, PlainTextRange(replacement_range.StartOffset(),
                             replacement_range.EndOffset()));
  }

  return GetInputMethodController().CommitText(
      text, ImeTextSpanVectorBuilder::Build(ime_text_spans),
      relative_caret_position);
}

WebTextInputInfo WebInputMethodControllerImpl::TextInputInfo() {
  if (IsEditContextActive())
    return GetInputMethodController().GetActiveEditContext()->TextInputInfo();

  return GetFrame()->GetInputMethodController().TextInputInfo();
}

int WebInputMethodControllerImpl::ComputeWebTextInputNextPreviousFlags() {
  return GetFrame()
      ->GetInputMethodController()
      .ComputeWebTextInputNextPreviousFlags();
}

WebTextInputType WebInputMethodControllerImpl::TextInputType() {
  if (IsEditContextActive())
    return GetInputMethodController().GetActiveEditContext()->TextInputType();

  return GetFrame()->GetInputMethodController().TextInputType();
}

void WebInputMethodControllerImpl::GetLayoutBounds(WebRect* control_bounds,
                                                   WebRect* selection_bounds) {
  GetInputMethodController().GetLayoutBounds(control_bounds, selection_bounds);
}

bool WebInputMethodControllerImpl::IsInputPanelPolicyManual() const {
  if (IsEditContextActive()) {
    return GetInputMethodController()
        .GetActiveEditContext()
        ->IsInputPanelPolicyManual();
  }
  return false;  // Default should always be automatic.
}

WebRange WebInputMethodControllerImpl::CompositionRange() {
  if (IsEditContextActive()) {
    return GetInputMethodController()
        .GetActiveEditContext()
        ->CompositionRange();
  }

  EphemeralRange range =
      GetFrame()->GetInputMethodController().CompositionEphemeralRange();

  if (range.IsNull())
    return WebRange();

  Element* editable =
      GetFrame()->Selection().RootEditableElementOrDocumentElement();

  editable->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kInput);

  return PlainTextRange::Create(*editable, range);
}

bool WebInputMethodControllerImpl::GetCompositionCharacterBounds(
    WebVector<WebRect>& bounds) {
  if (IsEditContextActive())
    return false;

  WebRange range = CompositionRange();
  if (range.IsEmpty())
    return false;

  size_t character_count = range.length();
  size_t offset = range.StartOffset();
  WebVector<WebRect> result(character_count);
  WebRect webrect;
  for (size_t i = 0; i < character_count; ++i) {
    if (!web_frame_->FirstRectForCharacterRange(offset + i, 1, webrect)) {
      DLOG(ERROR) << "Could not retrieve character rectangle at " << i;
      return false;
    }
    result[i] = webrect;
  }

  bounds.Swap(result);
  return true;
}

WebRange WebInputMethodControllerImpl::GetSelectionOffsets() const {
  if (IsEditContextActive()) {
    return GetInputMethodController()
        .GetActiveEditContext()
        ->GetSelectionOffsets();
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kInput);

  return GetFrame()->GetInputMethodController().GetSelectionOffsets();
}

LocalFrame* WebInputMethodControllerImpl::GetFrame() const {
  return web_frame_->GetFrame();
}

InputMethodController& WebInputMethodControllerImpl::GetInputMethodController()
    const {
  DCHECK(GetFrame());
  return GetFrame()->GetInputMethodController();
}

WebPlugin* WebInputMethodControllerImpl::FocusedPluginIfInputMethodSupported()
    const {
  WebPluginContainerImpl* container = GetFrame()->GetWebPluginContainer();
  if (container && container->SupportsInputMethod()) {
    return container->Plugin();
  }
  return nullptr;
}

}  // namespace blink
