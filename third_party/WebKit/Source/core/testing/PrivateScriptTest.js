// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

installClass("PrivateScriptTest", function(global, InternalsPrototype) {

    InternalsPrototype.initialize = function() {
        this.m_shortAttribute = -1;
        this.m_stringAttribute = "xxx";
        this.m_nodeAttribute = null;
        this.m_stringAttributeForPrivateScriptOnly = "yyy";
    }

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

    InternalsPrototype.addValues_ = function(value1, value2) {
        return value1 + value2;
    }

    InternalsPrototype.addInteger = function(value1, value2) {
        return this.addValues_(value1, value2);
    }

    InternalsPrototype.addString = function(value1, value2) {
        return this.addValues_(value1, value2);
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

    Object.defineProperty(InternalsPrototype, "readonlyShortAttribute", {
        get: function() { return 123; }
    });

    Object.defineProperty(InternalsPrototype, "shortAttribute", {
        get: function() { return this.m_shortAttribute; },
        set: function(value) { this.m_shortAttribute = value; }
    });

    Object.defineProperty(InternalsPrototype, "stringAttribute", {
        get: function() { return this.m_stringAttribute; },
        set: function(value) { this.m_stringAttribute = value; }
    });

    Object.defineProperty(InternalsPrototype, "nodeAttribute", {
        get: function() { return this.m_nodeAttribute; },
        set: function(value) { this.m_nodeAttribute = value; }
    });

    Object.defineProperty(InternalsPrototype, "nodeAttributeThrowsIndexSizeError", {
        get: function() { throw new DOMExceptionInPrivateScript("IndexSizeError", "getter threw error"); },
        set: function(value) { throw new DOMExceptionInPrivateScript("IndexSizeError", "setter threw error"); }
    });

    InternalsPrototype.voidMethodThrowsSyntaxError = function() {
        throw new DOMExceptionInPrivateScript("SyntaxError", "method threw error");
    }

    InternalsPrototype.voidMethodThrowsTypeError = function() {
        throw new DOMExceptionInPrivateScript("TypeError", "method threw TypeError");
    }

    InternalsPrototype.voidMethodThrowsRangeError = function() {
        throw new DOMExceptionInPrivateScript("RangeError", "method threw RangeError");
    }

    InternalsPrototype.voidMethodWithStackOverflow = function() {
        function f() { f(); }
        f();
    }

    InternalsPrototype.addIntegerForPrivateScriptOnly = function(value1, value2) {
        return value1 + value2;
    }

    Object.defineProperty(InternalsPrototype, "stringAttributeForPrivateScriptOnly", {
        get: function() { return this.m_stringAttributeForPrivateScriptOnly; },
        set: function(value) { this.m_stringAttributeForPrivateScriptOnly = value; }
    });

    InternalsPrototype.addIntegerImplementedInCPP = function(value1, value2) {
        return this.addIntegerImplementedInCPPForPrivateScriptOnly(value1, value2);
    }

    Object.defineProperty(InternalsPrototype, "stringAttributeImplementedInCPP", {
        get: function() { return this.m_stringAttributeImplementedInCPPForPrivateScriptOnly; },
        set: function(value) { this.m_stringAttributeImplementedInCPPForPrivateScriptOnly = value; }
    });
});
