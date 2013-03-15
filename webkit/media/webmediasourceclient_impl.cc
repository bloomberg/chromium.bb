// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/webmediasourceclient_impl.h"

#include "base/guid.h"
#include "media/filters/chunk_demuxer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSourceBuffer.h"

using ::WebKit::WebString;
using ::WebKit::WebMediaSourceClient;

namespace webkit_media {

#define COMPILE_ASSERT_MATCHING_STATUS_ENUM(webkit_name, chromium_name) \
  COMPILE_ASSERT(static_cast<int>(WebMediaSourceClient::webkit_name) == \
                 static_cast<int>(media::ChunkDemuxer::chromium_name),  \
                 mismatching_status_enums)
COMPILE_ASSERT_MATCHING_STATUS_ENUM(AddStatusOk, kOk);
COMPILE_ASSERT_MATCHING_STATUS_ENUM(AddStatusNotSupported, kNotSupported);
COMPILE_ASSERT_MATCHING_STATUS_ENUM(AddStatusReachedIdLimit, kReachedIdLimit);
#undef COMPILE_ASSERT_MATCHING_ENUM

class WebSourceBufferImpl : public WebKit::WebSourceBuffer {
 public:
  WebSourceBufferImpl(const std::string& id,
                      scoped_refptr<media::ChunkDemuxer> demuxer);
  virtual ~WebSourceBufferImpl();

  // WebKit::WebSourceBuffer implementation.
  virtual WebKit::WebTimeRanges buffered() OVERRIDE;
  virtual void append(const unsigned char* data, unsigned length) OVERRIDE;
  virtual void abort() OVERRIDE;
  virtual bool setTimestampOffset(double offset) OVERRIDE;
  virtual void removedFromMediaSource() OVERRIDE;

 private:
  std::string id_;
  scoped_refptr<media::ChunkDemuxer> demuxer_;

  DISALLOW_COPY_AND_ASSIGN(WebSourceBufferImpl);
};

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

WebMediaSourceClientImpl::WebMediaSourceClientImpl(
    const scoped_refptr<media::ChunkDemuxer>& demuxer,
    media::LogCB log_cb)
    : demuxer_(demuxer),
      log_cb_(log_cb) {
  DCHECK(demuxer_);
}

WebMediaSourceClientImpl::~WebMediaSourceClientImpl() {}

WebMediaSourceClient::AddStatus WebMediaSourceClientImpl::addSourceBuffer(
    const WebKit::WebString& type,
    const WebKit::WebVector<WebKit::WebString>& codecs,
    WebKit::WebSourceBuffer** source_buffer) {
  std::string id = base::GenerateGUID();
  std::vector<std::string> new_codecs(codecs.size());
  for (size_t i = 0; i < codecs.size(); ++i)
    new_codecs[i] = codecs[i].utf8().data();
  WebMediaSourceClient::AddStatus result =
      static_cast<WebMediaSourceClient::AddStatus>(
          demuxer_->AddId(id, type.utf8().data(), new_codecs));

  if (result == WebMediaSourceClient::AddStatusOk)
    *source_buffer = new WebSourceBufferImpl(id, demuxer_);

  return result;
}

double WebMediaSourceClientImpl::duration() {
  double duration = demuxer_->GetDuration();

  // Make sure super small durations don't get truncated to 0 and
  // large durations don't get converted to infinity by the double -> float
  // conversion.
  //
  // TODO(acolwell): Remove when WebKit is changed to report duration as a
  // double.
  if (duration > 0.0 && duration < std::numeric_limits<double>::infinity()) {
    duration = std::max(duration,
                        static_cast<double>(std::numeric_limits<float>::min()));
    duration = std::min(duration,
                        static_cast<double>(std::numeric_limits<float>::max()));
  }

  return static_cast<float>(duration);
}

void WebMediaSourceClientImpl::setDuration(double new_duration) {
  DCHECK_GE(new_duration, 0);
  demuxer_->SetDuration(new_duration);
}

void WebMediaSourceClientImpl::endOfStream(
    WebMediaSourceClient::EndOfStreamStatus status) {
  media::PipelineStatus pipeline_status = media::PIPELINE_OK;

  switch (status) {
    case WebMediaSourceClient::EndOfStreamStatusNoError:
      break;
    case WebMediaSourceClient::EndOfStreamStatusNetworkError:
      pipeline_status = media::PIPELINE_ERROR_NETWORK;
      break;
    case WebMediaSourceClient::EndOfStreamStatusDecodeError:
      pipeline_status = media::PIPELINE_ERROR_DECODE;
      break;
    default:
      NOTIMPLEMENTED();
  }

  if (!demuxer_->EndOfStream(pipeline_status))
    DVLOG(1) << "EndOfStream call failed.";
}

}  // namespace webkit_media
