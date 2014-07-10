// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/gfx/switches.h"

#include <fontconfig/fontconfig.h>

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/gfx/linux_font_delegate.h"
#endif

namespace gfx {

namespace {

bool SubpixelPositioningRequested(bool renderer) {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      renderer ? switches::kEnableWebkitTextSubpixelPositioning
               : switches::kEnableBrowserTextSubpixelPositioning);
}

// Initializes |params| with the system's default settings. |renderer| is true
// when setting WebKit renderer defaults.
void LoadDefaults(FontRenderParams* params, bool renderer) {
  // For non-GTK builds (read: Aura), just use reasonable hardcoded values.
  params->antialiasing = true;
  params->autohinter = true;
  params->use_bitmaps = true;
  params->hinting = FontRenderParams::HINTING_SLIGHT;

  // Fetch default subpixel rendering settings from FontConfig.
  FcPattern* pattern = FcPatternCreate();
  FcConfigSubstitute(NULL, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);
  FcResult result;
  FcPattern* match = FcFontMatch(0, pattern, &result);
  DCHECK(match);
  int fc_rgba = FC_RGBA_RGB;
  FcPatternGetInteger(match, FC_RGBA, 0, &fc_rgba);
  FcPatternDestroy(pattern);
  FcPatternDestroy(match);

  switch (fc_rgba) {
    case FC_RGBA_RGB:
      params->subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_RGB;
      break;
    case FC_RGBA_BGR:
      params->subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_BGR;
      break;
    case FC_RGBA_VRGB:
      params->subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_VRGB;
      break;
    case FC_RGBA_VBGR:
      params->subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_VBGR;
      break;
    default:
      params->subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_NONE;
  }

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  const LinuxFontDelegate* delegate = LinuxFontDelegate::instance();
  if (delegate) {
    params->antialiasing = delegate->UseAntialiasing();
    params->hinting = delegate->GetHintingStyle();
    params->subpixel_rendering = delegate->GetSubpixelRenderingStyle();
  }
#endif

  params->subpixel_positioning = SubpixelPositioningRequested(renderer);

  // To enable subpixel positioning, we need to disable hinting.
  if (params->subpixel_positioning)
    params->hinting = FontRenderParams::HINTING_NONE;
}

}  // namespace

const FontRenderParams& GetDefaultFontRenderParams() {
  static bool loaded_defaults = false;
  static FontRenderParams default_params;
  if (!loaded_defaults)
    LoadDefaults(&default_params, /* renderer */ false);
  loaded_defaults = true;
  return default_params;
}

const FontRenderParams& GetDefaultWebKitFontRenderParams() {
  static bool loaded_defaults = false;
  static FontRenderParams default_params;
  if (!loaded_defaults)
    LoadDefaults(&default_params, /* renderer */ true);
  loaded_defaults = true;
  return default_params;
}

bool GetDefaultWebkitSubpixelPositioning() {
  return SubpixelPositioningRequested(true);
}

}  // namespace gfx
