var initialize_IsolatedFileSystemTest = function() {

InspectorTest.createIsolatedFileSystemManager = function(workspace, fileSystemMapping)
{
    var manager = new MockIsolatedFileSystemManager();
    manager.fileSystemWorkspaceProvider = new WebInspector.FileSystemWorkspaceProvider(manager, workspace);
    manager.fileSystemMapping = fileSystemMapping;
    return manager;
}

var MockIsolatedFileSystem = function(path)
{
    this._path = path;
};
MockIsolatedFileSystem.prototype = {
    path: function()
    {
        return this._path;
    },

    requestFileContent: function(path, callback)
    {
        callback(this._files[path]);
    },

    setFileContent: function(path, newContent, callback)
    {
        this._files[path] = newContent;
        callback();
    },

    requestFilesRecursive: function(path, callback)
    {
        this._callback = callback;
        if (!this._files)
            return;
        this._innerRequestFilesRecursive();
    },

    _innerRequestFilesRecursive: function()
    {
        if (!this._callback)
            return;
        var files = Object.keys(this._files);
        for (var i = 0; i < files.length; ++i)
            this._callback(files[i].substr(1));
        delete this._callback;
    },

    _addFiles: function(files)
    {
        this._files = files;
        this._innerRequestFilesRecursive();
    }
}

var MockIsolatedFileSystemManager = function() {};
MockIsolatedFileSystemManager.prototype = {
    addMockFileSystem: function(path, skipAddFileSystem)
    {
        var fileSystem = new MockIsolatedFileSystem(path);
        this._fileSystems = this._fileSystems || {};
        this._fileSystems[path] = fileSystem;
        if (!skipAddFileSystem)
            this.fileSystemMapping.addFileSystem(path);
        this.dispatchEventToListeners(WebInspector.IsolatedFileSystemManager.Events.FileSystemAdded, fileSystem);
    },

    addFiles: function(path, files)
    {
        var fileSystem = this._fileSystems[path];
        fileSystem._addFiles(files);
    },

    removeMockFileSystem: function(path)
    {
        var fileSystem = this._fileSystems[path];
        delete this._fileSystems[path];
        this.fileSystemMapping.removeFileSystem(path);
        this.dispatchEventToListeners(WebInspector.IsolatedFileSystemManager.Events.FileSystemRemoved, fileSystem);
    },

    __proto__: WebInspector.Object.prototype
}

};
