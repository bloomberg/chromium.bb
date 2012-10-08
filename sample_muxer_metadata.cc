#include "sample_muxer_metadata.h"
#include <string>
#include "vttreader.h"

using std::string;

SampleMuxerMetadata::SampleMuxerMetadata() : segment_(NULL) {
}

bool SampleMuxerMetadata::Init(mkvmuxer::Segment* segment) {
  if (segment == NULL || segment_ != NULL)
    return false;

  segment_ = segment;
  return true;
}

bool SampleMuxerMetadata::Load(const char* file, Kind kind) {
  mkvmuxer::uint64 track_num;

  if (!AddTrack(kind, &track_num)) {
    printf("Unable to add track for WebVTT file \"%s\"\n", file);
    return false;
  }

  return Parse(file, kind, track_num);
}

bool SampleMuxerMetadata::Write(mkvmuxer::int64 time_ns) {
  typedef cues_set_t::iterator iter_t;

  iter_t i = cues_set_.begin();
  const iter_t j = cues_set_.end();

  while (i != j) {
    const cues_set_t::value_type& v = *i;

    if (time_ns >= 0 && v > time_ns)
      return true;  // nothing else to do just yet

    if (!v.Write(segment_)) {
      printf("\nCould not add metadata.\n");
      return false;  // error
    }

    cues_set_.erase(i++);
  }

  return true;
}

bool SampleMuxerMetadata::AddTrack(
    Kind kind,
    mkvmuxer::uint64* track_num) {
  *track_num = 0;

  // Track number value 0 means "let muxer choose track number"
  mkvmuxer::Track* const track = segment_->AddTrack(0);

  if (track == NULL)  // error
    return false;

  // Return the track number value chosen by the muxer
  *track_num = track->number();

  int type;
  const char* codec_id;

  switch (kind) {
    case kSubtitles:
      type = 0x11;
      codec_id = "D_WEBVTT/SUBTITLES";
      break;

    case kCaptions:
      type = 0x11;
      codec_id = "D_WEBVTT/CAPTIONS";
      break;

    case kDescriptions:
      type = 0x21;
      codec_id = "D_WEBVTT/DESCRIPTIONS";
      break;

    case kMetadata:
      type = 0x21;
      codec_id = "D_WEBVTT/METADATA";
      break;

    default:
      return false;
  }

  track->set_type(type);
  track->set_codec_id(codec_id);

  // TODO(matthewjheaney): set name and language

  return true;
}

bool SampleMuxerMetadata::Parse(
    const char* file,
    Kind /* kind */,
    mkvmuxer::uint64 track_num) {
  libwebvtt::VttReader r;
  int e = r.Open(file);

  if (e) {
    printf("Unable to open WebVTT file: \"%s\"\n", file);
    return false;
  }

  libwebvtt::Parser p(&r);

  e = p.Init();

  if (e < 0) {  // error
    printf("Error parsing WebVTT file: \"%s\"\n", file);
    return false;
  }

  SortableCue cue;
  cue.track_num = track_num;

  libwebvtt::Time t;
  t.hours = -1;

  for (;;) {
    cue_t& c = cue.cue;
    e = p.Parse(&c);

    if (e < 0) {  // error
      printf("Error parsing WebVTT file: \"%s\"\n", file);
      return false;
    }

    if (e > 0)  // EOF
      return true;

    if (c.start_time >= t) {
      t = c.start_time;
    } else {
      printf("bad WebVTT cue timestamp (out-of-order)\n");
      return false;
    }

    if (c.stop_time < c.start_time) {
      printf("bad WebVTT cue timestamp (stop < start)\n");
      return false;
    }

    cues_set_.insert(cue);
  }
}

void SampleMuxerMetadata::MakeFrame(const cue_t& c, string* pf) {
  pf->clear();
  WriteCueIdentifier(c.identifier, pf);
  WriteCueSettings(c.settings, pf);
  WriteCuePayload(c.payload, pf);
}

void SampleMuxerMetadata::WriteCueIdentifier(
    const string& identifier,
    string* pf) {
  pf->append(identifier);
  pf->push_back('\x0A');  // LF
}

void SampleMuxerMetadata::WriteCueSettings(
    const cue_t::settings_t& settings,
    string* pf) {
  if (settings.empty()) {
    pf->push_back('\x0A');  // LF
    return;
  }

  typedef cue_t::settings_t::const_iterator iter_t;

  iter_t i = settings.begin();
  const iter_t j = settings.end();

  for (;;) {
    const libwebvtt::Setting& setting = *i++;

    pf->append(setting.name);
    pf->push_back(':');
    pf->append(setting.value);

    if (i == j)
      break;

    pf->push_back(' ');  // separate settings with whitespace
  }

  pf->push_back('\x0A');  // LF
}

void SampleMuxerMetadata::WriteCuePayload(
    const cue_t::payload_t& payload,
    string* pf) {
  typedef cue_t::payload_t::const_iterator iter_t;

  iter_t i = payload.begin();
  const iter_t j = payload.end();

  while (i != j) {
    const string& line = *i++;
    pf->append(line);
    pf->push_back('\x0A');  // LF
  }
}

bool SampleMuxerMetadata::SortableCue::Write(
    mkvmuxer::Segment* segment) const {
  // Cue start time expressed in milliseconds
  const mkvmuxer::int64 start_ms = cue.start_time.presentation();

  // Cue start time expressed in nanoseconds (MKV time)
  const mkvmuxer::int64 start_ns = start_ms * 1000000;

  // Cue stop time expressed in milliseconds
  const mkvmuxer::int64 stop_ms = cue.stop_time.presentation();

  // Cue stop time expressed in nanonseconds
  const mkvmuxer::int64 stop_ns = stop_ms * 1000000;

  // Metadata blocks always specify the block duration.
  const mkvmuxer::int64 duration_ns = stop_ns - start_ns;

  string frame;
  MakeFrame(cue, &frame);

  typedef const mkvmuxer::uint8* data_t;
  const data_t buf = reinterpret_cast<data_t>(frame.data());
  const mkvmuxer::uint64 len = frame.length();

  return segment->AddMetadata(buf, len, track_num, start_ns, duration_ns);
}
