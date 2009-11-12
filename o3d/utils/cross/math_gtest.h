/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This file declares the some helper for gtest for the math library used by
// O3D.

#ifndef O3D_UTILS_CROSS_MATH_GTEST_H_
#define O3D_UTILS_CROSS_MATH_GTEST_H_

#include <ostream>  // NOLINT

namespace Vectormath {
namespace Aos {
class Vector4;
class Matrix4;

bool operator==(const Vector4& left, const Vector4& right);
bool operator!=(const Vector4& left, const Vector4& right);
bool operator==(const Matrix4& left, const Matrix4& right);
bool operator!=(const Matrix4& left, const Matrix4& right);
std::ostream& operator<<(std::ostream& stream, const Vector4& value);
std::ostream& operator<<(std::ostream& stream, const Matrix4& value);
}
}

namespace o3d {
class Float2;
class Float3;
class Float4;

bool operator==(const Float2& left, const Float2& right);
bool operator!=(const Float2& left, const Float2& right);
bool operator==(const Float3& left, const Float3& right);
bool operator!=(const Float3& left, const Float3& right);
bool operator==(const Float4& left, const Float4& right);
bool operator!=(const Float4& left, const Float4& right);
std::ostream& operator<<(std::ostream& stream, const Float2& value);
std::ostream& operator<<(std::ostream& stream, const Float3& value);
std::ostream& operator<<(std::ostream& stream, const Float4& value);
}

#endif  // O3D_UTILS_CROSS_MATH_GTEST_H_

