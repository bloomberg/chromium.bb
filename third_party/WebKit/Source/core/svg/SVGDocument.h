/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGDocument_h
#define SVGDocument_h

#include "core/dom/XMLDocument.h"
#include "platform/geometry/FloatPoint.h"

namespace WebCore {

class SVGSVGElement;

class SVGDocument FINAL : public XMLDocument {
public:
    static PassRefPtr<SVGDocument> create(const DocumentInit& initializer = DocumentInit())
    {
        return adoptRef(new SVGDocument(initializer));
    }

    static SVGSVGElement* rootElement(const Document&);
    SVGSVGElement* rootElement() const;

    void dispatchZoomEvent(float prevScale, float newScale);
    void dispatchScrollEvent();

    bool zoomAndPanEnabled() const;

    void startPan(const FloatPoint& start);
    void updatePan(const FloatPoint& pos) const;

    virtual PassRefPtr<Document> cloneDocumentWithoutChildren() OVERRIDE;

private:
    explicit SVGDocument(const DocumentInit&);

    FloatPoint m_translate;
};

DEFINE_DOCUMENT_TYPE_CASTS(SVGDocument);

} // namespace WebCore

#endif // SVGDocument_h
