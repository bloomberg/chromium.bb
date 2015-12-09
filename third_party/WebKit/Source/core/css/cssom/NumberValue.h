#ifndef NumberValue_h
#define NumberValue_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/StyleValue.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT NumberValue final : public StyleValue {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<NumberValue> create(double value)
    {
        return adoptRefWillBeNoop(new NumberValue(value));
    }

    double value() const { return m_value; }
    void setValue(double value)
    {
        m_value = value;
    }

    String cssString() const override { return String::number(m_value); }

    PassRefPtrWillBeRawPtr<CSSValue> toCSSValue() const override
    {
        return cssValuePool().createValue(m_value, CSSPrimitiveValue::UnitType::
Number);
    }

    StyleValueType type() const override { return StyleValueType::NumberType; }
protected:
    NumberValue(double value) : m_value(value) {}

    double m_value;
};

} // namespace blink

#endif
