// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/cast_audio_json.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"

namespace chromecast {
namespace media {

namespace {

void ReadFileRunCallback(CastAudioJsonProvider::TuningChangedCallback callback,
                         const base::FilePath& path,
                         bool error) {
  DCHECK(callback);

  std::string contents;
  base::ReadFileToString(path, &contents);
  std::unique_ptr<base::Value> value =
      base::JSONReader::ReadDeprecated(contents);
  if (value) {
    callback.Run(std::move(value));
    return;
  }
  LOG(ERROR) << "Unable to parse JSON in " << path;
}

}  // namespace

#if defined(OS_FUCHSIA)
const char kCastAudioJsonFilePath[] = "/system/data/cast_audio.json";
#else
const char kCastAudioJsonFilePath[] = "/etc/cast_audio.json";
#endif
const char kCastAudioJsonFileName[] = "cast_audio.json";

// static
base::FilePath CastAudioJson::GetFilePath() {
  base::FilePath tuning_path = CastAudioJson::GetFilePathForTuning();
  if (base::PathExists(tuning_path)) {
    return tuning_path;
  }

  return CastAudioJson::GetReadOnlyFilePath();
}

// static
base::FilePath CastAudioJson::GetReadOnlyFilePath() {
  return base::FilePath(kCastAudioJsonFilePath);
}

// static
base::FilePath CastAudioJson::GetFilePathForTuning() {
  return base::GetHomeDir().Append(kCastAudioJsonFileName);
}

CastAudioJsonProviderImpl::CastAudioJsonProviderImpl() {
  if (base::ThreadPoolInstance::Get()) {
    cast_audio_watcher_ = base::SequenceBound<FileWatcher>(
        base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                         base::TaskPriority::LOWEST}));
  }
}

CastAudioJsonProviderImpl::~CastAudioJsonProviderImpl() = default;

std::unique_ptr<base::Value> CastAudioJsonProviderImpl::GetCastAudioConfig() {
  std::string contents;
  base::ReadFileToString(CastAudioJson::GetFilePath(), &contents);
  return base::JSONReader::ReadDeprecated(contents);
}

void CastAudioJsonProviderImpl::SetTuningChangedCallback(
    TuningChangedCallback callback) {
  if (cast_audio_watcher_) {
    cast_audio_watcher_.Post(FROM_HERE, &FileWatcher::SetTuningChangedCallback,
                             std::move(callback));
  }
}

CastAudioJsonProviderImpl::FileWatcher::FileWatcher() = default;
CastAudioJsonProviderImpl::FileWatcher::~FileWatcher() = default;

void CastAudioJsonProviderImpl::FileWatcher::SetTuningChangedCallback(
    TuningChangedCallback callback) {
  watcher_.Watch(
      CastAudioJson::GetFilePathForTuning(), false /* recursive */,
      base::BindRepeating(&ReadFileRunCallback, std::move(callback)));
}

}  // namespace media
}  // namespace chromecast
