// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

rootUi = new (function() {

this.onDomReady_ = function() {
  $('#js_loading_banner').hide();
  $('#tabs').tabs({activate: this.onTabChange_.bind(this)});
  $('#tabs').css('visibility', 'visible');

  // Initialize the status bar.
  $('#status_messages').mouseenter(function() {
    $('#status_bar').addClass('expanded');
    $('#status_messages').scrollTop($('#status_messages').height());
  });
  $('#status_messages').mouseleave(function() {
    $('#status_bar').removeClass('expanded');
  });
  $('#progress_bar').progressbar({value: 1});
};

this.showTab = function(tabId) {
  var index = $('#tabs-' + tabId).index();
  if (index > 0)
    $('#tabs').tabs('option', 'active', index - 1);
};

this.onTabChange_ = function(_, ui) {
  switch(ui.newPanel.attr('id').replace('tabs-', '')) {
    case 'ps':
      return processes.redraw();
    case 'prof':
      return profiler.redraw();
    case 'mm':
      return mmap.redraw();
    case 'nheap':
      return nheap.redraw();
    case 'settings':
      return settings.reload();
    case 'storage':
      return storage.reload();
  }
};

this.showDialog = function(content, title) {
  var dialog = $('#message_dialog');
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
  $('#message_dialog').dialog('close');
};

this.setProgress = function(value) {
  $('#progress_bar').progressbar('option', 'value', value);
  $('#progress_bar-label').text(value + '%' );
};

this.setStatusMessage = function(content) {
  $('#status_messages').text(content);
};

$(document).ready(this.onDomReady_.bind(this));

})();