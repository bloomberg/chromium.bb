// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pass the query string params from the trusted URL.
const mainUrl = 'chrome-untrusted://eche-app/untrusted_index.html';
// The window.location.hash starts with # and window.location.search starts
// with ?, uses substring(1) to remove them.
let urlParams = window.location.hash ?
    new URLSearchParams(window.location.hash.substring(1)) :
    new URLSearchParams(window.location.search.substring(1));
urlParams = urlParams.toString();
document.getElementsByTagName('iframe')[0].src = mainUrl;
if (urlParams) {
    document.getElementsByTagName('iframe')[0].src = mainUrl + '?' + urlParams;
}

// Returns a remote for SignalingMessageExchanger interface which sends messages
// to the browser.
const signalMessageExchanger =
    chromeos.echeApp.mojom.SignalingMessageExchanger.getRemote();
// An object which receives request messages for the SignalingMessageObserver
// mojom interface and dispatches them as callbacks.
const signalingMessageObserverRouter =
    new chromeos.echeApp.mojom.SignalingMessageObserverCallbackRouter();
// Set up a message pipe to talk to the browser process.
signalMessageExchanger.setSignalingMessageObserver(
    signalingMessageObserverRouter.$.bindNewPipeAndPassRemote());
// Returns a remote for SystemInfoProvider interface which gets system info
// from the browser.
const systemInfo = chromeos.echeApp.mojom.SystemInfoProvider.getRemote();
// Returns a remote for UidGenerator interface which gets an uid from the
// browser.
const uidGenerator = chromeos.echeApp.mojom.UidGenerator.getRemote();
// An object which receives request messages for the SystemInfoObserver
// mojom interface and dispatches them as callbacks.
const systemInfoObserverRouter =
    new chromeos.echeApp.mojom.SystemInfoObserverCallbackRouter();
// Set up a message pipe to the browser process to monitor screen state.
systemInfo.setSystemInfoObserver(
    systemInfoObserverRouter.$.bindNewPipeAndPassRemote());

const notificationGenerator =
    chromeos.echeApp.mojom.NotificationGenerator.getRemote();

/**
 * A pipe through which we can send messages to the guest frame.
 * Use an undefined `target` to find the <iframe> automatically.
 * Do not rethrow errors, since handlers installed here are expected to
 * throw exceptions that are handled on the other side of the pipe. And
 * nothing `awaits` async callHandlerForMessageType_(), so they will always
 * be reported as `unhandledrejection` and trigger a crash report.
 */
 const guestMessagePipe =
 new MessagePipe('chrome-untrusted://eche-app',
                 /*target=*/ undefined,
                 /*rethrow_errors=*/ false);

// Register bi-directional SEND_SIGNAL pipes.
guestMessagePipe.registerHandler(Message.SEND_SIGNAL, async (signal) => {
signalMessageExchanger.sendSignalingMessage(signal);
})

signalingMessageObserverRouter.onReceivedSignalingMessage.addListener(
 (signal) => {
   guestMessagePipe.sendMessage(
       Message.SEND_SIGNAL, {/** @type {Uint8Array} */signal});
 });

// Register TEAR_DOWN_SIGNAL pipes.
guestMessagePipe.registerHandler(Message.TEAR_DOWN_SIGNAL, async () => {
    signalMessageExchanger.tearDownSignaling();
})

// window.close() doesn't work from the iframe.
guestMessagePipe.registerHandler(Message.CLOSE_WINDOW, async () => {
    window.close();
})

// Register GET_SYSTEM_INFO pipes for wrapping getSystemInfo async api call.
guestMessagePipe.registerHandler(Message.GET_SYSTEM_INFO, async () => {
    return /** @type {!SystemInfo} */ (await systemInfo.getSystemInfo());
})

// Register GET_UID pipes for wrapping getUid async api call.
guestMessagePipe.registerHandler(Message.GET_UID, async () => {
    return /** @type {!UidInfo} */ (await uidGenerator.getUid());
})

// Add Screen Backlight state listener and send state via pipes.
systemInfoObserverRouter.onScreenBacklightStateChanged.addListener(
    (state) => {
      guestMessagePipe.sendMessage(
          Message.SCREEN_BACKLIGHT_STATE, {/** @type {number} */state});
});

// Add tablet mode listener and send result via pipes.
systemInfoObserverRouter.onReceivedTabletModeChanged.addListener(
    (isTabletMode) => {
      guestMessagePipe.sendMessage(
          Message.TABLET_MODE, {/** @type {boolean} */isTabletMode});
});

guestMessagePipe.registerHandler(Message.SHOW_NOTIFICATION, async (message) => {
  // The C++ layer uses std::u16string, which use 16 bit characters. JS
  // strings support either 8 or 16 bit characters, and must be converted
  // to an array of 16 bit character codes that match std::u16string.
  const titleArray = {data: Array.from(message.title, c => c.charCodeAt())};
  const messageArray = {data: Array.from(message.message, c => c.charCodeAt())};
  notificationGenerator.showNotification(
      titleArray, messageArray, message.notificationType);
})

// We can't access hash change event inside iframe so parse the notification
// info from the anchor part of the url when hash is changed and send them to
// untrusted section via message pipes.
function locationHashChanged() {
    let urlParams = window.location.hash ?
    new URLSearchParams(window.location.hash.substring(1)) :
    new URLSearchParams(window.location.search.substring(1));
    const notificationId = urlParams.get('notification_id');
    const packageName = urlParams.get('package_name');
    const timestamp = urlParams.get('timestamp');
    const notificationInfo = /** @type {!NotificationInfo} */(
        {notificationId, packageName, timestamp});
    guestMessagePipe.sendMessage(Message.NOTIFICATION_INFO, notificationInfo);
}

window.onhashchange = locationHashChanged;
