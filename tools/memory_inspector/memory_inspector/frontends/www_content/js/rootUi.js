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

this.showDialog = function(content, title) {
  var dialog = $("#message_dialog");
  title = title || '';
  if (dialog.length == 0) {
    dialog = $('<div id="message_dialog"/>');
    $('body').append(dialog);
  }
  if (typeof(content) == 'string')
    dialog.empty().text(content);
  else
    dialog.empty().append(content);  // Assume is a jQuery DOM object.

  dialog.dialog({modal: true, title: title, height:'auto', width:'auto'});
};

this.hideDialog = function() {
  $("#message_dialog").dialog('close');
};

$(document).ready(this.onDomReady_.bind(this));

})();