// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

installClass("Internals", function(global) {
    var InternalsPrototype = Object.create(Element.prototype);

    InternalsPrototype.doNothing = function() {
    }

    InternalsPrototype.return123 = function() {
        return 123;
    }

    InternalsPrototype.echoInteger = function(value) {
        return value;
    }

    InternalsPrototype.echoString = function(value) {
        return value;
    }

    InternalsPrototype.echoNode = function(value) {
        return value;
    }

    InternalsPrototype.addInteger = function(value1, value2) {
        return value1 + value2;
    }

    InternalsPrototype.addString = function(value1, value2) {
        return value1 + value2;
    }

    InternalsPrototype.setIntegerToDocument = function(document, value) {
        document.integer = value;
    }

    InternalsPrototype.getIntegerFromDocument = function(document) {
        return document.integer;
    }

    InternalsPrototype.setIntegerToPrototype = function(value) {
        this.integer = value;
    }

    InternalsPrototype.getIntegerFromPrototype = function() {
        return this.integer;
    }

    InternalsPrototype.createElement = function(document) {
        return document.createElement("div");
    }

    InternalsPrototype.appendChild = function(node1, node2) {
        node1.appendChild(node2);
    }

    InternalsPrototype.firstChild = function(node) {
        return node.firstChild;
    }

    InternalsPrototype.nextSibling = function(node) {
        return node.nextSibling;
    }

    InternalsPrototype.innerHTML = function(node) {
        return node.innerHTML;
    }

    InternalsPrototype.setInnerHTML = function(node, string) {
        node.innerHTML = string;
    }

    InternalsPrototype.addClickListener = function(node) {
        node.addEventListener("click", function () {
            node.innerHTML = "clicked";
        });
    }

    InternalsPrototype.clickNode = function(document, node) {
        var event = new MouseEvent("click", { bubbles: true, cancelable: true, view: global });
        node.dispatchEvent(event);
    }

    return InternalsPrototype;
});
