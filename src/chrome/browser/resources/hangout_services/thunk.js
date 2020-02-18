// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessageExternal.addListener(function(
    message, sender, sendResponse) {
  function doSendResponse(value, errorString) {
    let error = null;
    if (errorString) {
      error = {};
      error['name'] = 'ComponentExtensionError';
      error['message'] = errorString;
    }

    const errorMessage = error || chrome.runtime.lastError;
    sendResponse({'value': value, 'error': errorMessage});
  }

  function getHost(url) {
    if (!url) {
      return '';
    }
    // Use the DOM to parse the URL. Since we don't add the anchor to
    // the page, this is the only reference to it and it will be
    // deleted once it's gone out of scope.
    const a = document.createElement('a');
    a.href = url;
    let origin = a.protocol + '//' + a.hostname;
    if (a.port != '') {
      origin = origin + ':' + a.port;
    }
    origin = origin + '/';
    return origin;
  }

  try {
    const requestInfo = {};

    // Set the tab ID. If it's passed in the message, use that.
    // Otherwise use the sender information.
    if (message['tabId']) {
      requestInfo['tabId'] = +message['tabId'];
      if (isNaN(requestInfo['tabId'])) {
        throw new Error(
            'Cannot convert tab ID string to integer: ' + message['tabId']);
      }
    } else if (sender.tab) {
      requestInfo['tabId'] = sender.tab.id;
    }

    if (sender.guestProcessId) {
      requestInfo['guestProcessId'] = sender.guestProcessId;
    }

    const method = message['method'];

    // Set the origin. If a URL is passed in the message, use that.
    // Otherwise use the sender information.
    let origin;
    if (message['winUrl']) {
      origin = getHost(message['winUrl']);
    } else {
      origin = getHost(sender.url);
    }

    if (method == 'cpu.getInfo') {
      chrome.system.cpu.getInfo(doSendResponse);
      return true;
    } else if (method == 'logging.setMetadata') {
      const metaData = message['metaData'];
      chrome.webrtcLoggingPrivate.setMetaData(
          requestInfo, origin, metaData, doSendResponse);
      return true;
    } else if (method == 'logging.start') {
      chrome.webrtcLoggingPrivate.start(requestInfo, origin, doSendResponse);
      return true;
    } else if (method == 'logging.uploadOnRenderClose') {
      chrome.webrtcLoggingPrivate.setUploadOnRenderClose(
          requestInfo, origin, true);
      doSendResponse();
      return false;
    } else if (method == 'logging.noUploadOnRenderClose') {
      chrome.webrtcLoggingPrivate.setUploadOnRenderClose(
          requestInfo, origin, false);
      doSendResponse();
      return false;
    } else if (method == 'logging.stop') {
      chrome.webrtcLoggingPrivate.stop(requestInfo, origin, doSendResponse);
      return true;
    } else if (method == 'logging.upload') {
      chrome.webrtcLoggingPrivate.upload(requestInfo, origin, doSendResponse);
      return true;
    } else if (method == 'logging.uploadStored') {
      const logId = message['logId'];
      chrome.webrtcLoggingPrivate.uploadStored(
          requestInfo, origin, logId, doSendResponse);
      return true;
    } else if (method == 'logging.stopAndUpload') {
      // Stop everything and upload. This is allowed to be called even if
      // logs have already been stopped or not started. Therefore, ignore
      // any errors along the way, but store them, so that if upload fails
      // they are all reported back.
      // Stop incoming and outgoing RTP dumps separately, otherwise
      // stopRtpDump will fail and not stop anything if either type has not
      // been started.
      const errors = [];
      chrome.webrtcLoggingPrivate.stopRtpDump(
          requestInfo, origin, true /* incoming */, false /* outgoing */,
          function() {
            appendLastErrorMessage(errors);
            chrome.webrtcLoggingPrivate.stopRtpDump(
                requestInfo, origin, false /* incoming */, true /* outgoing */,
                function() {
                  appendLastErrorMessage(errors);
                  chrome.webrtcLoggingPrivate.stop(
                      requestInfo, origin, function() {
                        appendLastErrorMessage(errors);
                        chrome.webrtcLoggingPrivate.upload(
                            requestInfo, origin, function(uploadValue) {
                              let errorMessage = null;
                              // If upload fails, report all previous errors.
                              // Otherwise, throw them away.
                              if (chrome.runtime.lastError !== undefined) {
                                appendLastErrorMessage(errors);
                                errorMessage = errors.join('; ');
                              }
                              doSendResponse(uploadValue, errorMessage);
                            });
                      });
                });
          });
      return true;
    } else if (method == 'logging.store') {
      const logId = message['logId'];
      chrome.webrtcLoggingPrivate.store(
          requestInfo, origin, logId, doSendResponse);
      return true;
    } else if (method == 'logging.discard') {
      chrome.webrtcLoggingPrivate.discard(requestInfo, origin, doSendResponse);
      return true;
    } else if (method == 'getSinks') {
      chrome.webrtcAudioPrivate.getSinks(doSendResponse);
      return true;
    } else if (method == 'getAssociatedSink') {
      const sourceId = message['sourceId'];
      chrome.webrtcAudioPrivate.getAssociatedSink(
          origin, sourceId, doSendResponse);
      return true;
    } else if (method == 'isExtensionEnabled') {
      // This method is necessary because there may be more than one
      // version of this extension, under different extension IDs. By
      // first calling this method on the extension ID, the client can
      // check if it's loaded; if it's not, the extension system will
      // call the callback with no arguments and set
      // chrome.runtime.lastError.
      doSendResponse();
      return false;
    } else if (method == 'getNaclArchitecture') {
      chrome.runtime.getPlatformInfo(function(obj) {
        doSendResponse(obj.nacl_arch);
      });
      return true;
    } else if (method == 'logging.startRtpDump') {
      const incoming = message['incoming'] || false;
      const outgoing = message['outgoing'] || false;
      chrome.webrtcLoggingPrivate.startRtpDump(
          requestInfo, origin, incoming, outgoing, doSendResponse);
      return true;
    } else if (method == 'logging.stopRtpDump') {
      const incoming = message['incoming'] || false;
      const outgoing = message['outgoing'] || false;
      chrome.webrtcLoggingPrivate.stopRtpDump(
          requestInfo, origin, incoming, outgoing, doSendResponse);
      return true;
    } else if (method == 'logging.startAudioDebugRecordings') {
      const seconds = message['seconds'] || 0;
      chrome.webrtcLoggingPrivate.startAudioDebugRecordings(
          requestInfo, origin, seconds, doSendResponse);
      return true;
    } else if (method == 'logging.stopAudioDebugRecordings') {
      chrome.webrtcLoggingPrivate.stopAudioDebugRecordings(
          requestInfo, origin, doSendResponse);
      return true;
    } else if (method == 'logging.startEventLogging') {
      const sessionId = message['sessionId'] || '';
      const maxLogSizeBytes = message['maxLogSizeBytes'] || 0;
      const outputPeriodMs = message['outputPeriodMs'] || -1;
      const webAppId = message['webAppId'] || 0;
      chrome.webrtcLoggingPrivate.startEventLogging(
          requestInfo, origin, sessionId, maxLogSizeBytes, outputPeriodMs,
          webAppId, doSendResponse);
      return true;
    } else if (method == 'getHardwarePlatformInfo') {
      chrome.enterprise.hardwarePlatform.getHardwarePlatformInfo(
          doSendResponse);
      return true;
    }

    throw new Error('Unknown method: ' + method);
  } catch (e) {
    doSendResponse(null, e.name + ': ' + e.message);
  }
});

// If Hangouts connects with a port named 'onSinksChangedListener', we
// will register a listener and send it a message {'eventName':
// 'onSinksChanged'} whenever the event fires.
function onSinksChangedPort(port) {
  function clientListener() {
    port.postMessage({'eventName': 'onSinksChanged'});
  }
  chrome.webrtcAudioPrivate.onSinksChanged.addListener(clientListener);

  port.onDisconnect.addListener(function() {
    chrome.webrtcAudioPrivate.onSinksChanged.removeListener(clientListener);
  });
}

// This is a one-time-use port for calling chooseDesktopMedia.  The page
// sends one message, identifying the requested source types, and the
// extension sends a single reply, with the user's selected streamId.  A port
// is used so that if the page is closed before that message is sent, the
// window picker dialog will be closed.
function onChooseDesktopMediaPort(port) {
  function sendResponse(streamId) {
    port.postMessage({'value': {'streamId': streamId}});
    port.disconnect();
  }

  port.onMessage.addListener(function(message) {
    const method = message['method'];
    if (method == 'chooseDesktopMedia') {
      const sources = message['sources'];
      let cancelId = null;
      const tab = port.sender.tab;
      if (tab) {
        // Per crbug.com/425344, in order to allow an <iframe> on a different
        // domain, to get desktop media, we need to set the tab.url to match
        // the <iframe>, even though it doesn't really load the new url.
        tab.url = port.sender.url;
        cancelId = chrome.desktopCapture.chooseDesktopMedia(
            sources, tab, sendResponse);
      } else {
        const requestInfo = {};
        requestInfo['guestProcessId'] = port.sender.guestProcessId || 0;
        requestInfo['guestRenderFrameId'] =
            port.sender.guestRenderFrameRoutingId || 0;
        cancelId = chrome.webrtcDesktopCapturePrivate.chooseDesktopMedia(
            sources, requestInfo, sendResponse);
      }
      port.onDisconnect.addListener(function() {
        // This method has no effect if called after the user has selected a
        // desktop media source, so it does not need to be conditional.
        if (tab) {
          chrome.desktopCapture.cancelChooseDesktopMedia(cancelId);
        } else {
          chrome.webrtcDesktopCapturePrivate.cancelChooseDesktopMedia(cancelId);
        }
      });
    }
  });
}

// A port for continuously reporting relevant CPU usage information to the page.
function onProcessCpu(port) {
  let tabPid = port.sender.guestProcessId || undefined;
  function processListener(processes) {
    if (tabPid == undefined) {
      // getProcessIdForTab sometimes fails, and does not call the callback.
      // (Tracked at https://crbug.com/368855.)
      // This call retries it on each process update until it succeeds.
      chrome.processes.getProcessIdForTab(port.sender.tab.id, function(x) {
        tabPid = x;
      });
      return;
    }
    const tabProcess = processes[tabPid];
    if (!tabProcess) {
      return;
    }

    let browserProcessCpu, gpuProcessCpu;
    for (const pid in processes) {
      const process = processes[pid];
      if (process.type == 'browser') {
        browserProcessCpu = process.cpu;
      } else if (process.type == 'gpu') {
        gpuProcessCpu = process.cpu;
      }
      if (browserProcessCpu && gpuProcessCpu) {
        break;
      }
    }

    port.postMessage({
      'browserCpuUsage': browserProcessCpu || 0,
      'gpuCpuUsage': gpuProcessCpu || 0,
      'tabCpuUsage': tabProcess.cpu,
      'tabNetworkUsage': tabProcess.network,
      'tabPrivateMemory': tabProcess.privateMemory,
      'tabJsMemoryAllocated': tabProcess.jsMemoryAllocated,
      'tabJsMemoryUsed': tabProcess.jsMemoryUsed
    });
  }

  chrome.processes.onUpdated.addListener(processListener);
  port.onDisconnect.addListener(function() {
    chrome.processes.onUpdated.removeListener(processListener);
  });
}

function appendLastErrorMessage(errors) {
  if (chrome.runtime.lastError !== undefined) {
    errors.push(chrome.runtime.lastError.message);
  }
}

chrome.runtime.onConnectExternal.addListener(function(port) {
  if (port.name == 'onSinksChangedListener') {
    onSinksChangedPort(port);
  } else if (port.name == 'chooseDesktopMedia') {
    onChooseDesktopMediaPort(port);
  } else if (port.name == 'processCpu') {
    onProcessCpu(port);
  } else {
    // Unknown port type.
    port.disconnect();
  }
});
