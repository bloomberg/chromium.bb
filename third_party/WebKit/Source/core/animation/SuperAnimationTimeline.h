#ifndef SuperAnimationTimeline_h
#define SuperAnimationTimeline_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class CORE_EXPORT SuperAnimationTimeline
    : public GarbageCollectedFinalized<SuperAnimationTimeline>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  virtual ~SuperAnimationTimeline() {}

  virtual double currentTime(bool&) = 0;

  virtual bool IsAnimationTimeline() const { return false; }

  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

}  // namespace blink

#endif
