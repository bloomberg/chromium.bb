var initialize_IsolatedFileSystemTest = function() {

InspectorTest.createIsolatedFileSystemManager = function(workspace)
{
    var normalizePath = WebInspector.IsolatedFileSystem.normalizePath
    WebInspector.IsolatedFileSystem = MockIsolatedFileSystem;
    WebInspector.IsolatedFileSystem.normalizePath = normalizePath;

    var manager = new MockIsolatedFileSystemManager();
    manager.fileSystemMapping = InspectorTest.testFileSystemMapping;
    manager.fileSystemWorkspaceBinding = new WebInspector.FileSystemWorkspaceBinding(manager, workspace, InspectorTest.testNetworkMapping);
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
                    if (InspectorTest.testExcludedFolderManager.isFileExcluded(this._path, files[i].substr(0, j + 1)))
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

    excludedFolderManager: function()
    {
        return InspectorTest.testExcludedFolderManager;
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

InspectorFrontendHost.isolatedFileSystem = function(name, url)
{
    return InspectorTest.TestFileSystem._instances[name];
}

InspectorTest.TestFileSystem = function(fileSystemPath)
{
    this.root = new InspectorTest.TestFileSystem.Entry("", true);
    this.fileSystemPath = fileSystemPath;
}

InspectorTest.TestFileSystem._instances = {};

InspectorTest.TestFileSystem.prototype = {
    reportCreated: function()
    {
        WebInspector.isolatedFileSystemManager._loaded = true;
        InspectorTest.TestFileSystem._instances[this.fileSystemPath] = this;
        InspectorFrontendHost.events.dispatchEventToListeners(InspectorFrontendHostAPI.Events.FileSystemAdded, {
            fileSystem: { fileSystemPath: this.fileSystemPath,
                          fileSystemName: this.fileSystemPath }
        });
    }
}

InspectorTest.TestFileSystem.Entry = function(name, isDirectory)
{
    this.name = name;
    this._children = [];
    this._childrenMap = {};
    this.isDirectory = isDirectory;
}

InspectorTest.TestFileSystem.Entry.prototype = {
    get fullPath()
    {
        return (this.parent ? this.parent.fullPath : "") + "/" + this.name;
    },

    mkdir: function(name)
    {
        var child = new InspectorTest.TestFileSystem.Entry(name, true);
        this._childrenMap[name] = child;
        this._children.push(child);
        child.parent = this;
        return child;
    },

    addFile: function(name, content)
    {
        var child = new InspectorTest.TestFileSystem.Entry(name, false);
        this._childrenMap[name] = child;
        this._children.push(child);
        child.parent = this;
        child.content = new Blob([content], {type: 'text/plain'});
    },

    createReader: function()
    {
        return new InspectorTest.TestFileSystem.Reader(this._children);
    },

    file: function(callback)
    {
        callback(this.content);
    },

    getDirectory: function(path, noop, callback)
    {
        this.getEntry(path, noop, callback);
    },

    getFile: function(path, noop, callback)
    {
        this.getEntry(path, noop, callback);
    },

    getEntry: function(path, noop, callback)
    {
        if (path.startsWith("/"))
            path = path.substring(1);
        if (!path) {
            callback(this);
            return;
        }
        var entry = this;
        for (var token of path.split("/"))
            entry = entry._childrenMap[token];
        callback(entry);
    }
}

InspectorTest.TestFileSystem.Reader = function(children)
{
    this._children = children;
}

InspectorTest.TestFileSystem.Reader.prototype = {
    readEntries: function(callback)
    {
        var children = this._children;
        this._children = [];
        callback(children);
    }
}

};
