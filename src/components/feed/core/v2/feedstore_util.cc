// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feedstore_util.h"

#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/consistency_token.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/public/stream_type.h"

namespace feedstore {
using feed::LocalActionId;
using feed::StreamType;

base::StringPiece StreamId(const StreamType& stream_type) {
  if (stream_type.IsForYou())
    return kForYouStreamId;
  DCHECK(stream_type.IsWebFeed());
  return kFollowStreamId;
}

StreamType StreamTypeFromId(base::StringPiece id) {
  if (id == kForYouStreamId)
    return feed::kForYouStream;
  if (id == kFollowStreamId)
    return feed::kWebFeedStream;
  return {};
}

int64_t ToTimestampMillis(base::Time t) {
  return (t - base::Time::UnixEpoch()).InMilliseconds();
}

base::Time FromTimestampMillis(int64_t millis) {
  return base::Time::UnixEpoch() + base::Milliseconds(millis);
}

void SetLastAddedTime(base::Time t, feedstore::StreamData& data) {
  data.set_last_added_time_millis(ToTimestampMillis(t));
}

base::Time GetLastAddedTime(const feedstore::StreamData& data) {
  return FromTimestampMillis(data.last_added_time_millis());
}

base::Time GetSessionIdExpiryTime(const Metadata& metadata) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Milliseconds(metadata.session_id().expiry_time_ms()));
}

void SetSessionId(Metadata& metadata,
                  std::string token,
                  base::Time expiry_time) {
  Metadata::SessionID* session_id = metadata.mutable_session_id();
  session_id->set_token(std::move(token));
  session_id->set_expiry_time_ms(
      expiry_time.ToDeltaSinceWindowsEpoch().InMilliseconds());
}

void SetContentLifetime(
    feedstore::Metadata& metadata,
    const StreamType& stream_type,
    feedstore::Metadata::StreamMetadata::ContentLifetime content_lifetime) {
  feedstore::Metadata::StreamMetadata& stream_metadata =
      feedstore::MetadataForStream(metadata, stream_type);
  *stream_metadata.mutable_content_lifetime() = std::move(content_lifetime);
}

void MaybeUpdateSessionId(Metadata& metadata,
                          absl::optional<std::string> token) {
  if (token && metadata.session_id().token() != *token) {
    base::Time expiry_time =
        token->empty()
            ? base::Time()
            : base::Time::Now() + feed::GetFeedConfig().session_id_max_age;
    SetSessionId(metadata, *token, expiry_time);
  }
}

absl::optional<Metadata> MaybeUpdateConsistencyToken(
    const feedstore::Metadata& metadata,
    const feedwire::ConsistencyToken& token) {
  if (token.has_token() && metadata.consistency_token() != token.token()) {
    auto metadata_copy = metadata;
    metadata_copy.set_consistency_token(token.token());
    return metadata_copy;
  }
  return absl::nullopt;
}

LocalActionId GetNextActionId(Metadata& metadata) {
  uint32_t id = metadata.next_action_id();
  // Never use 0, as that's an invalid LocalActionId.
  if (id == 0)
    ++id;
  metadata.set_next_action_id(id + 1);
  return LocalActionId(id);
}

const Metadata::StreamMetadata* FindMetadataForStream(
    const Metadata& metadata,
    const StreamType& stream_type) {
  base::StringPiece id = StreamId(stream_type);
  for (const auto& sm : metadata.stream_metadata()) {
    if (sm.stream_id() == id)
      return &sm;
  }
  return nullptr;
}

Metadata::StreamMetadata& MetadataForStream(Metadata& metadata,
                                            const StreamType& stream_type) {
  const Metadata::StreamMetadata* existing =
      FindMetadataForStream(metadata, stream_type);
  if (existing)
    return *const_cast<Metadata::StreamMetadata*>(existing);
  Metadata::StreamMetadata* sm = metadata.add_stream_metadata();
  sm->set_stream_id(std::string(StreamId(stream_type)));
  return *sm;
}

void SetStreamViewContentIds(Metadata& metadata,
                             const StreamType& stream_type,
                             const feed::ContentIdSet& content_ids) {
  Metadata::StreamMetadata& stream_metadata =
      MetadataForStream(metadata, stream_type);
  stream_metadata.clear_view_content_ids();
  stream_metadata.mutable_view_content_ids()->Add(content_ids.values().begin(),
                                                  content_ids.values().end());
}

bool IsKnownStale(const Metadata& metadata, const StreamType& stream_type) {
  const Metadata::StreamMetadata* sm =
      FindMetadataForStream(metadata, stream_type);
  return sm ? sm->is_known_stale() : false;
}

base::Time GetLastFetchTime(const Metadata& metadata,
                            const feed::StreamType& stream_type) {
  const Metadata::StreamMetadata* sm =
      FindMetadataForStream(metadata, stream_type);
  return sm ? FromTimestampMillis(sm->last_fetch_time_millis()) : base::Time();
}

void SetLastFetchTime(Metadata& metadata,
                      const StreamType& stream_type,
                      const base::Time& fetch_time) {
  Metadata::StreamMetadata& stream_metadata =
      MetadataForStream(metadata, stream_type);
  stream_metadata.set_last_fetch_time_millis(ToTimestampMillis(fetch_time));
}

feedstore::Metadata MakeMetadata(const std::string& gaia) {
  feedstore::Metadata md;
  md.set_stream_schema_version(feed::FeedStore::kCurrentStreamSchemaVersion);
  md.set_gaia(gaia);
  return md;
}

absl::optional<Metadata> SetStreamViewContentIds(
    const Metadata& metadata,
    const StreamType& stream_type,
    const feed::ContentIdSet& content_ids) {
  absl::optional<Metadata> result;
  if (!(GetViewContentIds(metadata, stream_type) == content_ids)) {
    result = metadata;
    SetStreamViewContentIds(*result, stream_type, content_ids);
  }
  return result;
}

feed::ContentIdSet GetContentIds(const StreamData& stream_data) {
  return feed::ContentIdSet{
      {stream_data.content_ids().begin(), stream_data.content_ids().end()}};
}
feed::ContentIdSet GetViewContentIds(const Metadata& metadata,
                                     const StreamType& stream_type) {
  const Metadata::StreamMetadata* stream_metadata =
      FindMetadataForStream(metadata, stream_type);
  if (stream_metadata) {
    return feed::ContentIdSet({stream_metadata->view_content_ids().begin(),
                               stream_metadata->view_content_ids().end()});
  }
  return {};
}

}  // namespace feedstore
