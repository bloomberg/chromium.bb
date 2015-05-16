#include "heap/stubs.h"

namespace blink {

struct HeapObject : public GarbageCollected<HeapObject> {
    void trace(Visitor*) { }
};

template<typename T>
class TemplateBase
    : public GarbageCollected<TemplateBase<T> > {
public:
    void trace(Visitor* visitor) { visitor->trace(m_obj); }
private:
    Member<HeapObject> m_obj;
};

class Subclass : public TemplateBase<Subclass> {
};

}
