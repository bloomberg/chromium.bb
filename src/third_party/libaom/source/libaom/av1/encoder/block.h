/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/*! \file
 * Declares various structs used to encode the current partition block.
 */
#ifndef AOM_AV1_ENCODER_BLOCK_H_
#define AOM_AV1_ENCODER_BLOCK_H_

#include "av1/common/blockd.h"
#include "av1/common/entropymv.h"
#include "av1/common/entropy.h"
#include "av1/common/enums.h"
#include "av1/common/mvref_common.h"

#include "av1/encoder/enc_enums.h"
#if !CONFIG_REALTIME_ONLY
#include "av1/encoder/partition_cnn_weights.h"
#endif

#include "av1/encoder/hash.h"

#ifdef __cplusplus
extern "C" {
#endif

//! Minimum linear dimension of a tpl block
#define MIN_TPL_BSIZE_1D 16
//! Maximum number of tpl block in a super block
#define MAX_TPL_BLK_IN_SB (MAX_SB_SIZE / MIN_TPL_BSIZE_1D)
//! Number of intra winner modes kept
#define MAX_WINNER_MODE_COUNT_INTRA 3
//! Number of inter winner modes kept
#define MAX_WINNER_MODE_COUNT_INTER 1
//! Number of txfm hash records kept for the partition block.
#define RD_RECORD_BUFFER_LEN 8
//! Number of txfm hash records kept for the txfm block.
#define TX_SIZE_RD_RECORD_BUFFER_LEN 256

/*! \brief Superblock level encoder info
 *
 * SuperblockEnc stores superblock level information used by the encoder for
 * more efficient encoding. Currently this is mostly used to store TPL data
 * for the current superblock.
 */
typedef struct {
  //! Maximum partition size for the sb.
  BLOCK_SIZE min_partition_size;
  //! Minimum partition size for the sb.
  BLOCK_SIZE max_partition_size;

  /*****************************************************************************
   * \name TPL Info
   *
   * Information gathered from tpl_model at tpl block precision for the
   * superblock to speed up the encoding process..
   ****************************************************************************/
  /**@{*/
  //! Number of TPL blocks in this superblock.
  int tpl_data_count;
  //! TPL's estimate of inter cost for each tpl block.
  int64_t tpl_inter_cost[MAX_TPL_BLK_IN_SB * MAX_TPL_BLK_IN_SB];
  //! TPL's estimate of tpl cost for each tpl block.
  int64_t tpl_intra_cost[MAX_TPL_BLK_IN_SB * MAX_TPL_BLK_IN_SB];
  //! Motion vectors found by TPL model for each tpl block.
  int_mv tpl_mv[MAX_TPL_BLK_IN_SB * MAX_TPL_BLK_IN_SB][INTER_REFS_PER_FRAME];
  //! TPL's stride for the arrays in this struct.
  int tpl_stride;
  /**@}*/
} SuperBlockEnc;

/*! \brief Stores the best performing modes.
 */
typedef struct {
  //! The mbmi used to reconstruct the winner mode.
  MB_MODE_INFO mbmi;
  //! Rdstats of the winner mode.
  RD_STATS rd_cost;
  //! Rdcost of the winner mode
  int64_t rd;
  //! Luma rate of the winner mode.
  int rate_y;
  //! Chroma rate of the winner mode.
  int rate_uv;
  //! The color map needed to reconstruct palette mode.
  uint8_t color_index_map[MAX_SB_SQUARE];
  //! The current winner mode.
  THR_MODES mode_index;
} WinnerModeStats;

/*! \brief Each source plane of the current macroblock
 *
 * This struct also stores the txfm buffers and quantizer settings.
 */
typedef struct macroblock_plane {
  //! Stores source - pred so the txfm can be computed later
  DECLARE_ALIGNED(32, int16_t, src_diff[MAX_SB_SQUARE]);
  //! Dequantized coefficients
  tran_low_t *dqcoeff;
  //! Quantized coefficients
  tran_low_t *qcoeff;
  //! Transformed coefficients
  tran_low_t *coeff;
  //! Location of the end of qcoeff (end of block).
  uint16_t *eobs;
  //! Contexts used to code the transform coefficients.
  uint8_t *txb_entropy_ctx;
  //! A buffer containing the source frame.
  struct buf_2d src;

  /*! \name Quantizer Settings
   *
   * \attention These are used/accessed only in the quantization process.
   * RDO does not and *must not* depend on any of these values.
   * All values below share the coefficient scale/shift used in TX.
   */
  /**@{*/
  //! Quantization step size used by AV1_XFORM_QUANT_FP.
  const int16_t *quant_fp_QTX;
  //! Offset used for rounding in the quantizer process by AV1_XFORM_QUANT_FP.
  const int16_t *round_fp_QTX;
  //! Quantization step size used by AV1_XFORM_QUANT_B.
  const int16_t *quant_QTX;
  //! Offset used for rounding in the quantizer process by AV1_XFORM_QUANT_B.
  const int16_t *round_QTX;
  //! Scale factor to shift coefficients toward zero. Only used by QUANT_B.
  const int16_t *quant_shift_QTX;
  //! Size of the quantization bin around 0. Only Used by QUANT_B
  const int16_t *zbin_QTX;
  //! Dequantizer
  const int16_t *dequant_QTX;
  /**@}*/
} MACROBLOCK_PLANE;

/*! \brief Costs for encoding the coefficients within a level.
 *
 * Covers everything including txb_skip, eob, dc_sign,
 */
typedef struct {
  //! Cost to skip txfm for the current txfm block.
  int txb_skip_cost[TXB_SKIP_CONTEXTS][2];
  /*! \brief Cost for encoding the base_eob of a level.
   *
   * Decoder uses base_eob to derive the base_level as base_eob := base_eob+1.
   */
  int base_eob_cost[SIG_COEF_CONTEXTS_EOB][3];
  /*! \brief Cost for encoding the base level of a coefficient.
   *
   * Decoder derives coeff_base as coeff_base := base_eob + 1.
   */
  int base_cost[SIG_COEF_CONTEXTS][8];
  /*! \brief Cost for encoding the last non-zero coefficient.
   *
   * Eob is derived from eob_extra at the decoder as eob := eob_extra + 1
   */
  int eob_extra_cost[EOB_COEF_CONTEXTS][2];
  //! Cost for encoding the dc_sign
  int dc_sign_cost[DC_SIGN_CONTEXTS][2];
  //! Cost for encoding an increment to the coefficient
  int lps_cost[LEVEL_CONTEXTS][COEFF_BASE_RANGE + 1 + COEFF_BASE_RANGE + 1];
} LV_MAP_COEFF_COST;

/*! \brief Costs for encoding the eob.
 */
typedef struct {
  //! eob_cost.
  int eob_cost[2][11];
} LV_MAP_EOB_COST;

/*! \brief Stores the transforms coefficients for the whole superblock.
 */
typedef struct {
  //! The transformed coefficients.
  tran_low_t *tcoeff[MAX_MB_PLANE];
  //! Where the transformed coefficients end.
  uint16_t *eobs[MAX_MB_PLANE];
  /*! \brief Transform block entropy contexts.
   *
   * Each element is used as a bit field.
   * - Bits 0~3: txb_skip_ctx
   * - Bits 4~5: dc_sign_ctx.
   */
  uint8_t *entropy_ctx[MAX_MB_PLANE];
} CB_COEFF_BUFFER;

/*! \brief Extended mode info derived from mbmi.
 */
typedef struct {
  // TODO(angiebird): Reduce the buffer size according to sb_type
  //! The reference mv list for the current block.
  CANDIDATE_MV ref_mv_stack[MODE_CTX_REF_FRAMES][USABLE_REF_MV_STACK_SIZE];
  //! The weights used to compute the ref mvs.
  uint16_t weight[MODE_CTX_REF_FRAMES][USABLE_REF_MV_STACK_SIZE];
  //! Number of ref mvs in the drl.
  uint8_t ref_mv_count[MODE_CTX_REF_FRAMES];
  //! Global mvs
  int_mv global_mvs[REF_FRAMES];
  //! Context used to encode the current mode.
  int16_t mode_context[MODE_CTX_REF_FRAMES];
} MB_MODE_INFO_EXT;

/*! \brief Stores best extended mode information at frame level.
 *
 * The frame level in here is used in bitstream preparation stage. The
 * information in \ref MB_MODE_INFO_EXT are copied to this struct to save
 * memory.
 */
typedef struct {
  //! \copydoc MB_MODE_INFO_EXT::ref_mv_stack
  CANDIDATE_MV ref_mv_stack[USABLE_REF_MV_STACK_SIZE];
  //! \copydoc MB_MODE_INFO_EXT::weight
  uint16_t weight[USABLE_REF_MV_STACK_SIZE];
  //! \copydoc MB_MODE_INFO_EXT::ref_mv_count
  uint8_t ref_mv_count;
  // TODO(Ravi/Remya): Reduce the buffer size of global_mvs
  //! \copydoc MB_MODE_INFO_EXT::global_mvs
  int_mv global_mvs[REF_FRAMES];
  //! \copydoc MB_MODE_INFO_EXT::mode_context
  int16_t mode_context;
  //! Offset of current coding block's coeff buffer relative to the sb.
  uint16_t cb_offset[PLANE_TYPES];
} MB_MODE_INFO_EXT_FRAME;

/*! \brief Txfm search results for a partition
 */
typedef struct {
  //! Txfm size used if the current mode is intra mode.
  TX_SIZE tx_size;
  //! Txfm sizes used if the current mode is inter mode.
  TX_SIZE inter_tx_size[INTER_TX_SIZE_BUF_LEN];
  //! Map showing which txfm block skips the txfm process.
  uint8_t blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  //! Map showing the txfm types for each blcok.
  uint8_t tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  //! Rd_stats for the whole partition block.
  RD_STATS rd_stats;
  //! Hash value of the current record.
  uint32_t hash_value;
} MB_RD_INFO;

/*! \brief Hash records of txfm search results for the partition block.
 */
typedef struct {
  //! Circular buffer that stores the txfm search results.
  MB_RD_INFO tx_rd_info[RD_RECORD_BUFFER_LEN];  // Circular buffer.
  //! Index to insert the newest \ref TXB_RD_INFO.
  int index_start;
  //! Number of info stored in this record.
  int num;
  //! Hash function
  CRC32C crc_calculator;
} MB_RD_RECORD;

/*! \brief Txfm search results for a tx block.
 */
typedef struct {
  //! Distortion after the txfm process
  int64_t dist;
  //! SSE of the prediction before the txfm process
  int64_t sse;
  //! Rate used to encode the txfm.
  int rate;
  //! Location of the end of non-zero entries.
  uint16_t eob;
  //! Transform type used on the current block.
  TX_TYPE tx_type;
  //! Unknown usage
  uint16_t entropy_context;
  //! Context used to code the coefficients.
  uint8_t txb_entropy_ctx;
  //! Whether the current info block contains  valid info
  uint8_t valid;
  //! Unused
  uint8_t fast;
  //! Whether trellis optimization is done.
  uint8_t perform_block_coeff_opt;
} TXB_RD_INFO;

/*! \brief Hash records of txfm search result for each tx block.
 */
typedef struct {
  //! The hash values.
  uint32_t hash_vals[TX_SIZE_RD_RECORD_BUFFER_LEN];
  //! The txfm search results
  TXB_RD_INFO tx_rd_info[TX_SIZE_RD_RECORD_BUFFER_LEN];
  //! Index to insert the newest \ref TXB_RD_INFO.
  int index_start;
  //! Number of info stored in this record.
  int num;
} TXB_RD_RECORD;

//! Number of compound rd stats
#define MAX_COMP_RD_STATS 64
/*! \brief Rdcost stats in compound mode.
 */
typedef struct {
  //! Rate of the compound modes.
  int32_t rate[COMPOUND_TYPES];
  //! Distortion of the compound modes.
  int64_t dist[COMPOUND_TYPES];
  //! Estimated rate of the compound modes.
  int32_t model_rate[COMPOUND_TYPES];
  //! Estimated distortion of the compound modes.
  int64_t model_dist[COMPOUND_TYPES];
  //! Rate need to send the mask type.
  int comp_rs2[COMPOUND_TYPES];
  //! Motion vector for each predictor.
  int_mv mv[2];
  //! Ref frame for each predictor.
  MV_REFERENCE_FRAME ref_frames[2];
  //! Current prediction mode.
  PREDICTION_MODE mode;
  //! Current interpolation filter.
  int_interpfilters filter;
  //! Refmv index in the drl.
  int ref_mv_idx;
  //! Whether the predictors are GLOBALMV.
  int is_global[2];
  //! Current parameters for interinter mode.
  INTERINTER_COMPOUND_DATA interinter_comp;
} COMP_RD_STATS;

/*! \brief Contains buffers used to speed up rdopt for obmc.
 *
 * See the comments for calc_target_weighted_pred for details.
 */
typedef struct {
  /*! \brief A new source weighted with the above and left predictors.
   *
   * Used to efficiently construct multiple obmc predictors during rdopt.
   */
  int32_t *wsrc;
  /*! \brief A new mask constructed from the original horz/vert mask.
   *
   * \copydetails wsrc
   */
  int32_t *mask;
  /*! \brief Prediction from the up predictor.
   *
   * Used to build the obmc predictor.
   */
  uint8_t *above_pred;
  /*! \brief Prediction from the up predictor.
   *
   * \copydetails above_pred
   */
  uint8_t *left_pred;
} OBMCBuffer;

/*! \brief Contains color maps used in palette mode.
 */
typedef struct {
  //! The best color map found.
  uint8_t best_palette_color_map[MAX_PALETTE_SQUARE];
  //! A temporary buffer used for k-means clustering.
  int kmeans_data_buf[2 * MAX_PALETTE_SQUARE];
} PALETTE_BUFFER;

/*! \brief Contains buffers used by av1_compound_type_rd()
 *
 * For sizes and alignment of these arrays, refer to
 * alloc_compound_type_rd_buffers() function.
 */
typedef struct {
  //! First prediction.
  uint8_t *pred0;
  //! Second prediction.
  uint8_t *pred1;
  //! Source - first prediction.
  int16_t *residual1;
  //! Second prediction - first prediction.
  int16_t *diff10;
  //! Backup of the best segmentation mask.
  uint8_t *tmp_best_mask_buf;
} CompoundTypeRdBuffers;

/*! \brief Holds some parameters related to partitioning schemes in AV1.
 */
// TODO(chiyotsai@google.com): Consolidate this with SIMPLE_MOTION_DATA_TREE
typedef struct {
#if !CONFIG_REALTIME_ONLY
  // The following 4 parameters are used for cnn-based partitioning on intra
  // frame.
  /*! \brief Current index on the partition block quad tree.
   *
   * Used to index into the cnn buffer for partition decision.
   */
  int quad_tree_idx;
  //! Whether the CNN buffer contains valid output.
  int cnn_output_valid;
  //! A buffer used by our segmentation CNN for intra-frame partitioning.
  float cnn_buffer[CNN_OUT_BUF_SIZE];
  //! log of the quantization parameter of the ancestor BLOCK_64X64.
  float log_q;
#endif

  /*! \brief Variance of the subblocks in the superblock.
   *
   * This is used by rt mode for variance based partitioning.
   * The indices corresponds to the following block sizes:
   * -   0    - 128x128
   * -  1-2   - 128x64
   * -  3-4   -  64x128
   * -  5-8   -  64x64
   * -  9-16  -  64x32
   * - 17-24  -  32x64
   * - 25-40  -  32x32
   * - 41-104 -  16x16
   */
  uint8_t variance_low[105];
} PartitionSearchInfo;

/*! \brief Defines the parameters used to perform txfm search.
 *
 * For the most part, this determines how various speed features are used.
 */
typedef struct {
  /*! \brief Whether to limit the intra txfm search type to the default txfm.
   *
   * This could either be a result of either sequence parameter or speed
   * features.
   */
  int use_default_intra_tx_type;
  /*! \brief Whether to limit the inter txfm search type to the default txfm.
   *
   * \copydetails use_default_intra_tx_type
   */
  int use_default_inter_tx_type;

  //! Whether to prune 2d transforms based on 1d transform results.
  int prune_2d_txfm_mode;

  /*! \brief Variable from \ref WinnerModeParams based on current eval mode.
   *
   * See the documentation for \ref WinnerModeParams for more detail.
   */
  unsigned int coeff_opt_dist_threshold;
  //! \copydoc coeff_opt_dist_threshold
  unsigned int coeff_opt_satd_threshold;
  //! \copydoc coeff_opt_dist_threshold
  unsigned int tx_domain_dist_threshold;
  //! \copydoc coeff_opt_dist_threshold
  TX_SIZE_SEARCH_METHOD tx_size_search_method;
  //! \copydoc coeff_opt_dist_threshold
  unsigned int use_transform_domain_distortion;
  //! \copydoc coeff_opt_dist_threshold
  unsigned int skip_txfm_level;

  /*! \brief How to search for the optimal tx_size
   *
   * If ONLY_4X4, use TX_4X4; if TX_MODE_LARGEST, use the largest tx_size for
   * the current partition block; if TX_MODE_SELECT, search through the whole
   * tree.
   *
   * \attention
   * Although this looks suspicious similar to a bitstream element, this
   * tx_mode_search_type is only used internally by the encoder, and is *not*
   * written to the bitstream. It determines what kind of tx_mode would be
   * searched. For example, we might set it to TX_MODE_LARGEST to find a good
   * candidate, then code it as TX_MODE_SELECT.
   */
  TX_MODE tx_mode_search_type;

  /*!
   * Flag to enable/disable DC block prediction.
   */
  unsigned int predict_dc_level;
} TxfmSearchParams;

/*!\cond */
#define MAX_NUM_8X8_TXBS ((MAX_MIB_SIZE >> 1) * (MAX_MIB_SIZE >> 1))
#define MAX_NUM_16X16_TXBS ((MAX_MIB_SIZE >> 2) * (MAX_MIB_SIZE >> 2))
#define MAX_NUM_32X32_TXBS ((MAX_MIB_SIZE >> 3) * (MAX_MIB_SIZE >> 3))
#define MAX_NUM_64X64_TXBS ((MAX_MIB_SIZE >> 4) * (MAX_MIB_SIZE >> 4))
/*!\endcond */

/*! \brief Stores various encoding/search decisions related to txfm search.
 *
 * This struct contains a cache of previous txfm results, and some buffers for
 * the current txfm decision.
 */
typedef struct {
  //! Whether to skip transform and quantization on a partition block level.
  int skip_txfm;

  /*! \brief Whether to skip transform and quantization on a txfm block level.
   *
   * Skips transform and quantization on a transform block level inside the
   * current partition block. Each element of this array is used as a bit-field.
   * So for example, the we are skipping on the luma plane, then the last bit
   * would be set to 1.
   */
  uint8_t blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];

  /*! \brief Transform types inside the partition block
   *
   * Keeps a record of what kind of transform to use for each of the transform
   * block inside the partition block.
   * \attention The buffer here is *never* directly used. Instead, this just
   * allocates the memory for MACROBLOCKD::tx_type_map during rdopt on the
   * partition block. So if we need to save memory, we could move the allocation
   * to pick_sb_mode instead.
   */
  uint8_t tx_type_map_[MAX_MIB_SIZE * MAX_MIB_SIZE];

  /** \name Txfm hash records
   * Hash records of the transform search results based on the residue. There
   * are two main types here:
   * - MB_RD_RECORD: records a whole *partition block*'s inter-mode txfm result.
   *   Since this operates on the partition block level, this can give us a
   *   whole txfm partition tree.
   * - TXB_RD_RECORD: records a txfm search result within a transform blcok
   *   itself. This operates on txb level only and onlyt appplies to square
   *   txfms.
   */
  /**@{*/
  //! Txfm hash record for the whole coding block.
  MB_RD_RECORD mb_rd_record;

  //! Inter mode txfm hash record for TX_8X8 blocks.
  TXB_RD_RECORD txb_rd_record_8X8[MAX_NUM_8X8_TXBS];
  //! Inter mode txfm hash record for TX_16X16 blocks.
  TXB_RD_RECORD txb_rd_record_16X16[MAX_NUM_16X16_TXBS];
  //! Inter mode txfm hash record for TX_32X32 blocks.
  TXB_RD_RECORD txb_rd_record_32X32[MAX_NUM_32X32_TXBS];
  //! Inter mode txfm hash record for TX_64X64 blocks.
  TXB_RD_RECORD txb_rd_record_64X64[MAX_NUM_64X64_TXBS];
  //! Intra mode txfm hash record for square tx blocks.
  TXB_RD_RECORD txb_rd_record_intra;
  /**@}*/

  /*! \brief Number of txb splits.
   *
   * Keep track of how many times we've used split tx partition for transform
   * blocks. Somewhat misleadingly, this parameter doesn't actually keep track
   * of the count of the current block. Instead, it's a cumulative count across
   * of the whole frame. The main usage is that if txb_split_count is zero, then
   * we can signal TX_MODE_LARGEST at frame level.
   */
  // TODO(chiyotsai@google.com): Move this to a more appropriate location such
  // as ThreadData.
  unsigned int txb_split_count;
#if CONFIG_SPEED_STATS
  //! For debugging. Used to check how many txfm searches we are doing.
  unsigned int tx_search_count;
#endif  // CONFIG_SPEED_STATS
} TxfmSearchInfo;
#undef MAX_NUM_8X8_TXBS
#undef MAX_NUM_16X16_TXBS
#undef MAX_NUM_32X32_TXBS
#undef MAX_NUM_64X64_TXBS

/*! \brief Holds the entropy costs for various modes sent to the bitstream.
 *
 * \attention This does not include the costs for mv and transformed
 * coefficients.
 */
typedef struct {
  /*****************************************************************************
   * \name Partition Costs
   ****************************************************************************/
  /**@{*/
  //! Cost for coding the partition.
  int partition_cost[PARTITION_CONTEXTS][EXT_PARTITION_TYPES];
  /**@}*/

  /*****************************************************************************
   * \name Intra Costs: General
   ****************************************************************************/
  /**@{*/
  //! Luma mode cost for inter frame.
  int mbmode_cost[BLOCK_SIZE_GROUPS][INTRA_MODES];
  //! Luma mode cost for intra frame.
  int y_mode_costs[INTRA_MODES][INTRA_MODES][INTRA_MODES];
  //! Chroma mode cost
  int intra_uv_mode_cost[CFL_ALLOWED_TYPES][INTRA_MODES][UV_INTRA_MODES];
  //! filter_intra_cost
  int filter_intra_cost[BLOCK_SIZES_ALL][2];
  //! filter_intra_mode_cost
  int filter_intra_mode_cost[FILTER_INTRA_MODES];
  //! angle_delta_cost
  int angle_delta_cost[DIRECTIONAL_MODES][2 * MAX_ANGLE_DELTA + 1];

  //! Rate rate associated with each alpha codeword
  int cfl_cost[CFL_JOINT_SIGNS][CFL_PRED_PLANES][CFL_ALPHABET_SIZE];
  /**@}*/

  /*****************************************************************************
   * \name Intra Costs: Screen Contents
   ****************************************************************************/
  /**@{*/
  //! intrabc_cost
  int intrabc_cost[2];

  //! palette_y_size_cost
  int palette_y_size_cost[PALATTE_BSIZE_CTXS][PALETTE_SIZES];
  //! palette_uv_size_cost
  int palette_uv_size_cost[PALATTE_BSIZE_CTXS][PALETTE_SIZES];
  //! palette_y_color_cost
  int palette_y_color_cost[PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS]
                          [PALETTE_COLORS];
  //! palette_uv_color_cost
  int palette_uv_color_cost[PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS]
                           [PALETTE_COLORS];
  //! palette_y_mode_cost
  int palette_y_mode_cost[PALATTE_BSIZE_CTXS][PALETTE_Y_MODE_CONTEXTS][2];
  //! palette_uv_mode_cost
  int palette_uv_mode_cost[PALETTE_UV_MODE_CONTEXTS][2];
  /**@}*/

  /*****************************************************************************
   * \name Inter Costs: MV Modes
   ****************************************************************************/
  /**@{*/
  //! skip_mode_cost
  int skip_mode_cost[SKIP_MODE_CONTEXTS][2];
  //! newmv_mode_cost
  int newmv_mode_cost[NEWMV_MODE_CONTEXTS][2];
  //! zeromv_mode_cost
  int zeromv_mode_cost[GLOBALMV_MODE_CONTEXTS][2];
  //! refmv_mode_cost
  int refmv_mode_cost[REFMV_MODE_CONTEXTS][2];
  //! drl_mode_cost0
  int drl_mode_cost0[DRL_MODE_CONTEXTS][2];
  /**@}*/

  /*****************************************************************************
   * \name Inter Costs: Ref Frame Types
   ****************************************************************************/
  /**@{*/
  //! single_ref_cost
  int single_ref_cost[REF_CONTEXTS][SINGLE_REFS - 1][2];
  //! comp_inter_cost
  int comp_inter_cost[COMP_INTER_CONTEXTS][2];
  //! comp_ref_type_cost
  int comp_ref_type_cost[COMP_REF_TYPE_CONTEXTS]
                        [CDF_SIZE(COMP_REFERENCE_TYPES)];
  //! uni_comp_ref_cost
  int uni_comp_ref_cost[UNI_COMP_REF_CONTEXTS][UNIDIR_COMP_REFS - 1]
                       [CDF_SIZE(2)];
  /*! \brief Cost for signaling ref_frame[0] in bidir-comp mode
   *
   * Includes LAST_FRAME, LAST2_FRAME, LAST3_FRAME, and GOLDEN_FRAME.
   */
  int comp_ref_cost[REF_CONTEXTS][FWD_REFS - 1][2];
  /*! \brief Cost for signaling ref_frame[1] in bidir-comp mode
   *
   * Includes ALTREF_FRAME, ALTREF2_FRAME, and BWDREF_FRAME.
   */
  int comp_bwdref_cost[REF_CONTEXTS][BWD_REFS - 1][2];
  /**@}*/

  /*****************************************************************************
   * \name Inter Costs: Compound Types
   ****************************************************************************/
  /**@{*/
  //! intra_inter_cost
  int intra_inter_cost[INTRA_INTER_CONTEXTS][2];
  //! inter_compound_mode_cost
  int inter_compound_mode_cost[INTER_MODE_CONTEXTS][INTER_COMPOUND_MODES];
  //! compound_type_cost
  int compound_type_cost[BLOCK_SIZES_ALL][MASKED_COMPOUND_TYPES];
  //! wedge_idx_cost
  int wedge_idx_cost[BLOCK_SIZES_ALL][16];
  //! interintra_cost
  int interintra_cost[BLOCK_SIZE_GROUPS][2];
  //! wedge_interintra_cost
  int wedge_interintra_cost[BLOCK_SIZES_ALL][2];
  //! interintra_mode_cost
  int interintra_mode_cost[BLOCK_SIZE_GROUPS][INTERINTRA_MODES];
  /**@}*/

  /*****************************************************************************
   * \name Inter Costs: Compound Masks
   ****************************************************************************/
  /**@{*/
  //! comp_idx_cost
  int comp_idx_cost[COMP_INDEX_CONTEXTS][2];
  //! comp_group_idx_cost
  int comp_group_idx_cost[COMP_GROUP_IDX_CONTEXTS][2];
  /**@}*/

  /*****************************************************************************
   * \name Inter Costs: Motion Modes/Filters
   ****************************************************************************/
  /**@{*/
  //! motion_mode_cost
  int motion_mode_cost[BLOCK_SIZES_ALL][MOTION_MODES];
  //! motion_mode_cost1
  int motion_mode_cost1[BLOCK_SIZES_ALL][2];
  //! switchable_interp_costs
  int switchable_interp_costs[SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS];
  /**@}*/

  /*****************************************************************************
   * \name Txfm Mode Costs
   ****************************************************************************/
  /**@{*/
  //! skip_txfm_cost
  int skip_txfm_cost[SKIP_CONTEXTS][2];
  //! tx_size_cost
  int tx_size_cost[TX_SIZES - 1][TX_SIZE_CONTEXTS][TX_SIZES];
  //! txfm_partition_cost
  int txfm_partition_cost[TXFM_PARTITION_CONTEXTS][2];
  //! inter_tx_type_costs
  int inter_tx_type_costs[EXT_TX_SETS_INTER][EXT_TX_SIZES][TX_TYPES];
  //! intra_tx_type_costs
  int intra_tx_type_costs[EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES]
                         [TX_TYPES];
  /**@}*/

  /*****************************************************************************
   * \name Restoration Mode Costs
   ****************************************************************************/
  /**@{*/
  //! switchable_restore_cost
  int switchable_restore_cost[RESTORE_SWITCHABLE_TYPES];
  //! wiener_restore_cost
  int wiener_restore_cost[2];
  //! sgrproj_restore_cost
  int sgrproj_restore_cost[2];
  /**@}*/
} ModeCosts;

/*! \brief Holds mv costs for encoding and motion search.
 */
typedef struct {
  /*****************************************************************************
   * \name Rate to Distortion Multipliers
   ****************************************************************************/
  /**@{*/
  //! A multiplier that converts mv cost to l2 error.
  int errorperbit;
  //! A multiplier that converts mv cost to l1 error.
  int sadperbit;
  /**@}*/

  /*****************************************************************************
   * \name Encoding Costs
   * Here are the entropy costs needed to encode a given mv.
   * \ref nmv_cost_alloc and \ref nmv_cost_hp_alloc are two arrays that holds
   * the memory for holding the mv cost. But since the motion vectors can be
   * negative, we shift them to the middle and store the resulting pointer in
   * \ref nmv_cost and \ref nmv_cost_hp for easier referencing. Finally, \ref
   * mv_cost_stack points to the \ref nmv_cost with the mv precision we are
   * currently working with. In essence, only \ref mv_cost_stack is needed for
   * motion search, the other can be considered private.
   ****************************************************************************/
  /**@{*/
  //! Costs for coding the zero components.
  int nmv_joint_cost[MV_JOINTS];

  //! Allocates memory for 1/4-pel motion vector costs.
  int nmv_cost_alloc[2][MV_VALS];
  //! Allocates memory for 1/8-pel motion vector costs.
  int nmv_cost_hp_alloc[2][MV_VALS];
  //! Points to the middle of \ref nmv_cost_alloc
  int *nmv_cost[2];
  //! Points to the middle of \ref nmv_cost_hp_alloc
  int *nmv_cost_hp[2];
  //! Points to the nmv_cost_hp in use.
  int **mv_cost_stack;
  /**@}*/
} MvCosts;

/*! \brief Holds the costs needed to encode the coefficients
 */
typedef struct {
  //! Costs for coding the coefficients.
  LV_MAP_COEFF_COST coeff_costs[TX_SIZES][PLANE_TYPES];
  //! Costs for coding the eobs.
  LV_MAP_EOB_COST eob_costs[7][2];
} CoeffCosts;

/*!\cond */
// 4: NEAREST, NEW, NEAR, GLOBAL
#define SINGLE_REF_MODES ((REF_FRAMES - 1) * 4)
/*!\endcond */
struct inter_modes_info;

/*! \brief Holds the motion samples for warp motion model estimation
 */
typedef struct {
  //! Number of samples.
  int num;
  //! Sample locations in current frame.
  int pts[16];
  //! Sample location in the reference frame.
  int pts_inref[16];
} WARP_SAMPLE_INFO;

/*! \brief Encoder's parameters related to the current coding block.
 *
 * This struct contains most of the information the encoder needs to encode the
 * current coding block. This includes the src and pred buffer, a copy of the
 * decoder's view of the current block, the txfm coefficients. This struct also
 * contains various buffers and data used to speed up the encoding process.
 */
typedef struct macroblock {
  /*****************************************************************************
   * \name Source, Buffers and Decoder
   ****************************************************************************/
  /**@{*/
  /*! \brief Each of the encoding plane.
   *
   * An array holding the src buffer for each of plane of the current block. It
   * also contains the txfm and quantized txfm coefficients.
   */
  struct macroblock_plane plane[MAX_MB_PLANE];

  /*! \brief Decoder's view of current coding block.
   *
   * Contains the encoder's copy of what the decoder sees in the current block.
   * Most importantly, this struct contains pointers to mbmi that is used in
   * final bitstream packing.
   */
  MACROBLOCKD e_mbd;

  /*! \brief Derived coding information.
   *
   * Contains extra information not transmitted in the bitstream but are
   * derived. For example, this contains the stack of ref_mvs.
   */
  MB_MODE_INFO_EXT mbmi_ext;

  /*! \brief Finalized mbmi_ext for the whole frame.
   *
   * Contains the finalized info in mbmi_ext that gets used at the frame level
   * for bitstream packing.
   */
  MB_MODE_INFO_EXT_FRAME *mbmi_ext_frame;

  //! Entropy context for the current row.
  FRAME_CONTEXT *row_ctx;
  /*! \brief Entropy context for the current tile.
   *
   * This context will be used to update color_map_cdf pointer which would be
   * used during pack bitstream. For single thread and tile-multithreading case
   * this pointer will be same as xd->tile_ctx, but for the case of row-mt:
   * xd->tile_ctx will point to a temporary context while tile_pb_ctx will point
   * to the accurate tile context.
   */
  FRAME_CONTEXT *tile_pb_ctx;

  /*! \brief Buffer of transformed coefficients
   *
   * Points to cb_coef_buff in the AV1_COMP struct, which contains the finalized
   * coefficients. This is here to conveniently copy the best coefficients to
   * frame level for bitstream packing. Since CB_COEFF_BUFFER is allocated on a
   * superblock level, we need to combine it with cb_offset to get the proper
   * position for the current coding block.
   */
  CB_COEFF_BUFFER *cb_coef_buff;
  //! Offset of current coding block's coeff buffer relative to the sb.
  uint16_t cb_offset[PLANE_TYPES];

  //! Modified source and masks used for fast OBMC search.
  OBMCBuffer obmc_buffer;
  //! Buffer to store the best palette map.
  PALETTE_BUFFER *palette_buffer;
  //! Buffer used for compound_type_rd().
  CompoundTypeRdBuffers comp_rd_buffer;
  //! Buffer to store convolution during averaging process in compound mode.
  CONV_BUF_TYPE *tmp_conv_dst;

  /*! \brief Temporary buffer to hold prediction.
   *
   * Points to a buffer that is used to hold temporary prediction results. This
   * is used in two ways:
   * - This is a temporary buffer used to pingpong the prediction in
   *   handle_inter_mode.
   * - xd->tmp_obmc_bufs also points to this buffer, and is used in ombc
   *   prediction.
   */
  uint8_t *tmp_pred_bufs[2];
  /**@}*/

  /*****************************************************************************
   * \name Rdopt Costs
   ****************************************************************************/
  /**@{*/
  /*! \brief Quantization index for the current partition block.
   *
   * This is used to as the index to find quantization parameter for luma and
   * chroma transformed coefficients.
   */
  int qindex;

  /*! \brief Difference between frame-level qindex and current qindex.
   *
   *  This is used to track whether a non-zero delta for qindex is used at least
   *  once in the current frame.
   */
  int delta_qindex;

  /*! \brief Rate-distortion multiplier.
   *
   * The rd multiplier used to determine the rate-distortion trade-off. This is
   * roughly proportional to the inverse of q-index for a given frame, but this
   * can be manipulated for better rate-control. For example, in tune_ssim
   * mode, this is scaled by a factor related to the variance of the current
   * block.
   */
  int rdmult;

  //! Energy in the current source coding block. Used to calculate \ref rdmult
  int mb_energy;
  //! Energy in the current source superblock. Used to calculate \ref rdmult
  int sb_energy_level;

  //! The rate needed to signal a mode to the bitstream.
  ModeCosts mode_costs;

  //! The rate needed to encode a new motion vector to the bitstream and some
  //! multipliers for motion search.
  MvCosts mv_costs;

  //! The rate needed to signal the txfm coefficients to the bitstream.
  CoeffCosts coeff_costs;
  /**@}*/

  /******************************************************************************
   * \name Segmentation
   *****************************************************************************/
  /**@{*/
  /*! \brief Skip mode for the segment
   *
   * A syntax element of the segmentation mode. In skip_block mode, all mvs are
   * set 0 and all txfms are skipped.
   */
  int seg_skip_block;
  /**@}*/

  /*****************************************************************************
   * \name Superblock
   ****************************************************************************/
  /**@{*/
  //! Information on a whole superblock level.
  // TODO(chiyotsai@google.com): Refactor this out of macroblock
  SuperBlockEnc sb_enc;

  /*! \brief Characteristics of the current superblock.
   *
   *  Characteristics like whether the block has high sad, low sad, etc. This is
   *  only used by av1 realtime mode.
   */
  uint8_t content_state_sb;
  /**@}*/

  /*****************************************************************************
   * \name Reference Frame Search
   ****************************************************************************/
  /**@{*/
  /*! \brief Sum absolute distortion of the predicted mv for each ref frame.
   *
   * This is used to measure how viable a reference frame is.
   */
  int pred_mv_sad[REF_FRAMES];
  //! The minimum of \ref pred_mv_sad.
  int best_pred_mv_sad;

  /*! \brief Disables certain ref frame pruning based on tpl.
   *
   * Determines whether a given ref frame is "good" based on data from the TPL
   * model. If so, this stops selective_ref frame from pruning the given ref
   * frame at block level.
   */
  uint8_t tpl_keep_ref_frame[REF_FRAMES];

  /*! \brief Warp motion samples buffer.
   *
   * Store the motion samples used for warp motion.
   */
  WARP_SAMPLE_INFO warp_sample_info[REF_FRAMES];

  /*! \brief Reference frames picked by the square subblocks in a superblock.
   *
   * Keeps track of ref frames that are selected by square partition blocks
   * within a superblock, in MI resolution. They can be used to prune ref frames
   * for rectangular blocks.
   */
  int picked_ref_frames_mask[MAX_MIB_SIZE * MAX_MIB_SIZE];

  /*! \brief Prune ref frames in real-time mode.
   *
   * Determines whether to prune reference frames in real-time mode. For the
   * most part, this is the same as nonrd_prune_ref_frame_search in
   * cpi->sf.rt_sf.nonrd_prune_ref_frame_search, but this can be selectively
   * turned off if the only frame available is GOLDEN_FRAME.
   */
  int nonrd_prune_ref_frame_search;
  /**@}*/

  /*****************************************************************************
   * \name Partition Search
   ****************************************************************************/
  /**@{*/
  //! Stores some partition-search related buffers.
  PartitionSearchInfo part_search_info;

  /*! \brief Whether to disable some features to force a mode in current block.
   *
   * In some cases, our speed features can be overly aggressive and remove all
   * modes search in the superblock. When this happens, we set
   * must_find_valid_partition to 1 to reduce the number of speed features, and
   * recode the superblock again.
   */
  int must_find_valid_partition;
  /**@}*/

  /*****************************************************************************
   * \name Prediction Mode Search
   ****************************************************************************/
  /**@{*/
  /*! \brief Inter skip mode.
   *
   * Skip mode tries to use the closest forward and backward references for
   * inter prediction. Skip here means to skip transmitting the reference
   * frames, not to be confused with skip_txfm.
   */
  int skip_mode;

  /*! \brief Factors used for rd-thresholding.
   *
   * Determines a rd threshold to determine whether to continue searching the
   * current mode. If the current best rd is already <= threshold, then we skip
   * the current mode.
   */
  int thresh_freq_fact[BLOCK_SIZES_ALL][MAX_MODES];

  /*! \brief Tracks the winner modes in the current coding block.
   *
   * Winner mode is a two-pass strategy to find the best prediction mode. In the
   * first pass, we search the prediction modes with a limited set of txfm
   * options, and keep the top modes. These modes are called the winner modes.
   * In the second pass, we retry the winner modes with more thorough txfm
   * options.
   */
  WinnerModeStats winner_mode_stats[AOMMAX(MAX_WINNER_MODE_COUNT_INTRA,
                                           MAX_WINNER_MODE_COUNT_INTER)];
  //! Tracks how many winner modes there are.
  int winner_mode_count;

  /*! \brief The model used for rd-estimation to avoid txfm
   *
   * These are for inter_mode_rd_model_estimation, which is another two pass
   * approach. In this speed feature, we collect data in the first couple frames
   * to build an rd model to estimate the rdcost of a prediction model based on
   * the residue error. Once enough data is collected, this speed feature uses
   * the estimated rdcost to find the most performant prediction mode. Then we
   * follow up with a second pass find the best transform for the mode.
   * Determines if one would go with reduced complexity transform block
   * search model to select prediction modes, or full complexity model
   * to select transform kernel.
   */
  TXFM_RD_MODEL rd_model;

  /*! \brief Stores the inter mode information needed to build an rd model.
   *
   * These are for inter_mode_rd_model_estimation, which is another two pass
   * approach. In this speed feature, we collect data in the first couple frames
   * to build an rd model to estimate the rdcost of a prediction model based on
   * the residue error. Once enough data is collected, this speed feature uses
   * the estimated rdcost to find the most performant prediction mode. Then we
   * follow up with a second pass find the best transform for the mode.
   */
  // TODO(any): try to consolidate this speed feature with winner mode
  // processing.
  struct inter_modes_info *inter_modes_info;

  //! How to blend the compound predictions.
  uint8_t compound_idx;

  //! A caches of results of compound type search so they can be reused later.
  COMP_RD_STATS comp_rd_stats[MAX_COMP_RD_STATS];
  //! The idx for the latest compound mode in the cache \ref comp_rd_stats.
  int comp_rd_stats_idx;

  /*! \brief Whether to recompute the luma prediction.
   *
   * In interpolation search, we can usually skip recalculating the luma
   * prediction because it is already calculated by a previous predictor. This
   * flag signifies that some modes might have been skipped, so we need to
   * rebuild the prediction.
   */
  int recalc_luma_mc_data;

  /*! \brief Data structure to speed up intrabc search.
   *
   * Contains the hash table, hash function, and buffer used for intrabc.
   */
  IntraBCHashInfo intrabc_hash_info;

  /*! \brief Whether to reuse the mode stored in intermode_cache. */
  int use_intermode_cache;
  /*! \brief The mode to reuse during \ref av1_rd_pick_inter_mode_sb. */
  const MB_MODE_INFO *intermode_cache;
  /**@}*/

  /*****************************************************************************
   * \name MV Search
   ****************************************************************************/
  /**@{*/
  /*! \brief Context used to determine the initial step size in motion search.
   *
   * This context is defined as the \f$l_\inf\f$ norm of the best ref_mvs for
   * each frame.
   */
  unsigned int max_mv_context[REF_FRAMES];

  /*! \brief Limit for the range of motion vectors.
   *
   * These define limits to motion vector components to prevent them from
   * extending outside the UMV borders
   */
  FullMvLimits mv_limits;
  /**@}*/

  /*****************************************************************************
   * \name Txfm Search
   ****************************************************************************/
  /**@{*/
  /*! \brief Parameters that control how motion search is done.
   *
   * Stores various txfm search related parameters such as txfm_type, txfm_size,
   * trellis eob search, etc.
   */
  TxfmSearchParams txfm_search_params;

  /*! \brief Results of the txfm searches that have been done.
   *
   * Caches old txfm search results and keeps the current txfm decisions to
   * facilitate rdopt.
   */
  TxfmSearchInfo txfm_search_info;

  /*! \brief Whether there is a strong color activity.
   *
   * Used in REALTIME coding mode to enhance the visual quality at the boundary
   * of moving color objects.
   */
  uint8_t color_sensitivity[2];
  /**@}*/

  /*****************************************************************************
   * \name Misc
   ****************************************************************************/
  /**@{*/
  //! Variance of the source frame.
  unsigned int source_variance;
  //! SSE of the current predictor.
  unsigned int pred_sse[REF_FRAMES];
  //! Prediction for ML based partition.
  DECLARE_ALIGNED(16, uint8_t, est_pred[128 * 128]);
  /**@}*/
} MACROBLOCK;
#undef SINGLE_REF_MODES

/*!\cond */
static INLINE int is_rect_tx_allowed_bsize(BLOCK_SIZE bsize) {
  static const char LUT[BLOCK_SIZES_ALL] = {
    0,  // BLOCK_4X4
    1,  // BLOCK_4X8
    1,  // BLOCK_8X4
    0,  // BLOCK_8X8
    1,  // BLOCK_8X16
    1,  // BLOCK_16X8
    0,  // BLOCK_16X16
    1,  // BLOCK_16X32
    1,  // BLOCK_32X16
    0,  // BLOCK_32X32
    1,  // BLOCK_32X64
    1,  // BLOCK_64X32
    0,  // BLOCK_64X64
    0,  // BLOCK_64X128
    0,  // BLOCK_128X64
    0,  // BLOCK_128X128
    1,  // BLOCK_4X16
    1,  // BLOCK_16X4
    1,  // BLOCK_8X32
    1,  // BLOCK_32X8
    1,  // BLOCK_16X64
    1,  // BLOCK_64X16
  };

  return LUT[bsize];
}

static INLINE int is_rect_tx_allowed(const MACROBLOCKD *xd,
                                     const MB_MODE_INFO *mbmi) {
  return is_rect_tx_allowed_bsize(mbmi->bsize) &&
         !xd->lossless[mbmi->segment_id];
}

static INLINE int tx_size_to_depth(TX_SIZE tx_size, BLOCK_SIZE bsize) {
  TX_SIZE ctx_size = max_txsize_rect_lookup[bsize];
  int depth = 0;
  while (tx_size != ctx_size) {
    depth++;
    ctx_size = sub_tx_size_map[ctx_size];
    assert(depth <= MAX_TX_DEPTH);
  }
  return depth;
}

static INLINE void set_blk_skip(uint8_t txb_skip[], int plane, int blk_idx,
                                int skip) {
  if (skip)
    txb_skip[blk_idx] |= 1UL << plane;
  else
    txb_skip[blk_idx] &= ~(1UL << plane);
#ifndef NDEBUG
  // Set chroma planes to uninitialized states when luma is set to check if
  // it will be set later
  if (plane == 0) {
    txb_skip[blk_idx] |= 1UL << (1 + 4);
    txb_skip[blk_idx] |= 1UL << (2 + 4);
  }

  // Clear the initialization checking bit
  txb_skip[blk_idx] &= ~(1UL << (plane + 4));
#endif
}

static INLINE int is_blk_skip(uint8_t *txb_skip, int plane, int blk_idx) {
#ifndef NDEBUG
  // Check if this is initialized
  assert(!(txb_skip[blk_idx] & (1UL << (plane + 4))));

  // The magic number is 0x77, this is to test if there is garbage data
  assert((txb_skip[blk_idx] & 0x88) == 0);
#endif
  return (txb_skip[blk_idx] >> plane) & 1;
}

/*!\endcond */

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_BLOCK_H_
