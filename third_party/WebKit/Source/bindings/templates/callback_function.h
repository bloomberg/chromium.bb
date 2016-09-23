{% filter format_blink_cpp_source_code %}

{% include 'copyright_block.txt' %}

#ifndef {{v8_class}}_h
#define {{v8_class}}_h

{% for filename in header_includes %}
#include "{{filename}}"
{% endfor %}

namespace blink {

class ScriptState;
{% for forward_declaration in forward_declarations %}
class {{forward_declaration}};
{% endfor %}

class {{exported}}{{v8_class}} final : public GarbageCollectedFinalized<{{v8_class}}> {

public:
    static {{v8_class}}* create(v8::Isolate* isolate, v8::Local<v8::Function> callback)
    {
        return new {{v8_class}}(isolate, callback);
    }

    ~{{v8_class}}() = default;

    DECLARE_TRACE();

    bool call({{argument_declarations | join(', ')}});

    v8::Local<v8::Function> v8Value(v8::Isolate* isolate)
    {
        return m_callback.newLocal(isolate);
    }

    void setWrapperReference(v8::Isolate* isolate, const v8::Persistent<v8::Object>& wrapper)
    {
        DCHECK(!m_callback.isEmpty());
        m_callback.setReference(wrapper, isolate);
    }

private:
    {{v8_class}}(v8::Isolate* isolate, v8::Local<v8::Function>);
    ScopedPersistent<v8::Function> m_callback;
};

} // namespace blink

#endif // {{v8_class}}_h

{% endfilter %}{# format_blink_cpp_source_code #}
