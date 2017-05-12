// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/geometry/DOMMatrixReadOnly.h"

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/TransformBuilder.h"
#include "core/geometry/DOMMatrix.h"
#include "core/geometry/DOMMatrixInit.h"
#include "core/geometry/DOMPoint.h"
#include "core/geometry/DOMPointInit.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace {

void SetDictionaryMembers(DOMMatrixInit& other) {
  if (!other.hasM11())
    other.setM11(other.hasA() ? other.a() : 1);

  if (!other.hasM12())
    other.setM12(other.hasB() ? other.b() : 0);

  if (!other.hasM21())
    other.setM21(other.hasC() ? other.c() : 0);

  if (!other.hasM22())
    other.setM22(other.hasD() ? other.d() : 1);

  if (!other.hasM41())
    other.setM41(other.hasE() ? other.e() : 0);

  if (!other.hasM42())
    other.setM42(other.hasF() ? other.f() : 0);
}

String GetErrorMessage(const char* a, const char* b) {
  return String::Format("The '%s' property should equal the '%s' property.", a,
                        b);
}

}  // namespace

bool DOMMatrixReadOnly::ValidateAndFixup(DOMMatrixInit& other,
                                         ExceptionState& exception_state) {
  if (other.hasA() && other.hasM11() && other.a() != other.m11()) {
    exception_state.ThrowTypeError(GetErrorMessage("a", "m11"));
    return false;
  }
  if (other.hasB() && other.hasM12() && other.b() != other.m12()) {
    exception_state.ThrowTypeError(GetErrorMessage("b", "m12"));
    return false;
  }
  if (other.hasC() && other.hasM21() && other.c() != other.m21()) {
    exception_state.ThrowTypeError(GetErrorMessage("c", "m21"));
    return false;
  }
  if (other.hasD() && other.hasM22() && other.d() != other.m22()) {
    exception_state.ThrowTypeError(GetErrorMessage("d", "m22"));
    return false;
  }
  if (other.hasE() && other.hasM41() && other.e() != other.m41()) {
    exception_state.ThrowTypeError(GetErrorMessage("e", "m41"));
    return false;
  }
  if (other.hasF() && other.hasM42() && other.f() != other.m42()) {
    exception_state.ThrowTypeError(GetErrorMessage("f", "m42"));
    return false;
  }
  if (other.hasIs2D() && other.is2D() &&
      (other.m31() || other.m32() || other.m13() || other.m23() ||
       other.m43() || other.m14() || other.m24() || other.m34() ||
       other.m33() != 1 || other.m44() != 1)) {
    exception_state.ThrowTypeError(
        "The is2D member is set to true but the input matrix is 3d matrix.");
    return false;
  }

  SetDictionaryMembers(other);
  if (!other.hasIs2D()) {
    bool is2d = !(other.m31() || other.m32() || other.m13() || other.m23() ||
                  other.m43() || other.m14() || other.m24() || other.m34() ||
                  other.m33() != 1 || other.m44() != 1);
    other.setIs2D(is2d);
  }
  return true;
}

DOMMatrixReadOnly* DOMMatrixReadOnly::Create(
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  return new DOMMatrixReadOnly(TransformationMatrix());
}

DOMMatrixReadOnly* DOMMatrixReadOnly::Create(
    ExecutionContext* execution_context,
    StringOrUnrestrictedDoubleSequence& init,
    ExceptionState& exception_state) {
  if (init.isString()) {
    if (!execution_context->IsDocument()) {
      exception_state.ThrowTypeError(
          "DOMMatrix can't be constructed with strings on workers.");
      return nullptr;
    }

    DOMMatrixReadOnly* matrix = new DOMMatrixReadOnly(TransformationMatrix());
    matrix->SetMatrixValueFromString(init.getAsString(), exception_state);
    return matrix;
  }

  if (init.isUnrestrictedDoubleSequence()) {
    const Vector<double>& sequence = init.getAsUnrestrictedDoubleSequence();
    if (sequence.size() != 6 && sequence.size() != 16) {
      exception_state.ThrowTypeError(
          "The sequence must contain 6 elements for a 2D matrix or 16 elements "
          "for a 3D matrix.");
      return nullptr;
    }
    return new DOMMatrixReadOnly(sequence, sequence.size());
  }

  NOTREACHED();
  return nullptr;
}

DOMMatrixReadOnly* DOMMatrixReadOnly::fromFloat32Array(
    NotShared<DOMFloat32Array> float32_array,
    ExceptionState& exception_state) {
  if (float32_array.View()->length() != 6 &&
      float32_array.View()->length() != 16) {
    exception_state.ThrowTypeError(
        "The sequence must contain 6 elements for a 2D matrix or 16 elements a "
        "for 3D matrix.");
    return nullptr;
  }
  return new DOMMatrixReadOnly(float32_array.View()->Data(),
                               float32_array.View()->length());
}

DOMMatrixReadOnly* DOMMatrixReadOnly::fromFloat64Array(
    NotShared<DOMFloat64Array> float64_array,
    ExceptionState& exception_state) {
  if (float64_array.View()->length() != 6 &&
      float64_array.View()->length() != 16) {
    exception_state.ThrowTypeError(
        "The sequence must contain 6 elements for a 2D matrix or 16 elements "
        "for a 3D matrix.");
    return nullptr;
  }
  return new DOMMatrixReadOnly(float64_array.View()->Data(),
                               float64_array.View()->length());
}

DOMMatrixReadOnly* DOMMatrixReadOnly::fromMatrix(
    DOMMatrixInit& other,
    ExceptionState& exception_state) {
  if (!ValidateAndFixup(other, exception_state)) {
    DCHECK(exception_state.HadException());
    return nullptr;
  }

  if (other.is2D()) {
    double args[] = {other.m11(), other.m12(), other.m21(),
                     other.m22(), other.m41(), other.m42()};
    return new DOMMatrixReadOnly(args, 6);
  }

  double args[] = {other.m11(), other.m12(), other.m13(), other.m14(),
                   other.m21(), other.m22(), other.m23(), other.m24(),
                   other.m31(), other.m32(), other.m33(), other.m34(),
                   other.m41(), other.m42(), other.m43(), other.m44()};
  return new DOMMatrixReadOnly(args, 16);
}

DOMMatrixReadOnly::~DOMMatrixReadOnly() {}

bool DOMMatrixReadOnly::is2D() const {
  return is2d_;
}

bool DOMMatrixReadOnly::isIdentity() const {
  return matrix_->IsIdentity();
}

DOMMatrix* DOMMatrixReadOnly::multiply(DOMMatrixInit& other,
                                       ExceptionState& exception_state) {
  return DOMMatrix::Create(this)->multiplySelf(other, exception_state);
}

DOMMatrix* DOMMatrixReadOnly::translate(double tx, double ty, double tz) {
  return DOMMatrix::Create(this)->translateSelf(tx, ty, tz);
}

DOMMatrix* DOMMatrixReadOnly::scale(double sx) {
  return scale(sx, sx);
}

DOMMatrix* DOMMatrixReadOnly::scale(double sx,
                                    double sy,
                                    double sz,
                                    double ox,
                                    double oy,
                                    double oz) {
  return DOMMatrix::Create(this)->scaleSelf(sx, sy, sz, ox, oy, oz);
}

DOMMatrix* DOMMatrixReadOnly::scale3d(double scale,
                                      double ox,
                                      double oy,
                                      double oz) {
  return DOMMatrix::Create(this)->scale3dSelf(scale, ox, oy, oz);
}

DOMMatrix* DOMMatrixReadOnly::rotate(double rot_x) {
  return DOMMatrix::Create(this)->rotateSelf(rot_x);
}

DOMMatrix* DOMMatrixReadOnly::rotate(double rot_x, double rot_y) {
  return DOMMatrix::Create(this)->rotateSelf(rot_x, rot_y);
}

DOMMatrix* DOMMatrixReadOnly::rotate(double rot_x, double rot_y, double rot_z) {
  return DOMMatrix::Create(this)->rotateSelf(rot_x, rot_y, rot_z);
}

DOMMatrix* DOMMatrixReadOnly::rotateFromVector(double x, double y) {
  return DOMMatrix::Create(this)->rotateFromVectorSelf(x, y);
}

DOMMatrix* DOMMatrixReadOnly::rotateAxisAngle(double x,
                                              double y,
                                              double z,
                                              double angle) {
  return DOMMatrix::Create(this)->rotateAxisAngleSelf(x, y, z, angle);
}

DOMMatrix* DOMMatrixReadOnly::skewX(double sx) {
  return DOMMatrix::Create(this)->skewXSelf(sx);
}

DOMMatrix* DOMMatrixReadOnly::skewY(double sy) {
  return DOMMatrix::Create(this)->skewYSelf(sy);
}

DOMMatrix* DOMMatrixReadOnly::flipX() {
  DOMMatrix* flip_x = DOMMatrix::Create(this);
  flip_x->setM11(-this->m11());
  flip_x->setM12(-this->m12());
  flip_x->setM13(-this->m13());
  flip_x->setM14(-this->m14());
  return flip_x;
}

DOMMatrix* DOMMatrixReadOnly::flipY() {
  DOMMatrix* flip_y = DOMMatrix::Create(this);
  flip_y->setM21(-this->m21());
  flip_y->setM22(-this->m22());
  flip_y->setM23(-this->m23());
  flip_y->setM24(-this->m24());
  return flip_y;
}

DOMMatrix* DOMMatrixReadOnly::inverse() {
  return DOMMatrix::Create(this)->invertSelf();
}

DOMPoint* DOMMatrixReadOnly::transformPoint(const DOMPointInit& point) {
  if (is2D() && point.z() == 0 && point.w() == 1) {
    double x = point.x() * m11() + point.y() * m12() + m41();
    double y = point.x() * m12() + point.y() * m22() + m42();
    return DOMPoint::Create(x, y, 0, 1);
  }

  double x = point.x() * m11() + point.y() * m21() + point.z() * m31() +
             point.w() * m41();
  double y = point.x() * m12() + point.y() * m22() + point.z() * m32() +
             point.w() * m42();
  double z = point.x() * m13() + point.y() * m23() + point.z() * m33() +
             point.w() * m43();
  double w = point.x() * m14() + point.y() * m24() + point.z() * m34() +
             point.w() * m44();
  return DOMPoint::Create(x, y, z, w);
}

DOMMatrixReadOnly::DOMMatrixReadOnly(const TransformationMatrix& matrix,
                                     bool is2d) {
  matrix_ = TransformationMatrix::Create(matrix);
  is2d_ = is2d;
}

NotShared<DOMFloat32Array> DOMMatrixReadOnly::toFloat32Array() const {
  float array[] = {
      static_cast<float>(matrix_->M11()), static_cast<float>(matrix_->M12()),
      static_cast<float>(matrix_->M13()), static_cast<float>(matrix_->M14()),
      static_cast<float>(matrix_->M21()), static_cast<float>(matrix_->M22()),
      static_cast<float>(matrix_->M23()), static_cast<float>(matrix_->M24()),
      static_cast<float>(matrix_->M31()), static_cast<float>(matrix_->M32()),
      static_cast<float>(matrix_->M33()), static_cast<float>(matrix_->M34()),
      static_cast<float>(matrix_->M41()), static_cast<float>(matrix_->M42()),
      static_cast<float>(matrix_->M43()), static_cast<float>(matrix_->M44())};

  return NotShared<DOMFloat32Array>(DOMFloat32Array::Create(array, 16));
}

NotShared<DOMFloat64Array> DOMMatrixReadOnly::toFloat64Array() const {
  double array[] = {
      matrix_->M11(), matrix_->M12(), matrix_->M13(), matrix_->M14(),
      matrix_->M21(), matrix_->M22(), matrix_->M23(), matrix_->M24(),
      matrix_->M31(), matrix_->M32(), matrix_->M33(), matrix_->M34(),
      matrix_->M41(), matrix_->M42(), matrix_->M43(), matrix_->M44()};

  return NotShared<DOMFloat64Array>(DOMFloat64Array::Create(array, 16));
}

const String DOMMatrixReadOnly::toString() const {
  std::stringstream stream;
  if (is2D()) {
    stream << "matrix(" << a() << ", " << b() << ", " << c() << ", " << d()
           << ", " << e() << ", " << f();
  } else {
    stream << "matrix3d(" << m11() << ", " << m12() << ", " << m13() << ", "
           << m14() << ", " << m21() << ", " << m22() << ", " << m23() << ", "
           << m24() << ", " << m31() << ", " << m32() << ", " << m33() << ", "
           << m34() << ", " << m41() << ", " << m42() << ", " << m43() << ", "
           << m44();
  }
  stream << ")";

  return String(stream.str().c_str());
}

ScriptValue DOMMatrixReadOnly::toJSONForBinding(
    ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.AddNumber("a", a());
  result.AddNumber("b", b());
  result.AddNumber("c", c());
  result.AddNumber("d", d());
  result.AddNumber("e", e());
  result.AddNumber("f", f());
  result.AddNumber("m11", m11());
  result.AddNumber("m12", m12());
  result.AddNumber("m13", m13());
  result.AddNumber("m14", m14());
  result.AddNumber("m21", m21());
  result.AddNumber("m22", m22());
  result.AddNumber("m23", m23());
  result.AddNumber("m24", m24());
  result.AddNumber("m31", m31());
  result.AddNumber("m32", m32());
  result.AddNumber("m33", m33());
  result.AddNumber("m34", m34());
  result.AddNumber("m41", m41());
  result.AddNumber("m42", m42());
  result.AddNumber("m43", m43());
  result.AddNumber("m44", m44());
  result.AddBoolean("is2D", is2D());
  result.AddBoolean("isIdentity", isIdentity());
  return result.GetScriptValue();
}

void DOMMatrixReadOnly::SetMatrixValueFromString(
    const String& input_string,
    ExceptionState& exception_state) {
  DEFINE_STATIC_LOCAL(String, identity_matrix2d, ("matrix(1, 0, 0, 1, 0, 0)"));
  String string = input_string;
  if (string.IsEmpty())
    string = identity_matrix2d;

  const CSSValue* value =
      CSSParser::ParseSingleValue(CSSPropertyTransform, string);

  if (!value || value->IsCSSWideKeyword()) {
    exception_state.ThrowDOMException(
        kSyntaxError, "Failed to parse '" + input_string + "'.");
    return;
  }

  if (value->IsIdentifierValue()) {
    DCHECK(ToCSSIdentifierValue(value)->GetValueID() == CSSValueNone);
    matrix_->MakeIdentity();
    is2d_ = true;
    return;
  }

  if (TransformBuilder::HasRelativeLengths(ToCSSValueList(*value))) {
    exception_state.ThrowDOMException(kSyntaxError,
                                      "Lengths must be absolute, not relative");
    return;
  }

  const ComputedStyle& initial_style = ComputedStyle::InitialStyle();
  TransformOperations operations = TransformBuilder::CreateTransformOperations(
      *value, CSSToLengthConversionData(&initial_style, &initial_style,
                                        LayoutViewItem(nullptr), 1.0f));

  if (operations.DependsOnBoxSize()) {
    exception_state.ThrowDOMException(
        kSyntaxError, "Lengths must be absolute, not depend on the box size");
    return;
  }

  matrix_->MakeIdentity();
  operations.Apply(FloatSize(0, 0), *matrix_);

  is2d_ = !operations.Has3DOperation();

  return;
}

}  // namespace blink
