/*
 * Copyright (C) 2006, 2007, 2009, 2012 Apple Inc. All rights reserved.
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

#ifndef LayoutFileUploadControl_h
#define LayoutFileUploadControl_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBlockFlow.h"

namespace blink {

class HTMLInputElement;

// Each LayoutFileUploadControl contains a LayoutButton (for opening the file
// chooser), and sufficient space to draw a file icon and filename. The
// LayoutButton has a shadow node associated with it to receive click/hover
// events.

class CORE_EXPORT LayoutFileUploadControl final : public LayoutBlockFlow {
 public:
  LayoutFileUploadControl(HTMLInputElement*);
  ~LayoutFileUploadControl() override;

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectFileUploadControl ||
           LayoutBlockFlow::IsOfType(type);
  }

  String ButtonValue();
  String FileTextValue() const;

  HTMLInputElement* UploadButton() const;
  int UploadButtonWidth();

  static const int kAfterButtonSpacing = 4;

  const char* GetName() const override { return "LayoutFileUploadControl"; }

 private:
  void UpdateFromElement() override;
  void ComputeIntrinsicLogicalWidths(
      LayoutUnit& min_logical_width,
      LayoutUnit& max_logical_width) const override;
  void ComputePreferredLogicalWidths() override;
  void PaintObject(const PaintInfo&, const LayoutPoint&) const override;

  int MaxFilenameWidth() const;

  PositionWithAffinity PositionForPoint(const LayoutPoint&) override;

  bool can_receive_dropped_files_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutFileUploadControl, IsFileUploadControl());

}  // namespace blink

#endif  // LayoutFileUploadControl_h
