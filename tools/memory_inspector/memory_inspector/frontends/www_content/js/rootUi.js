// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

rootUi = new (function() {

this.onDomReady_ = function() {
  $('#js_loading_banner').hide();
  $('#tabs').tabs();
  $('#tabs').css('visibility', 'visible');

  // Initialize the status bar.
  var statusBar = $('#statusBar');
  var statusMessages = $('#statusMessages');
  statusMessages.mouseenter(function() {
    statusBar.addClass('expanded');
    statusMessages.scrollTop(statusMessages.height());
  });
  statusMessages.mouseleave(function() {
    statusBar.removeClass('expanded');
  });

  var progressBar = $('#progressBar');
  var progressLabel = $('#progressBar-label');
  progressBar.progressbar({
    value: 1,
    change: function() {
      progressLabel.text(progressBar.progressbar('value') + '%' );
    }
  });
};

this.showTab = function(tabId) {
  var index = $('#tabs-' + tabId).index();
  if (index > 0)
    $('#tabs').tabs('option', 'active', index - 1);
};

$(document).ready(this.onDomReady_.bind(this));

})();