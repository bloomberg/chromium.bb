// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ListMarkerPainter.h"

#include "core/layout/LayoutListItem.h"
#include "core/layout/LayoutListMarker.h"
#include "core/layout/PaintInfo.h"
#include "core/layout/TextRunConstructor.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/GraphicsContextAnnotator.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "wtf/unicode/CharacterNames.h"

namespace blink {

void ListMarkerPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_layoutListMarker);

    if (paintInfo.phase != PaintPhaseForeground)
        return;

    if (m_layoutListMarker.style()->visibility() != VISIBLE)
        return;

    LayoutPoint boxOrigin(paintOffset + m_layoutListMarker.location());
    LayoutRect overflowRect(m_layoutListMarker.visualOverflowRect());
    overflowRect.moveBy(boxOrigin);

    IntRect pixelSnappedOverflowRect = pixelSnappedIntRect(overflowRect);
    if (!paintInfo.rect.intersects(pixelSnappedOverflowRect))
        return;

    LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutListMarker, paintInfo.phase, pixelSnappedOverflowRect);
    if (recorder.canUseCachedDrawing())
        return;

    LayoutRect box(boxOrigin, m_layoutListMarker.size());

    IntRect marker = m_layoutListMarker.getRelativeMarkerRect();
    marker.moveBy(roundedIntPoint(boxOrigin));

    GraphicsContext* context = paintInfo.context;

    if (m_layoutListMarker.isImage()) {
        context->drawImage(m_layoutListMarker.image()->image(&m_layoutListMarker, marker.size()).get(), marker);
        if (m_layoutListMarker.selectionState() != LayoutObject::SelectionNone) {
            LayoutRect selRect = m_layoutListMarker.localSelectionRect();
            selRect.moveBy(boxOrigin);
            context->fillRect(pixelSnappedIntRect(selRect), m_layoutListMarker.selectionBackgroundColor());
        }
        return;
    }

    if (m_layoutListMarker.selectionState() != LayoutObject::SelectionNone) {
        LayoutRect selRect = m_layoutListMarker.localSelectionRect();
        selRect.moveBy(boxOrigin);
        context->fillRect(pixelSnappedIntRect(selRect), m_layoutListMarker.selectionBackgroundColor());
    }

    const Color color(m_layoutListMarker.resolveColor(CSSPropertyColor));
    context->setStrokeColor(color);
    context->setStrokeStyle(SolidStroke);
    context->setStrokeThickness(1.0f);
    context->setFillColor(color);

    EListStyleType type = m_layoutListMarker.style()->listStyleType();
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
    if (m_layoutListMarker.text().isEmpty())
        return;

    const Font& font = m_layoutListMarker.style()->font();
    TextRun textRun = constructTextRun(&m_layoutListMarker, font, m_layoutListMarker.text(), m_layoutListMarker.styleRef());

    GraphicsContextStateSaver stateSaver(*context, false);
    if (!m_layoutListMarker.style()->isHorizontalWritingMode()) {
        marker.moveBy(roundedIntPoint(-boxOrigin));
        marker = marker.transposedRect();
        marker.moveBy(IntPoint(roundToInt(box.x()), roundToInt(box.y() - m_layoutListMarker.logicalHeight())));
        stateSaver.save();
        context->translate(marker.x(), marker.maxY());
        context->rotate(static_cast<float>(deg2rad(90.)));
        context->translate(-marker.x(), -marker.maxY());
    }

    TextRunPaintInfo textRunPaintInfo(textRun);
    textRunPaintInfo.bounds = marker;
    IntPoint textOrigin = IntPoint(marker.x(), marker.y() + m_layoutListMarker.style()->fontMetrics().ascent());

    if (type == Asterisks || type == Footnotes) {
        context->drawText(font, textRunPaintInfo, textOrigin);
    } else {
        // Text is not arbitrary. We can judge whether it's RTL from the first character,
        // and we only need to handle the direction RightToLeft for now.
        bool textNeedsReversing = WTF::Unicode::direction(m_layoutListMarker.text()[0]) == WTF::Unicode::RightToLeft;
        StringBuilder reversedText;
        if (textNeedsReversing) {
            unsigned length = m_layoutListMarker.text().length();
            reversedText.reserveCapacity(length);
            for (int i = length - 1; i >= 0; --i)
                reversedText.append(m_layoutListMarker.text()[i]);
            ASSERT(reversedText.length() == length);
            textRun.setText(reversedText.toString());
        }

        const UChar suffix = m_layoutListMarker.listMarkerSuffix(type, m_layoutListMarker.listItem()->value());
        UChar suffixStr[2] = {
            m_layoutListMarker.style()->isLeftToRightDirection() ? suffix : ' ',
            m_layoutListMarker.style()->isLeftToRightDirection() ? ' ' : suffix
        };
        TextRun suffixRun = constructTextRun(&m_layoutListMarker, font, suffixStr, 2, m_layoutListMarker.styleRef(), m_layoutListMarker.style()->direction());
        TextRunPaintInfo suffixRunInfo(suffixRun);
        suffixRunInfo.bounds = marker;

        if (m_layoutListMarker.style()->isLeftToRightDirection()) {
            context->drawText(font, textRunPaintInfo, textOrigin);
            context->drawText(font, suffixRunInfo, textOrigin + IntSize(font.width(textRun), 0));
        } else {
            context->drawText(font, suffixRunInfo, textOrigin);
            context->drawText(font, textRunPaintInfo, textOrigin + IntSize(font.width(suffixRun), 0));
        }
    }
}

} // namespace blink
