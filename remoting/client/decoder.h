// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DECODER_H_
#define REMOTING_CLIENT_DECODER_H_

#include <vector>

#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "gfx/rect.h"
#include "media/base/video_frame.h"
#include "remoting/base/protocol/chromotocol.pb.h"

namespace remoting {

// Defines the behavior of a decoder for decoding images received from the
// host.
//
// Sequence of actions with a decoder is as follows:
//
// 1. BeginDecode(VideoFrame)
// 2. PartialDecode(HostMessage)
//    ...
// 3. EndDecode()
//
// The decoder will reply with:
// 1. PartialDecodeDone(VideoFrame, UpdatedRects)
//    ...
// 2. DecodeDone(VideoFrame)
//
// The format of VideoFrame is a contract between the object that creates the
// decoder (most likely the renderer) and the decoder.
class Decoder {
 public:
  typedef std::vector<gfx::Rect> UpdatedRects;
  typedef Callback2<scoped_refptr<media::VideoFrame>, UpdatedRects>::Type
      PartialDecodeDoneCallback;
  typedef Callback1<scoped_refptr<media::VideoFrame> >::Type
      DecodeDoneCallback;

  Decoder(PartialDecodeDoneCallback* partial_decode_done_callback,
          DecodeDoneCallback* decode_done_callback)
      : partial_decode_done_(partial_decode_done_callback),
        decode_done_(decode_done_callback) {
  }

  virtual ~Decoder() {
  }

  // Tell the decoder to use |frame| as a target to write the decoded image
  // for the coming update stream.
  // Return true if the decoder can writes output to |frame| and accept
  // the codec format.
  // TODO(hclam): Provide more information when calling this function.
  virtual bool BeginDecode(scoped_refptr<media::VideoFrame> frame) = 0;

  // Give a HostMessage that contains the update stream packet that contains
  // the encoded data to the decoder.
  // The decoder will own |message| and is responsible for deleting it.
  // If the decoder has written something into |frame|,
  // |partial_decode_done_| is called with |frame| and updated regions.
  // Return true if the decoder can accept |message| and decode it.
  virtual bool PartialDecode(chromotocol_pb::HostMessage* message) = 0;

  // Notify the decoder that we have received the last update stream packet.
  // If the decoding of the update stream has completed |decode_done_| is
  // called with |frame|.
  // If the update stream is not received fully and this method is called the
  // decoder should also call |decode_done_| as soon as possible.
  virtual void EndDecode() = 0;

 protected:
  PartialDecodeDoneCallback* partial_decode_done() {
    return partial_decode_done_.get();
  }

  DecodeDoneCallback* decode_done() {
    return decode_done_.get();
  }

 private:
  scoped_ptr<PartialDecodeDoneCallback> partial_decode_done_;
  scoped_ptr<DecodeDoneCallback> decode_done_;

  DISALLOW_COPY_AND_ASSIGN(Decoder);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_DECODER_H_
