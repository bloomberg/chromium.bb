// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/RemoteFontFaceSource.h"

#include "core/css/CSSCustomFontData.h"
#include "core/css/CSSFontFace.h"
#include "core/css/CSSFontSelector.h"
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/LocalFrameClient.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/Histogram.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontCustomPlatformData.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoadPriority.h"
#include "platform/network/NetworkStateNotifier.h"
#include "platform/wtf/CurrentTime.h"
#include "public/platform/WebEffectiveConnectionType.h"

namespace blink {

RemoteFontFaceSource::RemoteFontFaceSource(FontResource* font,
                                           CSSFontSelector* font_selector,
                                           FontDisplay display)
    : font_(font),
      font_selector_(font_selector),
      display_(display),
      period_(display == kFontDisplaySwap ? kSwapPeriod : kBlockPeriod),
      histograms_(font->Url().ProtocolIsData()
                      ? FontLoadHistograms::kFromDataURL
                      : font->IsLoaded() ? FontLoadHistograms::kFromMemoryCache
                                         : FontLoadHistograms::kFromUnknown,
                  display_),
      is_intervention_triggered_(false) {
  if (ShouldTriggerWebFontsIntervention()) {
    is_intervention_triggered_ = true;
    period_ = kSwapPeriod;
  }

  // Note: this may call notifyFinished() and clear font_.
  font_->AddClient(this);
}

RemoteFontFaceSource::~RemoteFontFaceSource() {}

void RemoteFontFaceSource::Dispose() {
  if (font_) {
    font_->RemoveClient(this);
    font_ = nullptr;
  }
  PruneTable();
}

void RemoteFontFaceSource::PruneTable() {
  if (font_data_table_.IsEmpty())
    return;

  for (const auto& item : font_data_table_) {
    SimpleFontData* font_data = item.value.get();
    if (font_data && font_data->GetCustomFontData())
      font_data->GetCustomFontData()->ClearFontFaceSource();
  }
  font_data_table_.clear();
}

bool RemoteFontFaceSource::IsLoading() const {
  return font_ && font_->IsLoading();
}

bool RemoteFontFaceSource::IsLoaded() const {
  return !font_;
}

bool RemoteFontFaceSource::IsValid() const {
  return font_ || custom_font_data_;
}

void RemoteFontFaceSource::NotifyFinished(Resource* unused_resource) {
  DCHECK_EQ(unused_resource, font_);
  histograms_.MaySetDataSource(font_->GetResponse().WasCached()
                                   ? FontLoadHistograms::kFromDiskCache
                                   : FontLoadHistograms::kFromNetwork);
  histograms_.RecordRemoteFont(font_.Get(), is_intervention_triggered_);
  histograms_.FontLoaded(!font_->IsSameOriginOrCORSSuccessful(),
                         font_->GetStatus() == ResourceStatus::kLoadError,
                         is_intervention_triggered_);

  custom_font_data_ = font_->GetCustomFontData();

  // FIXME: Provide more useful message such as OTS rejection reason.
  // See crbug.com/97467
  if (font_->GetStatus() == ResourceStatus::kDecodeError &&
      font_selector_->GetDocument()) {
    font_selector_->GetDocument()->AddConsoleMessage(ConsoleMessage::Create(
        kOtherMessageSource, kWarningMessageLevel,
        "Failed to decode downloaded font: " + font_->Url().ElidedString()));
    if (font_->OtsParsingMessage().length() > 1)
      font_selector_->GetDocument()->AddConsoleMessage(ConsoleMessage::Create(
          kOtherMessageSource, kWarningMessageLevel,
          "OTS parsing error: " + font_->OtsParsingMessage()));
  }

  CSSFontFace::LoadFinishReason load_finish_reason =
      font_->GetResourceError().IsCancellation()
          ? CSSFontFace::LoadFinishReason::WasCancelled
          : CSSFontFace::LoadFinishReason::NormalFinish;
  font_->RemoveClient(this);
  font_ = nullptr;

  PruneTable();
  if (face_) {
    font_selector_->FontFaceInvalidated();
    face_->FontLoaded(this, load_finish_reason);
  }
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

  histograms_.LongLimitExceeded(is_intervention_triggered_);
}

void RemoteFontFaceSource::SwitchToSwapPeriod() {
  DCHECK_EQ(period_, kBlockPeriod);
  period_ = kSwapPeriod;

  PruneTable();
  if (face_) {
    font_selector_->FontFaceInvalidated();
    face_->DidBecomeVisibleFallback(this);
  }

  histograms_.RecordFallbackTime();
}

void RemoteFontFaceSource::SwitchToFailurePeriod() {
  if (period_ == kBlockPeriod)
    SwitchToSwapPeriod();
  DCHECK_EQ(period_, kSwapPeriod);
  period_ = kFailurePeriod;
}

bool RemoteFontFaceSource::ShouldTriggerWebFontsIntervention() {
  if (histograms_.GetDataSource() == FontLoadHistograms::kFromMemoryCache ||
      histograms_.GetDataSource() == FontLoadHistograms::kFromDataURL)
    return false;

  WebEffectiveConnectionType connection_type =
      font_selector_->GetDocument()
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

RefPtr<SimpleFontData> RemoteFontFaceSource::CreateFontData(
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

RefPtr<SimpleFontData> RemoteFontFaceSource::CreateLoadingFallbackFontData(
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
  RefPtr<CSSCustomFontData> css_font_data = CSSCustomFontData::Create(
      this, period_ == kBlockPeriod ? CSSCustomFontData::kInvisibleFallback
                                    : CSSCustomFontData::kVisibleFallback);
  return SimpleFontData::Create(temporary_font->PlatformData(), css_font_data);
}

void RemoteFontFaceSource::BeginLoadIfNeeded() {
  if (IsLoaded())
    return;
  DCHECK(font_);

  if (font_selector_->GetDocument() && font_->StillNeedsLoad()) {
    if (!font_->Url().ProtocolIsData() && !font_->IsLoaded() &&
        display_ == kFontDisplayAuto &&
        font_->IsLowPriorityLoadingAllowedForRemoteFont()) {
      // Set the loading priority to VeryLow since this font is not required
      // for painting the text.
      font_->DidChangePriority(kResourceLoadPriorityVeryLow, 0);
    }
    if (font_selector_->GetDocument()->Fetcher()->StartLoad(font_)) {
      // Start timers only when load is actually started asynchronously.
      if (!font_->IsLoaded()) {
        font_->StartLoadLimitTimers(
            TaskRunnerHelper::Get(TaskType::kUnspecedLoading,
                                  font_selector_->GetDocument())
                .get());
      }
      histograms_.LoadStarted();
    }
    if (is_intervention_triggered_) {
      font_selector_->GetDocument()->AddConsoleMessage(
          ConsoleMessage::Create(kOtherMessageSource, kInfoMessageLevel,
                                 "Slow network is detected. Fallback font will "
                                 "be used while loading: " +
                                     font_->Url().ElidedString()));
    }
  }

  if (face_)
    face_->DidBeginLoad();
}

DEFINE_TRACE(RemoteFontFaceSource) {
  visitor->Trace(font_);
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

void RemoteFontFaceSource::FontLoadHistograms::FontLoaded(
    bool is_cors_failed,
    bool load_error,
    bool is_intervention_triggered) {
  if (!is_long_limit_exceeded_ && font_display_ == kFontDisplayAuto &&
      !is_cors_failed && !load_error) {
    RecordInterventionResult(is_intervention_triggered);
  }
}

void RemoteFontFaceSource::FontLoadHistograms::LongLimitExceeded(
    bool is_intervention_triggered) {
  is_long_limit_exceeded_ = true;
  MaySetDataSource(kFromNetwork);
  if (font_display_ == kFontDisplayAuto)
    RecordInterventionResult(is_intervention_triggered);
}

void RemoteFontFaceSource::FontLoadHistograms::RecordFallbackTime() {
  if (blank_paint_time_ <= 0)
    return;
  int duration = static_cast<int>(CurrentTimeMS() - blank_paint_time_);
  DEFINE_STATIC_LOCAL(CustomCountHistogram, blank_text_shown_time_histogram,
                      ("WebFont.BlankTextShownTime", 0, 10000, 50));
  blank_text_shown_time_histogram.Count(duration);
  blank_paint_time_ = -1;
}

void RemoteFontFaceSource::FontLoadHistograms::RecordRemoteFont(
    const FontResource* font,
    bool is_intervention_triggered) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, cache_hit_histogram,
                      ("WebFont.CacheHit", kCacheHitEnumMax));
  cache_hit_histogram.Count(DataSourceMetricsValue());

  if (data_source_ == kFromDiskCache || data_source_ == kFromNetwork) {
    DCHECK_NE(load_start_time_, 0);
    int duration = static_cast<int>(CurrentTimeMS() - load_start_time_);
    RecordLoadTimeHistogram(font, duration, is_intervention_triggered);

    enum { kCORSFail, kCORSSuccess, kCORSEnumMax };
    int cors_value =
        font->IsSameOriginOrCORSSuccessful() ? kCORSSuccess : kCORSFail;
    DEFINE_STATIC_LOCAL(EnumerationHistogram, cors_histogram,
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
    int duration,
    bool is_intervention_triggered) {
  CHECK_NE(kFromUnknown, data_source_);

  if (font->ErrorOccurred()) {
    DEFINE_STATIC_LOCAL(CustomCountHistogram, load_error_histogram,
                        ("WebFont.DownloadTime.LoadError", 0, 10000, 50));
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, missed_cache_load_error_histogram,
        ("WebFont.MissedCache.DownloadTime.LoadError", 0, 10000, 50));
    load_error_histogram.Count(duration);
    if (data_source_ == kFromNetwork)
      missed_cache_load_error_histogram.Count(duration);
    return;
  }

  unsigned size = font->EncodedSize();
  if (size < 10 * 1024) {
    DEFINE_STATIC_LOCAL(CustomCountHistogram, under10k_histogram,
                        ("WebFont.DownloadTime.0.Under10KB", 0, 10000, 50));
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, missed_cache_under10k_histogram,
        ("WebFont.MissedCache.DownloadTime.0.Under10KB", 0, 10000, 50));
    under10k_histogram.Count(duration);
    if (data_source_ == kFromNetwork)
      missed_cache_under10k_histogram.Count(duration);
    return;
  }
  if (size < 50 * 1024) {
    DEFINE_STATIC_LOCAL(CustomCountHistogram, under50k_histogram,
                        ("WebFont.DownloadTime.1.10KBTo50KB", 0, 10000, 50));
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, missed_cache_under50k_histogram,
        ("WebFont.MissedCache.DownloadTime.1.10KBTo50KB", 0, 10000, 50));
    // Breakdowns metrics to understand WebFonts intervention.
    // Now we only cover this 10KBto50KB range because 70% of requests are
    // covered in this range, and having metrics for all size cases cost.
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram,
        missed_cache_and_intervention_triggered_under50k_histogram,
        ("WebFont.MissedCacheAndInterventionTriggered."
         "DownloadTime.1.10KBTo50KB",
         0, 10000, 50));
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram,
        missed_cache_and_intervention_not_triggered_under50k_histogram,
        ("WebFont.MissedCacheAndInterventionNotTriggered."
         "DownloadTime.1.10KBTo50KB",
         0, 10000, 50));
    under50k_histogram.Count(duration);
    if (data_source_ == kFromNetwork) {
      missed_cache_under50k_histogram.Count(duration);
      if (is_intervention_triggered)
        missed_cache_and_intervention_triggered_under50k_histogram.Count(
            duration);
      else
        missed_cache_and_intervention_not_triggered_under50k_histogram.Count(
            duration);
    }
    return;
  }
  if (size < 100 * 1024) {
    DEFINE_STATIC_LOCAL(CustomCountHistogram, under100k_histogram,
                        ("WebFont.DownloadTime.2.50KBTo100KB", 0, 10000, 50));
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, missed_cache_under100k_histogram,
        ("WebFont.MissedCache.DownloadTime.2.50KBTo100KB", 0, 10000, 50));
    under100k_histogram.Count(duration);
    if (data_source_ == kFromNetwork)
      missed_cache_under100k_histogram.Count(duration);
    return;
  }
  if (size < 1024 * 1024) {
    DEFINE_STATIC_LOCAL(CustomCountHistogram, under1mb_histogram,
                        ("WebFont.DownloadTime.3.100KBTo1MB", 0, 10000, 50));
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, missed_cache_under1mb_histogram,
        ("WebFont.MissedCache.DownloadTime.3.100KBTo1MB", 0, 10000, 50));
    under1mb_histogram.Count(duration);
    if (data_source_ == kFromNetwork)
      missed_cache_under1mb_histogram.Count(duration);
    return;
  }
  DEFINE_STATIC_LOCAL(CustomCountHistogram, over1mb_histogram,
                      ("WebFont.DownloadTime.4.Over1MB", 0, 10000, 50));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, missed_cache_over1mb_histogram,
      ("WebFont.MissedCache.DownloadTime.4.Over1MB", 0, 10000, 50));
  over1mb_histogram.Count(duration);
  if (data_source_ == kFromNetwork)
    missed_cache_over1mb_histogram.Count(duration);
}

void RemoteFontFaceSource::FontLoadHistograms::RecordInterventionResult(
    bool is_triggered) {
  CHECK_NE(kFromUnknown, data_source_);

  // interventionResult takes 0-3 values.
  int intervention_result = 0;
  if (is_long_limit_exceeded_)
    intervention_result |= 1 << 0;
  if (is_triggered)
    intervention_result |= 1 << 1;
  const int kBoundary = 1 << 2;

  DEFINE_STATIC_LOCAL(EnumerationHistogram, intervention_histogram,
                      ("WebFont.InterventionResult", kBoundary));
  DEFINE_STATIC_LOCAL(EnumerationHistogram, missed_cache_intervention_histogram,
                      ("WebFont.InterventionResult.MissedCache", kBoundary));
  intervention_histogram.Count(intervention_result);
  if (data_source_ == kFromNetwork)
    missed_cache_intervention_histogram.Count(intervention_result);
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
