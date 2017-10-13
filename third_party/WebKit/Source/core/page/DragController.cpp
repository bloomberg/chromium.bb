/*
 * Copyright (C) 2007, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/page/DragController.h"

#include <memory>

#include "bindings/core/v8/ExceptionState.h"
#include "build/build_config.h"
#include "core/clipboard/DataObject.h"
#include "core/clipboard/DataTransfer.h"
#include "core/clipboard/DataTransferAccessPolicy.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/Text.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/editing/DragCaret.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/commands/DragAndDropCommand.h"
#include "core/editing/serializers/Serialization.h"
#include "core/events/TextEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/forms/HTMLFormElement.h"
#include "core/html/forms/HTMLInputElement.h"
#include "core/input/EventHandler.h"
#include "core/input_type_names.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutImage.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/page/ChromeClient.h"
#include "core/page/DragData.h"
#include "core/page/DragSession.h"
#include "core/page/DragState.h"
#include "core/page/Page.h"
#include "core/svg/graphics/SVGImageForContainer.h"
#include "platform/DragImage.h"
#include "platform/SharedBuffer.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/graphics/paint/PaintRecordBuilder.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebDragData.h"
#include "public/platform/WebDragOperation.h"
#include "public/platform/WebImage.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebScreenInfo.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace blink {

static const int kMaxOriginalImageArea = 1500 * 1500;
static const int kLinkDragBorderInset = 2;
static const float kDragImageAlpha = 0.75f;

#if DCHECK_IS_ON()
static bool DragTypeIsValid(DragSourceAction action) {
  switch (action) {
    case kDragSourceActionDHTML:
    case kDragSourceActionImage:
    case kDragSourceActionLink:
    case kDragSourceActionSelection:
      return true;
    case kDragSourceActionNone:
      return false;
  }
  // Make sure MSVC doesn't complain that not all control paths return a value.
  NOTREACHED();
  return false;
}
#endif  // DCHECK_IS_ON()

static WebMouseEvent CreateMouseEvent(DragData* drag_data) {
  WebMouseEvent result(
      WebInputEvent::kMouseMove,
      WebFloatPoint(drag_data->ClientPosition().X(),
                    drag_data->ClientPosition().Y()),
      WebFloatPoint(drag_data->GlobalPosition().X(),
                    drag_data->GlobalPosition().Y()),
      WebPointerProperties::Button::kLeft, 0,
      static_cast<WebInputEvent::Modifiers>(drag_data->GetModifiers()),
      TimeTicks::Now().InSeconds());
  // TODO(dtapuska): Really we should chnage DragData to store the viewport
  // coordinates and scale.
  result.SetFrameScale(1);
  return result;
}

static DataTransfer* CreateDraggingDataTransfer(DataTransferAccessPolicy policy,
                                                DragData* drag_data) {
  return DataTransfer::Create(DataTransfer::kDragAndDrop, policy,
                              drag_data->PlatformData());
}

DragController::DragController(Page* page)
    : page_(page),
      document_under_mouse_(nullptr),
      drag_initiator_(nullptr),
      file_input_element_under_mouse_(nullptr),
      document_is_handling_drag_(false),
      drag_destination_action_(kDragDestinationActionNone),
      did_initiate_drag_(false) {}

DragController* DragController::Create(Page* page) {
  return new DragController(page);
}

static DocumentFragment* DocumentFragmentFromDragData(
    DragData* drag_data,
    LocalFrame* frame,
    Range* context,
    bool allow_plain_text,
    DragSourceType& drag_source_type) {
  DCHECK(drag_data);
  drag_source_type = DragSourceType::kHTMLSource;

  Document& document = context->OwnerDocument();
  if (drag_data->ContainsCompatibleContent()) {
    if (DocumentFragment* fragment = drag_data->AsFragment(frame))
      return fragment;

    if (drag_data->ContainsURL(DragData::kDoNotConvertFilenames)) {
      String title;
      String url = drag_data->AsURL(DragData::kDoNotConvertFilenames, &title);
      if (!url.IsEmpty()) {
        HTMLAnchorElement* anchor = HTMLAnchorElement::Create(document);
        anchor->SetHref(AtomicString(url));
        if (title.IsEmpty()) {
          // Try the plain text first because the url might be normalized or
          // escaped.
          if (drag_data->ContainsPlainText())
            title = drag_data->AsPlainText();
          if (title.IsEmpty())
            title = url;
        }
        Node* anchor_text = document.createTextNode(title);
        anchor->AppendChild(anchor_text);
        DocumentFragment* fragment = document.createDocumentFragment();
        fragment->AppendChild(anchor);
        return fragment;
      }
    }
  }
  if (allow_plain_text && drag_data->ContainsPlainText()) {
    drag_source_type = DragSourceType::kPlainTextSource;
    return CreateFragmentFromText(EphemeralRange(context),
                                  drag_data->AsPlainText());
  }

  return nullptr;
}

bool DragController::DragIsMove(FrameSelection& selection,
                                DragData* drag_data) {
  return document_under_mouse_ == drag_initiator_ &&
         selection.SelectionHasFocus() &&
         selection.ComputeVisibleSelectionInDOMTreeDeprecated()
             .IsContentEditable() &&
         selection.ComputeVisibleSelectionInDOMTreeDeprecated().IsRange() &&
         !IsCopyKeyDown(drag_data);
}

// FIXME: This method is poorly named.  We're just clearing the selection from
// the document this drag is exiting.
void DragController::CancelDrag() {
  page_->GetDragCaret().Clear();
}

void DragController::DragEnded() {
  drag_initiator_ = nullptr;
  did_initiate_drag_ = false;
  page_->GetDragCaret().Clear();
}

void DragController::DragExited(DragData* drag_data, LocalFrame& local_root) {
  DCHECK(drag_data);

  LocalFrameView* frame_view(local_root.View());
  if (frame_view) {
    DataTransferAccessPolicy policy = kDataTransferTypesReadable;
    DataTransfer* data_transfer = CreateDraggingDataTransfer(policy, drag_data);
    data_transfer->SetSourceOperation(drag_data->DraggingSourceOperationMask());
    local_root.GetEventHandler().CancelDragAndDrop(CreateMouseEvent(drag_data),
                                                   data_transfer);
    data_transfer->SetAccessPolicy(
        kDataTransferNumb);  // invalidate clipboard here for security
  }
  MouseMovedIntoDocument(nullptr);
  if (file_input_element_under_mouse_)
    file_input_element_under_mouse_->SetCanReceiveDroppedFiles(false);
  file_input_element_under_mouse_ = nullptr;
}

void DragController::PerformDrag(DragData* drag_data, LocalFrame& local_root) {
  DCHECK(drag_data);
  document_under_mouse_ =
      local_root.DocumentAtPoint(drag_data->ClientPosition());
  std::unique_ptr<UserGestureIndicator> gesture = LocalFrame::CreateUserGesture(
      document_under_mouse_ ? document_under_mouse_->GetFrame() : nullptr,
      UserGestureToken::kNewGesture);
  if ((drag_destination_action_ & kDragDestinationActionDHTML) &&
      document_is_handling_drag_) {
    bool prevented_default = false;
    if (local_root.View()) {
      // Sending an event can result in the destruction of the view and part.
      DataTransfer* data_transfer =
          CreateDraggingDataTransfer(kDataTransferReadable, drag_data);
      data_transfer->SetSourceOperation(
          drag_data->DraggingSourceOperationMask());
      EventHandler& event_handler = local_root.GetEventHandler();
      prevented_default = event_handler.PerformDragAndDrop(
                              CreateMouseEvent(drag_data), data_transfer) !=
                          WebInputEventResult::kNotHandled;
      if (!prevented_default) {
        // When drop target is plugin element and it can process drag, we
        // should prevent default behavior.
        const IntPoint point =
            local_root.View()->RootFrameToContents(drag_data->ClientPosition());
        const HitTestResult result = event_handler.HitTestResultAtPoint(point);
        prevented_default |=
            IsHTMLPlugInElement(*result.InnerNode()) &&
            ToHTMLPlugInElement(result.InnerNode())->CanProcessDrag();
      }

      // Invalidate clipboard here for security.
      data_transfer->SetAccessPolicy(kDataTransferNumb);
    }
    if (prevented_default) {
      document_under_mouse_ = nullptr;
      CancelDrag();
      return;
    }
  }

  if ((drag_destination_action_ & kDragDestinationActionEdit) &&
      ConcludeEditDrag(drag_data)) {
    document_under_mouse_ = nullptr;
    return;
  }

  document_under_mouse_ = nullptr;

  if (OperationForLoad(drag_data, local_root) != kDragOperationNone) {
    if (page_->GetSettings().GetNavigateOnDragDrop()) {
      page_->MainFrame()->Navigate(
          FrameLoadRequest(nullptr, ResourceRequest(drag_data->AsURL())));
    }

    // TODO(bokan): This case happens when we end a URL drag inside a guest
    // process which doesn't navigate. We assume that since we'll navigate the
    // page in the general case we don't end up sending `dragleave` and
    // `dragend` events but for plugins we wont navigate so it seems we should
    // be sending these events. crbug.com/748243.
    local_root.GetEventHandler().ClearDragState();
  }
}

void DragController::MouseMovedIntoDocument(Document* new_document) {
  if (document_under_mouse_ == new_document)
    return;

  // If we were over another document clear the selection
  if (document_under_mouse_)
    CancelDrag();
  document_under_mouse_ = new_document;
}

DragSession DragController::DragEnteredOrUpdated(DragData* drag_data,
                                                 LocalFrame& local_root) {
  DCHECK(drag_data);

  MouseMovedIntoDocument(
      local_root.DocumentAtPoint(drag_data->ClientPosition()));

  // TODO(esprehn): Replace acceptsLoadDrops with a Setting used in core.
  drag_destination_action_ =
      page_->GetChromeClient().AcceptsLoadDrops()
          ? kDragDestinationActionAny
          : static_cast<DragDestinationAction>(kDragDestinationActionDHTML |
                                               kDragDestinationActionEdit);

  DragSession drag_session;
  document_is_handling_drag_ = TryDocumentDrag(
      drag_data, drag_destination_action_, drag_session, local_root);
  if (!document_is_handling_drag_ &&
      (drag_destination_action_ & kDragDestinationActionLoad))
    drag_session.operation = OperationForLoad(drag_data, local_root);
  return drag_session;
}

static HTMLInputElement* AsFileInput(Node* node) {
  DCHECK(node);
  for (; node; node = node->OwnerShadowHost()) {
    if (IsHTMLInputElement(*node) &&
        ToHTMLInputElement(node)->type() == InputTypeNames::file)
      return ToHTMLInputElement(node);
  }
  return nullptr;
}

// This can return null if an empty document is loaded.
static Element* ElementUnderMouse(Document* document_under_mouse,
                                  const IntPoint& point) {
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestResult result(request, point);
  document_under_mouse->GetLayoutViewItem().HitTest(result);

  Node* n = result.InnerNode();
  while (n && !n->IsElementNode())
    n = n->ParentOrShadowHostNode();
  if (n && n->IsInShadowTree())
    n = n->OwnerShadowHost();

  return ToElement(n);
}

bool DragController::TryDocumentDrag(DragData* drag_data,
                                     DragDestinationAction action_mask,
                                     DragSession& drag_session,
                                     LocalFrame& local_root) {
  DCHECK(drag_data);

  if (!document_under_mouse_)
    return false;

  if (drag_initiator_ && !document_under_mouse_->GetSecurityOrigin()->CanAccess(
                             drag_initiator_->GetSecurityOrigin()))
    return false;

  bool is_handling_drag = false;
  if (action_mask & kDragDestinationActionDHTML) {
    is_handling_drag =
        TryDHTMLDrag(drag_data, drag_session.operation, local_root);
    // Do not continue if m_documentUnderMouse has been reset by tryDHTMLDrag.
    // tryDHTMLDrag fires dragenter event. The event listener that listens
    // to this event may create a nested run loop (open a modal dialog),
    // which could process dragleave event and reset m_documentUnderMouse in
    // dragExited.
    if (!document_under_mouse_)
      return false;
  }

  // It's unclear why this check is after tryDHTMLDrag.
  // We send drag events in tryDHTMLDrag and that may be the reason.
  LocalFrameView* frame_view = document_under_mouse_->View();
  if (!frame_view)
    return false;

  if (is_handling_drag) {
    page_->GetDragCaret().Clear();
    return true;
  }

  if ((action_mask & kDragDestinationActionEdit) &&
      CanProcessDrag(drag_data, local_root)) {
    IntPoint point =
        frame_view->RootFrameToContents(drag_data->ClientPosition());
    Element* element = ElementUnderMouse(document_under_mouse_.Get(), point);
    if (!element)
      return false;

    HTMLInputElement* element_as_file_input = AsFileInput(element);
    if (file_input_element_under_mouse_ != element_as_file_input) {
      if (file_input_element_under_mouse_)
        file_input_element_under_mouse_->SetCanReceiveDroppedFiles(false);
      file_input_element_under_mouse_ = element_as_file_input;
    }

    if (!file_input_element_under_mouse_) {
      page_->GetDragCaret().SetCaretPosition(
          document_under_mouse_->GetFrame()->PositionForPoint(point));
    }

    LocalFrame* inner_frame = element->GetDocument().GetFrame();
    drag_session.operation = DragIsMove(inner_frame->Selection(), drag_data)
                                 ? kDragOperationMove
                                 : kDragOperationCopy;
    drag_session.mouse_is_over_file_input = file_input_element_under_mouse_;
    drag_session.number_of_items_to_be_accepted = 0;

    const unsigned number_of_files = drag_data->NumberOfFiles();
    if (file_input_element_under_mouse_) {
      if (file_input_element_under_mouse_->IsDisabledFormControl())
        drag_session.number_of_items_to_be_accepted = 0;
      else if (file_input_element_under_mouse_->Multiple())
        drag_session.number_of_items_to_be_accepted = number_of_files;
      else if (number_of_files == 1)
        drag_session.number_of_items_to_be_accepted = 1;
      else
        drag_session.number_of_items_to_be_accepted = 0;

      if (!drag_session.number_of_items_to_be_accepted)
        drag_session.operation = kDragOperationNone;
      file_input_element_under_mouse_->SetCanReceiveDroppedFiles(
          drag_session.number_of_items_to_be_accepted);
    } else {
      // We are not over a file input element. The dragged item(s) will only
      // be loaded into the view the number of dragged items is 1.
      drag_session.number_of_items_to_be_accepted =
          number_of_files != 1 ? 0 : 1;
    }

    return true;
  }

  // We are not over an editable region. Make sure we're clearing any prior drag
  // cursor.
  page_->GetDragCaret().Clear();
  if (file_input_element_under_mouse_)
    file_input_element_under_mouse_->SetCanReceiveDroppedFiles(false);
  file_input_element_under_mouse_ = nullptr;
  return false;
}

DragOperation DragController::OperationForLoad(DragData* drag_data,
                                               LocalFrame& local_root) {
  DCHECK(drag_data);
  Document* doc = local_root.DocumentAtPoint(drag_data->ClientPosition());

  if (doc &&
      (did_initiate_drag_ || doc->IsPluginDocument() || HasEditableStyle(*doc)))
    return kDragOperationNone;
  return GetDragOperation(drag_data);
}

// Returns true if node at |point| is editable with populating |dragCaret| and
// |range|, otherwise returns false.
// TODO(yosin): We should return |VisibleSelection| rather than three values.
static bool SetSelectionToDragCaret(LocalFrame* frame,
                                    VisibleSelection& drag_caret,
                                    Range*& range,
                                    const IntPoint& point) {
  frame->Selection().SetSelection(drag_caret.AsSelection());
  if (frame->Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .IsNone()) {
    // TODO(editing-dev): The use of
    // updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited.  See http://crbug.com/590369 for more details.
    // |LocalFrame::positinForPoint()| requires clean layout.
    frame->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();
    const PositionWithAffinity& position = frame->PositionForPoint(point);
    if (!position.IsConnected())
      return false;

    frame->Selection().SetSelection(
        SelectionInDOMTree::Builder().Collapse(position).Build());
    drag_caret =
        frame->Selection().ComputeVisibleSelectionInDOMTreeDeprecated();
    range = CreateRange(drag_caret.ToNormalizedEphemeralRange());
  }
  return !frame->Selection()
              .ComputeVisibleSelectionInDOMTreeDeprecated()
              .IsNone() &&
         frame->Selection()
             .ComputeVisibleSelectionInDOMTreeDeprecated()
             .IsContentEditable();
}

DispatchEventResult DragController::DispatchTextInputEventFor(
    LocalFrame* inner_frame,
    DragData* drag_data) {
  // Layout should be clean due to a hit test performed in |elementUnderMouse|.
  DCHECK(!inner_frame->GetDocument()->NeedsLayoutTreeUpdate());
  DCHECK(page_->GetDragCaret().HasCaret());
  String text = page_->GetDragCaret().IsContentRichlyEditable()
                    ? ""
                    : drag_data->AsPlainText();
  const PositionWithAffinity& caret_position =
      page_->GetDragCaret().CaretPosition();
  DCHECK(caret_position.IsConnected()) << caret_position;
  Element* target =
      inner_frame->GetEditor().FindEventTargetFrom(CreateVisibleSelection(
          SelectionInDOMTree::Builder().Collapse(caret_position).Build()));
  return target->DispatchEvent(
      TextEvent::CreateForDrop(inner_frame->DomWindow(), text));
}

bool DragController::ConcludeEditDrag(DragData* drag_data) {
  DCHECK(drag_data);

  HTMLInputElement* file_input = file_input_element_under_mouse_;
  if (file_input_element_under_mouse_) {
    file_input_element_under_mouse_->SetCanReceiveDroppedFiles(false);
    file_input_element_under_mouse_ = nullptr;
  }

  if (!document_under_mouse_)
    return false;

  IntPoint point = document_under_mouse_->View()->RootFrameToContents(
      drag_data->ClientPosition());
  Element* element = ElementUnderMouse(document_under_mouse_.Get(), point);
  if (!element)
    return false;
  LocalFrame* inner_frame = element->ownerDocument()->GetFrame();
  DCHECK(inner_frame);

  if (page_->GetDragCaret().HasCaret() &&
      DispatchTextInputEventFor(inner_frame, drag_data) !=
          DispatchEventResult::kNotCanceled)
    return true;

  if (drag_data->ContainsFiles() && file_input) {
    // fileInput should be the element we hit tested for, unless it was made
    // display:none in a drop event handler.
    if (file_input->GetLayoutObject())
      DCHECK_EQ(file_input, element);
    if (file_input->IsDisabledFormControl())
      return false;

    return file_input->ReceiveDroppedFiles(drag_data);
  }

  // TODO(paulmeyer): Isn't |m_page->dragController()| the same as |this|?
  if (!page_->GetDragController().CanProcessDrag(
          drag_data, inner_frame->LocalFrameRoot())) {
    page_->GetDragCaret().Clear();
    return false;
  }

  if (page_->GetDragCaret().HasCaret()) {
    // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited.  See http://crbug.com/590369 for more details.
    page_->GetDragCaret()
        .CaretPosition()
        .GetPosition()
        .GetDocument()
        ->UpdateStyleAndLayoutIgnorePendingStylesheets();
  }

  const PositionWithAffinity& caret_position =
      page_->GetDragCaret().CaretPosition();
  if (!caret_position.IsConnected()) {
    // "editing/pasteboard/drop-text-events-sideeffect-crash.html" and
    // "editing/pasteboard/drop-text-events-sideeffect.html" reach here.
    page_->GetDragCaret().Clear();
    return false;
  }
  VisibleSelection drag_caret = CreateVisibleSelection(
      SelectionInDOMTree::Builder().Collapse(caret_position).Build());
  page_->GetDragCaret().Clear();
  // |innerFrame| can be removed by event handler called by
  // |dispatchTextInputEventFor()|.
  if (!inner_frame->Selection().IsAvailable()) {
    // "editing/pasteboard/drop-text-events-sideeffect-crash.html" reaches
    // here.
    return false;
  }
  Range* range = CreateRange(drag_caret.ToNormalizedEphemeralRange());
  Element* root_editable_element =
      inner_frame->Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .RootEditableElement();

  // For range to be null a WebKit client must have done something bad while
  // manually controlling drag behaviour
  if (!range)
    return false;
  ResourceFetcher* fetcher = range->OwnerDocument().Fetcher();
  ResourceCacheValidationSuppressor validation_suppressor(fetcher);

  // Start new Drag&Drop command group, invalidate previous command group.
  // Assume no other places is firing |DeleteByDrag| and |InsertFromDrop|.
  inner_frame->GetEditor().RegisterCommandGroup(
      DragAndDropCommand::Create(*inner_frame->GetDocument()));

  if (DragIsMove(inner_frame->Selection(), drag_data) ||
      IsRichlyEditablePosition(drag_caret.Base())) {
    DragSourceType drag_source_type = DragSourceType::kHTMLSource;
    DocumentFragment* fragment = DocumentFragmentFromDragData(
        drag_data, inner_frame, range, true, drag_source_type);
    if (!fragment)
      return false;

    if (DragIsMove(inner_frame->Selection(), drag_data)) {
      // NSTextView behavior is to always smart delete on moving a selection,
      // but only to smart insert if the selection granularity is word
      // granularity.
      const DeleteMode delete_mode =
          inner_frame->GetEditor().SmartInsertDeleteEnabled()
              ? DeleteMode::kSmart
              : DeleteMode::kSimple;
      const InsertMode insert_mode =
          (delete_mode == DeleteMode::kSmart &&
           inner_frame->Selection().Granularity() == TextGranularity::kWord &&
           drag_data->CanSmartReplace())
              ? InsertMode::kSmart
              : InsertMode::kSimple;

      if (!inner_frame->GetEditor().DeleteSelectionAfterDraggingWithEvents(
              inner_frame->GetEditor().FindEventTargetFromSelection(),
              delete_mode, drag_caret.Base()))
        return false;

      inner_frame->Selection().SetSelection(
          SelectionInDOMTree::Builder()
              .SetBaseAndExtent(EphemeralRange(range))
              .Build());
      if (inner_frame->Selection().IsAvailable()) {
        DCHECK(document_under_mouse_);
        if (!inner_frame->GetEditor().ReplaceSelectionAfterDraggingWithEvents(
                element, drag_data, fragment, range, insert_mode,
                drag_source_type))
          return false;
      }
    } else {
      if (SetSelectionToDragCaret(inner_frame, drag_caret, range, point)) {
        DCHECK(document_under_mouse_);
        if (!inner_frame->GetEditor().ReplaceSelectionAfterDraggingWithEvents(
                element, drag_data, fragment, range,
                drag_data->CanSmartReplace() ? InsertMode::kSmart
                                             : InsertMode::kSimple,
                drag_source_type))
          return false;
      }
    }
  } else {
    String text = drag_data->AsPlainText();
    if (text.IsEmpty())
      return false;

    if (SetSelectionToDragCaret(inner_frame, drag_caret, range, point)) {
      DCHECK(document_under_mouse_);
      if (!inner_frame->GetEditor().ReplaceSelectionAfterDraggingWithEvents(
              element, drag_data,
              CreateFragmentFromText(EphemeralRange(range), text), range,
              InsertMode::kSimple, DragSourceType::kPlainTextSource))
        return false;
    }
  }

  if (root_editable_element) {
    if (LocalFrame* frame = root_editable_element->GetDocument().GetFrame()) {
      frame->GetEventHandler().UpdateDragStateAfterEditDragIfNeeded(
          root_editable_element);
    }
  }

  return true;
}

bool DragController::CanProcessDrag(DragData* drag_data,
                                    LocalFrame& local_root) {
  DCHECK(drag_data);

  if (!drag_data->ContainsCompatibleContent())
    return false;

  if (local_root.ContentLayoutItem().IsNull())
    return false;

  IntPoint point =
      local_root.View()->RootFrameToContents(drag_data->ClientPosition());

  HitTestResult result =
      local_root.GetEventHandler().HitTestResultAtPoint(point);

  if (!result.InnerNode())
    return false;

  if (drag_data->ContainsFiles() && AsFileInput(result.InnerNode()))
    return true;

  if (IsHTMLPlugInElement(*result.InnerNode())) {
    HTMLPlugInElement* plugin = ToHTMLPlugInElement(result.InnerNode());
    if (!plugin->CanProcessDrag() && !HasEditableStyle(*result.InnerNode()))
      return false;
  } else if (!HasEditableStyle(*result.InnerNode())) {
    return false;
  }

  if (did_initiate_drag_ && document_under_mouse_ == drag_initiator_ &&
      result.IsSelected())
    return false;

  return true;
}

static DragOperation DefaultOperationForDrag(DragOperation src_op_mask) {
  // This is designed to match IE's operation fallback for the case where
  // the page calls preventDefault() in a drag event but doesn't set dropEffect.
  if (src_op_mask == kDragOperationEvery)
    return kDragOperationCopy;
  if (src_op_mask == kDragOperationNone)
    return kDragOperationNone;
  if (src_op_mask & kDragOperationMove)
    return kDragOperationMove;
  if (src_op_mask & kDragOperationCopy)
    return kDragOperationCopy;
  if (src_op_mask & kDragOperationLink)
    return kDragOperationLink;

  // FIXME: Does IE really return "generic" even if no operations were allowed
  // by the source?
  return kDragOperationGeneric;
}

bool DragController::TryDHTMLDrag(DragData* drag_data,
                                  DragOperation& operation,
                                  LocalFrame& local_root) {
  DCHECK(drag_data);
  DCHECK(document_under_mouse_);
  if (!local_root.View())
    return false;

  DataTransferAccessPolicy policy = kDataTransferTypesReadable;
  DataTransfer* data_transfer = CreateDraggingDataTransfer(policy, drag_data);
  DragOperation src_op_mask = drag_data->DraggingSourceOperationMask();
  data_transfer->SetSourceOperation(src_op_mask);

  WebMouseEvent event = CreateMouseEvent(drag_data);
  if (local_root.GetEventHandler().UpdateDragAndDrop(event, data_transfer) ==
      WebInputEventResult::kNotHandled) {
    data_transfer->SetAccessPolicy(
        kDataTransferNumb);  // invalidate clipboard here for security
    return false;
  }

  operation = data_transfer->DestinationOperation();
  if (data_transfer->DropEffectIsUninitialized()) {
    operation = DefaultOperationForDrag(src_op_mask);
  } else if (!(src_op_mask & operation)) {
    // The element picked an operation which is not supported by the source
    operation = kDragOperationNone;
  }

  data_transfer->SetAccessPolicy(
      kDataTransferNumb);  // invalidate clipboard here for security
  return true;
}

bool SelectTextInsteadOfDrag(const Node& node) {
  if (!node.IsTextNode())
    return false;

  // Editable elements loose their draggability,
  // see https://github.com/whatwg/html/issues/3114.
  if (HasEditableStyle(node))
    return true;

  for (Node& node : NodeTraversal::InclusiveAncestorsOf(node)) {
    if (node.IsHTMLElement() && ToHTMLElement(&node)->draggable())
      return false;
  }

  return node.CanStartSelection();
}

Node* DragController::DraggableNode(const LocalFrame* src,
                                    Node* start_node,
                                    const IntPoint& drag_origin,
                                    SelectionDragPolicy selection_drag_policy,
                                    DragSourceAction& drag_type) const {
  if (src->Selection().Contains(drag_origin)) {
    drag_type = kDragSourceActionSelection;
    if (selection_drag_policy == kImmediateSelectionDragResolution)
      return start_node;
  } else {
    drag_type = kDragSourceActionNone;
  }

  Node* node = nullptr;
  DragSourceAction candidate_drag_type = kDragSourceActionNone;
  for (const LayoutObject* layout_object = start_node->GetLayoutObject();
       layout_object; layout_object = layout_object->Parent()) {
    node = layout_object->NonPseudoNode();
    if (!node) {
      // Anonymous layout blocks don't correspond to actual DOM nodes, so we
      // skip over them for the purposes of finding a draggable node.
      continue;
    }
    if (drag_type != kDragSourceActionSelection &&
        SelectTextInsteadOfDrag(*node)) {
      // We have a click in an unselected, selectable text that is not
      // draggable... so we want to start the selection process instead
      // of looking for a parent to try to drag.
      return nullptr;
    }
    if (node->IsElementNode()) {
      EUserDrag drag_mode = layout_object->Style()->UserDrag();
      if (drag_mode == EUserDrag::kNone)
        continue;
      // Even if the image is part of a selection, we always only drag the image
      // in this case.
      if (layout_object->IsImage() && src->GetSettings() &&
          src->GetSettings()->GetLoadsImagesAutomatically()) {
        drag_type = kDragSourceActionImage;
        return node;
      }
      // Other draggable elements are considered unselectable.
      if (drag_mode == EUserDrag::kElement) {
        candidate_drag_type = kDragSourceActionDHTML;
        break;
      }
      if (IsHTMLAnchorElement(*node) &&
          ToHTMLAnchorElement(node)->IsLiveLink()) {
        candidate_drag_type = kDragSourceActionLink;
        break;
      }
    }
  }

  if (candidate_drag_type == kDragSourceActionNone) {
    // Either:
    // 1) Nothing under the cursor is considered draggable, so we bail out.
    // 2) There was a selection under the cursor but selectionDragPolicy is set
    //    to DelayedSelectionDragResolution and no other draggable element could
    //    be found, so bail out and allow text selection to start at the cursor
    //    instead.
    return nullptr;
  }

  DCHECK(node);
  if (drag_type == kDragSourceActionSelection) {
    // Dragging unselectable elements in a selection has special behavior if
    // selectionDragPolicy is DelayedSelectionDragResolution and this drag was
    // flagged as a potential selection drag. In that case, don't allow
    // selection and just drag the entire selection instead.
    DCHECK_EQ(selection_drag_policy, kDelayedSelectionDragResolution);
    node = start_node;
  } else {
    // If the cursor isn't over a selection, then just drag the node we found
    // earlier.
    DCHECK_EQ(drag_type, kDragSourceActionNone);
    drag_type = candidate_drag_type;
  }
  return node;
}

static ImageResourceContent* GetImageResource(Element* element) {
  DCHECK(element);
  LayoutObject* layout_object = element->GetLayoutObject();
  if (!layout_object || !layout_object->IsImage())
    return nullptr;
  LayoutImage* image = ToLayoutImage(layout_object);
  return image->CachedImage();
}

static Image* GetImage(Element* element) {
  DCHECK(element);
  ImageResourceContent* cached_image = GetImageResource(element);
  return (cached_image && !cached_image->ErrorOccurred())
             ? cached_image->GetImage()
             : nullptr;
}

static void PrepareDataTransferForImageDrag(LocalFrame* source,
                                            DataTransfer* data_transfer,
                                            Element* node,
                                            const KURL& link_url,
                                            const KURL& image_url,
                                            const String& label) {
  node->GetDocument().UpdateStyleAndLayoutTree();
  if (HasRichlyEditableStyle(*node)) {
    // TODO(editing-dev): We should use |EphemeralRange| instead of |Range|.
    Range* range = source->GetDocument()->createRange();
    range->selectNode(node, ASSERT_NO_EXCEPTION);
    source->Selection().SetSelection(
        SelectionInDOMTree::Builder()
            .SetBaseAndExtent(EphemeralRange(range))
            .Build());
  }
  data_transfer->DeclareAndWriteDragImage(node, link_url, image_url, label);
}

bool DragController::PopulateDragDataTransfer(LocalFrame* src,
                                              const DragState& state,
                                              const IntPoint& drag_origin) {
#if DCHECK_IS_ON()
  DCHECK(DragTypeIsValid(state.drag_type_));
#endif
  DCHECK(src);
  if (!src->View() || src->ContentLayoutItem().IsNull())
    return false;

  HitTestResult hit_test_result =
      src->GetEventHandler().HitTestResultAtPoint(drag_origin);
  // FIXME: Can this even happen? I guess it's possible, but should verify
  // with a layout test.
  if (!state.drag_src_->IsShadowIncludingInclusiveAncestorOf(
          hit_test_result.InnerNode())) {
    // The original node being dragged isn't under the drag origin anymore...
    // maybe it was hidden or moved out from under the cursor. Regardless, we
    // don't want to start a drag on something that's not actually under the
    // drag origin.
    return false;
  }
  const KURL& link_url = hit_test_result.AbsoluteLinkURL();
  const KURL& image_url = hit_test_result.AbsoluteImageURL();

  DataTransfer* data_transfer = state.drag_data_transfer_.Get();
  Node* node = state.drag_src_.Get();

  if (IsHTMLAnchorElement(*node) && ToHTMLAnchorElement(node)->IsLiveLink() &&
      !link_url.IsEmpty()) {
    // Simplify whitespace so the title put on the clipboard resembles what
    // the user sees on the web page. This includes replacing newlines with
    // spaces.
    data_transfer->WriteURL(node, link_url,
                            hit_test_result.TextContent().SimplifyWhiteSpace());
  }

  if (state.drag_type_ == kDragSourceActionSelection) {
    data_transfer->WriteSelection(src->Selection());
  } else if (state.drag_type_ == kDragSourceActionImage) {
    if (image_url.IsEmpty() || !node || !node->IsElementNode())
      return false;
    Element* element = ToElement(node);
    PrepareDataTransferForImageDrag(src, data_transfer, element, link_url,
                                    image_url,
                                    hit_test_result.AltDisplayString());
  } else if (state.drag_type_ == kDragSourceActionLink) {
    if (link_url.IsEmpty())
      return false;
  } else if (state.drag_type_ == kDragSourceActionDHTML) {
    LayoutObject* layout_object = node->GetLayoutObject();
    if (!layout_object) {
      // The layoutObject has disappeared, this can happen if the onStartDrag
      // handler has hidden the element in some way. In this case we just kill
      // the drag.
      return false;
    }

    IntRect bounding_including_descendants =
        layout_object->AbsoluteBoundingBoxRectIncludingDescendants();
    IntSize delta = drag_origin - bounding_including_descendants.Location();
    data_transfer->SetDragImageElement(node, IntPoint(delta));

    // FIXME: For DHTML/draggable element drags, write element markup to
    // clipboard.
  }
  return true;
}

static IntPoint DragLocationForDHTMLDrag(const IntPoint& mouse_dragged_point,
                                         const IntPoint& drag_origin,
                                         const IntPoint& drag_image_offset,
                                         bool is_link_image) {
  // dragImageOffset is the cursor position relative to the lower-left corner of
  // the image.
  const int y_offset = -drag_image_offset.Y();

  if (is_link_image) {
    return IntPoint(mouse_dragged_point.X() - drag_image_offset.X(),
                    mouse_dragged_point.Y() + y_offset);
  }

  return IntPoint(drag_origin.X() - drag_image_offset.X(),
                  drag_origin.Y() + y_offset);
}

FloatRect DragController::ClippedSelection(const LocalFrame& frame) {
  return DataTransfer::ClipByVisualViewport(
      FloatRect(frame.Selection().UnclippedBounds()), frame);
}

static IntPoint DragLocationForSelectionDrag(const LocalFrame& frame) {
  IntRect dragging_rect =
      EnclosingIntRect(DragController::ClippedSelection(frame));
  int xpos = dragging_rect.MaxX();
  xpos = dragging_rect.X() < xpos ? dragging_rect.X() : xpos;
  int ypos = dragging_rect.MaxY();
  ypos = dragging_rect.Y() < ypos ? dragging_rect.Y() : ypos;
  return IntPoint(xpos, ypos);
}

static const IntSize MaxDragImageSize(float device_scale_factor) {
#if defined(OS_MACOSX)
  // Match Safari's drag image size.
  static const IntSize kMaxDragImageSize(400, 400);
#else
  static const IntSize kMaxDragImageSize(200, 200);
#endif
  IntSize max_size_in_pixels = kMaxDragImageSize;
  max_size_in_pixels.Scale(device_scale_factor);
  return max_size_in_pixels;
}

static std::unique_ptr<DragImage> DragImageForImage(
    Element* element,
    Image* image,
    float device_scale_factor,
    const IntPoint& drag_origin,
    const IntPoint& image_element_location,
    const IntSize& image_element_size_in_pixels,
    IntPoint& drag_location) {
  std::unique_ptr<DragImage> drag_image;
  IntPoint origin;

  // Substitute an appropriately-sized SVGImageForContainer, to ensure dragged
  // SVG images scale seamlessly.
  RefPtr<SVGImageForContainer> svg_image;
  if (image->IsSVGImage()) {
    KURL url = element->GetDocument().CompleteURL(element->ImageSourceURL());
    svg_image = SVGImageForContainer::Create(
        ToSVGImage(image), image_element_size_in_pixels, 1, url);
    image = svg_image.get();
  }

  InterpolationQuality interpolation_quality =
      element->EnsureComputedStyle()->ImageRendering() ==
              EImageRendering::kPixelated
          ? kInterpolationNone
          : kInterpolationHigh;
  RespectImageOrientationEnum should_respect_image_orientation =
      LayoutObject::ShouldRespectImageOrientation(element->GetLayoutObject());
  ImageOrientation orientation;

  if (should_respect_image_orientation == kRespectImageOrientation &&
      image->IsBitmapImage())
    orientation = ToBitmapImage(image)->CurrentFrameOrientation();

  IntSize image_size = orientation.UsesWidthAsHeight()
                           ? image->Size().TransposedSize()
                           : image->Size();

  FloatSize image_scale =
      DragImage::ClampedImageScale(image_size, image_element_size_in_pixels,
                                   MaxDragImageSize(device_scale_factor));

  if (image_size.Area() <= kMaxOriginalImageArea &&
      (drag_image = DragImage::Create(
           image, should_respect_image_orientation, device_scale_factor,
           interpolation_quality, kDragImageAlpha, image_scale))) {
    IntSize original_size = image_element_size_in_pixels;
    origin = image_element_location;

    IntSize new_size = drag_image->Size();

    // Properly orient the drag image and orient it differently if it's smaller
    // than the original
    float scale = new_size.Width() / (float)original_size.Width();
    float dx = origin.X() - drag_origin.X();
    dx *= scale;
    origin.SetX((int)(dx + 0.5));
    float dy = origin.Y() - drag_origin.Y();
    dy *= scale;
    origin.SetY((int)(dy + 0.5));
  }

  drag_location = drag_origin + origin;
  return drag_image;
}

static std::unique_ptr<DragImage> DragImageForLink(const KURL& link_url,
                                                   const String& link_text,
                                                   float device_scale_factor) {
  FontDescription font_description;
  LayoutTheme::GetTheme().SystemFont(blink::CSSValueNone, font_description);
  return DragImage::Create(link_url, link_text, font_description,
                           device_scale_factor);
}

static IntPoint DragLocationForLink(const DragImage* link_image,
                                    const IntPoint& origin,
                                    float device_scale_factor,
                                    float page_scale_factor) {
  if (!link_image)
    return origin;

  // Offset the image so that the cursor is horizontally centered.
  FloatPoint image_offset(-link_image->Size().Width() / 2.f,
                          -kLinkDragBorderInset);
  // |origin| is in the coordinate space of the frame's contents whereas the
  // size of |link_image| is in physical pixels. Adjust the image offset to be
  // scaled in the frame's contents.
  // TODO(pdr): Unify this calculation with the DragImageForImage scaling code.
  float scale = 1.f / (device_scale_factor * page_scale_factor);
  image_offset.Scale(scale, scale);
  image_offset.MoveBy(origin);
  return RoundedIntPoint(image_offset);
}

// static
std::unique_ptr<DragImage> DragController::DragImageForSelection(
    const LocalFrame& frame,
    float opacity) {
  if (!frame.Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsRange())
    return nullptr;

  frame.View()->UpdateAllLifecyclePhasesExceptPaint();
  DCHECK(frame.GetDocument()->IsActive());

  FloatRect painting_rect = ClippedSelection(frame);
  GlobalPaintFlags paint_flags =
      kGlobalPaintSelectionOnly | kGlobalPaintFlattenCompositingLayers;

  PaintRecordBuilder builder(
      DataTransfer::DeviceSpaceRect(painting_rect, frame));
  frame.View()->PaintContents(builder.Context(), paint_flags,
                              EnclosingIntRect(painting_rect));
  return DataTransfer::CreateDragImageForFrame(
      frame, opacity, kDoNotRespectImageOrientation, painting_rect, builder,
      PropertyTreeState::Root());
}

bool DragController::StartDrag(LocalFrame* src,
                               const DragState& state,
                               const WebMouseEvent& drag_event,
                               const IntPoint& drag_origin) {
#if DCHECK_IS_ON()
  DCHECK(DragTypeIsValid(state.drag_type_));
#endif
  DCHECK(src);
  if (!src->View() || src->ContentLayoutItem().IsNull())
    return false;

  HitTestResult hit_test_result =
      src->GetEventHandler().HitTestResultAtPoint(drag_origin);
  if (!state.drag_src_->IsShadowIncludingInclusiveAncestorOf(
          hit_test_result.InnerNode())) {
    // The original node being dragged isn't under the drag origin anymore...
    // maybe it was hidden or moved out from under the cursor. Regardless, we
    // don't want to start a drag on something that's not actually under the
    // drag origin.
    return false;
  }
  const KURL& link_url = hit_test_result.AbsoluteLinkURL();
  const KURL& image_url = hit_test_result.AbsoluteImageURL();

  // TODO(pdr): This code shouldn't be necessary because drag_origin is already
  // in the coordinate space of the view's contents.
  IntPoint mouse_dragged_point = src->View()->RootFrameToContents(
      FlooredIntPoint(drag_event.PositionInRootFrame()));

  IntPoint drag_location;
  IntPoint drag_offset;

  DataTransfer* data_transfer = state.drag_data_transfer_.Get();
  // We allow DHTML/JS to set the drag image, even if its a link, image or text
  // we're dragging.  This is in the spirit of the IE API, which allows
  // overriding of pasteboard data and DragOp.
  std::unique_ptr<DragImage> drag_image =
      data_transfer->CreateDragImage(drag_offset, src);
  if (drag_image) {
    drag_location = DragLocationForDHTMLDrag(mouse_dragged_point, drag_origin,
                                             drag_offset, !link_url.IsEmpty());
  }

  Node* node = state.drag_src_.Get();
  if (state.drag_type_ == kDragSourceActionSelection) {
    if (!drag_image) {
      drag_image = DragImageForSelection(*src, kDragImageAlpha);
      drag_location = DragLocationForSelectionDrag(*src);
    }
    DoSystemDrag(drag_image.get(), drag_location, drag_origin, data_transfer,
                 src, false);
  } else if (state.drag_type_ == kDragSourceActionImage) {
    if (image_url.IsEmpty() || !node || !node->IsElementNode())
      return false;
    Element* element = ToElement(node);
    Image* image = GetImage(element);
    if (!image || image->IsNull() || !image->Data() || !image->Data()->size())
      return false;
    // We shouldn't be starting a drag for an image that can't provide an
    // extension.
    // This is an early detection for problems encountered later upon drop.
    DCHECK(!image->FilenameExtension().IsEmpty());
    if (!drag_image) {
      const IntRect& image_rect = hit_test_result.ImageRect();
      IntSize image_size_in_pixels = image_rect.Size();
      // TODO(oshima): Remove this scaling and simply pass imageRect to
      // dragImageForImage once all platforms are migrated to use zoom for dsf.
      image_size_in_pixels.Scale(src->GetPage()->DeviceScaleFactorDeprecated() *
                                 src->GetPage()->GetVisualViewport().Scale());

      float screen_device_scale_factor =
          src->GetPage()->GetChromeClient().GetScreenInfo().device_scale_factor;
      // Pass the selected image size in DIP becasue dragImageForImage clips the
      // image in DIP.  The coordinates of the locations are in Viewport
      // coordinates, and they're converted in the Blink client.
      // TODO(oshima): Currently, the dragged image on high DPI is scaled and
      // can be blurry because of this.  Consider to clip in the screen
      // coordinates to use high resolution image on high DPI screens.
      drag_image = DragImageForImage(element, image, screen_device_scale_factor,
                                     drag_origin, image_rect.Location(),
                                     image_size_in_pixels, drag_location);
    }
    DoSystemDrag(drag_image.get(), drag_location, drag_origin, data_transfer,
                 src, false);
  } else if (state.drag_type_ == kDragSourceActionLink) {
    if (link_url.IsEmpty())
      return false;
    if (src->Selection()
            .ComputeVisibleSelectionInDOMTreeDeprecated()
            .IsCaret() &&
        src->Selection()
            .ComputeVisibleSelectionInDOMTreeDeprecated()
            .IsContentEditable()) {
      // a user can initiate a drag on a link without having any text
      // selected.  In this case, we should expand the selection to
      // the enclosing anchor element
      if (Node* node = EnclosingAnchorElement(
              src->Selection()
                  .ComputeVisibleSelectionInDOMTreeDeprecated()
                  .Base())) {
        src->Selection().SetSelection(
            SelectionInDOMTree::Builder().SelectAllChildren(*node).Build());
      }
    }

    if (!drag_image) {
      DCHECK(src->GetPage());
      float screen_device_scale_factor =
          src->GetPage()->GetChromeClient().GetScreenInfo().device_scale_factor;
      drag_image = DragImageForLink(link_url, hit_test_result.TextContent(),
                                    screen_device_scale_factor);
      drag_location = DragLocationForLink(drag_image.get(), mouse_dragged_point,
                                          screen_device_scale_factor,
                                          src->GetPage()->PageScaleFactor());
    }
    DoSystemDrag(drag_image.get(), drag_location, mouse_dragged_point,
                 data_transfer, src, true);
  } else if (state.drag_type_ == kDragSourceActionDHTML) {
    DoSystemDrag(drag_image.get(), drag_location, drag_origin, data_transfer,
                 src, false);
  } else {
    NOTREACHED();
    return false;
  }

  return true;
}

// TODO(esprehn): forLink is dead code, what was it for?
void DragController::DoSystemDrag(DragImage* image,
                                  const IntPoint& drag_location,
                                  const IntPoint& event_pos,
                                  DataTransfer* data_transfer,
                                  LocalFrame* frame,
                                  bool for_link) {
  did_initiate_drag_ = true;
  drag_initiator_ = frame->GetDocument();

  // TODO(pdr): |drag_location| and |event_pos| should be passed in as
  // FloatPoints and we should calculate these adjusted values in floating
  // point to avoid unnecessary rounding.
  IntPoint adjusted_drag_location =
      frame->View()->ContentsToViewport(drag_location);
  IntPoint adjusted_event_pos = frame->View()->ContentsToViewport(event_pos);
  IntSize offset_size(adjusted_event_pos - adjusted_drag_location);
  WebPoint offset_point(offset_size.Width(), offset_size.Height());
  WebDragData drag_data = data_transfer->GetDataObject()->ToWebDragData();
  WebDragOperationsMask drag_operation_mask =
      static_cast<WebDragOperationsMask>(data_transfer->SourceOperation());
  WebImage drag_image;

  if (image) {
    float resolution_scale = image->ResolutionScale();
    float device_scale_factor =
        page_->GetChromeClient().GetScreenInfo().device_scale_factor;
    if (device_scale_factor != resolution_scale) {
      DCHECK_GT(resolution_scale, 0);
      float scale = device_scale_factor / resolution_scale;
      image->Scale(scale, scale);
    }
    drag_image = image->Bitmap();
  }

  page_->GetChromeClient().StartDragging(frame, drag_data, drag_operation_mask,
                                         drag_image, offset_point);
}

DragOperation DragController::GetDragOperation(DragData* drag_data) {
  // FIXME: To match the MacOS behaviour we should return DragOperationNone
  // if we are a modal window, we are the drag source, or the window is an
  // attached sheet If this can be determined from within WebCore
  // operationForDrag can be pulled into WebCore itself
  DCHECK(drag_data);
  return drag_data->ContainsURL() && !did_initiate_drag_ ? kDragOperationCopy
                                                         : kDragOperationNone;
}

bool DragController::IsCopyKeyDown(DragData* drag_data) {
  int modifiers = drag_data->GetModifiers();

#if defined(OS_MACOSX)
  return modifiers & WebInputEvent::kAltKey;
#else
  return modifiers & WebInputEvent::kControlKey;
#endif
}

DragState& DragController::GetDragState() {
  if (!drag_state_)
    drag_state_ = new DragState;
  return *drag_state_;
}

DEFINE_TRACE(DragController) {
  visitor->Trace(page_);
  visitor->Trace(document_under_mouse_);
  visitor->Trace(drag_initiator_);
  visitor->Trace(drag_state_);
  visitor->Trace(file_input_element_under_mouse_);
}

}  // namespace blink
