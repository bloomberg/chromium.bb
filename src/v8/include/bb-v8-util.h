#ifndef INCLUDED_BB_V8_UTIL
#define INCLUDED_BB_V8_UTIL


#include "v8.h"


namespace blpwtk2 {
namespace v8util {


V8_EXPORT void SetContextId(v8::Local<v8::Context> context, int id);


} // namespace v8util
} // namespace blpwtk2


#endif
