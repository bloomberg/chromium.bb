// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace gfx {

namespace {

// Caches font render params and updates them on system notifications.
class CachedFontRenderParams : public gfx::SingletonHwnd::Observer {
 public:
  static CachedFontRenderParams* GetInstance() {
    return Singleton<CachedFontRenderParams>::get();
  }

  const FontRenderParams& GetParams() {
    if (params_)
      return *params_;

    params_.reset(new FontRenderParams());
    params_->antialiasing = false;
    params_->subpixel_positioning = false;
    params_->autohinter = false;
    params_->use_bitmaps = false;
    params_->hinting = FontRenderParams::HINTING_MEDIUM;
    params_->subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_NONE;

    BOOL enabled = false;
    if (SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &enabled, 0) && enabled) {
      params_->antialiasing = true;
      params_->subpixel_positioning = true;

      UINT type = 0;
      if (SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &type, 0) &&
          type == FE_FONTSMOOTHINGCLEARTYPE) {
        params_->subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_RGB;
      }
    }
    gfx::SingletonHwnd::GetInstance()->AddObserver(this);
    return *params_;
  }

 private:
  friend struct DefaultSingletonTraits<CachedFontRenderParams>;

  CachedFontRenderParams() {}
  virtual ~CachedFontRenderParams() {
    // Can't remove the SingletonHwnd observer here since SingletonHwnd may have
    // been destroyed already (both singletons).
  }

  virtual void OnWndProc(HWND hwnd,
                         UINT message,
                         WPARAM wparam,
                         LPARAM lparam) OVERRIDE {
    if (message == WM_SETTINGCHANGE) {
      params_.reset();
      gfx::SingletonHwnd::GetInstance()->RemoveObserver(this);
    }
  }

  scoped_ptr<FontRenderParams> params_;

  DISALLOW_COPY_AND_ASSIGN(CachedFontRenderParams);
};

}  // namespace

const FontRenderParams& GetDefaultFontRenderParams() {
  return CachedFontRenderParams::GetInstance()->GetParams();
}

const FontRenderParams& GetDefaultWebKitFontRenderParams() {
  return GetDefaultFontRenderParams();
}

FontRenderParams GetCustomFontRenderParams(
    bool for_web_contents,
    const std::vector<std::string>* family_list,
    const int* pixel_size,
    const int* point_size,
    const int* style,
    std::string* family_out) {
  NOTIMPLEMENTED();
  return GetDefaultFontRenderParams();
}

}  // namespace gfx
