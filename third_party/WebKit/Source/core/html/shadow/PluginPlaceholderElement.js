// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

installClass('PluginPlaceholderElement', function(PluginPlaceholderElementPrototype) {
    PluginPlaceholderElementPrototype.createdCallback = function() {
        // FIXME: Move style out of script and into CSS.

        this.id = 'plugin-placeholder';
        this.style.width = '100%';
        this.style.height = '100%';
        this.style.overflow = 'hidden';
        this.style.display = 'flex';
        this.style.alignItems = 'center';
        this.style.backgroundColor = 'gray';
        this.style.font = '12px -webkit-control';

        var contentElement = document.createElement('div');
        contentElement.id = 'plugin-placeholder-content';
        contentElement.style.textAlign = 'center';
        contentElement.style.margin = 'auto';

        var messageElement = document.createElement('div');
        messageElement.id = 'plugin-placeholder-message';

        contentElement.appendChild(messageElement);
        this.appendChild(contentElement);

        this.messageElement = messageElement;
    };

    Object.defineProperty(PluginPlaceholderElementPrototype, 'message', {
        get: function() { return this.messageElement.textContent; },
        set: function(message) { this.messageElement.textContent = message; },
    });
});
