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
#include "WebTextAffinity.h"
#include "WebWidgetClient.h"

class WebView;  // FIXME: Move into the WebKit namespace.

namespace WebKit {
    class WebDragData;
    class WebFileChooserCompletion;
    class WebFrame;
    class WebNode;
    class WebRange;
    class WebString;
    class WebWidget;
    struct WebConsoleMessage;
    struct WebContextMenuInfo;
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


        // Spellchecker --------------------------------------------------------

        // The client should perform spell-checking on the given word
        // synchronously.  Return a length of 0 if the word is not misspelled.
        // FIXME hook this up
        //virtual void spellCheck(
        //    const WebString& word, int& misspelledOffset, int& misspelledLength) = 0;


        // Dialogs -------------------------------------------------------------

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

        // This method returns immediately after showing the dialog.  When the
        // dialog is closed, it should call the WebFileChooserCompletion to
        // pass the results of the dialog.
        // FIXME hook this up
        //virtual void runFileChooser(
        //    bool multiSelect, const WebString& title,
        //    const WebString& initialValue, WebFileChooserCompletion*) = 0;


        // UI ------------------------------------------------------------------

        // Called when script modifies window.status
        virtual void setStatusText(const WebString&) = 0;

        // Called when hovering over an anchor with the given URL.
        virtual void setMouseOverURL(const WebURL&) = 0;

        // Called when a tooltip should be shown at the current cursor position.
        virtual void setToolTipText(const WebString&, WebTextDirection hint) = 0;

        // Called when a context menu should be shown at the current cursor position.
        // FIXME hook this up
        //virtual void showContextMenu(const WebContextMenuInfo&) = 0;

        // Called when a drag-n-drop operation should begin.
        virtual void startDragging(
            const WebPoint& from, const WebDragData&, WebDragOperationsMask) = 0;

        // Take focus away from the WebView by focusing an adjacent UI element
        // in the containing window.
        virtual void focusNext() = 0;
        virtual void focusPrevious() = 0;


        // Session History -----------------------------------------------------

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


        // FIXME need to something for:
        // OnPasswordFormsSeen
        // OnAutofillFormSubmitted
        // QueryFormFieldAutofill
        // RemoveStoredAutofillEntry
        // ShowModalHTMLDialog <-- we should be able to kill this
        // GetWebDevToolsAgentDelegate
        // WasOpenedByUserGesture
    };

} // namespace WebKit

#endif
