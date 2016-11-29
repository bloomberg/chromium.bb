/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "platform/transforms/RotateTransformOperation.h"

#include "platform/animation/AnimationUtilities.h"

namespace blink {

bool RotateTransformOperation::operator==(
    const TransformOperation& other) const {
  if (!isSameType(other))
    return false;
  const Rotation& otherRotation = toRotateTransformOperation(other).m_rotation;
  return m_rotation.axis == otherRotation.axis &&
         m_rotation.angle == otherRotation.angle;
}

bool RotateTransformOperation::getCommonAxis(const RotateTransformOperation* a,
                                             const RotateTransformOperation* b,
                                             FloatPoint3D& resultAxis,
                                             double& resultAngleA,
                                             double& resultAngleB) {
  return Rotation::getCommonAxis(a ? a->m_rotation : Rotation(),
                                 b ? b->m_rotation : Rotation(), resultAxis,
                                 resultAngleA, resultAngleB);
}

PassRefPtr<TransformOperation> RotateTransformOperation::blend(
    const TransformOperation* from,
    double progress,
    bool blendToIdentity) {
  if (from && !from->isSameType(*this))
    return this;

  if (blendToIdentity)
    return RotateTransformOperation::create(
        Rotation(axis(), angle() * (1 - progress)), m_type);

  // Optimize for single axis rotation
  if (!from)
    return RotateTransformOperation::create(
        Rotation(axis(), angle() * progress), m_type);

  const RotateTransformOperation& fromRotate =
      toRotateTransformOperation(*from);
  if (type() == Rotate3D) {
    return RotateTransformOperation::create(
        Rotation::slerp(fromRotate.m_rotation, m_rotation, progress), Rotate3D);
  }

  ASSERT(axis() == fromRotate.axis());
  return RotateTransformOperation::create(
      Rotation(axis(), blink::blend(fromRotate.angle(), angle(), progress)),
      m_type);
}

bool RotateTransformOperation::canBlendWith(
    const TransformOperation& other) const {
  return other.isSameType(*this);
}

RotateAroundOriginTransformOperation::RotateAroundOriginTransformOperation(
    double angle,
    double originX,
    double originY)
    : RotateTransformOperation(Rotation(FloatPoint3D(0, 0, 1), angle),
                               RotateAroundOrigin),
      m_originX(originX),
      m_originY(originY) {}

void RotateAroundOriginTransformOperation::apply(
    TransformationMatrix& transform,
    const FloatSize& boxSize) const {
  transform.translate(m_originX, m_originY);
  RotateTransformOperation::apply(transform, boxSize);
  transform.translate(-m_originX, -m_originY);
}

bool RotateAroundOriginTransformOperation::operator==(
    const TransformOperation& other) const {
  if (!isSameType(other))
    return false;
  const RotateAroundOriginTransformOperation& otherRotate =
      toRotateAroundOriginTransformOperation(other);
  const Rotation& otherRotation = otherRotate.m_rotation;
  return m_rotation.axis == otherRotation.axis &&
         m_rotation.angle == otherRotation.angle &&
         m_originX == otherRotate.m_originX &&
         m_originY == otherRotate.m_originY;
}

PassRefPtr<TransformOperation> RotateAroundOriginTransformOperation::blend(
    const TransformOperation* from,
    double progress,
    bool blendToIdentity) {
  if (from && !from->isSameType(*this))
    return this;
  if (blendToIdentity) {
    return RotateAroundOriginTransformOperation::create(
        angle() * (1 - progress), m_originX, m_originY);
  }
  if (!from) {
    return RotateAroundOriginTransformOperation::create(angle() * progress,
                                                        m_originX, m_originY);
  }
  const RotateAroundOriginTransformOperation& fromRotate =
      toRotateAroundOriginTransformOperation(*from);
  return RotateAroundOriginTransformOperation::create(
      blink::blend(fromRotate.angle(), angle(), progress),
      blink::blend(fromRotate.m_originX, m_originX, progress),
      blink::blend(fromRotate.m_originY, m_originY, progress));
}

PassRefPtr<TransformOperation> RotateAroundOriginTransformOperation::zoom(
    double factor) {
  return create(angle(), m_originX * factor, m_originY * factor);
}

}  // namespace blink
