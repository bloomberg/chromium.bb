// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.aidl;

interface IProfile {
  void destroy() = 0;

  void clearBrowsingData(in int[] dataTypes, in IObjectWrapper completionCallback) = 1;

  String getPath() = 2;
}
