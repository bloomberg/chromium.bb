// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_enums.h"
#include "url/gurl.h"

namespace gfx {
struct VectorIcon;
}

namespace permissions {

// Describes the interface a feature making permission requests should
// implement. A class of this type is registered with the permission request
// manager to receive updates about the result of the permissions request
// from the bubble or infobar. It should live until it is unregistered or until
// RequestFinished is called.
// Note that no particular guarantees are made about what exact UI surface
// is presented to the user. The delegate may be coalesced with other bubble
// requests, or depending on the situation, not shown at all.
class PermissionRequest {
 public:
#if defined(OS_ANDROID)
  // On Android, icons are represented with an IDR_ identifier.
  typedef int IconId;
#else
  // On desktop, we use a vector icon.
  typedef const gfx::VectorIcon& IconId;
#endif

  PermissionRequest();
  virtual ~PermissionRequest() {}

  // The icon to use next to the message text fragment in the permission bubble.
  virtual IconId GetIconId() const = 0;

#if defined(OS_ANDROID)
  // Returns the full prompt text for this permission. This is currently only
  // used on Android.
  virtual base::string16 GetMessageText() const = 0;

  // Returns the title of this permission as text when the permission request is
  // displayed as a quiet prompt. Only used on Android. By default it returns
  // the same value as |GetTitleText| unless overridden.
  virtual base::string16 GetQuietTitleText() const;

  // Returns the full prompt text for this permission as text when the
  // permission request is displayed as a quiet prompt. Only used on Android. By
  // default it returns the same value as |GetMessageText| unless overridden.
  virtual base::string16 GetQuietMessageText() const;
#endif

#if !defined(OS_ANDROID)
  // Returns the short text for the chip button related to this permission.
  virtual base::Optional<base::string16> GetChipText() const;
#endif

  // Returns the shortened prompt text for this permission. The permission
  // bubble may coalesce different requests, and if it does, this text will
  // be displayed next to an image and indicate the user grants the permission.
  virtual base::string16 GetMessageTextFragment() const = 0;

  // Get the origin on whose behalf this permission request is being made.
  virtual GURL GetOrigin() const = 0;

  // Called when the user has granted the requested permission.
  // If is_one_time is true the permission will last until all tabs of a given
  // |origin| are closed or navigated away from. The permission will
  // automatically expire after 1 day.
  virtual void PermissionGranted(bool is_one_time) = 0;

  // Called when the user has denied the requested permission.
  virtual void PermissionDenied() = 0;

  // Called when the user has cancelled the permission request. This
  // corresponds to a denial, but is segregated in case the context needs to
  // be able to distinguish between an active refusal or an implicit refusal.
  virtual void Cancelled() = 0;

  // The UI this request was associated with was answered by the user.
  // It is safe for the request to be deleted at this point -- it will receive
  // no further message from the permission request system. This method will
  // eventually be called on every request which is not unregistered.
  // It is ok to call this method without actually resolving the request via
  // PermissionGranted(), PermissionDenied() or Canceled(). However, it will not
  // resolve the javascript promise from the requesting origin.
  virtual void RequestFinished() = 0;

  // Used to record UMA metrics for permission requests.
  virtual PermissionRequestType GetPermissionRequestType() const = 0;

  // Used to record UMA for whether requests are associated with a user gesture.
  // To keep things simple this metric is only recorded for the most popular
  // request types.
  virtual PermissionRequestGestureType GetGestureType() const;

  // Used on Android to determine what Android OS permissions are needed for
  // this permission request.
  virtual ContentSettingsType GetContentSettingsType() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(PermissionRequest);
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_H_
