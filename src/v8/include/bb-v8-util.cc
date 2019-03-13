#include "bb-v8-util.h"
#include "src/debug/debug-interface.h"


namespace blpwtk2 {
namespace v8util {


void SetContextId(v8::Local<v8::Context> context, int id) {
    v8::debug::SetContextId(context, id);
}


} // namespace v8util
} // namespace blpwtk2
