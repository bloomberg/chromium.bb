/*
 *  Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc.
 *  All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_RefPtr_h
#define WTF_RefPtr_h

#include "base/memory/scoped_refptr.h"

namespace WTF {

template <typename T>
using RefPtr = scoped_refptr<T>;

using base::AdoptRef;

template <typename T>
RefPtr<T> WrapRefPtr(T* ptr) {
  return RefPtr<T>(ptr);
}

}  // namespace WTF

using WTF::RefPtr;

#endif  // WTF_RefPtr_h
