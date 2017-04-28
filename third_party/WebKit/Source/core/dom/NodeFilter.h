/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef NodeFilter_h
#define NodeFilter_h

#include "platform/heap/Handle.h"

namespace blink {

// We never create NodeFilter instances.
// The IDL interface 'NodeFilter' is represented by V8NodeFilterCondition and a
// V8 value in Blink.
class NodeFilter final {
  STATIC_ONLY(NodeFilter);

 public:
  /**
     * The following constants are returned by the acceptNode()
     * method:
     */
  enum { kFilterAccept = 1, kFilterReject = 2, kFilterSkip = 3 };

  /**
     * These are the available values for the whatToShow parameter.
     * They are the same as the set of possible types for Node, and
     * their values are derived by using a bit position corresponding
     * to the value of NodeType for the equivalent node type.
     */
  enum {
    kShowAll = 0xFFFFFFFF,
    kShowElement = 0x00000001,
    kShowAttribute = 0x00000002,
    kShowText = 0x00000004,
    kShowCdataSection = 0x00000008,
    kShowEntityReference = 0x00000010,
    kShowEntity = 0x00000020,
    kShowProcessingInstruction = 0x00000040,
    kShowComment = 0x00000080,
    kShowDocument = 0x00000100,
    kShowDocumentType = 0x00000200,
    kShowDocumentFragment = 0x00000400,
    kShowNotation = 0x00000800
  };
};

}  // namespace blink

#endif  // NodeFilter_h
