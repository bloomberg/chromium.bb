// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_transform.h"

#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/transform.h"
#include "third_party/qcms/src/qcms.h"

#ifndef THIS_MUST_BE_INCLUDED_AFTER_QCMS_H
extern "C" {
#include "third_party/qcms/src/chain.h"
};
#endif

namespace gfx {

Transform Invert(const Transform& t) {
  Transform ret = t;
  if (!t.GetInverse(&ret)) {
    LOG(ERROR) << "Inverse should alsways be possible.";
  }
  return ret;
}

ColorTransform::TriStim Map(const Transform& t, ColorTransform::TriStim color) {
  t.TransformPoint(&color);
  return color;
}

ColorTransform::TriStim Xy2xyz(float x, float y) {
  return ColorTransform::TriStim(x, y, 1.0f - x - y);
}

void GetPrimaries(ColorSpace::PrimaryID id,
                  ColorTransform::TriStim primaries[4]) {
  switch (id) {
    case ColorSpace::PrimaryID::CUSTOM:
      NOTREACHED();

    case ColorSpace::PrimaryID::RESERVED0:
    case ColorSpace::PrimaryID::RESERVED:
    case ColorSpace::PrimaryID::UNSPECIFIED:
    case ColorSpace::PrimaryID::UNKNOWN:
    case ColorSpace::PrimaryID::BT709:
      // BT709 is our default case. Put it after the switch just
      // in case we somehow get an id which is not listed in the switch.
      // (We don't want to use "default", because we want the compiler
      //  to tell us if we forgot some enum values.)
      break;

    case ColorSpace::PrimaryID::BT470M:
      // Red
      primaries[0] = Xy2xyz(0.67f, 0.33f);
      // Green
      primaries[1] = Xy2xyz(0.21f, 0.71f);
      // Blue
      primaries[2] = Xy2xyz(0.14f, 0.08f);
      // Whitepoint
      primaries[3] = Xy2xyz(0.31f, 0.316f);
      return;

    case ColorSpace::PrimaryID::BT470BG:
      // Red
      primaries[0] = Xy2xyz(0.64f, 0.33f);
      // Green
      primaries[1] = Xy2xyz(0.29f, 0.60f);
      // Blue
      primaries[2] = Xy2xyz(0.15f, 0.06f);
      // Whitepoint (D65f)
      primaries[3] = Xy2xyz(0.3127f, 0.3290f);
      return;

    case ColorSpace::PrimaryID::SMPTE170M:
    case ColorSpace::PrimaryID::SMPTE240M:
      // Red
      primaries[0] = Xy2xyz(0.630f, 0.340f);
      // Green
      primaries[1] = Xy2xyz(0.310f, 0.595f);
      // Blue
      primaries[2] = Xy2xyz(0.155f, 0.070f);
      // Whitepoint (D65f)
      primaries[3] = Xy2xyz(0.3127f, 0.3290f);
      return;

    case ColorSpace::PrimaryID::FILM:
      // Red
      primaries[0] = Xy2xyz(0.681f, 0.319f);
      // Green
      primaries[1] = Xy2xyz(0.243f, 0.692f);
      // Blue
      primaries[2] = Xy2xyz(0.145f, 0.049f);
      // Whitepoint (Cf)
      primaries[3] = Xy2xyz(0.310f, 0.136f);
      return;

    case ColorSpace::PrimaryID::BT2020:
      // Red
      primaries[0] = Xy2xyz(0.708f, 0.292f);
      // Green
      primaries[1] = Xy2xyz(0.170f, 0.797f);
      // Blue
      primaries[2] = Xy2xyz(0.131f, 0.046f);
      // Whitepoint (D65f)
      primaries[3] = Xy2xyz(0.3127f, 0.3290f);
      return;

    case ColorSpace::PrimaryID::SMPTEST428_1:
      // X
      primaries[0] = Xy2xyz(1.0f, 0.0f);
      // Y
      primaries[1] = Xy2xyz(0.0f, 1.0f);
      // Z
      primaries[2] = Xy2xyz(0.0f, 0.0f);
      // Whitepoint (Ef)
      primaries[3] = Xy2xyz(1.0f / 3.0f, 1.0f / 3.0f);
      return;

    case ColorSpace::PrimaryID::SMPTEST431_2:
      // Red
      primaries[0] = Xy2xyz(0.680f, 0.320f);
      // Green
      primaries[1] = Xy2xyz(0.265f, 0.690f);
      // Blue
      primaries[2] = Xy2xyz(0.150f, 0.060f);
      // Whitepoint
      primaries[3] = Xy2xyz(0.314f, 0.351f);
      return;

    case ColorSpace::PrimaryID::SMPTEST432_1:
      // Red
      primaries[0] = Xy2xyz(0.680f, 0.320f);
      // Green
      primaries[1] = Xy2xyz(0.265f, 0.690f);
      // Blue
      primaries[2] = Xy2xyz(0.150f, 0.060f);
      // Whitepoint (D65f)
      primaries[3] = Xy2xyz(0.3127f, 0.3290f);
      return;

    case ColorSpace::PrimaryID::XYZ_D50:
      // X
      primaries[0] = Xy2xyz(1.0f, 0.0f);
      // Y
      primaries[1] = Xy2xyz(0.0f, 1.0f);
      // Z
      primaries[2] = Xy2xyz(0.0f, 0.0f);
      // D50
      primaries[3] = Xy2xyz(0.34567f, 0.35850f);
      return;
  }

  // Red
  primaries[0] = Xy2xyz(0.640f, 0.330f);
  // Green
  primaries[1] = Xy2xyz(0.300f, 0.600f);
  // Blue
  primaries[2] = Xy2xyz(0.150f, 0.060f);
  // Whitepoint (D65f)
  primaries[3] = Xy2xyz(0.3127f, 0.3290f);
}

GFX_EXPORT Transform GetPrimaryMatrix(ColorSpace::PrimaryID id) {
  ColorTransform::TriStim primaries[4];
  GetPrimaries(id, primaries);
  ColorTransform::TriStim WXYZ(primaries[3].x() / primaries[3].y(), 1.0f,
                               primaries[3].z() / primaries[3].y());

  Transform ret(
      primaries[0].x(), primaries[1].x(), primaries[2].x(), 0.0f,  // 1
      primaries[0].y(), primaries[1].y(), primaries[2].y(), 0.0f,  // 2
      primaries[0].z(), primaries[1].z(), primaries[2].z(), 0.0f,  // 3
      0.0f, 0.0f, 0.0f, 1.0f);                                     // 4

  ColorTransform::TriStim conv = Map(Invert(ret), WXYZ);
  ret.Scale3d(conv.x(), conv.y(), conv.z());

  // Chromatic adaptation.
  Transform bradford(0.8951000f, 0.2664000f, -0.1614000f, 0.0f,  // 1
                     -0.7502000f, 1.7135000f, 0.0367000f, 0.0f,  // 2
                     0.0389000f, -0.0685000f, 1.0296000f, 0.0f,  // 3
                     0.0f, 0.0f, 0.0f, 1.0f);                    // 4

  ColorTransform::TriStim D50(0.9642f, 1.0f, 0.8249f);
  ColorTransform::TriStim source_response = Map(bradford, WXYZ);
  ColorTransform::TriStim dest_response = Map(bradford, D50);

  Transform adapter;
  adapter.Scale3d(dest_response.x() / source_response.x(),
                  dest_response.y() / source_response.y(),
                  dest_response.z() / source_response.z());

  return Invert(bradford) * adapter * bradford * ret;
}

GFX_EXPORT float FromLinear(ColorSpace::TransferID id, float v) {
  switch (id) {
    case ColorSpace::TransferID::SMPTEST2084_NON_HDR:
      // Should already be handled.
      NOTREACHED();
    case ColorSpace::TransferID::CUSTOM:
    // TODO(hubbe): Actually implement custom transfer functions.
    case ColorSpace::TransferID::RESERVED0:
    case ColorSpace::TransferID::RESERVED:
    case ColorSpace::TransferID::UNSPECIFIED:
    case ColorSpace::TransferID::UNKNOWN:
    // All unknown values default to BT709

    case ColorSpace::TransferID::BT709:
    case ColorSpace::TransferID::SMPTE170M:
    case ColorSpace::TransferID::BT2020_10:
    case ColorSpace::TransferID::BT2020_12:
      // BT709 is our "default" cause, so put the code after the switch
      // to avoid "control reaches end of non-void function" errors.
      break;

    case ColorSpace::TransferID::GAMMA22:
      v = fmax(0.0f, v);
      return powf(v, 1.0f / 2.2f);

    case ColorSpace::TransferID::GAMMA28:
      v = fmax(0.0f, v);
      return powf(v, 1.0f / 2.8f);

    case ColorSpace::TransferID::SMPTE240M: {
      v = fmax(0.0f, v);
      float a = 1.11157219592173128753f;
      float b = 0.02282158552944503135f;
      if (v <= b) {
        return 4.0f * v;
      } else {
        return a * powf(v, 0.45f) - (a - 1.0f);
      }
    }

    case ColorSpace::TransferID::LINEAR:
      return v;

    case ColorSpace::TransferID::LOG:
      if (v < 0.01f)
        return 0.0f;
      return 1.0f + log(v) / log(10.0f) / 2.0f;

    case ColorSpace::TransferID::LOG_SQRT:
      if (v < sqrt(10.0f) / 1000.0f)
        return 0.0f;
      return 1.0f + log(v) / log(10.0f) / 2.5f;

    case ColorSpace::TransferID::IEC61966_2_4: {
      float a = 1.099296826809442f;
      float b = 0.018053968510807f;
      if (v < -b) {
        return -a * powf(-v, 0.45f) + (a - 1.0f);
      } else if (v <= b) {
        return 4.5f * v;
      } else {
        return a * powf(v, 0.45f) - (a - 1.0f);
      }
    }

    case ColorSpace::TransferID::BT1361_ECG: {
      float a = 1.099f;
      float b = 0.018f;
      float l = 0.0045f;
      if (v < -l) {
        return -(a * powf(-4.0f * v, 0.45f) + (a - 1.0f)) / 4.0f;
      } else if (v <= b) {
        return 4.5f * v;
      } else {
        return a * powf(v, 0.45f) - (a - 1.0f);
      }
    }

    case ColorSpace::TransferID::IEC61966_2_1: {  // SRGB
      v = fmax(0.0f, v);
      float a = 1.055f;
      float b = 0.0031308f;
      if (v < b) {
        return 12.92f * v;
      } else {
        return a * powf(v, 1.0f / 2.4f) - (a - 1.0f);
      }
    }
    case ColorSpace::TransferID::SMPTEST2084: {
      v = fmax(0.0f, v);
      float m1 = (2610.0f / 4096.0f) / 4.0f;
      float m2 = (2523.0f / 4096.0f) * 128.0f;
      float c1 = 3424.0f / 4096.0f;
      float c2 = (2413.0f / 4096.0f) * 32.0f;
      float c3 = (2392.0f / 4096.0f) * 32.0f;
      return powf((c1 + c2 * powf(v, m1)) / (1.0f + c3 * powf(v, m1)), m2);
    }

    case ColorSpace::TransferID::SMPTEST428_1:
      v = fmax(0.0f, v);
      return powf(48.0f * v + 52.37f, 1.0f / 2.6f);

    // Spec: http://www.arib.or.jp/english/html/overview/doc/2-STD-B67v1_0.pdf
    case ColorSpace::TransferID::ARIB_STD_B67: {
      const float a = 0.17883277f;
      const float b = 0.28466892f;
      const float c = 0.55991073f;
      const float Lmax = 12.0f;
      v = Lmax * fmax(0.0f, v);
      if (v <= 1)
        return 0.5f * sqrtf(v);
      else
        return a * log(v - b) + c;
    }

    // Chrome-specific values below
    case ColorSpace::TransferID::GAMMA24:
      v = fmax(0.0f, v);
      return powf(v, 1.0f / 2.4f);
  }

  v = fmax(0.0f, v);
  float a = 1.099296826809442f;
  float b = 0.018053968510807f;
  if (v <= b) {
    return 4.5f * v;
  } else {
    return a * powf(v, 0.45f) - (a - 1.0f);
  }
}

GFX_EXPORT float ToLinear(ColorSpace::TransferID id, float v) {
  switch (id) {
    case ColorSpace::TransferID::CUSTOM:
    // TODO(hubbe): Actually implement custom transfer functions.
    case ColorSpace::TransferID::RESERVED0:
    case ColorSpace::TransferID::RESERVED:
    case ColorSpace::TransferID::UNSPECIFIED:
    case ColorSpace::TransferID::UNKNOWN:
    // All unknown values default to BT709

    case ColorSpace::TransferID::BT709:
    case ColorSpace::TransferID::SMPTE170M:
    case ColorSpace::TransferID::BT2020_10:
    case ColorSpace::TransferID::BT2020_12:
      // BT709 is our "default" cause, so put the code after the switch
      // to avoid "control reaches end of non-void function" errors.
      break;

    case ColorSpace::TransferID::GAMMA22:
      v = fmax(0.0f, v);
      return powf(v, 2.2f);

    case ColorSpace::TransferID::GAMMA28:
      v = fmax(0.0f, v);
      return powf(v, 2.8f);

    case ColorSpace::TransferID::SMPTE240M: {
      v = fmax(0.0f, v);
      float a = 1.11157219592173128753f;
      float b = 0.02282158552944503135f;
      if (v <= FromLinear(ColorSpace::TransferID::SMPTE240M, b)) {
        return v / 4.0f;
      } else {
        return powf((v + a - 1.0f) / a, 1.0f / 0.45f);
      }
    }

    case ColorSpace::TransferID::LINEAR:
      return v;

    case ColorSpace::TransferID::LOG:
      if (v < 0.0f)
        return 0.0f;
      return powf(10.0f, (v - 1.0f) * 2.0f);

    case ColorSpace::TransferID::LOG_SQRT:
      if (v < 0.0f)
        return 0.0f;
      return powf(10.0f, (v - 1.0f) * 2.5f);

    case ColorSpace::TransferID::IEC61966_2_4: {
      float a = 1.099296826809442f;
      float b = 0.018053968510807f;
      if (v < FromLinear(ColorSpace::TransferID::IEC61966_2_4, -a)) {
        return -powf((a - 1.0f - v) / a, 1.0f / 0.45f);
      } else if (v <= FromLinear(ColorSpace::TransferID::IEC61966_2_4, b)) {
        return v / 4.5f;
      } else {
        return powf((v + a - 1.0f) / a, 1.0f / 0.45f);
      }
    }

    case ColorSpace::TransferID::BT1361_ECG: {
      float a = 1.099f;
      float b = 0.018f;
      float l = 0.0045f;
      if (v < FromLinear(ColorSpace::TransferID::BT1361_ECG, -l)) {
        return -powf((1.0f - a - v * 4.0f) / a, 1.0f / 0.45f) / 4.0f;
      } else if (v <= FromLinear(ColorSpace::TransferID::BT1361_ECG, b)) {
        return v / 4.5f;
      } else {
        return powf((v + a - 1.0f) / a, 1.0f / 0.45f);
      }
    }

    case ColorSpace::TransferID::IEC61966_2_1: {  // SRGB
      v = fmax(0.0f, v);
      float a = 1.055f;
      float b = 0.0031308f;
      if (v < FromLinear(ColorSpace::TransferID::IEC61966_2_1, b)) {
        return v / 12.92f;
      } else {
        return powf((v + a - 1.0f) / a, 2.4f);
      }
    }

    case ColorSpace::TransferID::SMPTEST2084: {
      v = fmax(0.0f, v);
      float m1 = (2610.0f / 4096.0f) / 4.0f;
      float m2 = (2523.0f / 4096.0f) * 128.0f;
      float c1 = 3424.0f / 4096.0f;
      float c2 = (2413.0f / 4096.0f) * 32.0f;
      float c3 = (2392.0f / 4096.0f) * 32.0f;
      return powf(
          fmax(powf(v, 1.0f / m2) - c1, 0) / (c2 - c3 * powf(v, 1.0f / m2)),
          1.0f / m1);
    }

    case ColorSpace::TransferID::SMPTEST428_1:
      return (powf(v, 2.6f) - 52.37f) / 48.0f;

    // Chrome-specific values below
    case ColorSpace::TransferID::GAMMA24:
      v = fmax(0.0f, v);
      return powf(v, 2.4f);

    case ColorSpace::TransferID::SMPTEST2084_NON_HDR:
      v = fmax(0.0f, v);
      return fmin(2.3f * pow(v, 2.8f), v / 5.0f + 0.8f);

    // Spec: http://www.arib.or.jp/english/html/overview/doc/2-STD-B67v1_0.pdf
    case ColorSpace::TransferID::ARIB_STD_B67: {
      v = fmax(0.0f, v);
      const float a = 0.17883277f;
      const float b = 0.28466892f;
      const float c = 0.55991073f;
      const float Lmax = 12.0f;
      float v_ = 0.0f;
      if (v <= 0.5f) {
        v_ = (v * 2.0f) * (v * 2.0f);
      } else {
        v_ = exp((v - c) / a) + b;
      }
      return v_ / Lmax;
    }
  }

  v = fmax(0.0f, v);
  float a = 1.099296826809442f;
  float b = 0.018053968510807f;
  if (v < FromLinear(ColorSpace::TransferID::BT709, b)) {
    return v / 4.5f;
  } else {
    return powf((v + a - 1.0f) / a, 1.0f / 0.45f);
  }
}

GFX_EXPORT Transform GetTransferMatrix(ColorSpace::MatrixID id) {
  // Default values for BT709;
  float Kr = 0.2126f;
  float Kb = 0.0722f;
  switch (id) {
    case ColorSpace::MatrixID::RGB:
      return Transform();

    case ColorSpace::MatrixID::BT709:
    case ColorSpace::MatrixID::UNSPECIFIED:
    case ColorSpace::MatrixID::RESERVED:
    case ColorSpace::MatrixID::UNKNOWN:
      // Default values are already set.
      break;

    case ColorSpace::MatrixID::FCC:
      Kr = 0.30f;
      Kb = 0.11f;
      break;

    case ColorSpace::MatrixID::BT470BG:
    case ColorSpace::MatrixID::SMPTE170M:
      Kr = 0.299f;
      Kb = 0.144f;
      break;

    case ColorSpace::MatrixID::SMPTE240M:
      Kr = 0.212f;
      Kb = 0.087f;
      break;

    case ColorSpace::MatrixID::YCOCG:
      return Transform(0.25f, 0.5f, 0.25f, 0.5f,    // 1
                       -0.25f, 0.5f, -0.25f, 0.5f,  // 2
                       0.5f, 0.0f, -0.5f, 0.0f,     // 3
                       0.0f, 0.0f, 0.0f, 1.0f);     // 4

    case ColorSpace::MatrixID::BT2020_NCL:
      Kr = 0.2627f;
      Kb = 0.0593f;
      break;

    // BT2020_CL is a special case.
    // Basically we return a matrix that transforms RGB values
    // to RYB values. (We replace the green component with the
    // the luminance.) Later steps will compute the Cb & Cr values.
    case ColorSpace::MatrixID::BT2020_CL:
      Kr = 0.2627f;
      Kb = 0.0593f;
      return Transform(1.0f, 0.0f, 0.0f, 0.0f,        // R
                       Kr, 1.0f - Kr - Kb, Kb, 0.0f,  // Y
                       0.0f, 0.0f, 1.0f, 0.0f,        // B
                       0.0f, 0.0f, 0.0f, 1.0f);

    case ColorSpace::MatrixID::YDZDX:
      return Transform(0.0f, 1.0f, 0.0f, 0.0f,               // 1
                       0.0f, -0.5f, 0.986566f / 2.0f, 0.5f,  // 2
                       0.5f, -0.991902f / 2.0f, 0.0f, 0.5f,  // 3
                       0.0f, 0.0f, 0.0f, 1.0f);              // 4
  }
  float u_m = 0.5f / (1.0f - Kb);
  float v_m = 0.5f / (1.0f - Kr);
  return Transform(
      Kr, 1.0f - Kr - Kb, Kb, 0.0f,                                 // 1
      u_m * -Kr, u_m * -(1.0f - Kr - Kb), u_m * (1.0f - Kb), 0.5f,  // 2
      v_m * (1.0f - Kr), v_m * -(1.0f - Kr - Kb), v_m * -Kb, 0.5f,  // 3
      0.0f, 0.0f, 0.0f, 1.0f);                                      // 4
}

Transform GetRangeAdjustMatrix(ColorSpace::RangeID range,
                               ColorSpace::MatrixID matrix) {
  switch (range) {
    case ColorSpace::RangeID::FULL:
    case ColorSpace::RangeID::UNSPECIFIED:
      return Transform();

    case ColorSpace::RangeID::DERIVED:
    case ColorSpace::RangeID::LIMITED:
      break;
  }
  switch (matrix) {
    case ColorSpace::MatrixID::RGB:
    case ColorSpace::MatrixID::YCOCG:
      // TODO(hubbe): Use Transform:Scale3d / Transform::Translate3d here.
      return Transform(255.0f / 219.0f, 0.0f, 0.0f, -16.0f / 219.0f,  // 1
                       0.0f, 255.0f / 219.0f, 0.0f, -16.0f / 219.0f,  // 2
                       0.0f, 0.0f, 255.0f / 219.0f, -16.0f / 219.0f,  // 3
                       0.0f, 0.0f, 0.0f, 1.0f);                       // 4

    case ColorSpace::MatrixID::BT709:
    case ColorSpace::MatrixID::UNSPECIFIED:
    case ColorSpace::MatrixID::RESERVED:
    case ColorSpace::MatrixID::FCC:
    case ColorSpace::MatrixID::BT470BG:
    case ColorSpace::MatrixID::SMPTE170M:
    case ColorSpace::MatrixID::SMPTE240M:
    case ColorSpace::MatrixID::BT2020_NCL:
    case ColorSpace::MatrixID::BT2020_CL:
    case ColorSpace::MatrixID::YDZDX:
    case ColorSpace::MatrixID::UNKNOWN:
      // TODO(hubbe): Use Transform:Scale3d / Transform::Translate3d here.
      return Transform(255.0f / 219.0f, 0.0f, 0.0f, -16.0f / 219.0f,  // 1
                       0.0f, 255.0f / 224.0f, 0.0f, -15.5f / 224.0f,  // 2
                       0.0f, 0.0f, 255.0f / 224.0f, -15.5f / 224.0f,  // 3
                       0.0f, 0.0f, 0.0f, 1.0f);                       // 4
  }
  NOTREACHED();
  return Transform();
}

class ColorTransformMatrix;
class ColorTransformToLinear;
class ColorTransformFromLinear;
class ColorTransformToBT2020CL;
class ColorTransformFromBT2020CL;
class ColorTransformNull;

class ColorTransformInternal : public ColorTransform {
 public:
  // Visitor pattern, Prepend() calls return prev->Join(this).
  virtual bool Prepend(ColorTransformInternal* prev) = 0;

  // Join methods, returns true if the |next| transform was successfully
  // assimilated into |this|.
  // If Join() returns true, |next| is no longer needed and can be deleted.
  virtual bool Join(const ColorTransformToLinear& next) { return false; }
  virtual bool Join(const ColorTransformFromLinear& next) { return false; }
  virtual bool Join(const ColorTransformToBT2020CL& next) { return false; }
  virtual bool Join(const ColorTransformFromBT2020CL& next) { return false; }
  virtual bool Join(const ColorTransformMatrix& next) { return false; }
  virtual bool Join(const ColorTransformNull& next) { return true; }

  // Return true if this is a null transform.
  virtual bool IsNull() { return false; }
};

class ColorTransformNull : public ColorTransformInternal {
 public:
  bool Prepend(ColorTransformInternal* prev) override {
    return prev->Join(*this);
  }
  bool IsNull() override { return true; }
  void transform(ColorTransform::TriStim* color, size_t num) override {}
};

class ColorTransformMatrix : public ColorTransformInternal {
 public:
  explicit ColorTransformMatrix(const Transform& matrix) : matrix_(matrix) {}

  bool Prepend(ColorTransformInternal* prev) override {
    return prev->Join(*this);
  }

  bool Join(const ColorTransformMatrix& next) override {
    Transform tmp = next.matrix_;
    tmp *= matrix_;
    matrix_ = tmp;
    return true;
  }

  bool IsNull() override {
    // Returns true if we're very close to an identity matrix.
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        float expected = i == j ? 1.0f : 0.0f;
        if (fabs(matrix_.matrix().get(i, j) - expected) > 0.00001f) {
          return false;
        }
      }
    }
    return true;
  }

  void transform(ColorTransform::TriStim* colors, size_t num) override {
    for (size_t i = 0; i < num; i++)
      matrix_.TransformPoint(colors + i);
  }

 private:
  Transform matrix_;
};

class ColorTransformFromLinear : public ColorTransformInternal {
 public:
  explicit ColorTransformFromLinear(ColorSpace::TransferID transfer)
      : transfer_(transfer) {}
  bool Prepend(ColorTransformInternal* prev) override {
    return prev->Join(*this);
  }

  bool IsNull() override { return transfer_ == ColorSpace::TransferID::LINEAR; }

  void transform(ColorTransform::TriStim* colors, size_t num) override {
    for (size_t i = 0; i < num; i++) {
      colors[i].set_x(FromLinear(transfer_, colors[i].x()));
      colors[i].set_y(FromLinear(transfer_, colors[i].y()));
      colors[i].set_z(FromLinear(transfer_, colors[i].z()));
    }
  }

 private:
  friend class ColorTransformToLinear;
  ColorSpace::TransferID transfer_;
};

class ColorTransformToLinear : public ColorTransformInternal {
 public:
  explicit ColorTransformToLinear(ColorSpace::TransferID transfer)
      : transfer_(transfer) {}

  bool Prepend(ColorTransformInternal* prev) override {
    return prev->Join(*this);
  }

  static bool IsGamma22(ColorSpace::TransferID transfer) {
    switch (transfer) {
      // We don't need to check BT709 here because it's been translated into
      // SRGB in ColorSpaceToColorSpaceTransform::ColorSpaceToLinear below.
      case ColorSpace::TransferID::GAMMA22:
      case ColorSpace::TransferID::IEC61966_2_1:  // SRGB
        return true;

      default:
        return false;
    }
  }

  bool Join(const ColorTransformFromLinear& next) override {
    if (transfer_ == next.transfer_ ||
        (IsGamma22(transfer_) && IsGamma22(next.transfer_))) {
      transfer_ = ColorSpace::TransferID::LINEAR;
      return true;
    }
    return false;
  }

  bool IsNull() override { return transfer_ == ColorSpace::TransferID::LINEAR; }

  // Assumes BT2020 primaries.
  static float Luma(const ColorTransform::TriStim& c) {
    return c.x() * 0.2627f + c.y() * 0.6780f + c.z() * 0.0593f;
  }

  void transform(ColorTransform::TriStim* colors, size_t num) override {
    if (transfer_ == ColorSpace::TransferID::SMPTEST2084_NON_HDR) {
      for (size_t i = 0; i < num; i++) {
        ColorTransform::TriStim ret(ToLinear(transfer_, colors[i].x()),
                                    ToLinear(transfer_, colors[i].y()),
                                    ToLinear(transfer_, colors[i].z()));
        if (Luma(ret) > 0.0) {
          ColorTransform::TriStim smpte2084(
              ToLinear(ColorSpace::TransferID::SMPTEST2084, colors[i].x()),
              ToLinear(ColorSpace::TransferID::SMPTEST2084, colors[i].y()),
              ToLinear(ColorSpace::TransferID::SMPTEST2084, colors[i].z()));
          smpte2084.Scale(Luma(ret) / Luma(smpte2084));
          ret = smpte2084;
        }
        colors[i] = ret;
      }
    } else {
      for (size_t i = 0; i < num; i++) {
        colors[i].set_x(ToLinear(transfer_, colors[i].x()));
        colors[i].set_y(ToLinear(transfer_, colors[i].y()));
        colors[i].set_z(ToLinear(transfer_, colors[i].z()));
      }
    }
  }

 private:
  ColorSpace::TransferID transfer_;
};

// BT2020 Constant Luminance is different than most other
// ways to encode RGB values as YUV. The basic idea is that
// transfer functions are applied on the Y value instead of
// on the RGB values. However, running the transfer function
// on the U and V values doesn't make any sense since they
// are centered at 0.5. To work around this, the transfer function
// is applied to the Y, R and B values, and then the U and V
// values are calculated from that.
// In our implementation, the YUV->RGB matrix is used to
// convert YUV to RYB (the G value is replaced with an Y value.)
// Then we run the transfer function like normal, and finally
// this class is inserted as an extra step which takes calculates
// the U and V values.
class ColorTransformToBT2020CL : public ColorTransformInternal {
 public:
  bool Prepend(ColorTransformInternal* prev) override {
    return prev->Join(*this);
  }

  bool Join(const ColorTransformFromBT2020CL& next) override {
    if (null_)
      return false;
    null_ = true;
    return true;
  }

  bool IsNull() override { return null_; }

  void transform(ColorTransform::TriStim* RYB, size_t num) override {
    for (size_t i = 0; i < num; i++) {
      float U, V;
      float B_Y = RYB[i].z() - RYB[i].y();
      if (B_Y <= 0) {
        U = B_Y / (-2.0 * -0.9702);
      } else {
        U = B_Y / (2.0 * 0.7910);
      }
      float R_Y = RYB[i].x() - RYB[i].y();
      if (R_Y <= 0) {
        V = R_Y / (-2.0 * -0.8591);
      } else {
        V = R_Y / (2.0 * 0.4969);
      }
      RYB[i] = ColorTransform::TriStim(RYB[i].y(), U, V);
    }
  }

 private:
  bool null_ = false;
};

// Inverse of ColorTransformToBT2020CL, see comment above for more info.
class ColorTransformFromBT2020CL : public ColorTransformInternal {
 public:
  bool Prepend(ColorTransformInternal* prev) override {
    return prev->Join(*this);
  }

  bool Join(const ColorTransformToBT2020CL& next) override {
    if (null_)
      return false;
    null_ = true;
    return true;
  }

  bool IsNull() override { return null_; }

  void transform(ColorTransform::TriStim* YUV, size_t num) override {
    if (null_)
      return;
    for (size_t i = 0; i < num; i++) {
      float Y = YUV[i].x();
      float U = YUV[i].y();
      float V = YUV[i].z();
      float B_Y, R_Y;
      if (U <= 0) {
        B_Y = Y * (-2.0 * -0.9702);
      } else {
        B_Y = U * (2.0 * 0.7910);
      }
      if (V <= 0) {
        R_Y = V * (-2.0 * -0.8591);
      } else {
        R_Y = V * (2.0 * 0.4969);
      }
      // Return an RYB value, later steps will fix it.
      YUV[i] = ColorTransform::TriStim(R_Y + Y, YUV[i].x(), B_Y + Y);
    }
  }

 private:
  bool null_ = false;
};

class ChainColorTransform : public ColorTransform {
 public:
  ChainColorTransform(std::unique_ptr<ColorTransform> a,
                      std::unique_ptr<ColorTransform> b)
      : a_(std::move(a)), b_(std::move(b)) {}

 private:
  void transform(TriStim* colors, size_t num) override {
    a_->transform(colors, num);
    b_->transform(colors, num);
  }
  std::unique_ptr<ColorTransform> a_;
  std::unique_ptr<ColorTransform> b_;
};

class TransformBuilder {
 public:
  void Append(std::unique_ptr<ColorTransformInternal> transform) {
    if (!disable_optimizations_ && transform->IsNull())
      return;  // Null transform
    transforms_.push_back(std::move(transform));
    if (disable_optimizations_)
      return;
    while (transforms_.size() >= 2 &&
           transforms_.back()->Prepend(
               transforms_[transforms_.size() - 2].get())) {
      transforms_.pop_back();
      if (transforms_.back()->IsNull()) {
        transforms_.pop_back();
        break;
      }
    }
  }

  std::unique_ptr<ColorTransform> GetTransform() {
    if (transforms_.empty())
      return base::MakeUnique<ColorTransformNull>();
    std::unique_ptr<ColorTransform> ret(std::move(transforms_.back()));
    transforms_.pop_back();

    while (!transforms_.empty()) {
      ret = std::unique_ptr<ColorTransform>(new ChainColorTransform(
          std::move(transforms_.back()), std::move(ret)));
      transforms_.pop_back();
    }

    return ret;
  }

  void disable_optimizations() { disable_optimizations_ = true; }

 private:
  bool disable_optimizations_ = false;
  std::vector<std::unique_ptr<ColorTransformInternal>> transforms_;
};

class ColorSpaceToColorSpaceTransform {
 public:
  static Transform GetPrimaryTransform(const ColorSpace& c) {
    if (c.primaries_ == ColorSpace::PrimaryID::CUSTOM) {
      return Transform(c.custom_primary_matrix_[0], c.custom_primary_matrix_[1],
                       c.custom_primary_matrix_[2], c.custom_primary_matrix_[3],
                       c.custom_primary_matrix_[4], c.custom_primary_matrix_[5],
                       c.custom_primary_matrix_[6], c.custom_primary_matrix_[7],
                       c.custom_primary_matrix_[8], c.custom_primary_matrix_[9],
                       c.custom_primary_matrix_[10],
                       c.custom_primary_matrix_[11], 0.0f, 0.0f, 0.0f, 1.0f);
    } else {
      return GetPrimaryMatrix(c.primaries_);
    }
  }

  static void ColorSpaceToColorSpace(ColorSpace from,
                                     ColorSpace to,
                                     ColorTransform::Intent intent,
                                     TransformBuilder* builder) {
    if (intent == ColorTransform::Intent::INTENT_PERCEPTUAL) {
      switch (from.transfer_) {
        case ColorSpace::TransferID::UNSPECIFIED:
        case ColorSpace::TransferID::BT709:
        case ColorSpace::TransferID::SMPTE170M:
          // SMPTE 1886 suggests that we should be using gamma 2.4 for BT709
          // content. However, most displays actually use a gamma of 2.2, and
          // user studies shows that users don't really care. Using the same
          // gamma as the display will let us optimize a lot more, so lets stick
          // with using the SRGB transfer function.
          from.transfer_ = ColorSpace::TransferID::IEC61966_2_1;
          break;

        case ColorSpace::TransferID::SMPTEST2084:
          // We don't have an HDR display, so replace SMPTE 2084 with something
          // that returns ranges more or less suitable for a normal display.
          from.transfer_ = ColorSpace::TransferID::SMPTEST2084_NON_HDR;
          break;

        case ColorSpace::TransferID::ARIB_STD_B67:
          // Interpreting HLG using a gamma 2.4 works reasonably well for SDR
          // displays. Once we have HDR output capabilies, we'll need to
          // change this.
          from.transfer_ = ColorSpace::TransferID::GAMMA24;
          break;

        default:  // Do nothing
          break;
      }

      // TODO(hubbe): shrink gamuts here (never stretch gamuts)
    }
    builder->Append(base::MakeUnique<ColorTransformMatrix>(
        GetRangeAdjustMatrix(from.range_, from.matrix_)));
    builder->Append(base::MakeUnique<ColorTransformMatrix>(
        Invert(GetTransferMatrix(from.matrix_))));
    builder->Append(base::MakeUnique<ColorTransformToLinear>(from.transfer_));
    if (from.matrix_ == ColorSpace::MatrixID::BT2020_CL) {
      // BT2020 CL is a special case.
      builder->Append(base::MakeUnique<ColorTransformFromBT2020CL>());
    }
    builder->Append(
        base::MakeUnique<ColorTransformMatrix>(GetPrimaryTransform(from)));

    builder->Append(base::MakeUnique<ColorTransformMatrix>(
        Invert(GetPrimaryTransform(to))));
    if (to.matrix_ == ColorSpace::MatrixID::BT2020_CL) {
      // BT2020 CL is a special case.
      builder->Append(base::MakeUnique<ColorTransformToBT2020CL>());
    }

    builder->Append(base::MakeUnique<ColorTransformFromLinear>(to.transfer_));
    builder->Append(
        base::MakeUnique<ColorTransformMatrix>(GetTransferMatrix(to.matrix_)));
    builder->Append(base::MakeUnique<ColorTransformMatrix>(
        Invert(GetRangeAdjustMatrix(to.range_, to.matrix_))));
  }
};

class QCMSColorTransform : public ColorTransformInternal {
 public:
  // Takes ownership of the profiles
  QCMSColorTransform(qcms_profile* from, qcms_profile* to)
      : from_(from), to_(to) {}
  ~QCMSColorTransform() override {
    qcms_profile_release(from_);
    qcms_profile_release(to_);
  }
  bool Prepend(ColorTransformInternal* prev) override {
    // Not currently optimizable.
    return false;
  }
  bool IsNull() override { return from_ == to_; }
  void transform(TriStim* colors, size_t num) override {
    CHECK(sizeof(TriStim) == sizeof(float[3]));
    // QCMS doesn't like numbers outside 0..1
    for (size_t i = 0; i < num; i++) {
      colors[i].set_x(fmin(1.0f, fmax(0.0f, colors[i].x())));
      colors[i].set_y(fmin(1.0f, fmax(0.0f, colors[i].y())));
      colors[i].set_z(fmin(1.0f, fmax(0.0f, colors[i].z())));
    }
    qcms_chain_transform(from_, to_, reinterpret_cast<float*>(colors),
                         reinterpret_cast<float*>(colors), num * 3);
  }

 private:
  qcms_profile *from_, *to_;
};

qcms_profile* GetQCMSProfileIfAvailable(const ColorSpace& color_space) {
  ICCProfile icc_profile = ICCProfile::FromColorSpace(color_space);
  if (icc_profile.GetData().empty())
    return nullptr;
  return qcms_profile_from_memory(icc_profile.GetData().data(),
                                  icc_profile.GetData().size());
}

qcms_profile* GetXYZD50Profile() {
  // QCMS is trixy, it has a datatype called qcms_CIE_xyY, but what it expects
  // is in fact not xyY color coordinates, it just wants the x/y values of the
  // primaries with Y equal to 1.0.
  qcms_CIE_xyYTRIPLE xyz;
  qcms_CIE_xyY w;
  xyz.red.x = 1.0f;
  xyz.red.y = 0.0f;
  xyz.red.Y = 1.0f;
  xyz.green.x = 0.0f;
  xyz.green.y = 1.0f;
  xyz.green.Y = 1.0f;
  xyz.blue.x = 0.0f;
  xyz.blue.y = 0.0f;
  xyz.blue.Y = 1.0f;
  w.x = 0.34567f;
  w.y = 0.35850f;
  w.Y = 1.0f;
  return qcms_profile_create_rgb_with_gamma(w, xyz, 1.0f);
}

std::unique_ptr<ColorTransform> ColorTransform::NewColorTransform(
    const ColorSpace& from,
    const ColorSpace& to,
    Intent intent) {
  TransformBuilder builder;
  if (intent == Intent::TEST_NO_OPT) {
    builder.disable_optimizations();
  }

  qcms_profile* from_profile = GetQCMSProfileIfAvailable(from);
  qcms_profile* to_profile = GetQCMSProfileIfAvailable(to);

  if (from_profile && to_profile) {
    return std::unique_ptr<ColorTransform>(
        new QCMSColorTransform(from_profile, to_profile));
  }
  if (from_profile) {
    builder.Append(std::unique_ptr<ColorTransformInternal>(
        new QCMSColorTransform(from_profile, GetXYZD50Profile())));
  }
  ColorSpaceToColorSpaceTransform::ColorSpaceToColorSpace(
      from_profile ? ColorSpace::CreateXYZD50() : from,
      to_profile ? ColorSpace::CreateXYZD50() : to, intent, &builder);
  if (to_profile) {
    builder.Append(std::unique_ptr<ColorTransformInternal>(
        new QCMSColorTransform(GetXYZD50Profile(), to_profile)));
  }

  return builder.GetTransform();
}

}  // namespace gfx
