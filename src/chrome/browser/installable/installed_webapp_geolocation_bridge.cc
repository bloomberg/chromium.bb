// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installed_webapp_geolocation_bridge.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/android/chrome_jni_headers/InstalledWebappGeolocationBridge_jni.h"
#include "chrome/browser/installable/installed_webapp_geolocation_context.h"
#include "services/device/public/cpp/geolocation/geoposition.h"

namespace {

const char kLocationUpdateHistogramName[] =
    "TrustedWebActivity.LocationUpdateErrorCode";

// Do not modify or reuse existing entries; they are used in a UMA histogram.
// Please edit TrustedWebActivityLocationErrorCode in the enums.xml if a value
// is added.
// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.chrome.browser.browserservices.constants)
enum class LocationUpdateError {
  // There was no error.
  kNone = 0,
  // Geoposition could not be determined, i.e. error from the TWA client app.
  kLocationError = 1,
  // Invalid position.
  kInvalidPosition = 2,
  // Trusted web activity service not found or does not handle the request.
  kNoTwa = 3,
  // NOTE: Add entries only immediately above this line.
  kMaxValue = kNoTwa
};

}  // namespace

InstalledWebappGeolocationBridge::InstalledWebappGeolocationBridge(
    mojo::PendingReceiver<Geolocation> receiver,
    const GURL& origin,
    InstalledWebappGeolocationContext* context)
    : context_(context),
      origin_(origin),
      high_accuracy_(false),
      has_position_to_report_(false),
      receiver_(this, std::move(receiver)) {
  DCHECK(context_);
  receiver_.set_disconnect_handler(
      base::BindOnce(&InstalledWebappGeolocationBridge::OnConnectionError,
                     base::Unretained(this)));
}

InstalledWebappGeolocationBridge::~InstalledWebappGeolocationBridge() {
  StopUpdates();
}

void InstalledWebappGeolocationBridge::StartListeningForUpdates() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (java_ref_.is_null()) {
    base::android::ScopedJavaLocalRef<jstring> j_origin =
        base::android::ConvertUTF8ToJavaString(
            env, origin_.DeprecatedGetOriginAsURL().spec());
    java_ref_.Reset(Java_InstalledWebappGeolocationBridge_create(
        env, reinterpret_cast<intptr_t>(this), j_origin));
  }
  Java_InstalledWebappGeolocationBridge_start(env, java_ref_, high_accuracy_);
}

void InstalledWebappGeolocationBridge::StopUpdates() {
  if (!java_ref_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_InstalledWebappGeolocationBridge_stopAndDestroy(env, java_ref_);
    java_ref_.Reset();
  }
}

void InstalledWebappGeolocationBridge::SetHighAccuracy(bool high_accuracy) {
  high_accuracy_ = high_accuracy;

  if (device::ValidateGeoposition(position_override_)) {
    OnLocationUpdate(position_override_);
    return;
  }

  StartListeningForUpdates();
}

void InstalledWebappGeolocationBridge::QueryNextPosition(
    QueryNextPositionCallback callback) {
  if (!position_callback_.is_null()) {
    DVLOG(1) << "Overlapped call to QueryNextPosition!";
    OnConnectionError();  // Simulate a connection error.
    return;
  }

  position_callback_ = std::move(callback);

  if (has_position_to_report_)
    ReportCurrentPosition();
}

void InstalledWebappGeolocationBridge::SetOverride(
    const device::mojom::Geoposition& position) {
  if (!position_callback_.is_null())
    ReportCurrentPosition();

  position_override_ = position;
  StopUpdates();

  OnLocationUpdate(position_override_);
}

void InstalledWebappGeolocationBridge::ClearOverride() {
  position_override_ = device::mojom::Geoposition();
  StartListeningForUpdates();
}

void InstalledWebappGeolocationBridge::OnConnectionError() {
  context_->OnConnectionError(this);

  // The above call deleted this instance, so the only safe thing to do is
  // return.
}

void InstalledWebappGeolocationBridge::OnLocationUpdate(
    const device::mojom::Geoposition& position) {
  DCHECK(context_);

  current_position_ = position;
  current_position_.valid = device::ValidateGeoposition(position);
  has_position_to_report_ = true;

  if (!position_callback_.is_null())
    ReportCurrentPosition();
}

void InstalledWebappGeolocationBridge::ReportCurrentPosition() {
  DCHECK(position_callback_);
  std::move(position_callback_).Run(current_position_.Clone());
  has_position_to_report_ = false;
}

void InstalledWebappGeolocationBridge::OnNewLocationAvailable(
    JNIEnv* env,
    jdouble latitude,
    jdouble longitude,
    jdouble time_stamp,
    jboolean has_altitude,
    jdouble altitude,
    jboolean has_accuracy,
    jdouble accuracy,
    jboolean has_heading,
    jdouble heading,
    jboolean has_speed,
    jdouble speed) {
  device::mojom::Geoposition position;
  position.latitude = latitude;
  position.longitude = longitude;
  position.timestamp = base::Time::FromDoubleT(time_stamp);
  if (has_altitude)
    position.altitude = altitude;
  if (has_accuracy)
    position.accuracy = accuracy;
  if (has_heading)
    position.heading = heading;
  if (has_speed)
    position.speed = speed;

  // If position is invalid, mark it as unavailable.
  if (!device::ValidateGeoposition(position)) {
    position.error_code =
        device::mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE;
    base::UmaHistogramEnumeration(kLocationUpdateHistogramName,
                                  LocationUpdateError::kInvalidPosition);
  } else {
    base::UmaHistogramEnumeration(kLocationUpdateHistogramName,
                                  LocationUpdateError::kNone);
  }

  OnLocationUpdate(position);
}

void InstalledWebappGeolocationBridge::OnNewErrorAvailable(JNIEnv* env,
                                                           jstring message) {
  base::UmaHistogramEnumeration(kLocationUpdateHistogramName,
                                LocationUpdateError::kLocationError);

  device::mojom::Geoposition position_error;
  position_error.error_code =
      device::mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE;
  position_error.error_message =
      base::android::ConvertJavaStringToUTF8(env, message);

  OnLocationUpdate(position_error);
}
