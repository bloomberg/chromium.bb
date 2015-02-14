// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * Note that this needs to be a function because it gets expanded at
 * compile-time into remoting.ClientSession.Capability enums and these
 * are not guaranteed to be present yet when this file is loaded.
 * @return {Array<remoting.ClientSession.Capability>}
 */
remoting.app_capabilities = function() {
  return ['APPLICATION_CAPABILITIES'];
}
