/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CallbackPromiseAdapter_h
#define CallbackPromiseAdapter_h

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "public/platform/WebCallbacks.h"
#include "public/platform/WebPassOwnPtr.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class CallbackPromiseAdapterBase {
public:
    // The following templates should be gone once the repositories are merged
    // and we can use C++11 libraries.
    template <typename T>
    struct PassTypeImpl {
        using Type = T;
    };
    template <typename T>
    struct PassTypeImpl<OwnPtr<T>> {
        using Type = PassOwnPtr<T>;
    };

    template <typename T>
    struct WebPassTypeImpl {
        using Type = T;
    };
    template <typename T>
    struct WebPassTypeImpl<OwnPtr<T>> {
        using Type = WebPassOwnPtr<T>;
    };
    template <typename T>
    using PassType = typename PassTypeImpl<T>::Type;
    template <typename T>
    using WebPassType = typename WebPassTypeImpl<T>::Type;
    template <typename T>
    static T& adopt(T& x) { return x; }
    template <typename T>
    static PassOwnPtr<T> adopt(WebPassOwnPtr<T>& x) { return x.release(); }
    template <typename T>
    static PassType<T> pass(T& x) { return x; }
    template <typename T>
    static PassOwnPtr<T> pass(OwnPtr<T>& x) { return x.release(); }
};


// TODO(yhirano): Add comments.
template<typename S, typename T>
class CallbackPromiseAdapter final
    : public WebCallbacks<typename CallbackPromiseAdapterBase::WebPassType<typename S::WebType>, typename CallbackPromiseAdapterBase::WebPassType<typename T::WebType>>, public CallbackPromiseAdapterBase {
    WTF_MAKE_NONCOPYABLE(CallbackPromiseAdapter);
public:
    explicit CallbackPromiseAdapter(ScriptPromiseResolver* resolver)
        : m_resolver(resolver)
    {
        ASSERT(m_resolver);
    }
    ~CallbackPromiseAdapter() override { }

    void onSuccess(WebPassType<typename S::WebType> r) override
    {
        typename S::WebType result(adopt(r));
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->resolve(S::take(m_resolver.get(), pass(result)));
    }

    void onError(WebPassType<typename T::WebType> e) override
    {
        typename T::WebType error(adopt(e));
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject(T::take(m_resolver.get(), pass(error)));
    }

private:
    Persistent<ScriptPromiseResolver> m_resolver;
};

template<typename T>
class CallbackPromiseAdapter<void, T> final : public WebCallbacks<void, typename CallbackPromiseAdapterBase::WebPassType<typename T::WebType>>, public CallbackPromiseAdapterBase {
    WTF_MAKE_NONCOPYABLE(CallbackPromiseAdapter);
public:
    explicit CallbackPromiseAdapter(ScriptPromiseResolver* resolver)
        : m_resolver(resolver)
    {
        ASSERT(m_resolver);
    }
    ~CallbackPromiseAdapter() override { }

    void onSuccess() override
    {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->resolve();
    }

    void onError(WebPassType<typename T::WebType> e) override
    {
        typename T::WebType error(adopt(e));
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject(T::take(m_resolver.get(), pass(error)));
    }

private:
    Persistent<ScriptPromiseResolver> m_resolver;
};

template<typename S>
class CallbackPromiseAdapter<S, void> final : public WebCallbacks<typename CallbackPromiseAdapterBase::WebPassType<typename S::WebType>, void>, public CallbackPromiseAdapterBase {
    WTF_MAKE_NONCOPYABLE(CallbackPromiseAdapter);
public:
    explicit CallbackPromiseAdapter(ScriptPromiseResolver* resolver)
        : m_resolver(resolver)
    {
        ASSERT(m_resolver);
    }
    ~CallbackPromiseAdapter() override { }

    void onSuccess(WebPassType<typename S::WebType> r) override
    {
        typename S::WebType result(adopt(r));
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->resolve(S::take(m_resolver.get(), pass(result)));
    }

    void onError() override
    {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject();
    }

private:
    Persistent<ScriptPromiseResolver> m_resolver;
};

template<typename T>
class CallbackPromiseAdapter<bool, T> final : public WebCallbacks<bool*, typename CallbackPromiseAdapterBase::WebPassType<typename T::WebType>>, public CallbackPromiseAdapterBase {
    WTF_MAKE_NONCOPYABLE(CallbackPromiseAdapter);
public:
    explicit CallbackPromiseAdapter(ScriptPromiseResolver* resolver)
        : m_resolver(resolver)
    {
        ASSERT(m_resolver);
    }
    ~CallbackPromiseAdapter() override { }

    // TODO(nhiroki): onSuccess should take ownership of a bool object for
    // consistency. (http://crbug.com/493531)
    void onSuccess(bool* result) override
    {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->resolve(*result);
    }

    void onError(WebPassType<typename T::WebType> e) override
    {
        typename T::WebType error(adopt(e));
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject(T::take(m_resolver.get(), pass(error)));
    }

private:
    Persistent<ScriptPromiseResolver> m_resolver;
};

template<>
class CallbackPromiseAdapter<void, void> final : public WebCallbacks<void, void>, public CallbackPromiseAdapterBase {
    WTF_MAKE_NONCOPYABLE(CallbackPromiseAdapter);
public:
    explicit CallbackPromiseAdapter(ScriptPromiseResolver* resolver)
        : m_resolver(resolver)
    {
        ASSERT(m_resolver);
    }
    ~CallbackPromiseAdapter() override { }

    void onSuccess() override
    {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->resolve();
    }

    void onError() override
    {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject();
    }

private:
    Persistent<ScriptPromiseResolver> m_resolver;
};

template<>
class CallbackPromiseAdapter<bool, void> final : public WebCallbacks<bool*, void>, public CallbackPromiseAdapterBase {
    WTF_MAKE_NONCOPYABLE(CallbackPromiseAdapter);
public:
    explicit CallbackPromiseAdapter(ScriptPromiseResolver* resolver)
        : m_resolver(resolver)
    {
        ASSERT(m_resolver);
    }
    ~CallbackPromiseAdapter() override { }

    // TODO(nhiroki): onSuccess should take ownership of a bool object for
    // consistency. (http://crbug.com/493531)
    void onSuccess(bool* result) override
    {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->resolve(*result);
    }

    void onError() override
    {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject();
    }

private:
    Persistent<ScriptPromiseResolver> m_resolver;
};

} // namespace blink

#endif
