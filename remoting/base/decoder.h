// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_DECODER_H_
#define REMOTING_BASE_DECODER_H_

#include <vector>

#include "base/task.h"
#include "base/scoped_ptr.h"
#include "gfx/rect.h"
#include "media/base/video_frame.h"
#include "remoting/base/protocol/chromotocol.pb.h"

namespace remoting {

// TODO(hclam): Merge this with the one in remoting/host/encoder.h.
typedef std::vector<gfx::Rect> UpdatedRects;

// Defines the behavior of a decoder for decoding images received from the
// host.
//
// Sequence of actions with a decoder is as follows:
//
// 1. BeginDecode(PartialDecodeDone, DecodeDone, VideoFrame)
// 2. PartialDecode(ChromotingHostMessage)
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
  Decoder()
      : encoding_(EncodingInvalid),
        started_(false) {
  }
  virtual ~Decoder() {
  }

  // Tell the decoder to use |frame| as a target to write the decoded image
  // for the coming update stream.
  // If decode is partially done and |frame| can be read, |partial_decode_done|
  // is called and |update_rects| contains the updated regions.
  // If decode is completed |decode_done| is called.
  // Return true if the decoder can writes output to |frame| and accept
  // the codec format.
  // TODO(hclam): Provide more information when calling this function.
  virtual bool BeginDecode(scoped_refptr<media::VideoFrame> frame,
                           UpdatedRects* updated_rects,
                           Task* partial_decode_done,
                           Task* decode_done) = 0;

  // Give a ChromotingHostMessage that contains the update stream packet that
  // contains the encoded data to the decoder.
  // The decoder will own |message| and is responsible for deleting it.
  //
  // If the decoder has written something into |frame|,
  // |partial_decode_done_| is called with |frame| and updated regions.
  // Return true if the decoder can accept |message| and decode it.
  //
  // ChromotingHostMessage returned by this method will contain a
  // UpdateStreamPacketMessage.
  // This message will contain either:
  // 1. UpdateStreamBeginRect
  // 2. UpdateStreamRectData
  // 3. UpdateStreamEndRect
  //
  // See remoting/base/protocol/chromotocol.proto for more information about
  // these messages.
  virtual bool PartialDecode(ChromotingHostMessage* message) = 0;

  // Notify the decoder that we have received the last update stream packet.
  // If the decoding of the update stream has completed |decode_done_| is
  // called with |frame|.
  // If the update stream is not received fully and this method is called the
  // decoder should also call |decode_done_| as soon as possible.
  virtual void EndDecode() = 0;

  // Return the encoding type that this decoder handles.
  virtual UpdateStreamEncoding Encoding() { return encoding_; }

  // Return the current state of the decoder: 'true' if we're in the middle
  // of BeginDecode() / EndDecode().
  virtual bool IsStarted() { return started_; }

  // --- NEW API ---
  // TODO(ajwong): This API is incorrect in the face of a streaming decode
  // protocol like VP8.  However, it breaks the layering abstraction by
  // depending on the network packet protocol buffer type. I'm going to go
  // forward with it as is, and then refactor again to support streaming
  // decodes.

  // Initializes the decoder to draw into the given |frame|.  The |clip|
  // specifies the region to draw into.  The clip region must fit inside
  // the dimensions of frame. Failure to do so will CHECK Fail.
  virtual void Initialize(scoped_refptr<media::VideoFrame> frame,
                          const gfx::Rect& clip) {}

  // Reset the decoder to an uninitialized state. Release all references to
  // the initialized |frame|.  Initialize() must be called before the decoder
  // is used again.
  virtual void Reset() {}

  // Feeds more data into the decoder.
  virtual void DecodeBytes(const std::string& encoded_bytes) {}

  // Returns true if decoder is ready to accept data via ProcessRectangleData.
  virtual bool IsReadyForData() { return false; }

 protected:
  // Every decoder will have two internal states because there are three
  // kinds of messages send to PartialDecode().
  //
  // Here's a state diagram:
  //
  //                UpdateStreamBeginRect       UpdateStreamRectData
  //                    ..............              ............
  //                   .              .            .            .
  //                  .                v          .              .
  // kWaitingForBeginRect         kWaitingForRectData            .
  //                  ^                .          ^              .
  //                   .              .            .            .
  //                    ..............              ............
  //                    UpdateStreaEndRect
  enum State {
    // In this state the decoder is waiting for UpdateStreamBeginRect.
    // After receiving UpdateStreaBeginRect, the encoder will transit to
    // to kWaitingForRectData state.
    kWaitingForBeginRect,

    // In this state the decoder is waiting for UpdateStreamRectData.
    // The decode remains in this state if UpdateStreamRectData is received.
    // The decoder will transit to kWaitingForBeginRect if UpdateStreamEndRect
    // is received.
    kWaitingForRectData,
  };

  // The encoding that this decoder supports.
  UpdateStreamEncoding encoding_;

  // Has the decoder been started? I.e., has BeginDecode() been called.
  bool started_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_DECODER_H_
