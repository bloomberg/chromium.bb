#include "config.h"
#include "core/css/cssom/StyleValue.h"

#include "bindings/core/v8/ScriptValue.h"

namespace blink {

PassRefPtrWillBeRawPtr<StyleValue> StyleValue::create(const CSSValue& val)
{
    // TODO: implement.
    return nullptr;
}

ScriptValue StyleValue::parse(ScriptState* state, const String& property, const String& cssText)
{
    // TODO: implement.
    return ScriptValue();
}

} // namespace blink
