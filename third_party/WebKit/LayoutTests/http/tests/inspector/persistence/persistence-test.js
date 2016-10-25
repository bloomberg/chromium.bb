var initialize_PersistenceTest = function() {

InspectorTest.preloadModule("persistence");
InspectorTest.preloadModule("sources");

WebInspector.PersistenceBinding.prototype.toString = function()
{
    var lines = [
        "{",
        "       network: " + this.network.url(),
        "    fileSystem: " + this.fileSystem.url(),
        "}"
    ];
    return lines.join("\n");
}

InspectorTest.waitForBinding = function(fileName)
{
    var uiSourceCodes = WebInspector.workspace.uiSourceCodes();
    for (var uiSourceCode of uiSourceCodes) {
        var binding = WebInspector.persistence.binding(uiSourceCode);
        if (!binding)
            continue;
        if (uiSourceCode.name() === fileName)
            return Promise.resolve(binding);
    }
    var fulfill;
    var promise = new Promise(x => fulfill = x);
    WebInspector.persistence.addEventListener(WebInspector.Persistence.Events.BindingCreated, onBindingCreated);
    return promise;

    function onBindingCreated(event)
    {
        var binding = event.data;
        if (binding.network.name() !== fileName && binding.fileSystem.name() !== fileName)
            return;
        WebInspector.persistence.removeEventListener(WebInspector.Persistence.Events.BindingCreated, onBindingCreated);
        fulfill(binding);
    }
}

InspectorTest.waitForUISourceCode = function(name, projectType)
{
    var uiSourceCodes = WebInspector.workspace.uiSourceCodes();
    var uiSourceCode = uiSourceCodes.find(filterCode);
    if (uiSourceCode)
        return Promise.resolve(uiSourceCode);

    var fulfill;
    var promise = new Promise(x => fulfill = x);
    WebInspector.workspace.addEventListener(WebInspector.Workspace.Events.UISourceCodeAdded, onUISourceCode);
    return promise;

    function onUISourceCode(event)
    {
        var uiSourceCode = event.data;
        if (!filterCode(uiSourceCode))
            return;
        WebInspector.workspace.removeEventListener(WebInspector.Workspace.Events.UISourceCodeAdded, onUISourceCode);
        fulfill(uiSourceCode);
    }

    function filterCode(uiSourceCode)
    {
        return uiSourceCode.name() === name && uiSourceCode.project().type() === projectType;
    }
}

InspectorTest.addFooJSFile = function(fs)
{
    return fs.root.mkdir("inspector").mkdir("persistence").mkdir("resources").addFile("foo.js", "\n\nwindow.foo = ()=>'foo';");
}

}
