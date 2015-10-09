var initialize_ServiceWorkersTest = function() {

InspectorTest.registerServiceWorker = function(script, scope)
{
    return InspectorTest.invokePageFunctionPromise("registerServiceWorker", [script, scope]);
}

InspectorTest.unregisterServiceWorker = function(scope)
{
    return InspectorTest.invokePageFunctionPromise("unregisterServiceWorker", [scope]);
}

InspectorTest.postToServiceWorker = function(scope, message)
{
    return InspectorTest.invokePageFunctionPromise("postToServiceWorker", [scope, message]);
}

InspectorTest.waitForServiceWorker = function(callback)
{
    function isRightTarget(target)
    {
        return target.isDedicatedWorker() && target.parentTarget() && target.parentTarget().isServiceWorker();
    }

    WebInspector.targetManager.observeTargets({
        targetAdded: function(target)
        {
            if (isRightTarget(target) && callback) {
                setTimeout(callback.bind(null, target), 0);
                callback = null;
            }
        },
        targetRemoved: function(target) {}
    });
}

function replaceInnerTextAll(rootElement, selectors, replacementString)
{
    var elements = rootElement.querySelectorAll(selectors);
    for (var i = 0; i < elements.length; i++)
        elements[i].innerText = replacementString;
}

function modifyTestUnfriendlyText(rootElement)
{
    replaceInnerTextAll(rootElement, ".service-worker-script-last-modified", "LAST-MODIFIED");
    replaceInnerTextAll(rootElement, ".service-worker-script-response-time", "RESPONSE-TIME");
    replaceInnerTextAll(rootElement, ".service-worker-client", "CLIENT");
}

InspectorTest.dumpServiceWorkersView = function(scopes)
{
    var swView = WebInspector.panels.resources.visibleView;
    modifyTestUnfriendlyText(swView._root);
    var results = [];
    var expectedTitles = scopes.map(function(scope) {return "Scope: " + (new URL(scope).pathname)});
    results.push("==== ServiceWorkersView ====");
    results.push(swView._root.querySelector(".service-workers-origin-title").innerText);
    var registrationElements = swView._root.querySelectorAll(".service-workers-registration");
    for (var i = 0; i < registrationElements.length; i++) {
        var registrationElement = registrationElements[i];
        var title = registrationElement.querySelector(".service-workers-registration-title").innerText;
        if (!expectedTitles.some(function(expectedTitle) { return title.indexOf(expectedTitle) != -1; }))
            continue;
        results.push(title);
        var versionElements = registrationElement.querySelectorAll(".service-workers-versions-panel");
        for (var j = 0; j < versionElements.length; j++) {
            if (versionElements[j].innerText.length)
                results.push(versionElements[j].innerText);
        }
    }
    results.push("============================");
    return results;
}

InspectorTest.deleteServiceWorkerRegistration = function(scope)
{
    InspectorTest.serviceWorkerManager.registrations().valuesArray().map(function (registration) {
        if (registration.scopeURL == scope)
            InspectorTest.serviceWorkerManager.deleteRegistration(registration.id);
    });
}

};

var registrations = {};

function registerServiceWorker(resolve, reject, script, scope)
{
    navigator.serviceWorker.register(script, {scope: scope})
        .then(function(reg) {
            registrations[scope] = reg;
            resolve();
        }, reject);
}

function postToServiceWorker(resolve, reject, scope, message)
{
    registrations[scope].active.postMessage(message);
    resolve();
}

function unregisterServiceWorker(resolve, reject, scope)
{
    var registration = registrations[scope];
    if (!registration) {
        reject("ServiceWorker for " + scope + " is not registered");
        return;
    }
    registration.unregister().then(function() {
            delete registrations[scope];
            resolve();
        }, reject);
}
