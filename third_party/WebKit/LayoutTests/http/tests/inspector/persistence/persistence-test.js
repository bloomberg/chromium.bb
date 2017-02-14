var initialize_PersistenceTest = function() {

InspectorTest.preloadModule("persistence");
InspectorTest.preloadModule("sources");

Runtime.experiments.enableForTest("persistenceValidation");

Persistence.PersistenceBinding.prototype.toString = function()
{
    var lines = [
        "{",
        "       network: " + this.network.url(),
        "    fileSystem: " + this.fileSystem.url(),
        "    exactMatch: " + this.exactMatch,
        "}"
    ];
    return lines.join("\n");
}

InspectorTest.waitForBinding = function(fileName)
{
    var uiSourceCodes = Workspace.workspace.uiSourceCodes();
    for (var uiSourceCode of uiSourceCodes) {
        var binding = Persistence.persistence.binding(uiSourceCode);
        if (!binding)
            continue;
        if (uiSourceCode.name() === fileName)
            return Promise.resolve(binding);
    }
    var fulfill;
    var promise = new Promise(x => fulfill = x);
    Persistence.persistence.addEventListener(Persistence.Persistence.Events.BindingCreated, onBindingCreated);
    return promise;

    function onBindingCreated(event)
    {
        var binding = event.data;
        if (binding.network.name() !== fileName && binding.fileSystem.name() !== fileName)
            return;
        Persistence.persistence.removeEventListener(Persistence.Persistence.Events.BindingCreated, onBindingCreated);
        fulfill(binding);
    }
}

InspectorTest.addFooJSFile = function(fs)
{
    return fs.root.mkdir("inspector").mkdir("persistence").mkdir("resources").addFile("foo.js", "\n\nwindow.foo = ()=>'foo';");
}

}
