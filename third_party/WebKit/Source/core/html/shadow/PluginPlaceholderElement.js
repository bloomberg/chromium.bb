// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

installClass('PluginPlaceholderElement', function(PluginPlaceholderElementPrototype) {
    // FIXME: Load this from a .css file.
    var styleSource =
        '#plugin-placeholder {' +
        '    width: 100%;' +
        '    height: 100%;' +
        '    overflow: hidden;' +
        '    display: flex;' +
        '    align-items: center;' +
        '    background: gray;' +
        '    font: 12px -webkit-control;' +
        '}' +
        '#plugin-placeholder-content {' +
        '    text-align: center;' +
        '    margin: auto;' +
        '}';

    PluginPlaceholderElementPrototype.createdCallback = function() {
        this.id = 'plugin-placeholder';

        var styleElement = document.createElement('style');
        styleElement.textContent = styleSource;

        var contentElement = document.createElement('div');
        contentElement.id = 'plugin-placeholder-content';

        var messageElement = document.createElement('div');
        messageElement.id = 'plugin-placeholder-message';

        contentElement.appendChild(messageElement);
        this.appendChild(styleElement);
        this.appendChild(contentElement);

        this.messageElement = messageElement;
    };

    Object.defineProperty(PluginPlaceholderElementPrototype, 'message', {
        get: function() { return this.messageElement.textContent; },
        set: function(message) { this.messageElement.textContent = message; },
    });
});
