/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc.
 *               All rights reserved.
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

#include "core/layout/LayoutEmbeddedObject.h"

#include "core/CSSValueKeywords.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html_names.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutView.h"
#include "core/page/Page.h"
#include "core/paint/EmbeddedObjectPaintInvalidator.h"
#include "core/paint/EmbeddedObjectPainter.h"
#include "core/plugins/PluginView.h"
#include "platform/text/PlatformLocale.h"

namespace blink {

using namespace HTMLNames;

LayoutEmbeddedObject::LayoutEmbeddedObject(Element* element)
    : LayoutEmbeddedContent(element) {
  View()->GetFrameView()->SetIsVisuallyNonEmpty();
}

LayoutEmbeddedObject::~LayoutEmbeddedObject() {}

PaintLayerType LayoutEmbeddedObject::LayerTypeRequired() const {
  // This can't just use LayoutEmbeddedContent::layerTypeRequired, because
  // PaintLayerCompositor doesn't loop through LayoutEmbeddedObjects the way it
  // does frames in order to update the self painting bit on their Layer.
  // Also, unlike iframes, embeds don't used the usesCompositing bit on
  // LayoutView in requiresAcceleratedCompositing.
  if (RequiresAcceleratedCompositing())
    return kNormalPaintLayer;
  return LayoutEmbeddedContent::LayerTypeRequired();
}

static String LocalizedUnavailablePluginReplacementText(
    Node* node,
    LayoutEmbeddedObject::PluginAvailability availability) {
  Locale& locale =
      node ? ToElement(node)->GetLocale() : Locale::DefaultLocale();
  switch (availability) {
    case LayoutEmbeddedObject::kPluginAvailable:
      break;
    case LayoutEmbeddedObject::kPluginMissing:
      return locale.QueryString(WebLocalizedString::kMissingPluginText);
    case LayoutEmbeddedObject::kPluginBlockedByContentSecurityPolicy:
      return locale.QueryString(WebLocalizedString::kBlockedPluginText);
  }
  NOTREACHED();
  return String();
}

void LayoutEmbeddedObject::SetPluginAvailability(
    PluginAvailability availability) {
  DCHECK_EQ(kPluginAvailable, plugin_availability_);
  plugin_availability_ = availability;

  unavailable_plugin_replacement_text_ =
      LocalizedUnavailablePluginReplacementText(GetNode(), availability);

  // node() is nullptr when LayoutEmbeddedContent is being destroyed.
  if (GetNode())
    SetShouldDoFullPaintInvalidation();
}

bool LayoutEmbeddedObject::ShowsUnavailablePluginIndicator() const {
  return plugin_availability_ != kPluginAvailable;
}

void LayoutEmbeddedObject::PaintContents(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) const {
  Element* element = ToElement(GetNode());
  if (!IsHTMLPlugInElement(element))
    return;

  LayoutEmbeddedContent::PaintContents(paint_info, paint_offset);
}

void LayoutEmbeddedObject::Paint(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) const {
  if (ShowsUnavailablePluginIndicator()) {
    LayoutReplaced::Paint(paint_info, paint_offset);
    return;
  }

  LayoutEmbeddedContent::Paint(paint_info, paint_offset);
}

void LayoutEmbeddedObject::PaintReplaced(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) const {
  EmbeddedObjectPainter(*this).PaintReplaced(paint_info, paint_offset);
}

PaintInvalidationReason LayoutEmbeddedObject::InvalidatePaint(
    const PaintInvalidatorContext& context) const {
  return EmbeddedObjectPaintInvalidator(*this, context).InvalidatePaint();
}

void LayoutEmbeddedObject::UpdateLayout() {
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  UpdateLogicalWidth();
  UpdateLogicalHeight();

  overflow_.reset();
  AddVisualEffectOverflow();

  UpdateAfterLayout();

  if (!GetEmbeddedContentView() && GetFrameView())
    GetFrameView()->AddPartToUpdate(*this);

  ClearNeedsLayout();
}

ScrollResult LayoutEmbeddedObject::Scroll(ScrollGranularity granularity,
                                          const FloatSize&) {
  return ScrollResult();
}

CompositingReasons LayoutEmbeddedObject::AdditionalCompositingReasons() const {
  if (RequiresAcceleratedCompositing())
    return kCompositingReasonPlugin;
  return kCompositingReasonNone;
}

LayoutReplaced* LayoutEmbeddedObject::EmbeddedReplacedContent() const {
  if (LocalFrameView* frame_view = ChildFrameView())
    return frame_view->EmbeddedReplacedContent();
  return nullptr;
}

}  // namespace blink
