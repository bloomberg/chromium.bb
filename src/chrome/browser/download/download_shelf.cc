// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_shelf.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/numerics/math_constants.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_started_animation.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"

using download::DownloadItem;

namespace {

// Delay before we show a transient download.
const int64_t kDownloadShowDelayInSeconds = 2;

// Get the opacity based on |animation_progress|, with values in [0.0, 1.0].
// Range of return value is [0, 255].
int GetOpacity(double animation_progress) {
  DCHECK(animation_progress >= 0 && animation_progress <= 1);

  // How many times to cycle the complete animation. This should be an odd
  // number so that the animation ends faded out.
  static const int kCompleteAnimationCycles = 5;
  double temp =
      ((animation_progress * kCompleteAnimationCycles) + 0.5) * base::kPiDouble;
  temp = sin(temp) / 2 + 0.5;
  return static_cast<int>(255.0 * temp);
}

} // namespace

DownloadShelf::DownloadShelf()
    : should_show_on_unhide_(false),
      is_hidden_(false),
      weak_ptr_factory_(this) {
}

DownloadShelf::~DownloadShelf() {}

// Download progress painting --------------------------------------------------

// static
void DownloadShelf::PaintDownloadProgress(
    gfx::Canvas* canvas,
    const ui::ThemeProvider& theme_provider,
    const base::TimeDelta& progress_time,
    int percent_done) {
  // Draw background (light blue circle).
  cc::PaintFlags bg_flags;
  bg_flags.setStyle(cc::PaintFlags::kFill_Style);
  SkColor indicator_color =
      theme_provider.GetColor(ThemeProperties::COLOR_TAB_THROBBER_SPINNING);
  bg_flags.setColor(SkColorSetA(indicator_color, 0x33));
  bg_flags.setAntiAlias(true);
  const SkScalar kCenterPoint = kProgressIndicatorSize / 2.f;
  SkPath bg;
  bg.addCircle(kCenterPoint, kCenterPoint, kCenterPoint);
  canvas->DrawPath(bg, bg_flags);

  // Calculate progress.
  SkScalar sweep_angle = 0.f;
  // Start at 12 o'clock.
  SkScalar start_pos = SkIntToScalar(270);
  if (percent_done < 0) {
    // For unknown size downloads, draw a 50 degree sweep that moves at
    // 0.08 degrees per millisecond.
    sweep_angle = 50.f;
    start_pos += static_cast<SkScalar>(progress_time.InMilliseconds() * 0.08);
  } else if (percent_done > 0) {
    sweep_angle = static_cast<SkScalar>(360 * percent_done / 100.0);
  }

  // Draw progress.
  SkPath progress;
  progress.addArc(
      SkRect::MakeLTRB(0, 0, kProgressIndicatorSize, kProgressIndicatorSize),
      start_pos, sweep_angle);
  cc::PaintFlags progress_flags;
  progress_flags.setColor(indicator_color);
  progress_flags.setStyle(cc::PaintFlags::kStroke_Style);
  progress_flags.setStrokeWidth(1.7f);
  progress_flags.setAntiAlias(true);
  canvas->DrawPath(progress, progress_flags);
}

// static
void DownloadShelf::PaintDownloadComplete(
    gfx::Canvas* canvas,
    const ui::ThemeProvider& theme_provider,
    double animation_progress) {
  // Start at full opacity, then loop back and forth five times before ending
  // at zero opacity.
  canvas->SaveLayerAlpha(GetOpacity(animation_progress));
  PaintDownloadProgress(canvas, theme_provider, base::TimeDelta(), 100);
  canvas->Restore();
}

// static
void DownloadShelf::PaintDownloadInterrupted(
    gfx::Canvas* canvas,
    const ui::ThemeProvider& theme_provider,
    double animation_progress) {
  // Start at zero opacity, then loop back and forth five times before ending
  // at full opacity.
  PaintDownloadComplete(canvas, theme_provider, 1.0 - animation_progress);
}

void DownloadShelf::AddDownload(DownloadItem* download) {
  DCHECK(download);
  if (DownloadItemModel(download).ShouldRemoveFromShelfWhenComplete()) {
    // If we are going to remove the download from the shelf upon completion,
    // wait a few seconds to see if it completes quickly. If it's a small
    // download, then the user won't have time to interact with it.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DownloadShelf::ShowDownloadById,
                       weak_ptr_factory_.GetWeakPtr(), download->GetId()),
        GetTransientDownloadShowDelay());
  } else {
    ShowDownload(download);
  }
}

void DownloadShelf::Open() {
  if (is_hidden_) {
    should_show_on_unhide_ = true;
    return;
  }
  DoOpen();
}

void DownloadShelf::Close(CloseReason reason) {
  if (is_hidden_) {
    should_show_on_unhide_ = false;
    return;
  }
  DoClose(reason);
}

void DownloadShelf::Hide() {
  if (is_hidden_)
    return;
  is_hidden_ = true;
  if (IsShowing()) {
    should_show_on_unhide_ = true;
    DoHide();
  }
}

void DownloadShelf::Unhide() {
  if (!is_hidden_)
    return;
  is_hidden_ = false;
  if (should_show_on_unhide_) {
    should_show_on_unhide_ = false;
    DoUnhide();
  }
}

base::TimeDelta DownloadShelf::GetTransientDownloadShowDelay() {
  return base::TimeDelta::FromSeconds(kDownloadShowDelayInSeconds);
}

content::DownloadManager* DownloadShelf::GetDownloadManager() {
  return content::BrowserContext::GetDownloadManager(browser()->profile());
}

void DownloadShelf::ShowDownload(DownloadItem* download) {
  if (download->GetState() == DownloadItem::COMPLETE &&
      DownloadItemModel(download).ShouldRemoveFromShelfWhenComplete())
    return;
  if (!DownloadCoreServiceFactory::GetForBrowserContext(
           content::DownloadItemUtils::GetBrowserContext(download))
           ->IsShelfEnabled())
    return;

  if (is_hidden_)
    Unhide();
  Open();
  DoAddDownload(download);

  // browser() can be NULL for tests.
  if (!browser())
    return;

  // Show the download started animation if:
  // - Download started animation is enabled for this download. It is disabled
  //   for "Save As" downloads and extension installs, for example.
  // - The browser has an active visible WebContents. (browser isn't minimized,
  //   or running under a test etc.)
  // - Rich animations are enabled.
  content::WebContents* shelf_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  if (DownloadItemModel(download).ShouldShowDownloadStartedAnimation() &&
      shelf_tab &&
      platform_util::IsVisible(shelf_tab->GetNativeView()) &&
      gfx::Animation::ShouldRenderRichAnimation()) {
    DownloadStartedAnimation::Show(shelf_tab);
  }
}

void DownloadShelf::ShowDownloadById(int32_t download_id) {
  content::DownloadManager* download_manager = GetDownloadManager();
  if (!download_manager)
    return;

  DownloadItem* download = download_manager->GetDownload(download_id);
  if (!download)
    return;

  ShowDownload(download);
}
