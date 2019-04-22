// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.addEventListener('load', function init() {
  const extensionView = document.querySelector('extensionview');

  /**
   * @param {string} str
   * @return {!Array<string>}
   */
  const splitUrlOnHash = function(str) {
    str = str || '';
    const pos = str.indexOf('#');
    return (pos !== -1) ? [str.substr(0, pos), str.substr(pos + 1)] : [str, ''];
  };

  new MutationObserver(function() {
    const newHash = splitUrlOnHash(extensionView.getAttribute('src'))[1];
    const oldHash = window.location.hash.substr(1);
    if (newHash !== oldHash) {
      window.location.hash = newHash;
    }
  }).observe(extensionView, {attributes: true});

  window.addEventListener('hashchange', function() {
    const newHash = window.location.hash.substr(1);
    const extensionViewSrcParts =
        splitUrlOnHash(extensionView.getAttribute('src'));
    if (newHash !== extensionViewSrcParts[1] && newHash.startsWith('offers')) {
      extensionView.load(extensionViewSrcParts[0] + '#' + newHash);
    }
  });

  extensionView.load(
      'chrome-extension://' + loadTimeData.getString('extensionId') +
          '/cast_setup/index.html#offers');
});
