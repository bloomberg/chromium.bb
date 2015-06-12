window.jsTestIsAsync = true;

var popupWindow = null;

var popupOpenCallback = null;

function popupOpenCallbackWrapper() {
    popupWindow.removeEventListener("didOpenPicker", popupOpenCallbackWrapper);
    setTimeout(popupOpenCallback, 0);
}

function waitUntilClosing(callback) {
    setTimeout(callback, 1);
}

function sendKey(input, keyName, ctrlKey, altKey) {
    var event = document.createEvent('KeyboardEvent');
    event.initKeyboardEvent('keydown', true, true, document.defaultView, keyName, 0, ctrlKey, altKey);
    input.dispatchEvent(event);
}

function rootWindow() {
    var currentWindow = window;
    while (currentWindow !== currentWindow.parent) {
        currentWindow = currentWindow.parent;
    }
    return currentWindow;
}

function openPicker(element, callback, errorCallback) {
    rootWindow().moveTo(window.screenX, window.screenY);
    element.offsetTop; // Force to lay out
    if (element.tagName === "SELECT") {
        sendKey(element, "Down", false, true);
    } else if (element.tagName === "INPUT") {
        if (element.type === "color") {
            element.focus();
            eventSender.keyDown(" ");
        } else {
            sendKey(element, "Down", false, true);
        }
    }
    popupWindow = window.internals.pagePopupWindow;
    if (typeof callback === "function" && popupWindow)
        setPopupOpenCallback(callback);
    else if (typeof errorCallback === "function" && !popupWindow)
        errorCallback();
}

function clickToOpenPicker(x, y, callback, errorCallback) {
    rootWindow().moveTo(window.screenX, window.screenY);
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    eventSender.mouseUp();
    popupWindow = window.internals.pagePopupWindow;
    if (typeof callback === "function" && popupWindow)
        setPopupOpenCallback(callback);
    else if (typeof errorCallback === "function" && !popupWindow)
        errorCallback();
}

function setPopupOpenCallback(callback) {
    console.assert(popupWindow);
    popupOpenCallback = (function(callback) {
        // We need to move the window to the top left of available space
        // because the window will move back to (0, 0) when the
        // ShellViewMsg_SetTestConfiguration IPC arrives.
        rootWindow().moveTo(window.screenX, window.screenY);
        callback();
    }).bind(this, callback);
    try {
        popupWindow.addEventListener("didOpenPicker", popupOpenCallbackWrapper, false);
    } catch(e) {
        debug(e.name);
    }
}
