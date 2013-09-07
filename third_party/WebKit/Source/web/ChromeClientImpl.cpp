/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "config.h"
#include "ChromeClientImpl.h"

#include "ColorChooserPopupUIController.h"
#include "ColorChooserUIController.h"
#include "DateTimeChooserImpl.h"
#include "ExternalDateTimeChooser.h"
#include "ExternalPopupMenu.h"
#include "HTMLNames.h"
#include "PopupContainer.h"
#include "PopupMenuChromium.h"
#include "RuntimeEnabledFeatures.h"
#include "WebAXObject.h"
#include "WebAutofillClient.h"
#include "WebColorChooser.h"
#include "WebConsoleMessage.h"
#include "WebCursorInfo.h"
#include "WebFileChooserCompletionImpl.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebInputElement.h"
#include "WebInputEvent.h"
#include "WebKit.h"
#include "WebNode.h"
#include "WebPasswordGeneratorClient.h"
#include "WebPlugin.h"
#include "WebPluginContainerImpl.h"
#include "WebPopupMenuImpl.h"
#include "WebPopupMenuInfo.h"
#include "WebPopupType.h"
#include "WebSettings.h"
#include "WebSettingsImpl.h"
#include "WebTextDirection.h"
#include "WebUserGestureIndicator.h"
#include "WebUserGestureToken.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "WebWindowFeatures.h"
#include "bindings/v8/ScriptController.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/accessibility/AccessibilityObject.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/html/HTMLInputElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/Console.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/PagePopupDriver.h"
#include "core/page/Settings.h"
#include "core/page/WindowFeatures.h"
#include "core/platform/ColorChooser.h"
#include "core/platform/ColorChooserClient.h"
#include "core/platform/Cursor.h"
#include "core/platform/DateTimeChooser.h"
#include "core/platform/FileChooser.h"
#include "core/platform/PlatformScreen.h"
#include "core/platform/chromium/support/WrappedResourceRequest.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/graphics/GraphicsLayer.h"
#include "core/platform/graphics/IntRect.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderWidget.h"
#include "modules/geolocation/Geolocation.h"
#include "public/platform/Platform.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebURLRequest.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringConcatenate.h"
#include "wtf/unicode/CharacterNames.h"

using namespace WebCore;

namespace WebKit {

// Converts a WebCore::PopupContainerType to a WebKit::WebPopupType.
static WebPopupType convertPopupType(PopupContainer::PopupType type)
{
    switch (type) {
    case PopupContainer::Select:
        return WebPopupTypeSelect;
    case PopupContainer::Suggestion:
        return WebPopupTypeSuggestion;
    default:
        ASSERT_NOT_REACHED();
        return WebPopupTypeNone;
    }
}

// Converts a WebCore::AXObjectCache::AXNotification to a WebKit::WebAXEvent
static WebAXEvent toWebAXEvent(AXObjectCache::AXNotification notification)
{
    // These enums have the same values; enforced in AssertMatchingEnums.cpp.
    return static_cast<WebAXEvent>(notification);
}

ChromeClientImpl::ChromeClientImpl(WebViewImpl* webView)
    : m_webView(webView)
    , m_toolbarsVisible(true)
    , m_statusbarVisible(true)
    , m_scrollbarsVisible(true)
    , m_menubarVisible(true)
    , m_resizable(true)
    , m_pagePopupDriver(webView)
{
}

ChromeClientImpl::~ChromeClientImpl()
{
}

void* ChromeClientImpl::webView() const
{
    return static_cast<void*>(m_webView);
}

void ChromeClientImpl::chromeDestroyed()
{
    // Our lifetime is bound to the WebViewImpl.
}

void ChromeClientImpl::setWindowRect(const FloatRect& r)
{
    if (m_webView->client())
        m_webView->client()->setWindowRect(IntRect(r));
}

FloatRect ChromeClientImpl::windowRect()
{
    WebRect rect;
    if (m_webView->client())
        rect = m_webView->client()->rootWindowRect();
    else {
        // These numbers will be fairly wrong. The window's x/y coordinates will
        // be the top left corner of the screen and the size will be the content
        // size instead of the window size.
        rect.width = m_webView->size().width;
        rect.height = m_webView->size().height;
    }
    return FloatRect(rect);
}

FloatRect ChromeClientImpl::pageRect()
{
    // We hide the details of the window's border thickness from the web page by
    // simple re-using the window position here.  So, from the point-of-view of
    // the web page, the window has no border.
    return windowRect();
}

void ChromeClientImpl::focus()
{
    if (m_webView->client())
        m_webView->client()->didFocus();
}

void ChromeClientImpl::unfocus()
{
    if (m_webView->client())
        m_webView->client()->didBlur();
}

bool ChromeClientImpl::canTakeFocus(FocusDirection)
{
    // For now the browser can always take focus if we're not running layout
    // tests.
    return !layoutTestMode();
}

void ChromeClientImpl::takeFocus(FocusDirection direction)
{
    if (!m_webView->client())
        return;
    if (direction == FocusDirectionBackward)
        m_webView->client()->focusPrevious();
    else
        m_webView->client()->focusNext();
}

void ChromeClientImpl::focusedNodeChanged(Node* node)
{
    m_webView->client()->focusedNodeChanged(WebNode(node));

    WebURL focusURL;
    if (node && node->isLink()) {
        // This HitTestResult hack is the easiest way to get a link URL out of a
        // WebCore::Node.
        HitTestResult hitTest(IntPoint(0, 0));
        // This cast must be valid because of the isLink() check.
        hitTest.setURLElement(toElement(node));
        if (hitTest.isLiveLink())
            focusURL = hitTest.absoluteLinkURL();
    }
    m_webView->client()->setKeyboardFocusURL(focusURL);
}

Page* ChromeClientImpl::createWindow(
    Frame* frame, const FrameLoadRequest& r, const WindowFeatures& features, NavigationPolicy navigationPolicy)
{
    if (!m_webView->client())
        return 0;

    WebNavigationPolicy policy = static_cast<WebNavigationPolicy>(navigationPolicy);
    if (policy == WebNavigationPolicyIgnore)
        policy = getNavigationPolicy();

    WebViewImpl* newView = toWebViewImpl(
        m_webView->client()->createView(WebFrameImpl::fromFrame(frame), WrappedResourceRequest(r.resourceRequest()), features, r.frameName(), policy));
    if (!newView)
        return 0;
    return newView->page();
}

static inline void updatePolicyForEvent(const WebInputEvent* inputEvent, NavigationPolicy* policy)
{
    if (!inputEvent || inputEvent->type != WebInputEvent::MouseUp)
        return;

    const WebMouseEvent* mouseEvent = static_cast<const WebMouseEvent*>(inputEvent);

    unsigned short buttonNumber;
    switch (mouseEvent->button) {
    case WebMouseEvent::ButtonLeft:
        buttonNumber = 0;
        break;
    case WebMouseEvent::ButtonMiddle:
        buttonNumber = 1;
        break;
    case WebMouseEvent::ButtonRight:
        buttonNumber = 2;
        break;
    default:
        return;
    }
    bool ctrl = mouseEvent->modifiers & WebMouseEvent::ControlKey;
    bool shift = mouseEvent->modifiers & WebMouseEvent::ShiftKey;
    bool alt = mouseEvent->modifiers & WebMouseEvent::AltKey;
    bool meta = mouseEvent->modifiers & WebMouseEvent::MetaKey;

    NavigationPolicy userPolicy = *policy;
    navigationPolicyFromMouseEvent(buttonNumber, ctrl, shift, alt, meta, &userPolicy);
    // User and app agree that we want a new window; let the app override the decorations.
    if (userPolicy == NavigationPolicyNewWindow && *policy == NavigationPolicyNewPopup)
        return;
    *policy = userPolicy;
}

WebNavigationPolicy ChromeClientImpl::getNavigationPolicy()
{
    // If our default configuration was modified by a script or wasn't
    // created by a user gesture, then show as a popup. Else, let this
    // new window be opened as a toplevel window.
    bool asPopup = !m_toolbarsVisible
        || !m_statusbarVisible
        || !m_scrollbarsVisible
        || !m_menubarVisible
        || !m_resizable;

    NavigationPolicy policy = NavigationPolicyNewForegroundTab;
    if (asPopup)
        policy = NavigationPolicyNewPopup;
    updatePolicyForEvent(WebViewImpl::currentInputEvent(), &policy);

    return static_cast<WebNavigationPolicy>(policy);
}

void ChromeClientImpl::show(NavigationPolicy navigationPolicy)
{
    if (!m_webView->client())
        return;

    WebNavigationPolicy policy = static_cast<WebNavigationPolicy>(navigationPolicy);
    if (policy == WebNavigationPolicyIgnore)
        policy = getNavigationPolicy();
    m_webView->client()->show(policy);
}

bool ChromeClientImpl::canRunModal()
{
    return !!m_webView->client();
}

void ChromeClientImpl::runModal()
{
    if (m_webView->client())
        m_webView->client()->runModal();
}

void ChromeClientImpl::setToolbarsVisible(bool value)
{
    m_toolbarsVisible = value;
}

bool ChromeClientImpl::toolbarsVisible()
{
    return m_toolbarsVisible;
}

void ChromeClientImpl::setStatusbarVisible(bool value)
{
    m_statusbarVisible = value;
}

bool ChromeClientImpl::statusbarVisible()
{
    return m_statusbarVisible;
}

void ChromeClientImpl::setScrollbarsVisible(bool value)
{
    m_scrollbarsVisible = value;
    WebFrameImpl* webFrame = toWebFrameImpl(m_webView->mainFrame());
    if (webFrame)
        webFrame->setCanHaveScrollbars(value);
}

bool ChromeClientImpl::scrollbarsVisible()
{
    return m_scrollbarsVisible;
}

void ChromeClientImpl::setMenubarVisible(bool value)
{
    m_menubarVisible = value;
}

bool ChromeClientImpl::menubarVisible()
{
    return m_menubarVisible;
}

void ChromeClientImpl::setResizable(bool value)
{
    m_resizable = value;
}

bool ChromeClientImpl::shouldReportDetailedMessageForSource(const String& url)
{
    return m_webView->client() && m_webView->client()->shouldReportDetailedMessageForSource(url);
}

void ChromeClientImpl::addMessageToConsole(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID, const String& stackTrace)
{
    if (m_webView->client()) {
        m_webView->client()->didAddMessageToConsole(
            WebConsoleMessage(static_cast<WebConsoleMessage::Level>(level), message),
            sourceID,
            lineNumber,
            stackTrace);
    }
}

bool ChromeClientImpl::canRunBeforeUnloadConfirmPanel()
{
    return !!m_webView->client();
}

bool ChromeClientImpl::runBeforeUnloadConfirmPanel(const String& message, Frame* frame)
{
    if (m_webView->client()) {
        return m_webView->client()->runModalBeforeUnloadDialog(
            WebFrameImpl::fromFrame(frame), message);
    }
    return false;
}

void ChromeClientImpl::closeWindowSoon()
{
    // Make sure this Page can no longer be found by JS.
    m_webView->page()->clearPageGroup();

    // Make sure that all loading is stopped.  Ensures that JS stops executing!
    m_webView->mainFrame()->stopLoading();

    if (m_webView->client())
        m_webView->client()->closeWidgetSoon();
}

// Although a Frame is passed in, we don't actually use it, since we
// already know our own m_webView.
void ChromeClientImpl::runJavaScriptAlert(Frame* frame, const String& message)
{
    if (m_webView->client()) {
        if (WebUserGestureIndicator::isProcessingUserGesture())
            WebUserGestureIndicator::currentUserGestureToken().setJavascriptPrompt();
        m_webView->client()->runModalAlertDialog(
            WebFrameImpl::fromFrame(frame), message);
    }
}

// See comments for runJavaScriptAlert().
bool ChromeClientImpl::runJavaScriptConfirm(Frame* frame, const String& message)
{
    if (m_webView->client()) {
        if (WebUserGestureIndicator::isProcessingUserGesture())
            WebUserGestureIndicator::currentUserGestureToken().setJavascriptPrompt();
        return m_webView->client()->runModalConfirmDialog(
            WebFrameImpl::fromFrame(frame), message);
    }
    return false;
}

// See comments for runJavaScriptAlert().
bool ChromeClientImpl::runJavaScriptPrompt(Frame* frame,
                                           const String& message,
                                           const String& defaultValue,
                                           String& result)
{
    if (m_webView->client()) {
        if (WebUserGestureIndicator::isProcessingUserGesture())
            WebUserGestureIndicator::currentUserGestureToken().setJavascriptPrompt();
        WebString actualValue;
        bool ok = m_webView->client()->runModalPromptDialog(
            WebFrameImpl::fromFrame(frame),
            message,
            defaultValue,
            &actualValue);
        if (ok)
            result = actualValue;
        return ok;
    }
    return false;
}

void ChromeClientImpl::setStatusbarText(const String& message)
{
    if (m_webView->client())
        m_webView->client()->setStatusText(message);
}

bool ChromeClientImpl::tabsToLinks()
{
    return m_webView->tabsToLinks();
}

IntRect ChromeClientImpl::windowResizerRect() const
{
    IntRect result;
    if (m_webView->client())
        result = m_webView->client()->windowResizerRect();
    return result;
}

void ChromeClientImpl::invalidateContentsAndRootView(const IntRect& updateRect)
{
    if (updateRect.isEmpty())
        return;
    m_webView->invalidateRect(updateRect);
}

void ChromeClientImpl::invalidateContentsForSlowScroll(const IntRect& updateRect)
{
    invalidateContentsAndRootView(updateRect);
}

void ChromeClientImpl::scheduleAnimation()
{
    m_webView->scheduleAnimation();
}

void ChromeClientImpl::scroll(
    const IntSize& scrollDelta, const IntRect& scrollRect,
    const IntRect& clipRect)
{
    if (!m_webView->isAcceleratedCompositingActive()) {
        if (m_webView->client()) {
            int dx = scrollDelta.width();
            int dy = scrollDelta.height();
            m_webView->client()->didScrollRect(dx, dy, intersection(scrollRect, clipRect));
        }
    } else
        m_webView->scrollRootLayerRect(scrollDelta, clipRect);
}

IntPoint ChromeClientImpl::screenToRootView(const IntPoint& point) const
{
    IntPoint windowPoint(point);

    if (m_webView->client()) {
        WebRect windowRect = m_webView->client()->windowRect();
        windowPoint.move(-windowRect.x, -windowRect.y);
    }

    return windowPoint;
}

IntRect ChromeClientImpl::rootViewToScreen(const IntRect& rect) const
{
    IntRect screenRect(rect);

    if (m_webView->client()) {
        WebRect windowRect = m_webView->client()->windowRect();
        screenRect.move(windowRect.x, windowRect.y);
    }

    return screenRect;
}

WebScreenInfo ChromeClientImpl::screenInfo() const
{
    return m_webView->client() ? m_webView->client()->screenInfo() : WebScreenInfo();
}

void ChromeClientImpl::contentsSizeChanged(Frame* frame, const IntSize& size) const
{
    m_webView->didChangeContentsSize();

    WebFrameImpl* webframe = WebFrameImpl::fromFrame(frame);
    webframe->didChangeContentsSize(size);
    if (webframe->client())
        webframe->client()->didChangeContentsSize(webframe, size);
}

void ChromeClientImpl::deviceOrPageScaleFactorChanged() const
{
    m_webView->deviceOrPageScaleFactorChanged();
}

void ChromeClientImpl::didProgrammaticallyScroll(Frame* frame, const IntPoint& scrollPoint) const
{
    ASSERT(frame->view()->inProgrammaticScroll());
    if (frame->page()->mainFrame() == frame)
        m_webView->didProgrammaticallyScroll(scrollPoint);
}

void ChromeClientImpl::layoutUpdated(Frame* frame) const
{
    m_webView->layoutUpdated(WebFrameImpl::fromFrame(frame));
}

void ChromeClientImpl::mouseDidMoveOverElement(
    const HitTestResult& result, unsigned modifierFlags)
{
    if (!m_webView->client())
        return;

    WebURL url;
    // Find out if the mouse is over a link, and if so, let our UI know...
    if (result.isLiveLink() && !result.absoluteLinkURL().string().isEmpty())
        url = result.absoluteLinkURL();
    else if (result.innerNonSharedNode()
             && (result.innerNonSharedNode()->hasTagName(HTMLNames::objectTag)
                 || result.innerNonSharedNode()->hasTagName(HTMLNames::embedTag))) {
        RenderObject* object = result.innerNonSharedNode()->renderer();
        if (object && object->isWidget()) {
            Widget* widget = toRenderWidget(object)->widget();
            if (widget && widget->isPluginContainer()) {
                WebPluginContainerImpl* plugin = toPluginContainerImpl(widget);
                url = plugin->plugin()->linkAtPosition(result.roundedPointInInnerNodeFrame());
            }
        }
    }

    m_webView->client()->setMouseOverURL(url);
}

void ChromeClientImpl::setToolTip(const String& tooltipText, TextDirection dir)
{
    if (!m_webView->client())
        return;
    WebTextDirection textDirection = (dir == RTL) ?
        WebTextDirectionRightToLeft :
        WebTextDirectionLeftToRight;
    m_webView->client()->setToolTipText(
        tooltipText, textDirection);
}

void ChromeClientImpl::dispatchViewportPropertiesDidChange(const ViewportArguments& arguments) const
{
    m_webView->updatePageDefinedPageScaleConstraints(arguments);
}

void ChromeClientImpl::print(Frame* frame)
{
    if (m_webView->client())
        m_webView->client()->printPage(WebFrameImpl::fromFrame(frame));
}

PassOwnPtr<ColorChooser> ChromeClientImpl::createColorChooser(ColorChooserClient* chooserClient, const Color&)
{
    OwnPtr<ColorChooserUIController> controller;
    if (RuntimeEnabledFeatures::pagePopupEnabled())
        controller = adoptPtr(new ColorChooserPopupUIController(this, chooserClient));
    else
        controller = adoptPtr(new ColorChooserUIController(this, chooserClient));
    controller->openUI();
    return controller.release();
}
PassOwnPtr<WebColorChooser> ChromeClientImpl::createWebColorChooser(WebColorChooserClient* chooserClient, const WebColor& initialColor)
{
    WebViewClient* client = m_webView->client();
    if (!client)
        return nullptr;
    return adoptPtr(client->createColorChooser(chooserClient, initialColor));
}

PassRefPtr<DateTimeChooser> ChromeClientImpl::openDateTimeChooser(DateTimeChooserClient* pickerClient, const DateTimeChooserParameters& parameters)
{
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    return DateTimeChooserImpl::create(this, pickerClient, parameters);
#else
    return ExternalDateTimeChooser::create(this, m_webView->client(), pickerClient, parameters);
#endif
}

void ChromeClientImpl::runOpenPanel(Frame* frame, PassRefPtr<FileChooser> fileChooser)
{
    WebViewClient* client = m_webView->client();
    if (!client)
        return;

    WebFileChooserParams params;
    params.multiSelect = fileChooser->settings().allowsMultipleFiles;
    params.directory = fileChooser->settings().allowsDirectoryUpload;
    params.acceptTypes = fileChooser->settings().acceptTypes();
    params.selectedFiles = fileChooser->settings().selectedFiles;
    if (params.selectedFiles.size() > 0)
        params.initialValue = params.selectedFiles[0];
#if ENABLE(MEDIA_CAPTURE)
    params.useMediaCapture = fileChooser->settings().useMediaCapture;
#endif
    WebFileChooserCompletionImpl* chooserCompletion =
        new WebFileChooserCompletionImpl(fileChooser);

    if (client->runFileChooser(params, chooserCompletion))
        return;

    // Choosing failed, so do callback with an empty list.
    chooserCompletion->didChooseFile(WebVector<WebString>());
}

void ChromeClientImpl::enumerateChosenDirectory(FileChooser* fileChooser)
{
    WebViewClient* client = m_webView->client();
    if (!client)
        return;

    WebFileChooserCompletionImpl* chooserCompletion =
        new WebFileChooserCompletionImpl(fileChooser);

    ASSERT(fileChooser && fileChooser->settings().selectedFiles.size());

    // If the enumeration can't happen, call the callback with an empty list.
    if (!client->enumerateChosenDirectory(fileChooser->settings().selectedFiles[0], chooserCompletion))
        chooserCompletion->didChooseFile(WebVector<WebString>());
}

void ChromeClientImpl::popupOpened(PopupContainer* popupContainer,
                                   const IntRect& bounds,
                                   bool handleExternally)
{
    // For Autofill popups, if the popup will not be fully visible, we shouldn't
    // show it at all. Among other things, this prevents users from being able
    // to interact via the keyboard with an invisible popup.
    if (popupContainer->popupType() == PopupContainer::Suggestion) {
        FrameView* view = m_webView->page()->mainFrame()->view();
        IntRect visibleRect = view->visibleContentRect(ScrollableArea::IncludeScrollbars);
        // |bounds| is in screen coordinates, so make sure to convert it to
        // content coordinates prior to comparing to |visibleRect|.
        IntRect screenRect = bounds;
        screenRect.setLocation(view->screenToContents(bounds.location()));
        if (!visibleRect.contains(screenRect)) {
            m_webView->hideAutofillPopup();
            return;
        }
    }

    if (!m_webView->client())
        return;

    WebWidget* webwidget;
    if (handleExternally) {
        WebPopupMenuInfo popupInfo;
        getPopupMenuInfo(popupContainer, &popupInfo);
        webwidget = m_webView->client()->createPopupMenu(popupInfo);
    } else {
        webwidget = m_webView->client()->createPopupMenu(
            convertPopupType(popupContainer->popupType()));
        // We only notify when the WebView has to handle the popup, as when
        // the popup is handled externally, the fact that a popup is showing is
        // transparent to the WebView.
        m_webView->popupOpened(popupContainer);
    }
    static_cast<WebPopupMenuImpl*>(webwidget)->initialize(popupContainer, bounds);
}

void ChromeClientImpl::popupClosed(WebCore::PopupContainer* popupContainer)
{
    m_webView->popupClosed(popupContainer);
}

void ChromeClientImpl::setCursor(const WebCore::Cursor& cursor)
{
    setCursor(WebCursorInfo(cursor));
}

void ChromeClientImpl::setCursor(const WebCursorInfo& cursor)
{
#if OS(MACOSX)
    // On Mac the mousemove event propagates to both the popup and main window.
    // If a popup is open we don't want the main window to change the cursor.
    if (m_webView->hasOpenedPopup())
        return;
#endif
    if (m_webView->client())
        m_webView->client()->didChangeCursor(cursor);
}

void ChromeClientImpl::setCursorForPlugin(const WebCursorInfo& cursor)
{
    setCursor(cursor);
}

void ChromeClientImpl::formStateDidChange(const Node* node)
{
    if (m_webView->client())
        m_webView->client()->didChangeFormState(WebNode(const_cast<Node*>(node)));

    // The current history item is not updated yet.  That happens lazily when
    // WebFrame::currentHistoryItem is requested.
    WebFrameImpl* webframe = WebFrameImpl::fromFrame(node->document().frame());
    if (webframe->client())
        webframe->client()->didUpdateCurrentHistoryItem(webframe);
}

void ChromeClientImpl::getPopupMenuInfo(PopupContainer* popupContainer,
                                        WebPopupMenuInfo* info)
{
    const Vector<PopupItem*>& inputItems = popupContainer->popupData();

    WebVector<WebMenuItemInfo> outputItems(inputItems.size());

    for (size_t i = 0; i < inputItems.size(); ++i) {
        const PopupItem& inputItem = *inputItems[i];
        WebMenuItemInfo& outputItem = outputItems[i];

        outputItem.label = inputItem.label;
        outputItem.enabled = inputItem.enabled;
        if (inputItem.textDirection == WebCore::RTL)
            outputItem.textDirection = WebTextDirectionRightToLeft;
        else
            outputItem.textDirection = WebTextDirectionLeftToRight;
        outputItem.hasTextDirectionOverride = inputItem.hasTextDirectionOverride;

        switch (inputItem.type) {
        case PopupItem::TypeOption:
            outputItem.type = WebMenuItemInfo::Option;
            break;
        case PopupItem::TypeGroup:
            outputItem.type = WebMenuItemInfo::Group;
            break;
        case PopupItem::TypeSeparator:
            outputItem.type = WebMenuItemInfo::Separator;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    info->itemHeight = popupContainer->menuItemHeight();
    info->itemFontSize = popupContainer->menuItemFontSize();
    info->selectedIndex = popupContainer->selectedIndex();
    info->items.swap(outputItems);
    info->rightAligned = popupContainer->menuStyle().textDirection() == RTL;
}

void ChromeClientImpl::postAccessibilityNotification(AccessibilityObject* obj, AXObjectCache::AXNotification notification)
{
    // Alert assistive technology about the accessibility object notification.
    if (!obj)
        return;

    m_webView->client()->postAccessibilityEvent(WebAXObject(obj), toWebAXEvent(notification));
}

String ChromeClientImpl::acceptLanguages()
{
    return m_webView->client()->acceptLanguages();
}

bool ChromeClientImpl::paintCustomOverhangArea(GraphicsContext* context, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect)
{
    Frame* frame = m_webView->mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        return pluginContainer->paintCustomOverhangArea(context, horizontalOverhangArea, verticalOverhangArea, dirtyRect);
    return false;
}

GraphicsLayerFactory* ChromeClientImpl::graphicsLayerFactory() const
{
    return m_webView->graphicsLayerFactory();
}

void ChromeClientImpl::attachRootGraphicsLayer(Frame* frame, GraphicsLayer* graphicsLayer)
{
    m_webView->setRootGraphicsLayer(graphicsLayer);
}

void ChromeClientImpl::scheduleCompositingLayerFlush()
{
    m_webView->scheduleCompositingLayerSync();
}

ChromeClient::CompositingTriggerFlags ChromeClientImpl::allowedCompositingTriggers() const
{
    if (!m_webView->allowsAcceleratedCompositing())
        return 0;

    CompositingTriggerFlags flags = 0;
    Settings& settings = m_webView->page()->settings();
    if (settings.acceleratedCompositingFor3DTransformsEnabled())
        flags |= ThreeDTransformTrigger;
    if (settings.acceleratedCompositingForVideoEnabled())
        flags |= VideoTrigger;
    if (settings.acceleratedCompositingForPluginsEnabled())
        flags |= PluginTrigger;
    if (settings.acceleratedCompositingForAnimationEnabled())
        flags |= AnimationTrigger;
    if (settings.acceleratedCompositingForCanvasEnabled())
        flags |= CanvasTrigger;
    if (settings.acceleratedCompositingForScrollableFramesEnabled())
        flags |= ScrollableInnerFrameTrigger;
    if (settings.acceleratedCompositingForFiltersEnabled())
        flags |= FilterTrigger;

    return flags;
}

void ChromeClientImpl::enterFullScreenForElement(Element* element)
{
    m_webView->enterFullScreenForElement(element);
}

void ChromeClientImpl::exitFullScreenForElement(Element* element)
{
    m_webView->exitFullScreenForElement(element);
}

bool ChromeClientImpl::hasOpenedPopup() const
{
    return m_webView->hasOpenedPopup();
}

PassRefPtr<PopupMenu> ChromeClientImpl::createPopupMenu(Frame& frame, PopupMenuClient* client) const
{
    if (WebViewImpl::useExternalPopupMenus())
        return adoptRef(new ExternalPopupMenu(frame, client, m_webView->client()));

    return adoptRef(new PopupMenuChromium(frame, client));
}

PagePopup* ChromeClientImpl::openPagePopup(PagePopupClient* client, const IntRect& originBoundsInRootView)
{
    ASSERT(m_pagePopupDriver);
    return m_pagePopupDriver->openPagePopup(client, originBoundsInRootView);
}

void ChromeClientImpl::closePagePopup(PagePopup* popup)
{
    ASSERT(m_pagePopupDriver);
    m_pagePopupDriver->closePagePopup(popup);
}

void ChromeClientImpl::setPagePopupDriver(PagePopupDriver* driver)
{
    ASSERT(driver);
    m_pagePopupDriver = driver;
}

void ChromeClientImpl::resetPagePopupDriver()
{
    m_pagePopupDriver = m_webView;
}

bool ChromeClientImpl::isPasswordGenerationEnabled() const
{
    return m_webView->passwordGeneratorClient();
}

void ChromeClientImpl::openPasswordGenerator(HTMLInputElement* input)
{
    ASSERT(isPasswordGenerationEnabled());
    WebInputElement webInput(input);
    m_webView->passwordGeneratorClient()->openPasswordGenerator(webInput);
}

bool ChromeClientImpl::shouldRunModalDialogDuringPageDismissal(const DialogType& dialogType, const String& dialogMessage, Document::PageDismissalType dismissalType) const
{
    const char* kDialogs[] = {"alert", "confirm", "prompt", "showModalDialog"};
    int dialog = static_cast<int>(dialogType);
    ASSERT_WITH_SECURITY_IMPLICATION(0 <= dialog && dialog < static_cast<int>(arraysize(kDialogs)));

    const char* kDismissals[] = {"beforeunload", "pagehide", "unload"};
    int dismissal = static_cast<int>(dismissalType) - 1; // Exclude NoDismissal.
    ASSERT_WITH_SECURITY_IMPLICATION(0 <= dismissal && dismissal < static_cast<int>(arraysize(kDismissals)));

    WebKit::Platform::current()->histogramEnumeration("Renderer.ModalDialogsDuringPageDismissal", dismissal * arraysize(kDialogs) + dialog, arraysize(kDialogs) * arraysize(kDismissals));

    String message = String("Blocked ") + kDialogs[dialog] + "('" + dialogMessage + "') during " + kDismissals[dismissal] + ".";
    m_webView->mainFrame()->addMessageToConsole(WebConsoleMessage(WebConsoleMessage::LevelError, message));

    return false;
}

bool ChromeClientImpl::shouldRubberBandInDirection(WebCore::ScrollDirection direction) const
{
    ASSERT(direction != WebCore::ScrollUp && direction != WebCore::ScrollDown);

    if (!m_webView->client())
        return false;

    if (direction == WebCore::ScrollLeft)
        return !m_webView->client()->historyBackListCount();
    if (direction == WebCore::ScrollRight)
        return !m_webView->client()->historyForwardListCount();

    ASSERT_NOT_REACHED();
    return true;
}

void ChromeClientImpl::numWheelEventHandlersChanged(unsigned numberOfWheelHandlers)
{
    m_webView->numberOfWheelEventHandlersChanged(numberOfWheelHandlers);
}

void ChromeClientImpl::needTouchEvents(bool needsTouchEvents)
{
    m_webView->hasTouchEventHandlers(needsTouchEvents);
}

bool ChromeClientImpl::requestPointerLock()
{
    return m_webView->requestPointerLock();
}

void ChromeClientImpl::requestPointerUnlock()
{
    return m_webView->requestPointerUnlock();
}

bool ChromeClientImpl::isPointerLocked()
{
    return m_webView->isPointerLocked();
}

void ChromeClientImpl::annotatedRegionsChanged()
{
    WebViewClient* client = m_webView->client();
    if (client)
        client->draggableRegionsChanged();
}

void ChromeClientImpl::didAssociateFormControls(const Vector<RefPtr<Element> >& elements)
{
    if (!m_webView->autofillClient())
        return;
    WebVector<WebNode> elementVector(static_cast<size_t>(elements.size()));
    size_t elementsCount = elements.size();
    for (size_t i = 0; i < elementsCount; ++i)
        elementVector[i] = elements[i];
    m_webView->autofillClient()->didAssociateFormControls(elementVector);
}

#if ENABLE(NAVIGATOR_CONTENT_UTILS)
PassOwnPtr<NavigatorContentUtilsClientImpl> NavigatorContentUtilsClientImpl::create(WebViewImpl* webView)
{
    return adoptPtr(new NavigatorContentUtilsClientImpl(webView));
}

NavigatorContentUtilsClientImpl::NavigatorContentUtilsClientImpl(WebViewImpl* webView)
    : m_webView(webView)
{
}

void NavigatorContentUtilsClientImpl::registerProtocolHandler(const String& scheme, const String& baseURL, const String& url, const String& title)
{
    m_webView->client()->registerProtocolHandler(scheme, baseURL, url, title);
}
#endif

} // namespace WebKit
