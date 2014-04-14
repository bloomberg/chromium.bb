// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Information about a particular status page. */
function StatusPageInfo(statusPageName, statusPageUrl) {
  this.date = '';
  this.inFlight = 0;
  this.jsonUrl = statusPageUrl + 'current?format=json';
  this.message = '';
  this.name = statusPageName;
  this.state = '';
  this.url = statusPageUrl;
}

/** Send and parse an asynchronous request to get a repo status JSON. */
StatusPageInfo.prototype.requestJson = function() {
  if (this.inFlight) return;

  this.inFlight++;
  gNumRequestsInFlight++;

  var statusPageInfo = this;
  var request = new XMLHttpRequest();
  request.open('GET', this.jsonUrl, true);
  request.onreadystatechange = function() {
    if (request.readyState == 4 && request.status == 200) {
      statusPageInfo.inFlight--;
      gNumRequestsInFlight--;

      var statusPageJson = JSON.parse(request.responseText);
      statusPageInfo.date = statusPageJson.date;
      statusPageInfo.message = statusPageJson.message;
      statusPageInfo.state = statusPageJson.general_state;
    }
  };
  request.send(null);
};

/** Creates HTML displaying the status. */
StatusPageInfo.prototype.createHtml = function() {
  var linkElement = document.createElement('a');
  linkElement.href = this.url;
  linkElement.innerHTML = this.name;

  var statusElement = document.createElement('li');
  statusElement.appendChild(linkElement);

  var dateElement = document.createElement('li');
  dateElement.innerHTML = this.date;

  var messageElement = document.createElement('li');
  messageElement.innerHTML = this.message;

  var boxElement = document.createElement('ul');
  boxElement.className = 'box ' + this.state;
  boxElement.appendChild(statusElement);
  boxElement.appendChild(dateElement);
  boxElement.appendChild(messageElement);
  return boxElement;
};
