// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/h264_vaapi_video_encoder_delegate.h"

#include <va/va.h>
#include <va/va_enc_h264.h>

#include "base/bits.h"
#include "base/cxx17_backports.h"
#include "base/memory/ref_counted_memory.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/video/h264_level_limits.h"

namespace media {
namespace {
// An IDR every 2048 frames, no I frames and no B frames.
// We choose IDR period to equal MaxFrameNum so it must be a power of 2.
constexpr int kIDRPeriod = 2048;
constexpr int kIPeriod = 0;
constexpr int kIPPeriod = 1;

// The qp range is 0-51 in H264. Select 26 because of the center value.
constexpr uint8_t kDefaultQP = 26;
// Note: Webrtc default values are 24 and 37 respectively, see
// h264_encoder_impl.cc.
// These values are selected to make our VEA tests pass.
constexpr uint8_t kMinQP = 24;
constexpr uint8_t kMaxQP = 42;

// Subjectively chosen bitrate window size for rate control, in ms.
constexpr int kCPBWindowSizeMs = 1500;

// Subjectively chosen.
constexpr size_t kMaxNumReferenceFrames = 4;
constexpr size_t kMaxRefIdxL0Size = kMaxNumReferenceFrames;
constexpr size_t kMaxRefIdxL1Size = 0;

// HRD parameters (ch. E.2.2 in H264 spec).
constexpr int kBitRateScale = 0;  // bit_rate_scale for SPS HRD parameters.
constexpr int kCPBSizeScale = 0;  // cpb_size_scale for SPS HRD parameters.

// 4:2:0
constexpr int kChromaFormatIDC = 1;

void FillVAEncRateControlParams(
    uint32_t bps,
    uint32_t window_size,
    uint32_t initial_qp,
    uint32_t min_qp,
    uint32_t max_qp,
    uint32_t framerate,
    uint32_t buffer_size,
    VAEncMiscParameterRateControl& rate_control_param,
    VAEncMiscParameterFrameRate& framerate_param,
    VAEncMiscParameterHRD& hrd_param) {
  memset(&rate_control_param, 0, sizeof(rate_control_param));
  rate_control_param.bits_per_second = bps;
  rate_control_param.window_size = window_size;
  rate_control_param.initial_qp = initial_qp;
  rate_control_param.min_qp = min_qp;
  rate_control_param.max_qp = max_qp;
  rate_control_param.rc_flags.bits.disable_frame_skip = true;

  memset(&framerate_param, 0, sizeof(framerate_param));
  framerate_param.framerate = framerate;

  memset(&hrd_param, 0, sizeof(hrd_param));
  hrd_param.buffer_size = buffer_size;
  hrd_param.initial_buffer_fullness = buffer_size / 2;
}

static scoped_refptr<base::RefCountedBytes> MakeRefCountedBytes(void* ptr,
                                                                size_t size) {
  return base::MakeRefCounted<base::RefCountedBytes>(
      reinterpret_cast<uint8_t*>(ptr), size);
}

static void InitVAPictureH264(VAPictureH264* va_pic) {
  *va_pic = {};
  va_pic->picture_id = VA_INVALID_ID;
  va_pic->flags = VA_PICTURE_H264_INVALID;
}

}  // namespace

H264VaapiVideoEncoderDelegate::EncodeParams::EncodeParams()
    : idr_period_frames(kIDRPeriod),
      i_period_frames(kIPeriod),
      ip_period_frames(kIPPeriod),
      bitrate_bps(0),
      framerate(0),
      cpb_window_size_ms(kCPBWindowSizeMs),
      cpb_size_bits(0),
      initial_qp(kDefaultQP),
      min_qp(kMinQP),
      max_qp(kMaxQP),
      max_num_ref_frames(kMaxNumReferenceFrames),
      max_ref_pic_list0_size(kMaxRefIdxL0Size),
      max_ref_pic_list1_size(kMaxRefIdxL1Size) {}

H264VaapiVideoEncoderDelegate::H264VaapiVideoEncoderDelegate(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    base::RepeatingClosure error_cb)
    : VaapiVideoEncoderDelegate(std::move(vaapi_wrapper), error_cb),
      packed_sps_(new H264BitstreamBuffer()),
      packed_pps_(new H264BitstreamBuffer()) {}

H264VaapiVideoEncoderDelegate::~H264VaapiVideoEncoderDelegate() {
  // H264VaapiVideoEncoderDelegate can be destroyed on any thread.
}

bool H264VaapiVideoEncoderDelegate::Initialize(
    const VideoEncodeAccelerator::Config& config,
    const VaapiVideoEncoderDelegate::Config& ave_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (config.output_profile) {
    case H264PROFILE_BASELINE:
    case H264PROFILE_MAIN:
    case H264PROFILE_HIGH:
      break;

    default:
      NOTIMPLEMENTED() << "Unsupported profile "
                       << GetProfileName(config.output_profile);
      return false;
  }

  if (config.input_visible_size.IsEmpty()) {
    DVLOGF(1) << "Input visible size could not be empty";
    return false;
  }
  visible_size_ = config.input_visible_size;
  // For 4:2:0, the pixel sizes have to be even.
  if ((visible_size_.width() % 2 != 0) || (visible_size_.height() % 2 != 0)) {
    DVLOGF(1) << "The pixel sizes are not even: " << visible_size_.ToString();
    return false;
  }
  constexpr size_t kH264MacroblockSizeInPixels = 16;
  coded_size_ = gfx::Size(
      base::bits::AlignUp(visible_size_.width(), kH264MacroblockSizeInPixels),
      base::bits::AlignUp(visible_size_.height(), kH264MacroblockSizeInPixels));
  mb_width_ = coded_size_.width() / kH264MacroblockSizeInPixels;
  mb_height_ = coded_size_.height() / kH264MacroblockSizeInPixels;

  profile_ = config.output_profile;
  level_ = config.h264_output_level.value_or(H264SPS::kLevelIDC4p0);
  uint32_t initial_framerate = config.initial_framerate.value_or(
      VideoEncodeAccelerator::kDefaultFramerate);

  // Checks if |level_| is valid. If it is invalid, set |level_| to a minimum
  // level that comforts Table A-1 in H.264 spec with specified bitrate,
  // framerate and dimension.
  if (!CheckH264LevelLimits(profile_, level_, config.bitrate.target(),
                            initial_framerate, mb_width_ * mb_height_)) {
    absl::optional<uint8_t> valid_level =
        FindValidH264Level(profile_, config.bitrate.target(), initial_framerate,
                           mb_width_ * mb_height_);
    if (!valid_level) {
      VLOGF(1) << "Could not find a valid h264 level for"
               << " profile=" << profile_
               << " bitrate=" << config.bitrate.target()
               << " framerate=" << initial_framerate
               << " size=" << config.input_visible_size.ToString();
      return false;
    }
    level_ = *valid_level;
  }

  curr_params_.max_ref_pic_list0_size =
      std::min(kMaxRefIdxL0Size, ave_config.max_num_ref_frames & 0xffff);
  curr_params_.max_ref_pic_list1_size = std::min(
      kMaxRefIdxL1Size, (ave_config.max_num_ref_frames >> 16) & 0xffff);
  curr_params_.max_num_ref_frames =
      std::min(kMaxNumReferenceFrames, curr_params_.max_ref_pic_list0_size +
                                           curr_params_.max_ref_pic_list1_size);

  VideoBitrateAllocation initial_bitrate_allocation;
  initial_bitrate_allocation.SetBitrate(0, 0, config.bitrate.target());
  if (!UpdateRates(initial_bitrate_allocation, initial_framerate))
    return false;

  UpdateSPS();
  UpdatePPS();

  return true;
}

gfx::Size H264VaapiVideoEncoderDelegate::GetCodedSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!coded_size_.IsEmpty());

  return coded_size_;
}

size_t H264VaapiVideoEncoderDelegate::GetMaxNumOfRefFrames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return curr_params_.max_num_ref_frames;
}

std::vector<gfx::Size> H264VaapiVideoEncoderDelegate::GetSVCLayerResolutions() {
  return {visible_size_};
}

bool H264VaapiVideoEncoderDelegate::PrepareEncodeJob(EncodeJob* encode_job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<H264Picture> pic = GetPicture(encode_job);
  DCHECK(pic);

  if (encode_job->IsKeyframeRequested() || encoding_parameters_changed_)
    frame_num_ = 0;

  pic->frame_num = frame_num_++;
  frame_num_ %= curr_params_.idr_period_frames;

  if (pic->frame_num == 0) {
    pic->idr = true;
    // H264 spec mandates idr_pic_id to differ between two consecutive IDRs.
    idr_pic_id_ ^= 1;
    pic->idr_pic_id = idr_pic_id_;
    ref_pic_list0_.clear();

    encoding_parameters_changed_ = false;
    encode_job->ProduceKeyframe();
  }

  if (pic->idr || (curr_params_.i_period_frames != 0 &&
                   pic->frame_num % curr_params_.i_period_frames == 0)) {
    pic->type = H264SliceHeader::kISlice;
  } else {
    pic->type = H264SliceHeader::kPSlice;
  }
  if (curr_params_.ip_period_frames != 1) {
    NOTIMPLEMENTED() << "B frames not implemented";
    return false;
  }

  pic->ref = true;
  pic->pic_order_cnt = pic->frame_num * 2;
  pic->top_field_order_cnt = pic->pic_order_cnt;
  pic->pic_order_cnt_lsb = pic->pic_order_cnt;

  DVLOGF(4) << "Starting a new frame, type: " << pic->type
            << (encode_job->IsKeyframeRequested() ? " (keyframe)" : "")
            << " frame_num: " << pic->frame_num
            << " POC: " << pic->pic_order_cnt;

  if (!SubmitFrameParameters(encode_job, curr_params_, current_sps_,
                             current_pps_, pic, ref_pic_list0_,
                             std::list<scoped_refptr<H264Picture>>())) {
    DVLOGF(1) << "Failed submitting frame parameters";
    return false;
  }

  if (pic->type == H264SliceHeader::kISlice) {
    // We always generate SPS and PPS with I(DR) frame. This will help for Seek
    // operation on the generated stream.
    if (!SubmitPackedHeaders(encode_job, packed_sps_, packed_pps_)) {
      DVLOGF(1) << "Failed submitting keyframe headers";
      return false;
    }
  }

  for (const auto& ref_pic : ref_pic_list0_)
    encode_job->AddReferencePicture(ref_pic);

  // Store the picture on the list of reference pictures and keep the list
  // below maximum size, dropping oldest references.
  if (pic->ref) {
    ref_pic_list0_.push_front(pic);
    const size_t max_num_ref_frames =
        base::checked_cast<size_t>(current_sps_.max_num_ref_frames);
    while (ref_pic_list0_.size() > max_num_ref_frames)
      ref_pic_list0_.pop_back();
  }

  return true;
}

bool H264VaapiVideoEncoderDelegate::UpdateRates(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  uint32_t bitrate = bitrate_allocation.GetSumBps();
  if (bitrate == 0 || framerate == 0)
    return false;

  if (curr_params_.bitrate_bps == bitrate &&
      curr_params_.framerate == framerate) {
    return true;
  }
  VLOGF(2) << "New bitrate: " << bitrate_allocation.GetSumBps()
           << ", New framerate: " << framerate;

  curr_params_.bitrate_bps = bitrate;
  curr_params_.framerate = framerate;
  curr_params_.cpb_size_bits =
      curr_params_.bitrate_bps * curr_params_.cpb_window_size_ms / 1000;

  bool previous_encoding_parameters_changed = encoding_parameters_changed_;

  UpdateSPS();

  // If SPS parameters are updated, it is required to send the SPS with IDR
  // frame. However, as a special case, we do not generate IDR frame if only
  // bitrate and framerate parameters are updated. This is safe because these
  // will not make a difference on decoder processing. The updated SPS will be
  // sent a next periodic or requested I(DR) frame. On the other hand, bitrate
  // and framerate parameter
  // changes must be affected for encoding. UpdateSPS()+SubmitFrameParameters()
  // shall apply them to an encoder properly.
  encoding_parameters_changed_ = previous_encoding_parameters_changed;
  return true;
}

void H264VaapiVideoEncoderDelegate::UpdateSPS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  memset(&current_sps_, 0, sizeof(H264SPS));

  // Spec A.2 and A.3.
  switch (profile_) {
    case H264PROFILE_BASELINE:
      // Due to https://crbug.com/345569, we don't distinguish between
      // constrained and non-constrained baseline profiles. Since many codecs
      // can't do non-constrained, and constrained is usually what we mean (and
      // it's a subset of non-constrained), default to it.
      current_sps_.profile_idc = H264SPS::kProfileIDCConstrainedBaseline;
      current_sps_.constraint_set0_flag = true;
      current_sps_.constraint_set1_flag = true;
      break;
    case H264PROFILE_MAIN:
      current_sps_.profile_idc = H264SPS::kProfileIDCMain;
      current_sps_.constraint_set1_flag = true;
      break;
    case H264PROFILE_HIGH:
      current_sps_.profile_idc = H264SPS::kProfileIDCHigh;
      break;
    default:
      NOTREACHED();
      return;
  }

  H264SPS::GetLevelConfigFromProfileLevel(profile_, level_,
                                          &current_sps_.level_idc,
                                          &current_sps_.constraint_set3_flag);

  current_sps_.seq_parameter_set_id = 0;
  current_sps_.chroma_format_idc = kChromaFormatIDC;

  DCHECK_GE(curr_params_.idr_period_frames, 16u)
      << "idr_period_frames must be >= 16";
  current_sps_.log2_max_frame_num_minus4 =
      base::bits::Log2Ceiling(curr_params_.idr_period_frames) - 4;
  current_sps_.pic_order_cnt_type = 0;
  current_sps_.log2_max_pic_order_cnt_lsb_minus4 =
      base::bits::Log2Ceiling(curr_params_.idr_period_frames * 2) - 4;
  current_sps_.max_num_ref_frames = curr_params_.max_num_ref_frames;

  current_sps_.frame_mbs_only_flag = true;

  DCHECK_GT(mb_width_, 0u);
  DCHECK_GT(mb_height_, 0u);
  current_sps_.pic_width_in_mbs_minus1 = mb_width_ - 1;
  DCHECK(current_sps_.frame_mbs_only_flag);
  current_sps_.pic_height_in_map_units_minus1 = mb_height_ - 1;

  if (visible_size_ != coded_size_) {
    // Visible size differs from coded size, fill crop information.
    current_sps_.frame_cropping_flag = true;
    DCHECK(!current_sps_.separate_colour_plane_flag);
    // Spec table 6-1. Only 4:2:0 for now.
    DCHECK_EQ(current_sps_.chroma_format_idc, 1);
    // Spec 7.4.2.1.1. Crop is in crop units, which is 2 pixels for 4:2:0.
    const unsigned int crop_unit_x = 2;
    const unsigned int crop_unit_y = 2 * (2 - current_sps_.frame_mbs_only_flag);
    current_sps_.frame_crop_left_offset = 0;
    current_sps_.frame_crop_right_offset =
        (coded_size_.width() - visible_size_.width()) / crop_unit_x;
    current_sps_.frame_crop_top_offset = 0;
    current_sps_.frame_crop_bottom_offset =
        (coded_size_.height() - visible_size_.height()) / crop_unit_y;
  }

  current_sps_.vui_parameters_present_flag = true;
  current_sps_.timing_info_present_flag = true;
  current_sps_.num_units_in_tick = 1;
  current_sps_.time_scale =
      curr_params_.framerate * 2;  // See equation D-2 in spec.
  current_sps_.fixed_frame_rate_flag = true;

  current_sps_.nal_hrd_parameters_present_flag = true;
  // H.264 spec ch. E.2.2.
  current_sps_.cpb_cnt_minus1 = 0;
  current_sps_.bit_rate_scale = kBitRateScale;
  current_sps_.cpb_size_scale = kCPBSizeScale;
  current_sps_.bit_rate_value_minus1[0] =
      (curr_params_.bitrate_bps >>
       (kBitRateScale + H264SPS::kBitRateScaleConstantTerm)) -
      1;
  current_sps_.cpb_size_value_minus1[0] =
      (curr_params_.cpb_size_bits >>
       (kCPBSizeScale + H264SPS::kCPBSizeScaleConstantTerm)) -
      1;
  current_sps_.cbr_flag[0] = true;
  current_sps_.initial_cpb_removal_delay_length_minus_1 =
      H264SPS::kDefaultInitialCPBRemovalDelayLength - 1;
  current_sps_.cpb_removal_delay_length_minus1 =
      H264SPS::kDefaultInitialCPBRemovalDelayLength - 1;
  current_sps_.dpb_output_delay_length_minus1 =
      H264SPS::kDefaultDPBOutputDelayLength - 1;
  current_sps_.time_offset_length = H264SPS::kDefaultTimeOffsetLength;
  current_sps_.low_delay_hrd_flag = false;

  GeneratePackedSPS();
  encoding_parameters_changed_ = true;
}

void H264VaapiVideoEncoderDelegate::UpdatePPS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  memset(&current_pps_, 0, sizeof(H264PPS));

  current_pps_.seq_parameter_set_id = current_sps_.seq_parameter_set_id;
  current_pps_.pic_parameter_set_id = 0;

  current_pps_.entropy_coding_mode_flag =
      current_sps_.profile_idc >= H264SPS::kProfileIDCMain;

  DCHECK_GT(curr_params_.max_ref_pic_list0_size, 0u);
  current_pps_.num_ref_idx_l0_default_active_minus1 =
      curr_params_.max_ref_pic_list0_size - 1;
  current_pps_.num_ref_idx_l1_default_active_minus1 =
      curr_params_.max_ref_pic_list1_size > 0
          ? curr_params_.max_ref_pic_list1_size - 1
          : curr_params_.max_ref_pic_list1_size;
  DCHECK_LE(curr_params_.initial_qp, 51u);
  current_pps_.pic_init_qp_minus26 =
      static_cast<int>(curr_params_.initial_qp) - 26;
  current_pps_.deblocking_filter_control_present_flag = true;
  current_pps_.transform_8x8_mode_flag =
      (current_sps_.profile_idc == H264SPS::kProfileIDCHigh);

  GeneratePackedPPS();
  encoding_parameters_changed_ = true;
}

void H264VaapiVideoEncoderDelegate::GeneratePackedSPS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  packed_sps_->Reset();

  packed_sps_->BeginNALU(H264NALU::kSPS, 3);

  packed_sps_->AppendBits(8, current_sps_.profile_idc);
  packed_sps_->AppendBool(current_sps_.constraint_set0_flag);
  packed_sps_->AppendBool(current_sps_.constraint_set1_flag);
  packed_sps_->AppendBool(current_sps_.constraint_set2_flag);
  packed_sps_->AppendBool(current_sps_.constraint_set3_flag);
  packed_sps_->AppendBool(current_sps_.constraint_set4_flag);
  packed_sps_->AppendBool(current_sps_.constraint_set5_flag);
  packed_sps_->AppendBits(2, 0);  // reserved_zero_2bits
  packed_sps_->AppendBits(8, current_sps_.level_idc);
  packed_sps_->AppendUE(current_sps_.seq_parameter_set_id);

  if (current_sps_.profile_idc == H264SPS::kProfileIDCHigh) {
    packed_sps_->AppendUE(current_sps_.chroma_format_idc);
    if (current_sps_.chroma_format_idc == 3)
      packed_sps_->AppendBool(current_sps_.separate_colour_plane_flag);
    packed_sps_->AppendUE(current_sps_.bit_depth_luma_minus8);
    packed_sps_->AppendUE(current_sps_.bit_depth_chroma_minus8);
    packed_sps_->AppendBool(current_sps_.qpprime_y_zero_transform_bypass_flag);
    packed_sps_->AppendBool(current_sps_.seq_scaling_matrix_present_flag);
    CHECK(!current_sps_.seq_scaling_matrix_present_flag);
  }

  packed_sps_->AppendUE(current_sps_.log2_max_frame_num_minus4);
  packed_sps_->AppendUE(current_sps_.pic_order_cnt_type);
  if (current_sps_.pic_order_cnt_type == 0)
    packed_sps_->AppendUE(current_sps_.log2_max_pic_order_cnt_lsb_minus4);
  else if (current_sps_.pic_order_cnt_type == 1)
    NOTREACHED();

  packed_sps_->AppendUE(current_sps_.max_num_ref_frames);
  packed_sps_->AppendBool(current_sps_.gaps_in_frame_num_value_allowed_flag);
  packed_sps_->AppendUE(current_sps_.pic_width_in_mbs_minus1);
  packed_sps_->AppendUE(current_sps_.pic_height_in_map_units_minus1);

  packed_sps_->AppendBool(current_sps_.frame_mbs_only_flag);
  if (!current_sps_.frame_mbs_only_flag)
    packed_sps_->AppendBool(current_sps_.mb_adaptive_frame_field_flag);

  packed_sps_->AppendBool(current_sps_.direct_8x8_inference_flag);

  packed_sps_->AppendBool(current_sps_.frame_cropping_flag);
  if (current_sps_.frame_cropping_flag) {
    packed_sps_->AppendUE(current_sps_.frame_crop_left_offset);
    packed_sps_->AppendUE(current_sps_.frame_crop_right_offset);
    packed_sps_->AppendUE(current_sps_.frame_crop_top_offset);
    packed_sps_->AppendUE(current_sps_.frame_crop_bottom_offset);
  }

  packed_sps_->AppendBool(current_sps_.vui_parameters_present_flag);
  if (current_sps_.vui_parameters_present_flag) {
    packed_sps_->AppendBool(false);  // aspect_ratio_info_present_flag
    packed_sps_->AppendBool(false);  // overscan_info_present_flag
    packed_sps_->AppendBool(false);  // video_signal_type_present_flag
    packed_sps_->AppendBool(false);  // chroma_loc_info_present_flag

    packed_sps_->AppendBool(current_sps_.timing_info_present_flag);
    if (current_sps_.timing_info_present_flag) {
      packed_sps_->AppendBits(32, current_sps_.num_units_in_tick);
      packed_sps_->AppendBits(32, current_sps_.time_scale);
      packed_sps_->AppendBool(current_sps_.fixed_frame_rate_flag);
    }

    packed_sps_->AppendBool(current_sps_.nal_hrd_parameters_present_flag);
    if (current_sps_.nal_hrd_parameters_present_flag) {
      packed_sps_->AppendUE(current_sps_.cpb_cnt_minus1);
      packed_sps_->AppendBits(4, current_sps_.bit_rate_scale);
      packed_sps_->AppendBits(4, current_sps_.cpb_size_scale);
      CHECK_LT(base::checked_cast<size_t>(current_sps_.cpb_cnt_minus1),
               base::size(current_sps_.bit_rate_value_minus1));
      for (int i = 0; i <= current_sps_.cpb_cnt_minus1; ++i) {
        packed_sps_->AppendUE(current_sps_.bit_rate_value_minus1[i]);
        packed_sps_->AppendUE(current_sps_.cpb_size_value_minus1[i]);
        packed_sps_->AppendBool(current_sps_.cbr_flag[i]);
      }
      packed_sps_->AppendBits(
          5, current_sps_.initial_cpb_removal_delay_length_minus_1);
      packed_sps_->AppendBits(5, current_sps_.cpb_removal_delay_length_minus1);
      packed_sps_->AppendBits(5, current_sps_.dpb_output_delay_length_minus1);
      packed_sps_->AppendBits(5, current_sps_.time_offset_length);
    }

    packed_sps_->AppendBool(false);  // vcl_hrd_parameters_flag
    if (current_sps_.nal_hrd_parameters_present_flag)
      packed_sps_->AppendBool(current_sps_.low_delay_hrd_flag);

    packed_sps_->AppendBool(false);  // pic_struct_present_flag
    packed_sps_->AppendBool(true);   // bitstream_restriction_flag

    packed_sps_->AppendBool(false);  // motion_vectors_over_pic_boundaries_flag
    packed_sps_->AppendUE(2);        // max_bytes_per_pic_denom
    packed_sps_->AppendUE(1);        // max_bits_per_mb_denom
    packed_sps_->AppendUE(16);       // log2_max_mv_length_horizontal
    packed_sps_->AppendUE(16);       // log2_max_mv_length_vertical

    // Explicitly set max_num_reorder_frames to 0 to allow the decoder to
    // output pictures early.
    packed_sps_->AppendUE(0);  // max_num_reorder_frames

    // The value of max_dec_frame_buffering shall be greater than or equal to
    // max_num_ref_frames.
    const unsigned int max_dec_frame_buffering =
        current_sps_.max_num_ref_frames;
    packed_sps_->AppendUE(max_dec_frame_buffering);
  }

  packed_sps_->FinishNALU();
}

void H264VaapiVideoEncoderDelegate::GeneratePackedPPS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  packed_pps_->Reset();

  packed_pps_->BeginNALU(H264NALU::kPPS, 3);

  packed_pps_->AppendUE(current_pps_.pic_parameter_set_id);
  packed_pps_->AppendUE(current_pps_.seq_parameter_set_id);
  packed_pps_->AppendBool(current_pps_.entropy_coding_mode_flag);
  packed_pps_->AppendBool(
      current_pps_.bottom_field_pic_order_in_frame_present_flag);
  CHECK_EQ(current_pps_.num_slice_groups_minus1, 0);
  packed_pps_->AppendUE(current_pps_.num_slice_groups_minus1);

  packed_pps_->AppendUE(current_pps_.num_ref_idx_l0_default_active_minus1);
  packed_pps_->AppendUE(current_pps_.num_ref_idx_l1_default_active_minus1);

  packed_pps_->AppendBool(current_pps_.weighted_pred_flag);
  packed_pps_->AppendBits(2, current_pps_.weighted_bipred_idc);

  packed_pps_->AppendSE(current_pps_.pic_init_qp_minus26);
  packed_pps_->AppendSE(current_pps_.pic_init_qs_minus26);
  packed_pps_->AppendSE(current_pps_.chroma_qp_index_offset);

  packed_pps_->AppendBool(current_pps_.deblocking_filter_control_present_flag);
  packed_pps_->AppendBool(current_pps_.constrained_intra_pred_flag);
  packed_pps_->AppendBool(current_pps_.redundant_pic_cnt_present_flag);

  packed_pps_->AppendBool(current_pps_.transform_8x8_mode_flag);
  packed_pps_->AppendBool(current_pps_.pic_scaling_matrix_present_flag);
  DCHECK(!current_pps_.pic_scaling_matrix_present_flag);
  packed_pps_->AppendSE(current_pps_.second_chroma_qp_index_offset);

  packed_pps_->FinishNALU();
}

void H264VaapiVideoEncoderDelegate::SubmitH264BitstreamBuffer(
    scoped_refptr<H264BitstreamBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!vaapi_wrapper_->SubmitBuffer(VAEncPackedHeaderDataBufferType,
                                    buffer->BytesInBuffer(), buffer->data())) {
    error_cb_.Run();
  }
}

bool H264VaapiVideoEncoderDelegate::SubmitFrameParameters(
    VaapiVideoEncoderDelegate::EncodeJob* job,
    const H264VaapiVideoEncoderDelegate::EncodeParams& encode_params,
    const H264SPS& sps,
    const H264PPS& pps,
    scoped_refptr<H264Picture> pic,
    const std::list<scoped_refptr<H264Picture>>& ref_pic_list0,
    const std::list<scoped_refptr<H264Picture>>& ref_pic_list1) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VAEncSequenceParameterBufferH264 seq_param = {};

#define SPS_TO_SP(a) seq_param.a = sps.a;
  SPS_TO_SP(seq_parameter_set_id);
  SPS_TO_SP(level_idc);

  seq_param.intra_period = encode_params.i_period_frames;
  seq_param.intra_idr_period = encode_params.idr_period_frames;
  seq_param.ip_period = encode_params.ip_period_frames;
  seq_param.bits_per_second = encode_params.bitrate_bps;

  SPS_TO_SP(max_num_ref_frames);
  absl::optional<gfx::Size> coded_size = sps.GetCodedSize();
  if (!coded_size) {
    DVLOGF(1) << "Invalid coded size";
    return false;
  }
  constexpr int kH264MacroblockSizeInPixels = 16;
  seq_param.picture_width_in_mbs =
      coded_size->width() / kH264MacroblockSizeInPixels;
  seq_param.picture_height_in_mbs =
      coded_size->height() / kH264MacroblockSizeInPixels;

#define SPS_TO_SP_FS(a) seq_param.seq_fields.bits.a = sps.a;
  SPS_TO_SP_FS(chroma_format_idc);
  SPS_TO_SP_FS(frame_mbs_only_flag);
  SPS_TO_SP_FS(log2_max_frame_num_minus4);
  SPS_TO_SP_FS(pic_order_cnt_type);
  SPS_TO_SP_FS(log2_max_pic_order_cnt_lsb_minus4);
#undef SPS_TO_SP_FS

  SPS_TO_SP(bit_depth_luma_minus8);
  SPS_TO_SP(bit_depth_chroma_minus8);

  SPS_TO_SP(frame_cropping_flag);
  if (sps.frame_cropping_flag) {
    SPS_TO_SP(frame_crop_left_offset);
    SPS_TO_SP(frame_crop_right_offset);
    SPS_TO_SP(frame_crop_top_offset);
    SPS_TO_SP(frame_crop_bottom_offset);
  }

  SPS_TO_SP(vui_parameters_present_flag);
#define SPS_TO_SP_VF(a) seq_param.vui_fields.bits.a = sps.a;
  SPS_TO_SP_VF(timing_info_present_flag);
#undef SPS_TO_SP_VF
  SPS_TO_SP(num_units_in_tick);
  SPS_TO_SP(time_scale);
#undef SPS_TO_SP

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncoderDelegate::SubmitBuffer,
                     base::Unretained(this), VAEncSequenceParameterBufferType,
                     MakeRefCountedBytes(&seq_param, sizeof(seq_param))));

  VAEncPictureParameterBufferH264 pic_param = {};

  auto va_surface_id = pic->AsVaapiH264Picture()->GetVASurfaceID();
  pic_param.CurrPic.picture_id = va_surface_id;
  pic_param.CurrPic.TopFieldOrderCnt = pic->top_field_order_cnt;
  pic_param.CurrPic.BottomFieldOrderCnt = pic->bottom_field_order_cnt;
  pic_param.CurrPic.flags = 0;

  pic_param.coded_buf = job->coded_buffer_id();
  pic_param.pic_parameter_set_id = pps.pic_parameter_set_id;
  pic_param.seq_parameter_set_id = pps.seq_parameter_set_id;
  pic_param.frame_num = pic->frame_num;
  pic_param.pic_init_qp = pps.pic_init_qp_minus26 + 26;
  pic_param.num_ref_idx_l0_active_minus1 =
      pps.num_ref_idx_l0_default_active_minus1;

  pic_param.pic_fields.bits.idr_pic_flag = pic->idr;
  pic_param.pic_fields.bits.reference_pic_flag = pic->ref;
#define PPS_TO_PP_PF(a) pic_param.pic_fields.bits.a = pps.a;
  PPS_TO_PP_PF(entropy_coding_mode_flag);
  PPS_TO_PP_PF(transform_8x8_mode_flag);
  PPS_TO_PP_PF(deblocking_filter_control_present_flag);
#undef PPS_TO_PP_PF

  VAEncSliceParameterBufferH264 slice_param = {};

  slice_param.num_macroblocks =
      seq_param.picture_width_in_mbs * seq_param.picture_height_in_mbs;
  slice_param.macroblock_info = VA_INVALID_ID;
  slice_param.slice_type = pic->type;
  slice_param.pic_parameter_set_id = pps.pic_parameter_set_id;
  slice_param.idr_pic_id = pic->idr_pic_id;
  slice_param.pic_order_cnt_lsb = pic->pic_order_cnt_lsb;
  slice_param.num_ref_idx_active_override_flag = true;

  for (VAPictureH264& picture : pic_param.ReferenceFrames)
    InitVAPictureH264(&picture);

  for (VAPictureH264& picture : slice_param.RefPicList0)
    InitVAPictureH264(&picture);

  for (VAPictureH264& picture : slice_param.RefPicList1)
    InitVAPictureH264(&picture);

  VAPictureH264* ref_frames_entry = pic_param.ReferenceFrames;
  VAPictureH264* ref_list_entry = slice_param.RefPicList0;
  // Initialize the current entry on slice and picture reference lists to
  // |ref_pic| and advance list pointers.
  auto fill_ref_frame = [&ref_frames_entry,
                         &ref_list_entry](scoped_refptr<H264Picture> ref_pic) {
    VAPictureH264 va_pic_h264;
    InitVAPictureH264(&va_pic_h264);
    va_pic_h264.picture_id = ref_pic->AsVaapiH264Picture()->GetVASurfaceID();
    va_pic_h264.flags = 0;

    *ref_frames_entry = va_pic_h264;
    *ref_list_entry = va_pic_h264;
    ++ref_frames_entry;
    ++ref_list_entry;
  };

  // Fill slice_param.RefPicList{0,1} with pictures from ref_pic_list{0,1},
  // respectively, and pic_param.ReferenceFrames with entries from both.
  std::for_each(ref_pic_list0.begin(), ref_pic_list0.end(), fill_ref_frame);
  ref_list_entry = slice_param.RefPicList1;
  std::for_each(ref_pic_list1.begin(), ref_pic_list1.end(), fill_ref_frame);

  VAEncMiscParameterRateControl rate_control_param;
  VAEncMiscParameterFrameRate framerate_param;
  VAEncMiscParameterHRD hrd_param;
  FillVAEncRateControlParams(
      encode_params.bitrate_bps,
      base::strict_cast<uint32_t>(encode_params.cpb_window_size_ms),
      base::strict_cast<uint32_t>(pic_param.pic_init_qp),
      base::strict_cast<uint32_t>(encode_params.min_qp),
      base::strict_cast<uint32_t>(encode_params.max_qp),
      encode_params.framerate,
      base::strict_cast<uint32_t>(encode_params.cpb_size_bits),
      rate_control_param, framerate_param, hrd_param);

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncoderDelegate::SubmitBuffer,
                     base::Unretained(this), VAEncPictureParameterBufferType,
                     MakeRefCountedBytes(&pic_param, sizeof(pic_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncoderDelegate::SubmitBuffer,
                     base::Unretained(this), VAEncSliceParameterBufferType,
                     MakeRefCountedBytes(&slice_param, sizeof(slice_param))));

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncoderDelegate::SubmitVAEncMiscParamBuffer,
      base::Unretained(this), VAEncMiscParameterTypeRateControl,
      MakeRefCountedBytes(&rate_control_param, sizeof(rate_control_param))));

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncoderDelegate::SubmitVAEncMiscParamBuffer,
      base::Unretained(this), VAEncMiscParameterTypeFrameRate,
      MakeRefCountedBytes(&framerate_param, sizeof(framerate_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncoderDelegate::SubmitVAEncMiscParamBuffer,
                     base::Unretained(this), VAEncMiscParameterTypeHRD,
                     MakeRefCountedBytes(&hrd_param, sizeof(hrd_param))));

  return true;
}

scoped_refptr<H264Picture> H264VaapiVideoEncoderDelegate::GetPicture(
    EncodeJob* job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return base::WrapRefCounted(
      reinterpret_cast<H264Picture*>(job->picture().get()));
}

bool H264VaapiVideoEncoderDelegate::SubmitPackedHeaders(
    EncodeJob* job,
    scoped_refptr<H264BitstreamBuffer> packed_sps,
    scoped_refptr<H264BitstreamBuffer> packed_pps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Submit SPS.
  VAEncPackedHeaderParameterBuffer par_buffer = {};
  par_buffer.type = VAEncPackedHeaderSequence;
  par_buffer.bit_length = packed_sps->BytesInBuffer() * 8;

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncoderDelegate::SubmitBuffer, base::Unretained(this),
      VAEncPackedHeaderParameterBufferType,
      MakeRefCountedBytes(&par_buffer, sizeof(par_buffer))));

  job->AddSetupCallback(
      base::BindOnce(&H264VaapiVideoEncoderDelegate::SubmitH264BitstreamBuffer,
                     base::Unretained(this), packed_sps));

  // Submit PPS.
  par_buffer = {};
  par_buffer.type = VAEncPackedHeaderPicture;
  par_buffer.bit_length = packed_pps->BytesInBuffer() * 8;

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncoderDelegate::SubmitBuffer, base::Unretained(this),
      VAEncPackedHeaderParameterBufferType,
      MakeRefCountedBytes(&par_buffer, sizeof(par_buffer))));

  job->AddSetupCallback(
      base::BindOnce(&H264VaapiVideoEncoderDelegate::SubmitH264BitstreamBuffer,
                     base::Unretained(this), packed_pps));

  return true;
}

}  // namespace media
