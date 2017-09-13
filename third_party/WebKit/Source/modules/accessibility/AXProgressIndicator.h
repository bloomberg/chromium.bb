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

#ifndef AXProgressIndicator_h
#define AXProgressIndicator_h

#include "modules/accessibility/AXLayoutObject.h"

namespace blink {

class AXObjectCacheImpl;
class HTMLProgressElement;
class LayoutProgress;

class AXProgressIndicator final : public AXLayoutObject {
  WTF_MAKE_NONCOPYABLE(AXProgressIndicator);

 public:
  static AXProgressIndicator* Create(LayoutProgress*, AXObjectCacheImpl&);

 private:
  AccessibilityRole DetermineAccessibilityRole() final;

  bool IsProgressIndicator() const override { return true; }

  bool ValueForRange(float* out_value) const override;
  bool MaxValueForRange(float* out_value) const override;
  bool MinValueForRange(float* out_value) const override;

  AXProgressIndicator(LayoutProgress*, AXObjectCacheImpl&);

  HTMLProgressElement* GetProgressElement() const;
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
};

}  // namespace blink

#endif  // AXProgressIndicator_h
