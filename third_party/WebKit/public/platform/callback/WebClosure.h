// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebClosure_h
#define WebClosure_h

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "public/platform/WebCommon.h"

#include <memory>
#include <utility>

#if BLINK_IMPLEMENTATION
#include "wtf/Functional.h"
#else
#include "base/logging.h"
#endif

namespace blink {

// Conversion from WTF closures to base closures to pass a callback out of
// blink.
class WebClosure {
public:
#if BLINK_IMPLEMENTATION
    WebClosure() {}

    explicit WebClosure(PassOwnPtr<SameThreadClosure> c)
    {
        m_closure = base::Bind(&RunAndDelete, base::Passed(base::WrapUnique(c.leakPtr())));
    }
#endif

    // TODO(danakj): These could be =default with MSVC 2015.
    WebClosure(WebClosure&& other) { *this = std::move(other); }
    WebClosure& operator=(WebClosure&& other)
    {
#if DCHECK_IS_ON()
        m_haveClosure = other.m_haveClosure;
        other.m_haveClosure = false;
#endif
        m_closure = std::move(other.m_closure);
        return *this;
    }

#if !BLINK_IMPLEMENTATION
    // TODO(danakj): This could be rvalue-ref-qualified.
    base::Closure TakeBaseClosure()
    {
#if DCHECK_IS_ON()
        // Don't call this more than once!
        DCHECK(m_haveClosure);
        m_haveClosure = false;
#endif
        return std::move(m_closure);
    }
#endif

private:
#if BLINK_IMPLEMENTATION
    static void RunAndDelete(std::unique_ptr<SameThreadClosure> c) { (*c)(); }
#endif

#if DCHECK_IS_ON()
    bool m_haveClosure = true;
#endif
    base::Closure m_closure;

    DISALLOW_COPY_AND_ASSIGN(WebClosure);
};

} // namespace blink

#endif // WebClosure_h
