/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';
tr.exportTo('cp', () => {
  class SessionIdRequest extends cp.RequestBase {
    constructor(options) {
      super(options);
      this.method_ = 'POST';
      this.body_ = new FormData();
      this.body_.set('page_state', JSON.stringify(options.sessionState));
    }

    get url_() {
      return SessionIdRequest.URL;
    }

    postProcess_(json) {
      return json.sid;
    }
  }
  SessionIdRequest.URL = '/short_uri';

  return {SessionIdRequest};
});
