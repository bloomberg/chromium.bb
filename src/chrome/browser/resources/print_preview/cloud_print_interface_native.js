// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cloudprint', function() {
  'use strict';

  /** @implements {cloudprint.CloudPrintInterface} */
  class CloudPrintInterfaceNative {
    constructor() {}

    /** @override */
    isCloudDestinationSearchInProgress() {}

    /** @override */
    getEventTarget() {}

    /** @override */
    search(opt_account, opt_origin) {}

    /** @override */
    invites(account) {}

    /** @override */
    processInvite(invitation, accept) {}

    /** @override */
    submit(destination, printTicket, documentTitle, data) {}

    /** @override */
    printer(printerId, origin, account) {}
  }

  // Export
  return {
    CloudPrintInterfaceNative: CloudPrintInterfaceNative,
  };
});
