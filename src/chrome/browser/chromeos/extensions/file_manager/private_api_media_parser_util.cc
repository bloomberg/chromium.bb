// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_media_parser_util.h"

#include <memory>

#include "base/check.h"
#include "base/values.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "net/base/mime_util.h"

namespace {

template <class T>
void SetValueScopedPtr(T value, std::unique_ptr<T>* destination) {
  DCHECK(destination);
  if (value >= 0)
    destination->reset(new T(value));
}

template <>
void SetValueScopedPtr(std::string value,
                       std::unique_ptr<std::string>* destination) {
  DCHECK(destination);
  if (!value.empty())
    *destination = std::make_unique<std::string>(std::move(value));
}

void ChangeAudioMimePrefixToVideo(std::string* mime_type) {
  const std::string audio_type("audio/*");
  if (net::MatchesMimeType(audio_type, *mime_type))
    mime_type->replace(0, audio_type.length() - 1, "video/");
}

}  // namespace

namespace extensions {

namespace api {

namespace file_manager_private {

std::unique_ptr<base::DictionaryValue> MojoMediaMetadataToValue(
    chrome::mojom::MediaMetadataPtr metadata) {
  DCHECK(metadata);

  file_manager_private::MediaMetadata media_metadata;
  media_metadata.mime_type = std::move(metadata->mime_type);

  // Video files have dimensions.
  if (metadata->height >= 0 && metadata->width >= 0) {
    ChangeAudioMimePrefixToVideo(&media_metadata.mime_type);
    SetValueScopedPtr(metadata->height, &media_metadata.height);
    SetValueScopedPtr(metadata->width, &media_metadata.width);
  }

  SetValueScopedPtr(metadata->duration, &media_metadata.duration);
  SetValueScopedPtr(metadata->rotation, &media_metadata.rotation);
  SetValueScopedPtr(std::move(metadata->artist), &media_metadata.artist);
  SetValueScopedPtr(std::move(metadata->album), &media_metadata.album);
  SetValueScopedPtr(std::move(metadata->comment), &media_metadata.comment);
  SetValueScopedPtr(std::move(metadata->copyright), &media_metadata.copyright);
  SetValueScopedPtr(metadata->disc, &media_metadata.disc);
  SetValueScopedPtr(std::move(metadata->genre), &media_metadata.genre);
  SetValueScopedPtr(std::move(metadata->language), &media_metadata.language);
  SetValueScopedPtr(std::move(metadata->title), &media_metadata.title);
  SetValueScopedPtr(metadata->track, &media_metadata.track);

  for (const chrome::mojom::MediaStreamInfoPtr& info : metadata->raw_tags) {
    file_manager_private::StreamInfo stream_info;
    stream_info.type = std::move(info->type);
    base::DictionaryValue* value = nullptr;
    info->additional_properties.GetAsDictionary(&value);
    stream_info.tags.additional_properties.Swap(value);
    media_metadata.raw_tags.push_back(std::move(stream_info));
  }

  return media_metadata.ToValue();
}

}  // namespace file_manager_private

}  // namespace api

}  // namespace extensions
