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

#include "HTMLNames.h"
#include "core/dom/Document.h"
#include "core/fileapi/FileList.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/ChromeClient.h"
#include "core/page/Frame.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/PageGroupLoadDeferrer.h"
#include "core/page/PopupOpeningObserver.h"
#include "core/page/SecurityOrigin.h"
#include "core/page/Settings.h"
#include "core/page/WindowFeatures.h"
#include "core/platform/DateTimeChooser.h"
#include "core/platform/FileChooser.h"
#include "core/platform/FileIconLoader.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/graphics/Icon.h"
#include "core/platform/network/DNS.h"
#include "core/platform/network/ResourceHandle.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderObject.h"
#include "core/storage/StorageNamespace.h"
#include "modules/geolocation/Geolocation.h"
#include <public/WebScreenInfo.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/Vector.h>

#if ENABLE(INPUT_TYPE_COLOR)
#include "core/platform/ColorChooser.h"
#endif

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

void Chrome::scrollbarsModeDidChange() const
{
    m_client->scrollbarsModeDidChange();
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

Page* Chrome::createWindow(Frame* frame, const FrameLoadRequest& request, const WindowFeatures& features, const NavigationAction& action) const
{
    Page* newPage = m_client->createWindow(frame, request, features, action);

    if (newPage) {
        if (StorageNamespace* oldSessionStorage = m_page->sessionStorage(false))
            newPage->setSessionStorage(oldSessionStorage->copy());
    }

    return newPage;
}

void Chrome::show() const
{
    m_client->show();
}

bool Chrome::canRunModal() const
{
    return m_client->canRunModal();
}

static bool canRunModalIfDuringPageDismissal(Page* page, ChromeClient::DialogType dialog, const String& message)
{
    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        FrameLoader::PageDismissalType dismissal = frame->loader()->pageDismissalEventBeingDispatched();
        if (dismissal != FrameLoader::NoDismissal)
            return page->chrome()->client()->shouldRunModalDialogDuringPageDismissal(dialog, message, dismissal);
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

void Chrome::setToolbarsVisible(bool b) const
{
    m_client->setToolbarsVisible(b);
}

bool Chrome::toolbarsVisible() const
{
    return m_client->toolbarsVisible();
}

void Chrome::setStatusbarVisible(bool b) const
{
    m_client->setStatusbarVisible(b);
}

bool Chrome::statusbarVisible() const
{
    return m_client->statusbarVisible();
}

void Chrome::setScrollbarsVisible(bool b) const
{
    m_client->setScrollbarsVisible(b);
}

bool Chrome::scrollbarsVisible() const
{
    return m_client->scrollbarsVisible();
}

void Chrome::setMenubarVisible(bool b) const
{
    m_client->setMenubarVisible(b);
}

bool Chrome::menubarVisible() const
{
    return m_client->menubarVisible();
}

void Chrome::setResizable(bool b) const
{
    m_client->setResizable(b);
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
        Document* document = result.innerNode()->document();
        if (document && document->isDNSPrefetchEnabled())
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
                HTMLInputElement* input = static_cast<HTMLInputElement*>(node);
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

#if ENABLE(INPUT_TYPE_COLOR)
PassOwnPtr<ColorChooser> Chrome::createColorChooser(ColorChooserClient* client, const Color& initialColor)
{
    notifyPopupOpeningObservers();
    return m_client->createColorChooser(client, initialColor);
}
#endif

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

void Chrome::loadIconForFiles(const Vector<String>& filenames, FileIconLoader* loader)
{
    m_client->loadIconForFiles(filenames, loader);
}

void Chrome::dispatchViewportPropertiesDidChange(const ViewportArguments& arguments) const
{
    m_client->dispatchViewportPropertiesDidChange(arguments);
}

void Chrome::setCursor(const Cursor& cursor)
{
    m_client->setCursor(cursor);
}

void Chrome::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    m_client->setCursorHiddenUntilMouseMoves(hiddenUntilMouseMoves);
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

PassRefPtr<PopupMenu> Chrome::createPopupMenu(PopupMenuClient* client) const
{
    notifyPopupOpeningObservers();
    return m_client->createPopupMenu(client);
}

PassRefPtr<SearchPopupMenu> Chrome::createSearchPopupMenu(PopupMenuClient* client) const
{
    notifyPopupOpeningObservers();
    return m_client->createSearchPopupMenu(client);
}

void Chrome::registerPopupOpeningObserver(PopupOpeningObserver* observer)
{
    ASSERT(observer);
    m_popupOpeningObservers.append(observer);
}

void Chrome::unregisterPopupOpeningObserver(PopupOpeningObserver* observer)
{
    size_t index = m_popupOpeningObservers.find(observer);
    ASSERT(index != notFound);
    m_popupOpeningObservers.remove(index);
}

void Chrome::notifyPopupOpeningObservers() const
{
    const Vector<PopupOpeningObserver*> observers(m_popupOpeningObservers);
    for (size_t i = 0; i < observers.size(); ++i)
        observers[i]->willOpenPopup();
}

} // namespace WebCore
