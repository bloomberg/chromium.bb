/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
 *
 */

#ifndef RenderObjectTypes_h
#define RenderObjectTypes_h

namespace WebCore {

enum RenderBaseObjectType {
    RenderNoneBaseObjectType                  = 0,
    RenderLayerModelBaseObjectType            = 1 <<  0,
    RenderBoxModelBaseObjectType              = 1 <<  1,
    RenderBoxBaseObjectType                   = 1 <<  2,
    RenderBlockBaseObjectType                 = 1 <<  3,
    RenderBlockFlowBaseObjectType             = 1 <<  4,
    RenderRegionBaseObjectType                = 1 <<  5,
    RenderRegionSetBaseObjectType             = 1 <<  6,
    RenderTextControlBaseObjectType           = 1 <<  7,
    RenderTextControlSingleLineBaseObjectType = 1 <<  8,
    RenderFlowThreadBaseObjectType            = 1 <<  9,
    RenderFlexibleBoxBaseObjectType           = 1 << 10,
    RenderReplacedBaseObjectType              = 1 << 11,
    RenderWidgetBaseObjectType                = 1 << 12,
    RenderPartBaseObjectType                  = 1 << 13,
    RenderEmbeddedBaseObjectType              = 1 << 14,
    RenderImageBaseObjectType                 = 1 << 15,
    // FIXME: There should not be two types signifying Image.
    RenderRenderImageBaseObjectType           = 1 << 16, // isImage and isRenderImage both exist and get overriden differently by RenderMedia.
    RenderMediaBaseObjectType                 = 1 << 17,
    RenderInlineBaseObjectType                = 1 << 18,
    RenderTextBaseObjectType                  = 1 << 19,

    RenderSVGBlockBaseObjectType              = 1 << 20,
    RenderSVGModelBaseObjectType              = 1 << 21,
    RenderSVGShapeBaseObjectType              = 1 << 22,
    RenderSVGContainerBaseObjectType          = 1 << 23,
    RenderSVGHiddenContainerBaseObjectType    = 1 << 24,
    RenderSVGResourceContainerBaseObjectType  = 1 << 25,
    RenderSVGResourceGradientBaseObjectType   = 1 << 26,
    RenderSVGInlineBaseObjectType             = 1 << 27
};

enum RenderObjectType {
    RenderNoneObjectType                      =  0,

    RenderAppletObjectType                    =  1,
    RenderBRObjectType                        =  2,
    RenderButtonObjectType                    =  3,
    RenderCanvasObjectType                    =  4,
    RenderCombineTextObjectType               =  5,
    RenderCounterObjectType                   =  6,
    RenderDeprecatedFlexibleBoxObjectType     =  7,
    RenderDetailsMarkerObjectType             =  8,
    RenderFieldsetObjectType                  =  9,
    RenderFileUploadControlObjectType         = 10,
    RenderFrameObjectType                     = 11,
    RenderFrameSetObjectType                  = 12,
    RenderFullScreenObjectType                = 13,
    RenderFullScreenPlaceholderObjectType     = 14,
    RenderGridObjectType                      = 15,
    RenderIFrameObjectType                    = 16,
    RenderListBoxObjectType                   = 17,
    RenderListItemObjectType                  = 18,
    RenderListMarkerObjectType                = 19,
    RenderMarqueeObjectType                   = 20,
    RenderMenuListObjectType                  = 21,
    RenderMeterObjectType                     = 22,
    RenderMultiColumnBlockObjectType          = 23,
    RenderMultiColumnFlowThreadObjectType     = 24,
    RenderMultiColumnSetObjectType            = 25,
    RenderNamedFlowThreadObjectType           = 26,
    RenderProgressObjectType                  = 27,
    RenderQuoteObjectType                     = 28,
    RenderReplicaObjectType                   = 29,
    RenderRubyBaseObjectType                  = 30,
    RenderRubyObjectType                      = 31,
    RenderRubyRunObjectType                   = 32,
    RenderRubyTextObjectType                  = 33,
    RenderScrollbarPartObjectType             = 34,
    RenderSearchFieldObjectType               = 35,
    RenderSliderContainerObjectType           = 36,
    RenderSliderObjectType                    = 37,
    RenderSliderThumbObjectType               = 38,
    RenderSVGEllipseObjectType                = 39,
    RenderSVGForeignObjectType                = 40,
    RenderSVGGradientStopObjectType           = 41,
    RenderSVGImageObjectType                  = 42,
    RenderSVGInlineTextObjectType             = 43,
    RenderSVGPathObjectType                   = 44,
    RenderSVGRectObjectType                   = 45,
    RenderSVGResourceClipperObjectType        = 46,
    RenderSVGResourceFilterObjectType         = 47,
    RenderSVGResourceFilterPrimitiveObjectType = 48,
    RenderSVGResourceLinearGradientObjectType = 49,
    RenderSVGResourceMarkerObjectType         = 50,
    RenderSVGResourceMaskerObjectType         = 51,
    RenderSVGResourcePatternObjectType        = 52,
    RenderSVGResourceRadialGradientObjectType = 53,
    RenderSVGRootObjectType                   = 54,
    RenderSVGTextObjectType                   = 55,
    RenderSVGTextPathObjectType               = 56,
    RenderSVGTSpanObjectType                  = 57,
    RenderSVGTransformableContainerObjectType = 58,
    RenderSVGViewportContainerObjectType      = 59,
    RenderTableCellObjectType                 = 60,
    RenderTableColObjectType                  = 61,
    RenderTableCaptionObjectType              = 62,
    RenderTableObjectType                     = 63,
    RenderTableRowObjectType                  = 64,
    RenderTableSectionObjectType              = 65,
    RenderTextAreaObjectType                  = 66,
    RenderTextControlInnerBlockObjectType     = 67,
    RenderTextControlInnerContainerObjectType = 68,
    RenderTextFragmentObjectType              = 69,
    RenderTextObjecType                       = 70,
    RenderTextTrackContainerElementObjectType = 71,
    RenderViewObjectType                      = 72,
    RenderVideoObjectType                     = 73,
    RenderWordBreakObjectType                 = 74
    // Max 127 values.
};

} // namespace WebCore

#endif // RenderObjectTypes_h
