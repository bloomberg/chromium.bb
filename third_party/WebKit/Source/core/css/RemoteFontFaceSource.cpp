// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/RemoteFontFaceSource.h"

#include "core/css/CSSCustomFontData.h"
#include "core/css/CSSFontFace.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrameClient.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/Histogram.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontCustomPlatformData.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontSelector.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoadPriority.h"
#include "platform/network/NetworkStateNotifier.h"
#include "platform/wtf/Time.h"
#include "public/platform/TaskType.h"
#include "public/platform/WebEffectiveConnectionType.h"

namespace blink {

RemoteFontFaceSource::RemoteFontFaceSource(CSSFontFace* css_font_face,
                                           FontSelector* font_selector,
                                           FontDisplay display)
    : face_(css_font_face),
      font_selector_(font_selector),
      display_(display),
      period_(display == kFontDisplaySwap ? kSwapPeriod : kBlockPeriod),
      is_intervention_triggered_(false) {
  DCHECK(face_);
  if (ShouldTriggerWebFontsIntervention()) {
    is_intervention_triggered_ = true;
    period_ = kSwapPeriod;
  }
}

RemoteFontFaceSource::~RemoteFontFaceSource() = default;

void RemoteFontFaceSource::Dispose() {
  ClearResource();
  PruneTable();
}

bool RemoteFontFaceSource::IsLoading() const {
  return GetResource() && GetResource()->IsLoading();
}

bool RemoteFontFaceSource::IsLoaded() const {
  return !GetResource();
}

bool RemoteFontFaceSource::IsValid() const {
  return GetResource() || custom_font_data_;
}

void RemoteFontFaceSource::NotifyFinished(Resource* resource) {
  FontResource* font = ToFontResource(resource);
  histograms_.RecordRemoteFont(font);

  custom_font_data_ = font->GetCustomFontData();

  // FIXME: Provide more useful message such as OTS rejection reason.
  // See crbug.com/97467
  if (font->GetStatus() == ResourceStatus::kDecodeError) {
    font_selector_->GetExecutionContext()->AddConsoleMessage(
        ConsoleMessage::Create(
            kOtherMessageSource, kWarningMessageLevel,
            "Failed to decode downloaded font: " + font->Url().ElidedString()));
    if (font->OtsParsingMessage().length() > 1) {
      font_selector_->GetExecutionContext()->AddConsoleMessage(
          ConsoleMessage::Create(
              kOtherMessageSource, kWarningMessageLevel,
              "OTS parsing error: " + font->OtsParsingMessage()));
    }
  }

  ClearResource();

  PruneTable();
  if (face_->FontLoaded(this))
    font_selector_->FontFaceInvalidated();
}

void RemoteFontFaceSource::FontLoadShortLimitExceeded(FontResource*) {
  if (IsLoaded())
    return;

  if (display_ == kFontDisplayFallback)
    SwitchToSwapPeriod();
  else if (display_ == kFontDisplayOptional)
    SwitchToFailurePeriod();
}

void RemoteFontFaceSource::FontLoadLongLimitExceeded(FontResource*) {
  if (IsLoaded())
    return;

  if (display_ == kFontDisplayBlock ||
      (!is_intervention_triggered_ && display_ == kFontDisplayAuto))
    SwitchToSwapPeriod();
  else if (display_ == kFontDisplayFallback)
    SwitchToFailurePeriod();

  histograms_.LongLimitExceeded();
}

void RemoteFontFaceSource::SwitchToSwapPeriod() {
  DCHECK_EQ(period_, kBlockPeriod);
  period_ = kSwapPeriod;

  PruneTable();
  if (face_->DidBecomeVisibleFallback(this))
    font_selector_->FontFaceInvalidated();

  histograms_.RecordFallbackTime();
}

void RemoteFontFaceSource::SwitchToFailurePeriod() {
  if (period_ == kBlockPeriod)
    SwitchToSwapPeriod();
  DCHECK_EQ(period_, kSwapPeriod);
  period_ = kFailurePeriod;
}

bool RemoteFontFaceSource::ShouldTriggerWebFontsIntervention() {
  if (!font_selector_->GetExecutionContext()->IsDocument())
    return false;

  WebEffectiveConnectionType connection_type =
      ToDocument(font_selector_->GetExecutionContext())
          ->GetFrame()
          ->Client()
          ->GetEffectiveConnectionType();

  bool network_is_slow =
      WebEffectiveConnectionType::kTypeOffline <= connection_type &&
      connection_type <= WebEffectiveConnectionType::kType3G;

  return network_is_slow && display_ == kFontDisplayAuto;
}

bool RemoteFontFaceSource::IsLowPriorityLoadingAllowedForRemoteFont() const {
  return is_intervention_triggered_;
}

scoped_refptr<SimpleFontData> RemoteFontFaceSource::CreateFontData(
    const FontDescription& font_description,
    const FontSelectionCapabilities& font_selection_capabilities) {
  if (period_ == kFailurePeriod || !IsValid())
    return nullptr;
  if (!IsLoaded())
    return CreateLoadingFallbackFontData(font_description);
  DCHECK(custom_font_data_);

  histograms_.RecordFallbackTime();

  return SimpleFontData::Create(
      custom_font_data_->GetFontPlatformData(
          font_description.EffectiveFontSize(),
          font_description.IsSyntheticBold(),
          font_description.IsSyntheticItalic(),
          font_description.GetFontSelectionRequest(),
          font_selection_capabilities, font_description.Orientation(),
          font_description.VariationSettings()),
      CustomFontData::Create());
}

scoped_refptr<SimpleFontData>
RemoteFontFaceSource::CreateLoadingFallbackFontData(
    const FontDescription& font_description) {
  // This temporary font is not retained and should not be returned.
  FontCachePurgePreventer font_cache_purge_preventer;
  SimpleFontData* temporary_font =
      FontCache::GetFontCache()->GetNonRetainedLastResortFallbackFont(
          font_description);
  if (!temporary_font) {
    NOTREACHED();
    return nullptr;
  }
  scoped_refptr<CSSCustomFontData> css_font_data = CSSCustomFontData::Create(
      this, period_ == kBlockPeriod ? CSSCustomFontData::kInvisibleFallback
                                    : CSSCustomFontData::kVisibleFallback);
  return SimpleFontData::Create(temporary_font->PlatformData(), css_font_data);
}

void RemoteFontFaceSource::BeginLoadIfNeeded() {
  if (IsLoaded())
    return;
  DCHECK(GetResource());

  FontResource* font = ToFontResource(GetResource());
  if (font->StillNeedsLoad()) {
    if (font->IsLowPriorityLoadingAllowedForRemoteFont()) {
      font_selector_->GetExecutionContext()->AddConsoleMessage(
          ConsoleMessage::Create(
              kOtherMessageSource, kInfoMessageLevel,
              "Slow network is detected. See "
              "https://www.chromestatus.com/feature/5636954674692096 for more "
              "details. Fallback font will be used while loading: " +
                  font->Url().ElidedString()));

      // Set the loading priority to VeryLow only when all other clients agreed
      // that this font is not required for painting the text.
      font->DidChangePriority(ResourceLoadPriority::kVeryLow, 0);
    }
    if (font_selector_->GetExecutionContext()->Fetcher()->StartLoad(font)) {
      // Start timers only when load is actually started asynchronously.
      if (!IsLoaded()) {
        font->StartLoadLimitTimers(
            font_selector_->GetExecutionContext()
                ->GetTaskRunner(TaskType::kUnspecedLoading)
                .get());
      }
      histograms_.LoadStarted();
    }
  }

  face_->DidBeginLoad();
}

void RemoteFontFaceSource::Trace(blink::Visitor* visitor) {
  visitor->Trace(face_);
  visitor->Trace(font_selector_);
  CSSFontFaceSource::Trace(visitor);
  FontResourceClient::Trace(visitor);
}

void RemoteFontFaceSource::FontLoadHistograms::LoadStarted() {
  if (!load_start_time_)
    load_start_time_ = CurrentTimeMS();
}

void RemoteFontFaceSource::FontLoadHistograms::FallbackFontPainted(
    DisplayPeriod period) {
  if (period == kBlockPeriod && !blank_paint_time_)
    blank_paint_time_ = CurrentTimeMS();
}

void RemoteFontFaceSource::FontLoadHistograms::LongLimitExceeded() {
  is_long_limit_exceeded_ = true;
  MaySetDataSource(kFromNetwork);
}

void RemoteFontFaceSource::FontLoadHistograms::RecordFallbackTime() {
  if (blank_paint_time_ <= 0)
    return;
  int duration = static_cast<int>(CurrentTimeMS() - blank_paint_time_);
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram,
                                  blank_text_shown_time_histogram,
                                  ("WebFont.BlankTextShownTime", 0, 10000, 50));
  blank_text_shown_time_histogram.Count(duration);
  blank_paint_time_ = -1;
}

void RemoteFontFaceSource::FontLoadHistograms::RecordRemoteFont(
    const FontResource* font) {
  MaySetDataSource(DataSourceForLoadFinish(font));

  DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, cache_hit_histogram,
                                  ("WebFont.CacheHit", kCacheHitEnumMax));
  cache_hit_histogram.Count(DataSourceMetricsValue());

  if (data_source_ == kFromDiskCache || data_source_ == kFromNetwork) {
    DCHECK_NE(load_start_time_, 0);
    int duration = static_cast<int>(CurrentTimeMS() - load_start_time_);
    RecordLoadTimeHistogram(font, duration);

    enum { kCORSFail, kCORSSuccess, kCORSEnumMax };
    int cors_value =
        font->IsSameOriginOrCORSSuccessful() ? kCORSSuccess : kCORSFail;
    DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, cors_histogram,
                                    ("WebFont.CORSSuccess", kCORSEnumMax));
    cors_histogram.Count(cors_value);
  }
}

void RemoteFontFaceSource::FontLoadHistograms::MaySetDataSource(
    DataSource data_source) {
  if (data_source_ != kFromUnknown)
    return;
  // Classify as memory cache hit if |load_start_time_| is not set, i.e.
  // this RemoteFontFaceSource instance didn't trigger FontResource
  // loading.
  if (load_start_time_ == 0)
    data_source_ = kFromMemoryCache;
  else
    data_source_ = data_source;
}

void RemoteFontFaceSource::FontLoadHistograms::RecordLoadTimeHistogram(
    const FontResource* font,
    int duration) {
  CHECK_NE(kFromUnknown, data_source_);

  if (font->ErrorOccurred()) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, load_error_histogram,
        ("WebFont.DownloadTime.LoadError", 0, 10000, 50));
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, missed_cache_load_error_histogram,
        ("WebFont.MissedCache.DownloadTime.LoadError", 0, 10000, 50));
    load_error_histogram.Count(duration);
    if (data_source_ == kFromNetwork)
      missed_cache_load_error_histogram.Count(duration);
    return;
  }

  unsigned size = font->EncodedSize();
  if (size < 10 * 1024) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, under10k_histogram,
        ("WebFont.DownloadTime.0.Under10KB", 0, 10000, 50));
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, missed_cache_under10k_histogram,
        ("WebFont.MissedCache.DownloadTime.0.Under10KB", 0, 10000, 50));
    under10k_histogram.Count(duration);
    if (data_source_ == kFromNetwork)
      missed_cache_under10k_histogram.Count(duration);
    return;
  }
  if (size < 50 * 1024) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, under50k_histogram,
        ("WebFont.DownloadTime.1.10KBTo50KB", 0, 10000, 50));
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, missed_cache_under50k_histogram,
        ("WebFont.MissedCache.DownloadTime.1.10KBTo50KB", 0, 10000, 50));
    under50k_histogram.Count(duration);
    if (data_source_ == kFromNetwork)
      missed_cache_under50k_histogram.Count(duration);
    return;
  }
  if (size < 100 * 1024) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, under100k_histogram,
        ("WebFont.DownloadTime.2.50KBTo100KB", 0, 10000, 50));
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, missed_cache_under100k_histogram,
        ("WebFont.MissedCache.DownloadTime.2.50KBTo100KB", 0, 10000, 50));
    under100k_histogram.Count(duration);
    if (data_source_ == kFromNetwork)
      missed_cache_under100k_histogram.Count(duration);
    return;
  }
  if (size < 1024 * 1024) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, under1mb_histogram,
        ("WebFont.DownloadTime.3.100KBTo1MB", 0, 10000, 50));
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, missed_cache_under1mb_histogram,
        ("WebFont.MissedCache.DownloadTime.3.100KBTo1MB", 0, 10000, 50));
    under1mb_histogram.Count(duration);
    if (data_source_ == kFromNetwork)
      missed_cache_under1mb_histogram.Count(duration);
    return;
  }
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, over1mb_histogram,
      ("WebFont.DownloadTime.4.Over1MB", 0, 10000, 50));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, missed_cache_over1mb_histogram,
      ("WebFont.MissedCache.DownloadTime.4.Over1MB", 0, 10000, 50));
  over1mb_histogram.Count(duration);
  if (data_source_ == kFromNetwork)
    missed_cache_over1mb_histogram.Count(duration);
}

RemoteFontFaceSource::FontLoadHistograms::CacheHitMetrics
RemoteFontFaceSource::FontLoadHistograms::DataSourceMetricsValue() {
  switch (data_source_) {
    case kFromDataURL:
      return kDataUrl;
    case kFromMemoryCache:
      return kMemoryHit;
    case kFromDiskCache:
      return kDiskHit;
    case kFromNetwork:
      return kMiss;
    case kFromUnknown:
    // Fall through.
    default:
      NOTREACHED();
  }
  return kMiss;
}

}  // namespace blink
