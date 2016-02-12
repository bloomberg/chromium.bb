/*
 * Copyright (C) 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "core/editing/Editor.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/CSSPropertyNames.h"
#include "core/EventNames.h"
#include "core/HTMLNames.h"
#include "core/clipboard/DataObject.h"
#include "core/clipboard/DataTransfer.h"
#include "core/clipboard/Pasteboard.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/ParserContentPolicy.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/RenderedPosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/commands/ApplyStyleCommand.h"
#include "core/editing/commands/DeleteSelectionCommand.h"
#include "core/editing/commands/IndentOutdentCommand.h"
#include "core/editing/commands/InsertListCommand.h"
#include "core/editing/commands/MoveSelectionCommand.h"
#include "core/editing/commands/RemoveFormatCommand.h"
#include "core/editing/commands/ReplaceSelectionCommand.h"
#include "core/editing/commands/SimplifyMarkupCommand.h"
#include "core/editing/commands/TypingCommand.h"
#include "core/editing/commands/UndoStack.h"
#include "core/editing/iterators/SearchBuffer.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/serializers/Serialization.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/events/ClipboardEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/ScopedEventQueue.h"
#include "core/events/TextEvent.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/input/EventHandler.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutImage.h"
#include "core/loader/EmptyClients.h"
#include "core/page/EditorClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/svg/SVGImageElement.h"
#include "platform/KillRing.h"
#include "platform/weborigin/KURL.h"
#include "wtf/text/CharacterNames.h"

namespace blink {

using namespace HTMLNames;
using namespace WTF;
using namespace Unicode;

Editor::RevealSelectionScope::RevealSelectionScope(Editor* editor)
    : m_editor(editor)
{
    ++m_editor->m_preventRevealSelection;
}

Editor::RevealSelectionScope::~RevealSelectionScope()
{
    ASSERT(m_editor->m_preventRevealSelection);
    --m_editor->m_preventRevealSelection;
    if (!m_editor->m_preventRevealSelection)
        m_editor->frame().selection().revealSelection(ScrollAlignment::alignToEdgeIfNeeded, RevealExtent);
}

// When an event handler has moved the selection outside of a text control
// we should use the target control's selection for this editing operation.
VisibleSelection Editor::selectionForCommand(Event* event)
{
    VisibleSelection selection = frame().selection().selection();
    if (!event)
        return selection;
    // If the target is a text control, and the current selection is outside of its shadow tree,
    // then use the saved selection for that text control.
    HTMLTextFormControlElement* textFormControlOfSelectionStart = enclosingTextFormControl(selection.start());
    HTMLTextFormControlElement* textFromControlOfTarget = isHTMLTextFormControlElement(*event->target()->toNode()) ? toHTMLTextFormControlElement(event->target()->toNode()) : 0;
    if (textFromControlOfTarget && (selection.start().isNull() || textFromControlOfTarget != textFormControlOfSelectionStart)) {
        if (RefPtrWillBeRawPtr<Range> range = textFromControlOfTarget->selection())
            return VisibleSelection(EphemeralRange(range.get()), TextAffinity::Downstream, selection.isDirectional());
    }
    return selection;
}

// Function considers Mac editing behavior a fallback when Page or Settings is not available.
EditingBehavior Editor::behavior() const
{
    if (!frame().settings())
        return EditingBehavior(EditingMacBehavior);

    return EditingBehavior(frame().settings()->editingBehaviorType());
}

static EditorClient& emptyEditorClient()
{
    DEFINE_STATIC_LOCAL(EmptyEditorClient, client, ());
    return client;
}

EditorClient& Editor::client() const
{
    if (Page* page = frame().page())
        return page->editorClient();
    return emptyEditorClient();
}

UndoStack* Editor::undoStack() const
{
    if (Page* page = frame().page())
        return &page->undoStack();
    return 0;
}

bool Editor::handleTextEvent(TextEvent* event)
{
    // Default event handling for Drag and Drop will be handled by DragController
    // so we leave the event for it.
    if (event->isDrop())
        return false;

    if (event->isPaste()) {
        if (event->pastingFragment())
            replaceSelectionWithFragment(event->pastingFragment(), false, event->shouldSmartReplace(), event->shouldMatchStyle());
        else
            replaceSelectionWithText(event->data(), false, event->shouldSmartReplace());
        return true;
    }

    String data = event->data();
    if (data == "\n") {
        if (event->isLineBreak())
            return insertLineBreak();
        return insertParagraphSeparator();
    }

    return insertTextWithoutSendingTextEvent(data, false, event);
}

bool Editor::canEdit() const
{
    return frame().selection().rootEditableElement();
}

bool Editor::canEditRichly() const
{
    return frame().selection().isContentRichlyEditable();
}

// WinIE uses onbeforecut and onbeforepaste to enables the cut and paste menu items. They
// also send onbeforecopy, apparently for symmetry, but it doesn't affect the menu items.
// We need to use onbeforecopy as a real menu enabler because we allow elements that are not
// normally selectable to implement copy/paste (like divs, or a document body).

bool Editor::canDHTMLCut()
{
    return !frame().selection().isInPasswordField() && !dispatchCPPEvent(EventTypeNames::beforecut, DataTransferNumb);
}

bool Editor::canDHTMLCopy()
{
    return !frame().selection().isInPasswordField() && !dispatchCPPEvent(EventTypeNames::beforecopy, DataTransferNumb);
}

bool Editor::canCut() const
{
    return canCopy() && canDelete();
}

static HTMLImageElement* imageElementFromImageDocument(Document* document)
{
    if (!document)
        return 0;
    if (!document->isImageDocument())
        return 0;

    HTMLElement* body = document->body();
    if (!body)
        return 0;

    Node* node = body->firstChild();
    if (!isHTMLImageElement(node))
        return 0;
    return toHTMLImageElement(node);
}

bool Editor::canCopy() const
{
    if (imageElementFromImageDocument(frame().document()))
        return true;
    FrameSelection& selection = frame().selection();
    return selection.isRange() && !selection.isInPasswordField();
}

bool Editor::canPaste() const
{
    return canEdit();
}

bool Editor::canDelete() const
{
    FrameSelection& selection = frame().selection();
    return selection.isRange() && selection.rootEditableElement();
}

bool Editor::canDeleteRange(const EphemeralRange& range) const
{
    Node* startContainer = range.startPosition().computeContainerNode();
    Node* endContainer = range.endPosition().computeContainerNode();
    if (!startContainer || !endContainer)
        return false;

    if (!startContainer->hasEditableStyle() || !endContainer->hasEditableStyle())
        return false;

    if (range.isCollapsed()) {
        VisiblePosition start = createVisiblePosition(range.startPosition());
        VisiblePosition previous = previousPositionOf(start);
        // FIXME: We sometimes allow deletions at the start of editable roots, like when the caret is in an empty list item.
        if (previous.isNull() || previous.deepEquivalent().anchorNode()->rootEditableElement() != startContainer->rootEditableElement())
            return false;
    }
    return true;
}

bool Editor::smartInsertDeleteEnabled() const
{
    if (Settings* settings = frame().settings())
        return settings->smartInsertDeleteEnabled();
    return false;
}

bool Editor::canSmartCopyOrDelete() const
{
    return smartInsertDeleteEnabled() && frame().selection().granularity() == WordGranularity;
}

bool Editor::isSelectTrailingWhitespaceEnabled() const
{
    if (Settings* settings = frame().settings())
        return settings->selectTrailingWhitespaceEnabled();
    return false;
}

bool Editor::deleteWithDirection(SelectionDirection direction, TextGranularity granularity, bool killRing, bool isTypingAction)
{
    if (!canEdit())
        return false;

    if (frame().selection().isRange()) {
        if (isTypingAction) {
            ASSERT(frame().document());
            TypingCommand::deleteKeyPressed(*frame().document(), canSmartCopyOrDelete() ? TypingCommand::SmartDelete : 0, granularity);
            revealSelectionAfterEditingOperation();
        } else {
            if (killRing)
                addToKillRing(selectedRange());
            deleteSelectionWithSmartDelete(canSmartCopyOrDelete());
            // Implicitly calls revealSelectionAfterEditingOperation().
        }
    } else {
        TypingCommand::Options options = 0;
        if (canSmartCopyOrDelete())
            options |= TypingCommand::SmartDelete;
        if (killRing)
            options |= TypingCommand::KillRing;
        switch (direction) {
        case DirectionForward:
        case DirectionRight:
            ASSERT(frame().document());
            TypingCommand::forwardDeleteKeyPressed(*frame().document(), options, granularity);
            break;
        case DirectionBackward:
        case DirectionLeft:
            ASSERT(frame().document());
            TypingCommand::deleteKeyPressed(*frame().document(), options, granularity);
            break;
        }
        revealSelectionAfterEditingOperation();
    }

    // FIXME: We should to move this down into deleteKeyPressed.
    // clear the "start new kill ring sequence" setting, because it was set to true
    // when the selection was updated by deleting the range
    if (killRing)
        setStartNewKillRingSequence(false);

    return true;
}

void Editor::deleteSelectionWithSmartDelete(bool smartDelete)
{
    if (frame().selection().isNone())
        return;

    ASSERT(frame().document());
    DeleteSelectionCommand::create(*frame().document(), smartDelete)->apply();
}

void Editor::pasteAsPlainText(const String& pastingText, bool smartReplace)
{
    Element* target = findEventTargetFromSelection();
    if (!target)
        return;
    target->dispatchEvent(TextEvent::createForPlainTextPaste(frame().domWindow(), pastingText, smartReplace));
}

void Editor::pasteAsFragment(PassRefPtrWillBeRawPtr<DocumentFragment> pastingFragment, bool smartReplace, bool matchStyle)
{
    Element* target = findEventTargetFromSelection();
    if (!target)
        return;
    target->dispatchEvent(TextEvent::createForFragmentPaste(frame().domWindow(), pastingFragment, smartReplace, matchStyle));
}

bool Editor::tryDHTMLCopy()
{
    if (frame().selection().isInPasswordField())
        return false;

    return !dispatchCPPEvent(EventTypeNames::copy, DataTransferWritable);
}

bool Editor::tryDHTMLCut()
{
    if (frame().selection().isInPasswordField())
        return false;

    return !dispatchCPPEvent(EventTypeNames::cut, DataTransferWritable);
}

bool Editor::tryDHTMLPaste(PasteMode pasteMode)
{
    return !dispatchCPPEvent(EventTypeNames::paste, DataTransferReadable, pasteMode);
}

void Editor::pasteAsPlainTextWithPasteboard(Pasteboard* pasteboard)
{
    String text = pasteboard->plainText();
    pasteAsPlainText(text, canSmartReplaceWithPasteboard(pasteboard));
}

void Editor::pasteWithPasteboard(Pasteboard* pasteboard)
{
    RefPtrWillBeRawPtr<DocumentFragment> fragment = nullptr;
    bool chosePlainText = false;

    if (pasteboard->isHTMLAvailable()) {
        unsigned fragmentStart = 0;
        unsigned fragmentEnd = 0;
        KURL url;
        String markup = pasteboard->readHTML(url, fragmentStart, fragmentEnd);
        if (!markup.isEmpty()) {
            ASSERT(frame().document());
            fragment = createFragmentFromMarkupWithContext(*frame().document(), markup, fragmentStart, fragmentEnd, url, DisallowScriptingAndPluginContent);
        }
    }

    if (!fragment) {
        String text = pasteboard->plainText();
        if (!text.isEmpty()) {
            chosePlainText = true;
            fragment = createFragmentFromText(selectedRange(), text);
        }
    }

    if (fragment)
        pasteAsFragment(fragment, canSmartReplaceWithPasteboard(pasteboard), chosePlainText);
}

void Editor::writeSelectionToPasteboard()
{
    KURL url = frame().document()->url();
    String html = frame().selection().selectedHTMLForClipboard();
    String plainText = frame().selectedTextForClipboard();
    Pasteboard::generalPasteboard()->writeHTML(html, url, plainText, canSmartCopyOrDelete());
}

static PassRefPtr<Image> imageFromNode(const Node& node)
{
    node.document().updateLayoutIgnorePendingStylesheets();
    LayoutObject* layoutObject = node.layoutObject();
    if (!layoutObject)
        return nullptr;

    if (layoutObject->isCanvas())
        return toHTMLCanvasElement(node).copiedImage(FrontBuffer, PreferNoAcceleration);

    if (layoutObject->isImage()) {
        LayoutImage* layoutImage = toLayoutImage(layoutObject);
        if (!layoutImage)
            return nullptr;

        ImageResource* cachedImage = layoutImage->cachedImage();
        if (!cachedImage || cachedImage->errorOccurred())
            return nullptr;
        return cachedImage->image();
    }

    return nullptr;
}

static void writeImageNodeToPasteboard(Pasteboard* pasteboard, Node* node, const String& title)
{
    ASSERT(pasteboard);
    ASSERT(node);

    RefPtr<Image> image = imageFromNode(*node);
    if (!image.get())
        return;

    // FIXME: This should probably be reconciled with HitTestResult::absoluteImageURL.
    AtomicString urlString;
    if (isHTMLImageElement(*node) || isHTMLInputElement(*node))
        urlString = toHTMLElement(node)->getAttribute(srcAttr);
    else if (isSVGImageElement(*node))
        urlString = toSVGElement(node)->imageSourceURL();
    else if (isHTMLEmbedElement(*node) || isHTMLObjectElement(*node) || isHTMLCanvasElement(*node))
        urlString = toHTMLElement(node)->imageSourceURL();
    KURL url = urlString.isEmpty() ? KURL() : node->document().completeURL(stripLeadingAndTrailingHTMLSpaces(urlString));

    pasteboard->writeImage(image.get(), url, title);
}

// Returns whether caller should continue with "the default processing", which is the same as
// the event handler NOT setting the return value to false
bool Editor::dispatchCPPEvent(const AtomicString& eventType, DataTransferAccessPolicy policy, PasteMode pasteMode)
{
    Element* target = findEventTargetFromSelection();
    if (!target)
        return true;

    DataTransfer* dataTransfer = DataTransfer::create(
        DataTransfer::CopyAndPaste,
        policy,
        policy == DataTransferWritable
            ? DataObject::create()
            : DataObject::createFromPasteboard(pasteMode));

    RefPtrWillBeRawPtr<Event> evt = ClipboardEvent::create(eventType, true, true, dataTransfer);
    target->dispatchEvent(evt);
    bool noDefaultProcessing = evt->defaultPrevented();
    if (noDefaultProcessing && policy == DataTransferWritable)
        Pasteboard::generalPasteboard()->writeDataObject(dataTransfer->dataObject());

    // invalidate clipboard here for security
    dataTransfer->setAccessPolicy(DataTransferNumb);

    return !noDefaultProcessing;
}

bool Editor::canSmartReplaceWithPasteboard(Pasteboard* pasteboard)
{
    return smartInsertDeleteEnabled() && pasteboard->canSmartReplace();
}

void Editor::replaceSelectionWithFragment(PassRefPtrWillBeRawPtr<DocumentFragment> fragment, bool selectReplacement, bool smartReplace, bool matchStyle)
{
    if (frame().selection().isNone() || !frame().selection().isContentEditable() || !fragment)
        return;

    ReplaceSelectionCommand::CommandOptions options = ReplaceSelectionCommand::PreventNesting | ReplaceSelectionCommand::SanitizeFragment;
    if (selectReplacement)
        options |= ReplaceSelectionCommand::SelectReplacement;
    if (smartReplace)
        options |= ReplaceSelectionCommand::SmartReplace;
    if (matchStyle)
        options |= ReplaceSelectionCommand::MatchStyle;
    ASSERT(frame().document());
    ReplaceSelectionCommand::create(*frame().document(), fragment, options, EditActionPaste)->apply();
    revealSelectionAfterEditingOperation();
}

void Editor::replaceSelectionWithText(const String& text, bool selectReplacement, bool smartReplace)
{
    replaceSelectionWithFragment(createFragmentFromText(selectedRange(), text), selectReplacement, smartReplace, true);
}

// TODO(xiaochengh): Merge it with |replaceSelectionWithFragment()|.
void Editor::replaceSelectionAfterDragging(PassRefPtrWillBeRawPtr<DocumentFragment> fragment, bool smartReplace, bool plainText)
{
    ReplaceSelectionCommand::CommandOptions options = ReplaceSelectionCommand::SelectReplacement | ReplaceSelectionCommand::PreventNesting;
    if (smartReplace)
        options |= ReplaceSelectionCommand::SmartReplace;
    if (plainText)
        options |= ReplaceSelectionCommand::MatchStyle;
    ASSERT(frame().document());
    ReplaceSelectionCommand::create(*frame().document(), fragment, options, EditActionDrag)->apply();
}

void Editor::moveSelectionAfterDragging(PassRefPtrWillBeRawPtr<DocumentFragment> fragment, const Position& pos, bool smartInsert, bool smartDelete)
{
    MoveSelectionCommand::create(fragment, pos, smartInsert, smartDelete)->apply();
}

EphemeralRange Editor::selectedRange()
{
    return frame().selection().selection().toNormalizedEphemeralRange();
}

bool Editor::shouldDeleteRange(const EphemeralRange& range) const
{
    if (range.isCollapsed())
        return false;

    return canDeleteRange(range);
}

void Editor::notifyComponentsOnChangedSelection(const VisibleSelection& oldSelection, FrameSelection::SetSelectionOptions options)
{
    client().respondToChangedSelection(m_frame, frame().selection().selectionType());
    setStartNewKillRingSequence(true);
}

void Editor::respondToChangedContents(const VisibleSelection& endingSelection)
{
    if (frame().settings() && frame().settings()->accessibilityEnabled()) {
        Node* node = endingSelection.start().anchorNode();
        if (AXObjectCache* cache = frame().document()->existingAXObjectCache())
            cache->handleEditableTextContentChanged(node);
    }

    spellChecker().updateMarkersForWordsAffectedByEditing(true);
    client().respondToChangedContents();
}

void Editor::removeFormattingAndStyle()
{
    ASSERT(frame().document());
    RemoveFormatCommand::create(*frame().document())->apply();
}

void Editor::clearLastEditCommand()
{
    m_lastEditCommand.clear();
}

Element* Editor::findEventTargetFrom(const VisibleSelection& selection) const
{
    Element* target = associatedElementOf(selection.start());
    if (!target)
        target = frame().document()->body();

    return target;
}

Element* Editor::findEventTargetFromSelection() const
{
    return findEventTargetFrom(frame().selection().selection());
}

void Editor::applyStyle(StylePropertySet* style, EditAction editingAction)
{
    switch (frame().selection().selectionType()) {
    case NoSelection:
        // do nothing
        break;
    case CaretSelection:
        computeAndSetTypingStyle(style, editingAction);
        break;
    case RangeSelection:
        if (style) {
            ASSERT(frame().document());
            ApplyStyleCommand::create(*frame().document(), EditingStyle::create(style).get(), editingAction)->apply();
        }
        break;
    }
}

void Editor::applyParagraphStyle(StylePropertySet* style, EditAction editingAction)
{
    if (frame().selection().isNone() || !style)
        return;
    ASSERT(frame().document());
    ApplyStyleCommand::create(*frame().document(), EditingStyle::create(style).get(), editingAction, ApplyStyleCommand::ForceBlockProperties)->apply();
}

void Editor::applyStyleToSelection(StylePropertySet* style, EditAction editingAction)
{
    if (!style || style->isEmpty() || !canEditRichly())
        return;

    applyStyle(style, editingAction);
}

void Editor::applyParagraphStyleToSelection(StylePropertySet* style, EditAction editingAction)
{
    if (!style || style->isEmpty() || !canEditRichly())
        return;

    applyParagraphStyle(style, editingAction);
}

bool Editor::selectionStartHasStyle(CSSPropertyID propertyID, const String& value) const
{
    return EditingStyle::create(propertyID, value)->triStateOfStyle(
        EditingStyle::styleAtSelectionStart(frame().selection().selection(), propertyID == CSSPropertyBackgroundColor).get());
}

TriState Editor::selectionHasStyle(CSSPropertyID propertyID, const String& value) const
{
    return EditingStyle::create(propertyID, value)->triStateOfStyle(frame().selection().selection());
}

String Editor::selectionStartCSSPropertyValue(CSSPropertyID propertyID)
{
    RefPtrWillBeRawPtr<EditingStyle> selectionStyle = EditingStyle::styleAtSelectionStart(frame().selection().selection(),
        propertyID == CSSPropertyBackgroundColor);
    if (!selectionStyle || !selectionStyle->style())
        return String();

    if (propertyID == CSSPropertyFontSize)
        return String::number(selectionStyle->legacyFontSize(frame().document()));
    return selectionStyle->style()->getPropertyValue(propertyID);
}

static void dispatchEditableContentChangedEvents(PassRefPtrWillBeRawPtr<Element> startRoot, PassRefPtrWillBeRawPtr<Element> endRoot)
{
    if (startRoot)
        startRoot->dispatchEvent(Event::create(EventTypeNames::webkitEditableContentChanged));
    if (endRoot && endRoot != startRoot)
        endRoot->dispatchEvent(Event::create(EventTypeNames::webkitEditableContentChanged));
}

void Editor::requestSpellcheckingAfterApplyingCommand(CompositeEditCommand* cmd)
{
    // Note: Request spell checking for and only for |ReplaceSelectionCommand|s
    // created in |Editor::replaceSelectionWithFragment()|.
    // TODO(xiaochengh): May also need to do this after dragging crbug.com/298046.
    if (cmd->editingAction() != EditActionPaste)
        return;
    if (frame().selection().isInPasswordField() || !spellChecker().isContinuousSpellCheckingEnabled())
        return;
    ASSERT(cmd->isReplaceSelectionCommand());
    const EphemeralRange& insertedRange = toReplaceSelectionCommand(cmd)->insertedRange();
    if (insertedRange.isNull())
        return;
    spellChecker().chunkAndMarkAllMisspellingsAndBadGrammar(cmd->endingSelection().rootEditableElement(), insertedRange);
}

void Editor::appliedEditing(PassRefPtrWillBeRawPtr<CompositeEditCommand> cmd)
{
    EventQueueScope scope;
    frame().document()->updateLayout();

    // Request spell checking after pasting before any further DOM change.
    requestSpellcheckingAfterApplyingCommand(cmd.get());

    EditCommandComposition* composition = cmd->composition();
    ASSERT(composition);
    dispatchEditableContentChangedEvents(composition->startingRootEditableElement(), composition->endingRootEditableElement());
    VisibleSelection newSelection(cmd->endingSelection());

    // Don't clear the typing style with this selection change. We do those things elsewhere if necessary.
    changeSelectionAfterCommand(newSelection, 0);

    if (!cmd->preservesTypingStyle())
        frame().selection().clearTypingStyle();

    // Command will be equal to last edit command only in the case of typing
    if (m_lastEditCommand.get() == cmd) {
        ASSERT(cmd->isTypingCommand());
    } else {
        // Only register a new undo command if the command passed in is
        // different from the last command
        m_lastEditCommand = cmd;
        if (UndoStack* undoStack = this->undoStack())
            undoStack->registerUndoStep(m_lastEditCommand->ensureComposition());
    }

    respondToChangedContents(newSelection);
}

void Editor::unappliedEditing(PassRefPtrWillBeRawPtr<EditCommandComposition> cmd)
{
    EventQueueScope scope;
    frame().document()->updateLayout();

    dispatchEditableContentChangedEvents(cmd->startingRootEditableElement(), cmd->endingRootEditableElement());

    VisibleSelection newSelection(cmd->startingSelection());
    newSelection.validatePositionsIfNeeded();
    if (newSelection.start().document() == frame().document() && newSelection.end().document() == frame().document())
        changeSelectionAfterCommand(newSelection, FrameSelection::CloseTyping | FrameSelection::ClearTypingStyle);

    m_lastEditCommand = nullptr;
    if (UndoStack* undoStack = this->undoStack())
        undoStack->registerRedoStep(cmd);
    respondToChangedContents(newSelection);
}

void Editor::reappliedEditing(PassRefPtrWillBeRawPtr<EditCommandComposition> cmd)
{
    EventQueueScope scope;
    frame().document()->updateLayout();

    dispatchEditableContentChangedEvents(cmd->startingRootEditableElement(), cmd->endingRootEditableElement());

    VisibleSelection newSelection(cmd->endingSelection());
    changeSelectionAfterCommand(newSelection, FrameSelection::CloseTyping | FrameSelection::ClearTypingStyle);

    m_lastEditCommand = nullptr;
    if (UndoStack* undoStack = this->undoStack())
        undoStack->registerUndoStep(cmd);
    respondToChangedContents(newSelection);
}

PassOwnPtrWillBeRawPtr<Editor> Editor::create(LocalFrame& frame)
{
    return adoptPtrWillBeNoop(new Editor(frame));
}

Editor::Editor(LocalFrame& frame)
    : m_frame(&frame)
    , m_preventRevealSelection(0)
    , m_shouldStartNewKillRingSequence(false)
    // This is off by default, since most editors want this behavior (this matches IE but not FF).
    , m_shouldStyleWithCSS(false)
    , m_killRing(adoptPtr(new KillRing))
    , m_areMarkedTextMatchesHighlighted(false)
    , m_defaultParagraphSeparator(EditorParagraphSeparatorIsDiv)
    , m_overwriteModeEnabled(false)
{
}

Editor::~Editor()
{
}

void Editor::clear()
{
    frame().inputMethodController().clear();
    m_shouldStyleWithCSS = false;
    m_defaultParagraphSeparator = EditorParagraphSeparatorIsDiv;
}

bool Editor::insertText(const String& text, KeyboardEvent* triggeringEvent)
{
    return frame().eventHandler().handleTextInputEvent(text, triggeringEvent);
}

bool Editor::insertTextWithoutSendingTextEvent(const String& text, bool selectInsertedText, TextEvent* triggeringEvent)
{
    if (text.isEmpty())
        return false;

    VisibleSelection selection = selectionForCommand(triggeringEvent);
    if (!selection.isContentEditable())
        return false;

    spellChecker().updateMarkersForWordsAffectedByEditing(isSpaceOrNewline(text[0]));

    // Get the selection to use for the event that triggered this insertText.
    // If the event handler changed the selection, we may want to use a different selection
    // that is contained in the event target.
    selection = selectionForCommand(triggeringEvent);
    if (selection.isContentEditable()) {
        if (Node* selectionStart = selection.start().anchorNode()) {
            RefPtrWillBeRawPtr<Document> document(selectionStart->document());

            // Insert the text
            TypingCommand::Options options = 0;
            if (selectInsertedText)
                options |= TypingCommand::SelectInsertedText;
            TypingCommand::insertText(*document.get(), text, selection, options, triggeringEvent && triggeringEvent->isComposition() ? TypingCommand::TextCompositionConfirm : TypingCommand::TextCompositionNone);

            // Reveal the current selection
            if (LocalFrame* editedFrame = document->frame()) {
                if (Page* page = editedFrame->page())
                    toLocalFrame(page->focusController().focusedOrMainFrame())->selection().revealSelection(ScrollAlignment::alignCenterIfNeeded);
            }
        }
    }

    return true;
}

bool Editor::insertLineBreak()
{
    if (!canEdit())
        return false;

    VisiblePosition caret = frame().selection().selection().visibleStart();
    bool alignToEdge = isEndOfEditableOrNonEditableContent(caret);
    ASSERT(frame().document());
    TypingCommand::insertLineBreak(*frame().document(), 0);
    revealSelectionAfterEditingOperation(alignToEdge ? ScrollAlignment::alignToEdgeIfNeeded : ScrollAlignment::alignCenterIfNeeded);

    return true;
}

bool Editor::insertParagraphSeparator()
{
    if (!canEdit())
        return false;

    if (!canEditRichly())
        return insertLineBreak();

    VisiblePosition caret = frame().selection().selection().visibleStart();
    bool alignToEdge = isEndOfEditableOrNonEditableContent(caret);
    ASSERT(frame().document());
    TypingCommand::insertParagraphSeparator(*frame().document(), 0);
    revealSelectionAfterEditingOperation(alignToEdge ? ScrollAlignment::alignToEdgeIfNeeded : ScrollAlignment::alignCenterIfNeeded);

    return true;
}

void Editor::cut()
{
    if (tryDHTMLCut())
        return; // DHTML did the whole operation
    if (!canCut())
        return;
    // TODO(yosin) We should use early return style here.
    if (shouldDeleteRange(selectedRange())) {
        spellChecker().updateMarkersForWordsAffectedByEditing(true);
        if (enclosingTextFormControl(frame().selection().start())) {
            String plainText = frame().selectedTextForClipboard();
            Pasteboard::generalPasteboard()->writePlainText(plainText,
                canSmartCopyOrDelete() ? Pasteboard::CanSmartReplace : Pasteboard::CannotSmartReplace);
        } else {
            writeSelectionToPasteboard();
        }
        deleteSelectionWithSmartDelete(canSmartCopyOrDelete());
    }
}

void Editor::copy()
{
    if (tryDHTMLCopy())
        return; // DHTML did the whole operation
    if (!canCopy())
        return;
    if (enclosingTextFormControl(frame().selection().start())) {
        Pasteboard::generalPasteboard()->writePlainText(frame().selectedTextForClipboard(),
            canSmartCopyOrDelete() ? Pasteboard::CanSmartReplace : Pasteboard::CannotSmartReplace);
    } else {
        Document* document = frame().document();
        if (HTMLImageElement* imageElement = imageElementFromImageDocument(document))
            writeImageNodeToPasteboard(Pasteboard::generalPasteboard(), imageElement, document->title());
        else
            writeSelectionToPasteboard();
    }
}

void Editor::paste()
{
    ASSERT(frame().document());
    if (tryDHTMLPaste(AllMimeTypes))
        return; // DHTML did the whole operation
    if (!canPaste())
        return;
    spellChecker().updateMarkersForWordsAffectedByEditing(false);
    ResourceFetcher* loader = frame().document()->fetcher();
    ResourceCacheValidationSuppressor validationSuppressor(loader);
    if (frame().selection().isContentRichlyEditable())
        pasteWithPasteboard(Pasteboard::generalPasteboard());
    else
        pasteAsPlainTextWithPasteboard(Pasteboard::generalPasteboard());
}

void Editor::pasteAsPlainText()
{
    if (tryDHTMLPaste(PlainTextOnly))
        return;
    if (!canPaste())
        return;
    spellChecker().updateMarkersForWordsAffectedByEditing(false);
    pasteAsPlainTextWithPasteboard(Pasteboard::generalPasteboard());
}

void Editor::performDelete()
{
    if (!canDelete())
        return;
    addToKillRing(selectedRange());
    deleteSelectionWithSmartDelete(canSmartCopyOrDelete());

    // clear the "start new kill ring sequence" setting, because it was set to true
    // when the selection was updated by deleting the range
    setStartNewKillRingSequence(false);
}

static void countEditingEvent(ExecutionContext* executionContext, const Event* event, UseCounter::Feature featureOnInput, UseCounter::Feature featureOnTextArea, UseCounter::Feature featureOnContentEditable, UseCounter::Feature featureOnNonNode)
{
    EventTarget* eventTarget = event->target();
    Node* node = eventTarget->toNode();
    if (!node) {
        UseCounter::count(executionContext, featureOnNonNode);
        return;
    }

    if (isHTMLInputElement(node)) {
        UseCounter::count(executionContext, featureOnInput);
        return;
    }

    if (isHTMLTextAreaElement(node)) {
        UseCounter::count(executionContext, featureOnTextArea);
        return;
    }

    HTMLTextFormControlElement* control = enclosingTextFormControl(node);
    if (isHTMLInputElement(control)) {
        UseCounter::count(executionContext, featureOnInput);
        return;
    }

    if (isHTMLTextAreaElement(control)) {
        UseCounter::count(executionContext, featureOnTextArea);
        return;
    }

    UseCounter::count(executionContext, featureOnContentEditable);
}

void Editor::countEvent(ExecutionContext* executionContext, const Event* event)
{
    if (!executionContext)
        return;

    if (event->type() == EventTypeNames::textInput) {
        countEditingEvent(executionContext, event,
            UseCounter::TextInputEventOnInput,
            UseCounter::TextInputEventOnTextArea,
            UseCounter::TextInputEventOnContentEditable,
            UseCounter::TextInputEventOnNotNode);
        return;
    }

    if (event->type() == EventTypeNames::webkitBeforeTextInserted) {
        countEditingEvent(executionContext, event,
            UseCounter::WebkitBeforeTextInsertedOnInput,
            UseCounter::WebkitBeforeTextInsertedOnTextArea,
            UseCounter::WebkitBeforeTextInsertedOnContentEditable,
            UseCounter::WebkitBeforeTextInsertedOnNotNode);
        return;
    }

    if (event->type() == EventTypeNames::webkitEditableContentChanged) {
        countEditingEvent(executionContext, event,
            UseCounter::WebkitEditableContentChangedOnInput,
            UseCounter::WebkitEditableContentChangedOnTextArea,
            UseCounter::WebkitEditableContentChangedOnContentEditable,
            UseCounter::WebkitEditableContentChangedOnNotNode);
    }
}

void Editor::copyImage(const HitTestResult& result)
{
    writeImageNodeToPasteboard(Pasteboard::generalPasteboard(), result.innerNodeOrImageMapImage(), result.altDisplayString());
}

bool Editor::canUndo()
{
    if (UndoStack* undoStack = this->undoStack())
        return undoStack->canUndo();
    return false;
}

void Editor::undo()
{
    if (UndoStack* undoStack = this->undoStack())
        undoStack->undo();
}

bool Editor::canRedo()
{
    if (UndoStack* undoStack = this->undoStack())
        return undoStack->canRedo();
    return false;
}

void Editor::redo()
{
    if (UndoStack* undoStack = this->undoStack())
        undoStack->redo();
}

void Editor::setBaseWritingDirection(WritingDirection direction)
{
    Element* focusedElement = frame().document()->focusedElement();
    if (isHTMLTextFormControlElement(focusedElement)) {
        if (direction == NaturalWritingDirection)
            return;
        focusedElement->setAttribute(dirAttr, direction == LeftToRightWritingDirection ? "ltr" : "rtl");
        focusedElement->dispatchInputEvent();
        frame().document()->updateLayoutTreeIfNeeded();
        return;
    }

    RefPtrWillBeRawPtr<MutableStylePropertySet> style = MutableStylePropertySet::create(HTMLQuirksMode);
    style->setProperty(CSSPropertyDirection, direction == LeftToRightWritingDirection ? "ltr" : direction == RightToLeftWritingDirection ? "rtl" : "inherit", false);
    applyParagraphStyleToSelection(style.get(), EditActionSetWritingDirection);
}

void Editor::revealSelectionAfterEditingOperation(const ScrollAlignment& alignment, RevealExtentOption revealExtentOption)
{
    if (m_preventRevealSelection)
        return;

    frame().selection().revealSelection(alignment, revealExtentOption);
}

void Editor::transpose()
{
    if (!canEdit())
        return;

    VisibleSelection selection = frame().selection().selection();
    if (!selection.isCaret())
        return;

    // Make a selection that goes back one character and forward two characters.
    VisiblePosition caret = selection.visibleStart();
    VisiblePosition next = isEndOfParagraph(caret) ? caret : nextPositionOf(caret);
    VisiblePosition previous = previousPositionOf(next);
    if (next.deepEquivalent() == previous.deepEquivalent())
        return;
    previous = previousPositionOf(previous);
    if (!inSameParagraph(next, previous))
        return;
    const EphemeralRange range = makeRange(previous, next);
    if (range.isNull())
        return;
    VisibleSelection newSelection(range);

    // Transpose the two characters.
    String text = plainText(range);
    if (text.length() != 2)
        return;
    String transposed = text.right(1) + text.left(1);

    // Select the two characters.
    if (!equalSelectionsInDOMTree(newSelection, frame().selection().selection()))
        frame().selection().setSelection(newSelection);

    // Insert the transposed characters.
    replaceSelectionWithText(transposed, false, false);
}

void Editor::addToKillRing(const EphemeralRange& range)
{
    if (m_shouldStartNewKillRingSequence)
        killRing().startNewSequence();

    String text = plainText(range);
    killRing().append(text);
    m_shouldStartNewKillRingSequence = false;
}

void Editor::changeSelectionAfterCommand(const VisibleSelection& newSelection,  FrameSelection::SetSelectionOptions options)
{
    // If the new selection is orphaned, then don't update the selection.
    if (newSelection.start().isOrphan() || newSelection.end().isOrphan())
        return;

    // See <rdar://problem/5729315> Some shouldChangeSelectedDOMRange contain Ranges for selections that are no longer valid
    bool selectionDidNotChangeDOMPosition = equalSelectionsInDOMTree(newSelection, frame().selection().selection());
    frame().selection().setSelection(newSelection, options);

    // Some editing operations change the selection visually without affecting its position within the DOM.
    // For example when you press return in the following (the caret is marked by ^):
    // <div contentEditable="true"><div>^Hello</div></div>
    // WebCore inserts <div><br></div> *before* the current block, which correctly moves the paragraph down but which doesn't
    // change the caret's DOM position (["hello", 0]). In these situations the above FrameSelection::setSelection call
    // does not call EditorClient::respondToChangedSelection(), which, on the Mac, sends selection change notifications and
    // starts a new kill ring sequence, but we want to do these things (matches AppKit).
    if (selectionDidNotChangeDOMPosition)
        client().respondToChangedSelection(m_frame, frame().selection().selectionType());
}

IntRect Editor::firstRectForRange(const EphemeralRange& range) const
{
    LayoutUnit extraWidthToEndOfLine;
    ASSERT(range.isNotNull());

    IntRect startCaretRect = RenderedPosition(createVisiblePosition(range.startPosition()).deepEquivalent(), TextAffinity::Downstream).absoluteRect(&extraWidthToEndOfLine);
    if (startCaretRect.isEmpty())
        return IntRect();

    IntRect endCaretRect = RenderedPosition(createVisiblePosition(range.endPosition()).deepEquivalent(), TextAffinity::Upstream).absoluteRect();
    if (endCaretRect.isEmpty())
        return IntRect();

    if (startCaretRect.y() == endCaretRect.y()) {
        // start and end are on the same line
        return IntRect(std::min(startCaretRect.x(), endCaretRect.x()),
            startCaretRect.y(),
            abs(endCaretRect.x() - startCaretRect.x()),
            std::max(startCaretRect.height(), endCaretRect.height()));
    }

    // start and end aren't on the same line, so go from start to the end of its line
    return IntRect(startCaretRect.x(),
        startCaretRect.y(),
        startCaretRect.width() + extraWidthToEndOfLine,
        startCaretRect.height());
}

IntRect Editor::firstRectForRange(const Range* range) const
{
    ASSERT(range);
    return firstRectForRange(EphemeralRange(range));
}

void Editor::computeAndSetTypingStyle(StylePropertySet* style, EditAction editingAction)
{
    if (!style || style->isEmpty()) {
        frame().selection().clearTypingStyle();
        return;
    }

    // Calculate the current typing style.
    RefPtrWillBeRawPtr<EditingStyle> typingStyle = nullptr;
    if (frame().selection().typingStyle()) {
        typingStyle = frame().selection().typingStyle()->copy();
        typingStyle->overrideWithStyle(style);
    } else {
        typingStyle = EditingStyle::create(style);
    }

    typingStyle->prepareToApplyAt(frame().selection().selection().visibleStart().deepEquivalent(), EditingStyle::PreserveWritingDirection);

    // Handle block styles, substracting these from the typing style.
    RefPtrWillBeRawPtr<EditingStyle> blockStyle = typingStyle->extractAndRemoveBlockProperties();
    if (!blockStyle->isEmpty()) {
        ASSERT(frame().document());
        ApplyStyleCommand::create(*frame().document(), blockStyle.get(), editingAction)->apply();
    }

    // Set the remaining style as the typing style.
    frame().selection().setTypingStyle(typingStyle);
}

bool Editor::findString(const String& target, FindOptions options)
{
    VisibleSelection selection = frame().selection().selection();

    // TODO(yosin) We should make |findRangeOfString()| to return
    // |EphemeralRange| rather than|Range| object.
    RefPtrWillBeRawPtr<Range> resultRange = findRangeOfString(target, EphemeralRange(selection.start(), selection.end()), static_cast<FindOptions>(options | FindAPICall));

    if (!resultRange)
        return false;

    frame().selection().setSelection(VisibleSelection(EphemeralRange(resultRange.get())));
    frame().selection().revealSelection();
    return true;
}

PassRefPtrWillBeRawPtr<Range> Editor::findStringAndScrollToVisible(const String& target, Range* previousMatch, FindOptions options)
{
    RefPtrWillBeRawPtr<Range> nextMatch = findRangeOfString(target, EphemeralRangeInFlatTree(previousMatch), options);
    if (!nextMatch)
        return nullptr;

    nextMatch->firstNode()->layoutObject()->scrollRectToVisible(LayoutRect(nextMatch->boundingBox()),
        ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded, UserScroll);

    return nextMatch.release();
}

// TODO(yosin) We should return |EphemeralRange| rather than |Range|. We use
// |Range| object for checking whether start and end position crossing shadow
// boundaries, however we can do it without |Range| object.
template <typename Strategy>
static PassRefPtrWillBeRawPtr<Range> findStringBetweenPositions(const String& target, const EphemeralRangeTemplate<Strategy>& referenceRange, FindOptions options)
{
    EphemeralRangeTemplate<Strategy> searchRange(referenceRange);

    bool forward = !(options & Backwards);

    while (true) {
        EphemeralRangeTemplate<Strategy> resultRange = findPlainText(searchRange, target, options);
        if (resultRange.isCollapsed())
            return nullptr;

        RefPtrWillBeRawPtr<Range> rangeObject = Range::create(resultRange.document(), toPositionInDOMTree(resultRange.startPosition()), toPositionInDOMTree(resultRange.endPosition()));
        if (!rangeObject->collapsed())
            return rangeObject.release();

        // Found text spans over multiple TreeScopes. Since it's impossible to
        // return such section as a Range, we skip this match and seek for the
        // next occurrence.
        // TODO(yosin) Handle this case.
        if (forward) {
            // TODO(yosin) We should use |PositionMoveType::Character|
            // for |nextPositionOf()|.
            searchRange = EphemeralRangeTemplate<Strategy>(nextPositionOf(resultRange.startPosition(), PositionMoveType::CodePoint), searchRange.endPosition());
        } else {
            // TODO(yosin) We should use |PositionMoveType::Character|
            // for |previousPositionOf()|.
            searchRange = EphemeralRangeTemplate<Strategy>(searchRange.startPosition(), previousPositionOf(resultRange.endPosition(), PositionMoveType::CodePoint));
        }
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

template <typename Strategy>
static PassRefPtrWillBeRawPtr<Range> findRangeOfStringAlgorithm(Document& document, const String& target, const EphemeralRangeTemplate<Strategy>& referenceRange, FindOptions options)
{
    if (target.isEmpty())
        return nullptr;

    // Start from an edge of the reference range. Which edge is used depends on
    // whether we're searching forward or backward, and whether startInSelection
    // is set.
    EphemeralRangeTemplate<Strategy> documentRange = EphemeralRangeTemplate<Strategy>::rangeOfContents(document);
    EphemeralRangeTemplate<Strategy> searchRange(documentRange);

    bool forward = !(options & Backwards);
    bool startInReferenceRange = false;
    if (referenceRange.isNotNull()) {
        startInReferenceRange = options & StartInSelection;
        if (forward && startInReferenceRange)
            searchRange = EphemeralRangeTemplate<Strategy>(referenceRange.startPosition(), documentRange.endPosition());
        else if (forward)
            searchRange = EphemeralRangeTemplate<Strategy>(referenceRange.endPosition(), documentRange.endPosition());
        else if (startInReferenceRange)
            searchRange = EphemeralRangeTemplate<Strategy>(documentRange.startPosition(), referenceRange.endPosition());
        else
            searchRange = EphemeralRangeTemplate<Strategy>(documentRange.startPosition(), referenceRange.startPosition());
    }

    RefPtrWillBeRawPtr<Range> resultRange = findStringBetweenPositions(target, searchRange, options);

    // If we started in the reference range and the found range exactly matches
    // the reference range, find again. Build a selection with the found range
    // to remove collapsed whitespace. Compare ranges instead of selection
    // objects to ignore the way that the current selection was made.
    if (resultRange && startInReferenceRange && normalizeRange(EphemeralRangeTemplate<Strategy>(resultRange.get())) == referenceRange) {
        if (forward)
            searchRange = EphemeralRangeTemplate<Strategy>(fromPositionInDOMTree<Strategy>(resultRange->endPosition()), searchRange.endPosition());
        else
            searchRange = EphemeralRangeTemplate<Strategy>(searchRange.startPosition(), fromPositionInDOMTree<Strategy>(resultRange->startPosition()));
        resultRange = findStringBetweenPositions(target, searchRange, options);
    }

    if (!resultRange && options & WrapAround)
        return findStringBetweenPositions(target, documentRange, options);

    return resultRange.release();
}

PassRefPtrWillBeRawPtr<Range> Editor::findRangeOfString(const String& target, const EphemeralRange& reference, FindOptions options)
{
    return findRangeOfStringAlgorithm<EditingStrategy>(*frame().document(), target, reference, options);
}

PassRefPtrWillBeRawPtr<Range> Editor::findRangeOfString(const String& target, const EphemeralRangeInFlatTree& reference, FindOptions options)
{
    return findRangeOfStringAlgorithm<EditingInFlatTreeStrategy>(*frame().document(), target, reference, options);
}

void Editor::setMarkedTextMatchesAreHighlighted(bool flag)
{
    if (flag == m_areMarkedTextMatchesHighlighted)
        return;

    m_areMarkedTextMatchesHighlighted = flag;
    frame().document()->markers().repaintMarkers(DocumentMarker::TextMatch);
}

void Editor::respondToChangedSelection(const VisibleSelection& oldSelection, FrameSelection::SetSelectionOptions options)
{
    spellChecker().respondToChangedSelection(oldSelection, options);
    frame().inputMethodController().cancelCompositionIfSelectionIsInvalid();
    notifyComponentsOnChangedSelection(oldSelection, options);
}

SpellChecker& Editor::spellChecker() const
{
    return frame().spellChecker();
}

void Editor::toggleOverwriteModeEnabled()
{
    m_overwriteModeEnabled = !m_overwriteModeEnabled;
    frame().selection().setShouldShowBlockCursor(m_overwriteModeEnabled);
}

void Editor::tidyUpHTMLStructure(Document& document)
{
    // hasEditableStyle() needs up-to-date ComputedStyle.
    document.updateLayoutTreeIfNeeded();
    bool needsValidStructure = document.hasEditableStyle() || (document.documentElement() && document.documentElement()->hasEditableStyle());
    if (!needsValidStructure)
        return;
    RefPtrWillBeRawPtr<Element> existingHead = nullptr;
    RefPtrWillBeRawPtr<Element> existingBody = nullptr;
    Element* currentRoot = document.documentElement();
    if (currentRoot) {
        if (isHTMLHtmlElement(currentRoot))
            return;
        if (isHTMLHeadElement(currentRoot))
            existingHead = currentRoot;
        else if (isHTMLBodyElement(currentRoot))
            existingBody = currentRoot;
        else if (isHTMLFrameSetElement(currentRoot))
            existingBody = currentRoot;
    }
    // We ensure only "the root is <html>."
    // documentElement as rootEditableElement is problematic.  So we move
    // non-<html> root elements under <body>, and the <body> works as
    // rootEditableElement.
    document.addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel, "document.execCommand() doesn't work with an invalid HTML structure. It is corrected automatically."));

    RefPtrWillBeRawPtr<Element> root = HTMLHtmlElement::create(document);
    if (existingHead)
        root->appendChild(existingHead.release());
    RefPtrWillBeRawPtr<Element> body = nullptr;
    if (existingBody)
        body = existingBody.release();
    else
        body = HTMLBodyElement::create(document);
    if (document.documentElement())
        body->appendChild(document.documentElement());
    root->appendChild(body.release());
    ASSERT(!document.documentElement());
    document.appendChild(root.release());

    // TODO(tkent): Should we check and move Text node children of <html>?
}

DEFINE_TRACE(Editor)
{
    visitor->trace(m_frame);
    visitor->trace(m_lastEditCommand);
    visitor->trace(m_mark);
}

} // namespace blink
