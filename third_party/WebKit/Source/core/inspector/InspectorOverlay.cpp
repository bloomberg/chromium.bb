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

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8InspectorOverlayHost.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/PseudoElement.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorOverlayHost.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/Chrome.h"
#include "core/page/EventHandler.h"
#include "core/page/Page.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderBoxModelObject.h"
#include "core/rendering/RenderInline.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/shapes/ShapeOutsideInfo.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "platform/JSONValues.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "public/platform/Platform.h"
#include "public/platform/WebData.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"
#include <v8.h>

namespace blink {

namespace {

class PathBuilder {
    WTF_MAKE_NONCOPYABLE(PathBuilder);
public:
    PathBuilder() : m_path(TypeBuilder::Array<JSONValue>::create()) { }
    virtual ~PathBuilder() { }

    PassRefPtr<TypeBuilder::Array<JSONValue> > path() const { return m_path; }
    void appendPath(const Path& path)
    {
        path.apply(this, &PathBuilder::appendPathElement);
    }

protected:
    virtual FloatPoint translatePoint(const FloatPoint& point) { return point; }

private:
    static void appendPathElement(void* pathBuilder, const PathElement* pathElement)
    {
        static_cast<PathBuilder*>(pathBuilder)->appendPathElement(pathElement);
    }

    void appendPathElement(const PathElement*);
    void appendPathCommandAndPoints(const char* command, const FloatPoint points[], size_t length);

    RefPtr<TypeBuilder::Array<JSONValue> > m_path;
};

class ShapePathBuilder : public PathBuilder {
public:
    ShapePathBuilder(FrameView& view, RenderObject& renderer, const ShapeOutsideInfo& shapeOutsideInfo)
        : m_view(view)
        , m_renderer(renderer)
        , m_shapeOutsideInfo(shapeOutsideInfo) { }

    static PassRefPtr<TypeBuilder::Array<JSONValue> > buildPath(FrameView& view, RenderObject& renderer, const ShapeOutsideInfo& shapeOutsideInfo, const Path& path)
    {
        ShapePathBuilder builder(view, renderer, shapeOutsideInfo);
        builder.appendPath(path);
        return builder.path();
    }

protected:
    virtual FloatPoint translatePoint(const FloatPoint& point)
    {
        FloatPoint rendererPoint = m_shapeOutsideInfo.shapeToRendererPoint(point);
        return m_view.contentsToRootView(roundedIntPoint(m_renderer.localToAbsolute(rendererPoint)));
    }

private:
    FrameView& m_view;
    RenderObject& m_renderer;
    const ShapeOutsideInfo& m_shapeOutsideInfo;
};

class InspectorOverlayChromeClient FINAL: public EmptyChromeClient {
public:
    InspectorOverlayChromeClient(ChromeClient& client, InspectorOverlay* overlay)
        : m_client(client)
        , m_overlay(overlay)
    { }

    virtual void setCursor(const Cursor& cursor) OVERRIDE
    {
        m_client.setCursor(cursor);
    }

    virtual void setToolTip(const String& tooltip, TextDirection direction) OVERRIDE
    {
        m_client.setToolTip(tooltip, direction);
    }

    virtual void invalidateContentsAndRootView(const IntRect&) OVERRIDE
    {
        m_overlay->invalidate();
    }

    virtual void invalidateContentsForSlowScroll(const IntRect&) OVERRIDE
    {
        m_overlay->invalidate();
    }

private:
    ChromeClient& m_client;
    InspectorOverlay* m_overlay;
};

static Path quadToPath(const FloatQuad& quad)
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

class Highlight {
public:
    Highlight()
        : m_showRulers(false)
        , m_showExtensionLines(false)
        , m_highlightPaths(JSONArray::create())
    { }

    void setDataFromConfig(const HighlightConfig& highlightConfig)
    {
        m_showRulers = highlightConfig.showRulers;
        m_showExtensionLines = highlightConfig.showExtensionLines;
    }

    void setElementInfo(PassRefPtr<JSONObject> elementInfo)
    {
        m_elementInfo = elementInfo;
    }

    void appendQuad(const FloatQuad& quad, const Color& fillColor, const Color& outlineColor = Color::transparent)
    {
        Path path = quadToPath(quad);
        PathBuilder builder;
        builder.appendPath(path);
        appendPath(builder.path(), fillColor, outlineColor);
    }

    void appendPath(PassRefPtr<JSONArrayBase> path, const Color& fillColor, const Color& outlineColor)
    {
        RefPtr<JSONObject> object = JSONObject::create();
        object->setValue("path", path);
        object->setString("fillColor", fillColor.serialized());
        if (outlineColor != Color::transparent)
            object->setString("outlineColor", outlineColor.serialized());
        m_highlightPaths->pushObject(object.release());
    }

    PassRefPtr<JSONObject> asJSONObject() const
    {
        RefPtr<JSONObject> object = JSONObject::create();
        object->setArray("paths", m_highlightPaths);
        object->setBoolean("showRulers", m_showRulers);
        object->setBoolean("showExtensionLines", m_showExtensionLines);
        if (m_elementInfo)
            object->setObject("elementInfo", m_elementInfo);
        return object.release();
    }

private:
    bool m_showRulers;
    bool m_showExtensionLines;
    RefPtr<JSONObject> m_elementInfo;
    RefPtr<JSONArray> m_highlightPaths;
};

static void contentsQuadToScreen(const FrameView* view, FloatQuad& quad)
{
    quad.setP1(view->contentsToRootView(roundedIntPoint(quad.p1())));
    quad.setP2(view->contentsToRootView(roundedIntPoint(quad.p2())));
    quad.setP3(view->contentsToRootView(roundedIntPoint(quad.p3())));
    quad.setP4(view->contentsToRootView(roundedIntPoint(quad.p4())));
}

static bool buildNodeQuads(RenderObject* renderer, FloatQuad* content, FloatQuad* padding, FloatQuad* border, FloatQuad* margin)
{
    FrameView* containingView = renderer->frameView();
    if (!containingView)
        return false;
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

    *content = renderer->localToAbsoluteQuad(FloatRect(contentBox));
    *padding = renderer->localToAbsoluteQuad(FloatRect(paddingBox));
    *border = renderer->localToAbsoluteQuad(FloatRect(borderBox));
    *margin = renderer->localToAbsoluteQuad(FloatRect(marginBox));

    contentsQuadToScreen(containingView, *content);
    contentsQuadToScreen(containingView, *padding);
    contentsQuadToScreen(containingView, *border);
    contentsQuadToScreen(containingView, *margin);

    return true;
}

static void buildNodeHighlight(Node& node, const HighlightConfig& highlightConfig, Highlight* highlight)
{
    RenderObject* renderer = node.renderer();
    if (!renderer)
        return;

    highlight->setDataFromConfig(highlightConfig);

    // RenderSVGRoot should be highlighted through the isBox() code path, all other SVG elements should just dump their absoluteQuads().
    if (renderer->node() && renderer->node()->isSVGElement() && !renderer->isSVGRoot()) {
        Vector<FloatQuad> quads;
        renderer->absoluteQuads(quads);
        for (size_t i = 0; i < quads.size(); ++i)
            highlight->appendQuad(quads[i], highlightConfig.content, highlightConfig.contentOutline);
        return;
    }

    FloatQuad content, padding, border, margin;
    if (!buildNodeQuads(renderer, &content, &padding, &border, &margin))
        return;
    highlight->appendQuad(content, highlightConfig.content, highlightConfig.contentOutline);
    highlight->appendQuad(padding, highlightConfig.padding);
    highlight->appendQuad(border, highlightConfig.border);
    highlight->appendQuad(margin, highlightConfig.margin);
}

} // anonymous namespace

InspectorOverlay::InspectorOverlay(Page* page, InspectorClient* client)
    : m_page(page)
    , m_client(client)
    , m_inspectModeEnabled(false)
    , m_overlayHost(InspectorOverlayHost::create())
    , m_drawViewSize(false)
    , m_drawViewSizeWithGrid(false)
    , m_omitTooltip(false)
    , m_timer(this, &InspectorOverlay::onTimer)
    , m_activeProfilerCount(0)
{
}

InspectorOverlay::~InspectorOverlay()
{
    ASSERT(!m_overlayPage);
}

void InspectorOverlay::paint(GraphicsContext& context)
{
    if (isEmpty())
        return;
    GraphicsContextStateSaver stateSaver(context);
    FrameView* view = toLocalFrame(overlayPage()->mainFrame())->view();
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

    return toLocalFrame(overlayPage()->mainFrame())->eventHandler().handleGestureEvent(event);
}

bool InspectorOverlay::handleMouseEvent(const PlatformMouseEvent& event)
{
    if (isEmpty())
        return false;

    EventHandler& eventHandler = toLocalFrame(overlayPage()->mainFrame())->eventHandler();
    switch (event.type()) {
    case PlatformEvent::MouseMoved:
        return eventHandler.handleMouseMoveEvent(event);
    case PlatformEvent::MousePressed:
        return eventHandler.handleMousePressEvent(event);
    case PlatformEvent::MouseReleased:
        return eventHandler.handleMouseReleaseEvent(event);
    default:
        return false;
    }
}

bool InspectorOverlay::handleTouchEvent(const PlatformTouchEvent& event)
{
    if (isEmpty())
        return false;

    return toLocalFrame(overlayPage()->mainFrame())->eventHandler().handleTouchEvent(event);
}

bool InspectorOverlay::handleKeyboardEvent(const PlatformKeyboardEvent& event)
{
    if (isEmpty())
        return false;

    return toLocalFrame(overlayPage()->mainFrame())->eventHandler().keyEvent(event);
}

void InspectorOverlay::drawOutline(GraphicsContext* context, const LayoutRect& rect, const Color& color)
{
    FloatRect outlineRect = rect;
    drawOutlinedQuad(context, outlineRect, Color(), color);
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

void InspectorOverlay::hideHighlight()
{
    m_highlightNode.clear();
    m_eventTargetNode.clear();
    m_highlightQuad.clear();
    update();
}

void InspectorOverlay::highlightNode(Node* node, Node* eventTarget, const HighlightConfig& highlightConfig, bool omitTooltip)
{
    m_nodeHighlightConfig = highlightConfig;
    m_highlightNode = node;
    m_eventTargetNode = eventTarget;
    m_omitTooltip = omitTooltip;
    update();
}

void InspectorOverlay::highlightQuad(PassOwnPtr<FloatQuad> quad, const HighlightConfig& highlightConfig)
{
    m_quadHighlightConfig = highlightConfig;
    m_highlightQuad = quad;
    m_omitTooltip = false;
    update();
}

void InspectorOverlay::showAndHideViewSize(bool showGrid)
{
    m_drawViewSize = true;
    m_drawViewSizeWithGrid = showGrid;
    update();
    m_timer.startOneShot(1, FROM_HERE);
}

Node* InspectorOverlay::highlightedNode() const
{
    return m_highlightNode.get();
}

bool InspectorOverlay::isEmpty()
{
    if (m_activeProfilerCount)
        return true;
    bool hasAlwaysVisibleElements = m_highlightNode || m_eventTargetNode || m_highlightQuad  || m_drawViewSize;
    bool hasInvisibleInInspectModeElements = !m_pausedInDebuggerMessage.isNull();
    return !(hasAlwaysVisibleElements || (hasInvisibleInInspectModeElements && !m_inspectModeEnabled));
}

void InspectorOverlay::update()
{
    if (isEmpty()) {
        m_client->hideHighlight();
        return;
    }

    FrameView* view = m_page->deprecatedLocalMainFrame()->view();
    if (!view)
        return;

    // Include scrollbars to avoid masking them by the gutter.
    IntSize size = view->unscaledVisibleContentSize(IncludeScrollbars);
    toLocalFrame(overlayPage()->mainFrame())->view()->resize(size);

    // Clear canvas and paint things.
    IntRect viewRect = view->visibleContentRect();
    reset(size, viewRect.x(), viewRect.y());

    drawNodeHighlight();
    drawQuadHighlight();
    if (!m_inspectModeEnabled)
        drawPausedInDebuggerMessage();
    drawViewSize();

    toLocalFrame(overlayPage()->mainFrame())->view()->updateLayoutAndStyleForPainting();

    m_client->highlight();
}

void InspectorOverlay::hide()
{
    m_timer.stop();
    m_highlightNode.clear();
    m_eventTargetNode.clear();
    m_highlightQuad.clear();
    m_pausedInDebuggerMessage = String();
    m_drawViewSize = false;
    m_drawViewSizeWithGrid = false;
    update();
}

static PassRefPtr<JSONObject> buildObjectForSize(const IntSize& size)
{
    RefPtr<JSONObject> result = JSONObject::create();
    result->setNumber("width", size.width());
    result->setNumber("height", size.height());
    return result.release();
}

void PathBuilder::appendPathCommandAndPoints(const char* command, const FloatPoint points[], size_t length)
{
    m_path->addItem(JSONString::create(command));
    for (size_t i = 0; i < length; i++) {
        FloatPoint point = translatePoint(points[i]);
        m_path->addItem(JSONBasicValue::create(point.x()));
        m_path->addItem(JSONBasicValue::create(point.y()));
    }
}

void PathBuilder::appendPathElement(const PathElement* pathElement)
{
    switch (pathElement->type) {
    // The points member will contain 1 value.
    case PathElementMoveToPoint:
        appendPathCommandAndPoints("M", pathElement->points, 1);
        break;
    // The points member will contain 1 value.
    case PathElementAddLineToPoint:
        appendPathCommandAndPoints("L", pathElement->points, 1);
        break;
    // The points member will contain 3 values.
    case PathElementAddCurveToPoint:
        appendPathCommandAndPoints("C", pathElement->points, 3);
        break;
    // The points member will contain 2 values.
    case PathElementAddQuadCurveToPoint:
        appendPathCommandAndPoints("Q", pathElement->points, 2);
        break;
    // The points member will contain no values.
    case PathElementCloseSubpath:
        appendPathCommandAndPoints("Z", 0, 0);
        break;
    }
}

static RefPtr<TypeBuilder::Array<double> > buildArrayForQuad(const FloatQuad& quad)
{
    RefPtr<TypeBuilder::Array<double> > array = TypeBuilder::Array<double>::create();
    array->addItem(quad.p1().x());
    array->addItem(quad.p1().y());
    array->addItem(quad.p2().x());
    array->addItem(quad.p2().y());
    array->addItem(quad.p3().x());
    array->addItem(quad.p3().y());
    array->addItem(quad.p4().x());
    array->addItem(quad.p4().y());
    return array.release();
}

static const ShapeOutsideInfo* shapeOutsideInfoForNode(Node* node, Shape::DisplayPaths* paths, FloatQuad* bounds)
{
    RenderObject* renderer = node->renderer();
    if (!renderer || !renderer->isBox() || !toRenderBox(renderer)->shapeOutsideInfo())
        return 0;

    FrameView* containingView = node->document().view();
    RenderBox* renderBox = toRenderBox(renderer);
    const ShapeOutsideInfo* shapeOutsideInfo = renderBox->shapeOutsideInfo();

    shapeOutsideInfo->computedShape().buildDisplayPaths(*paths);

    LayoutRect shapeBounds = shapeOutsideInfo->computedShapePhysicalBoundingBox();
    *bounds = renderBox->localToAbsoluteQuad(FloatRect(shapeBounds));
    contentsQuadToScreen(containingView, *bounds);

    return shapeOutsideInfo;
}

static void appendPathsForShapeOutside(Highlight& highlight, const HighlightConfig& config, Node* node)
{
    Shape::DisplayPaths paths;
    FloatQuad boundsQuad;

    const ShapeOutsideInfo* shapeOutsideInfo = shapeOutsideInfoForNode(node, &paths, &boundsQuad);
    if (!shapeOutsideInfo)
        return;

    if (!paths.shape.length()) {
        highlight.appendQuad(boundsQuad, config.shape);
        return;
    }

    highlight.appendPath(ShapePathBuilder::buildPath(*node->document().view(), *node->renderer(), *shapeOutsideInfo, paths.shape), config.shape, Color::transparent);
    if (paths.marginShape.length())
        highlight.appendPath(ShapePathBuilder::buildPath(*node->document().view(), *node->renderer(), *shapeOutsideInfo, paths.marginShape), config.shapeMargin, Color::transparent);
}

PassRefPtr<JSONObject> buildElementInfo(Element* element)
{
    RefPtr<JSONObject> elementInfo = JSONObject::create();
    Element* realElement = element;
    PseudoElement* pseudoElement = 0;
    if (element->isPseudoElement()) {
        pseudoElement = toPseudoElement(element);
        realElement = element->parentOrShadowHostElement();
    }
    bool isXHTML = realElement->document().isXHTMLDocument();
    elementInfo->setString("tagName", isXHTML ? realElement->nodeName() : realElement->nodeName().lower());
    elementInfo->setString("idValue", realElement->getIdAttribute());
    StringBuilder classNames;
    if (realElement->hasClass() && realElement->isStyledElement()) {
        HashSet<AtomicString> usedClassNames;
        const SpaceSplitString& classNamesString = realElement->classNames();
        size_t classNameCount = classNamesString.size();
        for (size_t i = 0; i < classNameCount; ++i) {
            const AtomicString& className = classNamesString[i];
            if (!usedClassNames.add(className).isNewEntry)
                continue;
            classNames.append('.');
            classNames.append(className);
        }
    }
    if (pseudoElement) {
        if (pseudoElement->pseudoId() == BEFORE)
            classNames.append("::before");
        else if (pseudoElement->pseudoId() == AFTER)
            classNames.append("::after");
    }
    if (!classNames.isEmpty())
        elementInfo->setString("className", classNames.toString());

    RenderObject* renderer = element->renderer();
    FrameView* containingView = element->document().view();
    if (!renderer || !containingView)
        return elementInfo;

    IntRect boundingBox = pixelSnappedIntRect(containingView->contentsToRootView(renderer->absoluteBoundingBoxRect()));
    RenderBoxModelObject* modelObject = renderer->isBoxModelObject() ? toRenderBoxModelObject(renderer) : 0;
    elementInfo->setString("nodeWidth", String::number(modelObject ? adjustForAbsoluteZoom(modelObject->pixelSnappedOffsetWidth(), modelObject) : boundingBox.width()));
    elementInfo->setString("nodeHeight", String::number(modelObject ? adjustForAbsoluteZoom(modelObject->pixelSnappedOffsetHeight(), modelObject) : boundingBox.height()));

    return elementInfo;
}

void InspectorOverlay::drawNodeHighlight()
{
    if (!m_highlightNode)
        return;

    Highlight highlight;
    appendPathsForShapeOutside(highlight, m_nodeHighlightConfig, m_highlightNode.get());
    buildNodeHighlight(*m_highlightNode, m_nodeHighlightConfig, &highlight);

    if (m_eventTargetNode && m_eventTargetNode->renderer()) {
        FloatQuad border, unused;
        if (buildNodeQuads(m_eventTargetNode->renderer(), &unused, &unused, &border, &unused))
            highlight.appendQuad(border, m_nodeHighlightConfig.eventTarget);
    }

    if (m_highlightNode->isElementNode() && !m_omitTooltip && m_nodeHighlightConfig.showInfo && m_highlightNode->renderer() && m_highlightNode->document().frame())
        highlight.setElementInfo(buildElementInfo(toElement(m_highlightNode.get())));

    evaluateInOverlay("drawHighlight", highlight.asJSONObject());
}

void InspectorOverlay::drawQuadHighlight()
{
    if (!m_highlightQuad)
        return;

    Highlight highlight;
    highlight.appendQuad(*m_highlightQuad, m_quadHighlightConfig.content, m_quadHighlightConfig.contentOutline);
    evaluateInOverlay("drawHighlight", highlight.asJSONObject());
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

Page* InspectorOverlay::overlayPage()
{
    if (m_overlayPage)
        return m_overlayPage.get();

    ScriptForbiddenScope::AllowUserAgentScript allowScript;

    static FrameLoaderClient* dummyFrameLoaderClient =  new EmptyFrameLoaderClient;
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    ASSERT(!m_overlayChromeClient);
    m_overlayChromeClient = adoptPtr(new InspectorOverlayChromeClient(m_page->chrome().client(), this));
    pageClients.chromeClient = m_overlayChromeClient.get();
    m_overlayPage = adoptPtrWillBeNoop(new Page(pageClients));

    Settings& settings = m_page->settings();
    Settings& overlaySettings = m_overlayPage->settings();

    overlaySettings.genericFontFamilySettings().updateStandard(settings.genericFontFamilySettings().standard());
    overlaySettings.genericFontFamilySettings().updateSerif(settings.genericFontFamilySettings().serif());
    overlaySettings.genericFontFamilySettings().updateSansSerif(settings.genericFontFamilySettings().sansSerif());
    overlaySettings.genericFontFamilySettings().updateCursive(settings.genericFontFamilySettings().cursive());
    overlaySettings.genericFontFamilySettings().updateFantasy(settings.genericFontFamilySettings().fantasy());
    overlaySettings.genericFontFamilySettings().updatePictograph(settings.genericFontFamilySettings().pictograph());
    overlaySettings.setMinimumFontSize(settings.minimumFontSize());
    overlaySettings.setMinimumLogicalFontSize(settings.minimumLogicalFontSize());
    overlaySettings.setScriptEnabled(true);
    overlaySettings.setPluginsEnabled(false);
    overlaySettings.setLoadsImagesAutomatically(true);
    // FIXME: http://crbug.com/363843. Inspector should probably create its
    // own graphics layers and attach them to the tree rather than going
    // through some non-composited paint function.
    overlaySettings.setAcceleratedCompositingEnabled(false);

    RefPtr<LocalFrame> frame = LocalFrame::create(dummyFrameLoaderClient, &m_overlayPage->frameHost(), 0);
    frame->setView(FrameView::create(frame.get()));
    frame->init();
    FrameLoader& loader = frame->loader();
    frame->view()->setCanHaveScrollbars(false);
    frame->view()->setTransparent(true);

    const blink::WebData& overlayPageHTMLResource = blink::Platform::current()->loadResource("InspectorOverlayPage.html");
    RefPtr<SharedBuffer> data = SharedBuffer::create(overlayPageHTMLResource.data(), overlayPageHTMLResource.size());
    loader.load(FrameLoadRequest(0, blankURL(), SubstituteData(data, "text/html", "UTF-8", KURL(), ForceSynchronousLoad)));
    v8::Isolate* isolate = toIsolate(frame.get());
    ScriptState* scriptState = ScriptState::forMainWorld(frame.get());
    ASSERT(!scriptState->contextIsEmpty());
    ScriptState::Scope scope(scriptState);
    v8::Handle<v8::Object> global = scriptState->context()->Global();
    v8::Handle<v8::Value> overlayHostObj = toV8(m_overlayHost.get(), global, isolate);
    global->Set(v8::String::NewFromUtf8(isolate, "InspectorOverlayHost"), overlayHostObj);

#if OS(WIN)
    evaluateInOverlay("setPlatform", "windows");
#elif OS(MACOSX)
    evaluateInOverlay("setPlatform", "mac");
#elif OS(POSIX)
    evaluateInOverlay("setPlatform", "linux");
#endif

    return m_overlayPage.get();
}

void InspectorOverlay::reset(const IntSize& viewportSize, int scrollX, int scrollY)
{
    RefPtr<JSONObject> resetData = JSONObject::create();
    resetData->setNumber("pageScaleFactor", m_page->settings().pinchVirtualViewportEnabled() ? 1 : m_page->pageScaleFactor());
    resetData->setNumber("deviceScaleFactor", m_page->deviceScaleFactor());
    resetData->setObject("viewportSize", buildObjectForSize(viewportSize));
    resetData->setNumber("pageZoomFactor", m_page->deprecatedLocalMainFrame()->pageZoomFactor());
    resetData->setNumber("scrollX", scrollX);
    resetData->setNumber("scrollY", scrollY);
    evaluateInOverlay("reset", resetData.release());
}

void InspectorOverlay::evaluateInOverlay(const String& method, const String& argument)
{
    ScriptForbiddenScope::AllowUserAgentScript allowScript;
    RefPtr<JSONArray> command = JSONArray::create();
    command->pushString(method);
    command->pushString(argument);
    toLocalFrame(overlayPage()->mainFrame())->script().executeScriptInMainWorld("dispatch(" + command->toJSONString() + ")", ScriptController::ExecuteScriptWhenScriptsDisabled);
}

void InspectorOverlay::evaluateInOverlay(const String& method, PassRefPtr<JSONValue> argument)
{
    ScriptForbiddenScope::AllowUserAgentScript allowScript;
    RefPtr<JSONArray> command = JSONArray::create();
    command->pushString(method);
    command->pushValue(argument);
    toLocalFrame(overlayPage()->mainFrame())->script().executeScriptInMainWorld("dispatch(" + command->toJSONString() + ")", ScriptController::ExecuteScriptWhenScriptsDisabled);
}

void InspectorOverlay::onTimer(Timer<InspectorOverlay>*)
{
    m_drawViewSize = false;
    update();
}

bool InspectorOverlay::getBoxModel(Node* node, RefPtr<TypeBuilder::DOM::BoxModel>& model)
{
    RenderObject* renderer = node->renderer();
    FrameView* view = node->document().view();
    if (!renderer || !view)
        return false;

    FloatQuad content, padding, border, margin;
    if (!buildNodeQuads(node->renderer(), &content, &padding, &border, &margin))
        return false;

    IntRect boundingBox = pixelSnappedIntRect(view->contentsToRootView(renderer->absoluteBoundingBoxRect()));
    RenderBoxModelObject* modelObject = renderer->isBoxModelObject() ? toRenderBoxModelObject(renderer) : 0;

    model = TypeBuilder::DOM::BoxModel::create()
        .setContent(buildArrayForQuad(content))
        .setPadding(buildArrayForQuad(padding))
        .setBorder(buildArrayForQuad(border))
        .setMargin(buildArrayForQuad(margin))
        .setWidth(modelObject ? adjustForAbsoluteZoom(modelObject->pixelSnappedOffsetWidth(), modelObject) : boundingBox.width())
        .setHeight(modelObject ? adjustForAbsoluteZoom(modelObject->pixelSnappedOffsetHeight(), modelObject) : boundingBox.height());

    Shape::DisplayPaths paths;
    FloatQuad boundsQuad;
    if (const ShapeOutsideInfo* shapeOutsideInfo = shapeOutsideInfoForNode(node, &paths, &boundsQuad)) {
        RefPtr<TypeBuilder::DOM::ShapeOutsideInfo> shapeTypeBuilder = TypeBuilder::DOM::ShapeOutsideInfo::create()
            .setBounds(buildArrayForQuad(boundsQuad))
            .setShape(ShapePathBuilder::buildPath(*view, *renderer, *shapeOutsideInfo, paths.shape))
            .setMarginShape(ShapePathBuilder::buildPath(*view, *renderer, *shapeOutsideInfo, paths.marginShape));
        model->setShapeOutside(shapeTypeBuilder);
    }

    return true;
}

void InspectorOverlay::freePage()
{
    if (m_overlayPage) {
        m_overlayPage->willBeDestroyed();
        m_overlayPage.clear();
    }
    m_overlayChromeClient.clear();
    m_timer.stop();

    // This will clear internal structures and issue update to the client. Safe to call last.
    hideHighlight();
}

void InspectorOverlay::startedRecordingProfile()
{
    if (!m_activeProfilerCount++)
        freePage();
}

} // namespace blink
