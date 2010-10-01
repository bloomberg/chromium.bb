// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CHROMOTING_VIEW_H_
#define REMOTING_CLIENT_CHROMOTING_VIEW_H_

#include "base/ref_counted.h"
#include "remoting/base/decoder.h"

namespace remoting {

// ChromotingView defines the behavior of an object that draws a view of the
// remote desktop. Its main function is to choose the right decoder and render
// the update stream onto the screen.
class ChromotingView {
 public:
  ChromotingView();
  virtual ~ChromotingView();

  // Get screen dimensions.
  // TODO(garykac): This will need to be extended to support multi-monitors.
  void GetScreenSize(int* width, int* height);

  // Initialize the common structures for the view.
  virtual bool Initialize() = 0;

  // Free up resources allocated by this view.
  virtual void TearDown() = 0;

  // Tells the ChromotingView to paint the current image on the screen.
  // TODO(hclam): Add rects as parameter if needed.
  virtual void Paint() = 0;

  // Fill the screen with one single static color, and ignore updates.
  // Useful for debugging.
  virtual void SetSolidFill(uint32 color) = 0;

  // Removes a previously set solid fill.  If no fill was previous set, this
  // does nothing.
  virtual void UnsetSolidFill() = 0;

  // Reposition and resize the viewport into the backing store. If the viewport
  // extends past the end of the backing store, it is filled with black.
  virtual void SetViewport(int x, int y, int width, int height) = 0;

  // Resize the underlying image that contains the host screen buffer.
  // This should match the size of the output from the decoder.
  //
  // TODO(garykac): This handles only 1 screen. We need multi-screen support.
  virtual void SetHostScreenSize(int width, int height) = 0;

  // Handle the BeginUpdateStream message.
  // This method should perform the following tasks:
  // (1) Perform any platform-specific tasks for start of update stream.
  // (2) Make sure the |frame_| has been initialized.
  // (3) Delete the HostMessage.
  virtual void HandleBeginUpdateStream(ChromotingHostMessage* msg) = 0;

  // Handle the UpdateStreamPacket message.
  // This method should perform the following tasks:
  // (1) Extract the decoding from the update packet message.
  // (2) Call SetupDecoder with the encoding to lazily initialize the decoder.
  //     We don't do this in BeginUpdateStream because the begin message
  //     doesn't contain the encoding.
  // (3) Call BeginDecoding if this is the first packet of the stream.
  // (4) Call the decoder's PartialDecode() method to decode the packet.
  //     This call will delete the HostMessage.
  // Note:
  // * For a given begin/end update stream, the encodings specified in the
  //   update packets must all match. We may revisit this constraint at a
  //   later date.
  virtual void HandleUpdateStreamPacket(ChromotingHostMessage* msg) = 0;

  // Handle the EndUpdateStream message.
  // This method should perform the following tasks:
  // (1) Call EndDecoding().
  // (2) Perform any platform-specific tasks for end of update stream.
  // (3) Delete the HostMessage.
  virtual void HandleEndUpdateStream(ChromotingHostMessage* msg) = 0;

 protected:
  // Setup the decoder based on the given encoding.
  // Returns true if a new decoder has already been started (with a call to
  // BeginDecoding).
  bool SetupDecoder(UpdateStreamEncoding encoding);

  // Prepare the decoder to start decoding a chunk of data.
  // This needs to be called if SetupDecoder() returns false.
  bool BeginDecoding(Task* partial_decode_done, Task* decode_done);

  // Decode the given message.
  // BeginDecoding() must be called before any calls to Decode().
  bool Decode(ChromotingHostMessage* msg);

  // Finish decoding and send notifications to update the view.
  bool EndDecoding();

  // Decoder used to decode the video frames (or frame fragements).
  scoped_ptr<Decoder> decoder_;

  // Framebuffer for the decoder.
  scoped_refptr<media::VideoFrame> frame_;

  // Dimensions of |frame_| bitmap.
  int frame_width_;
  int frame_height_;

  UpdatedRects update_rects_;
  UpdatedRects all_update_rects_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CHROMOTING_VIEW_H_
