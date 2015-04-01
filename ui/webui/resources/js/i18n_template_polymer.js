// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="i18n_template_no_process.js">

// Only start processing translations after polymer-ready has fired. This
// ensures that Elements in the shadow DOM are also translated. Note that this
// file should only be included in projects using Polymer. Otherwise, the
// "polymer-ready" event will never fire, and the page will not be translated.
window.addEventListener('polymer-ready', function() {
  i18nTemplate.process(document, loadTimeData);
});
