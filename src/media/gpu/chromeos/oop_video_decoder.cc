// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/oop_video_decoder.h"

#include "base/memory/ptr_util.h"
#include "media/base/bind_to_current_loop.h"
#include "media/gpu/macros.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

// Throughout this file, we have sprinkled many CHECK()s to assert invariants
// that should hold regardless of the behavior of the remote decoder or
// untrusted client. We use CHECK()s instead of DCHECK()s because
// OOPVideoDecoder and associated classes are very stateful so:
//
// a) They're hard to reason about.
// b) They're hard to fully exercise with tests.
// c) It's hard to reason if the violation of an invariant can have security
//    implications because once we enter into a bad state, everything is fair
//    game.
//
// Hence it's safer to crash and surface those crashes.
//
// More specifically:
//
// - It's illegal to call many methods if OOPVideoDecoder enters into an error
//   state (tracked by |has_error_|).
//
// - The media::VideoDecoder interface demands that its users don't call certain
//   methods while in specific states. An OOPVideoDecoder is used by an
//   in-process class (the VideoDecoderPipeline) to communicate with an
//   out-of-process video decoder. Therefore, we trust that the in-process user
//   of this class abides by the requirements of the media::VideoDecoder
//   interface and thus, we don't handle violations gracefully. In particular:
//
//   - No media::VideoDecoder methods should be called before the |init_cb|
//     passed to Initialize() is called. We track this interim state with
//     |init_cb_|.
//
//   - Initialize() should not be called while there are pending decodes (i.e.,
//     while !pending_decodes_.empty()).
//
//   - No media::VideoDecoder methods should be called before the |closure|
//     passed to Reset() is called. We track this interim state with
//     |reset_cb_|.

// TODO(b/220915557): OOPVideoDecoder cannot trust |remote_decoder_| (but
// |remote_decoder_| might trust us). We need to audit this class to make sure:
//
// - That OOPVideoDecoder validates everything coming from
//   |remote_video_decoder_|.
//
// - That OOPVideoDecoder meets the requirements of the media::VideoDecoder and
//   the media::VideoDecoderMixin interfaces. For example, we need to make sure
//   we guarantee statements like "all pending Decode() requests will be
//   finished or aborted before |closure| is called" (for
//   VideoDecoder::Reset()).
//
// - That OOPVideoDecoder asserts it's not being misused (which might cause us
//   to violate the requirements of the StableVideoDecoder interface). For
//   example, the StableVideoDecoder interface says for Decode(): "this must not
//   be called while there are pending Initialize(), Reset(), or Decode(EOS)
//   requests."

namespace media {

// static
std::unique_ptr<VideoDecoderMixin> OOPVideoDecoder::Create(
    mojo::PendingRemote<stable::mojom::StableVideoDecoder>
        pending_remote_decoder,
    std::unique_ptr<media::MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client) {
  // TODO(b/171813538): make the destructor of this class (as well as the
  // destructor of sister class VaapiVideoDecoder) public so the explicit
  // argument can be removed from this call to base::WrapUnique().
  return base::WrapUnique<VideoDecoderMixin>(new OOPVideoDecoder(
      std::move(media_log), std::move(decoder_task_runner), std::move(client),
      std::move(pending_remote_decoder)));
}

OOPVideoDecoder::OOPVideoDecoder(
    std::unique_ptr<media::MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client,
    mojo::PendingRemote<stable::mojom::StableVideoDecoder>
        pending_remote_decoder)
    : VideoDecoderMixin(std::move(media_log),
                        std::move(decoder_task_runner),
                        std::move(client)),
      remote_decoder_(std::move(pending_remote_decoder)),
      weak_this_factory_(this) {
  VLOGF(2);
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Set a connection error handler in case the remote decoder gets
  // disconnected, for instance, if the remote decoder process crashes.
  // The remote decoder lives in a utility process (for lacros-chrome,
  // this utility process is in ash-chrome).
  // base::Unretained() is safe because `this` owns the `mojo::Remote`.
  remote_decoder_.set_disconnect_handler(
      base::BindOnce(&OOPVideoDecoder::Stop, base::Unretained(this)));

  // TODO(b/195769334): |remote_consumer_handle| corresponds to the data pipe
  // that allows us to send data to the out-of-process video decoder. This data
  // pipe is separate from the one set up by renderers to send data to the GPU
  // process. Therefore, we're introducing the need for copying the encoded data
  // from one pipe to the other. Ideally, we would just forward the pipe
  // endpoint directly to the out-of-process video decoder and avoid the extra
  // copy. This would require us to plumb the mojo::ScopedDataPipeConsumerHandle
  // from the MojoVideoDecoderService all the way here.
  mojo::ScopedDataPipeConsumerHandle remote_consumer_handle;
  mojo_decoder_buffer_writer_ = MojoDecoderBufferWriter::Create(
      GetDefaultDecoderBufferConverterCapacity(DemuxerStream::VIDEO),
      &remote_consumer_handle);
  CHECK(mojo_decoder_buffer_writer_);

  DCHECK(!stable_video_frame_handle_releaser_remote_.is_bound());
  mojo::PendingReceiver<stable::mojom::VideoFrameHandleReleaser>
      stable_video_frame_handle_releaser_receiver =
          stable_video_frame_handle_releaser_remote_
              .BindNewPipeAndPassReceiver();

  // base::Unretained() is safe because `this` owns the `mojo::Remote`.
  stable_video_frame_handle_releaser_remote_.set_disconnect_handler(
      base::BindOnce(&OOPVideoDecoder::Stop, base::Unretained(this)));

  DCHECK(!stable_media_log_receiver_.is_bound());

  CHECK(!has_error_);
  // TODO(b/171813538): plumb the remaining parameters.
  remote_decoder_->Construct(
      client_receiver_.BindNewEndpointAndPassRemote(),
      stable_media_log_receiver_.BindNewPipeAndPassRemote(),
      std::move(stable_video_frame_handle_releaser_receiver),
      std::move(remote_consumer_handle), gfx::ColorSpace());
}

OOPVideoDecoder::~OOPVideoDecoder() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& pending_decode : pending_decodes_) {
    decoder_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(pending_decode.second),
                                  DecoderStatus::Codes::kAborted));
  }
}

void OOPVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                 bool low_delay,
                                 CdmContext* cdm_context,
                                 InitCB init_cb,
                                 const OutputCB& output_cb,
                                 const WaitingCB& waiting_cb) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!init_cb_);
  CHECK(pending_decodes_.empty());
  CHECK(!reset_cb_);

  if (has_error_) {
    // TODO(b/171813538): create specific error code for this decoder.
    std::move(init_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  // TODO(b/171813538): implement the re-initialization logic. The client can
  // call Initialize() multiple times. This implies properly asserting the
  // invariant imposed by media::VideoDecoder::Initialize(): "No VideoDecoder
  // calls should be made before |init_cb| is executed."
  if (output_cb_) {
    // TODO(b/171813538): create specific error code for this decoder.
    std::move(init_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  init_cb_ = std::move(init_cb);
  output_cb_ = output_cb;
  waiting_cb_ = waiting_cb;

  // TODO(b/171813538): plumb protected content.
  mojo::PendingRemote<stable::mojom::StableCdmContext>
      pending_remote_cdm_context;

  remote_decoder_->Initialize(config, low_delay,
                              std::move(pending_remote_cdm_context),
                              base::BindOnce(&OOPVideoDecoder::OnInitializeDone,
                                             weak_this_factory_.GetWeakPtr()));
}

void OOPVideoDecoder::OnInitializeDone(const DecoderStatus& status,
                                       bool needs_bitstream_conversion,
                                       int32_t max_decode_requests,
                                       VideoDecoderType decoder_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!has_error_);

  if (!status.is_ok() || (decoder_type != VideoDecoderType::kVaapi &&
                          decoder_type != VideoDecoderType::kV4L2)) {
    Stop();
    return;
  }
  decoder_type_ = decoder_type;
  std::move(init_cb_).Run(status);
}

void OOPVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                             DecodeCB decode_cb) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!init_cb_);
  CHECK(!reset_cb_);

  if (has_error_ || decoder_type_ == VideoDecoderType::kUnknown) {
    decoder_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(decode_cb),
                                  DecoderStatus::Codes::kNotInitialized));
    return;
  }

  if (decode_counter_ == std::numeric_limits<uint64_t>::max()) {
    // Error out in case of overflow.
    decoder_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb), DecoderStatus::Codes::kFailed));
    return;
  }
  const uint64_t decode_id = decode_counter_++;

  pending_decodes_.insert({decode_id, std::move(decode_cb)});

  const bool is_flushing = buffer->end_of_stream();

  CHECK(buffer);

  mojom::DecoderBufferPtr mojo_buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(buffer);
  if (!mojo_buffer) {
    Stop();
    return;
  }

  remote_decoder_->Decode(
      std::move(buffer),
      base::BindOnce(&OOPVideoDecoder::OnDecodeDone,
                     weak_this_factory_.GetWeakPtr(), decode_id, is_flushing));
}

void OOPVideoDecoder::OnDecodeDone(uint64_t decode_id,
                                   bool is_flushing,
                                   const DecoderStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!has_error_);

  // Check that decode callbacks are called in the same order as Decode().
  CHECK(!pending_decodes_.empty());
  if (pending_decodes_.cbegin()->first != decode_id) {
    VLOGF(2) << "Unexpected decode callback for request " << decode_id;
    Stop();
    return;
  }

  // Check that the |decode_cb| corresponding to the flush is not called until
  // the decode callback has been called for each pending decode.
  if (is_flushing && pending_decodes_.size() != 1) {
    VLOGF(2) << "Received a flush callback while having pending decodes";
    Stop();
    return;
  }

  auto it = pending_decodes_.begin();
  DecodeCB decode_cb = std::move(it->second);
  pending_decodes_.erase(it);
  std::move(decode_cb).Run(status);
}

void OOPVideoDecoder::Reset(base::OnceClosure reset_cb) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!init_cb_);
  CHECK(!reset_cb_);

  if (has_error_ || decoder_type_ == VideoDecoderType::kUnknown) {
    std::move(reset_cb).Run();
    return;
  }

  reset_cb_ = std::move(reset_cb);
  remote_decoder_->Reset(base::BindOnce(&OOPVideoDecoder::OnResetDone,
                                        weak_this_factory_.GetWeakPtr()));
}

void OOPVideoDecoder::OnResetDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!has_error_);
  CHECK(reset_cb_);
  if (!pending_decodes_.empty()) {
    VLOGF(2) << "Received a reset callback while having pending decodes";
    Stop();
    return;
  }
  std::move(reset_cb_).Run();
}

void OOPVideoDecoder::Stop() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (has_error_)
    return;

  has_error_ = true;

  // There may be in-flight decode, initialize or reset callbacks.
  // Invalidate any outstanding weak pointers so those callbacks are ignored.
  weak_this_factory_.InvalidateWeakPtrs();

  // |init_cb_| is likely to reentrantly destruct |this|, so we check for that
  // using an on-stack WeakPtr.
  base::WeakPtr<OOPVideoDecoder> weak_this = weak_this_factory_.GetWeakPtr();

  client_receiver_.reset();
  stable_media_log_receiver_.reset();
  remote_decoder_.reset();
  mojo_decoder_buffer_writer_.reset();
  stable_video_frame_handle_releaser_remote_.reset();

  if (init_cb_)
    std::move(init_cb_).Run(DecoderStatus::Codes::kFailed);

  if (!weak_this)
    return;

  for (auto& pending_decode : pending_decodes_) {
    std::move(pending_decode.second).Run(DecoderStatus::Codes::kFailed);
    if (!weak_this)
      return;
  }
  pending_decodes_.clear();

  if (reset_cb_)
    std::move(reset_cb_).Run();
}

void OOPVideoDecoder::ReleaseVideoFrame(
    const base::UnguessableToken& release_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!has_error_);
  CHECK(stable_video_frame_handle_releaser_remote_.is_bound());

  stable_video_frame_handle_releaser_remote_->ReleaseVideoFrame(release_token);
}

void OOPVideoDecoder::ApplyResolutionChange() {
  NOTREACHED();
}

bool OOPVideoDecoder::NeedsBitstreamConversion() const {
  NOTIMPLEMENTED();
  NOTREACHED();
  return false;
}

bool OOPVideoDecoder::CanReadWithoutStalling() const {
  NOTIMPLEMENTED();
  NOTREACHED();
  return true;
}

int OOPVideoDecoder::GetMaxDecodeRequests() const {
  NOTIMPLEMENTED();
  NOTREACHED();
  return 4;
}

VideoDecoderType OOPVideoDecoder::GetDecoderType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return decoder_type_;
}

bool OOPVideoDecoder::IsPlatformDecoder() const {
  NOTIMPLEMENTED();
  NOTREACHED();
  return true;
}

bool OOPVideoDecoder::NeedsTranscryption() {
  return false;
}

void OOPVideoDecoder::OnVideoFrameDecoded(
    const scoped_refptr<VideoFrame>& frame,
    bool can_read_without_stalling,
    const base::UnguessableToken& release_token) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!has_error_);

  // The destruction observer will be called after the client releases the
  // video frame. BindToCurrentLoop() is used to make sure that the WeakPtr
  // is dereferenced on the correct sequence.
  frame->AddDestructionObserver(BindToCurrentLoop(
      base::BindOnce(&OOPVideoDecoder::ReleaseVideoFrame,
                     weak_this_factory_.GetWeakPtr(), release_token)));

  // TODO(b/220915557): validate |frame|.
  if (output_cb_)
    output_cb_.Run(frame);
}

void OOPVideoDecoder::OnWaiting(WaitingReason reason) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!has_error_);

  if (waiting_cb_)
    waiting_cb_.Run(reason);
}

void OOPVideoDecoder::AddLogRecord(const MediaLogRecord& event) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (media_log_)
    media_log_->AddLogRecord(std::make_unique<media::MediaLogRecord>(event));
}

}  // namespace media
