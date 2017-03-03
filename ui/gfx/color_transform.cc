// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_transform.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <memory>
#include <sstream>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "third_party/qcms/src/qcms.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/skia_color_space_util.h"
#include "ui/gfx/transform.h"

#ifndef THIS_MUST_BE_INCLUDED_AFTER_QCMS_H
extern "C" {
#include "third_party/qcms/src/chain.h"
};
#endif

using std::exp;
using std::log;
using std::max;
using std::min;
using std::pow;
using std::sqrt;

namespace gfx {

namespace {

void InitStringStream(std::stringstream* ss) {
  ss->imbue(std::locale::classic());
  ss->precision(8);
  *ss << std::scientific;
}

std::string Str(float f) {
  std::stringstream ss;
  InitStringStream(&ss);
  ss << f;
  return ss.str();
}

// Helper for scoped QCMS profiles.
struct QcmsProfileDeleter {
  void operator()(qcms_profile* p) {
    if (p) {
      qcms_profile_release(p);
    }
  }
};
using ScopedQcmsProfile = std::unique_ptr<qcms_profile, QcmsProfileDeleter>;

Transform Invert(const Transform& t) {
  Transform ret = t;
  if (!t.GetInverse(&ret)) {
    LOG(ERROR) << "Inverse should always be possible.";
  }
  return ret;
}

float FromLinear(ColorSpace::TransferID id, float v) {
  switch (id) {
    case ColorSpace::TransferID::SMPTEST2084_NON_HDR:
      // Should already be handled.
      break;

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
        return -a * pow(-v, 0.45f) + (a - 1.0f);
      } else if (v <= b) {
        return 4.5f * v;
      } else {
        return a * pow(v, 0.45f) - (a - 1.0f);
      }
    }

    case ColorSpace::TransferID::BT1361_ECG: {
      float a = 1.099f;
      float b = 0.018f;
      float l = 0.0045f;
      if (v < -l) {
        return -(a * pow(-4.0f * v, 0.45f) + (a - 1.0f)) / 4.0f;
      } else if (v <= b) {
        return 4.5f * v;
      } else {
        return a * pow(v, 0.45f) - (a - 1.0f);
      }
    }

    case ColorSpace::TransferID::SMPTEST2084: {
      // Go from scRGB levels to 0-1.
      v *= 80.0f / 10000.0f;
      v = max(0.0f, v);
      float m1 = (2610.0f / 4096.0f) / 4.0f;
      float m2 = (2523.0f / 4096.0f) * 128.0f;
      float c1 = 3424.0f / 4096.0f;
      float c2 = (2413.0f / 4096.0f) * 32.0f;
      float c3 = (2392.0f / 4096.0f) * 32.0f;
      return pow((c1 + c2 * pow(v, m1)) / (1.0f + c3 * pow(v, m1)), m2);
    }

    // Spec: http://www.arib.or.jp/english/html/overview/doc/2-STD-B67v1_0.pdf
    case ColorSpace::TransferID::ARIB_STD_B67: {
      const float a = 0.17883277f;
      const float b = 0.28466892f;
      const float c = 0.55991073f;
      v = max(0.0f, v);
      if (v <= 1)
        return 0.5f * sqrt(v);
      else
        return a * log(v - b) + c;
    }

    default:
      // Handled by SkColorSpaceTransferFn.
      break;
  }
  NOTREACHED();
  return 0;
}

float ToLinear(ColorSpace::TransferID id, float v) {
  switch (id) {
    case ColorSpace::TransferID::LOG:
      if (v < 0.0f)
        return 0.0f;
      return pow(10.0f, (v - 1.0f) * 2.0f);

    case ColorSpace::TransferID::LOG_SQRT:
      if (v < 0.0f)
        return 0.0f;
      return pow(10.0f, (v - 1.0f) * 2.5f);

    case ColorSpace::TransferID::IEC61966_2_4: {
      float a = 1.099296826809442f;
      float b = 0.018053968510807f;
      if (v < FromLinear(ColorSpace::TransferID::IEC61966_2_4, -a)) {
        return -pow((a - 1.0f - v) / a, 1.0f / 0.45f);
      } else if (v <= FromLinear(ColorSpace::TransferID::IEC61966_2_4, b)) {
        return v / 4.5f;
      } else {
        return pow((v + a - 1.0f) / a, 1.0f / 0.45f);
      }
    }

    case ColorSpace::TransferID::BT1361_ECG: {
      float a = 1.099f;
      float b = 0.018f;
      float l = 0.0045f;
      if (v < FromLinear(ColorSpace::TransferID::BT1361_ECG, -l)) {
        return -pow((1.0f - a - v * 4.0f) / a, 1.0f / 0.45f) / 4.0f;
      } else if (v <= FromLinear(ColorSpace::TransferID::BT1361_ECG, b)) {
        return v / 4.5f;
      } else {
        return pow((v + a - 1.0f) / a, 1.0f / 0.45f);
      }
    }

    case ColorSpace::TransferID::SMPTEST2084: {
      v = max(0.0f, v);
      float m1 = (2610.0f / 4096.0f) / 4.0f;
      float m2 = (2523.0f / 4096.0f) * 128.0f;
      float c1 = 3424.0f / 4096.0f;
      float c2 = (2413.0f / 4096.0f) * 32.0f;
      float c3 = (2392.0f / 4096.0f) * 32.0f;
      v = pow(max(pow(v, 1.0f / m2) - c1, 0.0f) / (c2 - c3 * pow(v, 1.0f / m2)),
              1.0f / m1);
      // This matches the scRGB definition that 1.0 means 80 nits.
      // TODO(hubbe): It would be *nice* if 1.0 meant more than that, but
      // that might be difficult to do right now.
      v *= 10000.0f / 80.0f;
      return v;
    }

    case ColorSpace::TransferID::SMPTEST2084_NON_HDR:
      v = max(0.0f, v);
      return min(2.3f * pow(v, 2.8f), v / 5.0f + 0.8f);

    // Spec: http://www.arib.or.jp/english/html/overview/doc/2-STD-B67v1_0.pdf
    case ColorSpace::TransferID::ARIB_STD_B67: {
      v = max(0.0f, v);
      const float a = 0.17883277f;
      const float b = 0.28466892f;
      const float c = 0.55991073f;
      float v_ = 0.0f;
      if (v <= 0.5f) {
        v_ = (v * 2.0f) * (v * 2.0f);
      } else {
        v_ = exp((v - c) / a) + b;
      }
      return v_;
    }

    default:
      // Handled by SkColorSpaceTransferFn.
      break;
  }
  NOTREACHED();
  return 0;
}

Transform GetTransferMatrix(const gfx::ColorSpace& color_space) {
  SkMatrix44 transfer_matrix;
  color_space.GetTransferMatrix(&transfer_matrix);
  return Transform(transfer_matrix);
}

Transform GetRangeAdjustMatrix(const gfx::ColorSpace& color_space) {
  SkMatrix44 range_adjust_matrix;
  color_space.GetRangeAdjustMatrix(&range_adjust_matrix);
  return Transform(range_adjust_matrix);
}

Transform GetPrimaryTransform(const gfx::ColorSpace& color_space) {
  SkMatrix44 primary_matrix;
  color_space.GetPrimaryMatrix(&primary_matrix);
  return Transform(primary_matrix);
}

}  // namespace

class ColorTransformMatrix;
class ColorTransformSkTransferFn;
class ColorTransformFromLinear;
class ColorTransformToBT2020CL;
class ColorTransformFromBT2020CL;
class ColorTransformNull;
class QCMSColorTransform;

class ColorTransformStep {
 public:
  ColorTransformStep() {}
  virtual ~ColorTransformStep() {}
  virtual ColorTransformFromLinear* GetFromLinear() { return nullptr; }
  virtual ColorTransformToBT2020CL* GetToBT2020CL() { return nullptr; }
  virtual ColorTransformFromBT2020CL* GetFromBT2020CL() { return nullptr; }
  virtual ColorTransformSkTransferFn* GetSkTransferFn() { return nullptr; }
  virtual ColorTransformMatrix* GetMatrix() { return nullptr; }
  virtual ColorTransformNull* GetNull() { return nullptr; }
  virtual QCMSColorTransform* GetQCMS() { return nullptr; }

  // Join methods, returns true if the |next| transform was successfully
  // assimilated into |this|.
  // If Join() returns true, |next| is no longer needed and can be deleted.
  virtual bool Join(ColorTransformStep* next) { return false; }

  // Return true if this is a null transform.
  virtual bool IsNull() { return false; }
  virtual void Transform(ColorTransform::TriStim* color, size_t num) const = 0;
  virtual bool CanAppendShaderSource() { return false; }
  virtual void AppendShaderSource(std::stringstream* result) { NOTREACHED(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ColorTransformStep);
};

class ColorTransformInternal : public ColorTransform {
 public:
  ColorTransformInternal(const ColorSpace& from,
                         const ColorSpace& to,
                         Intent intent);
  ~ColorTransformInternal() override;

  gfx::ColorSpace GetSrcColorSpace() const override { return src_; };
  gfx::ColorSpace GetDstColorSpace() const override { return dst_; };

  void Transform(TriStim* colors, size_t num) const override {
    for (const auto& step : steps_)
      step->Transform(colors, num);
  }
  bool CanGetShaderSource() const override;
  std::string GetShaderSource() const override;
  bool IsIdentity() const override { return steps_.empty(); }
  size_t NumberOfStepsForTesting() const override { return steps_.size(); }

 private:
  void AppendColorSpaceToColorSpaceTransform(ColorSpace from,
                                             const ColorSpace& to,
                                             ColorTransform::Intent intent);
  void Simplify();

  // Retrieve the ICC profile from which |color_space| was created, only if that
  // is a more precise representation of the color space than the primaries and
  // transfer function in |color_space|.
  ScopedQcmsProfile GetQCMSProfileIfNecessary(const ColorSpace& color_space);

  std::list<std::unique_ptr<ColorTransformStep>> steps_;
  gfx::ColorSpace src_;
  gfx::ColorSpace dst_;
};

class ColorTransformNull : public ColorTransformStep {
 public:
  ColorTransformNull* GetNull() override { return this; }
  bool IsNull() override { return true; }
  void Transform(ColorTransform::TriStim* color, size_t num) const override {}
  bool CanAppendShaderSource() override { return true; }
  void AppendShaderSource(std::stringstream* result) override {}
};

class ColorTransformMatrix : public ColorTransformStep {
 public:
  explicit ColorTransformMatrix(const class Transform& matrix)
      : matrix_(matrix) {}
  ColorTransformMatrix* GetMatrix() override { return this; }
  bool Join(ColorTransformStep* next_untyped) override {
    ColorTransformMatrix* next = next_untyped->GetMatrix();
    if (!next)
      return false;
    class Transform tmp = next->matrix_;
    tmp *= matrix_;
    matrix_ = tmp;
    return true;
  }

  bool IsNull() override {
    return SkMatrixIsApproximatelyIdentity(matrix_.matrix());
  }

  void Transform(ColorTransform::TriStim* colors, size_t num) const override {
    for (size_t i = 0; i < num; i++)
      matrix_.TransformPoint(colors + i);
  }

  bool CanAppendShaderSource() override { return true; }

  void AppendShaderSource(std::stringstream* result) override {
    const SkMatrix44& m = matrix_.matrix();
    *result << "  color = mat3(";
    *result << m.get(0, 0) << ", " << m.get(1, 0) << ", " << m.get(2, 0) << ",";
    *result << std::endl;
    *result << "               ";
    *result << m.get(0, 1) << ", " << m.get(1, 1) << ", " << m.get(2, 1) << ",";
    *result << std::endl;
    *result << "               ";
    *result << m.get(0, 2) << ", " << m.get(1, 2) << ", " << m.get(2, 2) << ")";
    *result << " * color;" << std::endl;

    // Only print the translational component if it isn't the identity.
    if (m.get(0, 3) != 0.f || m.get(1, 3) != 0.f || m.get(2, 3) != 0.f) {
      *result << "  color += vec3(";
      *result << m.get(0, 3) << ", " << m.get(1, 3) << ", " << m.get(2, 3);
      *result << ");" << std::endl;
    }
  }

 private:
  class Transform matrix_;
};

class ColorTransformSkTransferFn : public ColorTransformStep {
 public:
  explicit ColorTransformSkTransferFn(const SkColorSpaceTransferFn& fn)
      : fn_(fn) {}
  ColorTransformSkTransferFn* GetSkTransferFn() override { return this; }

  bool Join(ColorTransformStep* next_untyped) override {
    ColorTransformSkTransferFn* next = next_untyped->GetSkTransferFn();
    if (!next)
      return false;
    if (SkTransferFnsApproximatelyCancel(fn_, next->fn_)) {
      // Set to be the identity.
      fn_.fA = 1;
      fn_.fB = 0;
      fn_.fC = 1;
      fn_.fD = 0;
      fn_.fE = 0;
      fn_.fF = 0;
      fn_.fG = 1;
      return true;
    }
    return false;
  }

  bool IsNull() override { return SkTransferFnIsApproximatelyIdentity(fn_); }

  void Transform(ColorTransform::TriStim* colors, size_t num) const override {
    for (size_t i = 0; i < num; i++) {
      colors[i].set_x(SkTransferFnEval(fn_, colors[i].x()));
      colors[i].set_y(SkTransferFnEval(fn_, colors[i].y()));
      colors[i].set_z(SkTransferFnEval(fn_, colors[i].z()));
    }
  }

  bool CanAppendShaderSource() override { return true; }

  void AppendShaderSourceChannel(std::stringstream* result, const char* value) {
    const float kEpsilon = 1.f / 1024.f;

    // Construct the linear segment
    //   linear = C * x + F
    // Elide operations that will be close to the identity.
    std::string linear = value;
    if (std::abs(fn_.fC - 1.f) > kEpsilon)
      linear = Str(fn_.fC) + " * " + linear;
    if (std::abs(fn_.fF) > kEpsilon)
      linear = linear + " + " + Str(fn_.fF);

    // Construct the nonlinear segment.
    //   nonlinear = pow(A * x + B, G) + E
    // Elide operations (especially the pow) that will be close to the
    // identity.
    std::string nonlinear = value;
    if (std::abs(fn_.fA - 1.f) > kEpsilon)
      nonlinear = Str(fn_.fA) + " * " + nonlinear;
    if (std::abs(fn_.fB) > kEpsilon)
      nonlinear = nonlinear + " + " + Str(fn_.fB);
    if (std::abs(fn_.fG - 1.f) > kEpsilon)
      nonlinear = "pow(" + nonlinear + ", " + Str(fn_.fG) + ")";
    if (std::abs(fn_.fE) > kEpsilon)
      nonlinear = nonlinear + " + " + Str(fn_.fE);

    // Add both parts, skpping the if clause if possible.
    if (fn_.fD > kEpsilon) {
      *result << "  if (" << value << " < " << Str(fn_.fD) << ")" << std::endl;
      *result << "    " << value << " = " << linear << ";" << std::endl;
      *result << "  else" << std::endl;
      *result << "    " << value << " = " << nonlinear << ";" << std::endl;
    } else {
      *result << "  " << value << " = " << nonlinear << ";" << std::endl;
    }
  }

  void AppendShaderSource(std::stringstream* result) override {
    // Append the transfer function for each channel.
    AppendShaderSourceChannel(result, "color.r");
    AppendShaderSourceChannel(result, "color.g");
    AppendShaderSourceChannel(result, "color.b");
  }

 private:
  SkColorSpaceTransferFn fn_;
};

class ColorTransformFromLinear : public ColorTransformStep {
 public:
  explicit ColorTransformFromLinear(ColorSpace::TransferID transfer)
      : transfer_(transfer) {}
  ColorTransformFromLinear* GetFromLinear() override { return this; }
  bool IsNull() override { return transfer_ == ColorSpace::TransferID::LINEAR; }
  void Transform(ColorTransform::TriStim* colors, size_t num) const override {
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

class ColorTransformToLinear : public ColorTransformStep {
 public:
  explicit ColorTransformToLinear(ColorSpace::TransferID transfer)
      : transfer_(transfer) {}

  bool Join(ColorTransformStep* next_untyped) override {
    ColorTransformFromLinear* next = next_untyped->GetFromLinear();
    if (!next)
      return false;
    if (transfer_ == next->transfer_) {
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

  static ColorTransform::TriStim ClipToWhite(ColorTransform::TriStim& c) {
    float maximum = max(max(c.x(), c.y()), c.z());
    if (maximum > 1.0f) {
      float l = Luma(c);
      c.Scale(1.0f / maximum);
      ColorTransform::TriStim white(1.0f, 1.0f, 1.0f);
      white.Scale((1.0f - 1.0f / maximum) * l / Luma(white));
      ColorTransform::TriStim black(0.0f, 0.0f, 0.0f);
      c += white - black;
    }
    return c;
  }

  void Transform(ColorTransform::TriStim* colors, size_t num) const override {
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
          ret = ClipToWhite(smpte2084);
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
class ColorTransformToBT2020CL : public ColorTransformStep {
 public:
  bool Join(ColorTransformStep* next_untyped) override {
    ColorTransformFromBT2020CL* next = next_untyped->GetFromBT2020CL();
    if (!next)
      return false;
    if (null_)
      return false;
    null_ = true;
    return true;
  }

  bool IsNull() override { return null_; }

  void Transform(ColorTransform::TriStim* RYB, size_t num) const override {
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
class ColorTransformFromBT2020CL : public ColorTransformStep {
 public:
  bool Join(ColorTransformStep* next_untyped) override {
    ColorTransformToBT2020CL* next = next_untyped->GetToBT2020CL();
    if (!next)
      return false;
    if (null_)
      return false;
    null_ = true;
    return true;
  }

  bool IsNull() override { return null_; }

  void Transform(ColorTransform::TriStim* YUV, size_t num) const override {
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

void ColorTransformInternal::AppendColorSpaceToColorSpaceTransform(
    ColorSpace from,
    const ColorSpace& to,
    ColorTransform::Intent intent) {
  if (intent == ColorTransform::Intent::INTENT_PERCEPTUAL) {
    switch (from.transfer_) {
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
        if (!to.IsHDR()) {
          // We don't have an HDR display, so replace SMPTE 2084 with
          // something that returns ranges more or less suitable for a normal
          // display.
          from.transfer_ = ColorSpace::TransferID::SMPTEST2084_NON_HDR;
        }
        break;

      case ColorSpace::TransferID::ARIB_STD_B67:
        if (!to.IsHDR()) {
          // Interpreting HLG using a gamma 2.4 works reasonably well for SDR
          // displays.
          from.transfer_ = ColorSpace::TransferID::GAMMA24;
        }
        break;

      default:  // Do nothing
        break;
    }

    // TODO(hubbe): shrink gamuts here (never stretch gamuts)
  }

  steps_.push_back(
      base::MakeUnique<ColorTransformMatrix>(GetRangeAdjustMatrix(from)));

  steps_.push_back(
      base::MakeUnique<ColorTransformMatrix>(Invert(GetTransferMatrix(from))));

  // If the target color space is not defined, just apply the adjust and
  // tranfer matrices. This path is used by YUV to RGB color conversion
  // when full color conversion is not enabled.
  if (!to.IsValid())
    return;

  SkColorSpaceTransferFn to_linear_fn;
  if (from.GetTransferFunction(&to_linear_fn)) {
    steps_.push_back(
        base::MakeUnique<ColorTransformSkTransferFn>(to_linear_fn));
  } else {
    steps_.push_back(base::MakeUnique<ColorTransformToLinear>(from.transfer_));
  }

  if (from.matrix_ == ColorSpace::MatrixID::BT2020_CL) {
    // BT2020 CL is a special case.
    steps_.push_back(base::MakeUnique<ColorTransformFromBT2020CL>());
  }
  steps_.push_back(
      base::MakeUnique<ColorTransformMatrix>(GetPrimaryTransform(from)));

  steps_.push_back(
      base::MakeUnique<ColorTransformMatrix>(Invert(GetPrimaryTransform(to))));
  if (to.matrix_ == ColorSpace::MatrixID::BT2020_CL) {
    // BT2020 CL is a special case.
    steps_.push_back(base::MakeUnique<ColorTransformToBT2020CL>());
  }

  SkColorSpaceTransferFn from_linear_fn;
  if (to.GetInverseTransferFunction(&from_linear_fn)) {
    steps_.push_back(
        base::MakeUnique<ColorTransformSkTransferFn>(from_linear_fn));
  } else {
    steps_.push_back(base::MakeUnique<ColorTransformFromLinear>(to.transfer_));
  }

  steps_.push_back(
      base::MakeUnique<ColorTransformMatrix>(GetTransferMatrix(to)));

  steps_.push_back(
      base::MakeUnique<ColorTransformMatrix>(Invert(GetRangeAdjustMatrix(to))));
}

class QCMSColorTransform : public ColorTransformStep {
 public:
  // Takes ownership of the profiles
  QCMSColorTransform(ScopedQcmsProfile from, ScopedQcmsProfile to)
      : from_(std::move(from)), to_(std::move(to)) {}
  ~QCMSColorTransform() override {}
  QCMSColorTransform* GetQCMS() override { return this; }
  bool Join(ColorTransformStep* next_untyped) override {
    QCMSColorTransform* next = next_untyped->GetQCMS();
    if (!next)
      return false;
    if (qcms_profile_match(to_.get(), next->from_.get())) {
      to_ = std::move(next->to_);
      return true;
    }
    return false;
  }
  bool IsNull() override {
    if (qcms_profile_match(from_.get(), to_.get()))
      return true;
    return false;
  }
  void Transform(ColorTransform::TriStim* colors, size_t num) const override {
    CHECK(sizeof(ColorTransform::TriStim) == sizeof(float[3]));
    // QCMS doesn't like numbers outside 0..1
    for (size_t i = 0; i < num; i++) {
      colors[i].set_x(min(1.0f, max(0.0f, colors[i].x())));
      colors[i].set_y(min(1.0f, max(0.0f, colors[i].y())));
      colors[i].set_z(min(1.0f, max(0.0f, colors[i].z())));
    }
    qcms_chain_transform(from_.get(), to_.get(),
                         reinterpret_cast<float*>(colors),
                         reinterpret_cast<float*>(colors), num * 3);
  }

 private:
  ScopedQcmsProfile from_;
  ScopedQcmsProfile to_;
};

ScopedQcmsProfile ColorTransformInternal::GetQCMSProfileIfNecessary(
    const ColorSpace& color_space) {
  ICCProfile icc_profile;
  if (!ICCProfile::FromId(color_space.icc_profile_id_, true, &icc_profile))
    return nullptr;
  return ScopedQcmsProfile(qcms_profile_from_memory(
      icc_profile.GetData().data(), icc_profile.GetData().size()));
}

ScopedQcmsProfile GetXYZD50Profile() {
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
  return ScopedQcmsProfile(qcms_profile_create_rgb_with_gamma(w, xyz, 1.0f));
}

ColorTransformInternal::ColorTransformInternal(const ColorSpace& src,
                                               const ColorSpace& dst,
                                               Intent intent)
    : src_(src), dst_(dst) {
  // If no source color space is specified, do no transformation.
  // TODO(ccameron): We may want dst assume sRGB at some point in the future.
  if (!src_.IsValid())
    return;

  // If the target color space is not defined, just apply the adjust and
  // tranfer matrices. This path is used by YUV to RGB color conversion
  // when full color conversion is not enabled.
  ScopedQcmsProfile src_profile;
  ScopedQcmsProfile dst_profile;
  if (dst.IsValid()) {
    src_profile = GetQCMSProfileIfNecessary(src_);
    dst_profile = GetQCMSProfileIfNecessary(dst_);
  }
  bool has_src_profile = !!src_profile;
  bool has_dst_profile = !!dst_profile;

  if (src_profile) {
    steps_.push_back(base::MakeUnique<QCMSColorTransform>(
        std::move(src_profile), GetXYZD50Profile()));
  }

  AppendColorSpaceToColorSpaceTransform(
      has_src_profile ? ColorSpace::CreateXYZD50() : src_,
      has_dst_profile ? ColorSpace::CreateXYZD50() : dst_, intent);

  if (dst_profile) {
    steps_.push_back(base::MakeUnique<QCMSColorTransform>(
        GetXYZD50Profile(), std::move(dst_profile)));
  }

  if (intent != Intent::TEST_NO_OPT)
    Simplify();
}

std::string ColorTransformInternal::GetShaderSource() const {
  std::stringstream result;
  InitStringStream(&result);
  result << "vec3 DoColorConversion(vec3 color) {" << std::endl;
  for (const auto& step : steps_)
    step->AppendShaderSource(&result);
  result << "  return color;" << std::endl;
  result << "}" << std::endl;
  return result.str();
}

bool ColorTransformInternal::CanGetShaderSource() const {
  for (const auto& step : steps_) {
    if (!step->CanAppendShaderSource())
      return false;
  }
  return true;
}

ColorTransformInternal::~ColorTransformInternal() {}

void ColorTransformInternal::Simplify() {
  for (auto iter = steps_.begin(); iter != steps_.end();) {
    std::unique_ptr<ColorTransformStep>& this_step = *iter;

    // Try to Join |next_step| into |this_step|. If successful, re-visit the
    // step before |this_step|.
    auto iter_next = iter;
    iter_next++;
    if (iter_next != steps_.end()) {
      std::unique_ptr<ColorTransformStep>& next_step = *iter_next;
      if (this_step->Join(next_step.get())) {
        steps_.erase(iter_next);
        if (iter != steps_.begin())
          --iter;
        continue;
      }
    }

    // If |this_step| step is a no-op, remove it, and re-visit the step before
    // |this_step|.
    if (this_step->IsNull()) {
      iter = steps_.erase(iter);
      if (iter != steps_.begin())
        --iter;
      continue;
    }

    ++iter;
  }
}

// static
std::unique_ptr<ColorTransform> ColorTransform::NewColorTransform(
    const ColorSpace& from,
    const ColorSpace& to,
    Intent intent) {
  return std::unique_ptr<ColorTransform>(
      new ColorTransformInternal(from, to, intent));
}

ColorTransform::ColorTransform() {}
ColorTransform::~ColorTransform() {}

}  // namespace gfx
