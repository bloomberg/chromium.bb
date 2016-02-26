// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Values_h
#define Values_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/RefCounted.h"
#include "wtf/TypeTraits.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {
namespace protocol {

class ListValue;
class DictionaryValue;
class Value;

class PLATFORM_EXPORT Value : public RefCounted<Value> {
public:
    static const int maxDepth = 1000;

    virtual ~Value() { }

    static PassRefPtr<Value> null()
    {
        return adoptRef(new Value());
    }

    typedef enum {
        TypeNull = 0,
        TypeBoolean,
        TypeNumber,
        TypeString,
        TypeObject,
        TypeArray
    } Type;

    Type type() const { return m_type; }

    bool isNull() const { return m_type == TypeNull; }

    virtual bool asBoolean(bool* output) const;
    virtual bool asNumber(double* output) const;
    virtual bool asNumber(int* output) const;
    virtual bool asString(String* output) const;

    String toJSONString() const;
    virtual void writeJSON(StringBuilder* output) const;

protected:
    Value() : m_type(TypeNull) { }
    explicit Value(Type type) : m_type(type) { }

private:
    friend class DictionaryValue;
    friend class ListValue;

    Type m_type;
};

class PLATFORM_EXPORT FundamentalValue : public Value {
public:

    static PassRefPtr<FundamentalValue> create(bool value)
    {
        return adoptRef(new FundamentalValue(value));
    }

    static PassRefPtr<FundamentalValue> create(int value)
    {
        return adoptRef(new FundamentalValue(value));
    }

    static PassRefPtr<FundamentalValue> create(double value)
    {
        return adoptRef(new FundamentalValue(value));
    }

    bool asBoolean(bool* output) const override;
    bool asNumber(double* output) const override;
    bool asNumber(int* output) const override;

    void writeJSON(StringBuilder* output) const override;

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
    static PassRefPtr<StringValue> create(const String& value)
    {
        return adoptRef(new StringValue(value));
    }

    static PassRefPtr<StringValue> create(const char* value)
    {
        return adoptRef(new StringValue(value));
    }

    bool asString(String* output) const override;

    void writeJSON(StringBuilder* output) const override;

private:
    explicit StringValue(const String& value) : Value(TypeString), m_stringValue(value) { }
    explicit StringValue(const char* value) : Value(TypeString), m_stringValue(value) { }

    String m_stringValue;
};

class PLATFORM_EXPORT DictionaryValue : public Value {
private:
    typedef HashMap<String, RefPtr<Value>> Dictionary;

public:
    typedef Dictionary::iterator iterator;
    typedef Dictionary::const_iterator const_iterator;

    static PassRefPtr<DictionaryValue> create()
    {
        return adoptRef(new DictionaryValue());
    }

    static PassRefPtr<DictionaryValue> cast(PassRefPtr<Value> value)
    {
        if (!value || value->type() != TypeObject)
            return nullptr;
        return adoptRef(static_cast<DictionaryValue*>(value.leakRef()));
    }

    void writeJSON(StringBuilder* output) const override;

    int size() const { return m_data.size(); }

    void setBoolean(const String& name, bool);
    void setNumber(const String& name, double);
    void setString(const String& name, const String&);
    void setValue(const String& name, PassRefPtr<Value>);
    void setObject(const String& name, PassRefPtr<DictionaryValue>);
    void setArray(const String& name, PassRefPtr<ListValue>);

    iterator find(const String& name);
    const_iterator find(const String& name) const;
    bool getBoolean(const String& name, bool* output) const;
    template<class T> bool getNumber(const String& name, T* output) const
    {
        RefPtr<Value> value = get(name);
        if (!value)
            return false;
        return value->asNumber(output);
    }
    bool getString(const String& name, String* output) const;
    PassRefPtr<DictionaryValue> getObject(const String& name) const;
    PassRefPtr<ListValue> getArray(const String& name) const;
    PassRefPtr<Value> get(const String& name) const;

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
    typedef Vector<RefPtr<Value>>::iterator iterator;
    typedef Vector<RefPtr<Value>>::const_iterator const_iterator;

    static PassRefPtr<ListValue> create()
    {
        return adoptRef(new ListValue());
    }

    static PassRefPtr<ListValue> cast(PassRefPtr<Value> value)
    {
        if (!value || value->type() != TypeArray)
            return nullptr;
        return adoptRef(static_cast<ListValue*>(value.leakRef()));
    }

    ~ListValue() override;

    void writeJSON(StringBuilder* output) const override;

    void pushValue(PassRefPtr<Value>);

    PassRefPtr<Value> get(size_t index);
    unsigned length() const { return m_data.size(); }

    iterator begin() { return m_data.begin(); }
    iterator end() { return m_data.end(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator end() const { return m_data.end(); }

private:
    ListValue();
    Vector<RefPtr<Value>> m_data;
};

} // namespace protocol
} // namespace blink

#endif // Values_h
