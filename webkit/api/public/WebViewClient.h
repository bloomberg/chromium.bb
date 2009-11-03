/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebViewClient_h
#define WebViewClient_h

#include "WebDragOperation.h"
#include "WebEditingAction.h"
#include "WebFileChooserCompletion.h"
#include "WebTextAffinity.h"
#include "WebTextDirection.h"
#include "WebWidgetClient.h"

namespace WebKit {
    class WebAccessibilityObject;
    class WebDragData;
    class WebFileChooserCompletion;
    class WebFrame;
    class WebNode;
    class WebNotificationPresenter;
    class WebRange;
    class WebString;
    class WebURL;
    class WebView;
    class WebWidget;
    struct WebConsoleMessage;
    struct WebContextMenuData;
    struct WebPoint;
    struct WebPopupMenuInfo;

    // Since a WebView is a WebWidget, a WebViewClient is a WebWidgetClient.
    // Virtual inheritance allows an implementation of WebWidgetClient to be
    // easily reused as part of an implementation of WebViewClient.
    class WebViewClient : virtual public WebWidgetClient {
    public:
        // Factory methods -----------------------------------------------------

        // Create a new related WebView.
        virtual WebView* createView(WebFrame* creator) = 0;

        // Create a new WebPopupMenu.  In the second form, the client is
        // responsible for rendering the contents of the popup menu.
        virtual WebWidget* createPopupMenu(bool activatable) = 0;
        virtual WebWidget* createPopupMenu(const WebPopupMenuInfo&) = 0;


        // Misc ----------------------------------------------------------------

        // A new message was added to the console.
        virtual void didAddMessageToConsole(
            const WebConsoleMessage&, const WebString& sourceName, unsigned sourceLine) = 0;

        // Called when script in the page calls window.print().  If frame is
        // non-null, then it selects a particular frame, including its
        // children, to print.  Otherwise, the main frame and its children
        // should be printed.
        virtual void printPage(WebFrame*) = 0;

        // Called to retrieve the provider of desktop notifications.
        virtual WebNotificationPresenter* notificationPresenter() = 0;


        // Navigational --------------------------------------------------------

        // These notifications bracket any loading that occurs in the WebView.
        virtual void didStartLoading() = 0;
        virtual void didStopLoading() = 0;


        // Editing -------------------------------------------------------------

        // These methods allow the client to intercept and overrule editing
        // operations.
        virtual bool shouldBeginEditing(const WebRange&) = 0;
        virtual bool shouldEndEditing(const WebRange&) = 0;
        virtual bool shouldInsertNode(
            const WebNode&, const WebRange&, WebEditingAction) = 0;
        virtual bool shouldInsertText(
            const WebString&, const WebRange&, WebEditingAction) = 0;
        virtual bool shouldChangeSelectedRange(
            const WebRange& from, const WebRange& to, WebTextAffinity,
            bool stillSelecting) = 0;
        virtual bool shouldDeleteRange(const WebRange&) = 0;
        virtual bool shouldApplyStyle(const WebString& style, const WebRange&) = 0;

        virtual bool isSmartInsertDeleteEnabled() = 0;
        virtual bool isSelectTrailingWhitespaceEnabled() = 0;
        virtual void setInputMethodEnabled(bool enabled) = 0;

        virtual void didBeginEditing() = 0;
        virtual void didChangeSelection(bool isSelectionEmpty) = 0;
        virtual void didChangeContents() = 0;
        virtual void didExecuteCommand(const WebString& commandName) = 0;
        virtual void didEndEditing() = 0;

        // This method is called in response to WebView's handleInputEvent()
        // when the default action for the current keyboard event is not
        // suppressed by the page, to give the embedder a chance to handle
        // the keyboard event specially.
        //
        // Returns true if the keyboard event was handled by the embedder,
        // indicating that the default action should be suppressed.
        virtual bool handleCurrentKeyboardEvent() = 0;


        // Spellchecker --------------------------------------------------------

        // The client should perform spell-checking on the given text.  If the
        // text contains a misspelled word, then upon return misspelledOffset
        // will point to the start of the misspelled word, and misspelledLength
        // will indicates its length.  Otherwise, if there was not a spelling
        // error, then upon return misspelledLength is 0.
        virtual void spellCheck(
            const WebString& text, int& misspelledOffset, int& misspelledLength) = 0;

        // Computes an auto-corrected replacement for a misspelled word.  If no
        // replacement is found, then an empty string is returned.
        virtual WebString autoCorrectWord(const WebString& misspelledWord) = 0;

        // Show or hide the spelling UI.
        virtual void showSpellingUI(bool show) = 0;

        // Returns true if the spelling UI is showing.
        virtual bool isShowingSpellingUI() = 0;

        // Update the spelling UI with the given word.
        virtual void updateSpellingUIWithMisspelledWord(const WebString& word) = 0;


        // Dialogs -------------------------------------------------------------

        // This method returns immediately after showing the dialog. When the
        // dialog is closed, it should call the WebFileChooserCompletion to
        // pass the results of the dialog. Returns false if
        // WebFileChooseCompletion will never be called.
        virtual bool runFileChooser(
            bool multiSelect, const WebString& title,
            const WebString& initialValue, WebFileChooserCompletion*) = 0;

        // Displays a modal alert dialog containing the given message.  Returns
        // once the user dismisses the dialog.
        virtual void runModalAlertDialog(
            WebFrame*, const WebString& message) = 0;

        // Displays a modal confirmation dialog with the given message as
        // description and OK/Cancel choices.  Returns true if the user selects
        // 'OK' or false otherwise.
        virtual bool runModalConfirmDialog(
            WebFrame*, const WebString& message) = 0;

        // Displays a modal input dialog with the given message as description
        // and OK/Cancel choices.  The input field is pre-filled with
        // defaultValue.  Returns true if the user selects 'OK' or false
        // otherwise.  Upon returning true, actualValue contains the value of
        // the input field.
        virtual bool runModalPromptDialog(
            WebFrame*, const WebString& message, const WebString& defaultValue,
            WebString* actualValue) = 0;

        // Displays a modal confirmation dialog containing the given message as
        // description and OK/Cancel choices, where 'OK' means that it is okay
        // to proceed with closing the view.  Returns true if the user selects
        // 'OK' or false otherwise.
        virtual bool runModalBeforeUnloadDialog(
            WebFrame*, const WebString& message) = 0;

        // Shows a context menu with commands relevant to a specific element on
        // the given frame. Additional context data is supplied.
        virtual void showContextMenu(WebFrame*, const WebContextMenuData&) = 0;

        // UI ------------------------------------------------------------------

        // Called when script modifies window.status
        virtual void setStatusText(const WebString&) = 0;

        // Called when hovering over an anchor with the given URL.
        virtual void setMouseOverURL(const WebURL&) = 0;

        // Called when keyboard focus switches to an anchor with the given URL.
        virtual void setKeyboardFocusURL(const WebURL&) = 0;

        // Called when a tooltip should be shown at the current cursor position.
        virtual void setToolTipText(const WebString&, WebTextDirection hint) = 0;

        // Called when a context menu should be shown at the current cursor position.
        // FIXME hook this up
        //virtual void showContextMenu(const WebContextMenuInfo&) = 0;

        // Called when a drag-n-drop operation should begin.
        virtual void startDragging(
            const WebPoint& from, const WebDragData&, WebDragOperationsMask) = 0;

        // Called to determine if drag-n-drop operations may initiate a page
        // navigation.
        virtual bool acceptsLoadDrops() = 0;

        // Take focus away from the WebView by focusing an adjacent UI element
        // in the containing window.
        virtual void focusNext() = 0;
        virtual void focusPrevious() = 0;


        // Session history -----------------------------------------------------

        // Tells the embedder to navigate back or forward in session history by
        // the given offset (relative to the current position in session
        // history).
        virtual void navigateBackForwardSoon(int offset) = 0;

        // Returns the number of history items before/after the current
        // history item.
        virtual int historyBackListCount() = 0;
        virtual int historyForwardListCount() = 0;

        // Called to notify the embedder when a new history item is added.
        virtual void didAddHistoryItem() = 0;


        // Accessibility -------------------------------------------------------

        // Notifies embedder that the focus has changed to the given
        // accessibility object.
        virtual void focusAccessibilityObject(const WebAccessibilityObject&) = 0;


        // Developer tools -----------------------------------------------------

        // Called to notify the client that the inspector's settings were
        // changed and should be saved.  See WebView::inspectorSettings.
        virtual void didUpdateInspectorSettings() = 0;


        // Autofill ------------------------------------------------------------

        // Queries the browser for suggestions to be shown for the form text
        // field named |name|.  |value| is the text entered by the user so
        // far and the WebNode corresponds to the input field.
        virtual void queryAutofillSuggestions(const WebNode&,
                                              const WebString& name,
                                              const WebString& value) = 0;

        // Instructs the browser to remove the autofill entry specified from
        // its DB.
        virtual void removeAutofillSuggestions(const WebString& name,
                                               const WebString& value) = 0;

    protected:
        ~WebViewClient() { }
    };

} // namespace WebKit

#endif
