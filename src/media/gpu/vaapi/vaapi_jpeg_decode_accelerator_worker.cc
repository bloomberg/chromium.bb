// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_jpeg_decode_accelerator_worker.h"

#include <utility>

#include <va/va.h>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class VAJDAWorkerDecoderFailure {
  kVaapiError = 0,
  kMaxValue = kVaapiError,
};

void ReportToVAJDAWorkerDecoderFailureUMA(VAJDAWorkerDecoderFailure failure) {
  UMA_HISTOGRAM_ENUMERATION("Media.VAJDAWorker.DecoderFailure", failure);
}

// Uses |decoder| to decode the JPEG corresponding to |encoded_data|.
// |decode_cb| is called when finished or when an error is encountered. We don't
// support decoding to scale, so |output_size| is only used for tracing.
void DecodeTask(
    VaapiJpegDecoder* decoder,
    std::vector<uint8_t> encoded_data,
    const gfx::Size& output_size,
    gpu::ImageDecodeAcceleratorWorker::CompletedDecodeCB decode_cb) {
  TRACE_EVENT2("jpeg", "VaapiJpegDecodeAcceleratorWorker::DecodeTask",
               "encoded_bytes", encoded_data.size(), "output_size",
               output_size.ToString());
  gpu::ImageDecodeAcceleratorWorker::CompletedDecodeCB scoped_decode_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(decode_cb),
                                                  nullptr);
  VaapiJpegDecodeStatus status;
  decoder->Decode(
      base::make_span<const uint8_t>(encoded_data.data(), encoded_data.size()),
      &status);
  if (status != VaapiJpegDecodeStatus::kSuccess) {
    VLOGF(1) << "Failed to decode - status = " << static_cast<uint32_t>(status);
    return;
  }
  std::unique_ptr<ScopedVAImage> scoped_image =
      decoder->GetImage(VA_FOURCC_RGBX /* preferred_image_fourcc */, &status);
  if (status != VaapiJpegDecodeStatus::kSuccess) {
    VLOGF(1) << "Failed to get image - status = "
             << static_cast<uint32_t>(status);
    return;
  }

  // TODO(crbug.com/868400): output the decoded data.
  DCHECK(scoped_image);
  std::move(scoped_decode_callback).Run(nullptr);
}

}  // namespace

VaapiJpegDecodeAcceleratorWorker::VaapiJpegDecodeAcceleratorWorker()
    : decoder_(std::make_unique<VaapiJpegDecoder>()) {
  if (!decoder_->Initialize(
          base::BindRepeating(&ReportToVAJDAWorkerDecoderFailureUMA,
                              VAJDAWorkerDecoderFailure::kVaapiError))) {
    return;
  }
  decoder_task_runner_ = base::CreateSequencedTaskRunnerWithTraits({});
  DCHECK(decoder_task_runner_);
}

VaapiJpegDecodeAcceleratorWorker::~VaapiJpegDecodeAcceleratorWorker() {
  if (decoder_task_runner_)
    decoder_task_runner_->DeleteSoon(FROM_HERE, std::move(decoder_));
}

bool VaapiJpegDecodeAcceleratorWorker::IsValid() const {
  // If |decoder_task_runner_| is nullptr, it means that the initialization of
  // |decoder_| failed.
  return !!decoder_task_runner_;
}

void VaapiJpegDecodeAcceleratorWorker::Decode(std::vector<uint8_t> encoded_data,
                                              const gfx::Size& output_size,
                                              CompletedDecodeCB decode_cb) {
  if (!IsValid()) {
    NOTREACHED();
    return;
  }
  DCHECK(!decoder_task_runner_->RunsTasksInCurrentSequence());
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DecodeTask, decoder_.get(), std::move(encoded_data),
                     output_size, std::move(decode_cb)));
}

}  // namespace media
