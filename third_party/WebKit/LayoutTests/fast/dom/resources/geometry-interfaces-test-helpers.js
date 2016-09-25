function assert_identity_2d_matrix(actual, description) {
  assert_matrix_equals(actual, {
      m11: 1, m12: 0, m13: 0, m14: 0,
      m21: 0, m22: 1, m23: 0, m24: 0,
      m31: 0, m32: 0, m33: 1, m34: 0,
      m41: 0, m42: 0, m43: 0, m44: 1,
      is2D: true, isIdentity: true
  }, description);
}

function assert_identity_3d_matrix(actual, description) {
  assert_matrix_equals(actual, {
      m11: 1, m12: 0, m13: 0, m14: 0,
      m21: 0, m22: 1, m23: 0, m24: 0,
      m31: 0, m32: 0, m33: 1, m34: 0,
      m41: 0, m42: 0, m43: 0, m44: 1,
      is2D: false, isIdentity: true
  }, description);
}

function assert_2d_matrix_equals(actual, expected, description) {
  if (Array.isArray(expected)) {
    assert_equals(6, expected.length);
    var full_expected = {
        m11: expected[0], m12: expected[1], m13: 0, m14: 0,
        m21: expected[2], m22: expected[3], m23: 0, m24: 0,
        m31: 0, m32: 0, m33: 1, m34: 0,
        m41: expected[4], m42: expected[5], m43: 0, m44: 1,
        is2D: true, isIdentity: false
    };
    assert_matrix_equals(actual, full_expected, description);
  } else {
    var full_expected = {
        m11: expected.m11, m12: expected.m12, m13: 0, m14: 0,
        m21: expected.m21, m22: expected.m22, m23: 0, m24: 0,
        m31: 0, m32: 0, m33: 1, m34: 0,
        m41: expected.m41, m42: expected.m42, m43: 0, m44: 1,
        is2D: true, isIdentity: false
    };
    assert_matrix_equals(actual, full_expected, description);
  }
}

function assert_3d_matrix_equals(actual, expected, description){
  if (Array.isArray(expected)) {
    assert_equals(16, expected.length);
    var full_expected = {
        m11: expected[0], m12: expected[1], m13: expected[2], m14: expected[3],
        m21: expected[4], m22: expected[5], m23: expected[6], m24: expected[7],
        m31: expected[8], m32: expected[9], m33: expected[10], m34: expected[11],
        m41: expected[12], m42: expected[13], m43: expected[14], m44: expected[15],
        is2D: false, isIdentity: false
    };
    assert_matrix_equals(actual, full_expected, description);
  } else {
    expected['is2D'] = false;
    expected['isIdentity'] = false;
    assert_matrix_equals(actual, expected, description);
  }
}

function assert_matrix_equals(actual, expected, description) {
  assert_equals(actual.isIdentity, expected.isIdentity, description);
  assert_equals(actual.is2D, expected.is2D, description);
  assert_equals(actual.m11, expected.m11, description);
  assert_equals(actual.m12, expected.m12, description);
  assert_equals(actual.m13, expected.m13, description);
  assert_equals(actual.m14, expected.m14, description);
  assert_equals(actual.m21, expected.m21, description);
  assert_equals(actual.m22, expected.m22, description);
  assert_equals(actual.m23, expected.m23, description);
  assert_equals(actual.m24, expected.m24, description);
  assert_equals(actual.m31, expected.m31, description);
  assert_equals(actual.m32, expected.m32, description);
  assert_equals(actual.m33, expected.m33, description);
  assert_equals(actual.m34, expected.m34, description);
  assert_equals(actual.m41, expected.m41, description);
  assert_equals(actual.m42, expected.m42, description);
  assert_equals(actual.m43, expected.m43, description);
  assert_equals(actual.m44, expected.m44, description);
  assert_equals(actual.m11, actual.a, description);
  assert_equals(actual.m12, actual.b, description);
  assert_equals(actual.m21, actual.c, description);
  assert_equals(actual.m22, actual.d, description);
  assert_equals(actual.m41, actual.e, description);
  assert_equals(actual.m42, actual.f, description);
}
