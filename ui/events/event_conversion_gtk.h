// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_CONVERSION_GTK_H_
#define UI_EVENTS_EVENT_CONVERSION_GTK_H_

#include <gtk/gtk.h>

#include "ui/events/events_export.h"

namespace ui {

EVENTS_EXPORT int GdkModifierToEventFlag(GdkModifierType gdk_modifier);

EVENTS_EXPORT GdkModifierType EventFlagToGdkModifier(int event_flag);

}  // namespace ui

#endif  // UI_EVENTS_EVENT_CONVERSION_GTK_H_
