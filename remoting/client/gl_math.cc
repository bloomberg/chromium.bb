// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/gl_math.h"

#include <sstream>

namespace {

// | m0, m1, m2, |   | Scale_x  Skew_x   Offset_x |
// | m3, m4, m5, | = | Skew_y   Scale_y  Offset_y |
// | m6, m7, m8  |   | 0        0        1        |

const int kXScaleKey = 0;
const int kXSkewKey = 1;
const int kXOffsetKey = 2;

const int kYSkewKey = 3;
const int kYScaleKey = 4;
const int kYOffsetKey = 5;

const int kXOffsetTransposedKey = 6;
const int kYOffsetTransposedKey = 7;

}  // namespace

namespace remoting {

void NormalizeTransformationMatrix(int view_width,
                                   int view_height,
                                   int canvas_width,
                                   int canvas_height,
                                   std::array<float, 9>* matrix) {
  (*matrix)[kXScaleKey] = canvas_width * (*matrix)[kXScaleKey] / view_width;
  (*matrix)[kYScaleKey] = canvas_height * (*matrix)[kYScaleKey] / view_height;
  (*matrix)[kXOffsetKey] /= view_width;
  (*matrix)[kYOffsetKey] /= view_height;
}

void TransposeTransformationMatrix(std::array<float, 9>* matrix) {
  std::swap((*matrix)[kXOffsetKey], (*matrix)[kXOffsetTransposedKey]);
  std::swap((*matrix)[kYOffsetKey], (*matrix)[kYOffsetTransposedKey]);
  std::swap((*matrix)[kXSkewKey], (*matrix)[kYSkewKey]);
}

void FillRectangleVertexPositions(float left,
                                  float top,
                                  float width,
                                  float height,
                                  std::array<float, 8>* positions) {
  (*positions)[0] = left;
  (*positions)[1] = top;

  (*positions)[2] = left;
  (*positions)[3] = top + height;

  (*positions)[4] = left + width;
  (*positions)[5] = top;

  (*positions)[6] = left + width;
  (*positions)[7] = top + height;
}

std::string MatrixToString(const float* mat, int num_rows, int num_cols) {
  std::ostringstream outstream;
  outstream << "[\n";
  for (int i = 0; i < num_rows; i++) {
    for (int j = 0; j < num_cols; j++) {
      outstream << mat[i * num_cols + j] << ", ";
    }
    outstream << "\n";
  }
  outstream << "]";
  return outstream.str();
}

}  // namespace remoting
