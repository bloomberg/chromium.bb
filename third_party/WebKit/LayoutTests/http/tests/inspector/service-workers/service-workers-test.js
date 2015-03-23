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
