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

InspectorTest.forceUseDefaultMapping = function() {
    Persistence.persistence._setMappingForTest((bindingCreated, bindingRemoved) => {
        return new Persistence.DefaultMapping(Workspace.workspace, Persistence.fileSystemMapping, bindingCreated, bindingRemoved);
    });
}

InspectorTest.initializeTestMapping = function() {
    var testMapping;
    Persistence.persistence._setMappingForTest((bindingCreated, bindingRemoved) => {
        testMapping = new TestMapping(bindingCreated, bindingRemoved);
        return testMapping;
    });
    return testMapping;
}

class TestMapping{
    constructor(onBindingAdded, onBindingRemoved) {
        this._onBindingAdded = onBindingAdded;
        this._onBindingRemoved = onBindingRemoved;
        this._bindings = new Set();
    }

    async addBinding(urlSuffix) {
        if (this._findBinding(urlSuffix)) {
            InspectorTest.addResult(`FAILED TO ADD BINDING: binding already exists for ${urlSuffix}`);
            InspectorTest.completeTest();
            return;
        }
        var networkUISourceCode = await InspectorTest.waitForUISourceCode(urlSuffix, Workspace.projectTypes.Network);
        var fileSystemUISourceCode = await InspectorTest.waitForUISourceCode(urlSuffix, Workspace.projectTypes.FileSystem);

        var binding = new Persistence.PersistenceBinding(networkUISourceCode, fileSystemUISourceCode, false);
        this._bindings.add(binding);
        this._onBindingAdded.call(null, binding);
    }

    _findBinding(urlSuffix) {
        for (var binding of this._bindings) {
            if (binding.network.url().endsWith(urlSuffix))
                return binding;
        }
        return null;
    }

    async removeBinding(urlSuffix) {
        var binding = this._findBinding(urlSuffix);
        if (!binding) {
            InspectorTest.addResult(`FAILED TO REMOVE BINDING: binding does not exist for ${urlSuffix}`);
            InspectorTest.completeTest();
            return;
        }
        this._bindings.delete(binding);
        this._onBindingRemoved.call(null, binding);
    }

    dispose() {
        for (var binding of this._bindings)
            this._onBindingRemoved.call(null, binding);
        this._bindings.clear();
    }
}

}
