var initialize_IsolatedFileSystemTest = function() {

InspectorTest.createIsolatedFileSystemManager = function(workspace, fileSystemMapping)
{
    var manager = new MockIsolatedFileSystemManager();
    manager.fileSystemWorkspaceBinding = new WebInspector.FileSystemWorkspaceBinding(manager, workspace);
    manager.fileSystemMapping = fileSystemMapping;
    return manager;
}

var MockIsolatedFileSystem = function(manager, path, name, rootURL)
{
    MockIsolatedFileSystem._isolatedFileSystemMocks = MockIsolatedFileSystem._isolatedFileSystemMocks || {};
    MockIsolatedFileSystem._isolatedFileSystemMocks[path] = this;
    this.originalTimestamp = 1000000;
    this.modificationTimestampDelta = 1000;
    this._path = path;
    this._manager = manager;
};
MockIsolatedFileSystem.prototype = {
    path: function()
    {
        return this._path;
    },

    normalizedPath: function()
    {
        return this._path;
    },

    requestFileContent: function(path, callback)
    {
        callback(this._files[path]);
    },

    requestMetadata: function(path, callback)
    {
        if (!(path in this._files)) {
            callback(null, null);
            return;
        }
        callback(new Date(this._modificationTimestamps[path]), this._files[path].length);
    },

    setFileContent: function(path, newContent, callback)
    {
        this._files[path] = newContent;
        this._modificationTimestamps[path] = (this._modificationTimestamps[path] || (this.originalTimestamp - this.modifiationTimestampDelta)) + this.modificationTimestampDelta;
        callback();
    },

    requestFilesRecursive: function(path, callback)
    {
        this._callback = callback;
        if (!this._files)
            return;
        this._innerRequestFilesRecursive();
    },

    renameFile: function(filePath, newName, callback)
    {
        callback(true, newName);
    },

    _innerRequestFilesRecursive: function()
    {
        if (!this._callback)
            return;
        var files = Object.keys(this._files);
        for (var i = 0; i < files.length; ++i) {
            var isExcluded = false;
            for (var j = 0; j < files[i].length; ++j) {
                if (files[i][j] === "/") {
                    if (this._manager.mapping().isFileExcluded(this._path, files[i].substr(0, j + 1)))
                        isExcluded = true;
                }
            }
            if (isExcluded)
                continue;
            this._callback(files[i].substr(1));
        }
        delete this._callback;
    },

    _addFiles: function(files)
    {
        this._files = files;
        this._modificationTimestamps = {};
        var files = Object.keys(this._files);
        for (var i = 0; i < files.length; ++i)
            this._modificationTimestamps[files[i]] = this.originalTimestamp;
        this._innerRequestFilesRecursive();
    }
}

var normalizePath = WebInspector.IsolatedFileSystem.normalizePath
WebInspector.IsolatedFileSystem = MockIsolatedFileSystem;
WebInspector.IsolatedFileSystem.normalizePath = normalizePath;

var MockIsolatedFileSystemManager = function() {};
MockIsolatedFileSystemManager.prototype = {
    addMockFileSystem: function(path, skipAddFileSystem)
    {
        var fileSystem = new MockIsolatedFileSystem(this, path, "", "");
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

    removeFileSystem: function(path)
    {
        var fileSystem = this._fileSystems[path];
        delete this._fileSystems[path];
        this.fileSystemMapping.removeFileSystem(path);
        this.dispatchEventToListeners(WebInspector.IsolatedFileSystemManager.Events.FileSystemRemoved, fileSystem);
    },

    mapping: function()
    {
        return this.fileSystemMapping;
    },

    __proto__: WebInspector.Object.prototype
}

InspectorTest.addMockFileSystem = function(path)
{
    var fileSystem = { fileSystemName: "", rootURL: "", fileSystemPath: path };
    WebInspector.isolatedFileSystemManager._onFileSystemAdded({data: {fileSystem: fileSystem}});
    return MockIsolatedFileSystem._isolatedFileSystemMocks[path];
}

InspectorTest.addFilesToMockFileSystem = function(path, files)
{
    MockIsolatedFileSystem._isolatedFileSystemMocks[path]._addFiles(files);
}

};
