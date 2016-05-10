/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WTF_PassOwnPtr_h
#define WTF_PassOwnPtr_h

#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"

namespace WTF {

// PassOwnPtr is now an alias of OwnPtr, as defined in Forward.h.

template <typename T> PassOwnPtr<T> adoptPtr(T*);
template <typename T> PassOwnPtr<T[]> adoptArrayPtr(T*);

template <typename T> inline PassOwnPtr<T> adoptPtr(T* ptr)
{
    return PassOwnPtr<T>(ptr);
}

template <typename T> inline PassOwnPtr<T[]> adoptArrayPtr(T* ptr)
{
    return PassOwnPtr<T[]>(ptr);
}

template <typename T, typename U> inline PassOwnPtr<T> static_pointer_cast(PassOwnPtr<U>&& p)
{
    static_assert(!std::is_array<T>::value, "pointers to array must never be converted");
    return adoptPtr(static_cast<T*>(p.leakPtr()));
}

} // namespace WTF

using WTF::adoptPtr;
using WTF::adoptArrayPtr;
using WTF::static_pointer_cast;

#endif // WTF_PassOwnPtr_h
