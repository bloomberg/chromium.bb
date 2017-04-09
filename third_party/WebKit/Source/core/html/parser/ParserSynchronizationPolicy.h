#ifndef ParserSynchronizationPolicy_h
#define ParserSynchronizationPolicy_h

namespace blink {

enum ParserSynchronizationPolicy {
  kAllowAsynchronousParsing,
  kForceSynchronousParsing,
};
}

#endif  // ParserSynchronizationPolicy_h
