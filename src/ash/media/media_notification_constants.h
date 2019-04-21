// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MEDIA_MEDIA_NOTIFICATION_CONSTANTS_H_
#define ASH_MEDIA_MEDIA_NOTIFICATION_CONSTANTS_H_

#include "ash/ash_export.h"

namespace ash {

// The custom view type that should be set on media session notifications.
ASH_EXPORT extern const char kMediaSessionNotificationCustomViewType[];

// The notifier ID associated with the media session service.
ASH_EXPORT extern const char kMediaSessionNotifierId[];

// The minimum size in px that the media session artwork can be to be displayed
// in the notification.
ASH_EXPORT extern const int kMediaSessionNotificationArtworkMinSize;

// The desired size in px for the media session artwork to be displayed in the
// notification. The media session service will try and select artwork closest
// to this size.
ASH_EXPORT extern const int kMediaSessionNotificationArtworkDesiredSize;

}  // namespace ash

#endif  // ASH_MEDIA_MEDIA_NOTIFICATION_CONSTANTS_H_
