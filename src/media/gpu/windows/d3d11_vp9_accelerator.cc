// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_vp9_accelerator.h"

#include <windows.h>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "media/gpu/windows/d3d11_vp9_picture.h"

namespace media {

#define RECORD_FAILURE(expr_name, expr_value)           \
  do {                                                  \
    media_log_->AddEvent(media_log_->CreateStringEvent( \
        MediaLogEvent::MEDIA_ERROR_LOG_ENTRY, "error",  \
        std::string("DX11VP9Failure(") + expr_name +    \
            ")=" + std::to_string(expr_value)));        \
  } while (0)

#define RETURN_ON_HR_FAILURE(expr_name, expr) \
  do {                                        \
    HRESULT expr_value = (expr);              \
    if (FAILED(expr_value)) {                 \
      RECORD_FAILURE(#expr_name, expr_value); \
      return false;                           \
    }                                         \
  } while (0)

D3D11VP9Accelerator::D3D11VP9Accelerator(
    D3D11VideoDecoderClient* client,
    MediaLog* media_log,
    Microsoft::WRL::ComPtr<ID3D11VideoDecoder> video_decoder,
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device,
    Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context)
    : client_(client),
      media_log_(media_log),
      status_feedback_(0),
      video_decoder_(std::move(video_decoder)),
      video_device_(std::move(video_device)),
      video_context_(std::move(video_context)) {}

D3D11VP9Accelerator::~D3D11VP9Accelerator() {}

scoped_refptr<VP9Picture> D3D11VP9Accelerator::CreateVP9Picture() {
  D3D11PictureBuffer* picture_buffer = client_->GetPicture();
  if (!picture_buffer)
    return nullptr;
  return base::MakeRefCounted<D3D11VP9Picture>(picture_buffer);
}

bool D3D11VP9Accelerator::BeginFrame(D3D11VP9Picture* pic) {
  Microsoft::WRL::ComPtr<ID3D11VideoDecoderOutputView> output_view;
  D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC view_desc = {
      .DecodeProfile = D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0,
      .ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D,
      .Texture2D = {.ArraySlice = (UINT)pic->level()}};

  RETURN_ON_HR_FAILURE(CreateVideoDecoderOutputView,
                       video_device_->CreateVideoDecoderOutputView(
                           pic->picture_buffer()->texture().Get(), &view_desc,
                           output_view.GetAddressOf()));

  HRESULT hr;
  do {
    hr = video_context_->DecoderBeginFrame(video_decoder_.Get(),
                                           output_view.Get(), 0, nullptr);
  } while (hr == E_PENDING || hr == D3DERR_WASSTILLDRAWING);

  if (FAILED(hr)) {
    RECORD_FAILURE("DecoderBeginFrame", hr);
    return false;
  }

  return true;
}

void D3D11VP9Accelerator::CopyFrameParams(
    const scoped_refptr<D3D11VP9Picture>& pic,
    DXVA_PicParams_VP9* pic_params) {
#define SET_PARAM(a, b) pic_params->a = pic->frame_hdr->b
#define COPY_PARAM(a) SET_PARAM(a, a)

  COPY_PARAM(profile);
  COPY_PARAM(show_frame);
  COPY_PARAM(error_resilient_mode);
  COPY_PARAM(refresh_frame_context);
  COPY_PARAM(frame_parallel_decoding_mode);
  COPY_PARAM(intra_only);
  COPY_PARAM(frame_context_idx);
  COPY_PARAM(allow_high_precision_mv);
  COPY_PARAM(refresh_frame_context);
  COPY_PARAM(frame_parallel_decoding_mode);
  COPY_PARAM(intra_only);
  COPY_PARAM(frame_context_idx);
  COPY_PARAM(allow_high_precision_mv);

  // extra_plane, BitDepthMinus8Luma, and BitDepthMinus8Chroma are initialized
  // at 0 already.

  pic_params->CurrPic.Index7Bits = pic->level();
  pic_params->frame_type = !pic->frame_hdr->IsKeyframe();
  pic_params->subsampling_x = pic->frame_hdr->subsampling_x == 1;
  pic_params->subsampling_y = pic->frame_hdr->subsampling_y == 1;

  SET_PARAM(width, frame_width);
  SET_PARAM(height, frame_height);
  SET_PARAM(interp_filter, interpolation_filter);
  SET_PARAM(log2_tile_cols, tile_cols_log2);
  SET_PARAM(log2_tile_rows, tile_rows_log2);
#undef COPY_PARAM
#undef SET_PARAM
}

void D3D11VP9Accelerator::CopyReferenceFrames(
    const scoped_refptr<D3D11VP9Picture>& pic,
    DXVA_PicParams_VP9* pic_params,
    const std::vector<scoped_refptr<VP9Picture>>& reference_pictures) {
  D3D11_TEXTURE2D_DESC texture_descriptor;
  pic->picture_buffer()->texture()->GetDesc(&texture_descriptor);

  for (size_t i = 0; i < base::size(pic_params->ref_frame_map); i++) {
    if (i < reference_pictures.size() && reference_pictures[i].get()) {
      scoped_refptr<D3D11VP9Picture> our_ref_pic(
          static_cast<D3D11VP9Picture*>(reference_pictures[i].get()));
      pic_params->ref_frame_map[i].Index7Bits = our_ref_pic->level();
      pic_params->ref_frame_coded_width[i] = texture_descriptor.Width;
      pic_params->ref_frame_coded_height[i] = texture_descriptor.Height;
    } else {
      pic_params->ref_frame_map[i].bPicEntry = 0xff;
      pic_params->ref_frame_coded_width[i] = 0;
      pic_params->ref_frame_coded_height[i] = 0;
    }
  }
}

void D3D11VP9Accelerator::CopyFrameRefs(
    DXVA_PicParams_VP9* pic_params,
    const scoped_refptr<D3D11VP9Picture>& pic) {
  for (size_t i = 0; i < base::size(pic_params->frame_refs); i++) {
    pic_params->frame_refs[i] =
        pic_params->ref_frame_map[pic->frame_hdr->ref_frame_idx[i]];
  }

  for (size_t i = 0; i < base::size(pic_params->ref_frame_sign_bias); i++) {
    pic_params->ref_frame_sign_bias[i] = pic->frame_hdr->ref_frame_sign_bias[i];
  }
}

void D3D11VP9Accelerator::CopyLoopFilterParams(
    DXVA_PicParams_VP9* pic_params,
    const Vp9LoopFilterParams& loop_filter_params) {
#define SET_PARAM(a, b) pic_params->a = loop_filter_params.b
  SET_PARAM(filter_level, level);
  SET_PARAM(sharpness_level, sharpness);
  SET_PARAM(mode_ref_delta_enabled, delta_enabled);
  SET_PARAM(mode_ref_delta_update, delta_update);
#undef SET_PARAM

  // base::size(...) doesn't work well in an array initializer.
  DCHECK_EQ(4lu, base::size(pic_params->ref_deltas));
  int ref_deltas[4] = {0};
  for (size_t i = 0; i < base::size(pic_params->ref_deltas); i++) {
    if (loop_filter_params.update_ref_deltas[i])
      ref_deltas[i] = loop_filter_params.ref_deltas[i];
    pic_params->ref_deltas[i] = ref_deltas[i];
  }

  int mode_deltas[2] = {0};
  DCHECK_EQ(2lu, base::size(pic_params->mode_deltas));
  for (size_t i = 0; i < base::size(pic_params->mode_deltas); i++) {
    if (loop_filter_params.update_mode_deltas[i])
      mode_deltas[i] = loop_filter_params.mode_deltas[i];
    pic_params->mode_deltas[i] = mode_deltas[i];
  }
}

void D3D11VP9Accelerator::CopyQuantParams(
    DXVA_PicParams_VP9* pic_params,
    const scoped_refptr<D3D11VP9Picture>& pic) {
#define SET_PARAM(a, b) pic_params->a = pic->frame_hdr->quant_params.b
  SET_PARAM(base_qindex, base_q_idx);
  SET_PARAM(y_dc_delta_q, delta_q_y_dc);
  SET_PARAM(uv_dc_delta_q, delta_q_uv_dc);
  SET_PARAM(uv_ac_delta_q, delta_q_uv_ac);
#undef SET_PARAM
}

void D3D11VP9Accelerator::CopySegmentationParams(
    DXVA_PicParams_VP9* pic_params,
    const Vp9SegmentationParams& segmentation_params) {
#define SET_PARAM(a, b) pic_params->stVP9Segments.a = segmentation_params.b
#define COPY_PARAM(a) SET_PARAM(a, a)
  COPY_PARAM(enabled);
  COPY_PARAM(update_map);
  COPY_PARAM(temporal_update);
  SET_PARAM(abs_delta, abs_or_delta_update);

  for (size_t i = 0; i < base::size(segmentation_params.tree_probs); i++) {
    COPY_PARAM(tree_probs[i]);
  }

  for (size_t i = 0; i < base::size(segmentation_params.pred_probs); i++) {
    COPY_PARAM(pred_probs[i]);
  }

  for (size_t i = 0; i < 8; i++) {
    for (size_t j = 0; j < 4; j++) {
      COPY_PARAM(feature_data[i][j]);
      if (segmentation_params.feature_enabled[i][j])
        pic_params->stVP9Segments.feature_mask[i] |= (1 << j);
    }
  }
#undef COPY_PARAM
#undef SET_PARAM
}

void D3D11VP9Accelerator::CopyHeaderSizeAndID(
    DXVA_PicParams_VP9* pic_params,
    const scoped_refptr<D3D11VP9Picture>& pic) {
  pic_params->uncompressed_header_size_byte_aligned =
      static_cast<USHORT>(pic->frame_hdr->uncompressed_header_size);
  pic_params->first_partition_size =
      static_cast<USHORT>(pic->frame_hdr->header_size_in_bytes);

  pic_params->StatusReportFeedbackNumber = status_feedback_++;
}

bool D3D11VP9Accelerator::SubmitDecoderBuffer(
    const DXVA_PicParams_VP9& pic_params,
    const scoped_refptr<D3D11VP9Picture>& pic) {
#define GET_BUFFER(type)                                 \
  RETURN_ON_HR_FAILURE(GetDecoderBuffer,                 \
                       video_context_->GetDecoderBuffer( \
                           video_decoder_.Get(), type, &buffer_size, &buffer))
#define RELEASE_BUFFER(type) \
  RETURN_ON_HR_FAILURE(      \
      ReleaseDecoderBuffer,  \
      video_context_->ReleaseDecoderBuffer(video_decoder_.Get(), type))

  UINT buffer_size;
  void* buffer;

  GET_BUFFER(D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS);
  memcpy(buffer, &pic_params, sizeof(pic_params));
  RELEASE_BUFFER(D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS);

  size_t buffer_offset = 0;
  while (buffer_offset < pic->frame_hdr->frame_size) {
    GET_BUFFER(D3D11_VIDEO_DECODER_BUFFER_BITSTREAM);
    size_t copy_size = pic->frame_hdr->frame_size - buffer_offset;
    bool contains_end = true;
    if (copy_size > buffer_size) {
      copy_size = buffer_size;
      contains_end = false;
    }
    memcpy(buffer, pic->frame_hdr->data + buffer_offset, copy_size);
    RELEASE_BUFFER(D3D11_VIDEO_DECODER_BUFFER_BITSTREAM);

    DXVA_Slice_VPx_Short slice_info;

    GET_BUFFER(D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL);
    slice_info.BSNALunitDataLocation = 0;
    slice_info.SliceBytesInBuffer = (UINT)copy_size;

    // See the DXVA header specification for values of wBadSliceChopping:
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/content/dxva/ns-dxva-_dxva_sliceinfo#wBadSliceChopping
    if (buffer_offset == 0 && contains_end)
      slice_info.wBadSliceChopping = 0;
    else if (buffer_offset == 0 && !contains_end)
      slice_info.wBadSliceChopping = 1;
    else if (buffer_offset != 0 && contains_end)
      slice_info.wBadSliceChopping = 2;
    else if (buffer_offset != 0 && !contains_end)
      slice_info.wBadSliceChopping = 3;

    memcpy(buffer, &slice_info, sizeof(slice_info));
    RELEASE_BUFFER(D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL);

    constexpr int buffers_count = 3;
    D3D11_VIDEO_DECODER_BUFFER_DESC buffers[buffers_count] = {};
    buffers[0].BufferType = D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS;
    buffers[0].DataOffset = 0;
    buffers[0].DataSize = sizeof(pic_params);
    buffers[1].BufferType = D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL;
    buffers[1].DataOffset = 0;
    buffers[1].DataSize = sizeof(slice_info);
    buffers[2].BufferType = D3D11_VIDEO_DECODER_BUFFER_BITSTREAM;
    buffers[2].DataOffset = 0;
    buffers[2].DataSize = copy_size;

    RETURN_ON_HR_FAILURE(SubmitDecoderBuffers,
                         video_context_->SubmitDecoderBuffers(
                             video_decoder_.Get(), buffers_count, buffers));
    buffer_offset += copy_size;
  }

  return true;
#undef GET_BUFFER
#undef RELEASE_BUFFER
}

bool D3D11VP9Accelerator::SubmitDecode(
    const scoped_refptr<VP9Picture>& picture,
    const Vp9SegmentationParams& segmentation_params,
    const Vp9LoopFilterParams& loop_filter_params,
    const std::vector<scoped_refptr<VP9Picture>>& reference_pictures,
    const base::Closure& on_finished_cb) {
  scoped_refptr<D3D11VP9Picture> pic(
      static_cast<D3D11VP9Picture*>(picture.get()));

  if (!BeginFrame(pic.get()))
    return false;

  DXVA_PicParams_VP9 pic_params = {};
  CopyFrameParams(pic, &pic_params);
  CopyReferenceFrames(pic, &pic_params, reference_pictures);
  CopyFrameRefs(&pic_params, pic);
  CopyLoopFilterParams(&pic_params, loop_filter_params);
  CopyQuantParams(&pic_params, pic);
  CopySegmentationParams(&pic_params, segmentation_params);
  CopyHeaderSizeAndID(&pic_params, pic);

  if (!SubmitDecoderBuffer(pic_params, pic))
    return false;

  RETURN_ON_HR_FAILURE(DecoderEndFrame,
                       video_context_->DecoderEndFrame(video_decoder_.Get()));
  if (on_finished_cb)
    on_finished_cb.Run();
  return true;
}

bool D3D11VP9Accelerator::OutputPicture(
    const scoped_refptr<VP9Picture>& picture) {
  D3D11VP9Picture* pic = static_cast<D3D11VP9Picture*>(picture.get());
  client_->OutputResult(picture.get(), pic->picture_buffer());
  return true;
}

bool D3D11VP9Accelerator::IsFrameContextRequired() const {
  return false;
}

bool D3D11VP9Accelerator::GetFrameContext(
    const scoped_refptr<VP9Picture>& picture,
    Vp9FrameContext* frame_context) {
  return false;
}

}  // namespace media
