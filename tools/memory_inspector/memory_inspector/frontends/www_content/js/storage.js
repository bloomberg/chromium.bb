// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

storage = new (function() {

this.table_ = null;
this.tableData_ = null;

this.onDomReady_ = function() {
  // Initialize the toolbar.
  $('#storage-profile-mmaps').button({icons:{primary: 'ui-icon-image'}})
      .click(this.profileMmapForSelectedSnapshots.bind(this));
  $('#storage-dump-mmaps').button({icons:{primary: 'ui-icon-calculator'}})
      .click(this.dumpMmapForSelectedSnapshot_.bind(this));
  $('#storage-profile-native').button({icons:{primary: 'ui-icon-image'}})
      .click(this.profileNativeForSelectedSnapshots.bind(this));

  // Create the table.
  this.table_ = new google.visualization.Table($('#storage-table')[0]);
};

this.reload = function() {
  webservice.ajaxRequest('/storage/list', this.onListAjaxResponse_.bind(this));
}

this.onListAjaxResponse_ = function(data) {
  this.tableData_ = new google.visualization.DataTable(data);
  this.redraw();
};

this.profileMmapForSelectedSnapshots = function() {
  // Generates a mmap profile for the selected snapshots.
  var sel = this.table_.getSelection();
  if (!sel.length || !this.tableData_)
    return;
  var archiveName = null;
  var snapshots = [];

  for (var i = 0; i < sel.length; ++i) {
    var row = sel[i].row;
    var curArchive = this.tableData_.getValue(row, 0);
    if (archiveName && curArchive != archiveName){
      alert('All the selected snapshots must belong to the same archive!');
      return;
    }
    archiveName = curArchive;
    snapshots.push(this.tableData_.getValue(row, 1));
  }
  profiler.profileArchivedMmaps(archiveName, snapshots);
  rootUi.showTab('prof');
};

this.dumpMmapForSelectedSnapshot_ = function() {
  var sel = this.table_.getSelection();
  if (sel.length != 1) {
    alert('Please select only one snapshot.')
    return;
  }

  var row = sel[0].row;
  mmap.dumpMmapsFromStorage(this.tableData_.getValue(row, 0),
                            this.tableData_.getValue(row, 1))
  rootUi.showTab('mm');
};

this.profileNativeForSelectedSnapshots = function() {

};

this.redraw = function() {
  if (!this.tableData_)
    return;
  this.table_.draw(this.tableData_);
};

$(document).ready(this.onDomReady_.bind(this));

})();