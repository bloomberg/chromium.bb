/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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
#include "WebViewImpl.h"

#include "public/platform/Platform.h"
#include "public/platform/WebDragData.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebGestureCurve.h"
#include "public/platform/WebImage.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "wtf/CurrentTime.h"
#include "wtf/MainThread.h"
#include "wtf/RefPtr.h"
#include "wtf/TemporaryChange.h"
#include "wtf/Uint8ClampedArray.h"
#include "AutofillPopupMenuClient.h"
#include "CSSValueKeywords.h"
#include "CompositionUnderlineVectorBuilder.h"
#include "ContextFeaturesClientImpl.h"
#include "DeviceOrientationClientProxy.h"
#include "FullscreenController.h"
#include "GeolocationClientProxy.h"
#include "GraphicsLayerFactoryChromium.h"
#include "HTMLNames.h"
#include "LinkHighlight.h"
#include "LocalFileSystemClient.h"
#include "MIDIClientProxy.h"
#include "PageWidgetDelegate.h"
#include "PinchViewports.h"
#include "PopupContainer.h"
#include "PrerendererClientImpl.h"
#include "RuntimeEnabledFeatures.h"
#include "SpeechInputClientImpl.h"
#include "SpeechRecognitionClientProxy.h"
#include "ValidationMessageClientImpl.h"
#include "ViewportAnchor.h"
#include "WebAXObject.h"
#include "WebActiveWheelFlingParameters.h"
#include "WebAutofillClient.h"
#include "WebDevToolsAgentImpl.h"
#include "WebDevToolsAgentPrivate.h"
#include "WebFrameImpl.h"
#include "WebHelperPluginImpl.h"
#include "WebHitTestResult.h"
#include "WebInputElement.h"
#include "WebInputEvent.h"
#include "WebInputEventConversion.h"
#include "WebMediaPlayerAction.h"
#include "WebNode.h"
#include "WebPagePopupImpl.h"
#include "WebPlugin.h"
#include "WebPluginAction.h"
#include "WebPluginContainerImpl.h"
#include "WebPopupMenuImpl.h"
#include "WebRange.h"
#include "WebSettingsImpl.h"
#include "WebTextInputInfo.h"
#include "WebViewClient.h"
#include "WebWindowFeatures.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/KeyboardEvent.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/Text.h"
#include "core/dom/WheelEvent.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/TextIterator.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/UniqueIdentifier.h"
#include "core/page/Chrome.h"
#include "core/page/ContextMenuController.h"
#include "core/page/DragController.h"
#include "core/page/DragSession.h"
#include "core/page/EventHandler.h"
#include "core/page/FocusController.h"
#include "core/page/Frame.h"
#include "core/page/FrameTree.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/PageGroup.h"
#include "core/page/PageGroupLoadDeferrer.h"
#include "core/page/PagePopupClient.h"
#include "core/page/PointerLockController.h"
#include "core/page/Settings.h"
#include "core/page/TouchDisambiguation.h"
#include "core/platform/ContextMenu.h"
#include "core/platform/ContextMenuItem.h"
#include "core/platform/Cursor.h"
#include "core/platform/DragData.h"
#include "core/platform/MIMETypeRegistry.h"
#include "core/platform/NotImplemented.h"
#include "core/platform/PlatformGestureEvent.h"
#include "core/platform/PlatformKeyboardEvent.h"
#include "core/platform/PlatformMouseEvent.h"
#include "core/platform/PlatformWheelEvent.h"
#include "core/platform/PopupMenuClient.h"
#include "core/platform/Timer.h"
#include "core/platform/chromium/ChromiumDataObject.h"
#include "core/platform/chromium/KeyboardCodes.h"
#include "core/platform/chromium/TraceEvent.h"
#include "core/platform/chromium/support/WebActiveGestureAnimation.h"
#include "core/platform/graphics/Color.h"
#include "core/platform/graphics/ColorSpace.h"
#include "core/platform/graphics/Extensions3D.h"
#include "core/platform/graphics/FontCache.h"
#include "core/platform/graphics/FontDescription.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/GraphicsContext3D.h"
#include "core/platform/graphics/Image.h"
#include "core/platform/graphics/ImageBuffer.h"
#include "core/platform/graphics/chromium/LayerPainterChromium.h"
#include "core/platform/graphics/gpu/SharedGraphicsContext3D.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/RenderWidget.h"
#include "core/rendering/TextAutosizer.h"
#include "modules/geolocation/GeolocationController.h"
#include "weborigin/SchemeRegistry.h"
#include "weborigin/SecurityOrigin.h"
#include "weborigin/SecurityPolicy.h"
#include "painting/ContinuousPainter.h"

#if ENABLE(DEFAULT_RENDER_THEME)
#include "core/platform/chromium/PlatformThemeChromiumDefault.h"
#include "core/rendering/RenderThemeChromiumDefault.h"
#endif

#if OS(WIN)
#if !ENABLE(DEFAULT_RENDER_THEME)
#include "core/rendering/RenderThemeChromiumWin.h"
#endif
#else
#include "core/rendering/RenderTheme.h"
#endif

// Get rid of WTF's pow define so we can use std::pow.
#undef pow
#include <cmath> // for std::pow

using namespace WebCore;
using namespace std;

// The following constants control parameters for automated scaling of webpages
// (such as due to a double tap gesture or find in page etc.). These are
// experimentally determined.
static const int touchPointPadding = 32;
static const int nonUserInitiatedPointPadding = 11;
static const float minScaleDifference = 0.01f;
static const float doubleTapZoomContentDefaultMargin = 5;
static const float doubleTapZoomContentMinimumMargin = 2;
static const double doubleTapZoomAnimationDurationInSeconds = 0.25;
static const float doubleTapZoomAlreadyLegibleRatio = 1.2f;

static const double multipleTargetsZoomAnimationDurationInSeconds = 0.25;
static const double findInPageAnimationDurationInSeconds = 0;

// Constants for viewport anchoring on resize.
static const float viewportAnchorXCoord = 0.5f;
static const float viewportAnchorYCoord = 0;

// Constants for zooming in on a focused text field.
static const double scrollAndScaleAnimationDurationInSeconds = 0.2;
static const int minReadableCaretHeight = 18;
static const float minScaleChangeToTriggerZoom = 1.05f;
static const float leftBoxRatio = 0.3f;
static const int caretPadding = 10;

namespace WebKit {

// Change the text zoom level by kTextSizeMultiplierRatio each time the user
// zooms text in or out (ie., change by 20%).  The min and max values limit
// text zoom to half and 3x the original text size.  These three values match
// those in Apple's port in WebKit/WebKit/WebView/WebView.mm
const double WebView::textSizeMultiplierRatio = 1.2;
const double WebView::minTextSizeMultiplier = 0.5;
const double WebView::maxTextSizeMultiplier = 3.0;

// Used to defer all page activity in cases where the embedder wishes to run
// a nested event loop. Using a stack enables nesting of message loop invocations.
static Vector<PageGroupLoadDeferrer*>& pageGroupLoadDeferrerStack()
{
    DEFINE_STATIC_LOCAL(Vector<PageGroupLoadDeferrer*>, deferrerStack, ());
    return deferrerStack;
}

// Ensure that the WebDragOperation enum values stay in sync with the original
// DragOperation constants.
#define COMPILE_ASSERT_MATCHING_ENUM(coreName) \
    COMPILE_ASSERT(int(coreName) == int(Web##coreName), dummy##coreName)
COMPILE_ASSERT_MATCHING_ENUM(DragOperationNone);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationCopy);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationLink);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationGeneric);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationPrivate);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationMove);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationDelete);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationEvery);

static const PopupContainerSettings autofillPopupSettings = {
    false, // setTextOnIndexChange
    false, // acceptOnAbandon
    true, // loopSelectionNavigation
    false // restrictWidthOfListBox (For security reasons show the entire entry
          // so the user doesn't enter information he did not intend to.)
};

static bool shouldUseExternalPopupMenus = false;

static int webInputEventKeyStateToPlatformEventKeyState(int webInputEventKeyState)
{
    int platformEventKeyState = 0;
    if (webInputEventKeyState & WebInputEvent::ShiftKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::ShiftKey;
    if (webInputEventKeyState & WebInputEvent::ControlKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::CtrlKey;
    if (webInputEventKeyState & WebInputEvent::AltKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::AltKey;
    if (webInputEventKeyState & WebInputEvent::MetaKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::MetaKey;
    return platformEventKeyState;
}

// WebView ----------------------------------------------------------------

WebView* WebView::create(WebViewClient* client)
{
    // Pass the WebViewImpl's self-reference to the caller.
    return adoptRef(new WebViewImpl(client)).leakRef();
}

void WebView::setUseExternalPopupMenus(bool useExternalPopupMenus)
{
    shouldUseExternalPopupMenus = useExternalPopupMenus;
}

void WebView::updateVisitedLinkState(unsigned long long linkHash)
{
    Page::visitedStateChanged(PageGroup::sharedGroup(), linkHash);
}

void WebView::resetVisitedLinkState()
{
    Page::allVisitedStateChanged(PageGroup::sharedGroup());
}

void WebView::willEnterModalLoop()
{
    PageGroup* pageGroup = PageGroup::sharedGroup();

    // We allow deferring inspector only when it is running in a separate process (no pages in a shared group)
    // to support debugger tests in DRT.
    if (pageGroup->pages().isEmpty())
        pageGroup = PageGroup::inspectorGroup();

    if (pageGroup->pages().isEmpty())
        pageGroupLoadDeferrerStack().append(static_cast<PageGroupLoadDeferrer*>(0));
    else {
        // Pick any page in the page group since we are deferring all pages.
        pageGroupLoadDeferrerStack().append(new PageGroupLoadDeferrer(*pageGroup->pages().begin(), true));
    }
}

void WebView::didExitModalLoop()
{
    ASSERT(pageGroupLoadDeferrerStack().size());

    delete pageGroupLoadDeferrerStack().last();
    pageGroupLoadDeferrerStack().removeLast();
}

void WebViewImpl::initializeMainFrame(WebFrameClient* frameClient)
{
    // NOTE: The WebFrameImpl takes a reference to itself within InitMainFrame
    // and releases that reference once the corresponding Frame is destroyed.
    RefPtr<WebFrameImpl> frame = WebFrameImpl::create(frameClient);

    frame->initializeAsMainFrame(page());
}

void WebViewImpl::initializeHelperPluginFrame(WebFrameClient* client)
{
    RefPtr<WebFrameImpl> frame = WebFrameImpl::create(client);
}

void WebViewImpl::setAutofillClient(WebAutofillClient* autofillClient)
{
    m_autofillClient = autofillClient;
}

void WebViewImpl::setDevToolsAgentClient(WebDevToolsAgentClient* devToolsClient)
{
    if (devToolsClient)
        m_devToolsAgent = adoptPtr(new WebDevToolsAgentImpl(this, devToolsClient));
    else
        m_devToolsAgent.clear();
}

void WebViewImpl::setValidationMessageClient(WebValidationMessageClient* client)
{
    ASSERT(client);
    m_validationMessage = ValidationMessageClientImpl::create(*this, *client);
    m_page->setValidationMessageClient(m_validationMessage.get());
}

void WebViewImpl::setPermissionClient(WebPermissionClient* permissionClient)
{
    m_permissionClient = permissionClient;
    m_featureSwitchClient->setPermissionClient(permissionClient);
}

void WebViewImpl::setPrerendererClient(WebPrerendererClient* prerendererClient)
{
    providePrerendererClientTo(m_page.get(), new PrerendererClientImpl(prerendererClient));
}

void WebViewImpl::setSpellCheckClient(WebSpellCheckClient* spellCheckClient)
{
    m_spellCheckClient = spellCheckClient;
}

void WebViewImpl::setPasswordGeneratorClient(WebPasswordGeneratorClient* client)
{
    m_passwordGeneratorClient = client;
}

WebViewImpl::WebViewImpl(WebViewClient* client)
    : m_client(client)
    , m_autofillClient(0)
    , m_permissionClient(0)
    , m_spellCheckClient(0)
    , m_passwordGeneratorClient(0)
    , m_chromeClientImpl(this)
    , m_contextMenuClientImpl(this)
    , m_dragClientImpl(this)
    , m_editorClientImpl(this)
    , m_inspectorClientImpl(this)
    , m_backForwardClientImpl(this)
    , m_shouldAutoResize(false)
    , m_observedNewNavigation(false)
#ifndef NDEBUG
    , m_newNavigationLoader(0)
#endif
    , m_zoomLevel(0)
    , m_minimumZoomLevel(zoomFactorToZoomLevel(minTextSizeMultiplier))
    , m_maximumZoomLevel(zoomFactorToZoomLevel(maxTextSizeMultiplier))
    , m_savedPageScaleFactor(0)
    , m_doubleTapZoomPageScaleFactor(0)
    , m_doubleTapZoomPending(false)
    , m_enableFakePageScaleAnimationForTesting(false)
    , m_fakePageScaleAnimationPageScaleFactor(0)
    , m_fakePageScaleAnimationUseAnchor(false)
    , m_contextMenuAllowed(false)
    , m_doingDragAndDrop(false)
    , m_ignoreInputEvents(false)
    , m_suppressNextKeypressEvent(false)
    , m_imeAcceptEvents(true)
    , m_operationsAllowed(WebDragOperationNone)
    , m_dragOperation(WebDragOperationNone)
    , m_featureSwitchClient(adoptPtr(new ContextFeaturesClientImpl()))
    , m_autofillPopupShowing(false)
    , m_autofillPopup(0)
    , m_isTransparent(false)
    , m_tabsToLinks(false)
    , m_layerTreeView(0)
    , m_rootLayer(0)
    , m_rootGraphicsLayer(0)
    , m_graphicsLayerFactory(adoptPtr(new GraphicsLayerFactoryChromium(this)))
    , m_isAcceleratedCompositingActive(false)
    , m_layerTreeViewCommitsDeferred(false)
    , m_compositorCreationFailed(false)
    , m_recreatingGraphicsContext(false)
#if ENABLE(INPUT_SPEECH)
    , m_speechInputClient(SpeechInputClientImpl::create(client))
#endif
    , m_speechRecognitionClient(SpeechRecognitionClientProxy::create(client ? client->speechRecognizer() : 0))
    , m_deviceOrientationClientProxy(adoptPtr(new DeviceOrientationClientProxy(client ? client->deviceOrientationClient() : 0)))
    , m_geolocationClientProxy(adoptPtr(new GeolocationClientProxy(client ? client->geolocationClient() : 0)))
    , m_userMediaClientImpl(this)
    , m_midiClientProxy(adoptPtr(new MIDIClientProxy(client ? client->webMIDIClient() : 0)))
#if ENABLE(NAVIGATOR_CONTENT_UTILS)
    , m_navigatorContentUtilsClient(NavigatorContentUtilsClientImpl::create(this))
#endif
    , m_flingModifier(0)
    , m_flingSourceDevice(false)
    , m_fullscreenController(FullscreenController::create(this))
    , m_showFPSCounter(false)
    , m_showPaintRects(false)
    , m_showDebugBorders(false)
    , m_continuousPaintingEnabled(false)
    , m_showScrollBottleneckRects(false)
    , m_baseBackgroundColor(Color::white)
    , m_helperPluginCloseTimer(this, &WebViewImpl::closePendingHelperPlugins)
{
    Page::PageClients pageClients;
    pageClients.chromeClient = &m_chromeClientImpl;
    pageClients.contextMenuClient = &m_contextMenuClientImpl;
    pageClients.editorClient = &m_editorClientImpl;
    pageClients.dragClient = &m_dragClientImpl;
    pageClients.inspectorClient = &m_inspectorClientImpl;
    pageClients.backForwardClient = &m_backForwardClientImpl;

    m_page = adoptPtr(new Page(pageClients));
    provideUserMediaTo(m_page.get(), &m_userMediaClientImpl);
    provideMIDITo(m_page.get(), m_midiClientProxy.get());
#if ENABLE(INPUT_SPEECH)
    provideSpeechInputTo(m_page.get(), m_speechInputClient.get());
#endif
    provideSpeechRecognitionTo(m_page.get(), m_speechRecognitionClient.get());
    provideNotification(m_page.get(), notificationPresenterImpl());
#if ENABLE(NAVIGATOR_CONTENT_UTILS)
    provideNavigatorContentUtilsTo(m_page.get(), m_navigatorContentUtilsClient.get());
#endif

    provideContextFeaturesTo(m_page.get(), m_featureSwitchClient.get());
    provideDeviceOrientationTo(m_page.get(), m_deviceOrientationClientProxy.get());
    provideGeolocationTo(m_page.get(), m_geolocationClientProxy.get());
    m_geolocationClientProxy->setController(GeolocationController::from(m_page.get()));

    provideLocalFileSystemTo(m_page.get(), LocalFileSystemClient::create());

    m_page->setGroupType(Page::SharedPageGroup);

    unsigned layoutMilestones = DidFirstLayout | DidFirstVisuallyNonEmptyLayout;
    m_page->addLayoutMilestones(static_cast<LayoutMilestones>(layoutMilestones));

    if (m_client) {
        setDeviceScaleFactor(m_client->screenInfo().deviceScaleFactor);
        setVisibilityState(m_client->visibilityState(), true);
    }

    m_inspectorSettingsMap = adoptPtr(new SettingsMap);
}

WebViewImpl::~WebViewImpl()
{
    ASSERT(!m_page);
    ASSERT(!m_helperPluginCloseTimer.isActive());
    ASSERT(m_helperPluginsPendingClose.isEmpty());
}

WebFrameImpl* WebViewImpl::mainFrameImpl()
{
    return m_page ? WebFrameImpl::fromFrame(m_page->mainFrame()) : 0;
}

bool WebViewImpl::tabKeyCyclesThroughElements() const
{
    ASSERT(m_page);
    return m_page->tabKeyCyclesThroughElements();
}

void WebViewImpl::setTabKeyCyclesThroughElements(bool value)
{
    if (m_page)
        m_page->setTabKeyCyclesThroughElements(value);
}

void WebViewImpl::handleMouseLeave(Frame& mainFrame, const WebMouseEvent& event)
{
    m_client->setMouseOverURL(WebURL());
    PageWidgetEventHandler::handleMouseLeave(mainFrame, event);
}

void WebViewImpl::handleMouseDown(Frame& mainFrame, const WebMouseEvent& event)
{
    // If there is a popup open, close it as the user is clicking on the page (outside of the
    // popup). We also save it so we can prevent a click on an element from immediately
    // reopening the same popup.
    RefPtr<WebCore::PopupContainer> selectPopup;
    RefPtr<WebPagePopupImpl> pagePopup;
    if (event.button == WebMouseEvent::ButtonLeft) {
        selectPopup = m_selectPopup;
        pagePopup = m_pagePopup;
        hidePopups();
        ASSERT(!m_selectPopup);
        ASSERT(!m_pagePopup);
    }

    m_lastMouseDownPoint = WebPoint(event.x, event.y);

    if (event.button == WebMouseEvent::ButtonLeft) {
        IntPoint point(event.x, event.y);
        point = m_page->mainFrame()->view()->windowToContents(point);
        HitTestResult result(m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(point));
        Node* hitNode = result.innerNonSharedNode();

        // Take capture on a mouse down on a plugin so we can send it mouse events.
        if (hitNode && hitNode->renderer() && hitNode->renderer()->isEmbeddedObject()) {
            m_mouseCaptureNode = hitNode;
            TRACE_EVENT_ASYNC_BEGIN0("input", "capturing mouse", this);
        }
    }

    PageWidgetEventHandler::handleMouseDown(mainFrame, event);

    if (m_selectPopup && m_selectPopup == selectPopup) {
        // That click triggered a select popup which is the same as the one that
        // was showing before the click.  It means the user clicked the select
        // while the popup was showing, and as a result we first closed then
        // immediately reopened the select popup.  It needs to be closed.
        hideSelectPopup();
    }

    if (m_pagePopup && pagePopup && m_pagePopup->hasSamePopupClient(pagePopup.get())) {
        // That click triggered a page popup that is the same as the one we just closed.
        // It needs to be closed.
        closePagePopup(m_pagePopup.get());
    }

    // Dispatch the contextmenu event regardless of if the click was swallowed.
#if OS(WIN)
    // On Windows, we handle it on mouse up, not down.
#elif OS(MACOSX)
    if (event.button == WebMouseEvent::ButtonRight
        || (event.button == WebMouseEvent::ButtonLeft
            && event.modifiers & WebMouseEvent::ControlKey))
        mouseContextMenu(event);
#else
    if (event.button == WebMouseEvent::ButtonRight)
        mouseContextMenu(event);
#endif
}

void WebViewImpl::mouseContextMenu(const WebMouseEvent& event)
{
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    m_page->contextMenuController().clearContextMenu();

    PlatformMouseEventBuilder pme(mainFrameImpl()->frameView(), event);

    // Find the right target frame. See issue 1186900.
    HitTestResult result = hitTestResultForWindowPos(pme.position());
    Frame* targetFrame;
    if (result.innerNonSharedNode())
        targetFrame = result.innerNonSharedNode()->document().frame();
    else
        targetFrame = m_page->focusController().focusedOrMainFrame();

#if OS(WIN)
    targetFrame->view()->setCursor(pointerCursor());
#endif

    m_contextMenuAllowed = true;
    targetFrame->eventHandler()->sendContextMenuEvent(pme);
    m_contextMenuAllowed = false;
    // Actually showing the context menu is handled by the ContextMenuClient
    // implementation...
}

void WebViewImpl::handleMouseUp(Frame& mainFrame, const WebMouseEvent& event)
{
    PageWidgetEventHandler::handleMouseUp(mainFrame, event);

#if OS(WIN)
    // Dispatch the contextmenu event regardless of if the click was swallowed.
    // On Mac/Linux, we handle it on mouse down, not up.
    if (event.button == WebMouseEvent::ButtonRight)
        mouseContextMenu(event);
#endif
}

bool WebViewImpl::handleMouseWheel(Frame& mainFrame, const WebMouseWheelEvent& event)
{
    hidePopups();
    return PageWidgetEventHandler::handleMouseWheel(mainFrame, event);
}

void WebViewImpl::scrollBy(const WebFloatSize& delta)
{
    if (m_flingSourceDevice == WebGestureEvent::Touchpad) {
        WebMouseWheelEvent syntheticWheel;
        const float tickDivisor = WebCore::WheelEvent::TickMultiplier;

        syntheticWheel.deltaX = delta.width;
        syntheticWheel.deltaY = delta.height;
        syntheticWheel.wheelTicksX = delta.width / tickDivisor;
        syntheticWheel.wheelTicksY = delta.height / tickDivisor;
        syntheticWheel.hasPreciseScrollingDeltas = true;
        syntheticWheel.x = m_positionOnFlingStart.x;
        syntheticWheel.y = m_positionOnFlingStart.y;
        syntheticWheel.globalX = m_globalPositionOnFlingStart.x;
        syntheticWheel.globalY = m_globalPositionOnFlingStart.y;
        syntheticWheel.modifiers = m_flingModifier;

        if (m_page && m_page->mainFrame() && m_page->mainFrame()->view())
            handleMouseWheel(*m_page->mainFrame(), syntheticWheel);
    } else {
        WebGestureEvent syntheticGestureEvent;

        syntheticGestureEvent.type = WebInputEvent::GestureScrollUpdateWithoutPropagation;
        syntheticGestureEvent.data.scrollUpdate.deltaX = delta.width;
        syntheticGestureEvent.data.scrollUpdate.deltaY = delta.height;
        syntheticGestureEvent.x = m_positionOnFlingStart.x;
        syntheticGestureEvent.y = m_positionOnFlingStart.y;
        syntheticGestureEvent.globalX = m_globalPositionOnFlingStart.x;
        syntheticGestureEvent.globalY = m_globalPositionOnFlingStart.y;
        syntheticGestureEvent.modifiers = m_flingModifier;
        syntheticGestureEvent.sourceDevice = WebGestureEvent::Touchscreen;

        if (m_page && m_page->mainFrame() && m_page->mainFrame()->view())
            handleGestureEvent(syntheticGestureEvent);
    }
}

bool WebViewImpl::handleGestureEvent(const WebGestureEvent& event)
{
    bool eventSwallowed = false;
    bool eventCancelled = false; // for disambiguation

    // Special handling for slow-path fling gestures, which have no PlatformGestureEvent equivalent.
    switch (event.type) {
    case WebInputEvent::GestureFlingStart: {
        if (mainFrameImpl()->frame()->eventHandler()->isScrollbarHandlingGestures()) {
            m_client->didHandleGestureEvent(event, eventCancelled);
            return eventSwallowed;
        }
        m_client->cancelScheduledContentIntents();
        m_positionOnFlingStart = WebPoint(event.x / pageScaleFactor(), event.y / pageScaleFactor());
        m_globalPositionOnFlingStart = WebPoint(event.globalX, event.globalY);
        m_flingModifier = event.modifiers;
        m_flingSourceDevice = event.sourceDevice;
        OwnPtr<WebGestureCurve> flingCurve = adoptPtr(Platform::current()->createFlingAnimationCurve(event.sourceDevice, WebFloatPoint(event.data.flingStart.velocityX, event.data.flingStart.velocityY), WebSize()));
        m_gestureAnimation = WebActiveGestureAnimation::createAtAnimationStart(flingCurve.release(), this);
        scheduleAnimation();
        eventSwallowed = true;

        m_client->didHandleGestureEvent(event, eventCancelled);
        return eventSwallowed;
    }
    case WebInputEvent::GestureFlingCancel:
        if (m_gestureAnimation) {
            m_gestureAnimation.clear();
            if (m_layerTreeView)
                m_layerTreeView->didStopFlinging();
            eventSwallowed = true;
        }

        m_client->didHandleGestureEvent(event, eventCancelled);
        return eventSwallowed;
    default:
        break;
    }

    PlatformGestureEventBuilder platformEvent(mainFrameImpl()->frameView(), event);

    // Handle link highlighting outside the main switch to avoid getting lost in the
    // complicated set of cases handled below.
    switch (event.type) {
    case WebInputEvent::GestureTapDown:
        // Queue a highlight animation, then hand off to regular handler.
#if OS(LINUX)
        if (settingsImpl()->gestureTapHighlightEnabled())
            enableTapHighlight(platformEvent);
#endif
        break;
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureLongPress:
        if (m_linkHighlight)
            m_linkHighlight->startHighlightAnimationIfNeeded();
        break;
    default:
        break;
    }

    switch (event.type) {
    case WebInputEvent::GestureTap: {
        m_client->cancelScheduledContentIntents();
        if (detectContentOnTouch(platformEvent.position())) {
            eventSwallowed = true;
            break;
        }

        RefPtr<WebCore::PopupContainer> selectPopup;
        selectPopup = m_selectPopup;
        hideSelectPopup();
        ASSERT(!m_selectPopup);

        // Don't trigger a disambiguation popup on sites designed for mobile devices.
        // Instead, assume that the page has been designed with big enough buttons and links.
        if (event.data.tap.width > 0 && !shouldDisableDesktopWorkarounds()) {
            // FIXME: didTapMultipleTargets should just take a rect instead of
            // an event.
            WebGestureEvent scaledEvent = event;
            scaledEvent.x = event.x / pageScaleFactor();
            scaledEvent.y = event.y / pageScaleFactor();
            scaledEvent.data.tap.width = event.data.tap.width / pageScaleFactor();
            scaledEvent.data.tap.height = event.data.tap.height / pageScaleFactor();
            IntRect boundingBox(scaledEvent.x - scaledEvent.data.tap.width / 2, scaledEvent.y - scaledEvent.data.tap.height / 2, scaledEvent.data.tap.width, scaledEvent.data.tap.height);
            Vector<IntRect> goodTargets;
            findGoodTouchTargets(boundingBox, mainFrameImpl()->frame(), goodTargets);
            // FIXME: replace touch adjustment code when numberOfGoodTargets == 1?
            // Single candidate case is currently handled by: https://bugs.webkit.org/show_bug.cgi?id=85101
            if (goodTargets.size() >= 2 && m_client && m_client->didTapMultipleTargets(scaledEvent, goodTargets)) {
                eventSwallowed = true;
                eventCancelled = true;
                break;
            }
        }

        eventSwallowed = mainFrameImpl()->frame()->eventHandler()->handleGestureEvent(platformEvent);

        if (m_selectPopup && m_selectPopup == selectPopup) {
            // That tap triggered a select popup which is the same as the one that
            // was showing before the tap. It means the user tapped the select
            // while the popup was showing, and as a result we first closed then
            // immediately reopened the select popup. It needs to be closed.
            hideSelectPopup();
        }

        break;
    }
    case WebInputEvent::GestureTwoFingerTap:
    case WebInputEvent::GestureLongPress:
    case WebInputEvent::GestureLongTap: {
        if (!mainFrameImpl() || !mainFrameImpl()->frameView())
            break;

        m_client->cancelScheduledContentIntents();
        m_page->contextMenuController().clearContextMenu();
        m_contextMenuAllowed = true;
        eventSwallowed = mainFrameImpl()->frame()->eventHandler()->handleGestureEvent(platformEvent);
        m_contextMenuAllowed = false;

        break;
    }
    case WebInputEvent::GestureTapDown: {
        m_client->cancelScheduledContentIntents();
        eventSwallowed = mainFrameImpl()->frame()->eventHandler()->handleGestureEvent(platformEvent);
        break;
    }
    case WebInputEvent::GestureDoubleTap:
        if (m_webSettings->doubleTapToZoomEnabled() && minimumPageScaleFactor() != maximumPageScaleFactor()) {
            m_client->cancelScheduledContentIntents();
            animateDoubleTapZoom(platformEvent.position());
        }
        // GestureDoubleTap is currently only used by Android for zooming. For WebCore,
        // GestureTap with tap count = 2 is used instead. So we drop GestureDoubleTap here.
        eventSwallowed = true;
        break;
    case WebInputEvent::GestureScrollBegin:
    case WebInputEvent::GesturePinchBegin:
        m_client->cancelScheduledContentIntents();
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureScrollUpdateWithoutPropagation:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureTapUnconfirmed:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate: {
        eventSwallowed = mainFrameImpl()->frame()->eventHandler()->handleGestureEvent(platformEvent);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
    m_client->didHandleGestureEvent(event, eventCancelled);
    return eventSwallowed;
}

void WebViewImpl::transferActiveWheelFlingAnimation(const WebActiveWheelFlingParameters& parameters)
{
    TRACE_EVENT0("webkit", "WebViewImpl::transferActiveWheelFlingAnimation");
    ASSERT(!m_gestureAnimation);
    m_positionOnFlingStart = parameters.point;
    m_globalPositionOnFlingStart = parameters.globalPoint;
    m_flingModifier = parameters.modifiers;
    OwnPtr<WebGestureCurve> curve = adoptPtr(Platform::current()->createFlingAnimationCurve(parameters.sourceDevice, WebFloatPoint(parameters.delta), parameters.cumulativeScroll));
    m_gestureAnimation = WebActiveGestureAnimation::createWithTimeOffset(curve.release(), this, parameters.startTime);
    scheduleAnimation();
}

bool WebViewImpl::startPageScaleAnimation(const IntPoint& targetPosition, bool useAnchor, float newScale, double durationInSeconds)
{
    WebPoint clampedPoint = targetPosition;
    if (!useAnchor) {
        clampedPoint = clampOffsetAtScale(targetPosition, newScale);
        if (!durationInSeconds) {
            setPageScaleFactor(newScale, clampedPoint);
            return false;
        }
    }
    if (useAnchor && newScale == pageScaleFactor())
        return false;

    if (m_enableFakePageScaleAnimationForTesting) {
        m_fakePageScaleAnimationTargetPosition = targetPosition;
        m_fakePageScaleAnimationUseAnchor = useAnchor;
        m_fakePageScaleAnimationPageScaleFactor = newScale;
    } else {
        if (!m_layerTreeView)
            return false;
        m_layerTreeView->startPageScaleAnimation(targetPosition, useAnchor, newScale, durationInSeconds);
    }
    return true;
}

void WebViewImpl::enableFakePageScaleAnimationForTesting(bool enable)
{
    m_enableFakePageScaleAnimationForTesting = enable;
}

void WebViewImpl::setShowFPSCounter(bool show)
{
    if (m_layerTreeView) {
        TRACE_EVENT0("webkit", "WebViewImpl::setShowFPSCounter");
        m_layerTreeView->setShowFPSCounter(show);
    }
    m_showFPSCounter = show;
}

void WebViewImpl::setShowPaintRects(bool show)
{
    if (m_layerTreeView) {
        TRACE_EVENT0("webkit", "WebViewImpl::setShowPaintRects");
        m_layerTreeView->setShowPaintRects(show);
    }
    m_showPaintRects = show;
}

void WebViewImpl::setShowDebugBorders(bool show)
{
    if (m_layerTreeView)
        m_layerTreeView->setShowDebugBorders(show);
    m_showDebugBorders = show;
}

void WebViewImpl::setContinuousPaintingEnabled(bool enabled)
{
    if (m_layerTreeView) {
        TRACE_EVENT0("webkit", "WebViewImpl::setContinuousPaintingEnabled");
        m_layerTreeView->setContinuousPaintingEnabled(enabled);
    }
    m_continuousPaintingEnabled = enabled;
    m_client->scheduleAnimation();
}

void WebViewImpl::setShowScrollBottleneckRects(bool show)
{
    if (m_layerTreeView)
        m_layerTreeView->setShowScrollBottleneckRects(show);
    m_showScrollBottleneckRects = show;
}

bool WebViewImpl::handleKeyEvent(const WebKeyboardEvent& event)
{
    ASSERT((event.type == WebInputEvent::RawKeyDown)
        || (event.type == WebInputEvent::KeyDown)
        || (event.type == WebInputEvent::KeyUp));

    // Halt an in-progress fling on a key event.
    if (m_gestureAnimation) {
        m_gestureAnimation.clear();
        if (m_layerTreeView)
            m_layerTreeView->didStopFlinging();
    }

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.
    // The m_suppressNextKeypressEvent is set if the KeyDown is handled by
    // Webkit. A keyDown event is typically associated with a keyPress(char)
    // event and a keyUp event. We reset this flag here as this is a new keyDown
    // event.
    m_suppressNextKeypressEvent = false;

    // If there is a select popup, it should be the one processing the event,
    // not the page.
    if (m_selectPopup)
        return m_selectPopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
    if (m_pagePopup) {
        m_pagePopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
        // We need to ignore the next Char event after this otherwise pressing
        // enter when selecting an item in the popup will go to the page.
        if (WebInputEvent::RawKeyDown == event.type)
            m_suppressNextKeypressEvent = true;
        return true;
    }

    // Give Autocomplete a chance to consume the key events it is interested in.
    if (autocompleteHandleKeyEvent(event))
        return true;

    RefPtr<Frame> frame = focusedWebCoreFrame();
    if (!frame)
        return false;

    EventHandler* handler = frame->eventHandler();
    if (!handler)
        return keyEventDefault(event);

#if !OS(MACOSX)
    const WebInputEvent::Type contextMenuTriggeringEventType =
#if OS(WIN)
        WebInputEvent::KeyUp;
#else
        WebInputEvent::RawKeyDown;
#endif

    bool isUnmodifiedMenuKey = !(event.modifiers & WebInputEvent::InputModifiers) && event.windowsKeyCode == VKEY_APPS;
    bool isShiftF10 = event.modifiers == WebInputEvent::ShiftKey && event.windowsKeyCode == VKEY_F10;
    if ((isUnmodifiedMenuKey || isShiftF10) && event.type == contextMenuTriggeringEventType) {
        sendContextMenuEvent(event);
        return true;
    }
#endif // !OS(MACOSX)

    PlatformKeyboardEventBuilder evt(event);

    if (handler->keyEvent(evt)) {
        if (WebInputEvent::RawKeyDown == event.type) {
            // Suppress the next keypress event unless the focused node is a plug-in node.
            // (Flash needs these keypress events to handle non-US keyboards.)
            Element* element = focusedElement();
            if (!element || !element->renderer() || !element->renderer()->isEmbeddedObject())
                m_suppressNextKeypressEvent = true;
        }
        return true;
    }

    return keyEventDefault(event);
}

bool WebViewImpl::autocompleteHandleKeyEvent(const WebKeyboardEvent& event)
{
    if (!m_autofillPopupShowing
        // Home and End should be left to the text field to process.
        || event.windowsKeyCode == VKEY_HOME
        || event.windowsKeyCode == VKEY_END)
      return false;

    // Pressing delete triggers the removal of the selected suggestion from the DB.
    if (event.windowsKeyCode == VKEY_DELETE
        && m_autofillPopup->selectedIndex() != -1) {
        Element* element = focusedElement();
        if (!element) {
            ASSERT_NOT_REACHED();
            return false;
        }
        if (!element->hasTagName(HTMLNames::inputTag)) {
            ASSERT_NOT_REACHED();
            return false;
        }

        int selectedIndex = m_autofillPopup->selectedIndex();

        if (!m_autofillPopupClient->canRemoveSuggestionAtIndex(selectedIndex))
            return false;

        WebString name = WebInputElement(toHTMLInputElement(element)).nameForAutofill();
        WebString value = m_autofillPopupClient->itemText(selectedIndex);
        m_autofillClient->removeAutocompleteSuggestion(name, value);
        // Update the entries in the currently showing popup to reflect the
        // deletion.
        m_autofillPopupClient->removeSuggestionAtIndex(selectedIndex);
        refreshAutofillPopup();
        return false;
    }

    if (!m_autofillPopup->isInterestedInEventForKey(event.windowsKeyCode))
        return false;

    if (m_autofillPopup->handleKeyEvent(PlatformKeyboardEventBuilder(event))) {
        // We need to ignore the next Char event after this otherwise pressing
        // enter when selecting an item in the menu will go to the page.
        if (WebInputEvent::RawKeyDown == event.type)
            m_suppressNextKeypressEvent = true;
        return true;
    }

    return false;
}

bool WebViewImpl::handleCharEvent(const WebKeyboardEvent& event)
{
    ASSERT(event.type == WebInputEvent::Char);

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.  The m_suppressNextKeypressEvent is set if the KeyDown is
    // handled by Webkit. A keyDown event is typically associated with a
    // keyPress(char) event and a keyUp event. We reset this flag here as it
    // only applies to the current keyPress event.
    bool suppress = m_suppressNextKeypressEvent;
    m_suppressNextKeypressEvent = false;

    // If there is a select popup, it should be the one processing the event,
    // not the page.
    if (m_selectPopup)
        return m_selectPopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
    if (m_pagePopup)
        return m_pagePopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));

    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return suppress;

    EventHandler* handler = frame->eventHandler();
    if (!handler)
        return suppress || keyEventDefault(event);

    PlatformKeyboardEventBuilder evt(event);
    if (!evt.isCharacterKey())
        return true;

    // Accesskeys are triggered by char events and can't be suppressed.
    if (handler->handleAccessKey(evt))
        return true;

    // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
    // the eventHandler::keyEvent. We mimic this behavior on all platforms since
    // for now we are converting other platform's key events to windows key
    // events.
    if (evt.isSystemKey())
        return false;

    if (!suppress && !handler->keyEvent(evt))
        return keyEventDefault(event);

    return true;
}

WebRect WebViewImpl::computeBlockBounds(const WebRect& rect, bool ignoreClipping)
{
    if (!mainFrameImpl())
        return WebRect();

    // Use the rect-based hit test to find the node.
    IntPoint point = mainFrameImpl()->frameView()->windowToContents(IntPoint(rect.x, rect.y));
    HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent | (ignoreClipping ? HitTestRequest::IgnoreClipping : 0);
    HitTestResult result = mainFrameImpl()->frame()->eventHandler()->hitTestResultAtPoint(point, hitType, IntSize(rect.width, rect.height));

    Node* node = result.innerNonSharedNode();
    if (!node)
        return WebRect();

    // Find the block type node based on the hit node.
    while (node && (!node->renderer() || node->renderer()->isInline()))
        node = node->parentNode();

    // Return the bounding box in the window coordinate system.
    if (node) {
        IntRect rect = node->Node::pixelSnappedBoundingBox();
        Frame* frame = node->document().frame();
        return frame->view()->contentsToWindow(rect);
    }
    return WebRect();
}

WebRect WebViewImpl::widenRectWithinPageBounds(const WebRect& source, int targetMargin, int minimumMargin)
{
    WebSize maxSize;
    if (mainFrame())
        maxSize = mainFrame()->contentsSize();
    IntSize scrollOffset;
    if (mainFrame())
        scrollOffset = mainFrame()->scrollOffset();
    int leftMargin = targetMargin;
    int rightMargin = targetMargin;

    const int absoluteSourceX = source.x + scrollOffset.width();
    if (leftMargin > absoluteSourceX) {
        leftMargin = absoluteSourceX;
        rightMargin = max(leftMargin, minimumMargin);
    }

    const int maximumRightMargin = maxSize.width - (source.width + absoluteSourceX);
    if (rightMargin > maximumRightMargin) {
        rightMargin = maximumRightMargin;
        leftMargin = min(leftMargin, max(rightMargin, minimumMargin));
    }

    const int newWidth = source.width + leftMargin + rightMargin;
    const int newX = source.x - leftMargin;

    ASSERT(newWidth >= 0);
    ASSERT(scrollOffset.width() + newX + newWidth <= maxSize.width);

    return WebRect(newX, source.y, newWidth, source.height);
}

float WebViewImpl::legibleScale() const
{
    // Pages should be as legible as on desktop when at dpi scale, so no
    // need to zoom in further when automatically determining zoom level
    // (after double tap, find in page, etc), though the user should still
    // be allowed to manually pinch zoom in further if they desire.
    float legibleScale = 1;
    if (page() && page()->settings().textAutosizingEnabled())
        legibleScale *= page()->settings().textAutosizingFontScaleFactor();
    return legibleScale;
}

void WebViewImpl::computeScaleAndScrollForBlockRect(const WebRect& blockRect, float padding, float defaultScaleWhenAlreadyLegible, float& scale, WebPoint& scroll)
{
    scale = pageScaleFactor();
    scroll.x = scroll.y = 0;

    WebRect rect = blockRect;

    if (!rect.isEmpty()) {
        float defaultMargin = doubleTapZoomContentDefaultMargin;
        float minimumMargin = doubleTapZoomContentMinimumMargin;
        // We want the margins to have the same physical size, which means we
        // need to express them in post-scale size. To do that we'd need to know
        // the scale we're scaling to, but that depends on the margins. Instead
        // we express them as a fraction of the target rectangle: this will be
        // correct if we end up fully zooming to it, and won't matter if we
        // don't.
        rect = widenRectWithinPageBounds(rect,
                static_cast<int>(defaultMargin * rect.width / m_size.width),
                static_cast<int>(minimumMargin * rect.width / m_size.width));
        // Fit block to screen, respecting limits.
        scale = static_cast<float>(m_size.width) / rect.width;
        scale = min(scale, legibleScale());
        if (pageScaleFactor() < defaultScaleWhenAlreadyLegible)
            scale = max(scale, defaultScaleWhenAlreadyLegible);
        scale = clampPageScaleFactorToLimits(scale);
    }

    // FIXME: If this is being called for auto zoom during find in page,
    // then if the user manually zooms in it'd be nice to preserve the
    // relative increase in zoom they caused (if they zoom out then it's ok
    // to zoom them back in again). This isn't compatible with our current
    // double-tap zoom strategy (fitting the containing block to the screen)
    // though.

    float screenWidth = m_size.width / scale;
    float screenHeight = m_size.height / scale;

    // Scroll to vertically align the block.
    if (rect.height < screenHeight) {
        // Vertically center short blocks.
        rect.y -= 0.5 * (screenHeight - rect.height);
    } else {
        // Ensure position we're zooming to (+ padding) isn't off the bottom of
        // the screen.
        rect.y = max<float>(rect.y, blockRect.y + padding - screenHeight);
    } // Otherwise top align the block.

    // Do the same thing for horizontal alignment.
    if (rect.width < screenWidth)
        rect.x -= 0.5 * (screenWidth - rect.width);
    else
        rect.x = max<float>(rect.x, blockRect.x + padding - screenWidth);
    scroll.x = rect.x;
    scroll.y = rect.y;

    scale = clampPageScaleFactorToLimits(scale);
    scroll = mainFrameImpl()->frameView()->windowToContents(scroll);
    scroll = clampOffsetAtScale(scroll, scale);
}

static bool invokesHandCursor(Node* node, bool shiftKey, Frame* frame)
{
    if (!node || !node->renderer())
        return false;

    ECursor cursor = node->renderer()->style()->cursor();
    return cursor == CURSOR_POINTER
        || (cursor == CURSOR_AUTO && frame->eventHandler()->useHandCursor(node, node->isLink(), shiftKey));
}

Node* WebViewImpl::bestTapNode(const PlatformGestureEvent& tapEvent)
{
    if (!m_page || !m_page->mainFrame())
        return 0;

    Node* bestTouchNode = 0;

    IntPoint touchEventLocation(tapEvent.position());
    m_page->mainFrame()->eventHandler()->adjustGesturePosition(tapEvent, touchEventLocation);

    IntPoint hitTestPoint = m_page->mainFrame()->view()->windowToContents(touchEventLocation);
    HitTestResult result = m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(hitTestPoint, HitTestRequest::TouchEvent | HitTestRequest::DisallowShadowContent);
    bestTouchNode = result.targetNode();

    // Make sure our highlight candidate uses a hand cursor as a heuristic to
    // choose appropriate targets.
    while (bestTouchNode && !invokesHandCursor(bestTouchNode, false, m_page->mainFrame()))
        bestTouchNode = bestTouchNode->parentNode();

    // We should pick the largest enclosing node with hand cursor set.
    while (bestTouchNode && bestTouchNode->parentNode() && invokesHandCursor(bestTouchNode->parentNode(), false, m_page->mainFrame()))
        bestTouchNode = bestTouchNode->parentNode();

    return bestTouchNode;
}

void WebViewImpl::enableTapHighlight(const PlatformGestureEvent& tapEvent)
{
    // Always clear any existing highlight when this is invoked, even if we don't get a new target to highlight.
    m_linkHighlight.clear();

    Node* touchNode = bestTapNode(tapEvent);

    if (!touchNode || !touchNode->renderer() || !touchNode->renderer()->enclosingLayer())
        return;

    Color highlightColor = touchNode->renderer()->style()->tapHighlightColor();
    // Safari documentation for -webkit-tap-highlight-color says if the specified color has 0 alpha,
    // then tap highlighting is disabled.
    // http://developer.apple.com/library/safari/#documentation/appleapplications/reference/safaricssref/articles/standardcssproperties.html
    if (!highlightColor.alpha())
        return;

    m_linkHighlight = LinkHighlight::create(touchNode, this);
}

void WebViewImpl::animateDoubleTapZoom(const IntPoint& point)
{
    if (!mainFrameImpl())
        return;

    WebRect rect(point.x(), point.y(), touchPointPadding, touchPointPadding);
    WebRect blockBounds = computeBlockBounds(rect, false);

    float scale;
    WebPoint scroll;

    computeScaleAndScrollForBlockRect(blockBounds, touchPointPadding, minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio, scale, scroll);

    bool stillAtPreviousDoubleTapScale = (pageScaleFactor() == m_doubleTapZoomPageScaleFactor
        && m_doubleTapZoomPageScaleFactor != minimumPageScaleFactor())
        || m_doubleTapZoomPending;

    bool scaleUnchanged = fabs(pageScaleFactor() - scale) < minScaleDifference;
    bool shouldZoomOut = blockBounds.isEmpty() || scaleUnchanged || stillAtPreviousDoubleTapScale;

    bool isAnimating;

    if (shouldZoomOut) {
        scale = minimumPageScaleFactor();
        isAnimating = startPageScaleAnimation(mainFrameImpl()->frameView()->windowToContents(point), true, scale, doubleTapZoomAnimationDurationInSeconds);
    } else {
        isAnimating = startPageScaleAnimation(scroll, false, scale, doubleTapZoomAnimationDurationInSeconds);
    }

    if (isAnimating) {
        m_doubleTapZoomPageScaleFactor = scale;
        m_doubleTapZoomPending = true;
    }
}

void WebViewImpl::zoomToFindInPageRect(const WebRect& rect)
{
    if (!mainFrameImpl())
        return;

    WebRect blockBounds = computeBlockBounds(rect, true);

    if (blockBounds.isEmpty()) {
        // Keep current scale (no need to scroll as x,y will normally already
        // be visible). FIXME: Revisit this if it isn't always true.
        return;
    }

    float scale;
    WebPoint scroll;

    computeScaleAndScrollForBlockRect(blockBounds, nonUserInitiatedPointPadding, minimumPageScaleFactor(), scale, scroll);

    startPageScaleAnimation(scroll, false, scale, findInPageAnimationDurationInSeconds);
}

bool WebViewImpl::zoomToMultipleTargetsRect(const WebRect& rect)
{
    if (!mainFrameImpl())
        return false;

    float scale;
    WebPoint scroll;

    computeScaleAndScrollForBlockRect(rect, nonUserInitiatedPointPadding, minimumPageScaleFactor(), scale, scroll);

    if (scale <= pageScaleFactor())
        return false;

    startPageScaleAnimation(scroll, false, scale, multipleTargetsZoomAnimationDurationInSeconds);
    return true;
}

void WebViewImpl::numberOfWheelEventHandlersChanged(unsigned numberOfWheelHandlers)
{
    if (m_client)
        m_client->numberOfWheelEventHandlersChanged(numberOfWheelHandlers);
}

void WebViewImpl::hasTouchEventHandlers(bool hasTouchHandlers)
{
    if (m_client)
        m_client->hasTouchEventHandlers(hasTouchHandlers);
}

bool WebViewImpl::hasTouchEventHandlersAt(const WebPoint& point)
{
    // FIXME: Implement this. Note that the point must be divided by pageScaleFactor.
    return true;
}

#if !OS(MACOSX)
// Mac has no way to open a context menu based on a keyboard event.
bool WebViewImpl::sendContextMenuEvent(const WebKeyboardEvent& event)
{
    // The contextMenuController() holds onto the last context menu that was
    // popped up on the page until a new one is created. We need to clear
    // this menu before propagating the event through the DOM so that we can
    // detect if we create a new menu for this event, since we won't create
    // a new menu if the DOM swallows the event and the defaultEventHandler does
    // not run.
    page()->contextMenuController().clearContextMenu();

    m_contextMenuAllowed = true;
    Frame* focusedFrame = page()->focusController().focusedOrMainFrame();
    bool handled = focusedFrame->eventHandler()->sendContextMenuEventForKey();
    m_contextMenuAllowed = false;
    return handled;
}
#endif

bool WebViewImpl::keyEventDefault(const WebKeyboardEvent& event)
{
    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;

    switch (event.type) {
    case WebInputEvent::Char:
        if (event.windowsKeyCode == VKEY_SPACE) {
            int keyCode = ((event.modifiers & WebInputEvent::ShiftKey) ? VKEY_PRIOR : VKEY_NEXT);
            return scrollViewWithKeyboard(keyCode, event.modifiers);
        }
        break;
    case WebInputEvent::RawKeyDown:
        if (event.modifiers == WebInputEvent::ControlKey) {
            switch (event.windowsKeyCode) {
#if !OS(MACOSX)
            case 'A':
                focusedFrame()->executeCommand(WebString::fromUTF8("SelectAll"));
                return true;
            case VKEY_INSERT:
            case 'C':
                focusedFrame()->executeCommand(WebString::fromUTF8("Copy"));
                return true;
#endif
            // Match FF behavior in the sense that Ctrl+home/end are the only Ctrl
            // key combinations which affect scrolling. Safari is buggy in the
            // sense that it scrolls the page for all Ctrl+scrolling key
            // combinations. For e.g. Ctrl+pgup/pgdn/up/down, etc.
            case VKEY_HOME:
            case VKEY_END:
                break;
            default:
                return false;
            }
        }
        if (!event.isSystemKey && !(event.modifiers & WebInputEvent::ShiftKey))
            return scrollViewWithKeyboard(event.windowsKeyCode, event.modifiers);
        break;
    default:
        break;
    }
    return false;
}

bool WebViewImpl::scrollViewWithKeyboard(int keyCode, int modifiers)
{
    ScrollDirection scrollDirection;
    ScrollGranularity scrollGranularity;
#if OS(MACOSX)
    // Control-Up/Down should be PageUp/Down on Mac.
    if (modifiers & WebMouseEvent::ControlKey) {
      if (keyCode == VKEY_UP)
        keyCode = VKEY_PRIOR;
      else if (keyCode == VKEY_DOWN)
        keyCode = VKEY_NEXT;
    }
#endif
    if (!mapKeyCodeForScroll(keyCode, &scrollDirection, &scrollGranularity))
        return false;
    return propagateScroll(scrollDirection, scrollGranularity);
}

bool WebViewImpl::mapKeyCodeForScroll(int keyCode,
                                      WebCore::ScrollDirection* scrollDirection,
                                      WebCore::ScrollGranularity* scrollGranularity)
{
    switch (keyCode) {
    case VKEY_LEFT:
        *scrollDirection = ScrollLeft;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_RIGHT:
        *scrollDirection = ScrollRight;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_UP:
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_DOWN:
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_HOME:
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByDocument;
        break;
    case VKEY_END:
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByDocument;
        break;
    case VKEY_PRIOR:  // page up
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByPage;
        break;
    case VKEY_NEXT:  // page down
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByPage;
        break;
    default:
        return false;
    }

    return true;
}

void WebViewImpl::hideSelectPopup()
{
    if (m_selectPopup)
        m_selectPopup->hidePopup();
}

bool WebViewImpl::propagateScroll(ScrollDirection scrollDirection,
                                  ScrollGranularity scrollGranularity)
{
    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;

    bool scrollHandled = frame->eventHandler()->scrollOverflow(scrollDirection, scrollGranularity);
    Frame* currentFrame = frame;
    while (!scrollHandled && currentFrame) {
        scrollHandled = currentFrame->view()->scroll(scrollDirection, scrollGranularity);
        currentFrame = currentFrame->tree()->parent();
    }
    return scrollHandled;
}

void  WebViewImpl::popupOpened(WebCore::PopupContainer* popupContainer)
{
    if (popupContainer->popupType() == WebCore::PopupContainer::Select) {
        ASSERT(!m_selectPopup);
        m_selectPopup = popupContainer;
    }
}

void  WebViewImpl::popupClosed(WebCore::PopupContainer* popupContainer)
{
    if (popupContainer->popupType() == WebCore::PopupContainer::Select) {
        ASSERT(m_selectPopup);
        m_selectPopup = 0;
    }
}

PagePopup* WebViewImpl::openPagePopup(PagePopupClient* client, const IntRect& originBoundsInRootView)
{
    ASSERT(client);
    if (hasOpenedPopup())
        hidePopups();
    ASSERT(!m_pagePopup);

    WebWidget* popupWidget = m_client->createPopupMenu(WebPopupTypePage);
    ASSERT(popupWidget);
    m_pagePopup = static_cast<WebPagePopupImpl*>(popupWidget);
    if (!m_pagePopup->initialize(this, client, originBoundsInRootView)) {
        m_pagePopup->closePopup();
        m_pagePopup = 0;
    }
    return m_pagePopup.get();
}

void WebViewImpl::closePagePopup(PagePopup* popup)
{
    ASSERT(popup);
    WebPagePopupImpl* popupImpl = static_cast<WebPagePopupImpl*>(popup);
    ASSERT(m_pagePopup.get() == popupImpl);
    if (m_pagePopup.get() != popupImpl)
        return;
    m_pagePopup->closePopup();
    m_pagePopup = 0;
}

void WebViewImpl::hideAutofillPopup()
{
    if (m_autofillPopupShowing) {
        m_autofillPopup->hidePopup();
        m_autofillPopupShowing = false;
    }
}

WebHelperPluginImpl* WebViewImpl::createHelperPlugin(const String& pluginType, const WebDocument& hostDocument)
{
    WebWidget* popupWidget = m_client->createPopupMenu(WebPopupTypeHelperPlugin);
    ASSERT(popupWidget);
    WebHelperPluginImpl* helperPlugin = static_cast<WebHelperPluginImpl*>(popupWidget);

    if (!helperPlugin->initialize(pluginType, hostDocument, this)) {
        helperPlugin->closeHelperPlugin();
        helperPlugin = 0;
    }
    return helperPlugin;
}

void WebViewImpl::closeHelperPluginSoon(PassRefPtr<WebHelperPluginImpl> helperPlugin)
{
    m_helperPluginsPendingClose.append(helperPlugin);
    if (!m_helperPluginCloseTimer.isActive())
        m_helperPluginCloseTimer.startOneShot(0);
}

void WebViewImpl::closePendingHelperPlugins(Timer<WebViewImpl>* timer)
{
    ASSERT_UNUSED(timer, !timer || timer == &m_helperPluginCloseTimer);
    ASSERT(!m_helperPluginsPendingClose.isEmpty());

    Vector<RefPtr<WebHelperPluginImpl> > helperPlugins;
    helperPlugins.swap(m_helperPluginsPendingClose);
    for (Vector<RefPtr<WebHelperPluginImpl> >::iterator it = helperPlugins.begin();
        it != helperPlugins.end(); ++it) {
        (*it)->closeHelperPlugin();
    }
    ASSERT(m_helperPluginsPendingClose.isEmpty());
}

Frame* WebViewImpl::focusedWebCoreFrame() const
{
    return m_page ? m_page->focusController().focusedOrMainFrame() : 0;
}

WebViewImpl* WebViewImpl::fromPage(Page* page)
{
    if (!page)
        return 0;
    return static_cast<WebViewImpl*>(page->chrome().client().webView());
}

// WebWidget ------------------------------------------------------------------

void WebViewImpl::close()
{
    if (m_page) {
        // Initiate shutdown for the entire frameset.  This will cause a lot of
        // notifications to be sent.
        if (m_page->mainFrame())
            m_page->mainFrame()->loader()->frameDetached();

        m_page.clear();
    }

    // Should happen after m_page.clear().
    if (m_devToolsAgent)
        m_devToolsAgent.clear();

    // Helper Plugins must be closed now since doing so accesses RenderViewImpl,
    // which will be destroyed after this function returns.
    if (m_helperPluginCloseTimer.isActive()) {
        m_helperPluginCloseTimer.stop();
        closePendingHelperPlugins(0);
    }

    // Reset the delegate to prevent notifications being sent as we're being
    // deleted.
    m_client = 0;

    deref();  // Balances ref() acquired in WebView::create
}

void WebViewImpl::willStartLiveResize()
{
    if (mainFrameImpl() && mainFrameImpl()->frameView())
        mainFrameImpl()->frameView()->willStartLiveResize();

    Frame* frame = mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        pluginContainer->willStartLiveResize();
}

WebSize WebViewImpl::size()
{
    return m_size;
}

void WebViewImpl::resize(const WebSize& newSize)
{
    if (m_shouldAutoResize || m_size == newSize)
        return;

    FrameView* view = mainFrameImpl()->frameView();
    if (!view)
        return;

    WebSize oldSize = m_size;
    float oldPageScaleFactor = pageScaleFactor();
    int oldContentsWidth = contentsSize().width();

    m_size = newSize;

    bool shouldAnchorAndRescaleViewport = settings()->viewportEnabled() && oldSize.width && oldContentsWidth && newSize.width != oldSize.width;
    ViewportAnchor viewportAnchor(mainFrameImpl()->frame()->eventHandler());
    if (shouldAnchorAndRescaleViewport) {
        viewportAnchor.setAnchor(view->visibleContentRect(),
                                 FloatSize(viewportAnchorXCoord, viewportAnchorYCoord));
    }

    // Set the fixed layout size from the viewport constraints before resizing.
    updatePageDefinedPageScaleConstraints(mainFrameImpl()->frame()->document()->viewportArguments());

    WebDevToolsAgentPrivate* agentPrivate = devToolsAgentPrivate();
    if (agentPrivate)
        agentPrivate->webViewResized(newSize);
    if (!agentPrivate || !agentPrivate->metricsOverridden()) {
        WebFrameImpl* webFrame = mainFrameImpl();
        if (webFrame->frameView()) {
            webFrame->frameView()->resize(m_size);
            if (m_pinchViewports)
                m_pinchViewports->setViewportSize(m_size);
        }
    }

    if (settings()->viewportEnabled()) {
        // Relayout immediately to recalculate the minimum scale limit.
        if (view->needsLayout())
            view->layout();

        if (shouldAnchorAndRescaleViewport) {
            float viewportWidthRatio = static_cast<float>(newSize.width) / oldSize.width;
            float contentsWidthRatio = static_cast<float>(contentsSize().width()) / oldContentsWidth;
            float scaleMultiplier = viewportWidthRatio / contentsWidthRatio;

            IntSize viewportSize = view->visibleContentRect().size();
            if (scaleMultiplier != 1) {
                float newPageScaleFactor = oldPageScaleFactor * scaleMultiplier;
                viewportSize.scale(pageScaleFactor() / newPageScaleFactor);
                IntPoint scrollOffsetAtNewScale = viewportAnchor.computeOrigin(viewportSize);
                setPageScaleFactor(newPageScaleFactor, scrollOffsetAtNewScale);
            } else {
                IntPoint scrollOffsetAtNewScale = clampOffsetAtScale(viewportAnchor.computeOrigin(viewportSize), pageScaleFactor());
                updateMainFrameScrollPosition(scrollOffsetAtNewScale, false);
            }
        }
    }

    sendResizeEventAndRepaint();
}

void WebViewImpl::willEndLiveResize()
{
    if (mainFrameImpl() && mainFrameImpl()->frameView())
        mainFrameImpl()->frameView()->willEndLiveResize();

    Frame* frame = mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        pluginContainer->willEndLiveResize();
}

void WebViewImpl::willEnterFullScreen()
{
    m_fullscreenController->willEnterFullScreen();
}

void WebViewImpl::didEnterFullScreen()
{
    m_fullscreenController->didEnterFullScreen();
}

void WebViewImpl::willExitFullScreen()
{
    m_fullscreenController->willExitFullScreen();
}

void WebViewImpl::didExitFullScreen()
{
    m_fullscreenController->didExitFullScreen();
}

void WebViewImpl::animate(double monotonicFrameBeginTime)
{
    TRACE_EVENT0("webkit", "WebViewImpl::animate");

    if (!monotonicFrameBeginTime)
        monotonicFrameBeginTime = monotonicallyIncreasingTime();

    // Create synthetic wheel events as necessary for fling.
    if (m_gestureAnimation) {
        if (m_gestureAnimation->animate(monotonicFrameBeginTime))
            scheduleAnimation();
        else {
            m_gestureAnimation.clear();
            if (m_layerTreeView)
                m_layerTreeView->didStopFlinging();

            PlatformGestureEvent endScrollEvent(PlatformEvent::GestureScrollEnd,
                m_positionOnFlingStart, m_globalPositionOnFlingStart, 0, 0, 0,
                false, false, false, false);

            mainFrameImpl()->frame()->eventHandler()->handleGestureScrollEnd(endScrollEvent);
        }
    }

    if (!m_page)
        return;

    PageWidgetDelegate::animate(m_page.get(), monotonicFrameBeginTime);

    if (m_continuousPaintingEnabled) {
        ContinuousPainter::setNeedsDisplayRecursive(m_rootGraphicsLayer, m_pageOverlays.get());
        m_client->scheduleAnimation();
    }
}

void WebViewImpl::layout()
{
    TRACE_EVENT0("webkit", "WebViewImpl::layout");
    PageWidgetDelegate::layout(m_page.get());
    if (m_layerTreeView)
        m_layerTreeView->setBackgroundColor(backgroundColor());

    if (m_linkHighlight)
        m_linkHighlight->updateGeometry();
}

void WebViewImpl::enterForceCompositingMode(bool enter)
{
    if (page()->settings().forceCompositingMode() == enter)
        return;

    TRACE_EVENT1("webkit", "WebViewImpl::enterForceCompositingMode", "enter", enter);
    settingsImpl()->setForceCompositingMode(enter);
    if (enter) {
        if (!m_page)
            return;
        Frame* mainFrame = m_page->mainFrame();
        if (!mainFrame)
            return;
        mainFrame->view()->updateCompositingLayersAfterStyleChange();
    }
}

void WebViewImpl::doPixelReadbackToCanvas(WebCanvas* canvas, const IntRect& rect)
{
    ASSERT(m_layerTreeView);

    SkBitmap target;
    target.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height(), rect.width() * 4);
    target.allocPixels();
    m_layerTreeView->compositeAndReadback(target.getPixels(), rect);
#if (!SK_R32_SHIFT && SK_B32_SHIFT == 16)
    // The compositor readback always gives back pixels in BGRA order, but for
    // example Android's Skia uses RGBA ordering so the red and blue channels
    // need to be swapped.
    uint8_t* pixels = reinterpret_cast<uint8_t*>(target.getPixels());
    for (size_t i = 0; i < target.getSize(); i += 4)
        std::swap(pixels[i], pixels[i + 2]);
#endif
    canvas->writePixels(target, rect.x(), rect.y());
}

void WebViewImpl::paint(WebCanvas* canvas, const WebRect& rect, PaintOptions option)
{
#if !OS(ANDROID)
    // ReadbackFromCompositorIfAvailable is the only option available on non-Android.
    // Ideally, Android would always use ReadbackFromCompositorIfAvailable as well.
    ASSERT(option == ReadbackFromCompositorIfAvailable);
#endif

    if (option == ReadbackFromCompositorIfAvailable && isAcceleratedCompositingActive()) {
        // If a canvas was passed in, we use it to grab a copy of the
        // freshly-rendered pixels.
        if (canvas) {
            // Clip rect to the confines of the rootLayerTexture.
            IntRect resizeRect(rect);
            resizeRect.intersect(IntRect(IntPoint(0, 0), m_layerTreeView->deviceViewportSize()));
            doPixelReadbackToCanvas(canvas, resizeRect);
        }
    } else {
        FrameView* view = page()->mainFrame()->view();
        PaintBehavior oldPaintBehavior = view->paintBehavior();
        if (isAcceleratedCompositingActive()) {
            ASSERT(option == ForceSoftwareRenderingAndIgnoreGPUResidentContent);
            view->setPaintBehavior(oldPaintBehavior | PaintBehaviorFlattenCompositingLayers);
        }

        double paintStart = currentTime();
        PageWidgetDelegate::paint(m_page.get(), pageOverlays(), canvas, rect, isTransparent() ? PageWidgetDelegate::Translucent : PageWidgetDelegate::Opaque);
        double paintEnd = currentTime();
        double pixelsPerSec = (rect.width * rect.height) / (paintEnd - paintStart);
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.SoftwarePaintDurationMS", (paintEnd - paintStart) * 1000, 0, 120, 30);
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.SoftwarePaintMegapixPerSecond", pixelsPerSec / 1000000, 10, 210, 30);

        if (isAcceleratedCompositingActive()) {
            ASSERT(option == ForceSoftwareRenderingAndIgnoreGPUResidentContent);
            view->setPaintBehavior(oldPaintBehavior);
        }
    }
}

bool WebViewImpl::isTrackingRepaints() const
{
    if (!page())
        return false;
    FrameView* view = page()->mainFrame()->view();
    return view->isTrackingRepaints();
}

void WebViewImpl::themeChanged()
{
    if (!page())
        return;
    FrameView* view = page()->mainFrame()->view();

    WebRect damagedRect(0, 0, m_size.width, m_size.height);
    view->invalidateRect(damagedRect);
}

void WebViewImpl::setNeedsRedraw()
{
    if (m_layerTreeView && isAcceleratedCompositingActive())
        m_layerTreeView->setNeedsRedraw();
}

void WebViewImpl::enterFullScreenForElement(WebCore::Element* element)
{
    m_fullscreenController->enterFullScreenForElement(element);
}

void WebViewImpl::exitFullScreenForElement(WebCore::Element* element)
{
    m_fullscreenController->exitFullScreenForElement(element);
}

bool WebViewImpl::hasHorizontalScrollbar()
{
    return mainFrameImpl()->frameView()->horizontalScrollbar();
}

bool WebViewImpl::hasVerticalScrollbar()
{
    return mainFrameImpl()->frameView()->verticalScrollbar();
}

const WebInputEvent* WebViewImpl::m_currentInputEvent = 0;

bool WebViewImpl::handleInputEvent(const WebInputEvent& inputEvent)
{
    TRACE_EVENT0("input", "WebViewImpl::handleInputEvent");
    // If we've started a drag and drop operation, ignore input events until
    // we're done.
    if (m_doingDragAndDrop)
        return true;

    if (m_devToolsAgent && m_devToolsAgent->handleInputEvent(m_page.get(), inputEvent))
        return true;

    // Report the event to be NOT processed by WebKit, so that the browser can handle it appropriately.
    if (m_ignoreInputEvents)
        return false;

    TemporaryChange<const WebInputEvent*> currentEventChange(m_currentInputEvent, &inputEvent);

    if (isPointerLocked() && WebInputEvent::isMouseEventType(inputEvent.type)) {
      pointerLockMouseEvent(inputEvent);
      return true;
    }

    if (m_mouseCaptureNode && WebInputEvent::isMouseEventType(inputEvent.type)) {
        TRACE_EVENT1("input", "captured mouse event", "type", inputEvent.type);
        // Save m_mouseCaptureNode since mouseCaptureLost() will clear it.
        RefPtr<Node> node = m_mouseCaptureNode;

        // Not all platforms call mouseCaptureLost() directly.
        if (inputEvent.type == WebInputEvent::MouseUp)
            mouseCaptureLost();

        AtomicString eventType;
        switch (inputEvent.type) {
        case WebInputEvent::MouseMove:
            eventType = eventNames().mousemoveEvent;
            break;
        case WebInputEvent::MouseLeave:
            eventType = eventNames().mouseoutEvent;
            break;
        case WebInputEvent::MouseDown:
            eventType = eventNames().mousedownEvent;
            break;
        case WebInputEvent::MouseUp:
            eventType = eventNames().mouseupEvent;
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        node->dispatchMouseEvent(
              PlatformMouseEventBuilder(mainFrameImpl()->frameView(), *static_cast<const WebMouseEvent*>(&inputEvent)),
              eventType, static_cast<const WebMouseEvent*>(&inputEvent)->clickCount);
        return true;
    }

    return PageWidgetDelegate::handleInputEvent(m_page.get(), *this, inputEvent);
}

void WebViewImpl::setCursorVisibilityState(bool isVisible)
{
    if (m_page)
        m_page->setIsCursorVisible(isVisible);
}

void WebViewImpl::mouseCaptureLost()
{
    TRACE_EVENT_ASYNC_END0("input", "capturing mouse", this);
    m_mouseCaptureNode = 0;
}

void WebViewImpl::setFocus(bool enable)
{
    m_page->focusController().setFocused(enable);
    if (enable) {
        m_page->focusController().setActive(true);
        RefPtr<Frame> focusedFrame = m_page->focusController().focusedFrame();
        if (focusedFrame) {
            Element* element = focusedFrame->document()->focusedElement();
            if (element && focusedFrame->selection().selection().isNone()) {
                // If the selection was cleared while the WebView was not
                // focused, then the focus element shows with a focus ring but
                // no caret and does respond to keyboard inputs.
                if (element->isTextFormControl()) {
                    element->updateFocusAppearance(true);
                } else if (element->isContentEditable()) {
                    // updateFocusAppearance() selects all the text of
                    // contentseditable DIVs. So we set the selection explicitly
                    // instead. Note that this has the side effect of moving the
                    // caret back to the beginning of the text.
                    Position position(element, 0, Position::PositionIsOffsetInAnchor);
                    focusedFrame->selection().setSelection(VisibleSelection(position, SEL_DEFAULT_AFFINITY));
                }
            }
        }
        m_imeAcceptEvents = true;
    } else {
        hidePopups();

        // Clear focus on the currently focused frame if any.
        if (!m_page)
            return;

        Frame* frame = m_page->mainFrame();
        if (!frame)
            return;

        RefPtr<Frame> focusedFrame = m_page->focusController().focusedFrame();
        if (focusedFrame) {
            // Finish an ongoing composition to delete the composition node.
            if (focusedFrame->inputMethodController().hasComposition()) {
                if (m_autofillClient)
                    m_autofillClient->setIgnoreTextChanges(true);

                focusedFrame->inputMethodController().confirmComposition();

                if (m_autofillClient)
                    m_autofillClient->setIgnoreTextChanges(false);
            }
            m_imeAcceptEvents = false;
        }
    }
}

bool WebViewImpl::setComposition(
    const WebString& text,
    const WebVector<WebCompositionUnderline>& underlines,
    int selectionStart,
    int selectionEnd)
{
    Frame* focused = focusedWebCoreFrame();
    if (!focused || !m_imeAcceptEvents)
        return false;

    // The input focus has been moved to another WebWidget object.
    // We should use this |editor| object only to complete the ongoing
    // composition.
    InputMethodController& inputMethodController = focused->inputMethodController();
    if (!focused->editor().canEdit() && !inputMethodController.hasComposition())
        return false;

    // We should verify the parent node of this IME composition node are
    // editable because JavaScript may delete a parent node of the composition
    // node. In this case, WebKit crashes while deleting texts from the parent
    // node, which doesn't exist any longer.
    RefPtr<Range> range = inputMethodController.compositionRange();
    if (range) {
        Node* node = range->startContainer();
        if (!node || !node->isContentEditable())
            return false;
    }

    // If we're not going to fire a keypress event, then the keydown event was
    // canceled.  In that case, cancel any existing composition.
    if (text.isEmpty() || m_suppressNextKeypressEvent) {
        // A browser process sent an IPC message which does not contain a valid
        // string, which means an ongoing composition has been canceled.
        // If the ongoing composition has been canceled, replace the ongoing
        // composition string with an empty string and complete it.
        String emptyString;
        Vector<CompositionUnderline> emptyUnderlines;
        inputMethodController.setComposition(emptyString, emptyUnderlines, 0, 0);
        return text.isEmpty();
    }

    // When the range of composition underlines overlap with the range between
    // selectionStart and selectionEnd, WebKit somehow won't paint the selection
    // at all (see InlineTextBox::paint() function in InlineTextBox.cpp).
    // But the selection range actually takes effect.
    inputMethodController.setComposition(String(text),
                           CompositionUnderlineVectorBuilder(underlines),
                           selectionStart, selectionEnd);

    return inputMethodController.hasComposition();
}

bool WebViewImpl::confirmComposition()
{
    return confirmComposition(DoNotKeepSelection);
}

bool WebViewImpl::confirmComposition(ConfirmCompositionBehavior selectionBehavior)
{
    return confirmComposition(WebString(), selectionBehavior);
}

bool WebViewImpl::confirmComposition(const WebString& text)
{
    return confirmComposition(text, DoNotKeepSelection);
}

bool WebViewImpl::confirmComposition(const WebString& text, ConfirmCompositionBehavior selectionBehavior)
{
    Frame* focused = focusedWebCoreFrame();
    if (!focused || !m_imeAcceptEvents)
        return false;
    Editor& editor = focused->editor();
    InputMethodController& inputMethodController = focused->inputMethodController();
    if (!inputMethodController.hasComposition() && !text.length())
        return false;

    // We should verify the parent node of this IME composition node are
    // editable because JavaScript may delete a parent node of the composition
    // node. In this case, WebKit crashes while deleting texts from the parent
    // node, which doesn't exist any longer.
    RefPtr<Range> range = inputMethodController.compositionRange();
    if (range) {
        Node* node = range->startContainer();
        if (!node || !node->isContentEditable())
            return false;
    }

    if (inputMethodController.hasComposition()) {
        if (text.length()) {
            inputMethodController.confirmComposition(String(text));
        } else {
            size_t location;
            size_t length;
            caretOrSelectionRange(&location, &length);

            inputMethodController.confirmComposition();
            if (selectionBehavior == KeepSelection)
                editor.setSelectionOffsets(location, location + length);
        }
    } else {
        editor.insertText(String(text), 0);
    }

    return true;
}

bool WebViewImpl::compositionRange(size_t* location, size_t* length)
{
    Frame* focused = focusedWebCoreFrame();
    if (!focused || !m_imeAcceptEvents)
        return false;

    RefPtr<Range> range = focused->inputMethodController().compositionRange();
    if (!range)
        return false;

    if (TextIterator::getLocationAndLengthFromRange(focused->selection().rootEditableElementOrDocumentElement(), range.get(), *location, *length))
        return true;
    return false;
}

WebTextInputInfo WebViewImpl::textInputInfo()
{
    WebTextInputInfo info;

    Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return info;

    FrameSelection& selection = focused->selection();
    Node* node = selection.selection().rootEditableElement();
    if (!node)
        return info;

    info.inputMode = inputModeOfFocusedElement();

    info.type = textInputType();
    if (info.type == WebTextInputTypeNone)
        return info;

    if (!focused->editor().canEdit())
        return info;

    info.value = plainText(rangeOfContents(node).get());

    if (info.value.isEmpty())
        return info;

    size_t location;
    size_t length;
    RefPtr<Range> range = selection.selection().firstRange();
    if (range && TextIterator::getLocationAndLengthFromRange(selection.rootEditableElement(), range.get(), location, length)) {
        info.selectionStart = location;
        info.selectionEnd = location + length;
    }
    range = focused->inputMethodController().compositionRange();
    if (range && TextIterator::getLocationAndLengthFromRange(selection.rootEditableElement(), range.get(), location, length)) {
        info.compositionStart = location;
        info.compositionEnd = location + length;
    }

    return info;
}

WebTextInputType WebViewImpl::textInputType()
{
    Element* element = focusedElement();
    if (!element)
        return WebTextInputTypeNone;

    if (element->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement* input = toHTMLInputElement(element);

        if (input->isDisabledOrReadOnly())
            return WebTextInputTypeNone;

        if (input->isPasswordField())
            return WebTextInputTypePassword;
        if (input->isSearchField())
            return WebTextInputTypeSearch;
        if (input->isEmailField())
            return WebTextInputTypeEmail;
        if (input->isNumberField())
            return WebTextInputTypeNumber;
        if (input->isTelephoneField())
            return WebTextInputTypeTelephone;
        if (input->isURLField())
            return WebTextInputTypeURL;
        if (input->isDateField())
            return WebTextInputTypeDate;
        if (input->isDateTimeLocalField())
            return WebTextInputTypeDateTimeLocal;
        if (input->isMonthField())
            return WebTextInputTypeMonth;
        if (input->isTimeField())
            return WebTextInputTypeTime;
        if (input->isWeekField())
            return WebTextInputTypeWeek;
        if (input->isTextField())
            return WebTextInputTypeText;

        return WebTextInputTypeNone;
    }

    if (isHTMLTextAreaElement(element)) {
        if (toHTMLTextAreaElement(element)->isDisabledOrReadOnly())
            return WebTextInputTypeNone;
        return WebTextInputTypeTextArea;
    }

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    if (element->isHTMLElement()) {
        if (toHTMLElement(element)->isDateTimeFieldElement())
            return WebTextInputTypeDateTimeField;
    }
#endif

    if (element->shouldUseInputMethod())
        return WebTextInputTypeContentEditable;

    return WebTextInputTypeNone;
}

WebString WebViewImpl::inputModeOfFocusedElement()
{
    if (!RuntimeEnabledFeatures::inputModeAttributeEnabled())
        return WebString();

    Element* element = focusedElement();
    if (!element)
        return WebString();

    if (element->hasTagName(HTMLNames::inputTag)) {
        const HTMLInputElement* input = toHTMLInputElement(element);
        if (input->supportsInputModeAttribute())
            return input->fastGetAttribute(HTMLNames::inputmodeAttr).lower();
        return WebString();
    }
    if (isHTMLTextAreaElement(element)) {
        const HTMLTextAreaElement* textarea = toHTMLTextAreaElement(element);
        return textarea->fastGetAttribute(HTMLNames::inputmodeAttr).lower();
    }

    return WebString();
}

bool WebViewImpl::selectionBounds(WebRect& anchor, WebRect& focus) const
{
    const Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;
    FrameSelection& selection = frame->selection();

    if (selection.isCaret()) {
        anchor = focus = selection.absoluteCaretBounds();
    } else {
        RefPtr<Range> selectedRange = selection.toNormalizedRange();
        if (!selectedRange)
            return false;

        RefPtr<Range> range(Range::create(selectedRange->startContainer()->document(),
            selectedRange->startContainer(),
            selectedRange->startOffset(),
            selectedRange->startContainer(),
            selectedRange->startOffset()));
        anchor = frame->editor().firstRectForRange(range.get());

        range = Range::create(selectedRange->endContainer()->document(),
            selectedRange->endContainer(),
            selectedRange->endOffset(),
            selectedRange->endContainer(),
            selectedRange->endOffset());
        focus = frame->editor().firstRectForRange(range.get());
    }

    IntRect scaledAnchor(frame->view()->contentsToWindow(anchor));
    IntRect scaledFocus(frame->view()->contentsToWindow(focus));
    scaledAnchor.scale(pageScaleFactor());
    scaledFocus.scale(pageScaleFactor());
    anchor = scaledAnchor;
    focus = scaledFocus;

    if (!selection.selection().isBaseFirst())
        std::swap(anchor, focus);
    return true;
}

bool WebViewImpl::selectionTextDirection(WebTextDirection& start, WebTextDirection& end) const
{
    const Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;
    FrameSelection& selection = frame->selection();
    if (!selection.toNormalizedRange())
        return false;
    start = selection.start().primaryDirection() == RTL ? WebTextDirectionRightToLeft : WebTextDirectionLeftToRight;
    end = selection.end().primaryDirection() == RTL ? WebTextDirectionRightToLeft : WebTextDirectionLeftToRight;
    return true;
}

bool WebViewImpl::isSelectionAnchorFirst() const
{
    if (const Frame* frame = focusedWebCoreFrame())
        return frame->selection().selection().isBaseFirst();
    return false;
}

bool WebViewImpl::setEditableSelectionOffsets(int start, int end)
{
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return false;

    Editor& editor = focused->editor();
    return editor.canEdit() && editor.setSelectionOffsets(start, end);
}

bool WebViewImpl::setCompositionFromExistingText(int compositionStart, int compositionEnd, const WebVector<WebCompositionUnderline>& underlines)
{
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return false;

    if (!focused->editor().canEdit())
        return false;

    InputMethodController& inputMethodController = focused->inputMethodController();
    inputMethodController.cancelComposition();

    if (compositionStart == compositionEnd)
        return true;

    inputMethodController.setCompositionFromExistingText(CompositionUnderlineVectorBuilder(underlines), compositionStart, compositionEnd);

    return true;
}

WebVector<WebCompositionUnderline> WebViewImpl::compositionUnderlines() const
{
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return WebVector<WebCompositionUnderline>();
    const Vector<CompositionUnderline>& underlines = focused->inputMethodController().customCompositionUnderlines();
    WebVector<WebCompositionUnderline> results(underlines.size());
    for (size_t index = 0; index < underlines.size(); ++index) {
        CompositionUnderline underline = underlines[index];
        results[index] = WebCompositionUnderline(underline.startOffset, underline.endOffset, static_cast<WebColor>(underline.color.rgb()), underline.thick);
    }
    return results;
}

void WebViewImpl::extendSelectionAndDelete(int before, int after)
{
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return;

    Editor& editor = focused->editor();
    if (!editor.canEdit())
        return;

    FrameSelection& selection = focused->selection();
    size_t location;
    size_t length;
    RefPtr<Range> range = selection.selection().firstRange();
    if (range && TextIterator::getLocationAndLengthFromRange(selection.rootEditableElement(), range.get(), location, length)) {
        editor.setSelectionOffsets(max(static_cast<int>(location) - before, 0), location + length + after);
        focused->document()->execCommand("delete", true);
    }
}

bool WebViewImpl::isSelectionEditable() const
{
    if (const Frame* frame = focusedWebCoreFrame())
        return frame->selection().isContentEditable();
    return false;
}

WebColor WebViewImpl::backgroundColor() const
{
    if (isTransparent())
        return Color::transparent;
    if (!m_page)
        return m_baseBackgroundColor;
    FrameView* view = m_page->mainFrame()->view();
    Color backgroundColor = view->documentBackgroundColor();
    if (!backgroundColor.isValid())
        return m_baseBackgroundColor;
    return backgroundColor.rgb();
}

bool WebViewImpl::caretOrSelectionRange(size_t* location, size_t* length)
{
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return false;

    FrameSelection& selection = focused->selection();
    RefPtr<Range> range = selection.selection().firstRange();
    if (!range)
        return false;

    return TextIterator::getLocationAndLengthFromRange(selection.rootEditableElementOrTreeScopeRootNode(), range.get(), *location, *length);
}

void WebViewImpl::setTextDirection(WebTextDirection direction)
{
    // The Editor::setBaseWritingDirection() function checks if we can change
    // the text direction of the selected node and updates its DOM "dir"
    // attribute and its CSS "direction" property.
    // So, we just call the function as Safari does.
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return;

    Editor& editor = focused->editor();
    if (!editor.canEdit())
        return;

    switch (direction) {
    case WebTextDirectionDefault:
        editor.setBaseWritingDirection(NaturalWritingDirection);
        break;

    case WebTextDirectionLeftToRight:
        editor.setBaseWritingDirection(LeftToRightWritingDirection);
        break;

    case WebTextDirectionRightToLeft:
        editor.setBaseWritingDirection(RightToLeftWritingDirection);
        break;

    default:
        notImplemented();
        break;
    }
}

bool WebViewImpl::isAcceleratedCompositingActive() const
{
    return m_isAcceleratedCompositingActive;
}

void WebViewImpl::willCloseLayerTreeView()
{
    setIsAcceleratedCompositingActive(false);
    m_layerTreeView = 0;
}

void WebViewImpl::didAcquirePointerLock()
{
    if (page())
        page()->pointerLockController().didAcquirePointerLock();
}

void WebViewImpl::didNotAcquirePointerLock()
{
    if (page())
        page()->pointerLockController().didNotAcquirePointerLock();
}

void WebViewImpl::didLosePointerLock()
{
    if (page())
        page()->pointerLockController().didLosePointerLock();
}

void WebViewImpl::didChangeWindowResizerRect()
{
    if (mainFrameImpl()->frameView())
        mainFrameImpl()->frameView()->windowResizerRectChanged();
}

// WebView --------------------------------------------------------------------

WebSettingsImpl* WebViewImpl::settingsImpl()
{
    if (!m_webSettings)
        m_webSettings = adoptPtr(new WebSettingsImpl(&m_page->settings()));
    ASSERT(m_webSettings);
    return m_webSettings.get();
}

WebSettings* WebViewImpl::settings()
{
    return settingsImpl();
}

WebString WebViewImpl::pageEncoding() const
{
    if (!m_page)
        return WebString();

    // FIXME: Is this check needed?
    if (!m_page->mainFrame()->document()->loader())
        return WebString();

    return m_page->mainFrame()->document()->encodingName();
}

void WebViewImpl::setPageEncoding(const WebString& encodingName)
{
    if (!m_page)
        return;

    // Only change override encoding, don't change default encoding.
    // Note that the new encoding must be 0 if it isn't supposed to be set.
    String newEncodingName;
    if (!encodingName.isEmpty())
        newEncodingName = encodingName;
    m_page->mainFrame()->loader()->reload(NormalReload, KURL(), newEncodingName);
}

bool WebViewImpl::dispatchBeforeUnloadEvent()
{
    // FIXME: This should really cause a recursive depth-first walk of all
    // frames in the tree, calling each frame's onbeforeunload.  At the moment,
    // we're consistent with Safari 3.1, not IE/FF.
    Frame* frame = m_page->mainFrame();
    if (!frame)
        return true;

    return frame->loader()->shouldClose();
}

void WebViewImpl::dispatchUnloadEvent()
{
    // Run unload handlers.
    m_page->mainFrame()->loader()->closeURL();
}

WebFrame* WebViewImpl::mainFrame()
{
    return mainFrameImpl();
}

WebFrame* WebViewImpl::findFrameByName(
    const WebString& name, WebFrame* relativeToFrame)
{
    if (!relativeToFrame)
        relativeToFrame = mainFrame();
    Frame* frame = toWebFrameImpl(relativeToFrame)->frame();
    frame = frame->tree()->find(name);
    return WebFrameImpl::fromFrame(frame);
}

WebFrame* WebViewImpl::focusedFrame()
{
    return WebFrameImpl::fromFrame(focusedWebCoreFrame());
}

void WebViewImpl::setFocusedFrame(WebFrame* frame)
{
    if (!frame) {
        // Clears the focused frame if any.
        if (Frame* focusedFrame = focusedWebCoreFrame())
            focusedFrame->selection().setFocused(false);
        return;
    }
    Frame* webcoreFrame = toWebFrameImpl(frame)->frame();
    webcoreFrame->page()->focusController().setFocusedFrame(webcoreFrame);
}

void WebViewImpl::setInitialFocus(bool reverse)
{
    if (!m_page)
        return;
    Frame* frame = page()->focusController().focusedOrMainFrame();
    if (Document* document = frame->document())
        document->setFocusedElement(0);
    page()->focusController().setInitialFocus(reverse ? FocusDirectionBackward : FocusDirectionForward);
}

void WebViewImpl::clearFocusedNode()
{
    RefPtr<Frame> frame = focusedWebCoreFrame();
    if (!frame)
        return;

    RefPtr<Document> document = frame->document();
    if (!document)
        return;

    RefPtr<Element> oldFocusedElement = document->focusedElement();

    // Clear the focused node.
    document->setFocusedElement(0);

    if (!oldFocusedElement)
        return;

    // If a text field has focus, we need to make sure the selection controller
    // knows to remove selection from it. Otherwise, the text field is still
    // processing keyboard events even though focus has been moved to the page and
    // keystrokes get eaten as a result.
    if (oldFocusedElement->isContentEditable() || oldFocusedElement->isTextFormControl())
        frame->selection().clear();
}

void WebViewImpl::scrollFocusedNodeIntoView()
{
    if (Element* element = focusedElement())
        element->scrollIntoViewIfNeeded(true);
}

void WebViewImpl::scrollFocusedNodeIntoRect(const WebRect& rect)
{
    Frame* frame = page()->mainFrame();
    Element* element = focusedElement();
    if (!frame || !frame->view() || !element)
        return;

    if (!m_webSettings->autoZoomFocusedNodeToLegibleScale()) {
        frame->view()->scrollElementToRect(element, IntRect(rect.x, rect.y, rect.width, rect.height));
        return;
    }

    float scale;
    IntPoint scroll;
    bool needAnimation;
    computeScaleAndScrollForFocusedNode(element, scale, scroll, needAnimation);
    if (needAnimation)
        startPageScaleAnimation(scroll, false, scale, scrollAndScaleAnimationDurationInSeconds);
}

void WebViewImpl::computeScaleAndScrollForFocusedNode(Node* focusedNode, float& newScale, IntPoint& newScroll, bool& needAnimation)
{
    focusedNode->document().updateLayoutIgnorePendingStylesheets();

    // 'caret' is rect encompassing the blinking cursor.
    IntRect textboxRect = focusedNode->document().view()->contentsToWindow(pixelSnappedIntRect(focusedNode->Node::boundingBox()));
    WebRect caret, unusedEnd;
    selectionBounds(caret, unusedEnd);
    IntRect unscaledCaret = caret;
    unscaledCaret.scale(1 / pageScaleFactor());
    caret = unscaledCaret;

    // Pick a scale which is reasonably readable. This is the scale at which
    // the caret height will become minReadableCaretHeight (adjusted for dpi
    // and font scale factor).
    float targetScale = 1;
    if (page())
        targetScale *= page()->settings().textAutosizingFontScaleFactor();

    newScale = clampPageScaleFactorToLimits(minReadableCaretHeight * targetScale / caret.height);
    const float deltaScale = newScale / pageScaleFactor();

    // Convert the rects to absolute space in the new scale.
    IntRect textboxRectInDocumentCoordinates = textboxRect;
    textboxRectInDocumentCoordinates.move(mainFrame()->scrollOffset());
    IntRect caretInDocumentCoordinates = caret;
    caretInDocumentCoordinates.move(mainFrame()->scrollOffset());

    int viewWidth = m_size.width / newScale;
    int viewHeight = m_size.height / newScale;

    if (textboxRectInDocumentCoordinates.width() <= viewWidth) {
        // Field is narrower than screen. Try to leave padding on left so field's
        // label is visible, but it's more important to ensure entire field is
        // onscreen.
        int idealLeftPadding = viewWidth * leftBoxRatio;
        int maxLeftPaddingKeepingBoxOnscreen = viewWidth - textboxRectInDocumentCoordinates.width();
        newScroll.setX(textboxRectInDocumentCoordinates.x() - min<int>(idealLeftPadding, maxLeftPaddingKeepingBoxOnscreen));
    } else {
        // Field is wider than screen. Try to left-align field, unless caret would
        // be offscreen, in which case right-align the caret.
        newScroll.setX(max<int>(textboxRectInDocumentCoordinates.x(), caretInDocumentCoordinates.x() + caretInDocumentCoordinates.width() + caretPadding - viewWidth));
    }
    if (textboxRectInDocumentCoordinates.height() <= viewHeight) {
        // Field is shorter than screen. Vertically center it.
        newScroll.setY(textboxRectInDocumentCoordinates.y() - (viewHeight - textboxRectInDocumentCoordinates.height()) / 2);
    } else {
        // Field is taller than screen. Try to top align field, unless caret would
        // be offscreen, in which case bottom-align the caret.
        newScroll.setY(max<int>(textboxRectInDocumentCoordinates.y(), caretInDocumentCoordinates.y() + caretInDocumentCoordinates.height() + caretPadding - viewHeight));
    }

    needAnimation = false;
    // If we are at less than the target zoom level, zoom in.
    if (deltaScale > minScaleChangeToTriggerZoom)
        needAnimation = true;
    // If the caret is offscreen, then animate.
    IntRect sizeRect(0, 0, viewWidth, viewHeight);
    if (!sizeRect.contains(caret))
        needAnimation = true;
    // If the box is partially offscreen and it's possible to bring it fully
    // onscreen, then animate.
    if (sizeRect.contains(textboxRectInDocumentCoordinates.width(), textboxRectInDocumentCoordinates.height()) && !sizeRect.contains(textboxRect))
        needAnimation = true;
}

void WebViewImpl::advanceFocus(bool reverse)
{
    page()->focusController().advanceFocus(reverse ? FocusDirectionBackward : FocusDirectionForward);
}

double WebViewImpl::zoomLevel()
{
    return m_zoomLevel;
}

double WebViewImpl::setZoomLevel(double zoomLevel)
{
    if (zoomLevel < m_minimumZoomLevel)
        m_zoomLevel = m_minimumZoomLevel;
    else if (zoomLevel > m_maximumZoomLevel)
        m_zoomLevel = m_maximumZoomLevel;
    else
        m_zoomLevel = zoomLevel;

    Frame* frame = mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        pluginContainer->plugin()->setZoomLevel(m_zoomLevel, false);
    else {
        float zoomFactor = static_cast<float>(zoomLevelToZoomFactor(m_zoomLevel));
        frame->setPageZoomFactor(zoomFactor);
    }

    return m_zoomLevel;
}

double WebViewImpl::setZoomLevel(bool textOnly, double zoomLevel)
{
    if (textOnly) {
        setZoomLevel(0.0f);
        return setTextZoomFactor(static_cast<float>(zoomLevelToZoomFactor(zoomLevel)));
    }
    setTextZoomFactor(1.0f);
    return setZoomLevel(zoomLevel);
}

void WebViewImpl::zoomLimitsChanged(double minimumZoomLevel,
                                    double maximumZoomLevel)
{
    m_minimumZoomLevel = minimumZoomLevel;
    m_maximumZoomLevel = maximumZoomLevel;
    m_client->zoomLimitsChanged(m_minimumZoomLevel, m_maximumZoomLevel);
}

float WebViewImpl::textZoomFactor()
{
    return mainFrameImpl()->frame()->textZoomFactor();
}

float WebViewImpl::setTextZoomFactor(float textZoomFactor)
{
    Frame* frame = mainFrameImpl()->frame();
    if (WebFrameImpl::pluginContainerFromFrame(frame))
        return 1;

    frame->setTextZoomFactor(textZoomFactor);

    return textZoomFactor;
}

void WebViewImpl::fullFramePluginZoomLevelChanged(double zoomLevel)
{
    if (zoomLevel == m_zoomLevel)
        return;

    m_zoomLevel = max(min(zoomLevel, m_maximumZoomLevel), m_minimumZoomLevel);
    m_client->zoomLevelChanged();
}

double WebView::zoomLevelToZoomFactor(double zoomLevel)
{
    return pow(textSizeMultiplierRatio, zoomLevel);
}

double WebView::zoomFactorToZoomLevel(double factor)
{
    // Since factor = 1.2^level, level = log(factor) / log(1.2)
    return log(factor) / log(textSizeMultiplierRatio);
}

float WebViewImpl::pageScaleFactor() const
{
    if (!page())
        return 1;

    return page()->pageScaleFactor();
}

float WebViewImpl::clampPageScaleFactorToLimits(float scaleFactor) const
{
    return m_pageScaleConstraintsSet.finalConstraints().clampToConstraints(scaleFactor);
}

IntPoint WebViewImpl::clampOffsetAtScale(const IntPoint& offset, float scale)
{
    FrameView* view = mainFrameImpl()->frameView();
    if (!view)
        return offset;

    IntPoint maxScrollExtent(contentsSize().width() - view->scrollOrigin().x(), contentsSize().height() - view->scrollOrigin().y());
    FloatSize scaledSize = view->unscaledVisibleContentSize();
    scaledSize.scale(1 / scale);

    IntPoint clampedOffset = offset;
    clampedOffset = clampedOffset.shrunkTo(maxScrollExtent - expandedIntSize(scaledSize));
    clampedOffset = clampedOffset.expandedTo(-view->scrollOrigin());

    return clampedOffset;
}

void WebViewImpl::setPageScaleFactor(float scaleFactor, const WebPoint& origin)
{
    if (!page())
        return;

    IntPoint newScrollOffset = origin;
    scaleFactor = clampPageScaleFactorToLimits(scaleFactor);
    newScrollOffset = clampOffsetAtScale(newScrollOffset, scaleFactor);

    page()->setPageScaleFactor(scaleFactor, newScrollOffset);
}

void WebViewImpl::setPageScaleFactorPreservingScrollOffset(float scaleFactor)
{
    if (clampPageScaleFactorToLimits(scaleFactor) == pageScaleFactor())
        return;

    IntPoint scrollOffset(mainFrame()->scrollOffset().width, mainFrame()->scrollOffset().height);
    setPageScaleFactor(scaleFactor, scrollOffset);
}

float WebViewImpl::deviceScaleFactor() const
{
    if (!page())
        return 1;

    return page()->deviceScaleFactor();
}

void WebViewImpl::setDeviceScaleFactor(float scaleFactor)
{
    if (!page())
        return;

    page()->setDeviceScaleFactor(scaleFactor);

    if (m_layerTreeView)
        m_layerTreeView->setDeviceScaleFactor(scaleFactor);
}

bool WebViewImpl::isFixedLayoutModeEnabled() const
{
    if (!page())
        return false;

    Frame* frame = page()->mainFrame();
    if (!frame || !frame->view())
        return false;

    return frame->view()->useFixedLayout();
}

void WebViewImpl::enableFixedLayoutMode(bool enable)
{
    if (!page())
        return;

    Frame* frame = page()->mainFrame();
    if (!frame || !frame->view())
        return;

    frame->view()->setUseFixedLayout(enable);

    if (m_isAcceleratedCompositingActive)
        updateLayerTreeViewport();
}


void WebViewImpl::enableAutoResizeMode(const WebSize& minSize, const WebSize& maxSize)
{
    m_shouldAutoResize = true;
    m_minAutoSize = minSize;
    m_maxAutoSize = maxSize;
    configureAutoResizeMode();
}

void WebViewImpl::disableAutoResizeMode()
{
    m_shouldAutoResize = false;
    configureAutoResizeMode();
}

void WebViewImpl::setUserAgentPageScaleConstraints(PageScaleConstraints newConstraints)
{
    if (newConstraints == m_pageScaleConstraintsSet.userAgentConstraints())
        return;

    m_pageScaleConstraintsSet.setUserAgentConstraints(newConstraints);

    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    mainFrameImpl()->frameView()->setNeedsLayout();
}

void WebViewImpl::setInitialPageScaleOverride(float initialPageScaleFactorOverride)
{
    PageScaleConstraints constraints = m_pageScaleConstraintsSet.userAgentConstraints();
    constraints.initialScale = initialPageScaleFactorOverride;

    if (constraints == m_pageScaleConstraintsSet.userAgentConstraints())
        return;

    m_pageScaleConstraintsSet.setNeedsReset(true);
    setUserAgentPageScaleConstraints(constraints);
}

void WebViewImpl::setPageScaleFactorLimits(float minPageScale, float maxPageScale)
{
    PageScaleConstraints constraints = m_pageScaleConstraintsSet.userAgentConstraints();
    constraints.minimumScale = minPageScale;
    constraints.maximumScale = maxPageScale;
    setUserAgentPageScaleConstraints(constraints);
}

void WebViewImpl::setIgnoreViewportTagScaleLimits(bool ignore)
{
    PageScaleConstraints constraints = m_pageScaleConstraintsSet.userAgentConstraints();
    if (ignore) {
        constraints.minimumScale = m_pageScaleConstraintsSet.defaultConstraints().minimumScale;
        constraints.maximumScale = m_pageScaleConstraintsSet.defaultConstraints().maximumScale;
    } else {
        constraints.minimumScale = -1;
        constraints.maximumScale = -1;
    }
    setUserAgentPageScaleConstraints(constraints);
}

void WebViewImpl::refreshPageScaleFactorAfterLayout()
{
    if (!mainFrame() || !page() || !page()->mainFrame() || !page()->mainFrame()->view())
        return;
    FrameView* view = page()->mainFrame()->view();

    updatePageDefinedPageScaleConstraints(mainFrameImpl()->frame()->document()->viewportArguments());
    m_pageScaleConstraintsSet.computeFinalConstraints();

    if (settings()->viewportEnabled()) {
        int verticalScrollbarWidth = 0;
        if (view->verticalScrollbar() && !view->verticalScrollbar()->isOverlayScrollbar())
            verticalScrollbarWidth = view->verticalScrollbar()->width();
        m_pageScaleConstraintsSet.adjustFinalConstraintsToContentsSize(m_size, contentsSize(), verticalScrollbarWidth);
    }

    float newPageScaleFactor = pageScaleFactor();
    if (m_pageScaleConstraintsSet.needsReset() && m_pageScaleConstraintsSet.finalConstraints().initialScale != -1) {
        newPageScaleFactor = m_pageScaleConstraintsSet.finalConstraints().initialScale;
        m_pageScaleConstraintsSet.setNeedsReset(false);
    }
    setPageScaleFactorPreservingScrollOffset(newPageScaleFactor);

    updateLayerTreeViewport();

    // Relayout immediately to avoid violating the rule that needsLayout()
    // isn't set at the end of a layout.
    if (view->needsLayout())
        view->layout();
}

void WebViewImpl::updatePageDefinedPageScaleConstraints(const ViewportArguments& arguments)
{
    if (!settings()->viewportEnabled() || !isFixedLayoutModeEnabled() || !page() || !m_size.width || !m_size.height)
        return;

    ViewportArguments adjustedArguments = arguments;
    if (settingsImpl()->viewportMetaLayoutSizeQuirk() && adjustedArguments.type == ViewportArguments::ViewportMeta) {
        if (adjustedArguments.maxWidth.type() == ExtendToZoom)
            adjustedArguments.maxWidth = Length(); // auto
        adjustedArguments.minWidth = adjustedArguments.maxWidth;
        adjustedArguments.minHeight = adjustedArguments.maxHeight;
    }
    m_pageScaleConstraintsSet.updatePageDefinedConstraints(adjustedArguments, m_size);

    if (settingsImpl()->supportDeprecatedTargetDensityDPI())
        m_pageScaleConstraintsSet.adjustPageDefinedConstraintsForAndroidWebView(adjustedArguments, m_size, page()->settings().layoutFallbackWidth(), deviceScaleFactor(), page()->settings().useWideViewport(), page()->settings().loadWithOverviewMode());

    WebSize layoutSize = flooredIntSize(m_pageScaleConstraintsSet.pageDefinedConstraints().layoutSize);

    if (page()->settings().textAutosizingEnabled() && page()->mainFrame() && layoutSize.width != fixedLayoutSize().width)
        page()->mainFrame()->document()->textAutosizer()->recalculateMultipliers();

    setFixedLayoutSize(layoutSize);
}

IntSize WebViewImpl::contentsSize() const
{
    RenderView* root = page()->mainFrame()->contentRenderer();
    if (!root)
        return IntSize();
    return root->documentRect().size();
}

WebSize WebViewImpl::contentsPreferredMinimumSize()
{
    Document* document = m_page->mainFrame()->document();
    if (!document || !document->renderView() || !document->documentElement())
        return WebSize();

    layout();
    FontCachePurgePreventer fontCachePurgePreventer; // Required by minPreferredLogicalWidth().
    IntSize preferredMinimumSize(document->renderView()->minPreferredLogicalWidth(), document->documentElement()->scrollHeight());
    preferredMinimumSize.scale(zoomLevelToZoomFactor(zoomLevel()));
    return preferredMinimumSize;
}

float WebViewImpl::minimumPageScaleFactor() const
{
    return m_pageScaleConstraintsSet.finalConstraints().minimumScale;
}

float WebViewImpl::maximumPageScaleFactor() const
{
    return m_pageScaleConstraintsSet.finalConstraints().maximumScale;
}

void WebViewImpl::saveScrollAndScaleState()
{
    m_savedPageScaleFactor = pageScaleFactor();
    m_savedScrollOffset = mainFrame()->scrollOffset();
}

void WebViewImpl::restoreScrollAndScaleState()
{
    if (!m_savedPageScaleFactor)
        return;

    startPageScaleAnimation(IntPoint(m_savedScrollOffset), false, m_savedPageScaleFactor, scrollAndScaleAnimationDurationInSeconds);
    resetSavedScrollAndScaleState();
}

void WebViewImpl::resetSavedScrollAndScaleState()
{
    m_savedPageScaleFactor = 0;
    m_savedScrollOffset = IntSize();
}

void WebViewImpl::resetScrollAndScaleState()
{
    page()->setPageScaleFactor(1, IntPoint());

    // Clear out the values for the current history item. This will prevent the history item from clobbering the
    // value determined during page scale initialization, which may be less than 1.
    page()->mainFrame()->loader()->history()->saveDocumentAndScrollState();
    page()->mainFrame()->loader()->history()->clearScrollPositionAndViewState();
    m_pageScaleConstraintsSet.setNeedsReset(true);

    // Clobber saved scales and scroll offsets.
    if (FrameView* view = page()->mainFrame()->document()->view())
        view->cacheCurrentScrollPosition();
    resetSavedScrollAndScaleState();
}

WebSize WebViewImpl::fixedLayoutSize() const
{
    if (!page())
        return WebSize();

    Frame* frame = page()->mainFrame();
    if (!frame || !frame->view())
        return WebSize();

    return frame->view()->fixedLayoutSize();
}

void WebViewImpl::setFixedLayoutSize(const WebSize& layoutSize)
{
    if (!page())
        return;

    Frame* frame = page()->mainFrame();
    if (!frame || !frame->view())
        return;

    frame->view()->setFixedLayoutSize(layoutSize);
}

void WebViewImpl::performMediaPlayerAction(const WebMediaPlayerAction& action,
                                           const WebPoint& location)
{
    HitTestResult result = hitTestResultForWindowPos(location);
    RefPtr<Node> node = result.innerNonSharedNode();
    if (!isHTMLVideoElement(node.get()) && !node->hasTagName(HTMLNames::audioTag))
        return;

    RefPtr<HTMLMediaElement> mediaElement =
        static_pointer_cast<HTMLMediaElement>(node);
    switch (action.type) {
    case WebMediaPlayerAction::Play:
        if (action.enable)
            mediaElement->play();
        else
            mediaElement->pause();
        break;
    case WebMediaPlayerAction::Mute:
        mediaElement->setMuted(action.enable);
        break;
    case WebMediaPlayerAction::Loop:
        mediaElement->setLoop(action.enable);
        break;
    case WebMediaPlayerAction::Controls:
        mediaElement->setControls(action.enable);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void WebViewImpl::performPluginAction(const WebPluginAction& action,
                                      const WebPoint& location)
{
    HitTestResult result = hitTestResultForWindowPos(location);
    RefPtr<Node> node = result.innerNonSharedNode();
    if (!node->hasTagName(HTMLNames::objectTag) && !node->hasTagName(HTMLNames::embedTag))
        return;

    RenderObject* object = node->renderer();
    if (object && object->isWidget()) {
        Widget* widget = toRenderWidget(object)->widget();
        if (widget && widget->isPluginContainer()) {
            WebPluginContainerImpl* plugin = toPluginContainerImpl(widget);
            switch (action.type) {
            case WebPluginAction::Rotate90Clockwise:
                plugin->plugin()->rotateView(WebPlugin::RotationType90Clockwise);
                break;
            case WebPluginAction::Rotate90Counterclockwise:
                plugin->plugin()->rotateView(WebPlugin::RotationType90Counterclockwise);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        }
    }
}

WebHitTestResult WebViewImpl::hitTestResultAt(const WebPoint& point)
{
    IntPoint scaledPoint = point;
    scaledPoint.scale(1 / pageScaleFactor(), 1 / pageScaleFactor());
    return hitTestResultForWindowPos(scaledPoint);
}

void WebViewImpl::copyImageAt(const WebPoint& point)
{
    if (!m_page)
        return;

    HitTestResult result = hitTestResultForWindowPos(point);

    if (result.absoluteImageURL().isEmpty()) {
        // There isn't actually an image at these coordinates.  Might be because
        // the window scrolled while the context menu was open or because the page
        // changed itself between when we thought there was an image here and when
        // we actually tried to retreive the image.
        //
        // FIXME: implement a cache of the most recent HitTestResult to avoid having
        //        to do two hit tests.
        return;
    }

    m_page->mainFrame()->editor().copyImage(result);
}

void WebViewImpl::dragSourceEndedAt(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperation operation)
{
    PlatformMouseEvent pme(clientPoint,
                           screenPoint,
                           LeftButton, PlatformEvent::MouseMoved, 0, false, false, false,
                           false, 0);
    m_page->mainFrame()->eventHandler()->dragSourceEndedAt(pme,
        static_cast<DragOperation>(operation));
}

void WebViewImpl::dragSourceMovedTo(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperation operation)
{
}

void WebViewImpl::dragSourceSystemDragEnded()
{
    // It's possible for us to get this callback while not doing a drag if
    // it's from a previous page that got unloaded.
    if (m_doingDragAndDrop) {
        m_page->dragController().dragEnded();
        m_doingDragAndDrop = false;
    }
}

WebDragOperation WebViewImpl::dragTargetDragEnter(
    const WebDragData& webDragData,
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed,
    int keyModifiers)
{
    ASSERT(!m_currentDragData);

    m_currentDragData = webDragData;
    m_operationsAllowed = operationsAllowed;

    return dragTargetDragEnterOrOver(clientPoint, screenPoint, DragEnter, keyModifiers);
}

WebDragOperation WebViewImpl::dragTargetDragOver(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed,
    int keyModifiers)
{
    m_operationsAllowed = operationsAllowed;

    return dragTargetDragEnterOrOver(clientPoint, screenPoint, DragOver, keyModifiers);
}

void WebViewImpl::dragTargetDragLeave()
{
    ASSERT(m_currentDragData);

    DragData dragData(
        m_currentDragData.get(),
        IntPoint(),
        IntPoint(),
        static_cast<DragOperation>(m_operationsAllowed));

    m_page->dragController().dragExited(&dragData);

    // FIXME: why is the drag scroll timer not stopped here?

    m_dragOperation = WebDragOperationNone;
    m_currentDragData = 0;
}

void WebViewImpl::dragTargetDrop(const WebPoint& clientPoint,
                                 const WebPoint& screenPoint,
                                 int keyModifiers)
{
    ASSERT(m_currentDragData);

    // If this webview transitions from the "drop accepting" state to the "not
    // accepting" state, then our IPC message reply indicating that may be in-
    // flight, or else delayed by javascript processing in this webview.  If a
    // drop happens before our IPC reply has reached the browser process, then
    // the browser forwards the drop to this webview.  So only allow a drop to
    // proceed if our webview m_dragOperation state is not DragOperationNone.

    if (m_dragOperation == WebDragOperationNone) { // IPC RACE CONDITION: do not allow this drop.
        dragTargetDragLeave();
        return;
    }

    m_currentDragData->setModifierKeyState(webInputEventKeyStateToPlatformEventKeyState(keyModifiers));
    DragData dragData(
        m_currentDragData.get(),
        clientPoint,
        screenPoint,
        static_cast<DragOperation>(m_operationsAllowed));

    m_page->dragController().performDrag(&dragData);

    m_dragOperation = WebDragOperationNone;
    m_currentDragData = 0;
}

void WebViewImpl::spellingMarkers(WebVector<uint32_t>* markers)
{
    Vector<uint32_t> result;
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        const Vector<DocumentMarker*>& documentMarkers = frame->document()->markers()->markers();
        for (size_t i = 0; i < documentMarkers.size(); ++i)
            result.append(documentMarkers[i]->hash());
    }
    markers->assign(result);
}

WebDragOperation WebViewImpl::dragTargetDragEnterOrOver(const WebPoint& clientPoint, const WebPoint& screenPoint, DragAction dragAction, int keyModifiers)
{
    ASSERT(m_currentDragData);

    m_currentDragData->setModifierKeyState(webInputEventKeyStateToPlatformEventKeyState(keyModifiers));
    DragData dragData(
        m_currentDragData.get(),
        clientPoint,
        screenPoint,
        static_cast<DragOperation>(m_operationsAllowed));

    DragSession dragSession;
    if (dragAction == DragEnter)
        dragSession = m_page->dragController().dragEntered(&dragData);
    else
        dragSession = m_page->dragController().dragUpdated(&dragData);

    DragOperation dropEffect = dragSession.operation;

    // Mask the drop effect operation against the drag source's allowed operations.
    if (!(dropEffect & dragData.draggingSourceOperationMask()))
        dropEffect = DragOperationNone;

     m_dragOperation = static_cast<WebDragOperation>(dropEffect);

    return m_dragOperation;
}

void WebViewImpl::sendResizeEventAndRepaint()
{
    // FIXME: This is wrong. The FrameView is responsible sending a resizeEvent
    // as part of layout. Layout is also responsible for sending invalidations
    // to the embedder. This method and all callers may be wrong. -- eseidel.
    if (mainFrameImpl()->frameView()) {
        // Enqueues the resize event.
        mainFrameImpl()->frame()->eventHandler()->sendResizeEvent();
    }

    if (m_client) {
        if (isAcceleratedCompositingActive()) {
            updateLayerTreeViewport();
        } else {
            WebRect damagedRect(0, 0, m_size.width, m_size.height);
            m_client->didInvalidateRect(damagedRect);
        }
    }
    if (m_pageOverlays)
        m_pageOverlays->update();
}

void WebViewImpl::configureAutoResizeMode()
{
    if (!mainFrameImpl() || !mainFrameImpl()->frame() || !mainFrameImpl()->frame()->view())
        return;

    mainFrameImpl()->frame()->view()->enableAutoSizeMode(m_shouldAutoResize, m_minAutoSize, m_maxAutoSize);
}

unsigned long WebViewImpl::createUniqueIdentifierForRequest()
{
    return createUniqueIdentifier();
}

void WebViewImpl::inspectElementAt(const WebPoint& point)
{
    if (!m_page)
        return;

    if (point.x == -1 || point.y == -1) {
        m_page->inspectorController().inspect(0);
    } else {
        HitTestRequest::HitTestRequestType hitType = HitTestRequest::Move | HitTestRequest::ReadOnly | HitTestRequest::AllowChildFrameContent | HitTestRequest::IgnorePointerEventsNone;
        HitTestRequest request(hitType);

        HitTestResult result(m_page->mainFrame()->view()->windowToContents(point));
        m_page->mainFrame()->contentRenderer()->hitTest(request, result);
        Node* node = result.innerNode();
        if (!node && m_page->mainFrame()->document())
            node = m_page->mainFrame()->document()->documentElement();
        m_page->inspectorController().inspect(node);
    }
}

WebString WebViewImpl::inspectorSettings() const
{
    return m_inspectorSettings;
}

void WebViewImpl::setInspectorSettings(const WebString& settings)
{
    m_inspectorSettings = settings;
}

bool WebViewImpl::inspectorSetting(const WebString& key, WebString* value) const
{
    if (!m_inspectorSettingsMap->contains(key))
        return false;
    *value = m_inspectorSettingsMap->get(key);
    return true;
}

void WebViewImpl::setInspectorSetting(const WebString& key,
                                      const WebString& value)
{
    m_inspectorSettingsMap->set(key, value);
    client()->didUpdateInspectorSetting(key, value);
}

WebDevToolsAgent* WebViewImpl::devToolsAgent()
{
    return m_devToolsAgent.get();
}

WebAXObject WebViewImpl::accessibilityObject()
{
    if (!mainFrameImpl())
        return WebAXObject();

    Document* document = mainFrameImpl()->frame()->document();
    return WebAXObject(
        document->axObjectCache()->getOrCreate(document->renderer()));
}

void WebViewImpl::applyAutofillSuggestions(
    const WebNode& node,
    const WebVector<WebString>& names,
    const WebVector<WebString>& labels,
    const WebVector<WebString>& icons,
    const WebVector<int>& itemIDs,
    int separatorIndex)
{
    ASSERT(names.size() == labels.size());
    ASSERT(names.size() == itemIDs.size());

    if (names.isEmpty()) {
        hideAutofillPopup();
        return;
    }

    RefPtr<Element> element = focusedElement();
    // If the node for which we queried the Autofill suggestions is not the
    // focused node, then we have nothing to do.  FIXME: also check the
    // caret is at the end and that the text has not changed.
    if (!element || element != PassRefPtr<Node>(node)) {
        hideAutofillPopup();
        return;
    }

    HTMLInputElement* inputElem = toHTMLInputElement(element.get());

    // The first time the Autofill popup is shown we'll create the client and
    // the popup.
    if (!m_autofillPopupClient)
        m_autofillPopupClient = adoptPtr(new AutofillPopupMenuClient);

    m_autofillPopupClient->initialize(
        inputElem, names, labels, icons, itemIDs, separatorIndex);

    if (!m_autofillPopup) {
        PopupContainerSettings popupSettings = autofillPopupSettings;
        popupSettings.deviceSupportsTouch = settingsImpl()->deviceSupportsTouch();
        m_autofillPopup = PopupContainer::create(m_autofillPopupClient.get(),
                                                 PopupContainer::Suggestion,
                                                 popupSettings);
    }

    if (m_autofillPopupShowing) {
        refreshAutofillPopup();
    } else {
        m_autofillPopupShowing = true;
        IntRect rect = element->pixelSnappedBoundingBox();
        m_autofillPopup->showInRect(FloatQuad(rect), rect.size(), element->ownerDocument()->view(), 0);
    }
}

void WebViewImpl::hidePopups()
{
    hideSelectPopup();
    hideAutofillPopup();
    if (m_pagePopup)
        closePagePopup(m_pagePopup.get());
}

void WebViewImpl::performCustomContextMenuAction(unsigned action)
{
    if (!m_page)
        return;
    ContextMenu* menu = m_page->contextMenuController().contextMenu();
    if (!menu)
        return;
    const ContextMenuItem* item = menu->itemWithAction(static_cast<ContextMenuAction>(ContextMenuItemBaseCustomTag + action));
    if (item)
        m_page->contextMenuController().contextMenuItemSelected(item);
    m_page->contextMenuController().clearContextMenu();
}

void WebViewImpl::showContextMenu()
{
    if (!page())
        return;

    page()->contextMenuController().clearContextMenu();
    m_contextMenuAllowed = true;
    if (Frame* focusedFrame = page()->focusController().focusedOrMainFrame())
        focusedFrame->eventHandler()->sendContextMenuEventForKey();
    m_contextMenuAllowed = false;
}

// WebView --------------------------------------------------------------------

void WebViewImpl::setIsTransparent(bool isTransparent)
{
    // Set any existing frames to be transparent.
    Frame* frame = m_page->mainFrame();
    while (frame) {
        frame->view()->setTransparent(isTransparent);
        frame = frame->tree()->traverseNext();
    }

    // Future frames check this to know whether to be transparent.
    m_isTransparent = isTransparent;
}

bool WebViewImpl::isTransparent() const
{
    return m_isTransparent;
}

void WebViewImpl::setBaseBackgroundColor(WebColor color)
{
    if (m_baseBackgroundColor == color)
        return;

    m_baseBackgroundColor = color;

    m_page->mainFrame()->view()->setBaseBackgroundColor(color);

    if (m_layerTreeView)
        m_layerTreeView->setBackgroundColor(backgroundColor());
}

void WebViewImpl::setIsActive(bool active)
{
    if (page())
        page()->focusController().setActive(active);
}

bool WebViewImpl::isActive() const
{
    return page() ? page()->focusController().isActive() : false;
}

void WebViewImpl::setDomainRelaxationForbidden(bool forbidden, const WebString& scheme)
{
    SchemeRegistry::setDomainRelaxationForbiddenForURLScheme(forbidden, String(scheme));
}

void WebViewImpl::setWindowFeatures(const WebWindowFeatures& features)
{
    m_page->chrome().setWindowFeatures(features);
}

void WebViewImpl::setScrollbarColors(unsigned inactiveColor,
                                     unsigned activeColor,
                                     unsigned trackColor) {
#if ENABLE(DEFAULT_RENDER_THEME)
    PlatformThemeChromiumDefault::setScrollbarColors(inactiveColor, activeColor, trackColor);
#endif
}

void WebViewImpl::setSelectionColors(unsigned activeBackgroundColor,
                                     unsigned activeForegroundColor,
                                     unsigned inactiveBackgroundColor,
                                     unsigned inactiveForegroundColor) {
#if ENABLE(DEFAULT_RENDER_THEME)
    RenderThemeChromiumDefault::setSelectionColors(activeBackgroundColor, activeForegroundColor, inactiveBackgroundColor, inactiveForegroundColor);
    RenderTheme::theme().platformColorsDidChange();
#endif
}

void WebView::addUserStyleSheet(const WebString& sourceCode,
                                const WebVector<WebString>& patternsIn,
                                WebView::UserContentInjectIn injectIn,
                                WebView::UserStyleInjectionTime injectionTime)
{
    Vector<String> patterns;
    for (size_t i = 0; i < patternsIn.size(); ++i)
        patterns.append(patternsIn[i]);

    PageGroup* pageGroup = PageGroup::sharedGroup();

    // FIXME: Current callers always want the level to be "author". It probably makes sense to let
    // callers specify this though, since in other cases the caller will probably want "user" level.
    //
    // FIXME: It would be nice to populate the URL correctly, instead of passing an empty URL.
    pageGroup->addUserStyleSheet(sourceCode, WebURL(), patterns, Vector<String>(),
                                 static_cast<UserContentInjectedFrames>(injectIn),
                                 UserStyleAuthorLevel,
                                 static_cast<WebCore::UserStyleInjectionTime>(injectionTime));
}

void WebView::removeAllUserContent()
{
    PageGroup::sharedGroup()->removeAllUserContent();
}

void WebViewImpl::didCommitLoad(bool* isNewNavigation, bool isNavigationWithinPage)
{
    if (isNewNavigation)
        *isNewNavigation = m_observedNewNavigation;

#ifndef NDEBUG
    ASSERT(!m_observedNewNavigation
        || m_page->mainFrame()->loader()->documentLoader() == m_newNavigationLoader);
    m_newNavigationLoader = 0;
#endif
    m_observedNewNavigation = false;
    if (*isNewNavigation && !isNavigationWithinPage)
        m_pageScaleConstraintsSet.setNeedsReset(true);

    // Make sure link highlight from previous page is cleared.
    m_linkHighlight.clear();
    m_gestureAnimation.clear();
    if (m_layerTreeView)
        m_layerTreeView->didStopFlinging();
    resetSavedScrollAndScaleState();
}

void WebViewImpl::layoutUpdated(WebFrameImpl* webframe)
{
    if (!m_client || webframe != mainFrameImpl())
        return;

    if (m_layerTreeViewCommitsDeferred) {
        // If we finished a layout while in deferred commit mode,
        // that means it's time to start producing frames again so un-defer.
        if (m_layerTreeView)
            m_layerTreeView->setDeferCommits(false);
        m_layerTreeViewCommitsDeferred = false;
    }

    if (m_shouldAutoResize && mainFrameImpl()->frame() && mainFrameImpl()->frame()->view()) {
        WebSize frameSize = mainFrameImpl()->frame()->view()->frameRect().size();
        if (frameSize != m_size) {
            m_size = frameSize;
            m_client->didAutoResize(m_size);
            sendResizeEventAndRepaint();
        }
    }

    if (m_pageScaleConstraintsSet.constraintsDirty())
        refreshPageScaleFactorAfterLayout();

    m_client->didUpdateLayout();
}

void WebViewImpl::didChangeContentsSize()
{
    m_pageScaleConstraintsSet.didChangeContentsSize(contentsSize(), pageScaleFactor());
}

void WebViewImpl::deviceOrPageScaleFactorChanged()
{
    if (pageScaleFactor() && pageScaleFactor() != 1)
        enterForceCompositingMode(true);
    m_pageScaleConstraintsSet.setNeedsReset(false);
    updateLayerTreeViewport();
}

bool WebViewImpl::useExternalPopupMenus()
{
    return shouldUseExternalPopupMenus;
}

void WebViewImpl::startDragging(Frame* frame,
                                const WebDragData& dragData,
                                WebDragOperationsMask mask,
                                const WebImage& dragImage,
                                const WebPoint& dragImageOffset)
{
    if (!m_client)
        return;
    ASSERT(!m_doingDragAndDrop);
    m_doingDragAndDrop = true;
    m_client->startDragging(WebFrameImpl::fromFrame(frame), dragData, mask, dragImage, dragImageOffset);
}

void WebViewImpl::observeNewNavigation()
{
    m_observedNewNavigation = true;
#ifndef NDEBUG
    m_newNavigationLoader = m_page->mainFrame()->loader()->documentLoader();
#endif
}

void WebViewImpl::setIgnoreInputEvents(bool newValue)
{
    ASSERT(m_ignoreInputEvents != newValue);
    m_ignoreInputEvents = newValue;
}

void WebViewImpl::addPageOverlay(WebPageOverlay* overlay, int zOrder)
{
    if (!m_pageOverlays)
        m_pageOverlays = PageOverlayList::create(this);

    m_pageOverlays->add(overlay, zOrder);
}

void WebViewImpl::removePageOverlay(WebPageOverlay* overlay)
{
    if (m_pageOverlays && m_pageOverlays->remove(overlay) && m_pageOverlays->empty())
        m_pageOverlays = nullptr;
}

void WebViewImpl::setOverlayLayer(WebCore::GraphicsLayer* layer)
{
    if (m_rootGraphicsLayer) {
        if (layer->parent() != m_rootGraphicsLayer)
            m_rootGraphicsLayer->addChild(layer);
    }
}

NotificationPresenterImpl* WebViewImpl::notificationPresenterImpl()
{
    if (!m_notificationPresenter.isInitialized() && m_client)
        m_notificationPresenter.initialize(m_client->notificationPresenter());
    return &m_notificationPresenter;
}

void WebViewImpl::refreshAutofillPopup()
{
    ASSERT(m_autofillPopupShowing);

    // Hide the popup if it has become empty.
    if (!m_autofillPopupClient->listSize()) {
        hideAutofillPopup();
        return;
    }

    WebRect newWidgetRect = m_autofillPopup->refresh(focusedElement()->pixelSnappedBoundingBox());
    // Let's resize the backing window if necessary.
    WebPopupMenuImpl* popupMenu = static_cast<WebPopupMenuImpl*>(m_autofillPopup->client());
    if (popupMenu && popupMenu->client()->windowRect() != newWidgetRect)
        popupMenu->client()->setWindowRect(newWidgetRect);
}

Element* WebViewImpl::focusedElement()
{
    Frame* frame = m_page->focusController().focusedFrame();
    if (!frame)
        return 0;

    Document* document = frame->document();
    if (!document)
        return 0;

    return document->focusedElement();
}

HitTestResult WebViewImpl::hitTestResultForWindowPos(const IntPoint& pos)
{
    IntPoint docPoint(m_page->mainFrame()->view()->windowToContents(pos));
    return m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(docPoint, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent);
}

void WebViewImpl::setTabsToLinks(bool enable)
{
    m_tabsToLinks = enable;
}

bool WebViewImpl::tabsToLinks() const
{
    return m_tabsToLinks;
}

void WebViewImpl::suppressInvalidations(bool enable)
{
    if (m_client)
        m_client->suppressCompositorScheduling(enable);
}

bool WebViewImpl::allowsAcceleratedCompositing()
{
    return !m_compositorCreationFailed;
}

void WebViewImpl::setRootGraphicsLayer(GraphicsLayer* layer)
{
    suppressInvalidations(true);

    if (page()->settings().pinchVirtualViewportEnabled()) {
        if (!m_pinchViewports)
            m_pinchViewports = PinchViewports::create(this);

        m_pinchViewports->setOverflowControlsHostLayer(layer);
        m_pinchViewports->setViewportSize(mainFrameImpl()->frame()->view()->frameRect().size());
        if (layer) {
            m_rootGraphicsLayer = m_pinchViewports->rootGraphicsLayer();
            m_rootLayer = m_pinchViewports->rootGraphicsLayer()->platformLayer();
            m_pinchViewports->registerViewportLayersWithTreeView(m_layerTreeView);
        } else {
            m_rootGraphicsLayer = 0;
            m_rootLayer = 0;
            m_pinchViewports->clearViewportLayersForTreeView(m_layerTreeView);
        }
    } else {
        m_rootGraphicsLayer = layer;
        m_rootLayer = layer ? layer->platformLayer() : 0;
    }

    setIsAcceleratedCompositingActive(layer);

    if (m_layerTreeView) {
        if (m_rootLayer)
            m_layerTreeView->setRootLayer(*m_rootLayer);
        else
            m_layerTreeView->clearRootLayer();
    }

    suppressInvalidations(false);
}

void WebViewImpl::scheduleCompositingLayerSync()
{
    m_layerTreeView->setNeedsRedraw();
}

void WebViewImpl::scrollRootLayerRect(const IntSize&, const IntRect&)
{
    updateLayerTreeViewport();
}

void WebViewImpl::invalidateRect(const IntRect& rect)
{
    if (m_isAcceleratedCompositingActive) {
        ASSERT(m_layerTreeView);
        updateLayerTreeViewport();
    } else if (m_client)
        m_client->didInvalidateRect(rect);
}

WebCore::GraphicsLayerFactory* WebViewImpl::graphicsLayerFactory() const
{
    return m_graphicsLayerFactory.get();
}

WebCore::RenderLayerCompositor* WebViewImpl::compositor() const
{
    if (!page()
        || !page()->mainFrame()
        || !page()->mainFrame()->document()
        || !page()->mainFrame()->document()->renderView())
        return 0;
    return page()->mainFrame()->document()->renderView()->compositor();
}

void WebViewImpl::registerForAnimations(WebLayer* layer)
{
    if (m_layerTreeView)
        m_layerTreeView->registerForAnimations(layer);
}

WebCore::GraphicsLayer* WebViewImpl::rootGraphicsLayer()
{
    return m_rootGraphicsLayer;
}

void WebViewImpl::scheduleAnimation()
{
    if (isAcceleratedCompositingActive() && Platform::current()->isThreadedCompositingEnabled()) {
        ASSERT(m_layerTreeView);
        m_layerTreeView->setNeedsAnimate();
        return;
    }
    if (m_client)
        m_client->scheduleAnimation();
}

void WebViewImpl::setIsAcceleratedCompositingActive(bool active)
{
    WebKit::Platform::current()->histogramEnumeration("GPU.setIsAcceleratedCompositingActive", active * 2 + m_isAcceleratedCompositingActive, 4);

    if (m_isAcceleratedCompositingActive == active)
        return;

    if (!active) {
        m_isAcceleratedCompositingActive = false;
        // We need to finish all GL rendering before sending didDeactivateCompositor() to prevent
        // flickering when compositing turns off. This is only necessary if we're not in
        // force-compositing-mode.
        if (m_layerTreeView && !page()->settings().forceCompositingMode())
            m_layerTreeView->finishAllRendering();
        m_client->didDeactivateCompositor();
        if (!m_layerTreeViewCommitsDeferred
            && WebKit::Platform::current()->isThreadedCompositingEnabled()) {
            ASSERT(m_layerTreeView);
            // In threaded compositing mode, force compositing mode is always on so setIsAcceleratedCompositingActive(false)
            // means that we're transitioning to a new page. Suppress commits until WebKit generates invalidations so
            // we don't attempt to paint too early in the next page load.
            m_layerTreeView->setDeferCommits(true);
            m_layerTreeViewCommitsDeferred = true;
        }
    } else if (m_layerTreeView) {
        m_isAcceleratedCompositingActive = true;
        updateLayerTreeViewport();
        if (m_pageOverlays)
            m_pageOverlays->update();

        m_client->didActivateCompositor(0);
    } else {
        TRACE_EVENT0("webkit", "WebViewImpl::setIsAcceleratedCompositingActive(true)");

        m_client->initializeLayerTreeView();
        m_layerTreeView = m_client->layerTreeView();
        if (m_layerTreeView) {
            m_layerTreeView->setRootLayer(*m_rootLayer);

            bool visible = page()->visibilityState() == PageVisibilityStateVisible;
            m_layerTreeView->setVisible(visible);
            m_layerTreeView->setDeviceScaleFactor(page()->deviceScaleFactor());
            m_layerTreeView->setPageScaleFactorAndLimits(pageScaleFactor(), minimumPageScaleFactor(), maximumPageScaleFactor());
            m_layerTreeView->setBackgroundColor(backgroundColor());
            m_layerTreeView->setHasTransparentBackground(isTransparent());
            updateLayerTreeViewport();
            m_client->didActivateCompositor(0);
            m_isAcceleratedCompositingActive = true;
            m_compositorCreationFailed = false;
            if (m_pageOverlays)
                m_pageOverlays->update();
            m_layerTreeView->setShowFPSCounter(m_showFPSCounter);
            m_layerTreeView->setShowPaintRects(m_showPaintRects);
            m_layerTreeView->setShowDebugBorders(m_showDebugBorders);
            m_layerTreeView->setContinuousPaintingEnabled(m_continuousPaintingEnabled);
            m_layerTreeView->setShowScrollBottleneckRects(m_showScrollBottleneckRects);
        } else {
            m_isAcceleratedCompositingActive = false;
            m_client->didDeactivateCompositor();
            m_compositorCreationFailed = true;
        }
    }
    if (page())
        page()->mainFrame()->view()->setClipsRepaints(!m_isAcceleratedCompositingActive);
}

void WebViewImpl::updateMainFrameScrollPosition(const IntPoint& scrollPosition, bool programmaticScroll)
{
    FrameView* frameView = page()->mainFrame()->view();
    if (!frameView)
        return;

    if (frameView->scrollPosition() == scrollPosition)
        return;

    bool oldProgrammaticScroll = frameView->inProgrammaticScroll();
    frameView->setInProgrammaticScroll(programmaticScroll);
    frameView->notifyScrollPositionChanged(scrollPosition);
    frameView->setInProgrammaticScroll(oldProgrammaticScroll);
}

void WebViewImpl::applyScrollAndScale(const WebSize& scrollDelta, float pageScaleDelta)
{
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    if (pageScaleDelta == 1) {
        TRACE_EVENT_INSTANT2("webkit", "WebViewImpl::applyScrollAndScale::scrollBy", "x", scrollDelta.width, "y", scrollDelta.height);
        WebSize webScrollOffset = mainFrame()->scrollOffset();
        IntPoint scrollOffset(webScrollOffset.width + scrollDelta.width, webScrollOffset.height + scrollDelta.height);
        updateMainFrameScrollPosition(scrollOffset, false);
    } else {
        // The page scale changed, so apply a scale and scroll in a single
        // operation.
        WebSize scrollOffset = mainFrame()->scrollOffset();
        scrollOffset.width += scrollDelta.width;
        scrollOffset.height += scrollDelta.height;

        WebPoint scrollPoint(scrollOffset.width, scrollOffset.height);
        setPageScaleFactor(pageScaleFactor() * pageScaleDelta, scrollPoint);
        m_doubleTapZoomPending = false;
    }
}

void WebViewImpl::didExitCompositingMode()
{
    ASSERT(m_isAcceleratedCompositingActive);
    setIsAcceleratedCompositingActive(false);
    m_compositorCreationFailed = true;
    m_client->didInvalidateRect(IntRect(0, 0, m_size.width, m_size.height));

    // Force a style recalc to remove all the composited layers.
    m_page->mainFrame()->document()->setNeedsStyleRecalc();

    if (m_pageOverlays)
        m_pageOverlays->update();
}

void WebViewImpl::updateLayerTreeViewport()
{
    if (!page() || !m_layerTreeView)
        return;

    m_layerTreeView->setPageScaleFactorAndLimits(pageScaleFactor(), minimumPageScaleFactor(), maximumPageScaleFactor());
}

void WebViewImpl::selectAutofillSuggestionAtIndex(unsigned listIndex)
{
    if (m_autofillPopupClient && listIndex < m_autofillPopupClient->getSuggestionsCount())
        m_autofillPopupClient->valueChanged(listIndex);
}

bool WebViewImpl::detectContentOnTouch(const WebPoint& position)
{
    HitTestResult touchHit = hitTestResultForWindowPos(position);

    if (touchHit.isContentEditable())
        return false;

    Node* node = touchHit.innerNode();
    if (!node || !node->isTextNode())
        return false;

    // Ignore when tapping on links or nodes listening to click events, unless the click event is on the
    // body element, in which case it's unlikely that the original node itself was intended to be clickable.
    for (; node && !node->hasTagName(HTMLNames::bodyTag); node = node->parentNode()) {
        if (node->isLink() || node->willRespondToTouchEvents() || node->willRespondToMouseClickEvents())
            return false;
    }

    WebContentDetectionResult content = m_client->detectContentAround(touchHit);
    if (!content.isValid())
        return false;

    m_client->scheduleContentIntent(content.intent());
    return true;
}

void WebViewImpl::didProgrammaticallyScroll(const WebCore::IntPoint& scrollPoint)
{
    m_client->didProgrammaticallyScroll(scrollPoint);
}

void WebViewImpl::setVisibilityState(WebPageVisibilityState visibilityState,
                                     bool isInitialState) {
    if (!page())
        return;

    ASSERT(visibilityState == WebPageVisibilityStateVisible
           || visibilityState == WebPageVisibilityStateHidden
           || visibilityState == WebPageVisibilityStatePrerender
           || visibilityState == WebPageVisibilityStatePreview);
    m_page->setVisibilityState(static_cast<PageVisibilityState>(static_cast<int>(visibilityState)), isInitialState);

    if (m_layerTreeView) {
        bool visible = visibilityState == WebPageVisibilityStateVisible;
        m_layerTreeView->setVisible(visible);
    }
}

bool WebViewImpl::requestPointerLock()
{
    return m_client && m_client->requestPointerLock();
}

void WebViewImpl::requestPointerUnlock()
{
    if (m_client)
        m_client->requestPointerUnlock();
}

bool WebViewImpl::isPointerLocked()
{
    return m_client && m_client->isPointerLocked();
}

void WebViewImpl::pointerLockMouseEvent(const WebInputEvent& event)
{
    AtomicString eventType;
    switch (event.type) {
    case WebInputEvent::MouseDown:
        eventType = eventNames().mousedownEvent;
        break;
    case WebInputEvent::MouseUp:
        eventType = eventNames().mouseupEvent;
        break;
    case WebInputEvent::MouseMove:
        eventType = eventNames().mousemoveEvent;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    const WebMouseEvent& mouseEvent = static_cast<const WebMouseEvent&>(event);

    if (page())
        page()->pointerLockController().dispatchLockedMouseEvent(
            PlatformMouseEventBuilder(mainFrameImpl()->frameView(), mouseEvent),
            eventType);
}

bool WebViewImpl::shouldDisableDesktopWorkarounds()
{
    if (!settings()->viewportEnabled() || !isFixedLayoutModeEnabled())
        return false;

    // A document is considered adapted to small screen UAs if one of these holds:
    // 1. The author specified viewport has a constrained width that is equal to
    //    the initial viewport width.
    // 2. The author has disabled viewport zoom.

    const PageScaleConstraints& constraints = m_pageScaleConstraintsSet.pageDefinedConstraints();

    return fixedLayoutSize().width == m_size.width
        || (constraints.minimumScale == constraints.maximumScale && constraints.minimumScale != -1);
}

} // namespace WebKit
