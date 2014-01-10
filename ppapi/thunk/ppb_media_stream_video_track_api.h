// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_MEDIA_STREAM_VIDEO_TRACK_API_H_
#define PPAPI_THUNK_PPB_MEDIA_STREAM_VIDEO_TRACK_API_H_

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT PPB_MediaStreamVideoTrack_API {
 public:
  virtual ~PPB_MediaStreamVideoTrack_API() {}
  virtual PP_Var GetId() = 0;
  virtual PP_Bool HasEnded() = 0;
  virtual int32_t Configure(uint32_t max_buffered_frames) = 0;
  virtual int32_t GetFrame(
      PP_Resource* frame,
      scoped_refptr<ppapi::TrackedCallback> callback) = 0;
  virtual int32_t RecycleFrame(PP_Resource frame) = 0;
  virtual void Close() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_MEDIA_STREAM_VIDEO_TRACK_API_H_
