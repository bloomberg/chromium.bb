// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var initialize_CacheStorageTest = function() {
InspectorTest.preloadPanel("resources");

InspectorTest.dumpCacheTree = async function()
{
    UI.panels.resources._sidebar.cacheStorageListTreeElement.expand();
    InspectorTest.addResult("Dumping CacheStorage tree:");
    var cachesTreeElement = UI.panels.resources._sidebar.cacheStorageListTreeElement;
    var promise = InspectorTest.addSnifferPromise(SDK.ServiceWorkerCacheModel.prototype, "_updateCacheNames");
    UI.panels.resources._sidebar.cacheStorageListTreeElement._refreshCaches();

    await promise;

    if (!cachesTreeElement.childCount()) {
        InspectorTest.addResult("    (empty)");
        return;
    }

    for (var i = 0; i < cachesTreeElement.childCount(); ++i) {
        var cacheTreeElement = cachesTreeElement.childAt(i);
        InspectorTest.addResult("    cache: " + cacheTreeElement.title);
        var view = cacheTreeElement._view;
        promise = InspectorTest.addSnifferPromise(Resources.ServiceWorkerCacheView.prototype, "_updateDataCallback");
        if (!view)
            cacheTreeElement.onselect(false);
        else
            view._updateData(true);
        view = cacheTreeElement._view;

        await promise;

        if (view._entries.length == 0) {
            InspectorTest.addResult("        (cache empty)");
            continue;
        }
        var dataGrid = view._dataGrid;
        for (var node of dataGrid.rootNode().children) {
            var children = Array.from(node.element().children).filter(function (element) {
                // Removes timestamp, potential flake depending on time zone
                return !(element.classList.contains("responseTime-column"));
            });
            var entries = Array.from(children, td => td.textContent).filter(text => text);
            InspectorTest.addResult("        " + entries.join(", "));
        }
    }
}

// If optionalEntry is not specified, then the whole cache is deleted.
InspectorTest.deleteCacheFromInspector = async function(cacheName, optionalEntry)
{
    UI.panels.resources._sidebar.cacheStorageListTreeElement.expand();
    if (optionalEntry)
        InspectorTest.addResult("Deleting CacheStorage entry " + optionalEntry + " in cache " + cacheName);
    else
        InspectorTest.addResult("Deleting CacheStorage cache " + cacheName);
    var cachesTreeElement = UI.panels.resources._sidebar.cacheStorageListTreeElement;
    var promise = InspectorTest.addSnifferPromise(SDK.ServiceWorkerCacheModel.prototype, "_updateCacheNames");
    UI.panels.resources._sidebar.cacheStorageListTreeElement._refreshCaches();

    await promise;

    if (!cachesTreeElement.childCount()) {
        throw "Error: Could not find CacheStorage cache " + cacheName;
        return;
    }

    for (var i = 0; i < cachesTreeElement.childCount(); i++) {
        var cacheTreeElement = cachesTreeElement.childAt(i);
        var title = cacheTreeElement.title;
        var elementCacheName = title.substring(0, title.lastIndexOf(" - "));
        if (elementCacheName != cacheName)
            continue;
        if (!optionalEntry) {
            // Here we're deleting the whole cache.
            promise = InspectorTest.addSnifferPromise(SDK.ServiceWorkerCacheModel.prototype, "_cacheRemoved");
            cacheTreeElement._clearCache();
            await promise;
            return;
        }

        promise = InspectorTest.addSnifferPromise(Resources.ServiceWorkerCacheView.prototype, "_updateDataCallback");
        // Here we're deleting only the entry.  We verify that it is present in the table.
        var view = cacheTreeElement._view;
        if (!view)
            cacheTreeElement.onselect(false);
        else
            view._updateData(true);
        view = cacheTreeElement._view;

        await promise;

        var entry = view._entries.find(entry => entry.request === optionalEntry);
        if (!entry) {
            throw "Error: Could not find cache entry to delete: " + optionalEntry;
            return;
        }
        await view._model.deleteCacheEntry(view._cache, entry.request);
        return;
    }
    throw "Error: Could not find CacheStorage cache " + cacheName;
}

InspectorTest.waitForCacheRefresh = function(callback)
{
    InspectorTest.addSniffer(SDK.ServiceWorkerCacheModel.prototype, "_updateCacheNames", callback, false);
}

InspectorTest.createCache = function(cacheName)
{
    return InspectorTest.callFunctionInPageAsync("createCache", [ cacheName ]);
}

InspectorTest.addCacheEntry = function(cacheName, requestUrl, responseText)
{
    return InspectorTest.callFunctionInPageAsync("addCacheEntry", [ cacheName, requestUrl, responseText ]);
}

InspectorTest.deleteCache = function(cacheName)
{
    return InspectorTest.callFunctionInPageAsync("deleteCache", [ cacheName ]);
}

InspectorTest.deleteCacheEntry = function(cacheName, requestUrl)
{
    return InspectorTest.callFunctionInPageAsync("deleteCacheEntry", [ cacheName, requestUrl ]);
}

InspectorTest.clearAllCaches = function()
{
    return InspectorTest.callFunctionInPageAsync("clearAllCaches");
}
}

function onCacheStorageError(e)
{
    console.error("CacheStorage error: " + e);
}

function createCache(cacheName)
{
    return caches.open(cacheName).catch(onCacheStorageError);
}

function addCacheEntry(cacheName, requestUrl, responseText)
{
    return caches.open(cacheName)
        .then(function(cache) {
            var request = new Request(requestUrl);
            var myBlob = new Blob();
            var init = { "status" : 200 , "statusText" : responseText };
            var response = new Response(myBlob, init);
            return cache.put(request, response);
        })
        .catch(onCacheStorageError);
}

function deleteCache(cacheName)
{
    return caches.delete(cacheName)
        .then(function(success) {
            if (!success)
                onCacheStorageError("Could not find cache " + cacheName);
        }).catch(onCacheStorageError);
}

function deleteCacheEntry(cacheName, requestUrl)
{
    return caches.open(cacheName)
        .then((cache) => cache.delete(new Request(requestUrl)))
        .catch(onCacheStorageError);
}

function clearAllCaches()
{
    return caches.keys()
        .then((keys) => Promise.all(keys.map((key) => caches.delete(key))))
        .catch(onCacheStorageError.bind(this, undefined));
}
