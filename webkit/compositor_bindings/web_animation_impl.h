// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebAnimationImpl_h
#define WebAnimationImpl_h

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAnimation.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace cc { class Animation; }

namespace WebKit {
class WebAnimationCurve;

class WebAnimationImpl : public WebAnimation {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebAnimationImpl(const WebAnimationCurve&,
                                                     TargetProperty,
                                                     int animation_id,
                                                     int group_id = 0);
  virtual ~WebAnimationImpl();

  // WebAnimation implementation
  virtual int id();
  virtual TargetProperty targetProperty() const;
  virtual int iterations() const;
  virtual void setIterations(int);
  virtual double startTime() const;
  virtual void setStartTime(double monotonic_time);
  virtual double timeOffset() const;
  virtual void setTimeOffset(double monotonic_time);
  virtual bool alternatesDirection() const;
  virtual void setAlternatesDirection(bool);

  scoped_ptr<cc::Animation> cloneToAnimation();

 private:
  scoped_ptr<cc::Animation> animation_;
};

}

#endif  // WebAnimationImpl_h
