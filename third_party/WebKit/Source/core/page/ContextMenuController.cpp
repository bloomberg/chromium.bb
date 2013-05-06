/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Igalia S.L
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
#include "core/page/ContextMenuController.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/Event.h"
#include "core/dom/EventNames.h"
#include "core/dom/ExceptionCodePlaceholder.h"
#include "core/dom/MouseEvent.h"
#include "core/dom/Node.h"
#include "core/dom/UserTypingGestureIndicator.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/ReplaceSelectionCommand.h"
#include "core/editing/TextIterator.h"
#include "core/editing/TypingCommand.h"
#include "core/editing/markup.h"
#include "core/history/BackForwardController.h"
#include "core/html/HTMLFormElement.h"
#include "core/inspector/InspectorController.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FormState.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/NavigationAction.h"
#include "core/page/Chrome.h"
#include "core/page/ContextMenuClient.h"
#include "core/page/ContextMenuProvider.h"
#include "core/page/EditorClient.h"
#include "core/page/EventHandler.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/page/WindowFeatures.h"
#include "core/platform/ContextMenu.h"
#include "core/platform/ContextMenuItem.h"
#include "core/platform/LocalizedStrings.h"
#include "core/platform/PlatformEvent.h"
#include "core/platform/network/ResourceRequest.h"
#include "core/rendering/HitTestRequest.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderObject.h"
#include <wtf/unicode/CharacterNames.h>
#include <wtf/unicode/Unicode.h>

using namespace WTF;
using namespace Unicode;

namespace WebCore {

ContextMenuController::ContextMenuController(Page* page, ContextMenuClient* client)
    : m_page(page)
    , m_client(client)
{
    ASSERT_ARG(page, page);
    ASSERT_ARG(client, client);
}

ContextMenuController::~ContextMenuController()
{
}

PassOwnPtr<ContextMenuController> ContextMenuController::create(Page* page, ContextMenuClient* client)
{
    return adoptPtr(new ContextMenuController(page, client));
}

void ContextMenuController::clearContextMenu()
{
    m_contextMenu.clear();
    if (m_menuProvider)
        m_menuProvider->contextMenuCleared();
    m_menuProvider = 0;
}

void ContextMenuController::handleContextMenuEvent(Event* event)
{
    m_contextMenu = createContextMenu(event);
    if (!m_contextMenu)
        return;

    populate();

    showContextMenu(event);
}

static PassOwnPtr<ContextMenuItem> separatorItem()
{
    return adoptPtr(new ContextMenuItem(SeparatorType, ContextMenuItemTagNoAction, String()));
}

void ContextMenuController::showContextMenu(Event* event, PassRefPtr<ContextMenuProvider> menuProvider)
{
    m_menuProvider = menuProvider;

    m_contextMenu = createContextMenu(event);
    if (!m_contextMenu) {
        clearContextMenu();
        return;
    }

    m_menuProvider->populateContextMenu(m_contextMenu.get());
    if (m_hitTestResult.isSelected()) {
        appendItem(*separatorItem(), m_contextMenu.get());
        populate();
    }
    showContextMenu(event);
}

PassOwnPtr<ContextMenu> ContextMenuController::createContextMenu(Event* event)
{
    ASSERT(event);
    
    if (!event->isMouseEvent())
        return nullptr;

    MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
    HitTestResult result(mouseEvent->absoluteLocation());

    if (Frame* frame = event->target()->toNode()->document()->frame())
        result = frame->eventHandler()->hitTestResultAtPoint(mouseEvent->absoluteLocation());

    if (!result.innerNonSharedNode())
        return nullptr;

    m_hitTestResult = result;

    return adoptPtr(new ContextMenu);
}

void ContextMenuController::showContextMenu(Event* event)
{
    addInspectElementItem();

    m_contextMenu = m_client->customizeMenu(m_contextMenu.release());
    event->setDefaultHandled();
}

static void openNewWindow(const KURL& urlToLoad, Frame* frame)
{
    if (Page* oldPage = frame->page()) {
        FrameLoadRequest request(frame->document()->securityOrigin(), ResourceRequest(urlToLoad, frame->loader()->outgoingReferrer()));
        Page* newPage = oldPage;
        if (!frame->settings() || frame->settings()->supportsMultipleWindows()) {
            newPage = oldPage->chrome()->createWindow(frame, request, WindowFeatures(), NavigationAction(request.resourceRequest()));
            if (!newPage)
                return;
            newPage->chrome()->show();
        }
        newPage->mainFrame()->loader()->loadFrameRequest(request, false, 0, 0, MaybeSendReferrer);
    }
}

void ContextMenuController::contextMenuItemSelected(const ContextMenuItem* item)
{
    ASSERT(item->type() == ActionType || item->type() == CheckableActionType);

    if (item->action() >= ContextMenuItemBaseApplicationTag)
        return;

    if (item->action() >= ContextMenuItemBaseCustomTag) {
        ASSERT(m_menuProvider);
        m_menuProvider->contextMenuItemSelected(item);
        return;
    }

    Frame* frame = m_hitTestResult.innerNonSharedNode()->document()->frame();
    if (!frame)
        return;

    switch (item->action()) {
    case ContextMenuItemTagOpenLinkInNewWindow:
        openNewWindow(m_hitTestResult.absoluteLinkURL(), frame);
        break;
    case ContextMenuItemTagDownloadLinkToDisk:
        break;
    case ContextMenuItemTagCopyLinkToClipboard:
        frame->editor()->copyURL(m_hitTestResult.absoluteLinkURL(), m_hitTestResult.textContent());
        break;
    case ContextMenuItemTagOpenImageInNewWindow:
        openNewWindow(m_hitTestResult.absoluteImageURL(), frame);
        break;
    case ContextMenuItemTagDownloadImageToDisk:
        break;
    case ContextMenuItemTagCopyImageToClipboard:
        // FIXME: The Pasteboard class is not written yet
        // For now, call into the client. This is temporary!
        frame->editor()->copyImage(m_hitTestResult);
        break;
    case ContextMenuItemTagOpenMediaInNewWindow:
        openNewWindow(m_hitTestResult.absoluteMediaURL(), frame);
        break;
    case ContextMenuItemTagCopyMediaLinkToClipboard:
        frame->editor()->copyURL(m_hitTestResult.absoluteMediaURL(), m_hitTestResult.textContent());
        break;
    case ContextMenuItemTagToggleMediaControls:
        m_hitTestResult.toggleMediaControlsDisplay();
        break;
    case ContextMenuItemTagToggleMediaLoop:
        m_hitTestResult.toggleMediaLoopPlayback();
        break;
    case ContextMenuItemTagEnterVideoFullscreen:
        m_hitTestResult.enterFullscreenForVideo();
        break;
    case ContextMenuItemTagMediaPlayPause:
        m_hitTestResult.toggleMediaPlayState();
        break;
    case ContextMenuItemTagMediaMute:
        m_hitTestResult.toggleMediaMuteState();
        break;
    case ContextMenuItemTagOpenFrameInNewWindow: {
        DocumentLoader* loader = frame->loader()->documentLoader();
        if (!loader->unreachableURL().isEmpty())
            openNewWindow(loader->unreachableURL(), frame);
        else
            openNewWindow(loader->url(), frame);
        break;
    }
    case ContextMenuItemTagCopy:
        frame->editor()->copy();
        break;
    case ContextMenuItemTagGoBack:
        if (Page* page = frame->page())
            page->backForward()->goBackOrForward(-1);
        break;
    case ContextMenuItemTagGoForward:
        if (Page* page = frame->page())
            page->backForward()->goBackOrForward(1);
        break;
    case ContextMenuItemTagStop:
        frame->loader()->stop();
        break;
    case ContextMenuItemTagReload:
        frame->loader()->reload();
        break;
    case ContextMenuItemTagCut:
        frame->editor()->command("Cut").execute();
        break;
    case ContextMenuItemTagPaste:
        frame->editor()->command("Paste").execute();
        break;
    case ContextMenuItemTagSpellingGuess: {
        FrameSelection* frameSelection = frame->selection();
        if (frame->editor()->shouldInsertText(item->title(), frameSelection->toNormalizedRange().get(), EditorInsertActionPasted)) {
            Document* document = frame->document();
            ReplaceSelectionCommand::CommandOptions replaceOptions = ReplaceSelectionCommand::MatchStyle | ReplaceSelectionCommand::PreventNesting | ReplaceSelectionCommand::SelectReplacement;
            ASSERT(frame->editor()->selectedText().length());
            RefPtr<ReplaceSelectionCommand> command = ReplaceSelectionCommand::create(document, createFragmentFromMarkup(document, item->title(), ""), replaceOptions);
            applyCommand(command);
            frameSelection->revealSelection(ScrollAlignment::alignToEdgeIfNeeded);
        }
        break;
    }
    case ContextMenuItemTagIgnoreSpelling:
        frame->editor()->ignoreSpelling();
        break;
    case ContextMenuItemTagLearnSpelling:
        frame->editor()->learnSpelling();
        break;
    case ContextMenuItemTagSearchWeb:
        break;
    case ContextMenuItemTagLookUpInDictionary:
        break;
    case ContextMenuItemTagOpenLink:
        if (Frame* targetFrame = m_hitTestResult.targetFrame())
            targetFrame->loader()->loadFrameRequest(FrameLoadRequest(frame->document()->securityOrigin(), ResourceRequest(m_hitTestResult.absoluteLinkURL(), frame->loader()->outgoingReferrer())), false, 0, 0, MaybeSendReferrer);
        else
            openNewWindow(m_hitTestResult.absoluteLinkURL(), frame);
        break;
    case ContextMenuItemTagOpenLinkInThisWindow:
        frame->loader()->loadFrameRequest(FrameLoadRequest(frame->document()->securityOrigin(), ResourceRequest(m_hitTestResult.absoluteLinkURL(), frame->loader()->outgoingReferrer())), false, 0, 0, MaybeSendReferrer);
        break;
    case ContextMenuItemTagBold:
        frame->editor()->command("ToggleBold").execute();
        break;
    case ContextMenuItemTagItalic:
        frame->editor()->command("ToggleItalic").execute();
        break;
    case ContextMenuItemTagUnderline:
        frame->editor()->toggleUnderline();
        break;
    case ContextMenuItemTagOutline:
        // We actually never enable this because CSS does not have a way to specify an outline font,
        // which may make this difficult to implement. Maybe a special case of text-shadow?
        break;
    case ContextMenuItemTagStartSpeaking:
        break;
    case ContextMenuItemTagStopSpeaking:
        break;
    case ContextMenuItemTagDefaultDirection:
        frame->editor()->setBaseWritingDirection(NaturalWritingDirection);
        break;
    case ContextMenuItemTagLeftToRight:
        frame->editor()->setBaseWritingDirection(LeftToRightWritingDirection);
        break;
    case ContextMenuItemTagRightToLeft:
        frame->editor()->setBaseWritingDirection(RightToLeftWritingDirection);
        break;
    case ContextMenuItemTagTextDirectionDefault:
        frame->editor()->command("MakeTextWritingDirectionNatural").execute();
        break;
    case ContextMenuItemTagTextDirectionLeftToRight:
        frame->editor()->command("MakeTextWritingDirectionLeftToRight").execute();
        break;
    case ContextMenuItemTagTextDirectionRightToLeft:
        frame->editor()->command("MakeTextWritingDirectionRightToLeft").execute();
        break;
    case ContextMenuItemTagShowSpellingPanel:
        frame->editor()->showSpellingGuessPanel();
        break;
    case ContextMenuItemTagCheckSpelling:
        frame->editor()->advanceToNextMisspelling();
        break;
    case ContextMenuItemTagCheckSpellingWhileTyping:
        frame->editor()->toggleContinuousSpellChecking();
        break;
    case ContextMenuItemTagCheckGrammarWithSpelling:
        frame->editor()->toggleGrammarChecking();
        break;
#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    case ContextMenuItemTagShowSubstitutions:
        frame->editor()->showSubstitutionsPanel();
        break;
    case ContextMenuItemTagSmartCopyPaste:
        frame->editor()->toggleSmartInsertDelete();
        break;
    case ContextMenuItemTagSmartQuotes:
        frame->editor()->toggleAutomaticQuoteSubstitution();
        break;
    case ContextMenuItemTagSmartDashes:
        frame->editor()->toggleAutomaticDashSubstitution();
        break;
    case ContextMenuItemTagSmartLinks:
        frame->editor()->toggleAutomaticLinkDetection();
        break;
    case ContextMenuItemTagTextReplacement:
        frame->editor()->toggleAutomaticTextReplacement();
        break;
    case ContextMenuItemTagCorrectSpellingAutomatically:
        frame->editor()->toggleAutomaticSpellingCorrection();
        break;
#endif
    case ContextMenuItemTagInspectElement:
        if (Page* page = frame->page())
            page->inspectorController()->inspect(m_hitTestResult.innerNonSharedNode());
        break;
    case ContextMenuItemTagDictationAlternative:
        frame->editor()->applyDictationAlternativelternative(item->title());
        break;
    default:
        break;
    }
}

void ContextMenuController::appendItem(ContextMenuItem& menuItem, ContextMenu* parentMenu)
{
    checkOrEnableIfNeeded(menuItem);
    if (parentMenu)
        parentMenu->appendItem(menuItem);
}

void ContextMenuController::createAndAppendFontSubMenu(ContextMenuItem& fontMenuItem)
{
    ContextMenu fontMenu;

    ContextMenuItem bold(CheckableActionType, ContextMenuItemTagBold, contextMenuItemTagBold());
    ContextMenuItem italic(CheckableActionType, ContextMenuItemTagItalic, contextMenuItemTagItalic());
    ContextMenuItem underline(CheckableActionType, ContextMenuItemTagUnderline, contextMenuItemTagUnderline());
    ContextMenuItem outline(ActionType, ContextMenuItemTagOutline, contextMenuItemTagOutline());
    appendItem(bold, &fontMenu);
    appendItem(italic, &fontMenu);
    appendItem(underline, &fontMenu);
    appendItem(outline, &fontMenu);

    fontMenuItem.setSubMenu(&fontMenu);
}


void ContextMenuController::createAndAppendSpellingAndGrammarSubMenu(ContextMenuItem& spellingAndGrammarMenuItem)
{
    ContextMenu spellingAndGrammarMenu;

    ContextMenuItem showSpellingPanel(ActionType, ContextMenuItemTagShowSpellingPanel, 
        contextMenuItemTagShowSpellingPanel(true));
    ContextMenuItem checkSpelling(ActionType, ContextMenuItemTagCheckSpelling, 
        contextMenuItemTagCheckSpelling());
    ContextMenuItem checkAsYouType(CheckableActionType, ContextMenuItemTagCheckSpellingWhileTyping, 
        contextMenuItemTagCheckSpellingWhileTyping());
    ContextMenuItem grammarWithSpelling(CheckableActionType, ContextMenuItemTagCheckGrammarWithSpelling, 
        contextMenuItemTagCheckGrammarWithSpelling());

    appendItem(showSpellingPanel, &spellingAndGrammarMenu);
    appendItem(checkSpelling, &spellingAndGrammarMenu);
    appendItem(checkAsYouType, &spellingAndGrammarMenu);
    appendItem(grammarWithSpelling, &spellingAndGrammarMenu);

    spellingAndGrammarMenuItem.setSubMenu(&spellingAndGrammarMenu);
}

void ContextMenuController::createAndAppendWritingDirectionSubMenu(ContextMenuItem& writingDirectionMenuItem)
{
    ContextMenu writingDirectionMenu;

    ContextMenuItem defaultItem(ActionType, ContextMenuItemTagDefaultDirection, 
        contextMenuItemTagDefaultDirection());
    ContextMenuItem ltr(CheckableActionType, ContextMenuItemTagLeftToRight, contextMenuItemTagLeftToRight());
    ContextMenuItem rtl(CheckableActionType, ContextMenuItemTagRightToLeft, contextMenuItemTagRightToLeft());

    appendItem(defaultItem, &writingDirectionMenu);
    appendItem(ltr, &writingDirectionMenu);
    appendItem(rtl, &writingDirectionMenu);

    writingDirectionMenuItem.setSubMenu(&writingDirectionMenu);
}

void ContextMenuController::createAndAppendTextDirectionSubMenu(ContextMenuItem& textDirectionMenuItem)
{
    ContextMenu textDirectionMenu;

    ContextMenuItem defaultItem(ActionType, ContextMenuItemTagTextDirectionDefault, contextMenuItemTagDefaultDirection());
    ContextMenuItem ltr(CheckableActionType, ContextMenuItemTagTextDirectionLeftToRight, contextMenuItemTagLeftToRight());
    ContextMenuItem rtl(CheckableActionType, ContextMenuItemTagTextDirectionRightToLeft, contextMenuItemTagRightToLeft());

    appendItem(defaultItem, &textDirectionMenu);
    appendItem(ltr, &textDirectionMenu);
    appendItem(rtl, &textDirectionMenu);

    textDirectionMenuItem.setSubMenu(&textDirectionMenu);
}

static bool selectionContainsPossibleWord(Frame* frame)
{
    // Current algorithm: look for a character that's not just a separator.
    for (TextIterator it(frame->selection()->toNormalizedRange().get()); !it.atEnd(); it.advance()) {
        int length = it.length();
        for (int i = 0; i < length; ++i)
            if (!(category(it.characterAt(i)) & (Separator_Space | Separator_Line | Separator_Paragraph)))
                return true;
    }
    return false;
}

void ContextMenuController::populate()
{
    ContextMenuItem OpenLinkItem(ActionType, ContextMenuItemTagOpenLink, contextMenuItemTagOpenLink());
    ContextMenuItem OpenLinkInNewWindowItem(ActionType, ContextMenuItemTagOpenLinkInNewWindow, 
        contextMenuItemTagOpenLinkInNewWindow());
    ContextMenuItem DownloadFileItem(ActionType, ContextMenuItemTagDownloadLinkToDisk, 
        contextMenuItemTagDownloadLinkToDisk());
    ContextMenuItem CopyLinkItem(ActionType, ContextMenuItemTagCopyLinkToClipboard, 
        contextMenuItemTagCopyLinkToClipboard());
    ContextMenuItem OpenImageInNewWindowItem(ActionType, ContextMenuItemTagOpenImageInNewWindow, 
        contextMenuItemTagOpenImageInNewWindow());
    ContextMenuItem DownloadImageItem(ActionType, ContextMenuItemTagDownloadImageToDisk, 
        contextMenuItemTagDownloadImageToDisk());
    ContextMenuItem CopyImageItem(ActionType, ContextMenuItemTagCopyImageToClipboard, 
        contextMenuItemTagCopyImageToClipboard());
    ContextMenuItem OpenMediaInNewWindowItem(ActionType, ContextMenuItemTagOpenMediaInNewWindow, String());
    ContextMenuItem CopyMediaLinkItem(ActionType, ContextMenuItemTagCopyMediaLinkToClipboard, 
        String());
    ContextMenuItem MediaPlayPause(ActionType, ContextMenuItemTagMediaPlayPause, 
        contextMenuItemTagMediaPlay());
    ContextMenuItem MediaMute(ActionType, ContextMenuItemTagMediaMute, 
        contextMenuItemTagMediaMute());
    ContextMenuItem ToggleMediaControls(CheckableActionType, ContextMenuItemTagToggleMediaControls, 
        contextMenuItemTagToggleMediaControls());
    ContextMenuItem ToggleMediaLoop(CheckableActionType, ContextMenuItemTagToggleMediaLoop, 
        contextMenuItemTagToggleMediaLoop());
    ContextMenuItem EnterVideoFullscreen(ActionType, ContextMenuItemTagEnterVideoFullscreen, 
        contextMenuItemTagEnterVideoFullscreen());
    ContextMenuItem SearchWebItem(ActionType, ContextMenuItemTagSearchWeb, contextMenuItemTagSearchWeb());
    ContextMenuItem CopyItem(ActionType, ContextMenuItemTagCopy, contextMenuItemTagCopy());
    ContextMenuItem BackItem(ActionType, ContextMenuItemTagGoBack, contextMenuItemTagGoBack());
    ContextMenuItem ForwardItem(ActionType, ContextMenuItemTagGoForward,  contextMenuItemTagGoForward());
    ContextMenuItem StopItem(ActionType, ContextMenuItemTagStop, contextMenuItemTagStop());
    ContextMenuItem ReloadItem(ActionType, ContextMenuItemTagReload, contextMenuItemTagReload());
    ContextMenuItem OpenFrameItem(ActionType, ContextMenuItemTagOpenFrameInNewWindow, 
        contextMenuItemTagOpenFrameInNewWindow());
    ContextMenuItem NoGuessesItem(ActionType, ContextMenuItemTagNoGuessesFound, 
        contextMenuItemTagNoGuessesFound());
    ContextMenuItem IgnoreSpellingItem(ActionType, ContextMenuItemTagIgnoreSpelling, 
        contextMenuItemTagIgnoreSpelling());
    ContextMenuItem LearnSpellingItem(ActionType, ContextMenuItemTagLearnSpelling, 
        contextMenuItemTagLearnSpelling());
    ContextMenuItem IgnoreGrammarItem(ActionType, ContextMenuItemTagIgnoreGrammar, 
        contextMenuItemTagIgnoreGrammar());
    ContextMenuItem CutItem(ActionType, ContextMenuItemTagCut, contextMenuItemTagCut());
    ContextMenuItem PasteItem(ActionType, ContextMenuItemTagPaste, contextMenuItemTagPaste());

    Node* node = m_hitTestResult.innerNonSharedNode();
    if (!node)
        return;
    Frame* frame = node->document()->frame();
    if (!frame)
        return;

    if (!m_hitTestResult.isContentEditable()) {
        FrameLoader* loader = frame->loader();
        KURL linkURL = m_hitTestResult.absoluteLinkURL();
        if (!linkURL.isEmpty()) {
            if (loader->client()->canHandleRequest(ResourceRequest(linkURL))) {
                appendItem(OpenLinkItem, m_contextMenu.get());
                appendItem(OpenLinkInNewWindowItem, m_contextMenu.get());
                appendItem(DownloadFileItem, m_contextMenu.get());
            }
            appendItem(CopyLinkItem, m_contextMenu.get());
        }

        KURL imageURL = m_hitTestResult.absoluteImageURL();
        if (!imageURL.isEmpty()) {
            if (!linkURL.isEmpty())
                appendItem(*separatorItem(), m_contextMenu.get());

            appendItem(OpenImageInNewWindowItem, m_contextMenu.get());
            appendItem(DownloadImageItem, m_contextMenu.get());
            if (imageURL.isLocalFile() || m_hitTestResult.image())
                appendItem(CopyImageItem, m_contextMenu.get());
        }

        KURL mediaURL = m_hitTestResult.absoluteMediaURL();
        if (!mediaURL.isEmpty()) {
            if (!linkURL.isEmpty() || !imageURL.isEmpty())
                appendItem(*separatorItem(), m_contextMenu.get());

            appendItem(MediaPlayPause, m_contextMenu.get());
            appendItem(MediaMute, m_contextMenu.get());
            appendItem(ToggleMediaControls, m_contextMenu.get());
            appendItem(ToggleMediaLoop, m_contextMenu.get());
            appendItem(EnterVideoFullscreen, m_contextMenu.get());

            appendItem(*separatorItem(), m_contextMenu.get());
            appendItem(CopyMediaLinkItem, m_contextMenu.get());
            appendItem(OpenMediaInNewWindowItem, m_contextMenu.get());
        }

        if (imageURL.isEmpty() && linkURL.isEmpty() && mediaURL.isEmpty()) {
            if (m_hitTestResult.isSelected()) {
                if (selectionContainsPossibleWord(frame)) {
                    appendItem(SearchWebItem, m_contextMenu.get());
                    appendItem(*separatorItem(), m_contextMenu.get());
                }

                appendItem(CopyItem, m_contextMenu.get());
            } else {
                if (!(frame->page() && frame->page()->inspectorController()->hasInspectorFrontendClient())) {
                    if (frame->page() && frame->page()->backForward()->canGoBackOrForward(-1))
                        appendItem(BackItem, m_contextMenu.get());

                    if (frame->page() && frame->page()->backForward()->canGoBackOrForward(1))
                        appendItem(ForwardItem, m_contextMenu.get());

                    // use isLoadingInAPISense rather than isLoading because Stop/Reload are
                    // intended to match WebKit's API, not WebCore's internal notion of loading status
                    if (loader->documentLoader()->isLoadingInAPISense())
                        appendItem(StopItem, m_contextMenu.get());
                    else
                        appendItem(ReloadItem, m_contextMenu.get());
                }

                if (frame->page() && frame != frame->page()->mainFrame())
                    appendItem(OpenFrameItem, m_contextMenu.get());
            }
        }
    } else { // Make an editing context menu
        FrameSelection* selection = frame->selection();
        bool inPasswordField = selection->isInPasswordField();
        if (!inPasswordField) {
            bool haveContextMenuItemsForMisspellingOrGrammer = false;
            bool spellCheckingEnabled = frame->editor()->isSpellCheckingEnabledFor(node);
            if (spellCheckingEnabled) {
                // Consider adding spelling-related or grammar-related context menu items (never both, since a single selected range
                // is never considered a misspelling and bad grammar at the same time)
                bool misspelling;
                bool badGrammar;
                Vector<String> guesses = frame->editor()->guessesForMisspelledOrUngrammatical(misspelling, badGrammar);
                if (misspelling || badGrammar) {
                    size_t size = guesses.size();
                    if (!size) {
                        // If there's bad grammar but no suggestions (e.g., repeated word), just leave off the suggestions
                        // list and trailing separator rather than adding a "No Guesses Found" item (matches AppKit)
                        if (misspelling) {
                            appendItem(NoGuessesItem, m_contextMenu.get());
                            appendItem(*separatorItem(), m_contextMenu.get());
                        }
                    } else {
                        for (unsigned i = 0; i < size; i++) {
                            const String &guess = guesses[i];
                            if (!guess.isEmpty()) {
                                ContextMenuItem item(ActionType, ContextMenuItemTagSpellingGuess, guess);
                                appendItem(item, m_contextMenu.get());
                            }
                        }
                        appendItem(*separatorItem(), m_contextMenu.get());
                    }
                    if (misspelling) {
                        appendItem(IgnoreSpellingItem, m_contextMenu.get());
                        appendItem(LearnSpellingItem, m_contextMenu.get());
                    } else
                        appendItem(IgnoreGrammarItem, m_contextMenu.get());
                    appendItem(*separatorItem(), m_contextMenu.get());
                    haveContextMenuItemsForMisspellingOrGrammer = true;
                }
            }

            if (!haveContextMenuItemsForMisspellingOrGrammer) {
                // Spelling and grammar checking is mutually exclusive with dictation alternatives.
                Vector<String> dictationAlternatives = m_hitTestResult.dictationAlternatives();
                if (!dictationAlternatives.isEmpty()) {
                    for (size_t i = 0; i < dictationAlternatives.size(); ++i) {
                        ContextMenuItem item(ActionType, ContextMenuItemTagDictationAlternative, dictationAlternatives[i]);
                        appendItem(item, m_contextMenu.get());
                    }
                    appendItem(*separatorItem(), m_contextMenu.get());
                }
            }
        }

        FrameLoader* loader = frame->loader();
        KURL linkURL = m_hitTestResult.absoluteLinkURL();
        if (!linkURL.isEmpty()) {
            if (loader->client()->canHandleRequest(ResourceRequest(linkURL))) {
                appendItem(OpenLinkItem, m_contextMenu.get());
                appendItem(OpenLinkInNewWindowItem, m_contextMenu.get());
                appendItem(DownloadFileItem, m_contextMenu.get());
            }
            appendItem(CopyLinkItem, m_contextMenu.get());
            appendItem(*separatorItem(), m_contextMenu.get());
        }

        if (m_hitTestResult.isSelected() && !inPasswordField && selectionContainsPossibleWord(frame)) {
            appendItem(SearchWebItem, m_contextMenu.get());
            appendItem(*separatorItem(), m_contextMenu.get());
        }

        appendItem(CutItem, m_contextMenu.get());
        appendItem(CopyItem, m_contextMenu.get());
        appendItem(PasteItem, m_contextMenu.get());

        if (!inPasswordField) {
            appendItem(*separatorItem(), m_contextMenu.get());
            ContextMenuItem SpellingAndGrammarMenuItem(SubmenuType, ContextMenuItemTagSpellingMenu, 
                contextMenuItemTagSpellingMenu());
            createAndAppendSpellingAndGrammarSubMenu(SpellingAndGrammarMenuItem);
            appendItem(SpellingAndGrammarMenuItem, m_contextMenu.get());
            bool shouldShowFontMenu = true;
            if (shouldShowFontMenu) {
                ContextMenuItem FontMenuItem(SubmenuType, ContextMenuItemTagFontMenu, 
                    contextMenuItemTagFontMenu());
                createAndAppendFontSubMenu(FontMenuItem);
                appendItem(FontMenuItem, m_contextMenu.get());
            }
            ContextMenuItem WritingDirectionMenuItem(SubmenuType, ContextMenuItemTagWritingDirectionMenu, 
                contextMenuItemTagWritingDirectionMenu());
            createAndAppendWritingDirectionSubMenu(WritingDirectionMenuItem);
            appendItem(WritingDirectionMenuItem, m_contextMenu.get());
            if (Page* page = frame->page()) {
                if (Settings* settings = page->settings()) {
                    bool includeTextDirectionSubmenu = settings->textDirectionSubmenuInclusionBehavior() == TextDirectionSubmenuAlwaysIncluded
                        || (settings->textDirectionSubmenuInclusionBehavior() == TextDirectionSubmenuAutomaticallyIncluded && frame->editor()->hasBidiSelection());
                    if (includeTextDirectionSubmenu) {
                        ContextMenuItem TextDirectionMenuItem(SubmenuType, ContextMenuItemTagTextDirectionMenu, 
                            contextMenuItemTagTextDirectionMenu());
                        createAndAppendTextDirectionSubMenu(TextDirectionMenuItem);
                        appendItem(TextDirectionMenuItem, m_contextMenu.get());
                    }
                }
            }
        }
    }
}

void ContextMenuController::addInspectElementItem()
{
    Node* node = m_hitTestResult.innerNonSharedNode();
    if (!node)
        return;

    Frame* frame = node->document()->frame();
    if (!frame)
        return;

    Page* page = frame->page();
    if (!page)
        return;

    if (!page->inspectorController())
        return;

    ContextMenuItem InspectElementItem(ActionType, ContextMenuItemTagInspectElement, contextMenuItemTagInspectElement());
    if (m_contextMenu && !m_contextMenu->items().isEmpty())
        appendItem(*separatorItem(), m_contextMenu.get());
    appendItem(InspectElementItem, m_contextMenu.get());
}

void ContextMenuController::checkOrEnableIfNeeded(ContextMenuItem& item) const
{
    if (item.type() == SeparatorType)
        return;
    
    Frame* frame = m_hitTestResult.innerNonSharedNode()->document()->frame();
    if (!frame)
        return;

    // Custom items already have proper checked and enabled values.
    if (ContextMenuItemBaseCustomTag <= item.action() && item.action() <= ContextMenuItemLastCustomTag)
        return;

    bool shouldEnable = true;
    bool shouldCheck = false; 

    switch (item.action()) {
        case ContextMenuItemTagCheckSpelling:
            shouldEnable = frame->editor()->canEdit();
            break;
        case ContextMenuItemTagDefaultDirection:
            shouldCheck = false;
            shouldEnable = false;
            break;
        case ContextMenuItemTagLeftToRight:
        case ContextMenuItemTagRightToLeft: {
            String direction = item.action() == ContextMenuItemTagLeftToRight ? "ltr" : "rtl";
            shouldCheck = frame->editor()->selectionHasStyle(CSSPropertyDirection, direction) != FalseTriState;
            shouldEnable = true;
            break;
        }
        case ContextMenuItemTagTextDirectionDefault: {
            Editor::Command command = frame->editor()->command("MakeTextWritingDirectionNatural");
            shouldCheck = command.state() == TrueTriState;
            shouldEnable = command.isEnabled();
            break;
        }
        case ContextMenuItemTagTextDirectionLeftToRight: {
            Editor::Command command = frame->editor()->command("MakeTextWritingDirectionLeftToRight");
            shouldCheck = command.state() == TrueTriState;
            shouldEnable = command.isEnabled();
            break;
        }
        case ContextMenuItemTagTextDirectionRightToLeft: {
            Editor::Command command = frame->editor()->command("MakeTextWritingDirectionRightToLeft");
            shouldCheck = command.state() == TrueTriState;
            shouldEnable = command.isEnabled();
            break;
        }
        case ContextMenuItemTagCopy:
            shouldEnable = frame->editor()->canDHTMLCopy() || frame->editor()->canCopy();
            break;
        case ContextMenuItemTagCut:
            shouldEnable = frame->editor()->canDHTMLCut() || frame->editor()->canCut();
            break;
        case ContextMenuItemTagIgnoreSpelling:
        case ContextMenuItemTagLearnSpelling:
            shouldEnable = frame->selection()->isRange();
            break;
        case ContextMenuItemTagPaste:
            shouldEnable = frame->editor()->canDHTMLPaste() || frame->editor()->canPaste();
            break;
        case ContextMenuItemTagUnderline: {
            shouldCheck = frame->editor()->selectionHasStyle(CSSPropertyWebkitTextDecorationsInEffect, "underline") != FalseTriState;
            shouldEnable = frame->editor()->canEditRichly();
            break;
        }
        case ContextMenuItemTagLookUpInDictionary:
            shouldEnable = frame->selection()->isRange();
            break;
        case ContextMenuItemTagCheckGrammarWithSpelling:
            if (frame->editor()->isGrammarCheckingEnabled())
                shouldCheck = true;
            shouldEnable = true;
            break;
        case ContextMenuItemTagItalic: {
            shouldCheck = frame->editor()->selectionHasStyle(CSSPropertyFontStyle, "italic") != FalseTriState;
            shouldEnable = frame->editor()->canEditRichly();
            break;
        }
        case ContextMenuItemTagBold: {
            shouldCheck = frame->editor()->selectionHasStyle(CSSPropertyFontWeight, "bold") != FalseTriState;
            shouldEnable = frame->editor()->canEditRichly();
            break;
        }
        case ContextMenuItemTagOutline:
            shouldEnable = false;
            break;
        case ContextMenuItemTagShowSpellingPanel:
            if (frame->editor()->spellingPanelIsShowing())
                item.setTitle(contextMenuItemTagShowSpellingPanel(false));
            else
                item.setTitle(contextMenuItemTagShowSpellingPanel(true));
            shouldEnable = frame->editor()->canEdit();
            break;
        case ContextMenuItemTagNoGuessesFound:
            shouldEnable = false;
            break;
        case ContextMenuItemTagCheckSpellingWhileTyping:
            shouldCheck = frame->editor()->isContinuousSpellCheckingEnabled();
            break;
        case ContextMenuItemTagStopSpeaking:
            break;
        case ContextMenuItemTagGoBack:
        case ContextMenuItemTagGoForward:
        case ContextMenuItemTagStop:
        case ContextMenuItemTagReload:
        case ContextMenuItemTagFontMenu:
        case ContextMenuItemTagNoAction:
        case ContextMenuItemTagOpenLinkInNewWindow:
        case ContextMenuItemTagOpenLinkInThisWindow:
        case ContextMenuItemTagDownloadLinkToDisk:
        case ContextMenuItemTagCopyLinkToClipboard:
        case ContextMenuItemTagOpenImageInNewWindow:
        case ContextMenuItemTagDownloadImageToDisk:
        case ContextMenuItemTagCopyImageToClipboard:
            break;
        case ContextMenuItemTagOpenMediaInNewWindow:
            if (m_hitTestResult.mediaIsVideo())
                item.setTitle(contextMenuItemTagOpenVideoInNewWindow());
            else
                item.setTitle(contextMenuItemTagOpenAudioInNewWindow());
            break;
        case ContextMenuItemTagCopyMediaLinkToClipboard:
            if (m_hitTestResult.mediaIsVideo())
                item.setTitle(contextMenuItemTagCopyVideoLinkToClipboard());
            else
                item.setTitle(contextMenuItemTagCopyAudioLinkToClipboard());
            break;
        case ContextMenuItemTagToggleMediaControls:
            shouldCheck = m_hitTestResult.mediaControlsEnabled();
            break;
        case ContextMenuItemTagToggleMediaLoop:
            shouldCheck = m_hitTestResult.mediaLoopEnabled();
            break;
        case ContextMenuItemTagEnterVideoFullscreen:
            shouldEnable = m_hitTestResult.mediaSupportsFullscreen();
            break;
        case ContextMenuItemTagOpenFrameInNewWindow:
        case ContextMenuItemTagSpellingGuess:
        case ContextMenuItemTagOther:
        case ContextMenuItemTagSearchInSpotlight:
        case ContextMenuItemTagSearchWeb:
        case ContextMenuItemTagOpenWithDefaultApplication:
        case ContextMenuItemPDFActualSize:
        case ContextMenuItemPDFZoomIn:
        case ContextMenuItemPDFZoomOut:
        case ContextMenuItemPDFAutoSize:
        case ContextMenuItemPDFSinglePage:
        case ContextMenuItemPDFFacingPages:
        case ContextMenuItemPDFContinuous:
        case ContextMenuItemPDFNextPage:
        case ContextMenuItemPDFPreviousPage:
        case ContextMenuItemTagOpenLink:
        case ContextMenuItemTagIgnoreGrammar:
        case ContextMenuItemTagSpellingMenu:
        case ContextMenuItemTagShowFonts:
        case ContextMenuItemTagStyles:
        case ContextMenuItemTagShowColors:
        case ContextMenuItemTagSpeechMenu:
        case ContextMenuItemTagStartSpeaking:
        case ContextMenuItemTagWritingDirectionMenu:
        case ContextMenuItemTagTextDirectionMenu:
        case ContextMenuItemTagPDFSinglePageScrolling:
        case ContextMenuItemTagPDFFacingPagesScrolling:
        case ContextMenuItemTagInspectElement:
        case ContextMenuItemBaseCustomTag:
        case ContextMenuItemCustomTagNoAction:
        case ContextMenuItemLastCustomTag:
        case ContextMenuItemBaseApplicationTag:
        case ContextMenuItemTagDictationAlternative:
            break;
        case ContextMenuItemTagMediaPlayPause:
            if (m_hitTestResult.mediaPlaying())
                item.setTitle(contextMenuItemTagMediaPause());
            else
                item.setTitle(contextMenuItemTagMediaPlay());
            break;
        case ContextMenuItemTagMediaMute:
            shouldEnable = m_hitTestResult.mediaHasAudio();
            shouldCheck = shouldEnable &&  m_hitTestResult.mediaMuted();
            break;
    }

    item.setChecked(shouldCheck);
    item.setEnabled(shouldEnable);
}

} // namespace WebCore
