// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/websourcebuffer_impl.h"

#include "media/filters/chunk_demuxer.h"

namespace webkit_media {

WebSourceBufferImpl::WebSourceBufferImpl(
    const std::string& id, scoped_refptr<media::ChunkDemuxer> demuxer)
    : id_(id),
      demuxer_(demuxer) {
  DCHECK(demuxer_);
}

WebSourceBufferImpl::~WebSourceBufferImpl() {
  DCHECK(!demuxer_) << "Object destroyed w/o removedFromMediaSource() call";
}

WebKit::WebTimeRanges WebSourceBufferImpl::buffered() {
  media::Ranges<base::TimeDelta> ranges = demuxer_->GetBufferedRanges(id_);
  WebKit::WebTimeRanges result(ranges.size());
  for (size_t i = 0; i < ranges.size(); i++) {
    result[i].start = ranges.start(i).InSecondsF();
    result[i].end = ranges.end(i).InSecondsF();
  }
  return result;
}

void WebSourceBufferImpl::append(const unsigned char* data, unsigned length) {
  demuxer_->AppendData(id_, data, length);
}

void WebSourceBufferImpl::abort() {
  demuxer_->Abort(id_);
}

bool WebSourceBufferImpl::setTimestampOffset(double offset) {
  base::TimeDelta time_offset = base::TimeDelta::FromMicroseconds(
      offset * base::Time::kMicrosecondsPerSecond);
  return demuxer_->SetTimestampOffset(id_, time_offset);
}

void WebSourceBufferImpl::removedFromMediaSource() {
  demuxer_->RemoveId(id_);
  demuxer_ = NULL;
}

}  // namespace webkit_media
