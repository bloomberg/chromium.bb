/*
 * Copyright 2019 The libgav1 Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBGAV1_SRC_OBU_PARSER_H_
#define LIBGAV1_SRC_OBU_PARSER_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "src/dsp/common.h"
#include "src/gav1/decoder_buffer.h"
#include "src/gav1/status_code.h"
#include "src/quantizer.h"
#include "src/utils/common.h"
#include "src/utils/compiler_attributes.h"
#include "src/utils/constants.h"
#include "src/utils/raw_bit_reader.h"
#include "src/utils/segmentation.h"
#include "src/utils/vector.h"

namespace libgav1 {

struct DecoderState;

// structs and enums related to Open Bitstream Units (OBU).

enum {
  kMinimumMajorBitstreamLevel = 2,
  kMaxOperatingPoints = 32,
  kSelectScreenContentTools = 2,
  kSelectIntegerMv = 2,
  kLoopFilterMaxModeDeltas = 2,
  kMaxCdefStrengths = 8,
  kLoopRestorationTileSizeMax = 256,
  kGlobalMotionAlphaBits = 12,
  kGlobalMotionTranslationBits = 12,
  kGlobalMotionTranslationOnlyBits = 9,
  kGlobalMotionAlphaPrecisionBits = 15,
  kGlobalMotionTranslationPrecisionBits = 6,
  kGlobalMotionTranslationOnlyPrecisionBits = 3,
  kMaxTileWidth = 4096,
  kMaxTileArea = 4096 * 2304,
  kMaxTileColumns = 64,
  kMaxTileRows = 64,
  kPrimaryReferenceNone = 7
};  // anonymous enum

struct ObuHeader {
  ObuType type;
  bool has_extension;
  bool has_size_field;
  int8_t temporal_id;
  int8_t spatial_id;
};

enum BitstreamProfile : uint8_t {
  kProfile0,
  kProfile1,
  kProfile2,
  kMaxProfiles
};

// In the bitstream the level is encoded in five bits: the first three bits
// encode |major| - 2 and the last two bits encode |minor|.
//
// If the mapped level (major.minor) is in the tables in Annex A.3, there are
// bitstream conformance requirements on the maximum or minimum values of
// several variables. The encoded value of 31 (which corresponds to the mapped
// level 9.3) is the "maximum parameters" level and imposes no level-based
// constraints on the bitstream.
struct BitStreamLevel {
  uint8_t major;  // Range: 2-9.
  uint8_t minor;  // Range: 0-3.
};

struct ColorConfig {
  int8_t bitdepth;
  bool is_monochrome;
  ColorPrimary color_primary;
  TransferCharacteristics transfer_characteristics;
  MatrixCoefficients matrix_coefficients;
  // A binary value (0 or 1) that is associated with the VideoFullRangeFlag
  // variable specified in ISO/IEC 23091-4/ITUT H.273.
  // * 0: the studio swing representation.
  // * 1: the full swing representation.
  ColorRange color_range;
  int8_t subsampling_x;
  int8_t subsampling_y;
  ChromaSamplePosition chroma_sample_position;
  bool separate_uv_delta_q;
};

struct TimingInfo {
  uint32_t num_units_in_tick;
  uint32_t time_scale;
  bool equal_picture_interval;
  uint32_t num_ticks_per_picture;
};

struct DecoderModelInfo {
  uint8_t encoder_decoder_buffer_delay_length;
  uint32_t num_units_in_decoding_tick;
  uint8_t buffer_removal_time_length;
  uint8_t frame_presentation_time_length;
};

struct OperatingParameters {
  uint8_t initial_display_delay[kMaxOperatingPoints];
  uint32_t decoder_buffer_delay[kMaxOperatingPoints];
  uint32_t encoder_buffer_delay[kMaxOperatingPoints];
  bool low_delay_mode_flag[kMaxOperatingPoints];
};

struct ObuSequenceHeader {
  BitstreamProfile profile;
  bool still_picture;
  bool reduced_still_picture_header;
  int operating_points;
  int operating_point_idc[kMaxOperatingPoints];
  BitStreamLevel level[kMaxOperatingPoints];
  int8_t tier[kMaxOperatingPoints];
  int8_t frame_width_bits;
  int8_t frame_height_bits;
  int32_t max_frame_width;
  int32_t max_frame_height;
  bool frame_id_numbers_present;
  int8_t frame_id_length_bits;
  int8_t delta_frame_id_length_bits;
  bool use_128x128_superblock;
  bool enable_filter_intra;
  bool enable_intra_edge_filter;
  bool enable_interintra_compound;
  bool enable_masked_compound;
  bool enable_warped_motion;
  bool enable_dual_filter;
  bool enable_order_hint;
  // If enable_order_hint is true, order_hint_bits is in the range [1, 8].
  // If enable_order_hint is false, order_hint_bits is 0.
  int8_t order_hint_bits;
  bool enable_jnt_comp;
  bool enable_ref_frame_mvs;
  bool choose_screen_content_tools;
  int8_t force_screen_content_tools;
  bool choose_integer_mv;
  int8_t force_integer_mv;
  bool enable_superres;
  bool enable_cdef;
  bool enable_restoration;
  ColorConfig color_config;
  bool timing_info_present_flag;
  TimingInfo timing_info;
  bool decoder_model_info_present_flag;
  DecoderModelInfo decoder_model_info;
  bool decoder_model_present_for_operating_point[kMaxOperatingPoints];
  OperatingParameters operating_parameters;
  bool initial_display_delay_present_flag;
  bool film_grain_params_present;
};

// Loop filter parameters:
//
// If level[0] and level[1] are both equal to 0, the loop filter process is
// not invoked.
//
// |sharpness| and |delta_enabled| are only used by the loop filter process.
//
// The |ref_deltas| and |mode_deltas| arrays are used not only by the loop
// filter process but also by the reference frame update and loading
// processes. The loop filter process uses |ref_deltas| and |mode_deltas| only
// when |delta_enabled| is true.
struct LoopFilter {
  // Contains loop filter strength values in the range of [0, 63].
  std::array<int8_t, kFrameLfCount> level;
  // Indicates the sharpness level in the range of [0, 7].
  int8_t sharpness;
  // Whether the filter level depends on the mode and reference frame used to
  // predict a block.
  bool delta_enabled;
  // Contains the adjustment needed for the filter level based on the chosen
  // reference frame, in the range of [-64, 63].
  std::array<int8_t, kNumReferenceFrameTypes> ref_deltas;
  // Contains the adjustment needed for the filter level based on the chosen
  // mode, in the range of [-64, 63].
  std::array<int8_t, kLoopFilterMaxModeDeltas> mode_deltas;
};

struct Delta {
  bool present;
  uint8_t scale;
  bool multi;
};

struct Cdef {
  uint8_t damping;
  uint8_t bits;
  uint8_t y_primary_strength[kMaxCdefStrengths];
  uint8_t y_secondary_strength[kMaxCdefStrengths];
  uint8_t uv_primary_strength[kMaxCdefStrengths];
  uint8_t uv_secondary_strength[kMaxCdefStrengths];
};

enum GlobalMotionTransformationType : uint8_t {
  kGlobalMotionTransformationTypeIdentity,
  kGlobalMotionTransformationTypeTranslation,
  kGlobalMotionTransformationTypeRotZoom,
  kGlobalMotionTransformationTypeAffine,
  kNumGlobalMotionTransformationTypes
};

// Global motion and warped motion parameters. See the paper for more info:
// S. Parker, Y. Chen, D. Barker, P. de Rivaz, D. Mukherjee, "Global and locally
// adaptive warped motion compensation in video compression", Proc. IEEE
// International Conference on Image Processing (ICIP), pp. 275-279, Sep. 2017.
struct GlobalMotion {
  GlobalMotionTransformationType type;
  int32_t params[6];

  // Represent two shearing operations. Computed from |params| by SetupShear().
  //
  // The least significant six (= kWarpParamRoundingBits) bits are all zeros.
  // (This means alpha, beta, gamma, and delta could be represented by a 10-bit
  // signed integer.) The minimum value is INT16_MIN (= -32768) and the maximum
  // value is 32704 = 0x7fc0, the largest int16_t value whose least significant
  // six bits are all zeros.
  //
  // Valid warp parameters (as validated by SetupShear()) have smaller ranges.
  // Their absolute values are less than 2^14 (= 16384). (This follows from
  // the warpValid check at the end of Section 7.11.3.6.)
  //
  // NOTE: Section 7.11.3.6 of the spec allows a maximum value of 32768, which
  // is outside the range of int16_t. When cast to int16_t, 32768 becomes
  // -32768. This potential int16_t overflow does not matter because either
  // 32768 or -32768 causes SetupShear() to return false,
  int16_t alpha;
  int16_t beta;
  int16_t gamma;
  int16_t delta;
};

struct TileInfo {
  bool uniform_spacing;
  int sb_rows;
  int sb_columns;
  int tile_count;
  int tile_columns_log2;
  int tile_columns;
  int tile_column_start[kMaxTileColumns + 1];
  int tile_rows_log2;
  int tile_rows;
  int tile_row_start[kMaxTileRows + 1];
  int16_t context_update_id;
  uint8_t tile_size_bytes;
};

struct ObuFrameHeader {
  uint16_t display_frame_id;
  uint16_t current_frame_id;
  int64_t frame_offset;
  uint16_t expected_frame_id[kNumInterReferenceFrameTypes];
  int32_t width;
  int32_t height;
  int32_t columns4x4;
  int32_t rows4x4;
  // The render size (render_width and render_height) is a hint to the
  // application about the desired display size. It has no effect on the
  // decoding process.
  int32_t render_width;
  int32_t render_height;
  int32_t upscaled_width;
  LoopRestoration loop_restoration;
  uint32_t buffer_removal_time[kMaxOperatingPoints];
  uint32_t frame_presentation_time;
  // Note: global_motion[0] (for kReferenceFrameIntra) is not used.
  std::array<GlobalMotion, kNumReferenceFrameTypes> global_motion;
  TileInfo tile_info;
  QuantizerParameters quantizer;
  Segmentation segmentation;
  bool show_existing_frame;
  // frame_to_show is in the range [0, 7]. Only used if show_existing_frame is
  // true.
  int8_t frame_to_show;
  FrameType frame_type;
  bool show_frame;
  bool showable_frame;
  bool error_resilient_mode;
  bool enable_cdf_update;
  bool frame_size_override_flag;
  // The order_hint syntax element in the uncompressed header. If
  // show_existing_frame is false, the OrderHint variable in the spec is equal
  // to this field, and so this field can be used in place of OrderHint when
  // show_existing_frame is known to be false, such as during tile decoding.
  uint8_t order_hint;
  int8_t primary_reference_frame;
  bool render_and_frame_size_different;
  uint8_t superres_scale_denominator;
  bool allow_screen_content_tools;
  bool allow_intrabc;
  bool frame_refs_short_signaling;
  // A bitmask that specifies which reference frame slots will be updated with
  // the current frame after it is decoded.
  uint8_t refresh_frame_flags;
  static_assert(sizeof(ObuFrameHeader::refresh_frame_flags) * 8 ==
                    kNumReferenceFrameTypes,
                "");
  bool found_reference;
  int8_t force_integer_mv;
  bool allow_high_precision_mv;
  InterpolationFilter interpolation_filter;
  bool is_motion_mode_switchable;
  bool use_ref_frame_mvs;
  bool enable_frame_end_update_cdf;
  // True if all segments are losslessly encoded at the coded resolution.
  bool coded_lossless;
  // True if all segments are losslessly encoded at the upscaled resolution.
  bool upscaled_lossless;
  TxMode tx_mode;
  // True means that the mode info for inter blocks contains the syntax
  // element comp_mode that indicates whether to use single or compound
  // prediction. False means that all inter blocks will use single prediction.
  bool reference_mode_select;
  // The frames to use for compound prediction when skip_mode is true.
  ReferenceFrameType skip_mode_frame[2];
  bool skip_mode_present;
  bool reduced_tx_set;
  bool allow_warped_motion;
  Delta delta_q;
  Delta delta_lf;
  // A valid value of reference_frame_index[i] is in the range [0, 7]. -1
  // indicates an invalid value.
  int8_t reference_frame_index[kNumInterReferenceFrameTypes];
  // The ref_order_hint[ i ] syntax element in the uncompressed header.
  // Specifies the expected output order hint for each reference frame.
  uint8_t reference_order_hint[kNumReferenceFrameTypes];
  LoopFilter loop_filter;
  Cdef cdef;
  FilmGrainParams film_grain_params;
};

enum MetadataType : uint8_t {
  // 0 is reserved for AOM use.
  kMetadataTypeHdrContentLightLevel = 1,
  kMetadataTypeHdrMasteringDisplayColorVolume
};

struct ObuMetadata {
  // Maximum content light level.
  uint16_t max_cll;
  // Maximum frame-average light level.
  uint16_t max_fall;
  uint16_t primary_chromaticity_x[3];
  uint16_t primary_chromaticity_y[3];
  uint16_t white_point_chromaticity_x;
  uint16_t white_point_chromaticity_y;
  uint32_t luminance_max;
  uint32_t luminance_min;
};

struct ObuTileGroup {
  int start;
  int end;
  // Pointer to the start of Tile Group data.
  const uint8_t* data;
  // Size of the Tile Group data (excluding the Tile Group headers).
  size_t data_size;
  // Offset of the start of Tile Group data relative to |ObuParser->data_|.
  size_t data_offset;
};

class ObuParser : public Allocable {
 public:
  ObuParser(const uint8_t* const data, size_t size,
            DecoderState* const decoder_state)
      : data_(data), size_(size), decoder_state_(*decoder_state) {}

  // Not copyable or movable.
  ObuParser(const ObuParser& rhs) = delete;
  ObuParser& operator=(const ObuParser& rhs) = delete;

  // Returns true if there is more data that needs to be parsed.
  bool HasData() const;

  // Parses a sequence of Open Bitstream Units until a decodable frame is found
  // (or until the end of stream is reached). A decodable frame is considered to
  // be found when one of the following happens:
  //   * A kObuFrame is seen.
  //   * The kObuTileGroup containing the last tile is seen.
  //   * A kFrameHeader with show_existing_frame = true is seen.
  //
  // If the parsing is successful, relevant fields will be populated. The fields
  // are valid only if the return value is kLibgav1StatusOk. Returns
  // kLibgav1StatusOk on success, an error status otherwise.
  StatusCode ParseOneFrame();

  // Getters. Only valid if ParseOneFrame() completes successfully.
  const Vector<ObuHeader>& obu_headers() const { return obu_headers_; }
  const ObuSequenceHeader& sequence_header() const { return sequence_header_; }
  const ObuFrameHeader& frame_header() const { return frame_header_; }
  const ObuMetadata& metadata() const { return metadata_; }
  const Vector<ObuTileGroup>& tile_groups() const { return tile_groups_; }

  // Setters.
  void set_sequence_header(const ObuSequenceHeader& sequence_header) {
    sequence_header_ = sequence_header;
    has_sequence_header_ = true;
  }

 private:
  // Initializes the bit reader. This is a function of its own to make unit
  // testing of private functions simpler.
  LIBGAV1_MUST_USE_RESULT bool InitBitReader(const uint8_t* data, size_t size);

  // Parse helper functions.
  bool ParseHeader();  // 5.3.2 and 5.3.3.
  bool ParseColorConfig(ObuSequenceHeader* sequence_header);       // 5.5.2.
  bool ParseTimingInfo(ObuSequenceHeader* sequence_header);        // 5.5.3.
  bool ParseDecoderModelInfo(ObuSequenceHeader* sequence_header);  // 5.5.4.
  bool ParseOperatingParameters(ObuSequenceHeader* sequence_header,
                                int index);  // 5.5.5.
  bool ParseSequenceHeader();                // 5.5.1.
  bool ParseFrameParameters();               // 5.9.2, 5.9.7 and 5.9.10.
  void MarkInvalidReferenceFrames();         // 5.9.4.
  bool ParseFrameSizeAndRenderSize();        // 5.9.5 and 5.9.6.
  bool ParseSuperResParametersAndComputeImageSize();  // 5.9.8 and 5.9.9.
  // Checks the bitstream conformance requirement in Section 6.8.6.
  bool ValidateInterFrameSize() const;
  bool ParseReferenceOrderHint();
  static int FindLatestBackwardReference(
      const int current_frame_hint,
      const std::array<int, kNumReferenceFrameTypes>& shifted_order_hints,
      const std::array<bool, kNumReferenceFrameTypes>& used_frame);
  static int FindEarliestBackwardReference(
      const int current_frame_hint,
      const std::array<int, kNumReferenceFrameTypes>& shifted_order_hints,
      const std::array<bool, kNumReferenceFrameTypes>& used_frame);
  static int FindLatestForwardReference(
      const int current_frame_hint,
      const std::array<int, kNumReferenceFrameTypes>& shifted_order_hints,
      const std::array<bool, kNumReferenceFrameTypes>& used_frame);
  static int FindReferenceWithSmallestOutputOrder(
      const std::array<int, kNumReferenceFrameTypes>& shifted_order_hints);
  bool SetFrameReferences(int8_t last_frame_idx,
                          int8_t gold_frame_idx);  // 7.8.
  bool ParseLoopFilterParameters();                // 5.9.11.
  bool ParseDeltaQuantizer(int8_t* delta);         // 5.9.13.
  bool ParseQuantizerParameters();                 // 5.9.12.
  bool ParseSegmentationParameters();              // 5.9.14.
  bool ParseQuantizerIndexDeltaParameters();       // 5.9.17.
  bool ParseLoopFilterDeltaParameters();           // 5.9.18.
  void ComputeSegmentLosslessAndQIndex();
  bool ParseCdefParameters();             // 5.9.19.
  bool ParseLoopRestorationParameters();  // 5.9.20.
  bool ParseTxModeSyntax();               // 5.9.21.
  bool ParseFrameReferenceModeSyntax();   // 5.9.23.
  // Returns whether skip mode is allowed. When it returns true, it also sets
  // the frame_header_.skip_mode_frame array.
  bool IsSkipModeAllowed();
  bool ParseSkipModeParameters();  // 5.9.22.
  bool ReadAllowWarpedMotion();
  bool ParseGlobalParamSyntax(
      int ref, int index,
      const std::array<GlobalMotion, kNumReferenceFrameTypes>&
          prev_global_motions);        // 5.9.25.
  bool ParseGlobalMotionParameters();  // 5.9.24.
  bool ParseFilmGrainParameters();     // 5.9.30.
  bool ParseTileInfoSyntax();          // 5.9.15.
  bool ParseFrameHeader();             // 5.9.
  bool ParseMetadata(size_t size);     // 5.8.
  // Validates the |start| and |end| fields of the current tile group. If
  // valid, updates next_tile_group_start_ and returns true. Otherwise,
  // returns false.
  bool ValidateTileGroup();
  void SetTileDataOffset(size_t total_size, size_t tg_header_size,
                         size_t bytes_consumed_so_far);
  bool ParseTileGroup(size_t size, size_t bytes_consumed_so_far);  // 5.11.1.

  // Parser elements.
  std::unique_ptr<RawBitReader> bit_reader_;
  const uint8_t* data_;
  size_t size_;

  // OBU elements. Only valid if ParseOneFrame() completes successfully.
  Vector<ObuHeader> obu_headers_;
  ObuSequenceHeader sequence_header_ = {};
  ObuFrameHeader frame_header_ = {};
  ObuMetadata metadata_ = {};
  Vector<ObuTileGroup> tile_groups_;
  // The expected |start| value of the next ObuTileGroup.
  int next_tile_group_start_ = 0;
  // If true, the sequence_header_ field is valid.
  bool has_sequence_header_ = false;
  // If true, the obu_extension_flag syntax element in the OBU header must be
  // 0. Set to true when parsing a sequence header if OperatingPointIdc is 0.
  bool extension_disallowed_ = false;

  DecoderState& decoder_state_;

  // For unit testing private functions.
  friend class ObuParserTest;
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_OBU_PARSER_H_
