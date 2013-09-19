/*
 * Copyright (C) 2006, 2007, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2012, Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/page/Chrome.h"

#include "public/platform/WebScreenInfo.h"
#include "HTMLNames.h"
#include "core/dom/Document.h"
#include "core/html/HTMLInputElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/ChromeClient.h"
#include "core/page/Frame.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/PageGroupLoadDeferrer.h"
#include "core/page/PopupOpeningObserver.h"
#include "core/page/WindowFeatures.h"
#include "core/platform/ColorChooser.h"
#include "core/platform/DateTimeChooser.h"
#include "core/platform/FileChooser.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/network/DNS.h"
#include "core/rendering/HitTestResult.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

using namespace HTMLNames;
using namespace std;

Chrome::Chrome(Page* page, ChromeClient* client)
    : m_page(page)
    , m_client(client)
{
    ASSERT(m_client);
}

Chrome::~Chrome()
{
    m_client->chromeDestroyed();
}

PassOwnPtr<Chrome> Chrome::create(Page* page, ChromeClient* client)
{
    return adoptPtr(new Chrome(page, client));
}

void Chrome::invalidateContentsAndRootView(const IntRect& updateRect)
{
    m_client->invalidateContentsAndRootView(updateRect);
}

void Chrome::invalidateContentsForSlowScroll(const IntRect& updateRect)
{
    m_client->invalidateContentsForSlowScroll(updateRect);
}

void Chrome::scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    m_client->scroll(scrollDelta, rectToScroll, clipRect);
    InspectorInstrumentation::didScroll(m_page);
}

IntPoint Chrome::screenToRootView(const IntPoint& point) const
{
    return m_client->screenToRootView(point);
}

IntRect Chrome::rootViewToScreen(const IntRect& rect) const
{
    return m_client->rootViewToScreen(rect);
}

WebKit::WebScreenInfo Chrome::screenInfo() const
{
    return m_client->screenInfo();
}

void Chrome::contentsSizeChanged(Frame* frame, const IntSize& size) const
{
    m_client->contentsSizeChanged(frame, size);
}

void Chrome::layoutUpdated(Frame* frame) const
{
    m_client->layoutUpdated(frame);
}

void Chrome::setWindowRect(const FloatRect& rect) const
{
    m_client->setWindowRect(rect);
}

FloatRect Chrome::windowRect() const
{
    return m_client->windowRect();
}

FloatRect Chrome::pageRect() const
{
    return m_client->pageRect();
}

void Chrome::focus() const
{
    m_client->focus();
}

void Chrome::unfocus() const
{
    m_client->unfocus();
}

bool Chrome::canTakeFocus(FocusDirection direction) const
{
    return m_client->canTakeFocus(direction);
}

void Chrome::takeFocus(FocusDirection direction) const
{
    m_client->takeFocus(direction);
}

void Chrome::focusedNodeChanged(Node* node) const
{
    m_client->focusedNodeChanged(node);
}

void Chrome::show(NavigationPolicy policy) const
{
    m_client->show(policy);
}

bool Chrome::canRunModal() const
{
    return m_client->canRunModal();
}

static bool canRunModalIfDuringPageDismissal(Page* page, ChromeClient::DialogType dialog, const String& message)
{
    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        Document::PageDismissalType dismissal = frame->document()->pageDismissalEventBeingDispatched();
        if (dismissal != Document::NoDismissal)
            return page->chrome().client().shouldRunModalDialogDuringPageDismissal(dialog, message, dismissal);
    }
    return true;
}

bool Chrome::canRunModalNow() const
{
    return canRunModal() && canRunModalIfDuringPageDismissal(m_page, ChromeClient::HTMLDialog, String());
}

void Chrome::runModal() const
{
    // Defer callbacks in all the other pages in this group, so we don't try to run JavaScript
    // in a way that could interact with this view.
    PageGroupLoadDeferrer deferrer(m_page, false);

    TimerBase::fireTimersInNestedEventLoop();
    m_client->runModal();
}

void Chrome::setWindowFeatures(const WindowFeatures& features) const
{
    m_client->setToolbarsVisible(features.toolBarVisible || features.locationBarVisible);
    m_client->setStatusbarVisible(features.statusBarVisible);
    m_client->setScrollbarsVisible(features.scrollbarsVisible);
    m_client->setMenubarVisible(features.menuBarVisible);
    m_client->setResizable(features.resizable);
}

bool Chrome::toolbarsVisible() const
{
    return m_client->toolbarsVisible();
}

bool Chrome::statusbarVisible() const
{
    return m_client->statusbarVisible();
}

bool Chrome::scrollbarsVisible() const
{
    return m_client->scrollbarsVisible();
}

bool Chrome::menubarVisible() const
{
    return m_client->menubarVisible();
}

bool Chrome::canRunBeforeUnloadConfirmPanel()
{
    return m_client->canRunBeforeUnloadConfirmPanel();
}

bool Chrome::runBeforeUnloadConfirmPanel(const String& message, Frame* frame)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willRunJavaScriptDialog(m_page, message);
    bool ok = m_client->runBeforeUnloadConfirmPanel(message, frame);
    InspectorInstrumentation::didRunJavaScriptDialog(cookie);
    return ok;
}

void Chrome::closeWindowSoon()
{
    m_client->closeWindowSoon();
}

void Chrome::runJavaScriptAlert(Frame* frame, const String& message)
{
    if (!canRunModalIfDuringPageDismissal(m_page, ChromeClient::AlertDialog, message))
        return;

    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

    ASSERT(frame);
    notifyPopupOpeningObservers();
    String displayMessage = frame->displayStringModifiedByEncoding(message);

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willRunJavaScriptDialog(m_page, displayMessage);
    m_client->runJavaScriptAlert(frame, displayMessage);
    InspectorInstrumentation::didRunJavaScriptDialog(cookie);
}

bool Chrome::runJavaScriptConfirm(Frame* frame, const String& message)
{
    if (!canRunModalIfDuringPageDismissal(m_page, ChromeClient::ConfirmDialog, message))
        return false;

    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

    ASSERT(frame);
    notifyPopupOpeningObservers();
    String displayMessage = frame->displayStringModifiedByEncoding(message);

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willRunJavaScriptDialog(m_page, displayMessage);
    bool ok = m_client->runJavaScriptConfirm(frame, displayMessage);
    InspectorInstrumentation::didRunJavaScriptDialog(cookie);
    return ok;
}

bool Chrome::runJavaScriptPrompt(Frame* frame, const String& prompt, const String& defaultValue, String& result)
{
    if (!canRunModalIfDuringPageDismissal(m_page, ChromeClient::PromptDialog, prompt))
        return false;

    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

    ASSERT(frame);
    notifyPopupOpeningObservers();
    String displayPrompt = frame->displayStringModifiedByEncoding(prompt);

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willRunJavaScriptDialog(m_page, displayPrompt);
    bool ok = m_client->runJavaScriptPrompt(frame, displayPrompt, frame->displayStringModifiedByEncoding(defaultValue), result);
    InspectorInstrumentation::didRunJavaScriptDialog(cookie);

    if (ok)
        result = frame->displayStringModifiedByEncoding(result);

    return ok;
}

void Chrome::setStatusbarText(Frame* frame, const String& status)
{
    ASSERT(frame);
    m_client->setStatusbarText(frame->displayStringModifiedByEncoding(status));
}

IntRect Chrome::windowResizerRect() const
{
    return m_client->windowResizerRect();
}

void Chrome::mouseDidMoveOverElement(const HitTestResult& result, unsigned modifierFlags)
{
    if (result.innerNode()) {
        if (result.innerNode()->document().isDNSPrefetchEnabled())
            prefetchDNS(result.absoluteLinkURL().host());
    }
    m_client->mouseDidMoveOverElement(result, modifierFlags);
}

void Chrome::setToolTip(const HitTestResult& result)
{
    // First priority is a potential toolTip representing a spelling or grammar error
    TextDirection toolTipDirection;
    String toolTip = result.spellingToolTip(toolTipDirection);

    // Next we'll consider a tooltip for element with "title" attribute
    if (toolTip.isEmpty())
        toolTip = result.title(toolTipDirection);

    // Lastly, for <input type="file"> that allow multiple files, we'll consider a tooltip for the selected filenames
    if (toolTip.isEmpty()) {
        if (Node* node = result.innerNonSharedNode()) {
            if (node->hasTagName(inputTag)) {
                HTMLInputElement* input = toHTMLInputElement(node);
                toolTip = input->defaultToolTip();

                // FIXME: We should obtain text direction of tooltip from
                // ChromeClient or platform. As of October 2011, all client
                // implementations don't use text direction information for
                // ChromeClient::setToolTip. We'll work on tooltip text
                // direction during bidi cleanup in form inputs.
                toolTipDirection = LTR;
            }
        }
    }

    m_client->setToolTip(toolTip, toolTipDirection);
}

void Chrome::print(Frame* frame)
{
    // FIXME: This should have PageGroupLoadDeferrer, like runModal() or runJavaScriptAlert(), becasue it's no different from those.
    m_client->print(frame);
}

void Chrome::enumerateChosenDirectory(FileChooser* fileChooser)
{
    m_client->enumerateChosenDirectory(fileChooser);
}

PassOwnPtr<ColorChooser> Chrome::createColorChooser(ColorChooserClient* client, const Color& initialColor)
{
    notifyPopupOpeningObservers();
    return m_client->createColorChooser(client, initialColor);
}

PassRefPtr<DateTimeChooser> Chrome::openDateTimeChooser(DateTimeChooserClient* client, const DateTimeChooserParameters& parameters)
{
    notifyPopupOpeningObservers();
    return m_client->openDateTimeChooser(client, parameters);
}

void Chrome::runOpenPanel(Frame* frame, PassRefPtr<FileChooser> fileChooser)
{
    notifyPopupOpeningObservers();
    m_client->runOpenPanel(frame, fileChooser);
}

void Chrome::dispatchViewportPropertiesDidChange(const ViewportArguments& arguments) const
{
    m_client->dispatchViewportPropertiesDidChange(arguments);
}

void Chrome::setCursor(const Cursor& cursor)
{
    m_client->setCursor(cursor);
}

void Chrome::scheduleAnimation()
{
    m_client->scheduleAnimation();
}

// --------

bool Chrome::hasOpenedPopup() const
{
    return m_client->hasOpenedPopup();
}

PassRefPtr<PopupMenu> Chrome::createPopupMenu(Frame& frame, PopupMenuClient* client) const
{
    notifyPopupOpeningObservers();
    return m_client->createPopupMenu(frame, client);
}

void Chrome::registerPopupOpeningObserver(PopupOpeningObserver* observer)
{
    ASSERT(observer);
    m_popupOpeningObservers.append(observer);
}

void Chrome::unregisterPopupOpeningObserver(PopupOpeningObserver* observer)
{
    size_t index = m_popupOpeningObservers.find(observer);
    ASSERT(index != kNotFound);
    m_popupOpeningObservers.remove(index);
}

void Chrome::notifyPopupOpeningObservers() const
{
    const Vector<PopupOpeningObserver*> observers(m_popupOpeningObservers);
    for (size_t i = 0; i < observers.size(); ++i)
        observers[i]->willOpenPopup();
}

} // namespace WebCore
