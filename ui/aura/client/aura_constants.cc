// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/aura_constants.h"

#include "ui/aura/window_property.h"

DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(AURA_EXPORT, bool)
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(AURA_EXPORT, ui::ModalType)
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(AURA_EXPORT, gfx::Rect*)
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(AURA_EXPORT, ui::InputMethod*)
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(AURA_EXPORT, ui::WindowShowState)

namespace aura {
namespace client {
namespace {

// Alphabetical sort.

const WindowProperty<bool> kAlwaysOnTopProp = {false};
const WindowProperty<bool> kAnimationsDisabledProp = {false};
const WindowProperty<ui::ModalType> kModalProp = {ui::MODAL_TYPE_NONE};
const WindowProperty<gfx::Rect*> kRestoreBoundsProp = {NULL};
const WindowProperty<ui::InputMethod*> kRootWindowInputMethodProp = {NULL};
const WindowProperty<ui::WindowShowState>
    kShowStateProp = {ui::SHOW_STATE_DEFAULT};

}  // namespace

// Alphabetical sort.

const WindowProperty<bool>* const kAlwaysOnTopKey = &kAlwaysOnTopProp;
const WindowProperty<bool>* const
    kAnimationsDisabledKey = &kAnimationsDisabledProp;
const WindowProperty<ui::ModalType>* const kModalKey = &kModalProp;
const WindowProperty<gfx::Rect*>* const kRestoreBoundsKey = &kRestoreBoundsProp;
const WindowProperty<ui::InputMethod*>* const
    kRootWindowInputMethodKey = &kRootWindowInputMethodProp;
const WindowProperty<ui::WindowShowState>* const
    kShowStateKey = &kShowStateProp;

}  // namespace client
}  // namespace aura
