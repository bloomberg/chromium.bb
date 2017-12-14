// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/text_detection_impl_win.h"

#include <windows.globalization.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_version.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shape_detection/text_detection_impl.h"

namespace shape_detection {

using ABI::Windows::Globalization::ILanguageFactory;
using ABI::Windows::Media::Ocr::IOcrEngineStatics;
using base::win::GetActivationFactory;
using base::win::ScopedHString;

// static
void TextDetectionImpl::Create(mojom::TextDetectionRequest request) {
  // OcrEngine class is only available in Win 10 onwards (v10.0.10240.0) that
  // documents in
  // https://docs.microsoft.com/en-us/uwp/api/windows.media.ocr.ocrengine.
  if (base::win::GetVersion() < base::win::VERSION_WIN10) {
    DVLOG(1) << "Optical character recognition not supported before Windows 10";
    return;
  }
  DCHECK_GE(base::win::OSInfo::GetInstance()->version_number().build, 10240);

  // Loads functions dynamically at runtime to prevent library dependencies.
  if (!(base::win::ResolveCoreWinRTDelayload() &&
        ScopedHString::ResolveCoreWinRTStringDelayload())) {
    DLOG(ERROR) << "Failed loading functions from combase.dll";
    return;
  }

  // Text Detection specification only supports Latin-1 text as documented in
  // https://wicg.github.io/shape-detection-api/text.html#text-detection-api.
  // TODO(junwei.fu): https://crbug.com/794097 consider supporting other Latin
  // script language.
  ScopedHString language_hstring = ScopedHString::Create("en");
  if (!language_hstring.is_valid())
    return;

  Microsoft::WRL::ComPtr<ILanguageFactory> language_factory;
  HRESULT hr =
      GetActivationFactory<ILanguageFactory,
                           RuntimeClass_Windows_Globalization_Language>(
          &language_factory);
  if (FAILED(hr)) {
    DLOG(ERROR) << "ILanguage factory failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  Microsoft::WRL::ComPtr<ABI::Windows::Globalization::ILanguage> language;
  hr = language_factory->CreateLanguage(language_hstring.get(), &language);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Create language failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  Microsoft::WRL::ComPtr<IOcrEngineStatics> engine_factory;
  hr = GetActivationFactory<IOcrEngineStatics,
                            RuntimeClass_Windows_Media_Ocr_OcrEngine>(
      &engine_factory);
  if (FAILED(hr)) {
    DLOG(ERROR) << "IOcrEngineStatics factory failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  boolean is_supported = false;
  hr = engine_factory->IsLanguageSupported(language.Get(), &is_supported);
  if (FAILED(hr) || !is_supported)
    return;

  Microsoft::WRL::ComPtr<IOcrEngine> ocr_engine;
  hr = engine_factory->TryCreateFromLanguage(language.Get(), &ocr_engine);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Create engine failed from language: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  mojo::MakeStrongBinding(
      std::make_unique<TextDetectionImplWin>(std::move(ocr_engine)),
      std::move(request));
}

TextDetectionImplWin::TextDetectionImplWin(
    Microsoft::WRL::ComPtr<IOcrEngine> ocr_engine)
    : ocr_engine_(std::move(ocr_engine)) {
  DCHECK(ocr_engine_);
}

TextDetectionImplWin::~TextDetectionImplWin() = default;

void TextDetectionImplWin::Detect(const SkBitmap& bitmap,
                                  DetectCallback callback) {
  std::move(callback).Run(std::vector<mojom::TextDetectionResultPtr>());
}

}  // namespace shape_detection
