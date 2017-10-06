/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All Rights Reserved.
 * Copyright 2010, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Geolocation_h
#define Geolocation_h

#include "bindings/modules/v8/v8_position_callback.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/page/PageVisibilityObserver.h"
#include "device/geolocation/public/interfaces/geolocation.mojom-blink.h"
#include "modules/ModulesExport.h"
#include "modules/geolocation/GeoNotifier.h"
#include "modules/geolocation/GeolocationWatchers.h"
#include "modules/geolocation/Geoposition.h"
#include "modules/geolocation/PositionError.h"
#include "modules/geolocation/PositionErrorCallback.h"
#include "modules/geolocation/PositionOptions.h"
#include "platform/Timer.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class LocalFrame;
class ExecutionContext;

class MODULES_EXPORT Geolocation final
    : public GarbageCollectedFinalized<Geolocation>,
      public ScriptWrappable,
      public ContextLifecycleObserver,
      public PageVisibilityObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Geolocation);

 public:
  static Geolocation* Create(ExecutionContext*);
  ~Geolocation();
  DECLARE_VIRTUAL_TRACE();
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

  // Inherited from ContextLifecycleObserver and PageVisibilityObserver.
  void ContextDestroyed(ExecutionContext*) override;

  Document* GetDocument() const;
  LocalFrame* GetFrame() const;

  // Creates a oneshot and attempts to obtain a position that meets the
  // constraints of the options.
  void getCurrentPosition(V8PositionCallback*,
                          PositionErrorCallback*,
                          const PositionOptions&);

  // Creates a watcher that will be notified whenever a new position is
  // available that meets the constraints of the options.
  int watchPosition(V8PositionCallback*,
                    PositionErrorCallback*,
                    const PositionOptions&);

  // Removes all references to the watcher, it will not be updated again.
  void clearWatch(int watch_id);

  // Notifies this that a new position is available. Must never be called
  // before permission is granted by the user.
  void PositionChanged();

  // Discards the notifier because a fatal error occurred for it.
  void FatalErrorOccurred(GeoNotifier*);

  // Adds the notifier to the set awaiting a cached position. Runs the success
  // callbacks for them if permission has been granted. Requests permission if
  // it is unknown.
  void RequestUsesCachedPosition(GeoNotifier*);

  // Discards the notifier if it is a oneshot because it timed it.
  void RequestTimedOut(GeoNotifier*);

  // Inherited from PageVisibilityObserver.
  void PageVisibilityChanged() override;

 private:
  explicit Geolocation(ExecutionContext*);

  typedef HeapVector<Member<GeoNotifier>> GeoNotifierVector;
  typedef HeapHashSet<TraceWrapperMember<GeoNotifier>> GeoNotifierSet;

  bool HasListeners() const {
    return !one_shots_.IsEmpty() || !watchers_.IsEmpty();
  }

  void SendError(GeoNotifierVector&, PositionError*);
  void SendPosition(GeoNotifierVector&, Geoposition*);

  // Removes notifiers that use a cached position from |notifiers| and
  // if |cached| is not null they are added to it.
  static void ExtractNotifiersWithCachedPosition(GeoNotifierVector& notifiers,
                                                 GeoNotifierVector* cached);

  // Copies notifiers from |src| vector to |dest| set.
  static void CopyToSet(const GeoNotifierVector& src, GeoNotifierSet& dest);

  static void StopTimer(GeoNotifierVector&);
  void StopTimersForOneShots();
  void StopTimersForWatchers();
  void StopTimers();

  // Sets a fatal error on the given notifiers.
  void CancelRequests(GeoNotifierVector&);

  // Sets a fatal error on all notifiers.
  void CancelAllRequests();

  // Runs the success callbacks on all notifiers. A position must be available
  // and the user must have given permission.
  void MakeSuccessCallbacks();

  // Sends the given error to all notifiers, unless the error is not fatal and
  // the notifier is due to receive a cached position. Clears the oneshots,
  // and also  clears the watchers if the error is fatal.
  void HandleError(PositionError*);

  // Connects to the Geolocation mojo service and starts polling for updates.
  void StartUpdating(GeoNotifier*);

  void StopUpdating();

  void UpdateGeolocationConnection();
  void QueryNextPosition();

  // Attempts to obtain a position for the given notifier, either by using
  // the cached position or by requesting one from the Geolocation.
  // Sets a fatal error if permission is denied or no position can be
  // obtained.
  void StartRequest(GeoNotifier*);

  bool HaveSuitableCachedPosition(const PositionOptions&);

  // Record whether the origin trying to access Geolocation would be allowed
  // to access a feature that can only be accessed by secure origins.
  // See https://goo.gl/Y0ZkNV
  void RecordOriginTypeAccess() const;

  void OnPositionUpdated(device::mojom::blink::GeopositionPtr);

  void OnGeolocationConnectionError();

  GeoNotifierSet one_shots_;
  GeolocationWatchers watchers_;
  Member<Geoposition> last_position_;

  device::mojom::blink::GeolocationPtr geolocation_;
  device::mojom::blink::GeolocationServicePtr geolocation_service_;
  bool enable_high_accuracy_ = false;

  // Whether a GeoNotifier is waiting for a position update.
  bool updating_ = false;

  // Set to true when |geolocation_| is disconnected. This is used to
  // detect when |geolocation_| is disconnected and reconnected while
  // running callbacks in response to a call to OnPositionUpdated().
  bool disconnected_geolocation_ = false;
};

}  // namespace blink

#endif  // Geolocation_h
