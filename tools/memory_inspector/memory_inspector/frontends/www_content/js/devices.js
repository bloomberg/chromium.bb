// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

devices = new (function() {

this.backends_ = [];  // ['Android', 'Linux']
this.devices_ = {};  // 'Android/a1b2' -> {.backend, .name, .id}
this.selDeviceUri_ = null;

this.onDomReady_ = function() {
  $('#refresh-devices').on('click', this.refresh.bind(this));
  $('#devices').on('change', this.onDeviceSelectionChange_.bind(this));
  this.refresh();
};

this.getSelectedURI = function() {
  return this.selDeviceUri_;
};

this.getAllBackends = function() {
  // Returns a list of the registered backends, e.g., ['Android', 'Linux'].
  return this.backends_;
};

this.getAllDevices = function() {
  // Returns a list of devices [{backend:'Android', name:'N7', id:'1234'}].
  return Object.keys(this.devices_).map(function(k) {
    return this.devices_[k];
  }, this);
};

this.refresh = function() {
  webservice.ajaxRequest('/devices', this.onDevicesAjaxResponse_.bind(this));
  webservice.ajaxRequest('/backends', this.onBackendsAjaxResponse_.bind(this));
};

this.onBackendsAjaxResponse_ = function(data) {
  if (!data || !data.length)
  {
    rootUi.showDialog('No backends detected! Memory Inspector looks terribly' +
                       ' broken. Please file a bug');
  }
  this.backends_ = data;
};

this.onDevicesAjaxResponse_ = function(data) {
  var devList = $('#devices');
  devList.empty();
  this.devices_ = {};
  data.forEach(function(device) {
    var deviceUri = device.backend + '/' + device.id;
    var deviceFullTime = device.backend + ' : ' +
                         device.name + ' [' + device.id + ']';
    devList.append($('<option/>').val(deviceUri).text(deviceFullTime));
    this.devices_[deviceUri] = device;
  }, this);

  if (data.length > 0) {
    this.onDeviceSelectionChange_();  // Start monitoring the first device.
  } else {
    rootUi.showDialog('No devices could be detected. Check the settings tab ' +
                      'to ensure that the adb path is properly configured.');
  }
};

this.onDeviceSelectionChange_ = function() {
  this.selDeviceUri_ = $('#devices').val();
  if (!this.selDeviceUri_)
    return;

  // Initialize device and start processes / OS stats.
  webservice.ajaxRequest('/initialize/' + this.selDeviceUri_,
                         this.onDeviceInitializationComplete_.bind(this));
};

this.onDeviceInitializationComplete_ = function() {
  processes.startPsTable();
  processes.startDeviceStats();
};

$(document).ready(this.onDomReady_.bind(this));

})();