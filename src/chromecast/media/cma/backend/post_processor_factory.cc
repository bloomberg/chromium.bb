// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/post_processor_factory.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/scoped_native_library.h"
#include "base/strings/stringprintf.h"
#include "chromecast/media/cma/backend/post_processors/post_processor_wrapper.h"
#include "chromecast/public/media/audio_post_processor2_shlib.h"
#include "chromecast/public/media/audio_post_processor_shlib.h"

namespace chromecast {
namespace media {

namespace {

const char kV1SoCreateFunction[] = "AudioPostProcessorShlib_Create";
const char kV2SoCreateFunction[] = "AudioPostProcessor2Shlib_Create";

}  // namespace

using CreatePostProcessor2Function =
    AudioPostProcessor2* (*)(const std::string&, int);

using CreatePostProcessorFunction = AudioPostProcessor* (*)(const std::string&,
                                                            int);

PostProcessorFactory::PostProcessorFactory() = default;
PostProcessorFactory::~PostProcessorFactory() = default;

std::unique_ptr<AudioPostProcessor2> PostProcessorFactory::CreatePostProcessor(
    const std::string& library_path,
    const std::string& config,
    int channels) {
  libraries_.push_back(std::make_unique<base::ScopedNativeLibrary>(
      base::FilePath(library_path)));
  CHECK(libraries_.back()->is_valid())
      << "Could not open post processing library " << library_path;

  auto v2_create = reinterpret_cast<CreatePostProcessor2Function>(
      libraries_.back()->GetFunctionPointer(kV2SoCreateFunction));
  if (v2_create) {
    return base::WrapUnique(v2_create(config, channels));
  }

  auto v1_create = reinterpret_cast<CreatePostProcessorFunction>(
      libraries_.back()->GetFunctionPointer(kV1SoCreateFunction));

  DCHECK(v1_create) << "Could not find " << kV1SoCreateFunction << "() in "
                    << library_path;

  LOG(WARNING) << "[Deprecated]: AudioPostProcessor will be deprecated soon."
               << " Please update " << library_path
               << " to AudioPostProcessor2.";

  return std::make_unique<AudioPostProcessorWrapper>(
      base::WrapUnique(v1_create(config, channels)), channels);
}

// static
bool PostProcessorFactory::IsPostProcessorLibrary(
    const base::FilePath& library_path) {
  base::ScopedNativeLibrary library(library_path);
  DCHECK(library.is_valid()) << "Could not open library " << library_path;

  // Check if library is V1 post processor.
  void* v1_create = library.GetFunctionPointer(kV1SoCreateFunction);
  if (v1_create) {
    return true;
  }

  // Check if library is V2 post processor.
  void* v2_create = library.GetFunctionPointer(kV2SoCreateFunction);
  if (v2_create) {
    return true;
  }

  return false;
}

}  // namespace media
}  // namespace chromecast
