// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var FilesMetadataBox = Polymer({
  is: 'files-metadata-entry',

  properties: {
    key: {
      type: String,
      reflectToAttribtue: true,
    },

    // If |value| is empty, the entire entry will be hidden.
    value: {
      type: String,
      reflectToAttribtue: true,
    },

    loading: {
      type: Boolean,
      reflectToAttribtue: true,
      value: false,
    },
  },

});
