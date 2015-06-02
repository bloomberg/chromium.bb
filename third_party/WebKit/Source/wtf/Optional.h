// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Optional_h
#define Optional_h

#include "wtf/Alignment.h"
#include "wtf/Assertions.h"
#include "wtf/Noncopyable.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Utility.h"

namespace WTF {

// This is a lightweight template similar to std::experimental::optional.
// It currently does not support assignment, swapping, comparison, etc.
//
// Use this instead of OwnPtr for cases where you only want to conditionally
// construct a "scope" object.
//
// Example:
//   Optional<DrawingRecorder> recorder;
//   if (shouldDraw)
//       recorder.emplace(constructor, args, here);
//   // recorder destroyed at end of scope
//
// Note in particular that unlike a pointer, though, dereferencing a const
// optional yields a const reference.

template <typename T>
class Optional {
    WTF_MAKE_NONCOPYABLE(Optional);
public:
    Optional() : m_engaged(false) { }
    ~Optional()
    {
        if (m_engaged)
            storage()->~T();
    }

    typedef bool Optional::*UnspecifiedBoolType;
    operator UnspecifiedBoolType() const { return m_engaged ? &Optional::m_engaged : nullptr; }

    T& operator*() { ASSERT(m_engaged); return *storage(); }
    const T& operator*() const { ASSERT(m_engaged); return *storage(); }
    T* operator->() { ASSERT(m_engaged); return storage(); }
    const T* operator->() const { ASSERT(m_engaged); return storage(); }

    template <typename... Args>
    void emplace(Args&&... args)
    {
        ASSERT(!m_engaged);
        new (storage()) T(forward<Args>(args)...);
        m_engaged = true;
    }

private:
    T* storage() { return reinterpret_cast_ptr<T*>(&m_storage.buffer); }
    const T* storage() const { return reinterpret_cast_ptr<const T*>(&m_storage.buffer); }

    bool m_engaged;
    AlignedBuffer<sizeof(T), WTF_ALIGN_OF(T)> m_storage;
};

} // namespace WTF

using WTF::Optional;

#endif // Optional_h
