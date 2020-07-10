// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for objects sent from C++ to JS for chrome://downloads.
 * @externs
 */

// eslint-disable-next-line no-var
var downloads = {};

/**
 * The type of the download object. The definition is based on the Data struct
 * in downloads.mojom.
 * @typedef {downloads.mojom.Data | {hideDate: boolean}}
 */
downloads.Data;
