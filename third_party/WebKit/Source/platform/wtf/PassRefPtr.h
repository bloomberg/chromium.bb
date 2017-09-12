/*
 *  Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc.
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

// PassRefPtr will soon be deleted.
// New code should instead pass ownership of the contents of a RefPtr using
// std::move().

#ifndef WTF_PassRefPtr_h
#define WTF_PassRefPtr_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/TypeTraits.h"

namespace WTF {

// requireAdoption() is not overloaded for WTF::RefCounted, which has a built-in
// assumption that adoption is required. requireAdoption() is for bootstrapping
// alternate reference count classes that are compatible with RefPtr/PassRefPtr
// but cannot have adoption checks enabled by default, such as skia's
// SkRefCnt. The purpose of requireAdoption() is to enable adoption checks only
// once it is known that the object will be used with RefPtr/PassRefPtr.
inline void RequireAdoption(const void*) {}

template <typename T>
ALWAYS_INLINE void RefIfNotNull(T* ptr) {
  if (LIKELY(ptr != 0)) {
    RequireAdoption(ptr);
    ptr->Ref();
  }
}

template <typename T>
ALWAYS_INLINE void DerefIfNotNull(T* ptr) {
  if (LIKELY(ptr != 0))
    ptr->Deref();
}

template <typename T>
PassRefPtr<T> WrapPassRefPtr(T* ptr) {
  return PassRefPtr<T>(ptr);
}

}  // namespace WTF

using WTF::PassRefPtr;

#endif  // WTF_PassRefPtr_h
