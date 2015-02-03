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

#ifndef LayoutTableAlgorithmFixed_h
#define LayoutTableAlgorithmFixed_h

#include "core/layout/LayoutTableAlgorithm.h"
#include "platform/Length.h"
#include "wtf/Vector.h"

namespace blink {

class LayoutTable;

class LayoutTableAlgorithmFixed final : public LayoutTableAlgorithm {
public:
    LayoutTableAlgorithmFixed(LayoutTable*);

    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minWidth, LayoutUnit& maxWidth) override;
    virtual void applyPreferredLogicalWidthQuirks(LayoutUnit& minWidth, LayoutUnit& maxWidth) const override;
    virtual void layout() override;
    virtual void willChangeTableLayout() override;

private:
    int calcWidthArray();

    Vector<Length> m_width;
};

} // namespace blink

#endif // LayoutTableAlgorithmFixed_h
