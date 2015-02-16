"use strict";
// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var global = {
    argumentsReceived: false,
    params: null,
    picker: null
};

/**
 * @param {Event} event
 */
function handleMessage(event) {
    window.removeEventListener("message", handleMessage, false);
    initialize(JSON.parse(event.data));
    global.argumentsReceived = true;
}

/**
 * @param {!Object} args
 */
function initialize(args) {
    global.params = args;
    var main = $("main");
    main.innerHTML = "";
    global.picker = new ListPicker(main, args);
}

function handleArgumentsTimeout() {
    if (global.argumentsReceived)
        return;
    initialize({});
}

/**
 * @constructor
 * @param {!Element} element
 * @param {!Object} config
 */
function ListPicker(element, config) {
    Picker.call(this, element, config);
    this._selectElement = createElement("select");
    this._element.appendChild(this._selectElement);
    this._layout();
    this._selectElement.focus();
    this._selectElement.addEventListener("mouseover", this._handleMouseOver.bind(this), false);
    this._selectElement.addEventListener("mouseup", this._handleMouseUp.bind(this), false);
    this._selectElement.addEventListener("keydown", this._handleKeyDown.bind(this), false);
    this._selectElement.addEventListener("change", this._handleChange.bind(this), false);
    window.addEventListener("message", this._handleWindowMessage.bind(this), false);
    window.addEventListener("mousemove", this._handleWindowMouseMove.bind(this), false);
    this.lastMousePositionX = Infinity;
    this.lastMousePositionY = Infinity;

    // Not sure why but we need to delay this call so that offsetHeight is
    // accurate. We wait for the window to resize to work around an issue
    // of immediate resize requests getting mixed up.
    this._handleWindowDidHideBound = this._handleWindowDidHide.bind(this);
    window.addEventListener("didHide", this._handleWindowDidHideBound, false);
    hideWindow();
}
ListPicker.prototype = Object.create(Picker.prototype);

ListPicker.prototype._handleWindowDidHide = function() {
    this._fixWindowSize();
    var selectedOption = this._selectElement.options[this._selectElement.selectedIndex];
    selectedOption.scrollIntoView(false);
    window.removeEventListener("didHide", this._handleWindowDidHideBound, false);
};

ListPicker.prototype._handleWindowMessage = function(event) {
    eval(event.data);
    if (window.updateData.type === "update")
        this._update(window.updateData);
    delete window.updateData;
};

ListPicker.prototype._handleWindowMouseMove = function (event) {
    this.lastMousePositionX = event.clientX;
    this.lastMousePositionY = event.clientY;
};

ListPicker.prototype._handleMouseOver = function(event) {
    if (event.toElement.tagName !== "OPTION")
        return;
    var savedScrollTop = this._selectElement.scrollTop;
    event.toElement.selected = true;
    this._selectElement.scrollTop = savedScrollTop;
};

ListPicker.prototype._handleMouseUp = function(event) {
    if (event.target.tagName !== "OPTION")
        return;
    window.pagePopupController.setValueAndClosePopup(0, this._selectElement.value);
};

ListPicker.prototype._handleChange = function(event) {
    window.pagePopupController.setValue(this._selectElement.value);
};

ListPicker.prototype._handleKeyDown = function(event) {
    var key = event.keyIdentifier;
    if (key === "U+001B") { // ESC
        window.pagePopupController.closePopup();
        event.preventDefault();
    } else if (key === "U+0009" /* TAB */ || key === "Enter") {
        window.pagePopupController.setValueAndClosePopup(0, this._selectElement.value);
        event.preventDefault();
    } else if (event.altKey && (key === "Down" || key === "Up")) {
        // We need to add a delay here because, if we do it immediately the key
        // press event will be handled by HTMLSelectElement and this popup will
        // be reopened.
        setTimeout(function () {
            window.pagePopupController.closePopup();
        }, 0);
        event.preventDefault();
    } else {
        // After a key press, we need to call setValue to reflect the selection
        // to the owner element. We can handle most cases with the change
        // event. But we need to call setValue even when the selection hasn't
        // changed. So we call it here too. setValue will be called twice for
        // some key presses but it won't matter.
        window.pagePopupController.setValue(this._selectElement.value);
    }
};

ListPicker.prototype._fixWindowSize = function() {
    this._selectElement.style.height = "";
    this._selectElement.size = 20;
    var maxHeight = this._selectElement.offsetHeight;
    this._selectElement.style.height = "0";
    var heightOutsideOfContent = this._selectElement.offsetHeight - this._selectElement.clientHeight;
    var desiredWindowHeight = this._selectElement.scrollHeight + heightOutsideOfContent;
    this._selectElement.style.height = desiredWindowHeight + "px";
    // scrollHeight returns floored value so we needed this check.
    if (this._hasVerticalScrollbar())
        desiredWindowHeight += 1;
    if (desiredWindowHeight > maxHeight)
        desiredWindowHeight = maxHeight;
    var desiredWindowWidth = Math.max(this._config.anchorRectInScreen.width, this._selectElement.offsetWidth);
    var windowRect = adjustWindowRect(desiredWindowWidth, desiredWindowHeight, this._selectElement.offsetWidth, 0);
    this._selectElement.style.width = windowRect.width + "px";
    this._selectElement.style.height = windowRect.height + "px";
    this._element.style.height = windowRect.height + "px";
    setWindowRect(windowRect);
};

ListPicker.prototype._hasVerticalScrollbar = function() {
    return this._selectElement.scrollWidth > this._selectElement.clientWidth;
};

ListPicker.prototype._listItemCount = function() {
    return this._selectElement.querySelectorAll("option,optgroup,hr").length;
};

ListPicker.prototype._layout = function() {
    for (var i = 0; i < this._config.children.length; ++i) {
        this._selectElement.appendChild(this._createItemElement(this._config.children[i]));
    }
    this._selectElement.value = this._config.selectedIndex;
};

ListPicker.prototype._update = function(data) {
    this._config.children = data.children;
    var oldValue = this._selectElement.value;
    while (this._selectElement.firstChild) {
        this._selectElement.removeChild(this._selectElement.firstChild);
    }
    for (var i = 0; i < this._config.children.length; ++i) {
        this._selectElement.appendChild(this._createItemElement(this._config.children[i]));
    }
    this._selectElement.value = this._config.selectedIndex;
    var elementUnderMouse = document.elementFromPoint(this.lastMousePositionX, this.lastMousePositionY);
    var optionUnderMouse = elementUnderMouse && elementUnderMouse.closest("option");
    if (optionUnderMouse)
        optionUnderMouse.selected = true;
    else
        this._selectElement.value = oldValue;
    this._fixWindowSize();
};

ListPicker.prototype._createItemElement = function(config) {
    if (config.type === "option") {
        var option = createElement("option");
        option.appendChild(document.createTextNode(config.label));
        option.value = config.value;
        option.title = config.title;
        option.disabled = config.disabled;
        option.setAttribute("aria-label", config.ariaLabel);
        this._applyItemStyle(option, config.style);
        this._selectElement.appendChild(option);
        return option;
    } else if (config.type === "optgroup") {
        var optgroup = createElement("optgroup");
        optgroup.label = config.label;
        optgroup.title = config.title;
        optgroup.disabled = config.disabled;
        optgroup.setAttribute("aria-label", config.ariaLabel);
        this._applyItemStyle(optgroup, config.style);
        for (var i = 0; i < config.children.length; ++i) {
            optgroup.appendChild(this._createItemElement(config.children[i]));
        }
        this._selectElement.appendChild(optgroup);
        return optgroup;
    } else if (config.type === "separator") {
        var hr = createElement("hr");
        hr.title = config.title;
        hr.disabled = config.disabled;
        hr.setAttribute("aria-label", config.ariaLabel);
        this._applyItemStyle(hr, config.style);
        return hr;
    }
};

ListPicker.prototype._applyItemStyle = function(element, styleConfig) {
    element.style.color = styleConfig.color;
    element.style.backgroundColor = styleConfig.backgroundColor;
    element.style.fontSize = styleConfig.fontSize + "px";
    element.style.fontWeight = styleConfig.fontWeight;
    element.style.fontFamily = styleConfig.fontFamily.join(",");
    element.style.visibility = styleConfig.visibility;
    element.style.display = styleConfig.display;
    element.style.direction = styleConfig.direction;
    element.style.unicodeBidi = styleConfig.unicodeBidi;
    element.style.zoom = styleConfig.zoom;
};

if (window.dialogArguments) {
    initialize(dialogArguments);
} else {
    window.addEventListener("message", handleMessage, false);
    window.setTimeout(handleArgumentsTimeout, 1000);
}
