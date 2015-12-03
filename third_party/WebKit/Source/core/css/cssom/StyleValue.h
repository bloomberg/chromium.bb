#ifndef StyleValue_h
#define StyleValue_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSValuePool.h"

namespace blink {

class ScriptState;
class ScriptValue;

class CORE_EXPORT StyleValue : public RefCountedWillBeGarbageCollectedFinalized<StyleValue>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    enum StyleValueType {
        KeywordValueType, SimpleLengthType, CalcLengthType, NumberType
    };

    virtual StyleValueType type() const = 0;

    static PassRefPtrWillBeRawPtr<StyleValue> create(const CSSValue&);
    static ScriptValue parse(ScriptState*, const String& property, const String& cssText);

    virtual const String& cssString() const = 0;
    virtual PassRefPtrWillBeRawPtr<CSSValue> toCSSValue() = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() { }

protected:
    StyleValue() {}
};

} // namespace blink

#endif
