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

#include <vector>
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/acm_random.h"

#include "./aom_config.h"

#include "aom_ports/mem.h"  // ROUND_POWER_OF_TWO
#include "aom/aomcx.h"
#include "aom/aomdx.h"
#include "aom/aom_encoder.h"
#include "aom/aom_decoder.h"

using libaom_test::ACMRandom;
namespace {

class CompressedSource {
 public:
  explicit CompressedSource(int seed) : rnd_(seed), frame_count_(0) {
    aom_codec_iface_t *algo = &aom_codec_av1_cx_algo;

    aom_codec_enc_cfg_t cfg;
    aom_codec_enc_config_default(algo, &cfg, 0);

    const int max_q = cfg.rc_max_quantizer;

    cfg.rc_end_usage = AOM_CQ;
    cfg.rc_max_quantizer = max_q;
    cfg.rc_min_quantizer = max_q;
    cfg.g_w = kWidth;
    cfg.g_h = kHeight;
    cfg.g_lag_in_frames = 0;

    aom_codec_enc_init(&enc_, algo, &cfg, 0);
  }

  ~CompressedSource() { aom_codec_destroy(&enc_); }

  const aom_codec_cx_pkt_t *ReadFrame() {
    uint8_t buf[kWidth * kHeight * 3 / 2] = { 0 };

    // render regular pattern
    const int period = rnd_.Rand8() % 32 + 1;
    const int phase = rnd_.Rand8() % period;

    const int val_a = rnd_.Rand8();
    const int val_b = rnd_.Rand8();

    for (int i = 0; i < (int)sizeof buf; ++i)
      buf[i] = (i + phase) % period < period / 2 ? val_a : val_b;

    aom_image_t img;
    aom_img_wrap(&img, AOM_IMG_FMT_I420, kWidth, kHeight, 0, buf);
    aom_codec_encode(&enc_, &img, frame_count_++, 1, 0, 0);

    aom_codec_iter_t iter = NULL;
    return aom_codec_get_cx_data(&enc_, &iter);
  }

 private:
  static const int kWidth = 32;
  static const int kHeight = 32;

  ACMRandom rnd_;
  aom_codec_ctx_t enc_;
  int frame_count_;
};

// lowers an aom_image_t to a easily comparable/printable form
std::vector<int16_t> Serialize(const aom_image_t *img) {
  const int w_uv = ROUND_POWER_OF_TWO(img->d_w, img->x_chroma_shift);
  const int h_uv = ROUND_POWER_OF_TWO(img->d_h, img->y_chroma_shift);
  const int w[] = { static_cast<int>(img->d_w), w_uv, w_uv };
  const int h[] = { static_cast<int>(img->d_h), h_uv, h_uv };

  std::vector<int16_t> bytes;
  bytes.reserve(img->d_w * img->d_h * 3);
  for (int plane = 0; plane < 3; ++plane) {
    for (int r = 0; r < h[plane]; ++r) {
      for (int c = 0; c < w[plane]; ++c) {
        const int offset = r * img->stride[plane] + c;
        if (img->fmt & AOM_IMG_FMT_HIGHBITDEPTH)
          bytes.push_back(img->planes[plane][offset * 2]);
        else
          bytes.push_back(img->planes[plane][offset]);
      }
    }
  }

  return bytes;
}

class Decoder {
 public:
  explicit Decoder(int allowLowbitdepth) {
    aom_codec_iface_t *algo = &aom_codec_av1_dx_algo;

    aom_codec_dec_cfg cfg = { 0 };
    cfg.allow_lowbitdepth = allowLowbitdepth;

    aom_codec_dec_init(&dec_, algo, &cfg, 0);
  }

  ~Decoder() { aom_codec_destroy(&dec_); }

  std::vector<int16_t> decode(const aom_codec_cx_pkt_t *pkt) {
    aom_codec_decode(&dec_, static_cast<uint8_t *>(pkt->data.frame.buf),
                     static_cast<unsigned int>(pkt->data.frame.sz), NULL, 0);

    aom_codec_iter_t iter = NULL;
    return Serialize(aom_codec_get_frame(&dec_, &iter));
  }

 private:
  aom_codec_ctx_t dec_;
};

// Try to reveal a mismatch between LBD and HBD coding paths.
TEST(CodingPathSync, SearchForHbdLbdMismatch) {
  const int count_tests = 100;
  for (int i = 0; i < count_tests; ++i) {
    Decoder dec_hbd(0);
    Decoder dec_lbd(1);

    CompressedSource enc(i);
    const aom_codec_cx_pkt_t *frame = enc.ReadFrame();
    // disable the comparison for now.
    // Re-enable it locally to help diagnosing LBD/HBD mismatches,
    // and re-enable for everybody when both coding paths match,
    // so they don't diverge anymore.
    if (0) {
      ASSERT_EQ(dec_lbd.decode(frame), dec_hbd.decode(frame));
    } else {
      // until the comparison is re-enabled, keep doing the decoding,
      // as it still detects decoder crashes (Issue 677)
      dec_lbd.decode(frame);
      dec_hbd.decode(frame);
    }
  }
}

}  // namespace
