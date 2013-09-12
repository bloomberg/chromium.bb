/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/inspector/InspectorOverlay.h"

#include "InspectorOverlayPage.h"
#include "V8InspectorOverlayHost.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorOverlayHost.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/EmptyClients.h"
#include "core/page/Chrome.h"
#include "core/page/EventHandler.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/platform/JSONValues.h"
#include "core/platform/PlatformMouseEvent.h"
#include "core/platform/graphics/GraphicsContextStateSaver.h"
#include "core/rendering/RenderBoxModelObject.h"
#include "core/rendering/RenderInline.h"
#include "core/rendering/RenderObject.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

namespace {

class InspectorOverlayChromeClient: public EmptyChromeClient {
public:
    InspectorOverlayChromeClient(ChromeClient& client, InspectorOverlay* overlay)
        : m_client(client)
        , m_overlay(overlay)
    { }

    virtual void setCursor(const Cursor& cursor)
    {
        m_client.setCursor(cursor);
    }

    virtual void setToolTip(const String& tooltip, TextDirection direction)
    {
        m_client.setToolTip(tooltip, direction);
    }

    virtual void invalidateRootView(const IntRect& rect)
    {
        m_overlay->invalidate();
    }

    virtual void invalidateContentsAndRootView(const IntRect& rect)
    {
        m_overlay->invalidate();
    }

    virtual void invalidateContentsForSlowScroll(const IntRect& rect)
    {
        m_overlay->invalidate();
    }

private:
    ChromeClient& m_client;
    InspectorOverlay* m_overlay;
};

Path quadToPath(const FloatQuad& quad)
{
    Path quadPath;
    quadPath.moveTo(quad.p1());
    quadPath.addLineTo(quad.p2());
    quadPath.addLineTo(quad.p3());
    quadPath.addLineTo(quad.p4());
    quadPath.closeSubpath();
    return quadPath;
}

void drawOutlinedQuad(GraphicsContext* context, const FloatQuad& quad, const Color& fillColor, const Color& outlineColor)
{
    static const int outlineThickness = 2;

    Path quadPath = quadToPath(quad);

    // Clip out the quad, then draw with a 2px stroke to get a pixel
    // of outline (because inflating a quad is hard)
    {
        context->save();
        context->clipOut(quadPath);

        context->setStrokeThickness(outlineThickness);
        context->setStrokeColor(outlineColor);
        context->strokePath(quadPath);

        context->restore();
    }

    // Now do the fill
    context->setFillColor(fillColor);
    context->fillPath(quadPath);
}

static void contentsQuadToPage(const FrameView* mainView, const FrameView* view, FloatQuad& quad)
{
    quad.setP1(view->contentsToRootView(roundedIntPoint(quad.p1())));
    quad.setP2(view->contentsToRootView(roundedIntPoint(quad.p2())));
    quad.setP3(view->contentsToRootView(roundedIntPoint(quad.p3())));
    quad.setP4(view->contentsToRootView(roundedIntPoint(quad.p4())));
    quad += mainView->scrollOffset();
}

static bool buildNodeQuads(Node* node, Vector<FloatQuad>& quads)
{
    RenderObject* renderer = node->renderer();
    Frame* containingFrame = node->document().frame();

    if (!renderer || !containingFrame)
        return false;

    FrameView* containingView = containingFrame->view();
    FrameView* mainView = containingFrame->page()->mainFrame()->view();
    IntRect boundingBox = pixelSnappedIntRect(containingView->contentsToRootView(renderer->absoluteBoundingBoxRect()));
    boundingBox.move(mainView->scrollOffset());

    // RenderSVGRoot should be highlighted through the isBox() code path, all other SVG elements should just dump their absoluteQuads().
    if (renderer->node() && renderer->node()->isSVGElement() && !renderer->isSVGRoot()) {
        renderer->absoluteQuads(quads);
        for (size_t i = 0; i < quads.size(); ++i)
            contentsQuadToPage(mainView, containingView, quads[i]);
        return false;
    }

    if (!renderer->isBox() && !renderer->isRenderInline())
        return false;

    LayoutRect contentBox;
    LayoutRect paddingBox;
    LayoutRect borderBox;
    LayoutRect marginBox;

    if (renderer->isBox()) {
        RenderBox* renderBox = toRenderBox(renderer);

        // RenderBox returns the "pure" content area box, exclusive of the scrollbars (if present), which also count towards the content area in CSS.
        contentBox = renderBox->contentBoxRect();
        contentBox.setWidth(contentBox.width() + renderBox->verticalScrollbarWidth());
        contentBox.setHeight(contentBox.height() + renderBox->horizontalScrollbarHeight());

        paddingBox = LayoutRect(contentBox.x() - renderBox->paddingLeft(), contentBox.y() - renderBox->paddingTop(),
            contentBox.width() + renderBox->paddingLeft() + renderBox->paddingRight(), contentBox.height() + renderBox->paddingTop() + renderBox->paddingBottom());
        borderBox = LayoutRect(paddingBox.x() - renderBox->borderLeft(), paddingBox.y() - renderBox->borderTop(),
            paddingBox.width() + renderBox->borderLeft() + renderBox->borderRight(), paddingBox.height() + renderBox->borderTop() + renderBox->borderBottom());
        marginBox = LayoutRect(borderBox.x() - renderBox->marginLeft(), borderBox.y() - renderBox->marginTop(),
            borderBox.width() + renderBox->marginWidth(), borderBox.height() + renderBox->marginHeight());
    } else {
        RenderInline* renderInline = toRenderInline(renderer);

        // RenderInline's bounding box includes paddings and borders, excludes margins.
        borderBox = renderInline->linesBoundingBox();
        paddingBox = LayoutRect(borderBox.x() + renderInline->borderLeft(), borderBox.y() + renderInline->borderTop(),
            borderBox.width() - renderInline->borderLeft() - renderInline->borderRight(), borderBox.height() - renderInline->borderTop() - renderInline->borderBottom());
        contentBox = LayoutRect(paddingBox.x() + renderInline->paddingLeft(), paddingBox.y() + renderInline->paddingTop(),
            paddingBox.width() - renderInline->paddingLeft() - renderInline->paddingRight(), paddingBox.height() - renderInline->paddingTop() - renderInline->paddingBottom());
        // Ignore marginTop and marginBottom for inlines.
        marginBox = LayoutRect(borderBox.x() - renderInline->marginLeft(), borderBox.y(),
            borderBox.width() + renderInline->marginWidth(), borderBox.height());
    }

    FloatQuad absContentQuad = renderer->localToAbsoluteQuad(FloatRect(contentBox));
    FloatQuad absPaddingQuad = renderer->localToAbsoluteQuad(FloatRect(paddingBox));
    FloatQuad absBorderQuad = renderer->localToAbsoluteQuad(FloatRect(borderBox));
    FloatQuad absMarginQuad = renderer->localToAbsoluteQuad(FloatRect(marginBox));

    contentsQuadToPage(mainView, containingView, absContentQuad);
    contentsQuadToPage(mainView, containingView, absPaddingQuad);
    contentsQuadToPage(mainView, containingView, absBorderQuad);
    contentsQuadToPage(mainView, containingView, absMarginQuad);

    quads.append(absMarginQuad);
    quads.append(absBorderQuad);
    quads.append(absPaddingQuad);
    quads.append(absContentQuad);

    return true;
}

static void buildNodeHighlight(Node* node, const HighlightConfig& highlightConfig, Highlight* highlight)
{
    RenderObject* renderer = node->renderer();
    Frame* containingFrame = node->document().frame();

    if (!renderer || !containingFrame)
        return;

    highlight->setDataFromConfig(highlightConfig);

    // RenderSVGRoot should be highlighted through the isBox() code path, all other SVG elements should just dump their absoluteQuads().
    if (renderer->node() && renderer->node()->isSVGElement() && !renderer->isSVGRoot())
        highlight->type = HighlightTypeRects;
    else if (renderer->isBox() || renderer->isRenderInline())
        highlight->type = HighlightTypeNode;
    buildNodeQuads(node, highlight->quads);
}

static void buildQuadHighlight(Page* page, const FloatQuad& quad, const HighlightConfig& highlightConfig, Highlight *highlight)
{
    if (!page)
        return;
    highlight->setDataFromConfig(highlightConfig);
    highlight->type = HighlightTypeRects;
    highlight->quads.append(quad);
}

} // anonymous namespace

InspectorOverlay::InspectorOverlay(Page* page, InspectorClient* client)
    : m_page(page)
    , m_client(client)
    , m_inspectModeEnabled(false)
    , m_drawViewSize(false)
    , m_drawViewSizeWithGrid(false)
    , m_timer(this, &InspectorOverlay::onTimer)
    , m_overlayHost(InspectorOverlayHost::create())
    , m_overrides(0)
    , m_overridesTopOffset(0)
{
}

InspectorOverlay::~InspectorOverlay()
{
}

void InspectorOverlay::paint(GraphicsContext& context)
{
    if (isEmpty())
        return;
    GraphicsContextStateSaver stateSaver(context);
    FrameView* view = overlayPage()->mainFrame()->view();
    ASSERT(!view->needsLayout());
    view->paint(&context, IntRect(0, 0, view->width(), view->height()));
}

void InspectorOverlay::invalidate()
{
    m_client->highlight();
}

bool InspectorOverlay::handleGestureEvent(const PlatformGestureEvent& event)
{
    if (isEmpty())
        return false;

    return overlayPage()->mainFrame()->eventHandler()->handleGestureEvent(event);
}

bool InspectorOverlay::handleMouseEvent(const PlatformMouseEvent& event)
{
    if (isEmpty())
        return false;

    EventHandler* eventHandler = overlayPage()->mainFrame()->eventHandler();
    bool result;
    switch (event.type()) {
    case PlatformEvent::MouseMoved:
        result = eventHandler->handleMouseMoveEvent(event);
        break;
    case PlatformEvent::MousePressed:
        result = eventHandler->handleMousePressEvent(event);
        break;
    case PlatformEvent::MouseReleased:
        result = eventHandler->handleMouseReleaseEvent(event);
        break;
    default:
        return false;
    }

    overlayPage()->mainFrame()->document()->updateLayout();
    return result;
}

bool InspectorOverlay::handleTouchEvent(const PlatformTouchEvent& event)
{
    if (isEmpty())
        return false;

    return overlayPage()->mainFrame()->eventHandler()->handleTouchEvent(event);
}

void InspectorOverlay::drawOutline(GraphicsContext* context, const LayoutRect& rect, const Color& color)
{
    FloatRect outlineRect = rect;
    drawOutlinedQuad(context, outlineRect, Color(), color);
}

void InspectorOverlay::getHighlight(Highlight* highlight) const
{
    if (!m_highlightNode && !m_highlightQuad)
        return;

    highlight->type = HighlightTypeRects;
    if (m_highlightNode)
        buildNodeHighlight(m_highlightNode.get(), m_nodeHighlightConfig, highlight);
    else
        buildQuadHighlight(m_page, *m_highlightQuad, m_quadHighlightConfig, highlight);
}

void InspectorOverlay::resize(const IntSize& size)
{
    m_size = size;
    update();
}

void InspectorOverlay::setPausedInDebuggerMessage(const String* message)
{
    m_pausedInDebuggerMessage = message ? *message : String();
    update();
}

void InspectorOverlay::setInspectModeEnabled(bool enabled)
{
    m_inspectModeEnabled = enabled;
    update();
}

void InspectorOverlay::setOverride(OverrideType type, bool enabled)
{
    bool currentEnabled = m_overrides & type;
    if (currentEnabled == enabled)
        return;
    if (enabled)
        m_overrides |= type;
    else
        m_overrides &= ~type;
    update();
}

void InspectorOverlay::setOverridesTopOffset(int offset)
{
    m_overridesTopOffset = offset;
    if (m_overrides)
        update();
}

void InspectorOverlay::hideHighlight()
{
    m_highlightNode.clear();
    m_eventTargetNode.clear();
    m_highlightQuad.clear();
    update();
}

void InspectorOverlay::highlightNode(Node* node, Node* eventTarget, const HighlightConfig& highlightConfig)
{
    m_nodeHighlightConfig = highlightConfig;
    m_highlightNode = node;
    m_eventTargetNode = eventTarget;
    update();
}

void InspectorOverlay::highlightQuad(PassOwnPtr<FloatQuad> quad, const HighlightConfig& highlightConfig)
{
    m_quadHighlightConfig = highlightConfig;
    m_highlightQuad = quad;
    update();
}

void InspectorOverlay::showAndHideViewSize(bool showGrid)
{
    m_drawViewSize = true;
    m_drawViewSizeWithGrid = showGrid;
    update();
    m_timer.startOneShot(1);
}

Node* InspectorOverlay::highlightedNode() const
{
    return m_highlightNode.get();
}

bool InspectorOverlay::isEmpty()
{
    bool hasAlwaysVisibleElements = m_highlightNode || m_eventTargetNode || m_highlightQuad || m_overrides || !m_size.isEmpty() || m_drawViewSize;
    bool hasInvisibleInInspectModeElements = !m_pausedInDebuggerMessage.isNull();
    return !(hasAlwaysVisibleElements || (hasInvisibleInInspectModeElements && !m_inspectModeEnabled));
}

void InspectorOverlay::update()
{
    if (isEmpty()) {
        m_client->hideHighlight();
        return;
    }

    FrameView* view = m_page->mainFrame()->view();
    if (!view)
        return;
    IntRect viewRect = view->visibleContentRect();
    FrameView* overlayView = overlayPage()->mainFrame()->view();
    IntSize frameViewFullSize = view->visibleContentRect(ScrollableArea::IncludeScrollbars).size();
    IntSize size = m_size.isEmpty() ? frameViewFullSize : m_size;
    size.scale(m_page->pageScaleFactor());
    overlayView->resize(size);

    // Clear canvas and paint things.
    reset(size, m_size.isEmpty() ? IntSize() : frameViewFullSize, viewRect.x(), viewRect.y());

    // Include scrollbars to avoid masking them by the gutter.
    drawGutter();
    drawNodeHighlight();
    drawQuadHighlight();
    if (!m_inspectModeEnabled)
        drawPausedInDebuggerMessage();
    drawViewSize();
    drawOverridesMessage();

    // Position DOM elements.
    overlayPage()->mainFrame()->document()->recalcStyle(Force);
    if (overlayView->needsLayout())
        overlayView->layout();

    // Kick paint.
    m_client->highlight();
}

void InspectorOverlay::hide()
{
    m_timer.stop();
    m_highlightNode.clear();
    m_eventTargetNode.clear();
    m_highlightQuad.clear();
    m_pausedInDebuggerMessage = String();
    m_size = IntSize();
    m_drawViewSize = false;
    m_drawViewSizeWithGrid = false;
    m_overrides = 0;
    m_overridesTopOffset = 0;
    update();
}

static PassRefPtr<JSONObject> buildObjectForPoint(const FloatPoint& point)
{
    RefPtr<JSONObject> object = JSONObject::create();
    object->setNumber("x", point.x());
    object->setNumber("y", point.y());
    return object.release();
}

static PassRefPtr<JSONArray> buildArrayForQuad(const FloatQuad& quad)
{
    RefPtr<JSONArray> array = JSONArray::create();
    array->pushObject(buildObjectForPoint(quad.p1()));
    array->pushObject(buildObjectForPoint(quad.p2()));
    array->pushObject(buildObjectForPoint(quad.p3()));
    array->pushObject(buildObjectForPoint(quad.p4()));
    return array.release();
}

static PassRefPtr<JSONObject> buildObjectForHighlight(const Highlight& highlight)
{
    RefPtr<JSONObject> object = JSONObject::create();
    RefPtr<JSONArray> array = JSONArray::create();
    for (size_t i = 0; i < highlight.quads.size(); ++i)
        array->pushArray(buildArrayForQuad(highlight.quads[i]));
    object->setArray("quads", array.release());
    object->setBoolean("showRulers", highlight.showRulers);
    object->setString("contentColor", highlight.contentColor.serialized());
    object->setString("contentOutlineColor", highlight.contentOutlineColor.serialized());
    object->setString("paddingColor", highlight.paddingColor.serialized());
    object->setString("borderColor", highlight.borderColor.serialized());
    object->setString("marginColor", highlight.marginColor.serialized());
    object->setString("eventTargetColor", highlight.eventTargetColor.serialized());
    return object.release();
}

static PassRefPtr<JSONObject> buildObjectForSize(const IntSize& size)
{
    RefPtr<JSONObject> result = JSONObject::create();
    result->setNumber("width", size.width());
    result->setNumber("height", size.height());
    return result.release();
}

void InspectorOverlay::drawGutter()
{
    evaluateInOverlay("drawGutter", "");
}

void InspectorOverlay::drawNodeHighlight()
{
    if (!m_highlightNode)
        return;

    Highlight highlight;
    buildNodeHighlight(m_highlightNode.get(), m_nodeHighlightConfig, &highlight);
    if (m_eventTargetNode) {
        Highlight eventTargetHighlight;
        buildNodeHighlight(m_eventTargetNode.get(), m_nodeHighlightConfig, &eventTargetHighlight);
        highlight.quads.append(eventTargetHighlight.quads[1]); // Add border from eventTargetNode to highlight.
    }
    RefPtr<JSONObject> highlightObject = buildObjectForHighlight(highlight);

    Node* node = m_highlightNode.get();
    if (node->isElementNode() && m_nodeHighlightConfig.showInfo && node->renderer() && node->document().frame()) {
        RefPtr<JSONObject> elementInfo = JSONObject::create();
        Element* element = toElement(node);
        bool isXHTML = element->document().isXHTMLDocument();
        elementInfo->setString("tagName", isXHTML ? element->nodeName() : element->nodeName().lower());
        elementInfo->setString("idValue", element->getIdAttribute());
        HashSet<AtomicString> usedClassNames;
        if (element->hasClass() && element->isStyledElement()) {
            StringBuilder classNames;
            const SpaceSplitString& classNamesString = element->classNames();
            size_t classNameCount = classNamesString.size();
            for (size_t i = 0; i < classNameCount; ++i) {
                const AtomicString& className = classNamesString[i];
                if (!usedClassNames.add(className).isNewEntry)
                    continue;
                classNames.append('.');
                classNames.append(className);
            }
            elementInfo->setString("className", classNames.toString());
        }

        RenderObject* renderer = node->renderer();
        Frame* containingFrame = node->document().frame();
        FrameView* containingView = containingFrame->view();
        IntRect boundingBox = pixelSnappedIntRect(containingView->contentsToRootView(renderer->absoluteBoundingBoxRect()));
        RenderBoxModelObject* modelObject = renderer->isBoxModelObject() ? toRenderBoxModelObject(renderer) : 0;
        elementInfo->setString("nodeWidth", String::number(modelObject ? adjustForAbsoluteZoom(modelObject->pixelSnappedOffsetWidth(), modelObject) : boundingBox.width()));
        elementInfo->setString("nodeHeight", String::number(modelObject ? adjustForAbsoluteZoom(modelObject->pixelSnappedOffsetHeight(), modelObject) : boundingBox.height()));
        highlightObject->setObject("elementInfo", elementInfo.release());
    }
    evaluateInOverlay("drawNodeHighlight", highlightObject);
}

void InspectorOverlay::drawQuadHighlight()
{
    if (!m_highlightQuad)
        return;

    Highlight highlight;
    buildQuadHighlight(m_page, *m_highlightQuad, m_quadHighlightConfig, &highlight);
    evaluateInOverlay("drawQuadHighlight", buildObjectForHighlight(highlight));
}

void InspectorOverlay::drawPausedInDebuggerMessage()
{
    if (!m_pausedInDebuggerMessage.isNull())
        evaluateInOverlay("drawPausedInDebuggerMessage", m_pausedInDebuggerMessage);
}

void InspectorOverlay::drawViewSize()
{
    if (m_drawViewSize)
        evaluateInOverlay("drawViewSize", m_drawViewSizeWithGrid ? "true" : "false");
}

void InspectorOverlay::drawOverridesMessage()
{
    RefPtr<JSONObject> data = JSONObject::create();
    if (!m_drawViewSize)
        data->setNumber("overrides", m_overrides);
    data->setNumber("topOffset", m_overridesTopOffset);
    evaluateInOverlay("drawOverridesMessage", data.release());
}

Page* InspectorOverlay::overlayPage()
{
    if (m_overlayPage)
        return m_overlayPage.get();

    static FrameLoaderClient* dummyFrameLoaderClient =  new EmptyFrameLoaderClient;
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    ASSERT(!m_overlayChromeClient);
    m_overlayChromeClient = adoptPtr(new InspectorOverlayChromeClient(m_page->chrome().client(), this));
    pageClients.chromeClient = m_overlayChromeClient.get();
    m_overlayPage = adoptPtr(new Page(pageClients));
    m_overlayPage->setGroupType(Page::InspectorPageGroup);

    Settings& settings = m_page->settings();
    Settings& overlaySettings = m_overlayPage->settings();

    overlaySettings.setStandardFontFamily(settings.standardFontFamily());
    overlaySettings.setSerifFontFamily(settings.serifFontFamily());
    overlaySettings.setSansSerifFontFamily(settings.sansSerifFontFamily());
    overlaySettings.setCursiveFontFamily(settings.cursiveFontFamily());
    overlaySettings.setFantasyFontFamily(settings.fantasyFontFamily());
    overlaySettings.setPictographFontFamily(settings.pictographFontFamily());
    overlaySettings.setMinimumFontSize(settings.minimumFontSize());
    overlaySettings.setMinimumLogicalFontSize(settings.minimumLogicalFontSize());
    overlaySettings.setMediaEnabled(false);
    overlaySettings.setScriptEnabled(true);
    overlaySettings.setPluginsEnabled(false);
    overlaySettings.setLoadsImagesAutomatically(true);

    RefPtr<Frame> frame = Frame::create(m_overlayPage.get(), 0, dummyFrameLoaderClient);
    frame->setView(FrameView::create(frame.get()));
    frame->init();
    FrameLoader* loader = frame->loader();
    frame->view()->setCanHaveScrollbars(false);
    frame->view()->setTransparent(true);
    ASSERT(loader->activeDocumentLoader());
    DocumentWriter* writer = loader->activeDocumentLoader()->beginWriting("text/html", "UTF-8");
    writer->addData(reinterpret_cast<const char*>(InspectorOverlayPage_html), sizeof(InspectorOverlayPage_html));
    loader->activeDocumentLoader()->endWriting(writer);
    v8::Isolate* isolate = isolateForFrame(frame.get());
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Context> frameContext = frame->script()->currentWorldContext();
    v8::Context::Scope contextScope(frameContext);
    v8::Handle<v8::Value> overlayHostObj = toV8(m_overlayHost.get(), v8::Handle<v8::Object>(), isolate);
    v8::Handle<v8::Object> global = frameContext->Global();
    global->Set(v8::String::New("InspectorOverlayHost"), overlayHostObj);

#if OS(WIN)
    evaluateInOverlay("setPlatform", "windows");
#elif OS(MACOSX)
    evaluateInOverlay("setPlatform", "mac");
#elif OS(POSIX)
    evaluateInOverlay("setPlatform", "linux");
#endif

    return m_overlayPage.get();
}

void InspectorOverlay::reset(const IntSize& viewportSize, const IntSize& frameViewFullSize, int scrollX, int scrollY)
{
    RefPtr<JSONObject> resetData = JSONObject::create();
    resetData->setNumber("pageScaleFactor", m_page->pageScaleFactor());
    resetData->setNumber("deviceScaleFactor", m_page->deviceScaleFactor());
    resetData->setObject("viewportSize", buildObjectForSize(viewportSize));
    resetData->setObject("frameViewFullSize", buildObjectForSize(frameViewFullSize));
    resetData->setNumber("pageZoomFactor", m_page->mainFrame()->pageZoomFactor());
    resetData->setNumber("scrollX", scrollX);
    resetData->setNumber("scrollY", scrollY);
    evaluateInOverlay("reset", resetData.release());
}

void InspectorOverlay::evaluateInOverlay(const String& method, const String& argument)
{
    RefPtr<JSONArray> command = JSONArray::create();
    command->pushString(method);
    command->pushString(argument);
    overlayPage()->mainFrame()->script()->executeScriptInMainWorld(ScriptSourceCode("dispatch(" + command->toJSONString() + ")"));
}

void InspectorOverlay::evaluateInOverlay(const String& method, PassRefPtr<JSONValue> argument)
{
    RefPtr<JSONArray> command = JSONArray::create();
    command->pushString(method);
    command->pushValue(argument);
    overlayPage()->mainFrame()->script()->executeScriptInMainWorld(ScriptSourceCode("dispatch(" + command->toJSONString() + ")"));
}

void InspectorOverlay::onTimer(Timer<InspectorOverlay>*)
{
    m_drawViewSize = false;
    update();
}

bool InspectorOverlay::getBoxModel(Node* node, Vector<FloatQuad>* quads)
{
    return buildNodeQuads(node, *quads);
}

void InspectorOverlay::freePage()
{
    m_overlayPage.clear();
    m_overlayChromeClient.clear();
    m_timer.stop();
}

} // namespace WebCore
