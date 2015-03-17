// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/InspectorHighlight.h"

#include "core/dom/ClientRect.h"
#include "core/dom/PseudoElement.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/shapes/ShapeOutsideInfo.h"
#include "core/layout/style/LayoutStyleConstants.h"
#include "platform/graphics/Path.h"

namespace blink {

namespace {

class PathBuilder {
    WTF_MAKE_NONCOPYABLE(PathBuilder);
public:
    PathBuilder() : m_path(TypeBuilder::Array<JSONValue>::create()) { }
    virtual ~PathBuilder() { }

    PassRefPtr<TypeBuilder::Array<JSONValue>> path() const { return m_path; }
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

    RefPtr<TypeBuilder::Array<JSONValue>> m_path;
};

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

class ShapePathBuilder : public PathBuilder {
public:
    ShapePathBuilder(FrameView& view, LayoutObject& renderer, const ShapeOutsideInfo& shapeOutsideInfo)
        : m_view(view)
        , m_renderer(renderer)
        , m_shapeOutsideInfo(shapeOutsideInfo) { }

    static PassRefPtr<TypeBuilder::Array<JSONValue>> buildPath(FrameView& view, LayoutObject& renderer, const ShapeOutsideInfo& shapeOutsideInfo, const Path& path)
    {
        ShapePathBuilder builder(view, renderer, shapeOutsideInfo);
        builder.appendPath(path);
        return builder.path();
    }

protected:
    virtual FloatPoint translatePoint(const FloatPoint& point)
    {
        FloatPoint rendererPoint = m_shapeOutsideInfo.shapeToRendererPoint(point);
        return m_view.contentsToRootFrame(roundedIntPoint(m_renderer.localToAbsolute(rendererPoint)));
    }

private:
    FrameView& m_view;
    LayoutObject& m_renderer;
    const ShapeOutsideInfo& m_shapeOutsideInfo;
};


PassRefPtr<TypeBuilder::Array<double>> buildArrayForQuad(const FloatQuad& quad)
{
    RefPtr<TypeBuilder::Array<double>> array = TypeBuilder::Array<double>::create();
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

void contentsQuadToRootFrame(const FrameView* view, FloatQuad& quad)
{
    quad.setP1(view->contentsToRootFrame(roundedIntPoint(quad.p1())));
    quad.setP2(view->contentsToRootFrame(roundedIntPoint(quad.p2())));
    quad.setP3(view->contentsToRootFrame(roundedIntPoint(quad.p3())));
    quad.setP4(view->contentsToRootFrame(roundedIntPoint(quad.p4())));
}

const ShapeOutsideInfo* shapeOutsideInfoForNode(Node* node, Shape::DisplayPaths* paths, FloatQuad* bounds)
{
    LayoutObject* renderer = node->layoutObject();
    if (!renderer || !renderer->isBox() || !toLayoutBox(renderer)->shapeOutsideInfo())
        return nullptr;

    FrameView* containingView = node->document().view();
    LayoutBox* layoutBox = toLayoutBox(renderer);
    const ShapeOutsideInfo* shapeOutsideInfo = layoutBox->shapeOutsideInfo();

    shapeOutsideInfo->computedShape().buildDisplayPaths(*paths);

    LayoutRect shapeBounds = shapeOutsideInfo->computedShapePhysicalBoundingBox();
    *bounds = layoutBox->localToAbsoluteQuad(FloatRect(shapeBounds));
    contentsQuadToRootFrame(containingView, *bounds);

    return shapeOutsideInfo;
}

bool buildNodeQuads(LayoutObject* renderer, FloatQuad* content, FloatQuad* padding, FloatQuad* border, FloatQuad* margin)
{
    FrameView* containingView = renderer->frameView();
    if (!containingView)
        return false;
    if (!renderer->isBox() && !renderer->isLayoutInline())
        return false;

    LayoutRect contentBox;
    LayoutRect paddingBox;
    LayoutRect borderBox;
    LayoutRect marginBox;

    if (renderer->isBox()) {
        LayoutBox* layoutBox = toLayoutBox(renderer);

        // LayoutBox returns the "pure" content area box, exclusive of the scrollbars (if present), which also count towards the content area in CSS.
        const int verticalScrollbarWidth = layoutBox->verticalScrollbarWidth();
        const int horizontalScrollbarHeight = layoutBox->horizontalScrollbarHeight();
        contentBox = layoutBox->contentBoxRect();
        contentBox.setWidth(contentBox.width() + verticalScrollbarWidth);
        contentBox.setHeight(contentBox.height() + horizontalScrollbarHeight);

        paddingBox = layoutBox->paddingBoxRect();
        paddingBox.setWidth(paddingBox.width() + verticalScrollbarWidth);
        paddingBox.setHeight(paddingBox.height() + horizontalScrollbarHeight);

        borderBox = layoutBox->borderBoxRect();

        marginBox = LayoutRect(borderBox.x() - layoutBox->marginLeft(), borderBox.y() - layoutBox->marginTop(),
            borderBox.width() + layoutBox->marginWidth(), borderBox.height() + layoutBox->marginHeight());
    } else {
        LayoutInline* layoutInline = toLayoutInline(renderer);

        // LayoutInline's bounding box includes paddings and borders, excludes margins.
        borderBox = LayoutRect(layoutInline->linesBoundingBox());
        paddingBox = LayoutRect(borderBox.x() + layoutInline->borderLeft(), borderBox.y() + layoutInline->borderTop(),
            borderBox.width() - layoutInline->borderLeft() - layoutInline->borderRight(), borderBox.height() - layoutInline->borderTop() - layoutInline->borderBottom());
        contentBox = LayoutRect(paddingBox.x() + layoutInline->paddingLeft(), paddingBox.y() + layoutInline->paddingTop(),
            paddingBox.width() - layoutInline->paddingLeft() - layoutInline->paddingRight(), paddingBox.height() - layoutInline->paddingTop() - layoutInline->paddingBottom());
        // Ignore marginTop and marginBottom for inlines.
        marginBox = LayoutRect(borderBox.x() - layoutInline->marginLeft(), borderBox.y(),
            borderBox.width() + layoutInline->marginWidth(), borderBox.height());
    }

    *content = renderer->localToAbsoluteQuad(FloatRect(contentBox));
    *padding = renderer->localToAbsoluteQuad(FloatRect(paddingBox));
    *border = renderer->localToAbsoluteQuad(FloatRect(borderBox));
    *margin = renderer->localToAbsoluteQuad(FloatRect(marginBox));

    contentsQuadToRootFrame(containingView, *content);
    contentsQuadToRootFrame(containingView, *padding);
    contentsQuadToRootFrame(containingView, *border);
    contentsQuadToRootFrame(containingView, *margin);

    return true;
}

void appendPathsForShapeOutside(InspectorHighlight* highlight, const InspectorHighlightConfig& config, Node* node)
{
    Shape::DisplayPaths paths;
    FloatQuad boundsQuad;

    const ShapeOutsideInfo* shapeOutsideInfo = shapeOutsideInfoForNode(node, &paths, &boundsQuad);
    if (!shapeOutsideInfo)
        return;

    if (!paths.shape.length()) {
        highlight->appendQuad(boundsQuad, config.shape);
        return;
    }

    highlight->appendPath(ShapePathBuilder::buildPath(*node->document().view(), *node->layoutObject(), *shapeOutsideInfo, paths.shape), config.shape, Color::transparent);
    if (paths.marginShape.length())
        highlight->appendPath(ShapePathBuilder::buildPath(*node->document().view(), *node->layoutObject(), *shapeOutsideInfo, paths.marginShape), config.shapeMargin, Color::transparent);
}

void appendNodeHighlight(InspectorHighlight* highlight, const InspectorHighlightConfig& highlightConfig, Node* node)
{
    LayoutObject* renderer = node->layoutObject();
    if (!renderer)
        return;

    // LayoutSVGRoot should be highlighted through the isBox() code path, all other SVG elements should just dump their absoluteQuads().
    if (renderer->node() && renderer->node()->isSVGElement() && !renderer->isSVGRoot()) {
        Vector<FloatQuad> quads;
        renderer->absoluteQuads(quads);
        FrameView* containingView = renderer->frameView();
        for (size_t i = 0; i < quads.size(); ++i) {
            if (containingView)
                contentsQuadToRootFrame(containingView, quads[i]);
            highlight->appendQuad(quads[i], highlightConfig.content, highlightConfig.contentOutline);
        }
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

PassRefPtr<JSONObject> buildElementInfo(Element* element)
{
    RefPtr<JSONObject> elementInfo = JSONObject::create();
    Element* realElement = element;
    PseudoElement* pseudoElement = nullptr;
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
            classNames.appendLiteral("::before");
        else if (pseudoElement->pseudoId() == AFTER)
            classNames.appendLiteral("::after");
    }
    if (!classNames.isEmpty())
        elementInfo->setString("className", classNames.toString());

    LayoutObject* renderer = element->layoutObject();
    FrameView* containingView = element->document().view();
    if (!renderer || !containingView)
        return elementInfo;

    // Render the getBoundingClientRect() data in the tooltip
    // to be consistent with the rulers (see http://crbug.com/262338).
    RefPtrWillBeRawPtr<ClientRect> boundingBox = element->getBoundingClientRect();
    elementInfo->setString("nodeWidth", String::number(boundingBox->width()));
    elementInfo->setString("nodeHeight", String::number(boundingBox->height()));

    return elementInfo;
}

} // namespace

InspectorHighlight::InspectorHighlight()
    : m_showRulers(false)
    , m_showExtensionLines()
    , m_highlightPaths(JSONArray::create())
{
}

InspectorHighlight::~InspectorHighlight()
{
}

// static
PassOwnPtrWillBeRawPtr<InspectorHighlight> InspectorHighlight::create(Node* node, const InspectorHighlightConfig& highlightConfig, bool appendElementInfo)
{
    InspectorHighlight* highlight = new InspectorHighlight();
    highlight->m_showRulers = highlightConfig.showRulers;
    highlight->m_showExtensionLines = highlightConfig.showExtensionLines;
    appendPathsForShapeOutside(highlight, highlightConfig, node);
    appendNodeHighlight(highlight, highlightConfig, node);
    if (appendElementInfo && node->isElementNode())
        highlight->m_elementInfo = buildElementInfo(toElement(node));
    return adoptPtrWillBeNoop(highlight);
}

void InspectorHighlight::appendQuad(const FloatQuad& quad, const Color& fillColor, const Color& outlineColor)
{
    Path path = quadToPath(quad);
    PathBuilder builder;
    builder.appendPath(path);
    appendPath(builder.path(), fillColor, outlineColor);
}

void InspectorHighlight::appendPath(PassRefPtr<JSONArrayBase> path, const Color& fillColor, const Color& outlineColor)
{
    RefPtr<JSONObject> object = JSONObject::create();
    object->setValue("path", path);
    object->setString("fillColor", fillColor.serialized());
    if (outlineColor != Color::transparent)
        object->setString("outlineColor", outlineColor.serialized());
    m_highlightPaths->pushObject(object.release());
}

void InspectorHighlight::appendEventTargetQuads(Node* eventTargetNode, const InspectorHighlightConfig& highlightConfig)
{
    if (eventTargetNode->layoutObject()) {
        FloatQuad border, unused;
        if (buildNodeQuads(eventTargetNode->layoutObject(), &unused, &unused, &border, &unused))
            appendQuad(border, highlightConfig.eventTarget);
    }
}

PassRefPtr<JSONObject> InspectorHighlight::asJSONObject() const
{
    RefPtr<JSONObject> object = JSONObject::create();
    object->setArray("paths", m_highlightPaths);
    object->setBoolean("showRulers", m_showRulers);
    object->setBoolean("showExtensionLines", m_showExtensionLines);
    if (m_elementInfo)
        object->setObject("elementInfo", m_elementInfo);
    return object.release();
}

// static
bool InspectorHighlight::getBoxModel(Node* node, RefPtr<TypeBuilder::DOM::BoxModel>& model)
{
    LayoutObject* renderer = node->layoutObject();
    FrameView* view = node->document().view();
    if (!renderer || !view)
        return false;

    FloatQuad content, padding, border, margin;
    if (!buildNodeQuads(node->layoutObject(), &content, &padding, &border, &margin))
        return false;

    IntRect boundingBox = view->contentsToRootFrame(renderer->absoluteBoundingBoxRect());
    LayoutBoxModelObject* modelObject = renderer->isBoxModelObject() ? toLayoutBoxModelObject(renderer) : nullptr;

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

// static
InspectorHighlightConfig InspectorHighlight::defaultConfig()
{
    InspectorHighlightConfig config;
    config.content = Color(255, 0, 0, 0);
    config.contentOutline = Color(128, 0, 0, 0);
    config.padding = Color(0, 255, 0, 0);
    config.border = Color(0, 0, 255, 0);
    config.margin = Color(255, 255, 255, 0);
    config.eventTarget = Color(128, 128, 128, 0);
    config.shape = Color(0, 0, 0, 0);
    config.shapeMargin = Color(128, 128, 128, 0);
    config.showInfo = true;
    config.showRulers = true;
    config.showExtensionLines = true;
    return config;
}

} // namespace blink
