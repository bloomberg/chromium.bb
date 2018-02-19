// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Wraps runIdleTask(callback) into a Promise for use with async/await.
 */

/**
 * Forces a Promise which runs as an idle task to resolve.
 */
async function forceIdlePromiseResolve(promise) {
    function runIdleTaskPromise() {
        return new Promise(resolve => testRunner.runIdleTasks(resolve));
    }
    await runIdleTaskPromise();
    return promise;
}

if (!window.fixedGetComputedAccessibleNode) {
    originalGetComputedAccessibleNode = window.getComputedAccessibleNode;

    // Monkey patch window.getComputedAccessibleNode with a version which causes
    // an immediate idle task run to ensure the promise gets resolved.
    window.getComputedAccessibleNode = function(element) {
        return forceIdlePromiseResolve(originalGetComputedAccessibleNode(element));
    }

    window.fixedGetComputedAccessibleNode = true;
}
