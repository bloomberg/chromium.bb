// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/transform.h"

#include <ostream>
#include <limits>

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/point3.h"

namespace {

bool PointsAreNearlyEqual(const gfx::Point3f& lhs,
                          const gfx::Point3f& rhs) {
  float epsilon = 0.0001f;
  return lhs.SquaredDistanceTo(rhs) < epsilon;
}

TEST(XFormTest, Equality) {
  ui::Transform lhs, rhs, interpolated;
  rhs.matrix().set3x3(1, 2, 3,
                      4, 5, 6,
                      7, 8, 9);
  interpolated = lhs;
  for (int i = 0; i <= 100; ++i) {
    for (int row = 0; row < 4; ++row) {
      for (int col = 0; col < 4; ++col) {
        float a = lhs.matrix().get(row, col);
        float b = rhs.matrix().get(row, col);
        float t = i / 100.0f;
        interpolated.matrix().set(row, col, a + (b - a) * t);
      }
    }
    if (i == 100) {
      EXPECT_TRUE(rhs == interpolated);
    } else {
      EXPECT_TRUE(rhs != interpolated);
    }
  }
  lhs = ui::Transform();
  rhs = ui::Transform();
  for (int i = 1; i < 100; ++i) {
    lhs.SetTranslate(i, i);
    rhs.SetTranslate(-i, -i);
    EXPECT_TRUE(lhs != rhs);
    rhs.ConcatTranslate(2*i, 2*i);
    EXPECT_TRUE(lhs == rhs);
  }
}

TEST(XFormTest, ConcatTranslate) {
  static const struct TestCase {
    int x1;
    int y1;
    float tx;
    float ty;
    int x2;
    int y2;
  } test_cases[] = {
    { 0, 0, 10.0f, 20.0f, 10, 20 },
    { 0, 0, -10.0f, -20.0f, 0, 0 },
    { 0, 0, -10.0f, -20.0f, -10, -20 },
    { 0, 0,
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::quiet_NaN(),
      10, 20 },
  };

  ui::Transform xform;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    const TestCase& value = test_cases[i];
    xform.ConcatTranslate(value.tx, value.ty);
    gfx::Point3f p1(value.x1, value.y1, 0);
    gfx::Point3f p2(value.x2, value.y2, 0);
    xform.TransformPoint(p1);
    if (value.tx == value.tx &&
        value.ty == value.ty) {
      EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
    }
  }
}

TEST(XFormTest, ConcatScale) {
  static const struct TestCase {
    int before;
    float scale;
    int after;
  } test_cases[] = {
    { 1, 10.0f, 10 },
    { 1, .1f, 1 },
    { 1, 100.0f, 100 },
    { 1, -1.0f, -100 },
    { 1, std::numeric_limits<float>::quiet_NaN(), 1 }
  };

  ui::Transform xform;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    const TestCase& value = test_cases[i];
    xform.ConcatScale(value.scale, value.scale);
    gfx::Point3f p1(value.before, value.before, 0);
    gfx::Point3f p2(value.after, value.after, 0);
    xform.TransformPoint(p1);
    if (value.scale == value.scale) {
      EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
    }
  }
}

TEST(XFormTest, ConcatRotate) {
  static const struct TestCase {
    int x1;
    int y1;
    float degrees;
    int x2;
    int y2;
  } test_cases[] = {
    { 1, 0, 90.0f, 0, 1 },
    { 1, 0, -90.0f, 1, 0 },
    { 1, 0, 90.0f, 0, 1 },
    { 1, 0, 360.0f, 0, 1 },
    { 1, 0, 0.0f, 0, 1 },
    { 1, 0, std::numeric_limits<float>::quiet_NaN(), 1, 0 }
  };

  ui::Transform xform;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    const TestCase& value = test_cases[i];
    xform.ConcatRotate(value.degrees);
    gfx::Point3f p1(value.x1, value.y1, 0);
    gfx::Point3f p2(value.x2, value.y2, 0);
    xform.TransformPoint(p1);
    if (value.degrees == value.degrees) {
      EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
    }
  }
}

TEST(XFormTest, SetTranslate) {
  static const struct TestCase {
    int x1; int y1;
    float tx; float ty;
    int x2; int y2;
  } test_cases[] = {
    { 0, 0, 10.0f, 20.0f, 10, 20 },
    { 10, 20, 10.0f, 20.0f, 20, 40 },
    { 10, 20, 0.0f, 0.0f, 10, 20 },
    { 0, 0,
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::quiet_NaN(),
      0, 0 }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    const TestCase& value = test_cases[i];
    for (int k = 0; k < 3; ++k) {
      gfx::Point3f p0, p1, p2;
      ui::Transform xform;
      switch (k) {
      case 0:
        p1.SetPoint(value.x1, 0, 0);
        p2.SetPoint(value.x2, 0, 0);
        xform.SetTranslateX(value.tx);
        break;
      case 1:
        p1.SetPoint(0, value.y1, 0);
        p2.SetPoint(0, value.y2, 0);
        xform.SetTranslateY(value.ty);
        break;
      case 2:
        p1.SetPoint(value.x1, value.y1, 0);
        p2.SetPoint(value.x2, value.y2, 0);
        xform.SetTranslate(value.tx, value.ty);
        break;
      }
      p0 = p1;
      xform.TransformPoint(p1);
      if (value.tx == value.tx &&
          value.ty == value.ty) {
        EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
        xform.TransformPointReverse(p1);
        EXPECT_TRUE(PointsAreNearlyEqual(p1, p0));
      }
    }
  }
}

TEST(XFormTest, SetScale) {
  static const struct TestCase {
    int before;
    float s;
    int after;
  } test_cases[] = {
    { 1, 10.0f, 10 },
    { 1, 1.0f, 1 },
    { 1, 0.0f, 0 },
    { 0, 10.0f, 0 },
    { 1, std::numeric_limits<float>::quiet_NaN(), 0 },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    const TestCase& value = test_cases[i];
    for (int k = 0; k < 3; ++k) {
      gfx::Point3f p0, p1, p2;
      ui::Transform xform;
      switch (k) {
      case 0:
        p1.SetPoint(value.before, 0, 0);
        p2.SetPoint(value.after, 0, 0);
        xform.SetScaleX(value.s);
        break;
      case 1:
        p1.SetPoint(0, value.before, 0);
        p2.SetPoint(0, value.after, 0);
        xform.SetScaleY(value.s);
        break;
      case 2:
        p1.SetPoint(value.before, value.before, 0);
        p2.SetPoint(value.after, value.after, 0);
        xform.SetScale(value.s, value.s);
        break;
      }
      p0 = p1;
      xform.TransformPoint(p1);
      if (value.s == value.s) {
        EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
        if (value.s != 0.0f) {
          xform.TransformPointReverse(p1);
          EXPECT_TRUE(PointsAreNearlyEqual(p1, p0));
        }
      }
    }
  }
}

TEST(XFormTest, SetRotate) {
  static const struct SetRotateCase {
    int x;
    int y;
    float degree;
    int xprime;
    int yprime;
  } set_rotate_cases[] = {
    { 100, 0, 90.0f, 0, 100 },
    { 0, 0, 90.0f, 0, 0 },
    { 0, 100, 90.0f, -100, 0 },
    { 0, 1, -90.0f, 1, 0 },
    { 100, 0, 0.0f, 100, 0 },
    { 0, 0, 0.0f, 0, 0 },
    { 0, 0, std::numeric_limits<float>::quiet_NaN(), 0, 0 },
    { 100, 0, 360.0f, 100, 0 }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(set_rotate_cases); ++i) {
    const SetRotateCase& value = set_rotate_cases[i];
    gfx::Point3f p0;
    gfx::Point3f p1(value.x, value.y, 0);
    gfx::Point3f p2(value.xprime, value.yprime, 0);
    p0 = p1;
    ui::Transform xform;
    xform.SetRotate(value.degree);
    // just want to make sure that we don't crash in the case of NaN.
    if (value.degree == value.degree) {
      xform.TransformPoint(p1);
      EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
      xform.TransformPointReverse(p1);
      EXPECT_TRUE(PointsAreNearlyEqual(p1, p0));
    }
  }
}

// 2D tests
TEST(XFormTest, ConcatTranslate2D) {
  static const struct TestCase {
    int x1;
    int y1;
    float tx;
    float ty;
    int x2;
    int y2;
  } test_cases[] = {
    { 0, 0, 10.0f, 20.0f, 10, 20},
    { 0, 0, -10.0f, -20.0f, 0, 0},
    { 0, 0, -10.0f, -20.0f, -10, -20},
    { 0, 0,
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::quiet_NaN(),
      10, 20},
  };

  ui::Transform xform;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    const TestCase& value = test_cases[i];
    xform.ConcatTranslate(value.tx, value.ty);
    gfx::Point p1(value.x1, value.y1);
    gfx::Point p2(value.x2, value.y2);
    xform.TransformPoint(p1);
    if (value.tx == value.tx &&
        value.ty == value.ty) {
      EXPECT_EQ(p1.x(), p2.x());
      EXPECT_EQ(p1.y(), p2.y());
    }
  }
}

TEST(XFormTest, ConcatScale2D) {
  static const struct TestCase {
    int before;
    float scale;
    int after;
  } test_cases[] = {
    { 1, 10.0f, 10},
    { 1, .1f, 1},
    { 1, 100.0f, 100},
    { 1, -1.0f, -100},
    { 1, std::numeric_limits<float>::quiet_NaN(), 1}
  };

  ui::Transform xform;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    const TestCase& value = test_cases[i];
    xform.ConcatScale(value.scale, value.scale);
    gfx::Point p1(value.before, value.before);
    gfx::Point p2(value.after, value.after);
    xform.TransformPoint(p1);
    if (value.scale == value.scale) {
      EXPECT_EQ(p1.x(), p2.x());
      EXPECT_EQ(p1.y(), p2.y());
    }
  }
}

TEST(XFormTest, ConcatRotate2D) {
  static const struct TestCase {
    int x1;
    int y1;
    float degrees;
    int x2;
    int y2;
  } test_cases[] = {
    { 1, 0, 90.0f, 0, 1},
    { 1, 0, -90.0f, 1, 0},
    { 1, 0, 90.0f, 0, 1},
    { 1, 0, 360.0f, 0, 1},
    { 1, 0, 0.0f, 0, 1},
    { 1, 0, std::numeric_limits<float>::quiet_NaN(), 1, 0}
  };

  ui::Transform xform;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    const TestCase& value = test_cases[i];
    xform.ConcatRotate(value.degrees);
    gfx::Point p1(value.x1, value.y1);
    gfx::Point p2(value.x2, value.y2);
    xform.TransformPoint(p1);
    if (value.degrees == value.degrees) {
      EXPECT_EQ(p1.x(), p2.x());
      EXPECT_EQ(p1.y(), p2.y());
    }
  }
}

TEST(XFormTest, SetTranslate2D) {
  static const struct TestCase {
    int x1; int y1;
    float tx; float ty;
    int x2; int y2;
  } test_cases[] = {
    { 0, 0, 10.0f, 20.0f, 10, 20},
    { 10, 20, 10.0f, 20.0f, 20, 40},
    { 10, 20, 0.0f, 0.0f, 10, 20},
    { 0, 0,
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::quiet_NaN(),
      0, 0}
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    const TestCase& value = test_cases[i];
    for (int j = -1; j < 2; ++j) {
      for (int k = 0; k < 3; ++k) {
        float epsilon = 0.0001f;
        gfx::Point p0, p1, p2;
        ui::Transform xform;
        switch (k) {
        case 0:
          p1.SetPoint(value.x1, 0);
          p2.SetPoint(value.x2, 0);
          xform.SetTranslateX(value.tx + j * epsilon);
          break;
        case 1:
          p1.SetPoint(0, value.y1);
          p2.SetPoint(0, value.y2);
          xform.SetTranslateY(value.ty + j * epsilon);
          break;
        case 2:
          p1.SetPoint(value.x1, value.y1);
          p2.SetPoint(value.x2, value.y2);
          xform.SetTranslate(value.tx + j * epsilon,
                             value.ty + j * epsilon);
          break;
        }
        p0 = p1;
        xform.TransformPoint(p1);
        if (value.tx == value.tx &&
            value.ty == value.ty) {
          EXPECT_EQ(p1.x(), p2.x());
          EXPECT_EQ(p1.y(), p2.y());
          xform.TransformPointReverse(p1);
          EXPECT_EQ(p1.x(), p0.x());
          EXPECT_EQ(p1.y(), p0.y());
        }
      }
    }
  }
}

TEST(XFormTest, SetScale2D) {
  static const struct TestCase {
    int before;
    float s;
    int after;
  } test_cases[] = {
    { 1, 10.0f, 10},
    { 1, 1.0f, 1},
    { 1, 0.0f, 0},
    { 0, 10.0f, 0},
    { 1, std::numeric_limits<float>::quiet_NaN(), 0},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    const TestCase& value = test_cases[i];
    for (int j = -1; j < 2; ++j) {
      for (int k = 0; k < 3; ++k) {
        float epsilon = 0.0001f;
        gfx::Point p0, p1, p2;
        ui::Transform xform;
        switch (k) {
        case 0:
          p1.SetPoint(value.before, 0);
          p2.SetPoint(value.after, 0);
          xform.SetScaleX(value.s + j * epsilon);
          break;
        case 1:
          p1.SetPoint(0, value.before);
          p2.SetPoint(0, value.after);
          xform.SetScaleY(value.s + j * epsilon);
          break;
        case 2:
          p1.SetPoint(value.before,
                      value.before);
          p2.SetPoint(value.after,
                      value.after);
          xform.SetScale(value.s + j * epsilon,
                         value.s + j * epsilon);
          break;
        }
        p0 = p1;
        xform.TransformPoint(p1);
        if (value.s == value.s) {
          EXPECT_EQ(p1.x(), p2.x());
          EXPECT_EQ(p1.y(), p2.y());
          if (value.s != 0.0f) {
            xform.TransformPointReverse(p1);
            EXPECT_EQ(p1.x(), p0.x());
            EXPECT_EQ(p1.y(), p0.y());
          }
        }
      }
    }
  }
}

TEST(XFormTest, SetRotate2D) {
  static const struct SetRotateCase {
    int x;
    int y;
    float degree;
    int xprime;
    int yprime;
  } set_rotate_cases[] = {
    { 100, 0, 90.0f, 0, 100},
    { 0, 0, 90.0f, 0, 0},
    { 0, 100, 90.0f, -100, 0},
    { 0, 1, -90.0f, 1, 0},
    { 100, 0, 0.0f, 100, 0},
    { 0, 0, 0.0f, 0, 0},
    { 0, 0, std::numeric_limits<float>::quiet_NaN(), 0, 0},
    { 100, 0, 360.0f, 100, 0}
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(set_rotate_cases); ++i) {
    const SetRotateCase& value = set_rotate_cases[i];
    for (int j = 1; j >= -1; --j) {
      float epsilon = 0.1f;
      gfx::Point pt(value.x, value.y);
      ui::Transform xform;
      // should be invariant to small floating point errors.
      xform.SetRotate(value.degree + j * epsilon);
      // just want to make sure that we don't crash in the case of NaN.
      if (value.degree == value.degree) {
        xform.TransformPoint(pt);
        EXPECT_EQ(value.xprime, pt.x());
        EXPECT_EQ(value.yprime, pt.y());
        xform.TransformPointReverse(pt);
        EXPECT_EQ(pt.x(), value.x);
        EXPECT_EQ(pt.y(), value.y);
      }
    }
  }
}

} // namespace
