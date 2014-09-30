window.jsTestIsAsync = true;

var popupWindow = null;

var popupOpenCallback = null;
function openPicker(input, callback) {
    if (window.internals)
        internals.setEnableMockPagePopup(true);
    input.offsetTop; // Force to lay out
    if (input.type === "color") {
        input.focus();
        eventSender.keyDown(" ");
    } else {
        sendKey(input, "Down", false, true);
    }
    popupWindow = document.getElementById('mock-page-popup').contentWindow;
    if (typeof callback === "function") {
        popupOpenCallback = callback;
        popupWindow.addEventListener("didOpenPicker", popupOpenCallbackWrapper, false);
    }
}

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

function openPickerWithoutMock(input, callback) {
    window.moveTo();
    input.offsetTop; // Force to lay out
    if (input.type === "color") {
        input.focus();
        eventSender.keyDown(" ");
    } else {
        sendKey(input, "Down", false, true);
    }
    popupWindow = window.internals.pagePopupWindow;
    if (typeof callback === "function") {
        popupOpenCallback = (function(callback) {
            // We need to move the window to the top left of available space
            // because the window will move back to (0, 0) when the
            // ShellViewMsg_SetTestConfiguration IPC arrives.
            window.moveTo();
            callback();
        }).bind(this, callback);
        popupWindow.addEventListener("didOpenPicker", popupOpenCallbackWrapper, false);
    }
}
