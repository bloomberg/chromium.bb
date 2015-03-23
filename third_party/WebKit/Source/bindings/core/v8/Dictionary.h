/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Dictionary_h
#define Dictionary_h

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/Nullable.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "core/CoreExport.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

namespace blink {

// Dictionary class provides ways to retrieve property values as C++ objects
// from a V8 object. Instances of this class must not outlive V8's handle scope
// because they hold a V8 value without putting it on persistent handles.
class CORE_EXPORT Dictionary {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    Dictionary();
    Dictionary(const v8::Handle<v8::Value>& options, v8::Isolate*, ExceptionState&);
    ~Dictionary();

    Dictionary& operator=(const Dictionary&);

    // This is different from the default constructor:
    //   * isObject() is true when using createEmpty().
    //   * isUndefinedOrNull() is true when using default constructor.
    static Dictionary createEmpty(v8::Isolate*);

    bool isObject() const;
    bool isUndefinedOrNull() const;

    bool get(const String&, Dictionary&) const;
    bool get(const String&, v8::Local<v8::Value>&) const;

    // Sets properties using default attributes.
    bool set(const String&, const v8::Handle<v8::Value>&);
    bool set(const String&, const String&);
    bool set(const String&, unsigned);
    bool set(const String&, const Dictionary&);

    v8::Handle<v8::Value> v8Value() const { return m_options; }

    class CORE_EXPORT ConversionContext {
    public:
        explicit ConversionContext(ExceptionState& exceptionState)
            : m_exceptionState(exceptionState)
            , m_dirty(true)
        {
            resetPerPropertyContext();
        }

        ExceptionState& exceptionState() const { return m_exceptionState; }

        bool isNullable() const { return m_isNullable; }
        String typeName() const { return m_propertyTypeName; }

        ConversionContext& setConversionType(const String&, bool);

        void throwTypeError(const String& detail);

        void resetPerPropertyContext();

    private:
        ExceptionState& m_exceptionState;
        bool m_dirty;

        bool m_isNullable;
        String m_propertyTypeName;
    };

    class ConversionContextScope {
    public:
        explicit ConversionContextScope(ConversionContext& context)
            : m_context(context) { }
        ~ConversionContextScope()
        {
            m_context.resetPerPropertyContext();
        }
    private:
        ConversionContext& m_context;
    };

    bool convert(ConversionContext&, const String&, Dictionary&) const;

    bool getOwnPropertiesAsStringHashMap(HashMap<String, String>&) const;
    bool getPropertyNames(Vector<String>&) const;

    bool hasProperty(const String&) const;

    v8::Isolate* isolate() const { return m_isolate; }

    bool getKey(const String& key, v8::Local<v8::Value>&) const;

private:
    v8::Local<v8::Context> v8Context() const
    {
        ASSERT(m_isolate);
        return m_isolate->GetCurrentContext();
    }

    v8::Handle<v8::Value> m_options;
    v8::Isolate* m_isolate;
    ExceptionState* m_exceptionState;
};

template<>
struct NativeValueTraits<Dictionary> {
    static inline Dictionary nativeValue(v8::Isolate* isolate, v8::Handle<v8::Value> value, ExceptionState& exceptionState)
    {
        return Dictionary(value, isolate, exceptionState);
    }
};

// DictionaryHelper is a collection of static methods for getting or
// converting a value from Dictionary.
struct DictionaryHelper {
    template <typename T>
    static bool get(const Dictionary&, const String& key, T& value);
    template <typename T>
    static bool get(const Dictionary&, const String& key, T& value, bool& hasValue);
    template <typename T>
    static bool get(const Dictionary&, const String& key, T& value, ExceptionState&);
    template <typename T>
    static bool getWithUndefinedOrNullCheck(const Dictionary& dictionary, const String& key, T& value)
    {
        v8::Local<v8::Value> v8Value;
        if (!dictionary.getKey(key, v8Value) || isUndefinedOrNull(v8Value))
            return false;
        return DictionaryHelper::get(dictionary, key, value);
    }
    template <template <typename> class PointerType, typename T>
    static bool get(const Dictionary&, const String& key, PointerType<T>& value);
    template <typename T>
    static bool convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, T& value);
    template <typename T>
    static bool convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, Nullable<T>& value);
    template <template <typename> class PointerType, typename T>
    static bool convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, PointerType<T>& value);
};

}

#endif // Dictionary_h
