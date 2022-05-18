// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/media_playlist.h"

#include <initializer_list>
#include <string>
#include <utility>

#include "base/strings/string_piece.h"
#include "media/formats/hls/media_playlist_test_builder.h"
#include "media/formats/hls/multivariant_playlist.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/tags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace media::hls {

namespace {

MultivariantPlaylist CreateMultivariantPlaylist(
    std::initializer_list<base::StringPiece> lines,
    GURL uri = GURL("http://localhost/multi_playlist.m3u8")) {
  std::string source;
  for (auto line : lines) {
    source.append(line.data(), line.size());
    source.append("\n");
  }

  // Parse the given source. Failure here isn't supposed to be part of the test,
  // so use a CHECK.
  auto result = MultivariantPlaylist::Parse(source, std::move(uri));
  CHECK(result.has_value());
  return std::move(result).value();
}

}  // namespace

TEST(HlsMediaPlaylistTest, XDiscontinuityTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");
  builder.ExpectPlaylist(HasVersion, 1);
  builder.ExpectPlaylist(HasTargetDuration, base::Seconds(10));

  // Default discontinuity state is false
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, false);

  builder.AppendLine("#EXT-X-DISCONTINUITY");
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, true);

  // The discontinuity tag does not apply to subsequent segments
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, false);

  // The discontinuity tag may only appear once per segment
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-DISCONTINUITY");
    fork.AppendLine("#EXT-X-DISCONTINUITY");
    fork.AppendLine("#EXTINF:9.9,\t");
    fork.AppendLine("video.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasDiscontinuity, true);
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }

  builder.ExpectOk();
}

TEST(HlsMediaPlaylistTest, XGapTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");
  builder.ExpectPlaylist(HasVersion, 1);
  builder.ExpectPlaylist(HasTargetDuration, base::Seconds(10));

  // Default gap state is false
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(IsGap, false);

  builder.AppendLine("#EXT-X-GAP");
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(IsGap, true);

  // The gap tag does not apply to subsequent segments
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(IsGap, false);

  // The gap tag may only appear once per segment
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-GAP");
    fork.AppendLine("#EXT-X-GAP");
    fork.AppendLine("#EXTINF:9.9,\t");
    fork.AppendLine("video.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(IsGap, true);
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }

  builder.ExpectOk();
}

TEST(HlsMediaPlaylistTest, Segments) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");
  builder.AppendLine("#EXT-X-VERSION:5");
  builder.ExpectPlaylist(HasVersion, 5);
  builder.ExpectPlaylist(HasTargetDuration, base::Seconds(10));

  builder.AppendLine("#EXTINF:9.2,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, false);
  builder.ExpectSegment(HasDuration, 9.2);
  builder.ExpectSegment(HasUri, GURL("http://localhost/video.ts"));
  builder.ExpectSegment(IsGap, false);
  builder.ExpectSegment(HasMediaSequenceNumber, 0);

  // Segments without #EXTINF tags are not allowed
  {
    auto fork = builder;
    fork.AppendLine("foobar.ts");
    fork.ExpectError(ParseStatusCode::kMediaSegmentMissingInfTag);
  }

  builder.AppendLine("#EXTINF:9.3,foo");
  builder.AppendLine("#EXT-X-DISCONTINUITY");
  builder.AppendLine("foo.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, true);
  builder.ExpectSegment(HasDuration, 9.3);
  builder.ExpectSegment(IsGap, false);
  builder.ExpectSegment(HasUri, GURL("http://localhost/foo.ts"));
  builder.ExpectSegment(HasMediaSequenceNumber, 1);

  builder.AppendLine("#EXTINF:9.2,bar");
  builder.AppendLine("http://foo/bar.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, false);
  builder.ExpectSegment(HasDuration, 9.2);
  builder.ExpectSegment(IsGap, false);
  builder.ExpectSegment(HasUri, GURL("http://foo/bar.ts"));
  builder.ExpectSegment(HasMediaSequenceNumber, 2);

  // Segments must not exceed the playlist's target duration when rounded to the
  // nearest integer
  {
    auto fork = builder;
    fork.AppendLine("#EXTINF:10.499,bar");
    fork.AppendLine("bar.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectOk();

    fork.AppendLine("#EXTINF:10.5,baz");
    fork.AppendLine("baz.ts");
    fork.ExpectError(ParseStatusCode::kMediaSegmentExceedsTargetDuration);
  }

  builder.ExpectOk();
}

// This test is similar to the `HlsMultivariantPlaylistTest` test of the same
// name, but due to subtle differences between media playlists and multivariant
// playlists its difficult to combine them. If new cases are added here that are
// also relevant to multivariant playlists, they should be added to that test as
// well.
TEST(HlsMediaPlaylistTest, VariableSubstitution) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");
  builder.AppendLine("#EXT-X-VERSION:8");
  builder.ExpectPlaylist(HasVersion, 8);
  builder.ExpectPlaylist(HasTargetDuration, base::Seconds(10));

  builder.AppendLine(R"(#EXT-X-DEFINE:NAME="ROOT",VALUE="http://video.com")");
  builder.AppendLine(R"(#EXT-X-DEFINE:NAME="MOVIE",VALUE="some_video/low")");

  // Valid variable references within URI items should be substituted
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("{$ROOT}/{$MOVIE}/fileSegment0.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(
      HasUri, GURL("http://video.com/some_video/low/fileSegment0.ts"));

  {
    // Invalid variable references within URI lines should result in an error
    auto fork = builder;
    fork.AppendLine("#EXTINF:9.9,\t");
    fork.AppendLine("{$root}/{$movie}/fileSegment0.ts");
    fork.ExpectError(ParseStatusCode::kVariableUndefined);
  }

  // Variable references outside of valid substitution points should not be
  // substituted
  {
    auto fork = builder;
    fork.AppendLine(R"(#EXT-X-DEFINE:NAME="LENGTH",VALUE="9.9")");
    fork.AppendLine("#EXTINF:{$LENGTH},\t");
    fork.AppendLine("http://foo/bar");
    fork.ExpectError(ParseStatusCode::kMalformedTag);
  }

  // Redefinition is not allowed
  {
    auto fork = builder;
    fork.AppendLine(
        R"(#EXT-X-DEFINE:NAME="ROOT",VALUE="https://www.google.com")");
    fork.ExpectError(ParseStatusCode::kVariableDefinedMultipleTimes);
  }

  // Importing in a parentless playlist is not allowed
  {
    auto fork = builder;
    fork.AppendLine(R"(#EXT-X-DEFINE:IMPORT="IMPORTED")");
    fork.ExpectError(ParseStatusCode::kImportedVariableInParentlessPlaylist);
  }

  // Test importing variables in a playlist with a parent
  auto parent = CreateMultivariantPlaylist(
      {"#EXTM3U", "#EXT-X-VERSION:8",
       R"(#EXT-X-DEFINE:NAME="IMPORTED",VALUE="HELLO")"});
  {
    // Referring to a parent playlist variable without importing it is an error
    auto fork = builder;
    fork.SetParent(&parent);
    fork.AppendLine("#EXTINF:9.9,\t");
    fork.AppendLine("segments/{$IMPORTED}.ts");
    fork.ExpectError(ParseStatusCode::kVariableUndefined);
  }
  {
    // Locally overwriting an unimported variable from a parent playlist is NOT
    // an error
    auto fork = builder;
    fork.SetParent(&parent);
    fork.AppendLine(R"(#EXT-X-DEFINE:NAME="IMPORTED",VALUE="WORLD")");
    fork.AppendLine("#EXTINF:9.9,\t");
    fork.AppendLine("segments/{$IMPORTED}.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasUri, GURL("http://localhost/segments/WORLD.ts"));
    fork.ExpectOk();

    // Importing a variable once it's been defined is an error
    fork.AppendLine(R"(#EXT-X-DEFINE:IMPORT="IMPORTED")");
    fork.ExpectError(ParseStatusCode::kVariableDefinedMultipleTimes);
  }
  {
    // Defining a variable once it's been imported is an error
    auto fork = builder;
    fork.SetParent(&parent);
    fork.AppendLine(R"(#EXT-X-DEFINE:IMPORT="IMPORTED")");
    fork.AppendLine(R"(#EXT-X-DEFINE:NAME="IMPORTED",VALUE="WORLD")");
    fork.ExpectError(ParseStatusCode::kVariableDefinedMultipleTimes);
  }
  {
    // Importing the same variable twice is an error
    auto fork = builder;
    fork.SetParent(&parent);
    fork.AppendLine(R"(#EXT-X-DEFINE:IMPORT="IMPORTED")");
    fork.AppendLine(R"(#EXT-X-DEFINE:IMPORT="IMPORTED")");
    fork.ExpectError(ParseStatusCode::kVariableDefinedMultipleTimes);
  }
  {
    // Importing a variable that hasn't been defined in the parent playlist is
    // an error
    auto fork = builder;
    fork.SetParent(&parent);
    fork.AppendLine(R"(#EXT-X-DEFINE:IMPORT="FOO")");
    fork.ExpectError(ParseStatusCode::kImportedVariableUndefined);
  }
  {
    // Test actually using an imported variable
    auto fork = builder;
    fork.SetParent(&parent);
    fork.AppendLine(R"(#EXT-X-DEFINE:IMPORT="IMPORTED")");
    fork.AppendLine("#EXTINF:9.9,\t");
    fork.AppendLine("segments/{$IMPORTED}.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasUri, GURL("http://localhost/segments/HELLO.ts"));
    fork.ExpectOk();
  }

  // Variables are not resolved recursively
  builder.AppendLine(R"(#EXT-X-DEFINE:NAME="BAR",VALUE="BAZ")");
  builder.AppendLine(R"(#EXT-X-DEFINE:NAME="FOO",VALUE="{$BAR}")");
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("http://{$FOO}.com/video");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasUri, GURL("http://{$BAR}.com/video"));

  builder.ExpectOk();
}

TEST(HlsMediaPlaylistTest, PlaylistType) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");

  // Without the EXT-X-PLAYLIST-TYPE tag, the playlist has no type.
  {
    auto fork = builder;
    fork.ExpectPlaylist(HasType, absl::nullopt);
    fork.ExpectOk();
  }

  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE:VOD");
    fork.ExpectPlaylist(HasType, PlaylistType::kVOD);
    fork.ExpectOk();
  }

  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE:EVENT");
    fork.ExpectPlaylist(HasType, PlaylistType::kEvent);
    fork.ExpectOk();
  }

  // This tag may not be specified twice
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE:VOD");
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE:EVENT");
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE:VOD");
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE:VOD");
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }

  // Unknown or invalid playlist types should trigger an error
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE:FOOBAR");
    fork.ExpectError(ParseStatusCode::kUnknownPlaylistType);
  }
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE:");
    fork.ExpectError(ParseStatusCode::kMalformedTag);
  }
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE");
    fork.ExpectError(ParseStatusCode::kMalformedTag);
  }
}

TEST(HlsMediaPlaylistTest, MultivariantPlaylistTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");

  // Media playlists may not contain tags exclusive to multivariant playlists
  for (TagName name = ToTagName(MultivariantPlaylistTagName::kMinValue);
       name <= ToTagName(MultivariantPlaylistTagName::kMaxValue); ++name) {
    auto tag_line = "#" + std::string{TagNameToString(name)};
    auto fork = builder;
    fork.AppendLine(tag_line);
    fork.ExpectError(ParseStatusCode::kMediaPlaylistHasMultivariantPlaylistTag);
  }
}

TEST(HlsMediaPlaylistTest, XIndependentSegmentsTagInParent) {
  auto parent1 = CreateMultivariantPlaylist({
      "#EXTM3U",
      "#EXT-X-INDEPENDENT-SEGMENTS",
  });

  // Parent value should carryover to media playlist
  MediaPlaylistTestBuilder builder;
  builder.SetParent(&parent1);
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");
  builder.ExpectPlaylist(HasIndependentSegments, true);
  builder.ExpectOk();

  // It's OK for this tag to reappear in the media playlist
  builder.AppendLine("#EXT-X-INDEPENDENT-SEGMENTS");
  builder.ExpectOk();

  // Without that tag in the parent, the value depends entirely on its presence
  // in the child
  auto parent2 = CreateMultivariantPlaylist({"#EXTM3U"});
  builder = MediaPlaylistTestBuilder();
  builder.SetParent(&parent2);
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");
  {
    auto fork = builder;
    fork.ExpectPlaylist(HasIndependentSegments, false);
    fork.ExpectOk();
  }
  builder.AppendLine("#EXT-X-INDEPENDENT-SEGMENTS");
  builder.ExpectPlaylist(HasIndependentSegments, true);
  builder.ExpectOk();
  EXPECT_FALSE(parent2.AreSegmentsIndependent());
}

TEST(HlsMediaPlaylistTest, XTargetDurationTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");

  // The XTargetDurationTag tag is required
  builder.ExpectError(ParseStatusCode::kMediaPlaylistMissingTargetDuration);

  // The XTargetDurationTag must appear exactly once
  builder.AppendLine("#EXT-X-TARGETDURATION:10");
  builder.ExpectPlaylist(HasTargetDuration, base::Seconds(10));
  builder.ExpectOk();

  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-TARGETDURATION:10");
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-TARGETDURATION:11");
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }

  // The XTargetDurationTag must be a valid DecimalInteger (unsigned)
  for (base::StringPiece x : {"-1", "0.5", "-1.5", "999999999999999999999"}) {
    MediaPlaylistTestBuilder builder2;
    builder2.AppendLine("#EXTM3U");
    builder2.AppendLine("#EXT-X-TARGETDURATION:", x);
    builder2.ExpectError(ParseStatusCode::kMalformedTag);
  }
}

TEST(HlsMediaPlaylistTest, XEndListTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");

  // Without the 'EXT-X-ENDLIST' tag, the default value is false, regardless of
  // the playlist type.
  {
    for (const base::StringPiece type : {"", "EVENT", "VOD"}) {
      auto fork = builder;
      if (!type.empty()) {
        fork.AppendLine("#EXT-X-PLAYLIST-TYPE:", type);
      }
      fork.ExpectPlaylist(IsEndList, false);
      fork.ExpectOk();
    }
  }

  // The 'EXT-X-ENDLIST' tag may not have any content
  {
    for (const base::StringPiece x : {"", "FOO=BAR", "1"}) {
      auto fork = builder;
      fork.AppendLine("#EXT-X-ENDLIST:", x);
      fork.ExpectError(ParseStatusCode::kMalformedTag);
    }
  }

  // The EXT-X-ENDLIST tag can appear anywhere in the playlist
  builder.AppendLine("#EXTINF:9.2,\t");
  builder.AppendLine("segment0.ts");
  builder.ExpectAdditionalSegment();

  builder.AppendLine("#EXT-X-ENDLIST");
  builder.ExpectPlaylist(IsEndList, true);

  builder.AppendLine("#EXTINF:9.2,\n");
  builder.AppendLine("segment1.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectOk();

  // The EXT-X-ENDLIST tag may not appear twice
  builder.AppendLine("#EXT-X-ENDLIST");
  builder.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
}

TEST(HlsMediaPlaylistTest, XIFramesOnlyTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");

  // Without the 'EXT-X-I-FRAMES-ONLY' tag, the default value is false.
  {
    auto fork = builder;
    fork.ExpectPlaylist(IsIFramesOnly, false);
    fork.ExpectOk();
  }

  // The 'EXT-X-I-FRAMES-ONLY' tag may not have any content
  {
    for (const base::StringPiece x : {"", "FOO=BAR", "1"}) {
      auto fork = builder;
      fork.AppendLine("#EXT-X-I-FRAMES-ONLY:", x);
      fork.ExpectError(ParseStatusCode::kMalformedTag);
    }
  }

  builder.AppendLine("#EXT-X-I-FRAMES-ONLY");
  builder.ExpectPlaylist(IsIFramesOnly, true);

  // This should not affect the calculation of the playlist's duration
  builder.AppendLine("#EXTINF:10,\t");
  builder.AppendLine("segment0.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDuration, 10);

  builder.AppendLine("#EXTINF:10,\t");
  builder.AppendLine("segment1.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDuration, 10);

  builder.ExpectPlaylist(HasComputedDuration, base::Seconds(20));
  builder.ExpectOk();

  // The 'EXT-X-I-FRAMES-ONLY' tag should not appear twice
  builder.AppendLine("#EXT-X-I-FRAMES-ONLY");
  builder.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
}

TEST(HlsMediaPlaylistTest, XMediaSequenceTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");

  // The EXT-X-MEDIA-SEQUENCE tag's content must be a valid DecimalInteger
  {
    for (const base::StringPiece x : {"", ":-1", ":{$foo}", ":1.5", ":one"}) {
      auto fork = builder;
      fork.AppendLine("#EXT-X-MEDIA-SEQUENCE", x);
      fork.ExpectError(ParseStatusCode::kMalformedTag);
    }
  }
  // The EXT-X-MEDIA-SEQUENCE tag may not appear twice
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-MEDIA-SEQUENCE:0");
    fork.AppendLine("#EXT-X-MEDIA-SEQUENCE:1");
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }
  // The EXT-X-MEDIA-SEQUENCE tag must appear before any media segment
  {
    auto fork = builder;
    fork.AppendLine("#EXTINF:9.8,\t");
    fork.AppendLine("segment0.ts");
    fork.AppendLine("#EXT-X-MEDIA-SEQUENCE:0");
    fork.ExpectError(ParseStatusCode::kMediaSegmentBeforeMediaSequenceTag);
  }

  const auto fill_playlist = [](auto& builder, auto first_sequence_number) {
    builder.AppendLine("#EXTINF:9.8,\t");
    builder.AppendLine("segment0.ts");
    builder.ExpectAdditionalSegment();
    builder.ExpectSegment(HasUri, GURL("http://localhost/segment0.ts"));
    builder.ExpectSegment(HasMediaSequenceNumber, first_sequence_number);

    builder.AppendLine("#EXTINF:9.8,\t");
    builder.AppendLine("segment1.ts");
    builder.ExpectAdditionalSegment();
    builder.ExpectSegment(HasMediaSequenceNumber, first_sequence_number + 1);

    builder.AppendLine("#EXTINF:9.8,\t");
    builder.AppendLine("segment2.ts");
    builder.ExpectAdditionalSegment();
    builder.ExpectSegment(HasMediaSequenceNumber, first_sequence_number + 2);
  };

  // If the playlist does not contain the EXT-X-MEDIA-SEQUENCE tag, the default
  // starting segment number is 0.
  auto fork = builder;
  fill_playlist(fork, 0);
  fork.ExpectPlaylist(HasMediaSequenceTag, false);
  fork.ExpectOk();

  // If the playlist has the EXT-X-MEDIA-SEQUENCE tag, it specifies the starting
  // segment number.
  fork = builder;
  fork.AppendLine("#EXT-X-MEDIA-SEQUENCE:0");
  fill_playlist(fork, 0);
  fork.ExpectPlaylist(HasMediaSequenceTag, true);
  fork.ExpectOk();

  fork = builder;
  fork.AppendLine("#EXT-X-MEDIA-SEQUENCE:15");
  fill_playlist(fork, 15);
  fork.ExpectPlaylist(HasMediaSequenceTag, true);
  fork.ExpectOk();

  fork = builder;
  fork.AppendLine("#EXT-X-MEDIA-SEQUENCE:9999");
  fill_playlist(fork, 9999);
  fork.ExpectPlaylist(HasMediaSequenceTag, true);
  fork.ExpectOk();
}

}  // namespace media::hls
