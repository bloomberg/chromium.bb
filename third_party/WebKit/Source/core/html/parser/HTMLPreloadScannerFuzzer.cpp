// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/MediaTypeNames.h"
#include "core/css/MediaValuesCached.h"
#include "core/html/HTMLDocument.h"
#include "core/html/parser/HTMLDocumentParser.h"
#include "core/html/parser/ResourcePreloader.h"
#include "core/html/parser/TextResourceDecoderForFuzzing.h"
#include "platform/testing/BlinkFuzzerTestSupport.h"
#include "platform/testing/FuzzedDataProvider.h"

namespace blink {

std::unique_ptr<CachedDocumentParameters> CachedDocumentParametersForFuzzing(
    FuzzedDataProvider& fuzzed_data) {
  std::unique_ptr<CachedDocumentParameters> document_parameters =
      CachedDocumentParameters::Create();
  document_parameters->do_html_preload_scanning = fuzzed_data.ConsumeBool();
  // TODO(csharrison): How should this be fuzzed?
  document_parameters->default_viewport_min_width = Length();
  document_parameters->viewport_meta_zero_values_quirk =
      fuzzed_data.ConsumeBool();
  document_parameters->viewport_meta_enabled = fuzzed_data.ConsumeBool();
  return document_parameters;
}

class MockResourcePreloader : public ResourcePreloader {
  void Preload(std::unique_ptr<PreloadRequest>,
               const NetworkHintsInterface&) override {}
};

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  FuzzedDataProvider fuzzed_data(data, size);

  HTMLParserOptions options;
  options.script_enabled = fuzzed_data.ConsumeBool();
  options.plugins_enabled = fuzzed_data.ConsumeBool();

  std::unique_ptr<CachedDocumentParameters> document_parameters =
      CachedDocumentParametersForFuzzing(fuzzed_data);

  KURL document_url(kParsedURLString, "http://whatever.test/");

  // Copied from HTMLPreloadScannerTest. May be worthwhile to fuzz.
  MediaValuesCached::MediaValuesCachedData media_data;
  media_data.viewport_width = 500;
  media_data.viewport_height = 600;
  media_data.device_width = 700;
  media_data.device_height = 800;
  media_data.device_pixel_ratio = 2.0;
  media_data.color_bits_per_component = 24;
  media_data.monochrome_bits_per_component = 0;
  media_data.primary_pointer_type = kPointerTypeFine;
  media_data.default_font_size = 16;
  media_data.three_d_enabled = true;
  media_data.media_type = MediaTypeNames::screen;
  media_data.strict_mode = true;
  media_data.display_mode = kWebDisplayModeBrowser;

  MockResourcePreloader preloader;

  std::unique_ptr<HTMLPreloadScanner> scanner = HTMLPreloadScanner::Create(
      options, document_url, std::move(document_parameters), media_data,
      TokenPreloadScanner::ScannerType::kMainDocument);

  TextResourceDecoderForFuzzing decoder(fuzzed_data);
  CString bytes = fuzzed_data.ConsumeRemainingBytes();
  String decoded_bytes = decoder.Decode(bytes.data(), bytes.length());
  scanner->AppendToEnd(decoded_bytes);
  PreloadRequestStream requests = scanner->Scan(document_url, nullptr);
  preloader.TakeAndPreload(requests);
  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}
