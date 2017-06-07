#ifndef WebFeature_h
#define WebFeature_h

namespace blink {

// A WebFeature conceptually represents some particular web-exposed API
// or code path which can be used/triggered by a web page.
// TODO(rbyers): Add CSS and animated CSS feature types by making this a
// more sophisticated class.
enum class WebFeature : uint32_t {
#include "UseCounterFeature.def"
};

}  // namespace blink

#endif  // WebFeature_h
