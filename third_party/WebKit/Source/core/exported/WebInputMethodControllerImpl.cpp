// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/exported/WebInputMethodControllerImpl.h"

#include "core/dom/Document.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/PlainTextRange.h"
#include "core/editing/ime/ImeTextSpanVectorBuilder.h"
#include "core/editing/ime/InputMethodController.h"
#include "core/exported/WebPluginContainerImpl.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/input_type_names.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "public/platform/WebString.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebRange.h"

namespace blink {

WebInputMethodControllerImpl::WebInputMethodControllerImpl(
    WebLocalFrameImpl& web_frame)
    : web_frame_(&web_frame) {}

WebInputMethodControllerImpl::~WebInputMethodControllerImpl() {}

void WebInputMethodControllerImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(web_frame_);
}

bool WebInputMethodControllerImpl::SetComposition(
    const WebString& text,
    const WebVector<WebImeTextSpan>& ime_text_spans,
    const WebRange& replacement_range,
    int selection_start,
    int selection_end) {
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

  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      LocalFrame::CreateUserGesture(GetFrame(), UserGestureToken::kNewGesture);

  GetInputMethodController().SetComposition(
      String(text), ImeTextSpanVectorBuilder::Build(ime_text_spans),
      selection_start, selection_end);

  return text.IsEmpty() || GetInputMethodController().HasComposition();
}

bool WebInputMethodControllerImpl::FinishComposingText(
    ConfirmCompositionBehavior selection_behavior) {
  // TODO(ekaramad): Here and in other IME calls we should expect the
  // call to be made when our frame is focused. This, however, is not the case
  // all the time. For instance, resetInputMethod call on RenderViewImpl could
  // be after losing the focus on frame. But since we return the core frame
  // in WebViewImpl::focusedLocalFrameInWidget(), we will reach here with
  // |m_webLocalFrame| not focused on page.

  if (WebPlugin* plugin = FocusedPluginIfInputMethodSupported())
    return plugin->FinishComposingText(selection_behavior);

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

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
  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      LocalFrame::CreateUserGesture(GetFrame(), UserGestureToken::kNewGesture);

  if (WebPlugin* plugin = FocusedPluginIfInputMethodSupported()) {
    return plugin->CommitText(text, ime_text_spans, replacement_range,
                              relative_caret_position);
  }

  // Select the range to be replaced with the composition later.
  if (!replacement_range.IsNull()) {
    web_frame_->SelectRange(replacement_range,
                            WebLocalFrame::kHideSelectionHandle,
                            blink::mojom::SelectionMenuBehavior::kHide);
  }

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  return GetInputMethodController().CommitText(
      text, ImeTextSpanVectorBuilder::Build(ime_text_spans),
      relative_caret_position);
}

WebTextInputInfo WebInputMethodControllerImpl::TextInputInfo() {
  return GetFrame()->GetInputMethodController().TextInputInfo();
}

int WebInputMethodControllerImpl::ComputeWebTextInputNextPreviousFlags() {
  return GetFrame()
      ->GetInputMethodController()
      .ComputeWebTextInputNextPreviousFlags();
}

WebTextInputType WebInputMethodControllerImpl::TextInputType() {
  return GetFrame()->GetInputMethodController().TextInputType();
}

WebRange WebInputMethodControllerImpl::GetSelectionOffsets() const {
  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  return GetFrame()->GetInputMethodController().GetSelectionOffsets();
}

LocalFrame* WebInputMethodControllerImpl::GetFrame() const {
  return web_frame_->GetFrame();
}

InputMethodController& WebInputMethodControllerImpl::GetInputMethodController()
    const {
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
