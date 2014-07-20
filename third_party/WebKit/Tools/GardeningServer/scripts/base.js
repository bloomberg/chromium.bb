/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

var base = base || {};

(function(){

base.endsWith = function(string, suffix)
{
    if (suffix.length > string.length)
        return false;
    var expectedIndex = string.length - suffix.length;
    return string.lastIndexOf(suffix) == expectedIndex;
};

base.joinPath = function(parent, child)
{
    if (parent.length == 0)
        return child;
    return parent + '/' + child;
};

base.trimExtension = function(url)
{
    var index = url.lastIndexOf('.');
    if (index == -1)
        return url;
    return url.substr(0, index);
}

base.uniquifyArray = function(array)
{
    var seen = {};
    var result = [];
    array.forEach(function(value) {
        if (seen[value])
            return;
        seen[value] = true;
        result.push(value);
    });
    return result;
};

base.filterTree = function(tree, isLeaf, predicate)
{
    var filteredTree = {};

    function walkSubtree(subtree, directory)
    {
        for (var childName in subtree) {
            var child = subtree[childName];
            var childPath = base.joinPath(directory, childName);
            if (isLeaf(child)) {
                if (predicate(child))
                    filteredTree[childPath] = child;
                continue;
            }
            walkSubtree(child, childPath);
        }
    }

    walkSubtree(tree, '');
    return filteredTree;
};

base.parseJSONP = function(jsonp)
{
    if (!jsonp)
        return {};

    if (!jsonp.match(/^[^{[]*\(/))
        return JSON.parse(jsonp);

    var startIndex = jsonp.indexOf('(') + 1;
    var endIndex = jsonp.lastIndexOf(')');
    if (startIndex == 0 || endIndex == -1)
        return {};
    return JSON.parse(jsonp.substr(startIndex, endIndex - startIndex));
};

// This is effectively a cache of possibly-resolved promises.
base.AsynchronousCache = function(fetch)
{
    this._fetch = fetch;
    this._promiseCache = {};
};

base.AsynchronousCache._sentinel = new Object();
base.AsynchronousCache.prototype.get = function(key)
{
    if (!(key in this._promiseCache)) {
        this._promiseCache[key] = base.AsynchronousCache._sentinel;
        this._promiseCache[key] = this._fetch.call(null, key);
    }
    if (this._promiseCache[key] === base.AsynchronousCache._sentinel)
        return Promise.reject(Error("Reentrant request for ", key));

    return this._promiseCache[key];
};

base.AsynchronousCache.prototype.clear = function()
{
    this._promiseCache = {};
};

// Based on http://src.chromium.org/viewvc/chrome/trunk/src/chrome/browser/resources/shared/js/cr/ui.js
base.extends = function(base, prototype)
{
    var extended = function() {
        var element = typeof base == 'string' ? document.createElement(base) : base.call(this);
        extended.prototype.__proto__ = element.__proto__;
        element.__proto__ = extended.prototype;
        var singleton = element.init && element.init.apply(element, arguments);
        if (singleton)
            return singleton;
        return element;
    }

    extended.prototype = prototype;
    return extended;
}

base.underscoredBuilderName = function(builderName)
{
    return builderName.replace(/[ .()]/g, '_');
}

base.queryParam = function(params)
{
    var result = []
    Object.keys(params, function(key, value) {
        result.push(encodeURIComponent(key) + '=' + encodeURIComponent(value));
    });
    // FIXME: Remove the conversion of space to plus. This is just here
    // to remain compatible with jQuery.param, but there's no reason to
    // deviate from the built-in encodeURIComponent behavior.
    return result.join('&').replace(/%20/g, '+');
}

})();
