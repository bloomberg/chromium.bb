/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/y4m_video_source.h"

#include "aom/aom_decoder.h"
#include "av1/decoder/decoder.h"

namespace {

const int kMaxPsnr = 100;

struct ParamPassingTestVideo {
  const char *name;
  uint32_t width;
  uint32_t height;
  uint32_t bitrate;
  int frames;
};

const ParamPassingTestVideo kAV1ParamPassingTestVector = {
  "niklas_1280_720_30.y4m", 1280, 720, 600, 3
};

struct EncodeParameters {
  int32_t lossless;
  int32_t render_size[2];
};

const EncodeParameters kAV1EncodeParameterSet[] = {
  { 1, { 0, 0 } },
  { 0, { 0, 0 } },
  { 1, { 0, 0 } },
  { 0, { 640, 480 } },
};

class AVxEncoderParmsGetToDecoder
    : public ::libaom_test::EncoderTest,
      public ::libaom_test::CodecTestWithParam<EncodeParameters> {
 protected:
  AVxEncoderParmsGetToDecoder()
      : EncoderTest(GET_PARAM(0)), encode_parms(GET_PARAM(1)) {}

  virtual ~AVxEncoderParmsGetToDecoder() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(::libaom_test::kTwoPassGood);
    cfg_.g_lag_in_frames = 25;
    test_video_ = kAV1ParamPassingTestVector;
    cfg_.rc_target_bitrate = test_video_.bitrate;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 1) {
      encoder->Control(AV1E_SET_LOSSLESS, encode_parms.lossless);
      if (encode_parms.render_size[0] > 0 && encode_parms.render_size[1] > 0) {
        encoder->Control(AV1E_SET_RENDER_SIZE, encode_parms.render_size);
      }
    }
  }

  virtual void DecompressedFrameHook(const aom_image_t &img,
                                     aom_codec_pts_t pts) {
    (void)pts;
    if (encode_parms.render_size[0] > 0 && encode_parms.render_size[1] > 0) {
      EXPECT_EQ(encode_parms.render_size[0], (int)img.r_w);
      EXPECT_EQ(encode_parms.render_size[1], (int)img.r_h);
    }
  }

  virtual void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) {
    if (encode_parms.lossless) {
      EXPECT_EQ(kMaxPsnr, pkt->data.psnr.psnr[0]);
    }
  }

  virtual bool HandleDecodeResult(const aom_codec_err_t res_dec,
                                  libaom_test::Decoder *decoder) {
    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    return AOM_CODEC_OK == res_dec;
  }

  ParamPassingTestVideo test_video_;

 private:
  EncodeParameters encode_parms;
};

TEST_P(AVxEncoderParmsGetToDecoder, BitstreamParms) {
  init_flags_ = AOM_CODEC_USE_PSNR;

  testing::internal::scoped_ptr<libaom_test::VideoSource> video(
      new libaom_test::Y4mVideoSource(test_video_.name, 0, test_video_.frames));
  ASSERT_TRUE(video.get() != NULL);

  ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
}

AV1_INSTANTIATE_TEST_CASE(AVxEncoderParmsGetToDecoder,
                          ::testing::ValuesIn(kAV1EncodeParameterSet));
}  // namespace
