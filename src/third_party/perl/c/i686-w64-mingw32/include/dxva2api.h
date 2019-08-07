/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the w64 mingw-runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef _INC_DXVA2API
#define _INC_DXVA2API
#if (_WIN32_WINNT >= 0x0600)
#include <objbase.h>
#include <d3d9.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifndef __REFERENCE_TIME_DEFINED
#define __REFERENCE_TIME_DEFINED
typedef LONGLONG REFERENCE_TIME;
#endif /*__REFERENCE_TIME_DEFINED*/

#define DXVA2_ProcAmp_None 0x0000
#define DXVA2_ProcAmp_Brightness 0x0001
#define DXVA2_ProcAmp_Contrast 0x0002
#define DXVA2_ProcAmp_Hue 0x0004
#define DXVA2_ProcAmp_Saturation 0x0008

#define DXVA2_E_NOT_INITIALIZED     MAKE_HRESULT(1, 4, 4096)
#define DXVA2_E_NEW_VIDEO_DEVICE    MAKE_HRESULT(1, 4, 4097)
#define DXVA2_E_VIDEO_DEVICE_LOCKED MAKE_HRESULT(1, 4, 4098)
#define DXVA2_E_NOT_AVAILABLE       MAKE_HRESULT(1, 4, 4099)


typedef struct IDirect3DDeviceManager9 IDirect3DDeviceManager9;
typedef struct IDirectXVideoDecoderService IDirectXVideoDecoderService;

typedef enum _DXVA2_SampleFormat {
  DXVA2_SampleFormatMask                  = 0x00FF,
  DXVA2_SampleUnknown                     = 0,
  DXVA2_SampleProgressiveFrame            = 2,
  DXVA2_SampleFieldInterleavedEvenFirst   = 3,
  DXVA2_SampleFieldInterleavedOddFirst    = 4,
  DXVA2_SampleFieldSingleEven             = 5,
  DXVA2_SampleFieldSingleOdd              = 6,
  DXVA2_SampleSubStream                   = 7 
} DXVA2_SampleFormat;

typedef enum _DXVA2_VideoChromaSubSampling {
  DXVA2_VideoChromaSubsamplingMask                              = 0x0F,
  DXVA2_VideoChromaSubsampling_Unknown                          = 0,
  DXVA2_VideoChromaSubsampling_ProgressiveChroma                = 0x8,
  DXVA2_VideoChromaSubsampling_Horizontally_Cosited             = 0x4,
  DXVA2_VideoChromaSubsampling_Vertically_Cosited               = 0x2,
  DXVA2_VideoChromaSubsampling_Vertically_AlignedChromaPlanes   = 0x1,
  DXVA2_VideoChromaSubsampling_MPEG2                            = 
              DXVA2_VideoChromaSubsampling_Horizontally_Cosited |
              DXVA2_VideoChromaSubsampling_Vertically_AlignedChromaPlanes,
  DXVA2_VideoChromaSubsampling_MPEG1                            = 
              DXVA2_VideoChromaSubsampling_Vertically_AlignedChromaPlanes,
  DXVA2_VideoChromaSubsampling_DV_PAL                           = 
              DXVA2_VideoChromaSubsampling_Horizontally_Cosited | 
              DXVA2_VideoChromaSubsampling_Vertically_Cosited,
  DXVA2_VideoChromaSubsampling_Cosited                          = 
              DXVA2_VideoChromaSubsampling_Horizontally_Cosited | 
              DXVA2_VideoChromaSubsampling_Vertically_Cosited | 
              DXVA2_VideoChromaSubsampling_Vertically_AlignedChromaPlanes 
} DXVA2_VideoChromaSubSampling;

typedef enum _DXVA2_NominalRange {
  DXVA2_NominalRangeMask       = 0x07,
  DXVA2_NominalRange_Unknown   = 0,
  DXVA2_NominalRange_Normal    = 1,
  DXVA2_NominalRange_Wide      = 2,
  DXVA2_NominalRange_0_255     = 1,
  DXVA2_NominalRange_16_235    = 2,
  DXVA2_NominalRange_48_208    = 3 
} DXVA2_NominalRange;

typedef enum _DXVA2_VideoLighting {
  DXVA2_VideoLightingMask       = 0x0F,
  DXVA2_VideoLighting_Unknown   = 0,
  DXVA2_VideoLighting_bright    = 1,
  DXVA2_VideoLighting_office    = 2,
  DXVA2_VideoLighting_dim       = 3,
  DXVA2_VideoLighting_dark      = 4 
} DXVA2_VideoLighting;

typedef enum _DXVA2_VideoPrimaries {
  DXVA2_VideoPrimariesMask             = 0x001f,
  DXVA2_VideoPrimaries_Unknown         = 0,
  DXVA2_VideoPrimaries_reserved        = 1,
  DXVA2_VideoPrimaries_BT709           = 2,
  DXVA2_VideoPrimaries_BT470_2_SysM    = 3,
  DXVA2_VideoPrimaries_BT470_2_SysBG   = 4,
  DXVA2_VideoPrimaries_SMPTE170M       = 5,
  DXVA2_VideoPrimaries_SMPTE240M       = 6,
  DXVA2_VideoPrimaries_EBU3213         = 7,
  DXVA2_VideoPrimaries_SMPTE_C         = 8 
} DXVA2_VideoPrimaries;

typedef enum _DXVA2_VideoTransferFunction {
  DXVA2_VideoTransFuncMask       = 0x001f,
  DXVA2_VideoTransFunc_Unknown   = 0,
  DXVA2_VideoTransFunc_10        = 1,
  DXVA2_VideoTransFunc_18        = 2,
  DXVA2_VideoTransFunc_20        = 3,
  DXVA2_VideoTransFunc_22        = 4,
  DXVA2_VideoTransFunc_709       = 5,
  DXVA2_VideoTransFunc_240M      = 6,
  DXVA2_VideoTransFunc_sRGB      = 7,
  DXVA2_VideoTransFunc_28        = 8 
} DXVA2_VideoTransferFunction;

typedef enum  {
  DXVA2_SurfaceType_DecoderRenderTarget      = 0,
  DXVA2_SurfaceType_ProcessorRenderTarget    = 1,
  DXVA2_SurfaceType_D3DRenderTargetTexture   = 2 
} DXVA2_SurfaceType;

typedef enum _DXVA2_VideoTransferMatrix {
  DXVA2_VideoTransferMatrixMask         = 0x07,
  DXVA2_VideoTransferMatrix_Unknown     = 0,
  DXVA2_VideoTransferMatrix_BT709       = 1,
  DXVA2_VideoTransferMatrix_BT601       = 2,
  DXVA2_VideoTransferMatrix_SMPTE240M   = 3 
} DXVA2_VideoTransferMatrix;

typedef struct _DXVA2_AYUVSample16 {
  USHORT Cr;
  USHORT Cb;
  USHORT Y;
  USHORT Alpha;
} DXVA2_AYUVSample16;

typedef struct _DXVA2_AYUVSample8 {
  UCHAR Cr;
  UCHAR Cb;
  UCHAR Y;
  UCHAR Alpha;
} DXVA2_AYUVSample8;

typedef struct _DXVA2_ConfigPictureDecode {
  GUID   guidConfigBitstreamEncryption;
  GUID   guidConfigMBcontrolEncryption;
  GUID   guidConfigResidDiffEncryption;
  UINT   ConfigBitstreamRaw;
  UINT   ConfigMBcontrolRasterOrder;
  UINT   ConfigResidDiffHost;
  UINT   ConfigSpatialResid8;
  UINT   ConfigResid8Subtraction;
  UINT   ConfigSpatialHost8or9Clipping;
  UINT   ConfigSpatialResidInterleaved;
  UINT   ConfigIntraResidUnsigned;
  UINT   ConfigResidDiffAccelerator;
  UINT   ConfigHostInverseScan;
  UINT   ConfigSpecificIDCT;
  UINT   Config4GroupedCoefs;
  USHORT ConfigMinRenderTargetBuffCount;
  USHORT ConfigDecoderSpecific;
} DXVA2_ConfigPictureDecode;

typedef struct _DXVA2_DecodeBufferDesc {
  DWORD CompressedBufferType;
  UINT  BufferIndex;
  UINT  DataOffset;
  UINT  DataSize;
  UINT  FirstMBaddress;
  UINT  NumMBsInBuffer;
  UINT  Width;
  UINT  Height;
  UINT  Stride;
  UINT  ReservedBits;
  PVOID pvPVPState;
} DXVA2_DecodeBufferDesc;

typedef struct _DXVA2_DecodeExtensionData {
  UINT  Function;
  PVOID pPrivateInputData;
  UINT  PrivateInputDataSize;
  PVOID pPrivateOutputData;
  UINT  PrivateOutputDataSize;
} DXVA2_DecodeExtensionData;

typedef struct _DXVA2_DecodeExecuteParams {
  UINT                      NumCompBuffers;
  DXVA2_DecodeBufferDesc    *pCompressedBuffers;
  DXVA2_DecodeExtensionData *pExtensionData;
} DXVA2_DecodeExecuteParams;

typedef struct {
  __C89_NAMELESS union {
    __C89_NAMELESS struct {
      UINT SampleFormat            :8;
      UINT VideoChromaSubsampling  :4;
      UINT NominalRange            :3;
      UINT VideoTransferMatrix     :3;
      UINT VideoLighting           :4;
      UINT VideoPrimaries          :5;
      UINT VideoTransferFunction   :5;
    } DUMMYSTRUCTNAME;
    UINT   value;
  } DUMMYUNIONNAME;
} DXVA2_ExtendedFormat;

typedef struct _DXVA2_Fixed32 {
  __C89_NAMELESS union {
    __C89_NAMELESS struct {
      USHORT Fraction;
      SHORT  Value;
    } DUMMYSTRUCTNAME;
    LONG   ll;
  } DUMMYUNIONNAME;
} DXVA2_Fixed32;

typedef struct _DXVA2_FilterValues {
  DXVA2_Fixed32 Level;
  DXVA2_Fixed32 Threshold;
  DXVA2_Fixed32 Radius;
} DXVA2_FilterValues;

typedef struct _DXVA2_Frequency {
  UINT Numerator;
  UINT Denominator;
} DXVA2_Frequency;

typedef struct _DXVA2_ProcAmpValues {
  DXVA2_Fixed32 Brightness;
  DXVA2_Fixed32 Contrast;
  DXVA2_Fixed32 Hue;
  DXVA2_Fixed32 Saturation;
} DXVA2_ProcAmpValues;

typedef struct _DXVA2_ValueRange {
  DXVA2_Fixed32 MinValue;
  DXVA2_Fixed32 MaxValue;
  DXVA2_Fixed32 DefaultValue;
  DXVA2_Fixed32 StepSize;
} DXVA2_ValueRange;

typedef struct _DXVA2_VideoDesc {
  UINT                 SampleWidth;
  UINT                 SampleHeight;
  DXVA2_ExtendedFormat SampleFormat;
  D3DFORMAT            Format;
  DXVA2_Frequency      InputSampleFreq;
  DXVA2_Frequency      OutputFrameFreq;
  UINT                 UABProtectionLevel;
  UINT                 Reserved;
} DXVA2_VideoDesc;

/* DeviceCaps
DXVA2_VPDev_EmulatedDXVA1
DXVA2_VPDev_HardwareDevice
DXVA2_VPDev_SoftwareDevice
*/
/* DeinterlaceTechnology
DXVA2_DeinterlaceTech_Unknown
DXVA2_DeinterlaceTech_BOBLineReplicate
DXVA2_DeinterlaceTech_BOBVerticalStretch
DXVA2_DeinterlaceTech_BOBVerticalStretch4Tap
DXVA2_DeinterlaceTech_MedianFiltering
DXVA2_DeinterlaceTech_EdgeFiltering
DXVA2_DeinterlaceTech_FieldAdaptive
DXVA2_DeinterlaceTech_PixelAdaptive
DXVA2_DeinterlaceTech_MotionVectorSteered
DXVA2_DeinterlaceTech_InverseTelecine
*/

/* VideoProcessorOperations
DXVA2_VideoProcess_YUV2RGB
DXVA2_VideoProcess_StretchX
DXVA2_VideoProcess_StretchY
DXVA2_VideoProcess_AlphaBlend
DXVA2_VideoProcess_SubRects
DXVA2_VideoProcess_SubStreams
DXVA2_VideoProcess_SubStreamsExtended
DXVA2_VideoProcess_YUV2RGBExtended
DXVA2_VideoProcess_AlphaBlendExtended
DXVA2_VideoProcess_Constriction
DXVA2_VideoProcess_NoiseFilter
DXVA2_VideoProcess_DetailFilter
DXVA2_VideoProcess_PlanarAlpha
DXVA2_VideoProcess_LinearScaling
DXVA2_VideoProcess_GammaCompensated
DXVA2_VideoProcess_MaintainsOriginalFieldData
*/

/*NoiseFilterTechnology
DXVA2_NoiseFilterTech_Unsupported
DXVA2_NoiseFilterTech_Unknown
DXVA2_NoiseFilterTech_Median
DXVA2_NoiseFilterTech_Temporal
DXVA2_NoiseFilterTech_BlockNoise
DXVA2_NoiseFilterTech_MosquitoNoise
*/

/* DetailFilterTechnology
DXVA2_DetailFilterTech_Unsupported
DXVA2_DetailFilterTech_Unknown
DXVA2_DetailFilterTech_Edge
DXVA2_DetailFilterTech_Sharpening
*/
typedef struct _DXVA2_VideoProcessBltParams {
  REFERENCE_TIME       TargetFrame;
  RECT                 TargetRect;
  SIZE                 ConstrictionSize;
  UINT                 StreamingFlags;
  DXVA2_AYUVSample16   BackgroundColor;
  DXVA2_ExtendedFormat DestFormat;
  DXVA2_ProcAmpValues  ProcAmpValues;
  DXVA2_Fixed32        Alpha;
  DXVA2_FilterValues   NoiseFilterLuma;
  DXVA2_FilterValues   NoiseFilterChroma;
  DXVA2_FilterValues   DetailFilterLuma;
  DXVA2_FilterValues   DetailFilterChroma;
  DWORD                DestData;
} DXVA2_VideoProcessBltParams;

typedef struct _DXVA2_VideoProcessorCaps {
  UINT    DeviceCaps;
  D3DPOOL InputPool;
  UINT    NumForwardRefSamples;
  UINT    NumBackwardRefSamples;
  UINT    Reserved;
  UINT    DeinterlaceTechnology;
  UINT    ProcAmpControlCaps;
  UINT    VideoProcessorOperations;
  UINT    NoiseFilterTechnology;
  UINT    DetailFilterTechnology;
} DXVA2_VideoProcessorCaps;

/* SampleData
DXVA2_SampleData_RFF
DXVA2_SampleData_TFF
DXVA2_SampleData_RFF_TFF_Present
*/

typedef struct _DXVA2_VideoSample {
  REFERENCE_TIME       Start;
  REFERENCE_TIME       End;
  DXVA2_ExtendedFormat SampleFormat;
  IDirect3DSurface9*   SrcSurface;
  RECT                 SrcRect;
  RECT                 DstRect;
  DXVA2_AYUVSample8    Pal[16];
  DXVA2_Fixed32        PlanarAlpha;
  DWORD                SampleData;
} DXVA2_VideoSample;

/* DXVA H264 */
typedef struct {
    __C89_NAMELESS union {
        __C89_NAMELESS struct {
            UCHAR Index7Bits     : 7;
            UCHAR AssociatedFlag : 1;
        };
        UCHAR bPicEntry;
    };
} DXVA_PicEntry_H264;

#pragma pack(push, 1)
typedef struct {
    USHORT  wFrameWidthInMbsMinus1;
    USHORT  wFrameHeightInMbsMinus1;
    DXVA_PicEntry_H264 InPic;
    DXVA_PicEntry_H264 OutPic;
    USHORT  PicOrderCnt_offset;
    INT     CurrPicOrderCnt;
    UINT    StatusReportFeedbackNumber;
    UCHAR   model_id;
    UCHAR   separate_colour_description_present_flag;
    UCHAR   film_grain_bit_depth_luma_minus8;
    UCHAR   film_grain_bit_depth_chroma_minus8;
    UCHAR   film_grain_full_range_flag;
    UCHAR   film_grain_colour_primaries;
    UCHAR   film_grain_transfer_characteristics;
    UCHAR   film_grain_matrix_coefficients;
    UCHAR   blending_mode_id;
    UCHAR   log2_scale_factor;
    UCHAR   comp_model_present_flag[4];
    UCHAR   num_intensity_intervals_minus1[4];
    UCHAR   num_model_values_minus1[4];
    UCHAR   intensity_interval_lower_bound[3][16];
    UCHAR   intensity_interval_upper_bound[3][16];
    SHORT   comp_model_value[3][16][8];
} DXVA_FilmGrainChar_H264;
#pragma pack(pop)

/* DXVA MPEG-I/II and VC-1 */
typedef struct _DXVA_PictureParameters {
    USHORT  wDecodedPictureIndex;
    USHORT  wDeblockedPictureIndex;
    USHORT  wForwardRefPictureIndex;
    USHORT  wBackwardRefPictureIndex;
    USHORT  wPicWidthInMBminus1;
    USHORT  wPicHeightInMBminus1;
    UCHAR   bMacroblockWidthMinus1;
    UCHAR   bMacroblockHeightMinus1;
    UCHAR   bBlockWidthMinus1;
    UCHAR   bBlockHeightMinus1;
    UCHAR   bBPPminus1;
    UCHAR   bPicStructure;
    UCHAR   bSecondField;
    UCHAR   bPicIntra;
    UCHAR   bPicBackwardPrediction;
    UCHAR   bBidirectionalAveragingMode;
    UCHAR   bMVprecisionAndChromaRelation;
    UCHAR   bChromaFormat;
    UCHAR   bPicScanFixed;
    UCHAR   bPicScanMethod;
    UCHAR   bPicReadbackRequests;
    UCHAR   bRcontrol;
    UCHAR   bPicSpatialResid8;
    UCHAR   bPicOverflowBlocks;
    UCHAR   bPicExtrapolation;
    UCHAR   bPicDeblocked;
    UCHAR   bPicDeblockConfined;
    UCHAR   bPic4MVallowed;
    UCHAR   bPicOBMC;
    UCHAR   bPicBinPB;
    UCHAR   bMV_RPS;
    UCHAR   bReservedBits;
    USHORT  wBitstreamFcodes;
    USHORT  wBitstreamPCEelements;
    UCHAR   bBitstreamConcealmentNeed;
    UCHAR   bBitstreamConcealmentMethod;
} DXVA_PictureParameters, *LPDXVA_PictureParameters;

typedef struct _DXVA_QmatrixData {
    BYTE    bNewQmatrix[4];
    WORD    Qmatrix[4][8 * 8];
} DXVA_QmatrixData, *LPDXVA_QmatrixData;

#pragma pack(push, 1)
typedef struct _DXVA_SliceInfo {
    USHORT  wHorizontalPosition;
    USHORT  wVerticalPosition;
    UINT    dwSliceBitsInBuffer;
    UINT    dwSliceDataLocation;
    UCHAR   bStartCodeBitOffset;
    UCHAR   bReservedBits;
    USHORT  wMBbitOffset;
    USHORT  wNumberMBsInSlice;
    USHORT  wQuantizerScaleCode;
    USHORT  wBadSliceChopping;
} DXVA_SliceInfo, *LPDXVA_SliceInfo;
#pragma pack(pop)

typedef struct {
    USHORT wFrameWidthInMbsMinus1;
    USHORT wFrameHeightInMbsMinus1;
    DXVA_PicEntry_H264 CurrPic;
    UCHAR  num_ref_frames;
    __C89_NAMELESS union {
        __C89_NAMELESS struct {
            USHORT field_pic_flag           : 1;
            USHORT MbaffFrameFlag           : 1;
            USHORT residual_colour_transform_flag : 1;
            USHORT sp_for_switch_flag       : 1;
            USHORT chroma_format_idc        : 2;
            USHORT RefPicFlag               : 1;
            USHORT constrained_intra_pred_flag : 1;
            USHORT weighted_pred_flag       : 1;
            USHORT weighted_bipred_idc      : 2;
            USHORT MbsConsecutiveFlag       : 1;
            USHORT frame_mbs_only_flag      : 1;
            USHORT transform_8x8_mode_flag  : 1;
            USHORT MinLumaBipredSize8x8Flag : 1;
            USHORT IntraPicFlag             : 1;
        };
        USHORT wBitFields;
    };
    UCHAR   bit_depth_luma_minus8;
    UCHAR   bit_depth_chroma_minus8;
    USHORT  Reserved16Bits;
    UINT    StatusReportFeedbackNumber;
    DXVA_PicEntry_H264 RefFrameList[16];
    INT     CurrFieldOrderCnt[2];
    INT     FieldOrderCntList[16][2];
    CHAR    pic_init_qs_minus26;
    CHAR    chroma_qp_index_offset;
    CHAR    second_chroma_qp_index_offset;
    UCHAR   ContinuationFlag;
    CHAR    pic_init_qp_minus26;
    UCHAR   num_ref_idx_l0_active_minus1;
    UCHAR   num_ref_idx_l1_active_minus1;
    UCHAR   Reserved8BitsA;
    USHORT  FrameNumList[16];
    UINT    UsedForReferenceFlags;
    USHORT  NonExistingFrameFlags;
    USHORT  frame_num;
    UCHAR   log2_max_frame_num_minus4;
    UCHAR   pic_order_cnt_type;
    UCHAR   log2_max_pic_order_cnt_lsb_minus4;
    UCHAR   delta_pic_order_always_zero_flag;
    UCHAR   direct_8x8_inference_flag;
    UCHAR   entropy_coding_mode_flag;
    UCHAR   pic_order_present_flag;
    UCHAR   num_slice_groups_minus1;
    UCHAR   slice_group_map_type;
    UCHAR   deblocking_filter_control_present_flag;
    UCHAR   redundant_pic_cnt_present_flag;
    UCHAR   Reserved8BitsB;
    USHORT  slice_group_change_rate_minus1;
    UCHAR   SliceGroupMap[810];
} DXVA_PicParams_H264;

typedef struct {
    UCHAR   bScalingLists4x4[6][16];
    UCHAR   bScalingLists8x8[2][64];
} DXVA_Qmatrix_H264;

typedef struct {
    UINT    BSNALunitDataLocation;
    UINT    SliceBytesInBuffer;
    USHORT  wBadSliceChopping;
    USHORT  first_mb_in_slice;
    USHORT  NumMbsForSlice;
    USHORT  BitOffsetToSliceData;
    UCHAR   slice_type;
    UCHAR   luma_log2_weight_denom;
    UCHAR   chroma_log2_weight_denom;
    UCHAR   num_ref_idx_l0_active_minus1;
    UCHAR   num_ref_idx_l1_active_minus1;
    CHAR    slice_alpha_c0_offset_div2;
    CHAR    slice_beta_offset_div2;
    UCHAR   Reserved8Bits;
    DXVA_PicEntry_H264 RefPicList[2][32];
    SHORT   Weights[2][32][3][2];
    CHAR    slice_qs_delta;
    CHAR    slice_qp_delta;
    UCHAR   redundant_pic_cnt;
    UCHAR   direct_spatial_mv_pred_flag;
    UCHAR   cabac_init_idc;
    UCHAR   disable_deblocking_filter_idc;
    USHORT  slice_id;
} DXVA_Slice_H264_Long;

#pragma pack(push, 1)
typedef struct {
    UINT    BSNALunitDataLocation;
    UINT    SliceBytesInBuffer;
    USHORT  wBadSliceChopping;
} DXVA_Slice_H264_Short;
#pragma pack(pop)


/* Constants */

#define DXVA2_VideoDecoderRenderTarget 0
#define DXVA2_VideoProcessorRenderTarget 1
#define DXVA2_VideoSoftwareRenderTarget 2

/* CompressedBufferType */
#define DXVA2_PictureParametersBufferType 0
#define DXVA2_MacroBlockControlBufferType 1
#define DXVA2_ResidualDifferenceBufferType 2
#define DXVA2_DeblockingControlBufferType 3
#define DXVA2_InverseQuantizationMatrixBufferType 4
#define DXVA2_SliceControlBufferType 5
#define DXVA2_BitStreamDateBufferType 6
#define DXVA2_MotionVectorBuffer 7
#define DXVA2_FilmGrainBuffer 8

__forceinline const DXVA2_Fixed32 DXVA2_Fixed32OpaqueAlpha (void) {
  DXVA2_Fixed32 f32;
  f32.ll = 0 + (1 << 16);
  return f32;
}

__forceinline const DXVA2_Fixed32 DXVA2_Fixed32TransparentAlpha (void) {
  DXVA2_Fixed32 f32;
  f32.ll = 0;
  return f32;
}

__forceinline float DXVA2FixedToFloat (const DXVA2_Fixed32 f32) {
  return (float)f32.Value + (float)f32.Fraction / (1 << 16);
}

__forceinline DXVA2_Fixed32 DXVA2FloatToFixed (const float f) {
  DXVA2_Fixed32 f32;
  f32.Value    = ((ULONG) (f * (1 << 16))) >> 16;
  f32.Fraction = ((ULONG) (f * (1 << 16))) & 0xFFFF;
  return f32;
}

HRESULT WINAPI DXVA2CreateDirect3DDeviceManager9(UINT *pResetToken,IDirect3DDeviceManager9 **ppDXVAManager);
HRESULT WINAPI DXVA2CreateVideoService(IDirect3DDevice9 *pDD,REFIID riid,void **ppService);

#ifdef __cplusplus
}
#endif

/* COM Interfaces */

#undef  INTERFACE
#define INTERFACE IDirectXVideoDecoder
DECLARE_INTERFACE_(IDirectXVideoDecoder,IUnknown)
{
    BEGIN_INTERFACE

    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /* IDirectXVideoDecoder methods */
    STDMETHOD_(HRESULT,GetVideoDecoderService)(THIS_ IDirectXVideoDecoderService **ppAccelServices) PURE;
    STDMETHOD_(HRESULT,GetCreationParameters)(THIS_ GUID *pDeviceGuid,DXVA2_VideoDesc *pVideoDesc,DXVA2_ConfigPictureDecode *pConfig,IDirect3DSurface9 ***pppDecoderRenderTargets,UINT *pNumSurfaces) PURE;
    STDMETHOD_(HRESULT,GetBuffer)(THIS_ UINT BufferType,void **ppBuffer,UINT *pBufferSize) PURE;
    STDMETHOD_(HRESULT,ReleaseBuffer)(THIS_ UINT BufferType) PURE;
    STDMETHOD_(HRESULT,BeginFrame)(THIS_ IDirect3DSurface9 *pRenderTarget,void *pvPVPData) PURE;
    STDMETHOD_(HRESULT,EndFrame)(THIS_ HANDLE *pHandleComplete) PURE;
    STDMETHOD_(HRESULT,Execute)(THIS_ const DXVA2_DecodeExecuteParams *pExecuteParams) PURE;

    END_INTERFACE
};
#ifdef COBJMACROS
#define IDirectXVideoDecoder_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IDirectXVideoDecoder_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IDirectXVideoDecoder_Release(This) (This)->lpVtbl->Release(This)
#define IDirectXVideoDecoder_GetVideoDecoderService(This,ppAccelServices) (This)->lpVtbl->GetVideoDecoderService(This,ppAccelServices)
#define IDirectXVideoDecoder_GetCreationParameters(This,pDeviceGuid,pVideoDesc,pConfig,pppDecoderRenderTargets,pNumSurfaces) (This)->lpVtbl->GetCreationParameters(This,pDeviceGuid,pVideoDesc,pConfig,pppDecoderRenderTargets,pNumSurfaces)
#define IDirectXVideoDecoder_GetBuffer(This,BufferType,ppBuffer,pBufferSize) (This)->lpVtbl->GetBuffer(This,BufferType,ppBuffer,pBufferSize)
#define IDirectXVideoDecoder_ReleaseBuffer(This,BufferType) (This)->lpVtbl->ReleaseBuffer(This,BufferType)
#define IDirectXVideoDecoder_BeginFrame(This,pRenderTarget,pvPVPData) (This)->lpVtbl->BeginFrame(This,pRenderTarget,pvPVPData)
#define IDirectXVideoDecoder_EndFrame(This,pHandleComplete) (This)->lpVtbl->EndFrame(This,pHandleComplete)
#define IDirectXVideoDecoder_Execute(This,pExecuteParams) (This)->lpVtbl->Execute(This,pExecuteParams)
#endif /*COBJMACROS*/

#undef  INTERFACE
#define INTERFACE IDirectXVideoAccelerationService
DECLARE_INTERFACE_(IDirectXVideoAccelerationService,IUnknown)
{
    BEGIN_INTERFACE

    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /* IDirectXVideoAccelerationService methods */
    STDMETHOD_(HRESULT,CreateSurface)(THIS_ UINT Width,UINT Height,UINT BackBuffers,D3DFORMAT Format,D3DPOOL Pool,DWORD Usage,DWORD DxvaType,IDirect3DSurface9 **ppSurface,HANDLE *pSharedHandle) PURE;

    END_INTERFACE
};
#ifdef COBJMACROS
#define IDirectXVideoAccelerationService_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IDirectXVideoAccelerationService_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IDirectXVideoAccelerationService_Release(This) (This)->lpVtbl->Release(This)
#define IDirectXVideoAccelerationService_CreateSurface(This,Width,Height,BackBuffers,Format,Pool,Usage,DxvaType,ppSurface,pSharedHandle) (This)->lpVtbl->CreateSurface(This,Width,Height,BackBuffers,Format,Pool,Usage,DxvaType,ppSurface,pSharedHandle)
#endif /*COBJMACROS*/

#undef  INTERFACE
#define INTERFACE IDirectXVideoDecoderService
DECLARE_INTERFACE_(IDirectXVideoDecoderService,IDirectXVideoAccelerationService)
{
    BEGIN_INTERFACE

    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /* IDirectXVideoAccelerationService methods */
    STDMETHOD_(HRESULT,CreateSurface)(THIS_ UINT Width,UINT Height,UINT BackBuffers,D3DFORMAT Format,D3DPOOL Pool,DWORD Usage,DWORD DxvaType,IDirect3DSurface9 **ppSurface,HANDLE *pSharedHandle) PURE;
    
    /* IDirectXVideoDecoderService methods */
    STDMETHOD_(HRESULT,GetDecoderDeviceGuids)(THIS_ UINT *Count,GUID **pGuids) PURE;
    STDMETHOD_(HRESULT,GetDecoderRenderTargets)(THIS_ REFGUID Guid,UINT *pCount,D3DFORMAT **pFormats) PURE;
    STDMETHOD_(HRESULT,GetDecoderConfigurations)(THIS_ REFGUID Guid,const DXVA2_VideoDesc *pVideoDesc,IUnknown *pReserved,UINT *pCount,DXVA2_ConfigPictureDecode **ppConfigs) PURE;
    STDMETHOD_(HRESULT,CreateVideoDecoder)(THIS_ REFGUID Guid,const DXVA2_VideoDesc *pVideoDesc,DXVA2_ConfigPictureDecode *pConfig,IDirect3DSurface9 **ppDecoderRenderTargets,UINT NumSurfaces,IDirectXVideoDecoder **ppDecode) PURE;

    END_INTERFACE
};
#ifdef COBJMACROS
#define IDirectXVideoDecoderService_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IDirectXVideoDecoderService_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IDirectXVideoDecoderService_Release(This) (This)->lpVtbl->Release(This)
#define IDirectXVideoDecoderService_CreateSurface(This,Width,Height,BackBuffers,Format,Pool,Usage,DxvaType,ppSurface,pSharedHandle) (This)->lpVtbl->CreateSurface(This,Width,Height,BackBuffers,Format,Pool,Usage,DxvaType,ppSurface,pSharedHandle)
#define IDirectXVideoDecoderService_GetDecoderDeviceGuids(This,Count,pGuids) (This)->lpVtbl->GetDecoderDeviceGuids(This,Count,pGuids)
#define IDirectXVideoDecoderService_GetDecoderRenderTargets(This,Guid,pCount,pFormats) (This)->lpVtbl->GetDecoderRenderTargets(This,Guid,pCount,pFormats)
#define IDirectXVideoDecoderService_GetDecoderConfigurations(This,Guid,pVideoDesc,pReserved,pCount,ppConfigs) (This)->lpVtbl->GetDecoderConfigurations(This,Guid,pVideoDesc,pReserved,pCount,ppConfigs)
#define IDirectXVideoDecoderService_CreateVideoDecoder(This,Guid,pVideoDesc,pConfig,ppDecoderRenderTargets,NumSurfaces,ppDecode) (This)->lpVtbl->CreateVideoDecoder(This,Guid,pVideoDesc,pConfig,ppDecoderRenderTargets,NumSurfaces,ppDecode)
#endif /*COBJMACROS*/

#undef  INTERFACE
#define INTERFACE IDirect3DDeviceManager9
DECLARE_INTERFACE_(IDirect3DDeviceManager9,IUnknown)
{
    BEGIN_INTERFACE

    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /* IDirect3DDeviceManager9 methods */
    STDMETHOD_(HRESULT,ResetDevice)(THIS_ IDirect3DDevice9 *pDevice,UINT resetToken) PURE;
    STDMETHOD_(HRESULT,OpenDeviceHandle)(THIS_ HANDLE *phDevice) PURE;
    STDMETHOD_(HRESULT,CloseDeviceHandle)(THIS_ HANDLE hDevice) PURE;
    STDMETHOD_(HRESULT,TestDevice)(THIS_ HANDLE hDevice) PURE;
    STDMETHOD_(HRESULT,LockDevice)(THIS_ HANDLE hDevice,IDirect3DDevice9 **ppDevice,WINBOOL fBlock) PURE;
    STDMETHOD_(HRESULT,UnlockDevice)(THIS_ HANDLE hDevice,BOOL fSaveState) PURE;
    STDMETHOD_(HRESULT,GetVideoService)(THIS_ HANDLE hDevice,REFIID riid,void **ppService) PURE;

    END_INTERFACE
};
#ifdef COBJMACROS
#define IDirect3DDeviceManager9_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IDirect3DDeviceManager9_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IDirect3DDeviceManager9_Release(This) (This)->lpVtbl->Release(This)
#define IDirect3DDeviceManager9_ResetDevice(This,pDevice,resetToken) (This)->lpVtbl->ResetDevice(This,pDevice,resetToken)
#define IDirect3DDeviceManager9_OpenDeviceHandle(This,phDevice) (This)->lpVtbl->OpenDeviceHandle(This,phDevice)
#define IDirect3DDeviceManager9_CloseDeviceHandle(This,hDevice) (This)->lpVtbl->CloseDeviceHandle(This,hDevice)
#define IDirect3DDeviceManager9_TestDevice(This,hDevice) (This)->lpVtbl->TestDevice(This,hDevice)
#define IDirect3DDeviceManager9_LockDevice(This,hDevice,ppDevice,fBlock) (This)->lpVtbl->LockDevice(This,hDevice,ppDevice,fBlock)
#define IDirect3DDeviceManager9_UnlockDevice(This,hDevice,fSaveState) (This)->lpVtbl->UnlockDevice(This,hDevice,fSaveState)
#define IDirect3DDeviceManager9_GetVideoService(This,hDevice,riid,ppService) (This)->lpVtbl->GetVideoService(This,hDevice,riid,ppService)
#endif /*COBJMACROS*/

#endif /*(_WIN32_WINNT >= 0x0600)*/
#endif /*_INC_DXVA2API*/
