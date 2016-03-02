// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Values_h
#define Values_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/TypeTraits.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {
namespace protocol {

class ListValue;
class DictionaryValue;
class Value;

class PLATFORM_EXPORT Value {
    WTF_MAKE_NONCOPYABLE(Value);
public:
    static const int maxDepth = 1000;

    virtual ~Value() { }

    static PassOwnPtr<Value> null()
    {
        return adoptPtr(new Value());
    }

    enum ValueType {
        TypeNull = 0,
        TypeBoolean,
        TypeNumber,
        TypeString,
        TypeObject,
        TypeArray
    };

    ValueType type() const { return m_type; }

    bool isNull() const { return m_type == TypeNull; }

    virtual bool asBoolean(bool* output) const;
    virtual bool asNumber(double* output) const;
    virtual bool asNumber(int* output) const;
    virtual bool asString(String* output) const;

    String toJSONString() const;
    virtual void writeJSON(StringBuilder* output) const;
    virtual PassOwnPtr<Value> clone() const;

protected:
    Value() : m_type(TypeNull) { }
    explicit Value(ValueType type) : m_type(type) { }

private:
    friend class DictionaryValue;
    friend class ListValue;

    ValueType m_type;
};

class PLATFORM_EXPORT FundamentalValue : public Value {
public:
    static PassOwnPtr<FundamentalValue> create(bool value)
    {
        return adoptPtr(new FundamentalValue(value));
    }

    static PassOwnPtr<FundamentalValue> create(int value)
    {
        return adoptPtr(new FundamentalValue(value));
    }

    static PassOwnPtr<FundamentalValue> create(double value)
    {
        return adoptPtr(new FundamentalValue(value));
    }

    bool asBoolean(bool* output) const override;
    bool asNumber(double* output) const override;
    bool asNumber(int* output) const override;
    void writeJSON(StringBuilder* output) const override;
    PassOwnPtr<Value> clone() const override;

private:
    explicit FundamentalValue(bool value) : Value(TypeBoolean), m_boolValue(value) { }
    explicit FundamentalValue(int value) : Value(TypeNumber), m_doubleValue((double)value) { }
    explicit FundamentalValue(double value) : Value(TypeNumber), m_doubleValue(value) { }

    union {
        bool m_boolValue;
        double m_doubleValue;
    };
};

class PLATFORM_EXPORT StringValue : public Value {
public:
    static PassOwnPtr<StringValue> create(const String& value)
    {
        return adoptPtr(new StringValue(value));
    }

    static PassOwnPtr<StringValue> create(const char* value)
    {
        return adoptPtr(new StringValue(value));
    }

    bool asString(String* output) const override;
    void writeJSON(StringBuilder* output) const override;
    PassOwnPtr<Value> clone() const override;

private:
    explicit StringValue(const String& value) : Value(TypeString), m_stringValue(value) { }
    explicit StringValue(const char* value) : Value(TypeString), m_stringValue(value) { }

    String m_stringValue;
};

class PLATFORM_EXPORT DictionaryValue : public Value {
private:
    typedef HashMap<String, OwnPtr<Value>> Dictionary;

public:
    typedef Dictionary::iterator iterator;
    typedef Dictionary::const_iterator const_iterator;

    static PassOwnPtr<DictionaryValue> create()
    {
        return adoptPtr(new DictionaryValue());
    }

    static DictionaryValue* cast(Value* value)
    {
        if (!value || value->type() != TypeObject)
            return nullptr;
        return static_cast<DictionaryValue*>(value);
    }

    static PassOwnPtr<DictionaryValue> cast(PassOwnPtr<Value> value)
    {
        return adoptPtr(DictionaryValue::cast(value.leakPtr()));
    }

    void writeJSON(StringBuilder* output) const override;
    PassOwnPtr<Value> clone() const override;

    int size() const { return m_data.size(); }

    void setBoolean(const String& name, bool);
    void setNumber(const String& name, double);
    void setString(const String& name, const String&);
    void setValue(const String& name, PassOwnPtr<Value>);
    void setObject(const String& name, PassOwnPtr<DictionaryValue>);
    void setArray(const String& name, PassOwnPtr<ListValue>);

    bool getBoolean(const String& name, bool* output) const;
    template<class T> bool getNumber(const String& name, T* output) const
    {
        Value* value = get(name);
        if (!value)
            return false;
        return value->asNumber(output);
    }
    bool getString(const String& name, String* output) const;

    DictionaryValue* getObject(const String& name) const;
    ListValue* getArray(const String& name) const;
    Value* get(const String& name) const;

    bool booleanProperty(const String& name, bool defaultValue) const;

    void remove(const String& name);

    iterator begin() { return m_data.begin(); }
    iterator end() { return m_data.end(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator end() const { return m_data.end(); }
    ~DictionaryValue() override;

private:
    DictionaryValue();

    Dictionary m_data;
    Vector<String> m_order;
};

class PLATFORM_EXPORT ListValue : public Value {
public:
    static PassOwnPtr<ListValue> create()
    {
        return adoptPtr(new ListValue());
    }

    static ListValue* cast(Value* value)
    {
        if (!value || value->type() != TypeArray)
            return nullptr;
        return static_cast<ListValue*>(value);
    }

    static PassOwnPtr<ListValue> cast(PassOwnPtr<Value> value)
    {
        return adoptPtr(ListValue::cast(value.leakPtr()));
    }

    ~ListValue() override;

    void writeJSON(StringBuilder* output) const override;
    PassOwnPtr<Value> clone() const override;

    void pushValue(PassOwnPtr<Value>);

    Value* get(size_t index);
    unsigned length() const { return m_data.size(); }

private:
    ListValue();
    Vector<OwnPtr<Value>> m_data;
};

} // namespace protocol
} // namespace blink

#endif // Values_h
