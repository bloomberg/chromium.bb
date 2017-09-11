/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
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

#ifndef FilterOperationResolver_h
#define FilterOperationResolver_h

#include "core/CSSValueKeywords.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/style/FilterOperations.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSValue;
class StyleResolverState;

class CORE_EXPORT FilterOperationResolver {
  STATIC_ONLY(FilterOperationResolver);

 public:
  static FilterOperation::OperationType FilterOperationForType(CSSValueID);
  static FilterOperations CreateFilterOperations(StyleResolverState&,
                                                 const CSSValue&);
  static FilterOperations CreateOffscreenFilterOperations(const CSSValue&);
};

}  // namespace blink

#endif  // FilterOperationResolver_h
