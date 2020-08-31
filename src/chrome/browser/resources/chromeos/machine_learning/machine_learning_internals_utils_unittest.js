// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var MachineLearningInternalsUnitTests = class extends testing.Test {
  /** @override */
  get extraLibraries() {
    return [
      '../../../../../ui/webui/resources/js/cr.js',
      'machine_learning_internals_utils.js',
    ];
  }
};

TEST_F('MachineLearningInternalsUnitTests', 'enumToString', function() {
  const TestEnum = {
    OK: 1,
    FAIL: 2,
  };
  assertEquals(
      'OK', machine_learning_internals.utils.enumToString(1, TestEnum));
  assertEquals(
      'FAIL', machine_learning_internals.utils.enumToString(2, TestEnum));
  assertThrows(() => {
    machine_learning_internals.utils.enumToString(3, TestEnum);
  }, 'Unknown enumValue: 3');
});

TEST_F('MachineLearningInternalsUnitTests', 'makeTensor', function() {
  const myArray = [1.0, 2.0, 3.0];
  const myTensor =
      machine_learning_internals.utils.makeTensor(myArray, [myArray.length]);
  assertEquals(myArray, myTensor.data.floatList.value);
  assertEquals(myArray.length, myTensor.shape.value[0]);
  assertThrows(() => {
    machine_learning_internals.utils.makeTensor(myArray, [2, 3]);
  }, `valueList.length ${myArray.length} != expectedLength 6`);
});
