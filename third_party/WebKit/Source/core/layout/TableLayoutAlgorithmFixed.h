/*
 * Copyright (C) 2002 Lars Knoll (knoll@kde.org)
 *           (C) 2002 Dirk Mueller (mueller@kde.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License.
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

#ifndef TableLayoutAlgorithmFixed_h
#define TableLayoutAlgorithmFixed_h

#include "core/layout/TableLayoutAlgorithm.h"
#include "platform/Length.h"
#include "platform/wtf/Vector.h"

namespace blink {

class LayoutTable;

class TableLayoutAlgorithmFixed final : public TableLayoutAlgorithm {
 public:
  TableLayoutAlgorithmFixed(LayoutTable*);

  void ComputeIntrinsicLogicalWidths(LayoutUnit& min_width,
                                     LayoutUnit& max_width) override;
  void ApplyPreferredLogicalWidthQuirks(LayoutUnit& min_width,
                                        LayoutUnit& max_width) const override;
  void UpdateLayout() override;
  void WillChangeTableLayout() override;

 private:
  int CalcWidthArray();
  bool recorded_width_difference_;

  Vector<Length> width_;
};

}  // namespace blink

#endif  // TableLayoutAlgorithmFixed_h
