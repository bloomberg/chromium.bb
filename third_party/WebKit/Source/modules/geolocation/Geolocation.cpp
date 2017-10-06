/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
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

#include "modules/geolocation/Geolocation.h"

#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/Document.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/frame/Deprecation.h"
#include "core/frame/HostsUsingFeatures.h"
#include "core/frame/PerformanceMonitor.h"
#include "core/frame/Settings.h"
#include "core/probe/CoreProbes.h"
#include "modules/geolocation/Coordinates.h"
#include "modules/geolocation/GeolocationError.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/CurrentTime.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFeaturePolicyFeature.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {
namespace {

const char kPermissionDeniedErrorMessage[] = "User denied Geolocation";
const char kFramelessDocumentErrorMessage[] =
    "Geolocation cannot be used in frameless documents";

Geoposition* CreateGeoposition(
    const device::mojom::blink::Geoposition& position) {
  Coordinates* coordinates = Coordinates::Create(
      position.latitude, position.longitude,
      // Lowest point on land is at approximately -400 meters.
      position.altitude > -10000., position.altitude, position.accuracy,
      position.altitude_accuracy >= 0., position.altitude_accuracy,
      position.heading >= 0. && position.heading <= 360., position.heading,
      position.speed >= 0., position.speed);
  return Geoposition::Create(coordinates,
                             ConvertSecondsToDOMTimeStamp(position.timestamp));
}

PositionError* CreatePositionError(
    device::mojom::blink::Geoposition::ErrorCode mojom_error_code,
    const String& error) {
  PositionError::ErrorCode error_code = PositionError::kPositionUnavailable;
  switch (mojom_error_code) {
    case device::mojom::blink::Geoposition::ErrorCode::PERMISSION_DENIED:
      error_code = PositionError::kPermissionDenied;
      break;
    case device::mojom::blink::Geoposition::ErrorCode::POSITION_UNAVAILABLE:
      error_code = PositionError::kPositionUnavailable;
      break;
    case device::mojom::blink::Geoposition::ErrorCode::NONE:
    case device::mojom::blink::Geoposition::ErrorCode::TIMEOUT:
      NOTREACHED();
      break;
  }
  return PositionError::Create(error_code, error);
}

static void ReportGeolocationViolation(ExecutionContext* context) {
  if (!UserGestureIndicator::ProcessingUserGesture()) {
    PerformanceMonitor::ReportGenericViolation(
        context, PerformanceMonitor::kDiscouragedAPIUse,
        "Only request geolocation information in response to a user gesture.",
        0, nullptr);
  }
}

}  // namespace

Geolocation* Geolocation::Create(ExecutionContext* context) {
  Geolocation* geolocation = new Geolocation(context);
  return geolocation;
}

Geolocation::Geolocation(ExecutionContext* context)
    : ContextLifecycleObserver(context),
      PageVisibilityObserver(GetDocument()->GetPage()) {}

Geolocation::~Geolocation() {
}

DEFINE_TRACE(Geolocation) {
  visitor->Trace(one_shots_);
  visitor->Trace(watchers_);
  visitor->Trace(last_position_);
  ContextLifecycleObserver::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(Geolocation) {
  for (const auto& one_shot : one_shots_)
    visitor->TraceWrappers(one_shot);
  visitor->TraceWrappers(watchers_);
  ScriptWrappable::TraceWrappers(visitor);
}

Document* Geolocation::GetDocument() const {
  return ToDocument(GetExecutionContext());
}

LocalFrame* Geolocation::GetFrame() const {
  return GetDocument() ? GetDocument()->GetFrame() : 0;
}

void Geolocation::ContextDestroyed(ExecutionContext*) {
  geolocation_service_.reset();
  CancelAllRequests();
  StopUpdating();
  last_position_ = nullptr;
  one_shots_.clear();
  watchers_.Clear();
}

void Geolocation::RecordOriginTypeAccess() const {
  DCHECK(GetFrame());

  Document* document = this->GetDocument();
  DCHECK(document);

  // It is required by isSecureContext() but isn't
  // actually used. This could be used later if a warning is shown in the
  // developer console.
  String insecure_origin_msg;
  if (document->IsSecureContext(insecure_origin_msg)) {
    UseCounter::Count(document, WebFeature::kGeolocationSecureOrigin);
    UseCounter::CountCrossOriginIframe(
        *document, WebFeature::kGeolocationSecureOriginIframe);
    Deprecation::CountDeprecationFeaturePolicy(
        *document, WebFeaturePolicyFeature::kGeolocation);
  } else if (GetFrame()
                 ->GetSettings()
                 ->GetAllowGeolocationOnInsecureOrigins()) {
    // TODO(jww): This should be removed after WebView is fixed so that it
    // disallows geolocation in insecure contexts.
    //
    // See https://crbug.com/603574.
    Deprecation::CountDeprecation(
        document, WebFeature::kGeolocationInsecureOriginDeprecatedNotRemoved);
    Deprecation::CountDeprecationCrossOriginIframe(
        *document,
        WebFeature::kGeolocationInsecureOriginIframeDeprecatedNotRemoved);
    HostsUsingFeatures::CountAnyWorld(
        *document, HostsUsingFeatures::Feature::kGeolocationInsecureHost);
    Deprecation::CountDeprecationFeaturePolicy(
        *document, WebFeaturePolicyFeature::kGeolocation);
  } else {
    Deprecation::CountDeprecation(document,
                                  WebFeature::kGeolocationInsecureOrigin);
    Deprecation::CountDeprecationCrossOriginIframe(
        *document, WebFeature::kGeolocationInsecureOriginIframe);
    HostsUsingFeatures::CountAnyWorld(
        *document, HostsUsingFeatures::Feature::kGeolocationInsecureHost);
  }
}

void Geolocation::getCurrentPosition(V8PositionCallback* success_callback,
                                     PositionErrorCallback* error_callback,
                                     const PositionOptions& options) {
  if (!GetFrame())
    return;

  ReportGeolocationViolation(GetDocument());
  probe::breakableLocation(GetDocument(), "Geolocation.getCurrentPosition");

  GeoNotifier* notifier =
      GeoNotifier::Create(this, success_callback, error_callback, options);
  StartRequest(notifier);

  one_shots_.insert(notifier);
}

int Geolocation::watchPosition(V8PositionCallback* success_callback,
                               PositionErrorCallback* error_callback,
                               const PositionOptions& options) {
  if (!GetFrame())
    return 0;

  ReportGeolocationViolation(GetDocument());
  probe::breakableLocation(GetDocument(), "Geolocation.watchPosition");

  GeoNotifier* notifier =
      GeoNotifier::Create(this, success_callback, error_callback, options);
  StartRequest(notifier);

  int watch_id;
  // Keep asking for the next id until we're given one that we don't already
  // have.
  do {
    watch_id = GetExecutionContext()->CircularSequentialID();
  } while (!watchers_.Add(watch_id, notifier));
  return watch_id;
}

void Geolocation::StartRequest(GeoNotifier* notifier) {
  RecordOriginTypeAccess();
  String error_message;
  if (!GetFrame()->GetSettings()->GetAllowGeolocationOnInsecureOrigins() &&
      !GetExecutionContext()->IsSecureContext(error_message)) {
    notifier->SetFatalError(
        PositionError::Create(PositionError::kPermissionDenied, error_message));
    return;
  }

  if (HaveSuitableCachedPosition(notifier->Options())) {
    notifier->SetUseCachedPosition();
  } else {
    if (notifier->Options().timeout() > 0)
      StartUpdating(notifier);
    notifier->StartTimer();
  }
}

void Geolocation::FatalErrorOccurred(GeoNotifier* notifier) {
  // This request has failed fatally. Remove it from our lists.
  one_shots_.erase(notifier);
  watchers_.Remove(notifier);

  if (!HasListeners())
    StopUpdating();
}

void Geolocation::RequestUsesCachedPosition(GeoNotifier* notifier) {
  notifier->RunSuccessCallback(last_position_);

  // If this is a one-shot request, stop it. Otherwise, if the watch still
  // exists, start the service to get updates.
  if (one_shots_.Contains(notifier)) {
    one_shots_.erase(notifier);
  } else if (watchers_.Contains(notifier)) {
    if (notifier->Options().timeout() > 0)
      StartUpdating(notifier);
    notifier->StartTimer();
  }

  if (!HasListeners())
    StopUpdating();
}

void Geolocation::RequestTimedOut(GeoNotifier* notifier) {
  // If this is a one-shot request, stop it.
  one_shots_.erase(notifier);

  if (!HasListeners())
    StopUpdating();
}

bool Geolocation::HaveSuitableCachedPosition(const PositionOptions& options) {
  if (!last_position_)
    return false;
  if (!options.maximumAge())
    return false;
  DOMTimeStamp current_time_millis =
      ConvertSecondsToDOMTimeStamp(CurrentTime());
  return last_position_->timestamp() >
         current_time_millis - options.maximumAge();
}

void Geolocation::clearWatch(int watch_id) {
  if (watch_id <= 0)
    return;

  watchers_.Remove(watch_id);

  if (!HasListeners())
    StopUpdating();
}

void Geolocation::SendError(GeoNotifierVector& notifiers,
                            PositionError* error) {
  for (GeoNotifier* notifier : notifiers)
    notifier->RunErrorCallback(error);
}

void Geolocation::SendPosition(GeoNotifierVector& notifiers,
                               Geoposition* position) {
  for (GeoNotifier* notifier : notifiers)
    notifier->RunSuccessCallback(position);
}

void Geolocation::StopTimer(GeoNotifierVector& notifiers) {
  for (GeoNotifier* notifier : notifiers)
    notifier->StopTimer();
}

void Geolocation::StopTimersForOneShots() {
  GeoNotifierVector copy;
  CopyToVector(one_shots_, copy);

  StopTimer(copy);
}

void Geolocation::StopTimersForWatchers() {
  GeoNotifierVector copy;
  watchers_.GetNotifiersVector(copy);

  StopTimer(copy);
}

void Geolocation::StopTimers() {
  StopTimersForOneShots();
  StopTimersForWatchers();
}

void Geolocation::CancelRequests(GeoNotifierVector& notifiers) {
  for (GeoNotifier* notifier : notifiers)
    notifier->SetFatalError(PositionError::Create(
        PositionError::kPositionUnavailable, kFramelessDocumentErrorMessage));
}

void Geolocation::CancelAllRequests() {
  GeoNotifierVector copy;
  CopyToVector(one_shots_, copy);
  CancelRequests(copy);
  watchers_.GetNotifiersVector(copy);
  CancelRequests(copy);
}

void Geolocation::ExtractNotifiersWithCachedPosition(
    GeoNotifierVector& notifiers,
    GeoNotifierVector* cached) {
  GeoNotifierVector non_cached;
  for (GeoNotifier* notifier : notifiers) {
    if (notifier->UseCachedPosition()) {
      if (cached)
        cached->push_back(notifier);
    } else
      non_cached.push_back(notifier);
  }
  swap(notifiers, non_cached);
}

void Geolocation::CopyToSet(const GeoNotifierVector& src,
                            GeoNotifierSet& dest) {
  for (GeoNotifier* notifier : src)
    dest.insert(notifier);
}

void Geolocation::HandleError(PositionError* error) {
  DCHECK(error);

  GeoNotifierVector one_shots_copy;
  CopyToVector(one_shots_, one_shots_copy);

  GeoNotifierVector watchers_copy;
  watchers_.GetNotifiersVector(watchers_copy);

  // Clear the lists before we make the callbacks, to avoid clearing notifiers
  // added by calls to Geolocation methods from the callbacks, and to prevent
  // further callbacks to these notifiers.
  GeoNotifierVector one_shots_with_cached_position;
  one_shots_.clear();
  if (error->IsFatal())
    watchers_.Clear();
  else {
    // Don't send non-fatal errors to notifiers due to receive a cached
    // position.
    ExtractNotifiersWithCachedPosition(one_shots_copy,
                                       &one_shots_with_cached_position);
    ExtractNotifiersWithCachedPosition(watchers_copy, 0);
  }

  SendError(one_shots_copy, error);
  SendError(watchers_copy, error);

  // hasListeners() doesn't distinguish between notifiers due to receive a
  // cached position and those requiring a fresh position. Perform the check
  // before restoring the notifiers below.
  if (!HasListeners())
    StopUpdating();

  // Maintain a reference to the cached notifiers until their timer fires.
  CopyToSet(one_shots_with_cached_position, one_shots_);
}

void Geolocation::MakeSuccessCallbacks() {
  DCHECK(last_position_);

  GeoNotifierVector one_shots_copy;
  CopyToVector(one_shots_, one_shots_copy);

  GeoNotifierVector watchers_copy;
  watchers_.GetNotifiersVector(watchers_copy);

  // Clear the lists before we make the callbacks, to avoid clearing notifiers
  // added by calls to Geolocation methods from the callbacks, and to prevent
  // further callbacks to these notifiers.
  one_shots_.clear();

  SendPosition(one_shots_copy, last_position_);
  SendPosition(watchers_copy, last_position_);

  if (!HasListeners())
    StopUpdating();
}

void Geolocation::PositionChanged() {
  // Stop all currently running timers.
  StopTimers();

  MakeSuccessCallbacks();
}

void Geolocation::StartUpdating(GeoNotifier* notifier) {
  updating_ = true;
  if (notifier->Options().enableHighAccuracy() && !enable_high_accuracy_) {
    enable_high_accuracy_ = true;
    if (geolocation_)
      geolocation_->SetHighAccuracy(true);
  }
  UpdateGeolocationConnection();
}

void Geolocation::StopUpdating() {
  updating_ = false;
  UpdateGeolocationConnection();
  enable_high_accuracy_ = false;
}

void Geolocation::UpdateGeolocationConnection() {
  if (!GetExecutionContext() || !GetPage() || !GetPage()->IsPageVisible() ||
      !updating_) {
    geolocation_.reset();
    disconnected_geolocation_ = true;
    return;
  }
  if (geolocation_)
    return;

  GetFrame()->GetInterfaceProvider().GetInterface(
      mojo::MakeRequest(&geolocation_service_));
  geolocation_service_->CreateGeolocation(
      mojo::MakeRequest(&geolocation_),
      UserGestureIndicator::ProcessingUserGesture());

  geolocation_.set_connection_error_handler(ConvertToBaseCallback(WTF::Bind(
      &Geolocation::OnGeolocationConnectionError, WrapWeakPersistent(this))));
  if (enable_high_accuracy_)
    geolocation_->SetHighAccuracy(true);
  QueryNextPosition();
}

void Geolocation::QueryNextPosition() {
  geolocation_->QueryNextPosition(ConvertToBaseCallback(
      WTF::Bind(&Geolocation::OnPositionUpdated, WrapPersistent(this))));
}

void Geolocation::OnPositionUpdated(
    device::mojom::blink::GeopositionPtr position) {
  disconnected_geolocation_ = false;
  if (position->valid) {
    last_position_ = CreateGeoposition(*position);
    PositionChanged();
  } else {
    HandleError(
        CreatePositionError(position->error_code, position->error_message));
  }
  if (!disconnected_geolocation_)
    QueryNextPosition();
}

void Geolocation::PageVisibilityChanged() {
  UpdateGeolocationConnection();
}

void Geolocation::OnGeolocationConnectionError() {
  StopUpdating();
  // The only reason that we would fail to get a ConnectionError is if we lack
  // sufficient permission.
  PositionError* error = PositionError::Create(PositionError::kPermissionDenied,
                                               kPermissionDeniedErrorMessage);
  error->SetIsFatal(true);
  HandleError(error);
}

}  // namespace blink
