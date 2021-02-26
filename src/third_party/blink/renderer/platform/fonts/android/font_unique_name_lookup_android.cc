// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file.h"
#include "base/timer/elapsed_timer.h"

#include "third_party/blink/public/common/font_unique_name_lookup/icu_fold_case_util.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/fonts/android/font_unique_name_lookup_android.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

FontUniqueNameLookupAndroid::~FontUniqueNameLookupAndroid() = default;

void FontUniqueNameLookupAndroid::PrepareFontUniqueNameLookup(
    NotifyFontUniqueNameLookupReady callback) {
  DCHECK(!font_table_matcher_.get());
  DCHECK(RuntimeEnabledFeatures::FontSrcLocalMatchingEnabled());

  pending_callbacks_.push_back(std::move(callback));

  // We bind the service on the first call to PrepareFontUniqueNameLookup. After
  // that we do not need to make additional IPC requests to retrieve the table.
  // The observing callback was added to the list, so all clients will be
  // informed when the lookup table has arrived.
  if (pending_callbacks_.size() > 1)
    return;

  EnsureServiceConnected();

  firmware_font_lookup_service_->GetUniqueNameLookupTable(base::BindOnce(
      &FontUniqueNameLookupAndroid::ReceiveReadOnlySharedMemoryRegion,
      base::Unretained(this)));
}

bool FontUniqueNameLookupAndroid::IsFontUniqueNameLookupReadyForSyncLookup() {
  if (!RuntimeEnabledFeatures::FontSrcLocalMatchingEnabled())
    return true;

  EnsureServiceConnected();

  // If we have the table already, we're ready for sync lookups.
  if (font_table_matcher_.get())
    return true;

  // We have previously determined via IPC whether the table is sync available.
  // Return what we found out before.
  if (sync_available_.has_value())
    return sync_available_.value();

  // If we haven't asked the browser before, probe synchronously - if the table
  // is available on the browser side, we can continue with sync operation.

  bool sync_available_from_mojo = false;
  base::ReadOnlySharedMemoryRegion shared_memory_region;
  firmware_font_lookup_service_->GetUniqueNameLookupTableIfAvailable(
      &sync_available_from_mojo, &shared_memory_region);
  sync_available_ = sync_available_from_mojo;

  if (*sync_available_) {
    // Adopt the shared memory region, do not notify anyone in callbacks as
    // PrepareFontUniqueNameLookup must not have been called yet. Just return
    // true from this function.
    DCHECK_EQ(pending_callbacks_.size(), 0u);
    ReceiveReadOnlySharedMemoryRegion(std::move(shared_memory_region));
  }

  // If it wasn't available synchronously LocalFontFaceSource has to call
  // PrepareFontUniqueNameLookup.
  return *sync_available_;
}

sk_sp<SkTypeface> FontUniqueNameLookupAndroid::MatchUniqueName(
    const String& font_unique_name) {
  if (!IsFontUniqueNameLookupReadyForSyncLookup())
    return nullptr;
  sk_sp<SkTypeface> result_font =
      MatchUniqueNameFromFirmwareFonts(font_unique_name);
  if (result_font)
    return result_font;
  if (RuntimeEnabledFeatures::AndroidDownloadableFontsMatchingEnabled()) {
    return MatchUniqueNameFromDownloadableFonts(font_unique_name);
  } else {
    return nullptr;
  }
}

void FontUniqueNameLookupAndroid::EnsureServiceConnected() {
  if (firmware_font_lookup_service_ &&
      (!RuntimeEnabledFeatures::AndroidDownloadableFontsMatchingEnabled() ||
       android_font_lookup_service_))
    return;

  if (!firmware_font_lookup_service_) {
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        firmware_font_lookup_service_.BindNewPipeAndPassReceiver());
  }

  if (RuntimeEnabledFeatures::AndroidDownloadableFontsMatchingEnabled() &&
      !android_font_lookup_service_) {
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        android_font_lookup_service_.BindNewPipeAndPassReceiver());
  }
}

void FontUniqueNameLookupAndroid::ReceiveReadOnlySharedMemoryRegion(
    base::ReadOnlySharedMemoryRegion shared_memory_region) {
  font_table_matcher_ =
      std::make_unique<FontTableMatcher>(shared_memory_region.Map());
  while (!pending_callbacks_.IsEmpty()) {
    NotifyFontUniqueNameLookupReady callback = pending_callbacks_.TakeFirst();
    std::move(callback).Run();
  }
}

sk_sp<SkTypeface> FontUniqueNameLookupAndroid::MatchUniqueNameFromFirmwareFonts(
    const String& font_unique_name) {
  base::Optional<FontTableMatcher::MatchResult> match_result =
      font_table_matcher_->MatchName(font_unique_name.Utf8().c_str());
  if (!match_result)
    return nullptr;
  return SkTypeface::MakeFromFile(match_result->font_path.c_str(),
                                  match_result->ttc_index);
}

bool FontUniqueNameLookupAndroid::RequestedNameInQueryableFonts(
    const String& font_unique_name) {
  if (!queryable_fonts_) {
    Vector<String> retrieved_fonts;
    android_font_lookup_service_->GetUniqueNameLookupTable(&retrieved_fonts);
    queryable_fonts_ = std::move(retrieved_fonts);
  }
  return queryable_fonts_ && queryable_fonts_->Contains(String::FromUTF8(
                                 IcuFoldCase(font_unique_name.Utf8()).c_str()));
}

sk_sp<SkTypeface>
FontUniqueNameLookupAndroid::MatchUniqueNameFromDownloadableFonts(
    const String& font_unique_name) {
  if (!android_font_lookup_service_.is_bound()) {
    LOG(ERROR) << "Service not connected.";
    return nullptr;
  }

  if (!RequestedNameInQueryableFonts(font_unique_name))
    return nullptr;

  DEFINE_STATIC_LOCAL_IMPL(
      CustomCountHistogram, lookup_latency_histogram_success,
      ("Android.FontLookup.Blink.DLFontsLatencySuccess", 0, 10000000, 50),
      false);
  DEFINE_STATIC_LOCAL_IMPL(
      CustomCountHistogram, lookup_latency_histogram_failure,
      ("Android.FontLookup.Blink.DLFontsLatencySuccess", 0, 10000000, 50),
      false);

  base::File font_file;
  String case_folded_unique_font_name =
      String::FromUTF8(IcuFoldCase(font_unique_name.Utf8()).c_str());

  base::ElapsedTimer elapsed_timer;

  if (!android_font_lookup_service_->MatchLocalFontByUniqueName(
          case_folded_unique_font_name, &font_file)) {
    LOG(ERROR)
        << "Mojo method returned false for case-folded unique font name: "
        << case_folded_unique_font_name;
    lookup_latency_histogram_failure.CountMicroseconds(elapsed_timer.Elapsed());
    return nullptr;
  }

  if (!font_file.IsValid()) {
    LOG(ERROR) << "Received platform font handle invalid, fd: "
               << font_file.GetPlatformFile();
    lookup_latency_histogram_failure.CountMicroseconds(elapsed_timer.Elapsed());
    return nullptr;
  }

  sk_sp<SkData> font_data = SkData::MakeFromFD(font_file.GetPlatformFile());

  if (!font_data || font_data->isEmpty()) {
    LOG(ERROR) << "Received file descriptor has 0 size.";
    lookup_latency_histogram_failure.CountMicroseconds(elapsed_timer.Elapsed());
    return nullptr;
  }

  sk_sp<SkTypeface> return_typeface(SkTypeface::MakeFromData(font_data));

  if (!return_typeface) {
    lookup_latency_histogram_failure.CountMicroseconds(elapsed_timer.Elapsed());
    LOG(ERROR) << "Cannot instantiate SkTypeface from font blob SkData.";
  }

  lookup_latency_histogram_success.CountMicroseconds(elapsed_timer.Elapsed());
  return return_typeface;
}

}  // namespace blink
