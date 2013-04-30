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

#ifndef InspectorOverlay_h
#define InspectorOverlay_h

#include "core/platform/graphics/Color.h"
#include "core/platform/graphics/FloatQuad.h"
#include "core/platform/graphics/LayoutRect.h"
#include "core/platform/Timer.h"

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>
#include <wtf/Vector.h>

namespace WebCore {

class Color;
class GraphicsContext;
class InspectorClient;
class InspectorValue;
class IntRect;
class Node;
class Page;

struct HighlightConfig {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Color content;
    Color contentOutline;
    Color padding;
    Color border;
    Color margin;
    Color eventTarget;
    bool showInfo;
    bool showRulers;
};

enum HighlightType {
    HighlightTypeNode,
    HighlightTypeRects,
};

struct Highlight {
    Highlight()
        : type(HighlightTypeNode)
        , showRulers(false)
    {
    }

    void setDataFromConfig(const HighlightConfig& highlightConfig)
    {
        contentColor = highlightConfig.content;
        contentOutlineColor = highlightConfig.contentOutline;
        paddingColor = highlightConfig.padding;
        borderColor = highlightConfig.border;
        marginColor = highlightConfig.margin;
        eventTargetColor = highlightConfig.eventTarget;
        showRulers = highlightConfig.showRulers;
    }

    Color contentColor;
    Color contentOutlineColor;
    Color paddingColor;
    Color borderColor;
    Color marginColor;
    Color eventTargetColor;

    // When the type is Node, there are 4 or 5 quads (margin, border, padding, content, optional eventTarget).
    // When the type is Rects, this is just a list of quads.
    HighlightType type;
    Vector<FloatQuad> quads;
    bool showRulers;
};

class InspectorOverlay {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<InspectorOverlay> create(Page* page, InspectorClient* client)
    {
        return adoptPtr(new InspectorOverlay(page, client));
    }
    ~InspectorOverlay();

    void update();
    void hide();
    void paint(GraphicsContext&);
    void drawOutline(GraphicsContext*, const LayoutRect&, const Color&);
    void getHighlight(Highlight*) const;
    void resize(const IntSize&);

    void setPausedInDebuggerMessage(const String*);

    void hideHighlight();
    void highlightNode(Node*, Node* eventTarget, const HighlightConfig&);
    void highlightQuad(PassOwnPtr<FloatQuad>, const HighlightConfig&);
    void showAndHideViewSize();

    Node* highlightedNode() const;

    void reportMemoryUsage(MemoryObjectInfo*) const;

    void freePage();
private:
    InspectorOverlay(Page*, InspectorClient*);

    void drawGutter();
    void drawNodeHighlight();
    void drawQuadHighlight();
    void drawPausedInDebuggerMessage();
    void drawViewSize();

    Page* overlayPage();
    void reset(const IntSize& viewportSize, const IntSize& frameViewFullSize, int scrollX, int scrollY);
    void evaluateInOverlay(const String& method, const String& argument);
    void evaluateInOverlay(const String& method, PassRefPtr<InspectorValue> argument);
    void onTimer(Timer<InspectorOverlay>*);

    Page* m_page;
    InspectorClient* m_client;
    String m_pausedInDebuggerMessage;
    RefPtr<Node> m_highlightNode;
    RefPtr<Node> m_eventTargetNode;
    HighlightConfig m_nodeHighlightConfig;
    OwnPtr<FloatQuad> m_highlightQuad;
    OwnPtr<Page> m_overlayPage;
    HighlightConfig m_quadHighlightConfig;
    IntSize m_size;
    bool m_drawViewSize;
    Timer<InspectorOverlay> m_timer;
};

} // namespace WebCore


#endif // InspectorOverlay_h
