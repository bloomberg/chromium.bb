#include "bb-v8-util.h"
#include "src/debug/debug-interface.h"


namespace blpwtk2 {
namespace v8util {


v8_inspector::V8Inspector *GetInspector(v8::Isolate *isolate) {
    return v8::debug::GetInspector(isolate);
}


void SetContextId(v8::Local<v8::Context> context, int id) {
    v8::debug::SetContextId(context, id);
}


} // namespace v8util
} // namespace blpwtk2
