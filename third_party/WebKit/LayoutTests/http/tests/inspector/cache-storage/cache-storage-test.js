// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var initialize_CacheStorageTest = function() {
InspectorTest.preloadPanel("resources");

InspectorTest.dumpCacheTree = function()
{
    UI.panels.resources.cacheStorageListTreeElement.expand();
    InspectorTest.addResult("Dumping CacheStorage tree:");
    var cachesTreeElement = UI.panels.resources.cacheStorageListTreeElement;
    var promise = new Promise(function(resolve, reject) {
        InspectorTest.addSnifferPromise(SDK.ServiceWorkerCacheModel.prototype, "_updateCacheNames").then(crawlCacheTree).catch(reject);

        function crawlCacheTree()
        {
            if (!cachesTreeElement.childCount()) {
                InspectorTest.addResult("    (empty)");
                return resolve();
            }

            queryView(0);

            function queryView(i)
            {
                var cacheTreeElement = cachesTreeElement.childAt(i);
                InspectorTest.addResult("    cache: " + cacheTreeElement.title);
                var view = cacheTreeElement._view;
                InspectorTest.addSniffer(Resources.ServiceWorkerCacheView.prototype, "_updateDataCallback", addDataResult, false);
                if (!view)
                    cacheTreeElement.onselect(false);
                else
                    view._updateData(true);
                view = cacheTreeElement._view;

                function addDataResult()
                {
                    if (view._entries.length == 0) {
                        InspectorTest.addResult("        (cache empty)");
                        nextOrResolve();
                        return;
                    }
                    var dataGrid = view._dataGrid;
                    for (var node of dataGrid.rootNode().children) {
                        var entries = [];
                        for (var j = 0; j < node.element().children.length; j++) {
                            var td = node.element().children[j];
                            if (td.textContent)
                                entries.push(td.textContent);
                        }
                        InspectorTest.addResult("        " + entries.join(", "));
                    }
                    nextOrResolve();
                }

                function nextOrResolve()
                {
                    var next = i + 1;
                    if (next < cachesTreeElement.childCount())
                        queryView(next);
                    else
                        resolve();
                }
            }
        }
    });
    UI.panels.resources.cacheStorageListTreeElement._refreshCaches();
    return promise;
}

// If optionalEntry is not specified, then the whole cache is deleted.
InspectorTest.deleteCacheFromInspector = function(cacheName, optionalEntry)
{
    UI.panels.resources.cacheStorageListTreeElement.expand();
    if (optionalEntry) {
        InspectorTest.addResult("Deleting CacheStorage entry " + optionalEntry + " in cache " + cacheName);
    } else {
        InspectorTest.addResult("Deleting CacheStorage cache " + cacheName);
    }
    var cachesTreeElement = UI.panels.resources.cacheStorageListTreeElement;
    var promise = new Promise(function(resolve, reject) {
        InspectorTest.addSnifferPromise(SDK.ServiceWorkerCacheModel.prototype, "_updateCacheNames")
            .then(function() {
                if (!cachesTreeElement.childCount()) {
                    reject("Error: Could not find CacheStorage cache " + cacheName);
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
                        InspectorTest.addSniffer(SDK.ServiceWorkerCacheModel.prototype, "_cacheRemoved", resolve)
                        cacheTreeElement._clearCache();
                        return;
                    }

                    // Here we're deleting only the entry.  We verify that it is present in the table.
                    var view = cacheTreeElement._view;
                    InspectorTest.addSniffer(Resources.ServiceWorkerCacheView.prototype, "_updateDataCallback", deleteEntryOrReject, false);
                    if (!view)
                        cacheTreeElement.onselect(false);
                    else
                        view._updateData(true);
                    view = cacheTreeElement._view;

                    function deleteEntryOrReject()
                    {
                        for (var entry of view._entries) {
                            if (entry.request == optionalEntry) {
                                view._model.deleteCacheEntry(view._cache, entry.request, resolve);
                                return;
                            }
                        }
                        reject("Error: Could not find cache entry to delete: " + optionalEntry);
                        return;
                    }
                    return;
                }
                reject("Error: Could not find CacheStorage cache " + cacheName);
            }).catch(reject);
    });
    UI.panels.resources.cacheStorageListTreeElement._refreshCaches();
    return promise;
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
