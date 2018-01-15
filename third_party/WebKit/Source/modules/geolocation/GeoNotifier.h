// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeoNotifier_h
#define GeoNotifier_h

#include "bindings/modules/v8/v8_position_callback.h"
#include "bindings/modules/v8/v8_position_error_callback.h"
#include "modules/geolocation/PositionOptions.h"
#include "platform/Timer.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"

namespace blink {

class Geolocation;
class Geoposition;
class PositionError;

class GeoNotifier final : public GarbageCollectedFinalized<GeoNotifier>,
                          public TraceWrapperBase {
 public:
  static GeoNotifier* Create(Geolocation* geolocation,
                             V8PositionCallback* position_callback,
                             V8PositionErrorCallback* position_error_callback,
                             const PositionOptions& options) {
    return new GeoNotifier(geolocation, position_callback,
                           position_error_callback, options);
  }
  ~GeoNotifier() = default;
  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

  const PositionOptions& Options() const { return options_; }

  // Sets the given error as the fatal error if there isn't one yet.
  // Starts the timer with an interval of 0.
  void SetFatalError(PositionError*);

  bool UseCachedPosition() const { return use_cached_position_; }

  // Tells the notifier to use a cached position and starts its timer with
  // an interval of 0.
  void SetUseCachedPosition();

  void RunSuccessCallback(Geoposition*);
  void RunErrorCallback(PositionError*);

  void StartTimer();
  void StopTimer();

 private:
  GeoNotifier(Geolocation*,
              V8PositionCallback*,
              V8PositionErrorCallback*,
              const PositionOptions&);

  // Runs the error callback if there is a fatal error. Otherwise, if a
  // cached position must be used, registers itself for receiving one.
  // Otherwise, the notifier has expired, and its error callback is run.
  void TimerFired(TimerBase*);

  Member<Geolocation> geolocation_;
  TraceWrapperMember<V8PositionCallback> success_callback_;
  TraceWrapperMember<V8PositionErrorCallback> error_callback_;
  const PositionOptions options_;
  TaskRunnerTimer<GeoNotifier> timer_;
  Member<PositionError> fatal_error_;
  bool use_cached_position_;
};

}  // namespace blink

#endif  // GeoNotifier_h
