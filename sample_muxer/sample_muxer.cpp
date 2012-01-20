// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <stdio.h>
#include <string.h>

// libwebm parser includes
#include "mkvreader.hpp"
#include "mkvparser.hpp"

// libwebm muxer includes
#include "mkvmuxer.hpp"
#include "mkvwriter.hpp"
#include "mkvmuxerutil.hpp"

namespace {

void Usage() {
  printf("Usage: sample_muxer -i input -o output [options]\n");
  printf("\n");
  printf("Main options:\n");
  printf("  -h | -?                     show help\n");
  printf("  -video <int>                >0 outputs video\n");
  printf("  -audio <int>                >0 outputs audio\n");
  printf("  -live <int>                 >0 puts the muxer into live mode\n");
  printf("                              0 puts the muxer into file mode\n");
  printf("  -output_cues <int>          >0 outputs cues element\n");
  printf("  -cues_on_video_track <int>  >0 outputs cues on video track\n");
  printf("  -cues_on_audio_track <int>  >0 outputs cues on audio track\n");
  printf("                              0 outputs cues on audio track\n");
  printf("  -max_cluster_duration <double> in seconds\n");
  printf("  -max_cluster_size <int>     in bytes\n");
  printf("  -switch_tracks <int>        >0 switches tracks in output\n");
  printf("  -audio_track_number <int>   >0 Changes the audio track number\n");
  printf("  -video_track_number <int>   >0 Changes the video track number\n");
  printf("  -chunking <string>          Chunk output\n");
  printf("\n");
  printf("Video options:\n");
  printf("  -display_width <int>        Display width in pixels\n");
  printf("  -display_height <int>       Display height in pixels\n");
  printf("  -stereo_mode <int>          3D video mode\n");
  printf("\n");
  printf("Cues options:\n");
  printf("  -output_cues_block_number <int> >0 outputs cue block number\n");
}

} //end namespace

int main(int argc, char* argv[]) {
  using mkvmuxer::uint64;

  char* input = NULL;
  char* output = NULL;

  // Segment variables
  bool output_video = true;
  bool output_audio = true;
  bool live_mode = false;
  bool output_cues = true;
  bool cues_on_video_track = true;
  bool cues_on_audio_track = true;
  uint64 max_cluster_duration = 0;
  uint64 max_cluster_size = 0;
  bool switch_tracks = false;
  int audio_track_number = 0; // 0 tells muxer to decide.
  int video_track_number = 0; // 0 tells muxer to decide.
  bool chunking = false;
  const char* chunk_name = NULL;

  bool output_cues_block_number = true;

  uint64 display_width = 0;
  uint64 display_height = 0;
  uint64 stereo_mode = 0;

  for (int i = 1; i < argc; ++i) {
    char* end;

    if (!strcmp("-h", argv[i]) || !strcmp("-?", argv[i])) {
      Usage();
      return 0;
    } else if (!strcmp("-i", argv[i])) {
      input = argv[++i];
    } else if (!strcmp("-o", argv[i])) {
      output = argv[++i];
    } else if (!strcmp("-video", argv[i])) {
      output_video = strtol(argv[++i], &end, 10) == 0 ? false : true;
    } else if (!strcmp("-audio", argv[i])) {
      output_audio = strtol(argv[++i], &end, 10) == 0 ? false : true;
    } else if (!strcmp("-live", argv[i])) {
      live_mode = strtol(argv[++i], &end, 10) == 0 ? false : true;
    } else if (!strcmp("-output_cues", argv[i])) {
      output_cues = strtol(argv[++i], &end, 10) == 0 ? false : true;
    } else if (!strcmp("-cues_on_video_track", argv[i])) {
      cues_on_video_track = strtol(argv[++i], &end, 10) == 0 ? false : true;
    } else if (!strcmp("-cues_on_audio_track", argv[i])) {
      cues_on_audio_track = strtol(argv[++i], &end, 10) == 0 ? false : true;
    } else if (!strcmp("-max_cluster_duration", argv[i])) {
      const double seconds = strtod(argv[++i], &end);
      max_cluster_duration =
          static_cast<uint64>(seconds * 1000000000.0);
    } else if (!strcmp("-max_cluster_size", argv[i])) {
      max_cluster_size = strtol(argv[++i], &end, 10);
    } else if (!strcmp("-switch_tracks", argv[i])) {
      switch_tracks = strtol(argv[++i], &end, 10) == 0 ? false : true;
    } else if (!strcmp("-audio_track_number", argv[i])) {
      audio_track_number = strtol(argv[++i], &end, 10);
    } else if (!strcmp("-video_track_number", argv[i])) {
      video_track_number = strtol(argv[++i], &end, 10);
    } else if (!strcmp("-chunking", argv[i])) {
      chunking = true;
      chunk_name = argv[++i];
    } else if (!strcmp("-display_width", argv[i])) {
      display_width = strtol(argv[++i], &end, 10);
    } else if (!strcmp("-display_height", argv[i])) {
      display_height = strtol(argv[++i], &end, 10);
    } else if (!strcmp("-stereo_mode", argv[i])) {
      stereo_mode = strtol(argv[++i], &end, 10);
    } else if (!strcmp("-output_cues_block_number", argv[i])) {
      output_cues_block_number =
          strtol(argv[++i], &end, 10) == 0 ? false : true;
    }
  }

  if (input == NULL || output == NULL) {
    Usage();
    return 0;
  }

  // Get parser header info
  mkvparser::MkvReader reader;

  if (reader.Open(input)) {
    printf("\n Filename is invalid or error while opening.\n");
    return -1;
  }

  long long pos = 0;
  mkvparser::EBMLHeader ebml_header;
  ebml_header.Parse(&reader, pos);

  mkvparser::Segment* parser_segment;
  long long ret = mkvparser::Segment::CreateInstance(&reader,
                                                     pos,
                                                     parser_segment);
  if (ret) {
    printf("\n Segment::CreateInstance() failed.");
    return -1;
  }

  ret = parser_segment->Load();
  if (ret < 0) {
    printf("\n Segment::Load() failed.");
    return -1;
  }

  const mkvparser::SegmentInfo* const segment_info = parser_segment->GetInfo();
  const long long timeCodeScale = segment_info->GetTimeCodeScale();

  // Set muxer header info
  mkvmuxer::MkvWriter writer;

  if (!writer.Open(output)) {
    printf("\n Filename is invalid or error while opening.\n");
    return -1;
  }

  // Set Segment element attributes
  mkvmuxer::Segment muxer_segment;

  if (!muxer_segment.Init(&writer)) {
    printf("\n Could not initialize muxer segment!\n");
    return -1;
  }

  if (live_mode)
    muxer_segment.set_mode(mkvmuxer::Segment::kLive);
  else
    muxer_segment.set_mode(mkvmuxer::Segment::kFile);

  if (chunking)
    muxer_segment.SetChunking(true, chunk_name);

  if (max_cluster_duration > 0)
    muxer_segment.set_max_cluster_duration(max_cluster_duration);
  if (max_cluster_size > 0)
    muxer_segment.set_max_cluster_size(max_cluster_size);
  muxer_segment.OutputCues(output_cues);

  // Set SegmentInfo element attributes
  mkvmuxer::SegmentInfo* const info = muxer_segment.GetSegmentInfo();
  info->set_timecode_scale(timeCodeScale);
  info->set_writing_app("sample_muxer");

  // Set Tracks element attributes
  enum { kVideoTrack = 1, kAudioTrack = 2 };
  const mkvparser::Tracks* const parser_tracks = parser_segment->GetTracks();
  unsigned long i = 0;
  uint64 vid_track = 0; // no track added
  uint64 aud_track = 0; // no track added

  while (i != parser_tracks->GetTracksCount()) {
    int track_num = i++;
    if (switch_tracks)
      track_num = i % parser_tracks->GetTracksCount();

    const mkvparser::Track* const parser_track =
        parser_tracks->GetTrackByIndex(track_num);

    if (parser_track == NULL)
      continue;

    // TODO(fgalligan): Add support for language to parser.
    const char* const track_name = parser_track->GetNameAsUTF8();

    const long long track_type = parser_track->GetType();

    if (track_type == kVideoTrack && output_video) {
      // Get the video track from the parser
      const mkvparser::VideoTrack* const pVideoTrack =
          static_cast<const mkvparser::VideoTrack*>(parser_track);
      const long long width =  pVideoTrack->GetWidth();
      const long long height = pVideoTrack->GetHeight();

      // Add the video track to the muxer
      vid_track = muxer_segment.AddVideoTrack(static_cast<int>(width),
                                              static_cast<int>(height),
                                              video_track_number);
      if (!vid_track) {
        printf("\n Could not add video track.\n");
        return -1;
      }

      mkvmuxer::VideoTrack* const video =
          static_cast<mkvmuxer::VideoTrack*>(
              muxer_segment.GetTrackByNumber(vid_track));
      if (!video) {
        printf("\n Could not get video track.\n");
        return -1;
      }

      if (track_name)
        video->set_name(track_name);

      if (display_width > 0)
        video->set_display_width(display_width);
      if (display_height > 0)
        video->set_display_height(display_height);
      if (stereo_mode > 0)
        video->SetStereoMode(stereo_mode);

      const double rate = pVideoTrack->GetFrameRate();
      if (rate > 0.0) {
        video->set_frame_rate(rate);
      }
    } else if (track_type == kAudioTrack && output_audio) {
      // Get the audio track from the parser
      const mkvparser::AudioTrack* const pAudioTrack =
          static_cast<const mkvparser::AudioTrack*>(parser_track);
      const long long channels =  pAudioTrack->GetChannels();
      const double sample_rate = pAudioTrack->GetSamplingRate();

      // Add the audio track to the muxer
      aud_track = muxer_segment.AddAudioTrack(static_cast<int>(sample_rate),
                                              static_cast<int>(channels),
                                              audio_track_number);
      if (!aud_track) {
        printf("\n Could not add audio track.\n");
        return -1;
      }

      mkvmuxer::AudioTrack* const audio =
          static_cast<mkvmuxer::AudioTrack*>(
              muxer_segment.GetTrackByNumber(aud_track));
      if (!audio) {
        printf("\n Could not get audio track.\n");
        return -1;
      }

      if (track_name)
        audio->set_name(track_name);

      size_t private_size;
      const unsigned char* const private_data =
          pAudioTrack->GetCodecPrivate(private_size);
      if (private_size > 0) {
        if (!audio->SetCodecPrivate(private_data, private_size)) {
          printf("\n Could not add audio private data.\n");
          return -1;
        }
      }

      const long long bit_depth = pAudioTrack->GetBitDepth();
      if (bit_depth > 0)
        audio->set_bit_depth(bit_depth);
    }
  }

  // Set Cues element attributes
  mkvmuxer::Cues* const cues = muxer_segment.GetCues();
  cues->set_output_block_number(output_cues_block_number);
  if (cues_on_video_track && vid_track)
    muxer_segment.CuesTrack(vid_track);
  if (cues_on_audio_track && aud_track)
    muxer_segment.CuesTrack(aud_track);

  // Write clusters
  unsigned char* data = NULL;
  int data_len = 0;

  const mkvparser::Cluster* cluster = parser_segment->GetFirst();

  while ((cluster != NULL) && !cluster->EOS()) {
    const mkvparser::BlockEntry* block_entry = cluster->GetFirst();

    while ((block_entry != NULL) && !block_entry->EOS()) {
      const mkvparser::Block* const block = block_entry->GetBlock();
      const long long trackNum = block->GetTrackNumber();
      const mkvparser::Track* const parser_track =
          parser_tracks->GetTrackByNumber(
              static_cast<unsigned long>(trackNum));
      const long long track_type = parser_track->GetType();

      if ((track_type == kAudioTrack && output_audio) ||
          (track_type == kVideoTrack && output_video)) {
        const int frame_count = block->GetFrameCount();
        const long long time_ns = block->GetTime(cluster);
        const bool is_key = block->IsKey();

        for (int i = 0; i < frame_count; ++i) {
          const mkvparser::Block::Frame& frame = block->GetFrame(i);

          if (frame.len > data_len) {
            delete [] data;
            data = new unsigned char[frame.len];
            if (!data)
              return -1;
            data_len = frame.len;
          }

          if (frame.Read(&reader, data))
            return -1;

          uint64 track_num = vid_track;
          if (track_type == kAudioTrack)
            track_num = aud_track;

          if (!muxer_segment.AddFrame(data,
                                      frame.len,
                                      track_num,
                                      time_ns,
                                      is_key)) {
            printf("\n Could not add frame.\n");
            return -1;
          }
        }
      }

      block_entry = cluster->GetNext(block_entry);
    }

    cluster = parser_segment->GetNext(cluster);
  }

  muxer_segment.Finalize();

  delete [] data;
  delete parser_segment;

  writer.Close();
  reader.Close();

  return 0;
}



