/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2012-2013 Intel Corporation. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ViewportDescription_h
#define ViewportDescription_h

#include "core/CoreExport.h"
#include "core/frame/PageScaleConstraints.h"
#include "platform/Length.h"
#include "platform/geometry/FloatSize.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class LocalFrame;

struct CORE_EXPORT ViewportDescription {
  DISALLOW_NEW();

  enum Type {
    // These are ordered in increasing importance.
    kUserAgentStyleSheet,
    kHandheldFriendlyMeta,
    kMobileOptimizedMeta,
    kViewportMeta,
    kAuthorStyleSheet
  } type;

  enum {
    kValueAuto = -1,
    kValueDeviceWidth = -2,
    kValueDeviceHeight = -3,
    kValuePortrait = -4,
    kValueLandscape = -5,
    kValueDeviceDPI = -6,
    kValueLowDPI = -7,
    kValueMediumDPI = -8,
    kValueHighDPI = -9,
    kValueExtendToZoom = -10
  };

  ViewportDescription(Type type = kUserAgentStyleSheet)
      : type(type),
        zoom(kValueAuto),
        min_zoom(kValueAuto),
        max_zoom(kValueAuto),
        user_zoom(true),
        orientation(kValueAuto),
        deprecated_target_density_dpi(kValueAuto),
        zoom_is_explicit(false),
        min_zoom_is_explicit(false),
        max_zoom_is_explicit(false),
        user_zoom_is_explicit(false) {}

  // All arguments are in CSS units.
  PageScaleConstraints Resolve(const FloatSize& initial_viewport_size,
                               Length legacy_fallback_width) const;

  Length min_width;
  Length max_width;
  Length min_height;
  Length max_height;
  float zoom;
  float min_zoom;
  float max_zoom;
  bool user_zoom;
  float orientation;
  float deprecated_target_density_dpi;  // Only used for Android WebView

  // Whether the computed value was explicitly specified rather than being
  // inferred.
  bool zoom_is_explicit;
  bool min_zoom_is_explicit;
  bool max_zoom_is_explicit;
  bool user_zoom_is_explicit;

  bool operator==(const ViewportDescription& other) const {
    // Used for figuring out whether to reset the viewport or not,
    // thus we are not taking type into account.
    return min_width == other.min_width && max_width == other.max_width &&
           min_height == other.min_height && max_height == other.max_height &&
           zoom == other.zoom && min_zoom == other.min_zoom &&
           max_zoom == other.max_zoom && user_zoom == other.user_zoom &&
           orientation == other.orientation &&
           deprecated_target_density_dpi ==
               other.deprecated_target_density_dpi &&
           zoom_is_explicit == other.zoom_is_explicit &&
           min_zoom_is_explicit == other.min_zoom_is_explicit &&
           max_zoom_is_explicit == other.max_zoom_is_explicit &&
           user_zoom_is_explicit == other.user_zoom_is_explicit;
  }

  bool operator!=(const ViewportDescription& other) const {
    return !(*this == other);
  }

  bool IsLegacyViewportType() const {
    return type >= kHandheldFriendlyMeta && type <= kViewportMeta;
  }
  bool IsMetaViewportType() const { return type == kViewportMeta; }
  bool IsSpecifiedByAuthor() const { return type != kUserAgentStyleSheet; }
  bool MatchesHeuristicsForGpuRasterization() const;

  // Reports UMA stat on whether the page is considered mobile or desktop and
  // what kind of mobile it is. Applies only to Android, must only be called
  // once per page load.
  void ReportMobilePageStats(const LocalFrame*) const;

 private:
  enum Direction { kHorizontal, kVertical };
  static float ResolveViewportLength(const Length&,
                                     const FloatSize& initial_viewport_size,
                                     Direction);
};

}  // namespace blink

#endif  // ViewportDescription_h
