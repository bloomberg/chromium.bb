// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_MEDIA_PLAYLIST_H_
#define MEDIA_FORMATS_HLS_MEDIA_PLAYLIST_H_

#include <vector>

#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/playlist.h"
#include "media/formats/hls/tags.h"
#include "media/formats/hls/types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace media::hls {

class MediaSegment;
class MultivariantPlaylist;

class MEDIA_EXPORT MediaPlaylist final : public Playlist {
 public:
  // This structure describes information about partial segments in the
  // playlist.
  struct PartialSegmentInfo {
    // The maximum duration (in seconds) of any partial segment. Each partial
    // segment must be at least 85% of this, except for any where
    // `HasDiscontinuity() == true` or the final partial segment of a parent
    // segment.
    types::DecimalFloatingPoint target_duration;
  };

  MediaPlaylist(const MediaPlaylist&) = delete;
  MediaPlaylist(MediaPlaylist&&);
  MediaPlaylist& operator=(const MediaPlaylist&) = delete;
  MediaPlaylist& operator=(MediaPlaylist&&);
  ~MediaPlaylist();

  // Returns all segments in this playlist, in chronological order. This vector
  // may be copied independently of this Playlist.
  const std::vector<MediaSegment>& GetSegments() const { return segments_; }

  // Returns the target duration (maximum length of any segment, rounded to the
  // nearest integer) for this playlist.
  base::TimeDelta GetTargetDuration() const { return target_duration_; }

  // Returns the sum of the duration of all segments in this playlist.
  // Computed via the 'EXTINF' attribute, so may be slightly longer than the
  // actual duration.
  base::TimeDelta GetComputedDuration() const { return computed_duration_; }

  // Returns the type of this playlist (as specified by the
  // 'EXT-X-PLAYLIST-TYPE' tag). If this is present, then the server must follow
  // the constraints detailed on `PlaylistType` when the playlist is reloaded.
  // If this property is absent, that implies that the server may append new
  // segments to the end or remove old segments from the beginning of this
  // playlist upon reloading.
  absl::optional<PlaylistType> GetPlaylistType() const {
    return playlist_type_;
  }

  // Returns information about partial segments in this playlist. This will be
  // non-empty if this playlist contains at least one partial segment, and may
  // be empty if this playlist contains no partial segments.
  absl::optional<PartialSegmentInfo> GetPartialSegmentInfo() const {
    return partial_segment_info_;
  }

  // Returns whether this playlist contained the 'EXT-X-ENDLIST' tag. This
  // indicates, in the cause of EVENT or live playlists, that no further
  // segments will be appended in future updates.
  bool IsEndList() const { return end_list_; }

  // Indicates that this playlist contained the 'EXT-X-I-FRAMES-ONLY tag.
  // This means that each media segment in this playlist contains a single
  // I-frame, and that the media segment duration should be interpreted as the
  // time between that I-frame and the following one, or the end of the
  // presentation.
  // https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#section-4.4.3.6
  bool IsIFramesOnly() const { return i_frames_only_; }

  // The presence of the EXT-X-MEDIA-SEQUENCE tag is a hint that, in the case of
  // live playlists, media segments may become unavailable after the time this
  // playlist was loaded + the duration of this playlist.
  // https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#:~:text=nominal%20playback%20rate).-,If,-the%20Media%20Playlist
  bool HasMediaSequenceTag() const { return has_media_sequence_tag_; }

  // Attempts to parse the media playlist represented by `source`. `uri` must be
  // a valid, non-empty GURL referring to the URI of this playlist. If this
  // playlist was found through a multivariant playlist, `parent_playlist` must
  // point to that playlist in order to support persistent properties and
  // imported variables. Otherwise, it should be `nullptr`. If `source` is
  // invalid, this returns an error. Otherwise, the parsed playlist is returned.
  static ParseStatus::Or<MediaPlaylist> Parse(
      base::StringPiece source,
      GURL uri,
      const MultivariantPlaylist* parent_playlist);

 private:
  struct CtorArgs;
  explicit MediaPlaylist(CtorArgs);

  base::TimeDelta target_duration_;
  absl::optional<PartialSegmentInfo> partial_segment_info_;
  std::vector<MediaSegment> segments_;
  base::TimeDelta computed_duration_;
  absl::optional<PlaylistType> playlist_type_;
  bool end_list_;
  bool i_frames_only_;
  bool has_media_sequence_tag_;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_MEDIA_PLAYLIST_H_
