// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_view.h"

#include "remoting/base/decoder_verbatim.h"
#include "remoting/base/decoder_zlib.h"

namespace remoting {

ChromotingView::ChromotingView()
    : frame_width_(0),
      frame_height_(0) {
}

ChromotingView::~ChromotingView() {}

// TODO(garykac): This assumes a single screen. This will need to be adjusted
// to add support for mulitple monitors.
void ChromotingView::GetScreenSize(int* width, int* height) {
  *width = frame_width_;
  *height = frame_height_;
}

bool ChromotingView::SetupDecoder(UpdateStreamEncoding encoding) {
  if (encoding == EncodingInvalid) {
    LOG(ERROR) << "Cannot create encoder for EncodingInvalid";
    return false;
  }

  // If we're in the middle of decoding a stream, then we need to make sure
  // that that all packets in that stream match the encoding of the first
  // packet.
  //
  // If we decide to relax this constraint in the future, we'll need to
  // update this to keep a set of decoders around.
  if (decoder_.get() && decoder_->IsStarted()) {
    // Verify that the encoding matches the decoder. Once we've started
    // decoding, we can't switch to another decoder.
    if (decoder_->Encoding() != encoding) {
      LOG(ERROR) << "Encoding mismatch: Set up to handle "
                 << "UpdateStreamEncoding=" << decoder_->Encoding()
                 << " but received request for "
                 << encoding;
      return false;
    }
    return true;
  }

  // Lazily initialize a new decoder.
  // We create a new decoder if we don't currently have a decoder or if the
  // decoder doesn't match the desired encoding.
  if (!decoder_.get() || decoder_->Encoding() != encoding) {
    // Initialize a new decoder based on this message encoding.
    if (encoding == EncodingNone) {
      decoder_.reset(new DecoderVerbatim());
    } else if (encoding == EncodingZlib) {
      decoder_.reset(new DecoderZlib());
    }
    // Make sure we successfully allocated a decoder of the correct type.
    DCHECK(decoder_.get());
    DCHECK(decoder_->Encoding() == encoding);
  }

  return true;
}

bool ChromotingView::BeginDecoding(Task* partial_decode_done,
                                   Task* decode_done) {
  if (decoder_->IsStarted()) {
    LOG(ERROR) << "BeginDecoding called without ending previous decode.";
    return false;
  }

  decoder_->BeginDecode(frame_, &update_rects_,
                        partial_decode_done, decode_done);

  if (!decoder_->IsStarted()) {
    LOG(ERROR) << "Unable to start decoding.";
    return false;
  }

  return true;
}

bool ChromotingView::Decode(ChromotingHostMessage* msg) {
  if (!decoder_->IsStarted()) {
    LOG(ERROR) << "Attempt to decode payload before calling BeginDecode.";
    return false;
  }

  return decoder_->PartialDecode(msg);
}

bool ChromotingView::EndDecoding() {
  if (!decoder_->IsStarted()) {
    LOG(ERROR) << "Attempt to end decode when none has been started.";
    return false;
  }

  decoder_->EndDecode();

  if (decoder_->IsStarted()) {
    LOG(ERROR) << "Unable to properly end decoding.\n";
    return false;
  }

  return true;
}

}  // namespace remoting
