var initialize_ServiceWorkersTest = function() {

InspectorTest.registerServiceWorker = function(script, scope)
{
    return new Promise(function(resolve, reject){
        var args = {script: script, scope: scope};
        var jsonArgs = JSON.stringify(args).replace(/\"/g, "\\\"");
        function innerCallback(msg)
        {
            if (msg.messageText.indexOf("registerServiceWorker success") !== -1)
                resolve();
            else if (msg.messageText.indexOf("registerServiceWorker fail") !== -1)
                reject();
            else
                InspectorTest.addConsoleSniffer(innerCallback);
        }
        InspectorTest.addConsoleSniffer(innerCallback);
        InspectorTest.evaluateInPage("registerServiceWorker(\"" + jsonArgs + "\")");
    });
}

InspectorTest.unregisterServiceWorker = function(scope)
{
    return new Promise(function(resolve, reject){
        var args = {scope: scope};
        var jsonArgs = JSON.stringify(args).replace(/\"/g, "\\\"");
        function innerCallback(msg)
        {
            if (msg.messageText.indexOf("unregisterServiceWorker success") !== -1)
                resolve();
            else if (msg.messageText.indexOf("unregisterServiceWorker fail") !== -1)
                reject();
            else
                InspectorTest.addConsoleSniffer(innerCallback);
        }
        InspectorTest.addConsoleSniffer(innerCallback);
        InspectorTest.evaluateInPage("unregisterServiceWorker(\"" + jsonArgs + "\")");
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
        var versionElements = registrationElement.querySelectorAll(".service-workers-version-row");
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

function registerServiceWorker(jsonArgs)
{
    var args = JSON.parse(jsonArgs);

    navigator.serviceWorker.register(args.script, {scope: args.scope})
        .then(function(reg){
            registrations[args.scope] = reg;
            console.log("registerServiceWorker success");
        }).catch(function() {
            console.log("registerServiceWorker fail");
        });
}

function unregisterServiceWorker(jsonArgs)
{
    var args = JSON.parse(jsonArgs);
    var registration = registrations[args.scope];
    if (!registration) {
        console.log("ServiceWorker for " + args.scope + " is not registered");
        console.log("unregisterServiceWorker fail");
        return;
    }
    registration.unregister()
        .then(function(){
            console.log("unregisterServiceWorker success");
        }).catch(function() {
            console.log("unregisterServiceWorker fail");
        });
}
