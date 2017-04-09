/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef BidiRunForLine_h
#define BidiRunForLine_h

#include "core/layout/api/LineLayoutInline.h"
#include "core/layout/line/TrailingObjects.h"
#include "platform/text/BidiResolver.h"

namespace blink {

TextDirection DeterminePlaintextDirectionality(LineLayoutItem root,
                                               LineLayoutItem current = nullptr,
                                               unsigned pos = 0);

void ConstructBidiRunsForLine(InlineBidiResolver&,
                              BidiRunList<BidiRun>&,
                              const InlineIterator& end_of_line,
                              VisualDirectionOverride,
                              bool previous_line_broke_cleanly,
                              bool is_new_uba_paragraph);

}  // namespace blink

#endif  // BidiRunForLine_h
