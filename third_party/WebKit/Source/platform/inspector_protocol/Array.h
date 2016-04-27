// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Array_h
#define Array_h

#include "platform/PlatformExport.h"
#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/ErrorSupport.h"
#include "platform/inspector_protocol/String16.h"
#include "platform/inspector_protocol/ValueConversions.h"
#include "platform/inspector_protocol/Values.h"

namespace blink {
namespace protocol {

template<typename T>
class ArrayBase {
public:
    static PassOwnPtr<Array<T>> create()
    {
        return adoptPtr(new Array<T>());
    }

    static PassOwnPtr<Array<T>> parse(protocol::Value* value, ErrorSupport* errors)
    {
        protocol::ListValue* array = ListValue::cast(value);
        if (!array) {
            errors->addError("array expected");
            return nullptr;
        }
        errors->push();
        OwnPtr<Array<T>> result = adoptPtr(new Array<T>());
        for (size_t i = 0; i < array->size(); ++i) {
            errors->setName(String16::number(i));
            T item = FromValue<T>::parse(array->at(i), errors);
            result->m_vector.append(item);
        }
        errors->pop();
        if (errors->hasErrors())
            return nullptr;
        return result.release();
    }

    void addItem(const T& value)
    {
        m_vector.append(value);
    }

    size_t length()
    {
        return m_vector.size();
    }

    T get(size_t index)
    {
        return m_vector[index];
    }

    PassOwnPtr<protocol::ListValue> serialize()
    {
        OwnPtr<protocol::ListValue> result = ListValue::create();
        for (auto& item : m_vector)
            result->pushValue(toValue(item));
        return result.release();
    }

private:
    protocol::Vector<T> m_vector;
};

template<> class Array<String> : public ArrayBase<String> {};
template<> class Array<String16> : public ArrayBase<String16> {};
template<> class Array<int> : public ArrayBase<int> {};
template<> class Array<double> : public ArrayBase<double> {};
template<> class Array<bool> : public ArrayBase<bool> {};

template<typename T>
class Array {
public:
    static PassOwnPtr<Array<T>> create()
    {
        return adoptPtr(new Array<T>());
    }

    static PassOwnPtr<Array<T>> parse(protocol::Value* value, ErrorSupport* errors)
    {
        protocol::ListValue* array = ListValue::cast(value);
        if (!array) {
            errors->addError("array expected");
            return nullptr;
        }
        OwnPtr<Array<T>> result = adoptPtr(new Array<T>());
        errors->push();
        for (size_t i = 0; i < array->size(); ++i) {
            errors->setName(String16::number(i));
            OwnPtr<T> item = FromValue<T>::parse(array->at(i), errors);
            result->m_vector.append(item.release());
        }
        errors->pop();
        if (errors->hasErrors())
            return nullptr;
        return result.release();
    }

    void addItem(PassOwnPtr<T> value)
    {
        m_vector.append(std::move(value));
    }

    size_t length()
    {
        return m_vector.size();
    }

    T* get(size_t index)
    {
        return m_vector[index].get();
    }

    PassOwnPtr<protocol::ListValue> serialize()
    {
        OwnPtr<protocol::ListValue> result = ListValue::create();
        for (auto& item : m_vector)
            result->pushValue(toValue(item.get()));
        return result.release();
    }

private:
    protocol::Vector<OwnPtr<T>> m_vector;
};

} // namespace platform
} // namespace blink

#endif // !defined(Array_h)
