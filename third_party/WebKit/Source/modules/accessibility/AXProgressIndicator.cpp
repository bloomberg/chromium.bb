/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "modules/accessibility/AXProgressIndicator.h"

#include "core/html/HTMLProgressElement.h"
#include "core/layout/LayoutProgress.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "wtf/MathExtras.h"

namespace blink {

using namespace HTMLNames;

AXProgressIndicator::AXProgressIndicator(LayoutProgress* layout_object,
                                         AXObjectCacheImpl& ax_object_cache)
    : AXLayoutObject(layout_object, ax_object_cache) {}

AXProgressIndicator* AXProgressIndicator::Create(
    LayoutProgress* layout_object,
    AXObjectCacheImpl& ax_object_cache) {
  return new AXProgressIndicator(layout_object, ax_object_cache);
}

AccessibilityRole AXProgressIndicator::DetermineAccessibilityRole() {
  if ((aria_role_ = DetermineAriaRoleAttribute()) != kUnknownRole)
    return aria_role_;
  return kProgressIndicatorRole;
}

bool AXProgressIndicator::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  return AccessibilityIsIgnoredByDefault(ignored_reasons);
}

float AXProgressIndicator::ValueForRange() const {
  if (HasAttribute(aria_valuenowAttr))
    return GetAttribute(aria_valuenowAttr).ToFloat();

  if (GetElement()->position() >= 0)
    return clampTo<float>(GetElement()->value());
  // Indeterminate progress bar should return 0.
  return 0.0f;
}

float AXProgressIndicator::MaxValueForRange() const {
  if (HasAttribute(aria_valuemaxAttr))
    return GetAttribute(aria_valuemaxAttr).ToFloat();

  return clampTo<float>(GetElement()->max());
}

float AXProgressIndicator::MinValueForRange() const {
  if (HasAttribute(aria_valueminAttr))
    return GetAttribute(aria_valueminAttr).ToFloat();

  return 0.0f;
}

HTMLProgressElement* AXProgressIndicator::GetElement() const {
  return ToLayoutProgress(layout_object_)->ProgressElement();
}

}  // namespace blink
