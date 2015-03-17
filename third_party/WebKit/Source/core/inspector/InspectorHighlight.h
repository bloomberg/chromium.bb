// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorHighlight_h
#define InspectorHighlight_h

#include "core/InspectorTypeBuilder.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/Color.h"
#include "platform/heap/Handle.h"

namespace blink {

class Color;
class JSONValue;

struct InspectorHighlightConfig {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Color content;
    Color contentOutline;
    Color padding;
    Color border;
    Color margin;
    Color eventTarget;
    Color shape;
    Color shapeMargin;

    bool showInfo;
    bool showRulers;
    bool showExtensionLines;
};

class InspectorHighlight : public NoBaseWillBeGarbageCollectedFinalized<InspectorHighlight> {
public:
    ~InspectorHighlight();

    static PassOwnPtrWillBeRawPtr<InspectorHighlight> create(Node*, const InspectorHighlightConfig&, bool appendElementInfo);
    static PassOwnPtrWillBeRawPtr<InspectorHighlight> create()
    {
        return adoptPtrWillBeNoop(new InspectorHighlight());
    }

    static bool getBoxModel(Node*, RefPtr<TypeBuilder::DOM::BoxModel>&);
    static InspectorHighlightConfig defaultConfig();

    void appendPath(PassRefPtr<JSONArrayBase> path, const Color& fillColor, const Color& outlineColor);
    void appendQuad(const FloatQuad&, const Color& fillColor, const Color& outlineColor = Color::transparent);
    void appendEventTargetQuads(Node* eventTargetNode, const InspectorHighlightConfig&);
    PassRefPtr<JSONObject> asJSONObject() const;

    DEFINE_INLINE_TRACE() { }

private:
    InspectorHighlight();

    bool m_showRulers;
    bool m_showExtensionLines;
    RefPtr<JSONObject> m_elementInfo;
    RefPtr<JSONArray> m_highlightPaths;
};

} // namespace blink


#endif // InspectorHighlight_h
