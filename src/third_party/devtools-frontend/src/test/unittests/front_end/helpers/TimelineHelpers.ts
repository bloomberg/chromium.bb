// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as SDK from '../../../../front_end/core/sdk/sdk.js';

export class FakeStorage extends SDK.TracingModel.BackingStorage {
  appendString() {
    throw new Error('Not implemented yet');
  }

  appendAccessibleString(): () => Promise<string|null> {
    throw new Error('Not implemented yet');
  }

  finishWriting() {
    throw new Error('Not implemented yet');
  }

  reset() {
    throw new Error('Not implemented yet');
  }
}
