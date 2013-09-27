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

#include "config.h"
#include "core/page/DragController.h"

#include "HTMLNames.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Clipboard.h"
#include "core/dom/ClipboardAccessPolicy.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Text.h"
#include "core/events/TextEvent.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/MoveSelectionCommand.h"
#include "core/editing/ReplaceSelectionCommand.h"
#include "core/editing/htmlediting.h"
#include "core/editing/markup.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/page/DragActions.h"
#include "core/page/DragClient.h"
#include "core/page/DragSession.h"
#include "core/page/DragState.h"
#include "core/page/EventHandler.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/platform/DragData.h"
#include "core/platform/DragImage.h"
#include "core/platform/chromium/ChromiumDataObject.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/graphics/Image.h"
#include "core/platform/graphics/ImageOrientation.h"
#include "core/platform/network/ResourceRequest.h"
#include "core/rendering/HitTestRequest.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/CurrentTime.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

#if OS(WIN)
#include <windows.h>
#endif

namespace WebCore {

const int DragController::DragIconRightInset = 7;
const int DragController::DragIconBottomInset = 3;

static const int MaxOriginalImageArea = 1500 * 1500;
static const int LinkDragBorderInset = 2;
static const float DragImageAlpha = 0.75f;

static bool dragTypeIsValid(DragSourceAction action)
{
    switch (action) {
    case DragSourceActionDHTML:
    case DragSourceActionImage:
    case DragSourceActionLink:
    case DragSourceActionSelection:
        return true;
    case DragSourceActionNone:
        return false;
    }
    // Make sure MSVC doesn't complain that not all control paths return a value.
    return false;
}

static PlatformMouseEvent createMouseEvent(DragData* dragData)
{
    bool shiftKey, ctrlKey, altKey, metaKey;
    shiftKey = ctrlKey = altKey = metaKey = false;
    int keyState = dragData->modifierKeyState();
    shiftKey = static_cast<bool>(keyState & PlatformEvent::ShiftKey);
    ctrlKey = static_cast<bool>(keyState & PlatformEvent::CtrlKey);
    altKey = static_cast<bool>(keyState & PlatformEvent::AltKey);
    metaKey = static_cast<bool>(keyState & PlatformEvent::MetaKey);

    return PlatformMouseEvent(dragData->clientPosition(), dragData->globalPosition(),
                              LeftButton, PlatformEvent::MouseMoved, 0, shiftKey, ctrlKey, altKey,
                              metaKey, currentTime());
}

static PassRefPtr<Clipboard> createDraggingClipboard(ClipboardAccessPolicy policy, DragData* dragData)
{
    return Clipboard::create(Clipboard::DragAndDrop, policy, dragData->platformData());
}

DragController::DragController(Page* page, DragClient* client)
    : m_page(page)
    , m_client(client)
    , m_documentUnderMouse(0)
    , m_dragInitiator(0)
    , m_fileInputElementUnderMouse(0)
    , m_documentIsHandlingDrag(false)
    , m_dragDestinationAction(DragDestinationActionNone)
    , m_didInitiateDrag(false)
{
    ASSERT(m_client);
}

DragController::~DragController()
{
}

PassOwnPtr<DragController> DragController::create(Page* page, DragClient* client)
{
    return adoptPtr(new DragController(page, client));
}

static PassRefPtr<DocumentFragment> documentFragmentFromDragData(DragData* dragData, Frame* frame, RefPtr<Range> context,
                                          bool allowPlainText, bool& chosePlainText)
{
    ASSERT(dragData);
    chosePlainText = false;

    Document& document = context->ownerDocument();
    if (dragData->containsCompatibleContent()) {
        if (PassRefPtr<DocumentFragment> fragment = dragData->asFragment(frame, context, allowPlainText, chosePlainText))
            return fragment;

        if (dragData->containsURL(frame, DragData::DoNotConvertFilenames)) {
            String title;
            String url = dragData->asURL(frame, DragData::DoNotConvertFilenames, &title);
            if (!url.isEmpty()) {
                RefPtr<HTMLAnchorElement> anchor = HTMLAnchorElement::create(document);
                anchor->setHref(url);
                if (title.isEmpty()) {
                    // Try the plain text first because the url might be normalized or escaped.
                    if (dragData->containsPlainText())
                        title = dragData->asPlainText(frame);
                    if (title.isEmpty())
                        title = url;
                }
                RefPtr<Node> anchorText = document.createTextNode(title);
                anchor->appendChild(anchorText);
                RefPtr<DocumentFragment> fragment = document.createDocumentFragment();
                fragment->appendChild(anchor);
                return fragment.release();
            }
        }
    }
    if (allowPlainText && dragData->containsPlainText()) {
        chosePlainText = true;
        return createFragmentFromText(context.get(), dragData->asPlainText(frame)).get();
    }

    return 0;
}

bool DragController::dragIsMove(FrameSelection& selection, DragData* dragData)
{
    return m_documentUnderMouse == m_dragInitiator && selection.isContentEditable() && selection.isRange() && !isCopyKeyDown(dragData);
}

// FIXME: This method is poorly named.  We're just clearing the selection from the document this drag is exiting.
void DragController::cancelDrag()
{
    m_page->dragCaretController().clear();
}

void DragController::dragEnded()
{
    m_dragInitiator = 0;
    m_didInitiateDrag = false;
    m_page->dragCaretController().clear();
}

DragSession DragController::dragEntered(DragData* dragData)
{
    return dragEnteredOrUpdated(dragData);
}

void DragController::dragExited(DragData* dragData)
{
    ASSERT(dragData);
    Frame* mainFrame = m_page->mainFrame();

    if (RefPtr<FrameView> v = mainFrame->view()) {
        ClipboardAccessPolicy policy = (!m_documentUnderMouse || m_documentUnderMouse->securityOrigin()->isLocal()) ? ClipboardReadable : ClipboardTypesReadable;
        RefPtr<Clipboard> clipboard = createDraggingClipboard(policy, dragData);
        clipboard->setSourceOperation(dragData->draggingSourceOperationMask());
        mainFrame->eventHandler()->cancelDragAndDrop(createMouseEvent(dragData), clipboard.get());
        clipboard->setAccessPolicy(ClipboardNumb);    // invalidate clipboard here for security
    }
    mouseMovedIntoDocument(0);
    if (m_fileInputElementUnderMouse)
        m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
    m_fileInputElementUnderMouse = 0;
}

DragSession DragController::dragUpdated(DragData* dragData)
{
    return dragEnteredOrUpdated(dragData);
}

bool DragController::performDrag(DragData* dragData)
{
    ASSERT(dragData);
    m_documentUnderMouse = m_page->mainFrame()->documentAtPoint(dragData->clientPosition());
    if ((m_dragDestinationAction & DragDestinationActionDHTML) && m_documentIsHandlingDrag) {
        RefPtr<Frame> mainFrame = m_page->mainFrame();
        bool preventedDefault = false;
        if (mainFrame->view()) {
            // Sending an event can result in the destruction of the view and part.
            RefPtr<Clipboard> clipboard = createDraggingClipboard(ClipboardReadable, dragData);
            clipboard->setSourceOperation(dragData->draggingSourceOperationMask());
            preventedDefault = mainFrame->eventHandler()->performDragAndDrop(createMouseEvent(dragData), clipboard.get());
            clipboard->setAccessPolicy(ClipboardNumb); // Invalidate clipboard here for security
        }
        if (preventedDefault) {
            m_documentUnderMouse = 0;
            return true;
        }
    }

    if ((m_dragDestinationAction & DragDestinationActionEdit) && concludeEditDrag(dragData)) {
        m_documentUnderMouse = 0;
        return true;
    }

    m_documentUnderMouse = 0;

    if (operationForLoad(dragData) == DragOperationNone)
        return false;

    m_page->mainFrame()->loader()->load(FrameLoadRequest(0, ResourceRequest(dragData->asURL(m_page->mainFrame()))));
    return true;
}

void DragController::mouseMovedIntoDocument(Document* newDocument)
{
    if (m_documentUnderMouse == newDocument)
        return;

    // If we were over another document clear the selection
    if (m_documentUnderMouse)
        cancelDrag();
    m_documentUnderMouse = newDocument;
}

DragSession DragController::dragEnteredOrUpdated(DragData* dragData)
{
    ASSERT(dragData);
    ASSERT(m_page->mainFrame());
    mouseMovedIntoDocument(m_page->mainFrame()->documentAtPoint(dragData->clientPosition()));

    m_dragDestinationAction = m_client->actionMaskForDrag(dragData);
    if (m_dragDestinationAction == DragDestinationActionNone) {
        cancelDrag(); // FIXME: Why not call mouseMovedIntoDocument(0)?
        return DragSession();
    }

    DragSession dragSession;
    m_documentIsHandlingDrag = tryDocumentDrag(dragData, m_dragDestinationAction, dragSession);
    if (!m_documentIsHandlingDrag && (m_dragDestinationAction & DragDestinationActionLoad))
        dragSession.operation = operationForLoad(dragData);
    return dragSession;
}

static HTMLInputElement* asFileInput(Node* node)
{
    ASSERT(node);
    for (; node; node = node->shadowHost()) {
        if (node->hasTagName(HTMLNames::inputTag) && toHTMLInputElement(node)->isFileUpload())
            return toHTMLInputElement(node);
    }
    return 0;
}

// This can return null if an empty document is loaded.
static Element* elementUnderMouse(Document* documentUnderMouse, const IntPoint& p)
{
    Frame* frame = documentUnderMouse->frame();
    float zoomFactor = frame ? frame->pageZoomFactor() : 1;
    LayoutPoint point = roundedLayoutPoint(FloatPoint(p.x() * zoomFactor, p.y() * zoomFactor));

    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent);
    HitTestResult result(point);
    documentUnderMouse->renderView()->hitTest(request, result);

    Node* n = result.innerNode();
    while (n && !n->isElementNode())
        n = n->parentOrShadowHostNode();
    if (n)
        n = n->deprecatedShadowAncestorNode();

    return toElement(n);
}

bool DragController::tryDocumentDrag(DragData* dragData, DragDestinationAction actionMask, DragSession& dragSession)
{
    ASSERT(dragData);

    if (!m_documentUnderMouse)
        return false;

    if (m_dragInitiator && !m_documentUnderMouse->securityOrigin()->canReceiveDragData(m_dragInitiator->securityOrigin()))
        return false;

    bool isHandlingDrag = false;
    if (actionMask & DragDestinationActionDHTML) {
        isHandlingDrag = tryDHTMLDrag(dragData, dragSession.operation);
        // Do not continue if m_documentUnderMouse has been reset by tryDHTMLDrag.
        // tryDHTMLDrag fires dragenter event. The event listener that listens
        // to this event may create a nested message loop (open a modal dialog),
        // which could process dragleave event and reset m_documentUnderMouse in
        // dragExited.
        if (!m_documentUnderMouse)
            return false;
    }

    // It's unclear why this check is after tryDHTMLDrag.
    // We send drag events in tryDHTMLDrag and that may be the reason.
    RefPtr<FrameView> frameView = m_documentUnderMouse->view();
    if (!frameView)
        return false;

    if (isHandlingDrag) {
        m_page->dragCaretController().clear();
        return true;
    }

    if ((actionMask & DragDestinationActionEdit) && canProcessDrag(dragData)) {
        IntPoint point = frameView->windowToContents(dragData->clientPosition());
        Element* element = elementUnderMouse(m_documentUnderMouse.get(), point);
        if (!element)
            return false;

        HTMLInputElement* elementAsFileInput = asFileInput(element);
        if (m_fileInputElementUnderMouse != elementAsFileInput) {
            if (m_fileInputElementUnderMouse)
                m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
            m_fileInputElementUnderMouse = elementAsFileInput;
        }

        if (!m_fileInputElementUnderMouse)
            m_page->dragCaretController().setCaretPosition(m_documentUnderMouse->frame()->visiblePositionForPoint(point));

        Frame* innerFrame = element->document().frame();
        dragSession.operation = dragIsMove(innerFrame->selection(), dragData) ? DragOperationMove : DragOperationCopy;
        dragSession.mouseIsOverFileInput = m_fileInputElementUnderMouse;
        dragSession.numberOfItemsToBeAccepted = 0;

        unsigned numberOfFiles = dragData->numberOfFiles();
        if (m_fileInputElementUnderMouse) {
            if (m_fileInputElementUnderMouse->isDisabledFormControl())
                dragSession.numberOfItemsToBeAccepted = 0;
            else if (m_fileInputElementUnderMouse->multiple())
                dragSession.numberOfItemsToBeAccepted = numberOfFiles;
            else if (numberOfFiles > 1)
                dragSession.numberOfItemsToBeAccepted = 0;
            else
                dragSession.numberOfItemsToBeAccepted = 1;

            if (!dragSession.numberOfItemsToBeAccepted)
                dragSession.operation = DragOperationNone;
            m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(dragSession.numberOfItemsToBeAccepted);
        } else {
            // We are not over a file input element. The dragged item(s) will only
            // be loaded into the view the number of dragged items is 1.
            dragSession.numberOfItemsToBeAccepted = numberOfFiles != 1 ? 0 : 1;
        }

        return true;
    }

    // We are not over an editable region. Make sure we're clearing any prior drag cursor.
    m_page->dragCaretController().clear();
    if (m_fileInputElementUnderMouse)
        m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
    m_fileInputElementUnderMouse = 0;
    return false;
}

DragOperation DragController::operationForLoad(DragData* dragData)
{
    ASSERT(dragData);
    Document* doc = m_page->mainFrame()->documentAtPoint(dragData->clientPosition());

    if (doc && (m_didInitiateDrag || doc->isPluginDocument() || doc->rendererIsEditable()))
        return DragOperationNone;
    return dragOperation(dragData);
}

static bool setSelectionToDragCaret(Frame* frame, VisibleSelection& dragCaret, RefPtr<Range>& range, const IntPoint& point)
{
    frame->selection().setSelection(dragCaret);
    if (frame->selection().isNone()) {
        dragCaret = frame->visiblePositionForPoint(point);
        frame->selection().setSelection(dragCaret);
        range = dragCaret.toNormalizedRange();
    }
    return !frame->selection().isNone() && frame->selection().isContentEditable();
}

bool DragController::dispatchTextInputEventFor(Frame* innerFrame, DragData* dragData)
{
    ASSERT(m_page->dragCaretController().hasCaret());
    String text = m_page->dragCaretController().isContentRichlyEditable() ? "" : dragData->asPlainText(innerFrame);
    Node* target = innerFrame->editor().findEventTargetFrom(m_page->dragCaretController().caretPosition());
    return target->dispatchEvent(TextEvent::createForDrop(innerFrame->domWindow(), text), IGNORE_EXCEPTION);
}

bool DragController::concludeEditDrag(DragData* dragData)
{
    ASSERT(dragData);

    RefPtr<HTMLInputElement> fileInput = m_fileInputElementUnderMouse;
    if (m_fileInputElementUnderMouse) {
        m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
        m_fileInputElementUnderMouse = 0;
    }

    if (!m_documentUnderMouse)
        return false;

    IntPoint point = m_documentUnderMouse->view()->windowToContents(dragData->clientPosition());
    Element* element = elementUnderMouse(m_documentUnderMouse.get(), point);
    if (!element)
        return false;
    RefPtr<Frame> innerFrame = element->ownerDocument()->frame();
    ASSERT(innerFrame);

    if (m_page->dragCaretController().hasCaret() && !dispatchTextInputEventFor(innerFrame.get(), dragData))
        return true;

    if (dragData->containsFiles() && fileInput) {
        // fileInput should be the element we hit tested for, unless it was made
        // display:none in a drop event handler.
        ASSERT(fileInput == element || !fileInput->renderer());
        if (fileInput->isDisabledFormControl())
            return false;

        return fileInput->receiveDroppedFiles(dragData);
    }

    if (!m_page->dragController().canProcessDrag(dragData)) {
        m_page->dragCaretController().clear();
        return false;
    }

    VisibleSelection dragCaret = m_page->dragCaretController().caretPosition();
    m_page->dragCaretController().clear();
    RefPtr<Range> range = dragCaret.toNormalizedRange();
    RefPtr<Element> rootEditableElement = innerFrame->selection().rootEditableElement();

    // For range to be null a WebKit client must have done something bad while
    // manually controlling drag behaviour
    if (!range)
        return false;
    ResourceFetcher* fetcher = range->ownerDocument().fetcher();
    ResourceCacheValidationSuppressor validationSuppressor(fetcher);
    if (dragIsMove(innerFrame->selection(), dragData) || dragCaret.isContentRichlyEditable()) {
        bool chosePlainText = false;
        RefPtr<DocumentFragment> fragment = documentFragmentFromDragData(dragData, innerFrame.get(), range, true, chosePlainText);
        if (!fragment || !innerFrame->editor().shouldInsertFragment(fragment, range, EditorInsertActionDropped)) {
            return false;
        }

        if (dragIsMove(innerFrame->selection(), dragData)) {
            // NSTextView behavior is to always smart delete on moving a selection,
            // but only to smart insert if the selection granularity is word granularity.
            bool smartDelete = innerFrame->editor().smartInsertDeleteEnabled();
            bool smartInsert = smartDelete && innerFrame->selection().granularity() == WordGranularity && dragData->canSmartReplace();
            MoveSelectionCommand::create(fragment, dragCaret.base(), smartInsert, smartDelete)->apply();
        } else {
            if (setSelectionToDragCaret(innerFrame.get(), dragCaret, range, point)) {
                ReplaceSelectionCommand::CommandOptions options = ReplaceSelectionCommand::SelectReplacement | ReplaceSelectionCommand::PreventNesting;
                if (dragData->canSmartReplace())
                    options |= ReplaceSelectionCommand::SmartReplace;
                if (chosePlainText)
                    options |= ReplaceSelectionCommand::MatchStyle;
                ASSERT(m_documentUnderMouse);
                ReplaceSelectionCommand::create(*m_documentUnderMouse.get(), fragment, options)->apply();
            }
        }
    } else {
        String text = dragData->asPlainText(innerFrame.get());
        if (text.isEmpty() || !innerFrame->editor().shouldInsertText(text, range.get(), EditorInsertActionDropped)) {
            return false;
        }

        if (setSelectionToDragCaret(innerFrame.get(), dragCaret, range, point)) {
            ASSERT(m_documentUnderMouse);
            ReplaceSelectionCommand::create(*m_documentUnderMouse.get(), createFragmentFromText(range.get(), text),  ReplaceSelectionCommand::SelectReplacement | ReplaceSelectionCommand::MatchStyle | ReplaceSelectionCommand::PreventNesting)->apply();
        }
    }

    if (rootEditableElement) {
        if (Frame* frame = rootEditableElement->document().frame())
            frame->eventHandler()->updateDragStateAfterEditDragIfNeeded(rootEditableElement.get());
    }

    return true;
}

bool DragController::canProcessDrag(DragData* dragData)
{
    ASSERT(dragData);

    if (!dragData->containsCompatibleContent())
        return false;

    IntPoint point = m_page->mainFrame()->view()->windowToContents(dragData->clientPosition());
    HitTestResult result = HitTestResult(point);
    if (!m_page->mainFrame()->contentRenderer())
        return false;

    result = m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(point);

    if (!result.innerNonSharedNode())
        return false;

    if (dragData->containsFiles() && asFileInput(result.innerNonSharedNode()))
        return true;

    if (result.innerNonSharedNode()->isPluginElement()) {
        HTMLPlugInElement* plugin = toHTMLPlugInElement(result.innerNonSharedNode());
        if (!plugin->canProcessDrag() && !result.innerNonSharedNode()->rendererIsEditable())
            return false;
    } else if (!result.innerNonSharedNode()->rendererIsEditable())
        return false;

    if (m_didInitiateDrag && m_documentUnderMouse == m_dragInitiator && result.isSelected())
        return false;

    return true;
}

static DragOperation defaultOperationForDrag(DragOperation srcOpMask)
{
    // This is designed to match IE's operation fallback for the case where
    // the page calls preventDefault() in a drag event but doesn't set dropEffect.
    if (srcOpMask == DragOperationEvery)
        return DragOperationCopy;
    if (srcOpMask == DragOperationNone)
        return DragOperationNone;
    if (srcOpMask & DragOperationMove || srcOpMask & DragOperationGeneric)
        return DragOperationMove;
    if (srcOpMask & DragOperationCopy)
        return DragOperationCopy;
    if (srcOpMask & DragOperationLink)
        return DragOperationLink;

    // FIXME: Does IE really return "generic" even if no operations were allowed by the source?
    return DragOperationGeneric;
}

bool DragController::tryDHTMLDrag(DragData* dragData, DragOperation& operation)
{
    ASSERT(dragData);
    ASSERT(m_documentUnderMouse);
    RefPtr<Frame> mainFrame = m_page->mainFrame();
    RefPtr<FrameView> viewProtector = mainFrame->view();
    if (!viewProtector)
        return false;

    ClipboardAccessPolicy policy = m_documentUnderMouse->securityOrigin()->isLocal() ? ClipboardReadable : ClipboardTypesReadable;
    RefPtr<Clipboard> clipboard = createDraggingClipboard(policy, dragData);
    DragOperation srcOpMask = dragData->draggingSourceOperationMask();
    clipboard->setSourceOperation(srcOpMask);

    PlatformMouseEvent event = createMouseEvent(dragData);
    if (!mainFrame->eventHandler()->updateDragAndDrop(event, clipboard.get())) {
        clipboard->setAccessPolicy(ClipboardNumb);    // invalidate clipboard here for security
        return false;
    }

    operation = clipboard->destinationOperation();
    if (clipboard->dropEffectIsUninitialized())
        operation = defaultOperationForDrag(srcOpMask);
    else if (!(srcOpMask & operation)) {
        // The element picked an operation which is not supported by the source
        operation = DragOperationNone;
    }

    clipboard->setAccessPolicy(ClipboardNumb);    // invalidate clipboard here for security
    return true;
}

Node* DragController::draggableNode(const Frame* src, Node* startNode, const IntPoint& dragOrigin, DragState& state) const
{
    state.m_dragType = (src->selection().contains(dragOrigin)) ? DragSourceActionSelection : DragSourceActionNone;

    for (const RenderObject* renderer = startNode->renderer(); renderer; renderer = renderer->parent()) {
        Node* node = renderer->nonPseudoNode();
        if (!node)
            // Anonymous render blocks don't correspond to actual DOM nodes, so we skip over them
            // for the purposes of finding a draggable node.
            continue;
        if (!(state.m_dragType & DragSourceActionSelection) && node->isTextNode() && node->canStartSelection())
            // In this case we have a click in the unselected portion of text. If this text is
            // selectable, we want to start the selection process instead of looking for a parent
            // to try to drag.
            return 0;
        if (node->isElementNode()) {
            EUserDrag dragMode = renderer->style()->userDrag();
            if (dragMode == DRAG_NONE)
                continue;
            if (renderer->isImage()
                && src->settings()
                && src->settings()->loadsImagesAutomatically()) {
                state.m_dragType = static_cast<DragSourceAction>(state.m_dragType | DragSourceActionImage);
                return node;
            }
            if (isHTMLAnchorElement(node)
                && toHTMLAnchorElement(node)->isLiveLink()) {
                state.m_dragType = static_cast<DragSourceAction>(state.m_dragType | DragSourceActionLink);
                return node;
            }
            if (dragMode == DRAG_ELEMENT) {
                state.m_dragType = static_cast<DragSourceAction>(state.m_dragType | DragSourceActionDHTML);
                return node;
            }
        }
    }

    // We either have nothing to drag or we have a selection and we're not over a draggable element.
    return (state.m_dragType & DragSourceActionSelection) ? startNode : 0;
}

static ImageResource* getImageResource(Element* element)
{
    ASSERT(element);
    RenderObject* renderer = element->renderer();
    if (!renderer || !renderer->isImage())
        return 0;
    RenderImage* image = toRenderImage(renderer);
    return image->cachedImage();
}

static Image* getImage(Element* element)
{
    ASSERT(element);
    ImageResource* cachedImage = getImageResource(element);
    // Don't use cachedImage->imageForRenderer() here as that may return BitmapImages for cached SVG Images.
    // Users of getImage() want access to the SVGImage, in order to figure out the filename extensions,
    // which would be empty when asking the cached BitmapImages.
    return (cachedImage && !cachedImage->errorOccurred()) ?
        cachedImage->image() : 0;
}

static void prepareClipboardForImageDrag(Frame* source, Clipboard* clipboard, Element* node, const KURL& linkURL, const KURL& imageURL, const String& label)
{
    if (node->isContentRichlyEditable()) {
        RefPtr<Range> range = source->document()->createRange();
        range->selectNode(node, ASSERT_NO_EXCEPTION);
        source->selection().setSelection(VisibleSelection(range.get(), DOWNSTREAM));
    }
    clipboard->declareAndWriteDragImage(node, !linkURL.isEmpty() ? linkURL : imageURL, label);
}

bool DragController::populateDragClipboard(Frame* src, const DragState& state, const IntPoint& dragOrigin)
{
    ASSERT(dragTypeIsValid(state.m_dragType));
    ASSERT(src);
    if (!src->view() || !src->contentRenderer())
        return false;

    HitTestResult hitTestResult = src->eventHandler()->hitTestResultAtPoint(dragOrigin, HitTestRequest::ReadOnly | HitTestRequest::Active);
    // FIXME: Can this even happen? I guess it's possible, but should verify
    // with a layout test.
    if (!state.m_dragSrc->contains(hitTestResult.innerNode())) {
        // The original node being dragged isn't under the drag origin anymore... maybe it was
        // hidden or moved out from under the cursor. Regardless, we don't want to start a drag on
        // something that's not actually under the drag origin.
        return false;
    }
    const KURL& linkURL = hitTestResult.absoluteLinkURL();
    const KURL& imageURL = hitTestResult.absoluteImageURL();

    Clipboard* clipboard = state.m_dragClipboard.get();
    Node* node = state.m_dragSrc.get();

    if (state.m_dragType == DragSourceActionSelection) {
        if (enclosingTextFormControl(src->selection().start())) {
            clipboard->writePlainText(src->selectedTextForClipboard());
        } else {
            RefPtr<Range> selectionRange = src->selection().toNormalizedRange();
            ASSERT(selectionRange);

            clipboard->writeRange(selectionRange.get(), src);
        }
    } else if (state.m_dragType == DragSourceActionImage) {
        if (imageURL.isEmpty() || !node || !node->isElementNode())
            return false;
        Element* element = toElement(node);
        prepareClipboardForImageDrag(src, clipboard, element, linkURL, imageURL, hitTestResult.altDisplayString());
    } else if (state.m_dragType == DragSourceActionLink) {
        if (linkURL.isEmpty())
            return false;
        // Simplify whitespace so the title put on the clipboard resembles what the user sees
        // on the web page. This includes replacing newlines with spaces.
        clipboard->writeURL(linkURL, hitTestResult.textContent().simplifyWhiteSpace());
    }
    // FIXME: For DHTML/draggable element drags, write element markup to clipboard.
    return true;
}

static IntPoint dragLocationForDHTMLDrag(const IntPoint& mouseDraggedPoint, const IntPoint& dragOrigin, const IntPoint& dragImageOffset, bool isLinkImage)
{
    // dragImageOffset is the cursor position relative to the lower-left corner of the image.
    const int yOffset = -dragImageOffset.y();

    if (isLinkImage)
        return IntPoint(mouseDraggedPoint.x() - dragImageOffset.x(), mouseDraggedPoint.y() + yOffset);

    return IntPoint(dragOrigin.x() - dragImageOffset.x(), dragOrigin.y() + yOffset);
}

static IntPoint dragLocationForSelectionDrag(Frame* sourceFrame)
{
    IntRect draggingRect = enclosingIntRect(sourceFrame->selection().bounds());
    int xpos = draggingRect.maxX();
    xpos = draggingRect.x() < xpos ? draggingRect.x() : xpos;
    int ypos = draggingRect.maxY();
    ypos = draggingRect.y() < ypos ? draggingRect.y() : ypos;
    return IntPoint(xpos, ypos);
}

static const IntSize& maxDragImageSize()
{
#if OS(MACOSX)
    // Match Safari's drag image size.
    static const IntSize maxDragImageSize(400, 400);
#else
    static const IntSize maxDragImageSize(200, 200);
#endif
    return maxDragImageSize;
}

static PassOwnPtr<DragImage> dragImageForImage(Element* element, Image* image, const IntPoint& dragOrigin, const IntRect& imageRect, IntPoint& dragLocation)
{
    OwnPtr<DragImage> dragImage;
    IntPoint origin;

    if (image->size().height() * image->size().width() <= MaxOriginalImageArea
        && (dragImage = DragImage::create(image, element->renderer() ? element->renderer()->shouldRespectImageOrientation() : DoNotRespectImageOrientation))) {
        IntSize originalSize = imageRect.size();
        origin = imageRect.location();

        dragImage->fitToMaxSize(imageRect.size(), maxDragImageSize());
        dragImage->dissolveToFraction(DragImageAlpha);
        IntSize newSize = dragImage->size();

        // Properly orient the drag image and orient it differently if it's smaller than the original
        float scale = newSize.width() / (float)originalSize.width();
        float dx = origin.x() - dragOrigin.x();
        dx *= scale;
        origin.setX((int)(dx + 0.5));
        float dy = origin.y() - dragOrigin.y();
        dy *= scale;
        origin.setY((int)(dy + 0.5));
    }

    dragLocation = dragOrigin + origin;
    return dragImage.release();
}

static PassOwnPtr<DragImage> dragImageForLink(const KURL& linkURL, const String& linkText, float deviceScaleFactor, const IntPoint& mouseDraggedPoint, IntPoint& dragLoc)
{
    FontDescription fontDescription;
    RenderTheme::theme().systemFont(WebCore::CSSValueNone, fontDescription);
    OwnPtr<DragImage> dragImage = DragImage::create(linkURL, linkText, fontDescription, deviceScaleFactor);

    IntSize size = dragImage ? dragImage->size() : IntSize();
    IntPoint dragImageOffset(-size.width() / 2, -LinkDragBorderInset);
    dragLoc = IntPoint(mouseDraggedPoint.x() + dragImageOffset.x(), mouseDraggedPoint.y() + dragImageOffset.y());

    return dragImage.release();
}

bool DragController::startDrag(Frame* src, const DragState& state, const PlatformMouseEvent& dragEvent, const IntPoint& dragOrigin)
{
    ASSERT(dragTypeIsValid(state.m_dragType));
    ASSERT(src);
    if (!src->view() || !src->contentRenderer())
        return false;

    HitTestResult hitTestResult = src->eventHandler()->hitTestResultAtPoint(dragOrigin);
    if (!state.m_dragSrc->contains(hitTestResult.innerNode()))
        // The original node being dragged isn't under the drag origin anymore... maybe it was
        // hidden or moved out from under the cursor. Regardless, we don't want to start a drag on
        // something that's not actually under the drag origin.
        return false;
    const KURL& linkURL = hitTestResult.absoluteLinkURL();
    const KURL& imageURL = hitTestResult.absoluteImageURL();

    IntPoint mouseDraggedPoint = src->view()->windowToContents(dragEvent.position());

    IntPoint dragLocation;
    IntPoint dragOffset;

    Clipboard* clipboard = state.m_dragClipboard.get();
    // We allow DHTML/JS to set the drag image, even if its a link, image or text we're dragging.
    // This is in the spirit of the IE API, which allows overriding of pasteboard data and DragOp.
    OwnPtr<DragImage> dragImage = clipboard->createDragImage(dragOffset, src);
    if (dragImage) {
        dragLocation = dragLocationForDHTMLDrag(mouseDraggedPoint, dragOrigin, dragOffset, !linkURL.isEmpty());
    }

    Node* node = state.m_dragSrc.get();
    if (state.m_dragType == DragSourceActionSelection) {
        if (!dragImage) {
            dragImage = src->dragImageForSelection();
            if (dragImage)
                dragImage->dissolveToFraction(DragImageAlpha);
            dragLocation = dragLocationForSelectionDrag(src);
        }
        doSystemDrag(dragImage.get(), dragLocation, dragOrigin, clipboard, src, false);
    } else if (state.m_dragType == DragSourceActionImage) {
        if (imageURL.isEmpty() || !node || !node->isElementNode())
            return false;
        Element* element = toElement(node);
        Image* image = getImage(element);
        if (!image || image->isNull())
            return false;
        // We shouldn't be starting a drag for an image that can't provide an extension.
        // This is an early detection for problems encountered later upon drop.
        ASSERT(!image->filenameExtension().isEmpty());
        if (!dragImage) {
            dragImage = dragImageForImage(element, image, dragOrigin, hitTestResult.imageRect(), dragLocation);
        }
        doSystemDrag(dragImage.get(), dragLocation, dragOrigin, clipboard, src, false);
    } else if (state.m_dragType == DragSourceActionLink) {
        if (linkURL.isEmpty())
            return false;
        if (src->selection().isCaret() && src->selection().isContentEditable()) {
            // a user can initiate a drag on a link without having any text
            // selected.  In this case, we should expand the selection to
            // the enclosing anchor element
            if (Node* node = enclosingAnchorElement(src->selection().base()))
                src->selection().setSelection(VisibleSelection::selectionFromContentsOfNode(node));
        }

        if (!dragImage) {
            ASSERT(src->page());
            float deviceScaleFactor = src->page()->deviceScaleFactor();
            dragImage = dragImageForLink(linkURL, hitTestResult.textContent(), deviceScaleFactor, mouseDraggedPoint, dragLocation);
        }
        doSystemDrag(dragImage.get(), dragLocation, mouseDraggedPoint, clipboard, src, true);
    } else if (state.m_dragType == DragSourceActionDHTML) {
        if (!dragImage)
            return false;
        doSystemDrag(dragImage.get(), dragLocation, dragOrigin, clipboard, src, false);
    } else {
        ASSERT_NOT_REACHED();
        return false;
    }

    return true;
}

void DragController::doSystemDrag(DragImage* image, const IntPoint& dragLocation, const IntPoint& eventPos, Clipboard* clipboard, Frame* frame, bool forLink)
{
    m_didInitiateDrag = true;
    m_dragInitiator = frame->document();
    // Protect this frame and view, as a load may occur mid drag and attempt to unload this frame
    RefPtr<Frame> frameProtector = m_page->mainFrame();
    RefPtr<FrameView> viewProtector = frameProtector->view();
    m_client->startDrag(image, viewProtector->rootViewToContents(frame->view()->contentsToRootView(dragLocation)),
        viewProtector->rootViewToContents(frame->view()->contentsToRootView(eventPos)), clipboard, frameProtector.get(), forLink);
    // DragClient::startDrag can cause our Page to dispear, deallocating |this|.
    if (!frameProtector->page())
        return;

    cleanupAfterSystemDrag();
}

DragOperation DragController::dragOperation(DragData* dragData)
{
    // FIXME: To match the MacOS behaviour we should return DragOperationNone
    // if we are a modal window, we are the drag source, or the window is an
    // attached sheet If this can be determined from within WebCore
    // operationForDrag can be pulled into WebCore itself
    ASSERT(dragData);
    return dragData->containsURL(0) && !m_didInitiateDrag ? DragOperationCopy : DragOperationNone;
}

bool DragController::isCopyKeyDown(DragData*)
{
    // FIXME: This should not be OS specific.  Delegate to the embedder instead.
#if OS(WIN)
    return ::GetAsyncKeyState(VK_CONTROL);
#else
    return false;
#endif
}

void DragController::cleanupAfterSystemDrag()
{
}

} // namespace WebCore

