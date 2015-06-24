// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/LayoutEditor.h"

#include "core/dom/Node.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutObject.h"
#include "platform/JSONValues.h"


namespace blink {

namespace {

PassRefPtr<JSONObject> createAnchor(float x, float y, const String& propertyName, FloatPoint deltaVector)
{
    RefPtr<JSONObject> object = JSONObject::create();
    object->setNumber("x", x);
    object->setNumber("y", y);
    object->setString("propertyName", propertyName);

    RefPtr<JSONObject> deltaVectorJSON = JSONObject::create();
    deltaVectorJSON->setNumber("x", deltaVector.x());
    deltaVectorJSON->setNumber("y", deltaVector.y());
    object->setObject("deltaVector", deltaVectorJSON.release());

    return object.release();
}

void contentsQuadToViewport(const FrameView* view, FloatQuad& quad)
{
    quad.setP1(view->contentsToViewport(roundedIntPoint(quad.p1())));
    quad.setP2(view->contentsToViewport(roundedIntPoint(quad.p2())));
    quad.setP3(view->contentsToViewport(roundedIntPoint(quad.p3())));
    quad.setP4(view->contentsToViewport(roundedIntPoint(quad.p4())));
}

FloatPoint orthogonalVector(FloatPoint from, FloatPoint to, FloatPoint defaultVector)
{
    if (from == to)
        return defaultVector;

    return FloatPoint(to.y() - from.y(), from.x() - to.x());
}

} // namespace

LayoutEditor::LayoutEditor(Node* node)
    : m_node(node)
{
}

PassRefPtr<JSONObject> LayoutEditor::buildJSONInfo() const
{
    LayoutObject* layoutObject = m_node->layoutObject();

    if (!layoutObject)
        return nullptr;

    FrameView* containingView = layoutObject->frameView();
    if (!containingView)
        return nullptr;
    if (!layoutObject->isBox() && !layoutObject->isLayoutInline())
        return nullptr;

    LayoutRect paddingBox;

    if (layoutObject->isBox()) {
        LayoutBox* layoutBox = toLayoutBox(layoutObject);

        // LayoutBox returns the "pure" content area box, exclusive of the scrollbars (if present), which also count towards the content area in CSS.
        const int verticalScrollbarWidth = layoutBox->verticalScrollbarWidth();
        const int horizontalScrollbarHeight = layoutBox->horizontalScrollbarHeight();

        paddingBox = layoutBox->paddingBoxRect();
        paddingBox.setWidth(paddingBox.width() + verticalScrollbarWidth);
        paddingBox.setHeight(paddingBox.height() + horizontalScrollbarHeight);
    } else {
        LayoutInline* layoutInline = toLayoutInline(layoutObject);
        LayoutRect borderBox = LayoutRect(layoutInline->linesBoundingBox());
        paddingBox = LayoutRect(borderBox.x() + layoutInline->borderLeft(), borderBox.y() + layoutInline->borderTop(),
            borderBox.width() - layoutInline->borderLeft() - layoutInline->borderRight(), borderBox.height() - layoutInline->borderTop() - layoutInline->borderBottom());
    }

    FloatQuad padding = layoutObject->localToAbsoluteQuad(FloatRect(paddingBox));
    contentsQuadToViewport(containingView, padding);

    float xLeft = (padding.p1().x() + padding.p4().x()) / 2;
    float yLeft = (padding.p1().y() + padding.p4().y()) / 2;
    FloatPoint orthoLeft = orthogonalVector(padding.p4(), padding.p1(), FloatPoint(-1, 0));

    float xRight = (padding.p2().x() + padding.p3().x()) / 2;
    float yRight = (padding.p2().y() + padding.p3().y()) / 2;
    FloatPoint orthoRight = orthogonalVector(padding.p2(), padding.p3(), FloatPoint(1, 0));

    RefPtr<JSONObject> object = JSONObject::create();
    RefPtr<JSONArray> anchors = JSONArray::create();
    anchors->pushObject(createAnchor(xLeft, yLeft, "padding-left", orthoLeft));
    anchors->pushObject(createAnchor(xRight, yRight, "padding-right", orthoRight));
    object->setArray("anchors", anchors.release());

    return object.release();

}

} // namespace blink
