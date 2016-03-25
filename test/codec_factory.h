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
#ifndef TEST_CODEC_FACTORY_H_
#define TEST_CODEC_FACTORY_H_

#include "./aom_config.h"
#include "aom/aom_decoder.h"
#include "aom/aom_encoder.h"
#if CONFIG_VP10_ENCODER
#include "aom/vp8cx.h"
#endif
#if CONFIG_VP10_DECODER
#include "aom/vp8dx.h"
#endif

#include "test/decode_test_driver.h"
#include "test/encode_test_driver.h"
namespace libaom_test {

const int kCodecFactoryParam = 0;

class CodecFactory {
 public:
  CodecFactory() {}

  virtual ~CodecFactory() {}

  virtual Decoder* CreateDecoder(aom_codec_dec_cfg_t cfg,
                                 unsigned long deadline) const = 0;

  virtual Decoder* CreateDecoder(aom_codec_dec_cfg_t cfg,
                                 const aom_codec_flags_t flags,
                                 unsigned long deadline)  // NOLINT(runtime/int)
      const = 0;

  virtual Encoder* CreateEncoder(aom_codec_enc_cfg_t cfg,
                                 unsigned long deadline,
                                 const unsigned long init_flags,
                                 TwopassStatsStore* stats) const = 0;

  virtual aom_codec_err_t DefaultEncoderConfig(aom_codec_enc_cfg_t* cfg,
                                               int usage) const = 0;
};

/* Provide CodecTestWith<n>Params classes for a variable number of parameters
 * to avoid having to include a pointer to the CodecFactory in every test
 * definition.
 */
template <class T1>
class CodecTestWithParam
    : public ::testing::TestWithParam<
          std::tr1::tuple<const libaom_test::CodecFactory*, T1> > {};

template <class T1, class T2>
class CodecTestWith2Params
    : public ::testing::TestWithParam<
          std::tr1::tuple<const libaom_test::CodecFactory*, T1, T2> > {};

template <class T1, class T2, class T3>
class CodecTestWith3Params
    : public ::testing::TestWithParam<
          std::tr1::tuple<const libaom_test::CodecFactory*, T1, T2, T3> > {};

/*
 * VP10 Codec Definitions
 */
#if CONFIG_VP10
class VP10Decoder : public Decoder {
 public:
  VP10Decoder(aom_codec_dec_cfg_t cfg, unsigned long deadline)
      : Decoder(cfg, deadline) {}

  VP10Decoder(aom_codec_dec_cfg_t cfg, const aom_codec_flags_t flag,
              unsigned long deadline)  // NOLINT
      : Decoder(cfg, flag, deadline) {}

 protected:
  virtual aom_codec_iface_t* CodecInterface() const {
#if CONFIG_VP10_DECODER
    return &aom_codec_vp10_dx_algo;
#else
    return NULL;
#endif
  }
};

class VP10Encoder : public Encoder {
 public:
  VP10Encoder(aom_codec_enc_cfg_t cfg, unsigned long deadline,
              const unsigned long init_flags, TwopassStatsStore* stats)
      : Encoder(cfg, deadline, init_flags, stats) {}

 protected:
  virtual aom_codec_iface_t* CodecInterface() const {
#if CONFIG_VP10_ENCODER
    return &aom_codec_vp10_cx_algo;
#else
    return NULL;
#endif
  }
};

class VP10CodecFactory : public CodecFactory {
 public:
  VP10CodecFactory() : CodecFactory() {}

  virtual Decoder* CreateDecoder(aom_codec_dec_cfg_t cfg,
                                 unsigned long deadline) const {
    return CreateDecoder(cfg, 0, deadline);
  }

  virtual Decoder* CreateDecoder(aom_codec_dec_cfg_t cfg,
                                 const aom_codec_flags_t flags,
                                 unsigned long deadline) const {  // NOLINT
#if CONFIG_VP10_DECODER
    return new VP10Decoder(cfg, flags, deadline);
#else
    return NULL;
#endif
  }

  virtual Encoder* CreateEncoder(aom_codec_enc_cfg_t cfg,
                                 unsigned long deadline,
                                 const unsigned long init_flags,
                                 TwopassStatsStore* stats) const {
#if CONFIG_VP10_ENCODER
    return new VP10Encoder(cfg, deadline, init_flags, stats);
#else
    return NULL;
#endif
  }

  virtual aom_codec_err_t DefaultEncoderConfig(aom_codec_enc_cfg_t* cfg,
                                               int usage) const {
#if CONFIG_VP10_ENCODER
    return aom_codec_enc_config_default(&aom_codec_vp10_cx_algo, cfg, usage);
#else
    return VPX_CODEC_INCAPABLE;
#endif
  }
};

const libaom_test::VP10CodecFactory kVP10;

#define VP10_INSTANTIATE_TEST_CASE(test, ...)                              \
  INSTANTIATE_TEST_CASE_P(                                                 \
      VP10, test,                                                          \
      ::testing::Combine(                                                  \
          ::testing::Values(static_cast<const libaom_test::CodecFactory*>( \
              &libaom_test::kVP10)),                                       \
          __VA_ARGS__))
#else
#define VP10_INSTANTIATE_TEST_CASE(test, ...)
#endif  // CONFIG_VP10

}  // namespace libaom_test
#endif  // TEST_CODEC_FACTORY_H_
