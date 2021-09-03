// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {assert} = chai;

import * as SDK from '../../../../../front_end/core/sdk/sdk.js';

describe('TracingModel', () => {
  it('is able to determine if a phase is a nestable async phase', () => {
    assert.isTrue(
        SDK.TracingModel.TracingModel.isNestableAsyncPhase('b'), '\'b\' should be considered a nestable async phase');
    assert.isTrue(
        SDK.TracingModel.TracingModel.isNestableAsyncPhase('e'), '\'e\' should be considered a nestable async phase');
    assert.isTrue(
        SDK.TracingModel.TracingModel.isNestableAsyncPhase('n'), '\'n\' should be considered a nestable async phase');
  });

  it('is able to determine if a phase is not a nestable async phase', () => {
    assert.isFalse(
        SDK.TracingModel.TracingModel.isNestableAsyncPhase('a'),
        '\'a\' should not be considered a nestable async phase');
    assert.isFalse(
        SDK.TracingModel.TracingModel.isNestableAsyncPhase('f'),
        '\'f\' should not be considered a nestable async phase');
    assert.isFalse(
        SDK.TracingModel.TracingModel.isNestableAsyncPhase('m'),
        '\'m\' should not be considered a nestable async phase');
  });

  // TODO continue writing tests here or use another describe block
});
