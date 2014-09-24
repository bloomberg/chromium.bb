// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ListMarkerPainter.h"

#include "core/paint/BlockPainter.h"
#include "core/rendering/GraphicsContextAnnotator.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderListItem.h"
#include "core/rendering/RenderListMarker.h"
#include "core/rendering/TextRunConstructor.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "wtf/unicode/CharacterNames.h"

namespace blink {

void ListMarkerPainter::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderListMarker);

    if (paintInfo.phase != PaintPhaseForeground)
        return;

    if (m_renderListMarker.style()->visibility() != VISIBLE)
        return;

    LayoutPoint boxOrigin(paintOffset + m_renderListMarker.location());
    LayoutRect overflowRect(m_renderListMarker.visualOverflowRect());
    overflowRect.moveBy(boxOrigin);

    if (!paintInfo.rect.intersects(pixelSnappedIntRect(overflowRect)))
        return;

    LayoutRect box(boxOrigin, m_renderListMarker.size());

    IntRect marker = m_renderListMarker.getRelativeMarkerRect();
    marker.moveBy(roundedIntPoint(boxOrigin));

    GraphicsContext* context = paintInfo.context;

    if (m_renderListMarker.isImage()) {
        context->drawImage(m_renderListMarker.image()->image(&m_renderListMarker, marker.size()).get(), marker);
        if (m_renderListMarker.selectionState() != RenderObject::SelectionNone) {
            LayoutRect selRect = m_renderListMarker.localSelectionRect();
            selRect.moveBy(boxOrigin);
            context->fillRect(pixelSnappedIntRect(selRect), m_renderListMarker.selectionBackgroundColor());
        }
        return;
    }

    if (m_renderListMarker.selectionState() != RenderObject::SelectionNone) {
        LayoutRect selRect = m_renderListMarker.localSelectionRect();
        selRect.moveBy(boxOrigin);
        context->fillRect(pixelSnappedIntRect(selRect), m_renderListMarker.selectionBackgroundColor());
    }

    const Color color(m_renderListMarker.resolveColor(CSSPropertyColor));
    context->setStrokeColor(color);
    context->setStrokeStyle(SolidStroke);
    context->setStrokeThickness(1.0f);
    context->setFillColor(color);

    EListStyleType type = m_renderListMarker.style()->listStyleType();
    switch (type) {
    case Disc:
        context->fillEllipse(marker);
        return;
    case Circle:
        context->strokeEllipse(marker);
        return;
    case Square:
        context->fillRect(marker);
        return;
    case NoneListStyle:
        return;
    case Afar:
    case Amharic:
    case AmharicAbegede:
    case ArabicIndic:
    case Armenian:
    case BinaryListStyle:
    case Bengali:
    case Cambodian:
    case CJKIdeographic:
    case CjkEarthlyBranch:
    case CjkHeavenlyStem:
    case DecimalLeadingZero:
    case DecimalListStyle:
    case Devanagari:
    case Ethiopic:
    case EthiopicAbegede:
    case EthiopicAbegedeAmEt:
    case EthiopicAbegedeGez:
    case EthiopicAbegedeTiEr:
    case EthiopicAbegedeTiEt:
    case EthiopicHalehameAaEr:
    case EthiopicHalehameAaEt:
    case EthiopicHalehameAmEt:
    case EthiopicHalehameGez:
    case EthiopicHalehameOmEt:
    case EthiopicHalehameSidEt:
    case EthiopicHalehameSoEt:
    case EthiopicHalehameTiEr:
    case EthiopicHalehameTiEt:
    case EthiopicHalehameTig:
    case Georgian:
    case Gujarati:
    case Gurmukhi:
    case Hangul:
    case HangulConsonant:
    case Hebrew:
    case Hiragana:
    case HiraganaIroha:
    case Kannada:
    case Katakana:
    case KatakanaIroha:
    case Khmer:
    case Lao:
    case LowerAlpha:
    case LowerArmenian:
    case LowerGreek:
    case LowerHexadecimal:
    case LowerLatin:
    case LowerNorwegian:
    case LowerRoman:
    case Malayalam:
    case Mongolian:
    case Myanmar:
    case Octal:
    case Oriya:
    case Oromo:
    case Persian:
    case Sidama:
    case Somali:
    case Telugu:
    case Thai:
    case Tibetan:
    case Tigre:
    case TigrinyaEr:
    case TigrinyaErAbegede:
    case TigrinyaEt:
    case TigrinyaEtAbegede:
    case UpperAlpha:
    case UpperArmenian:
    case UpperGreek:
    case UpperHexadecimal:
    case UpperLatin:
    case UpperNorwegian:
    case UpperRoman:
    case Urdu:
    case Asterisks:
    case Footnotes:
        break;
    }
    if (m_renderListMarker.text().isEmpty())
        return;

    const Font& font = m_renderListMarker.style()->font();
    TextRun textRun = constructTextRun(&m_renderListMarker, font, m_renderListMarker.text(), m_renderListMarker.style());

    GraphicsContextStateSaver stateSaver(*context, false);
    if (!m_renderListMarker.style()->isHorizontalWritingMode()) {
        marker.moveBy(roundedIntPoint(-boxOrigin));
        marker = marker.transposedRect();
        marker.moveBy(IntPoint(roundToInt(box.x()), roundToInt(box.y() - m_renderListMarker.logicalHeight())));
        stateSaver.save();
        context->translate(marker.x(), marker.maxY());
        context->rotate(static_cast<float>(deg2rad(90.)));
        context->translate(-marker.x(), -marker.maxY());
    }

    TextRunPaintInfo textRunPaintInfo(textRun);
    textRunPaintInfo.bounds = marker;
    IntPoint textOrigin = IntPoint(marker.x(), marker.y() + m_renderListMarker.style()->fontMetrics().ascent());

    if (type == Asterisks || type == Footnotes) {
        context->drawText(font, textRunPaintInfo, textOrigin);
    } else {
        // Text is not arbitrary. We can judge whether it's RTL from the first character,
        // and we only need to handle the direction RightToLeft for now.
        bool textNeedsReversing = WTF::Unicode::direction(m_renderListMarker.text()[0]) == WTF::Unicode::RightToLeft;
        StringBuilder reversedText;
        if (textNeedsReversing) {
            int length = m_renderListMarker.text().length();
            reversedText.reserveCapacity(length);
            for (int i = length - 1; i >= 0; --i)
                reversedText.append(m_renderListMarker.text()[i]);
            ASSERT(reversedText.length() == reversedText.capacity());
            textRun.setText(reversedText.toString());
        }

        const UChar suffix = m_renderListMarker.listMarkerSuffix(type, m_renderListMarker.listItem()->value());
        UChar suffixStr[2] = {
            m_renderListMarker.style()->isLeftToRightDirection() ? suffix : ' ',
            m_renderListMarker.style()->isLeftToRightDirection() ? ' ' : suffix
        };
        TextRun suffixRun = constructTextRun(&m_renderListMarker, font, suffixStr, 2, m_renderListMarker.style(), m_renderListMarker.style()->direction());
        TextRunPaintInfo suffixRunInfo(suffixRun);
        suffixRunInfo.bounds = marker;

        if (m_renderListMarker.style()->isLeftToRightDirection()) {
            context->drawText(font, textRunPaintInfo, textOrigin);
            context->drawText(font, suffixRunInfo, textOrigin + IntSize(font.width(textRun), 0));
        } else {
            context->drawText(font, suffixRunInfo, textOrigin);
            context->drawText(font, textRunPaintInfo, textOrigin + IntSize(font.width(suffixRun), 0));
        }
    }
}

} // namespace blink
