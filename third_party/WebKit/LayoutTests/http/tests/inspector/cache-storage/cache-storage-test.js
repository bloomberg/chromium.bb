// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var initialize_CacheStorageTest = function() {
InspectorTest.preloadPanel("resources");

InspectorTest.dumpCacheTree = function()
{
    WebInspector.panels.resources.cacheStorageListTreeElement.expand();
    InspectorTest.addResult("Dumping CacheStorage tree:");
    var cachesTreeElement = WebInspector.panels.resources.cacheStorageListTreeElement;
    var promise = new Promise(function(resolve, reject) {
        InspectorTest.addSnifferPromise(WebInspector.ServiceWorkerCacheModel.prototype, "_updateCacheNames").then(crawlCacheTree).catch(reject);

        function crawlCacheTree()
        {
            if (!cachesTreeElement.childCount()) {
                InspectorTest.addResult("    (empty)");
                return resolve();
            }

            WebInspector.panels.resources.cacheStorageListTreeElement._refreshCaches();
            queryView(0);

            function queryView(i)
            {
                var cacheTreeElement = cachesTreeElement.childAt(i);
                InspectorTest.addResult("    cache: " + cacheTreeElement.titleText);
                var view = cacheTreeElement._view;
                InspectorTest.addSniffer(WebInspector.ServiceWorkerCacheView.prototype, "_updateDataCallback", addDataResult, false);
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
                    for (var entry of view._entries)
                        InspectorTest.addResult("        '" + entry.request._value + "': '" + entry.response._value + "'");
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
    WebInspector.panels.resources.cacheStorageListTreeElement._refreshCaches();
    return promise;
}

InspectorTest.deleteCacheFromInspector = function(cacheName)
{
    WebInspector.panels.resources.cacheStorageListTreeElement.expand();
    InspectorTest.addResult("Deleting CacheStorage cache " + cacheName);
    var cachesTreeElement = WebInspector.panels.resources.cacheStorageListTreeElement;
    var promise = new Promise(function(resolve, reject) {
        InspectorTest.addSnifferPromise(WebInspector.ServiceWorkerCacheModel.prototype, "_updateCacheNames")
            .then(function() {
                if (!cachesTreeElement.childCount())
                    return resolve();
                for (var i = 0; i < cachesTreeElement.childCount(); i++) {
                    var cacheTreeElement = cachesTreeElement.childAt(i);
                    var title = cacheTreeElement.titleText;
                    var elementCacheName = title.substring(0, title.lastIndexOf(" - "));
                    if (elementCacheName != cacheName)
                        continue;
                    InspectorTest.addSniffer(WebInspector.ServiceWorkerCacheModel.prototype, "_cacheRemoved", resolve)
                    cacheTreeElement._clearCache();
                    return;
                }
                InspectorTest.addResult("Error: Could not find cache to delete.");
                reject();
            }).catch(reject);
    });
    WebInspector.panels.resources.cacheStorageListTreeElement._refreshCaches();
    return promise;
}

InspectorTest.waitForCacheRefresh = function(callback)
{
    InspectorTest.addSniffer(WebInspector.ServiceWorkerCacheModel.prototype, "_updateCacheNames", callback, false);
}

InspectorTest.createCache = function(cacheName)
{
    return InspectorTest.invokePageFunctionPromise("createCache", [cacheName]);
}

InspectorTest.addCacheEntry = function(cacheName, requestUrl, responseText)
{
    return InspectorTest.invokePageFunctionPromise("addCacheEntry", [cacheName, requestUrl, responseText]);
}

InspectorTest.deleteCache = function(cacheName)
{
    return InspectorTest.invokePageFunctionPromise("deleteCache", [cacheName]);
}

InspectorTest.clearAllCaches = function()
{
    return InspectorTest.invokePageFunctionPromise("clearAllCaches", []);
}
}

function onCacheStorageError(reject, e)
{
    console.error("CacheStorage error: " + e);
    reject();
}

function createCache(resolve, reject, cacheName)
{
    caches.open(cacheName).then(resolve).catch(onCacheStorageError.bind(this, reject));
}

function addCacheEntry(resolve, reject, cacheName, requestUrl, responseText)
{
    caches.open(cacheName)
        .then(function(cache) {
            var request = new Request(requestUrl);
            var myBlob = new Blob();
            var init = { "status" : 200 , "statusText" : responseText };
            var response = new Response(myBlob, init);
            return cache.put(request, response);
        })
        .then(resolve)
        .catch(onCacheStorageError.bind(this, reject));
}

function deleteCache(resolve, reject, cacheName)
{
    caches.delete(cacheName)
        .then(function(success) {
            if (success)
                resolve();
            else
                onCacheStorageError(reject, "Could not find cache " + cacheName);
        }).catch(onCacheStorageError.bind(this, reject));
}

function clearAllCaches(resolve, reject)
{
    caches.keys()
        .then(function(keys) {
            return Promise.all(keys.map(function(key) {
                return caches.delete(key);
            }));
        })
        .then(resolve)
        .catch(onCacheStorageError.bind(this, reject));
}
