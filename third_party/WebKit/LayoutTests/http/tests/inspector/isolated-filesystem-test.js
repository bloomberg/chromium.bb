var initialize_IsolatedFileSystemTest = function() {

InspectorFrontendHost.isolatedFileSystem = function(name)
{
    return InspectorTest.TestFileSystem._instances[name];
}

InspectorTest.TestFileSystem = function(fileSystemPath)
{
    this.root = new InspectorTest.TestFileSystem.Entry(this, "", true, null);
    this.fileSystemPath = fileSystemPath;
}

InspectorTest.TestFileSystem._instances = {};

InspectorTest.TestFileSystem.prototype = {
    dumpAsText: function() {
        var result = [];
        dfs(this.root, '');
        result[0] = this.fileSystemPath;
        return result.join('\n');

        function dfs(node, indent) {
            result.push(indent + node.name);
            var newIndent = indent + '    ';
            for (var child of node._children)
                dfs(child, newIndent);
        }
    },

    reportCreated: function(callback)
    {
        var fileSystemPath = this.fileSystemPath;
        InspectorTest.TestFileSystem._instances[this.fileSystemPath] = this;
        InspectorFrontendHost.events.dispatchEventToListeners(InspectorFrontendHostAPI.Events.FileSystemAdded, {
            fileSystem: { fileSystemPath: this.fileSystemPath,
                          fileSystemName: this.fileSystemPath }
        });

        Workspace.isolatedFileSystemManager.addEventListener(Workspace.IsolatedFileSystemManager.Events.FileSystemAdded, created);

        function created(event)
        {
            var fileSystem = event.data;
            if (fileSystem.path() !== fileSystemPath)
                return;
            Workspace.isolatedFileSystemManager.removeEventListener(Workspace.IsolatedFileSystemManager.Events.FileSystemAdded, created);
            callback(fileSystem);
        }
    },

    reportRemoved: function()
    {
        delete InspectorTest.TestFileSystem._instances[this.fileSystemPath];
        InspectorFrontendHost.events.dispatchEventToListeners(InspectorFrontendHostAPI.Events.FileSystemRemoved, this.fileSystemPath);
    },

    addFileMapping: function(urlPrefix, pathPrefix)
    {
        var fileSystemMapping = new Workspace.FileSystemMapping(Workspace.isolatedFileSystemManager);
        fileSystemMapping.addFileSystem(this.fileSystemPath);
        fileSystemMapping.addFileMapping(this.fileSystemPath, urlPrefix, pathPrefix);
        fileSystemMapping.dispose();
        Workspace.fileSystemMapping._loadFromSettings();
    },

    /**
     * @param {string} path
     * @param {string} content
     * @param {number} lastModified
     */
    addFile: function(path, content, lastModified)
    {
        var pathTokens = path.split("/");
        var node = this.root;
        var folders = pathTokens.slice(0, pathTokens.length - 1);
        var fileName = pathTokens.peekLast();
        for (var folder of folders) {
            var dir = node._childrenMap[folder];
            if (!dir)
                dir = node.mkdir(folder);
            node = dir;
        }
        var file = node.addFile(fileName, content);
        if (lastModified)
            file._timestamp = lastModified;
        return file;
    }
}

InspectorTest.TestFileSystem.Entry = function(fileSystem, name, isDirectory, parent)
{
    this._fileSystem = fileSystem;
    this.name = name;
    this._children = [];
    this._childrenMap = {};
    this.isDirectory = isDirectory;
    this._timestamp = 1000000;
    this._parent = parent;
}

InspectorTest.TestFileSystem.Entry.prototype = {
    get fullPath()
    {
        return this.parent ? this.parent.fullPath  + "/" + this.name : "";
    },

    remove: function(success, failure)
    {
        this._parent._removeChild(this, success, failure);
    },

    _removeChild: function(child, success, failure)
    {
        var index = this._children.indexOf(child);
        if (index === -1) {
            failure("Failed to remove file: file not found.");
            return;
        }
        var fullPath = this._fileSystem.fileSystemPath + child.fullPath;
        this._children.splice(index, 1);
        delete this._childrenMap[child.name];
        child.parent = null;
        InspectorFrontendHost.events.dispatchEventToListeners(
            InspectorFrontendHostAPI.Events.FileSystemFilesChangedAddedRemoved,
            {changed: [], added: [], removed: [fullPath]});
        success();
    },

    mkdir: function(name)
    {
        var child = new InspectorTest.TestFileSystem.Entry(this._fileSystem, name, true, this);
        this._childrenMap[name] = child;
        this._children.push(child);
        child.parent = this;
        return child;
    },

    addFile: function(name, content)
    {
        var child = new InspectorTest.TestFileSystem.Entry(this._fileSystem, name, false, this);
        this._childrenMap[name] = child;
        this._children.push(child);
        child.parent = this;
        child.content = new Blob([content], {type: 'text/plain'});
        var fullPath = this._fileSystem.fileSystemPath + child.fullPath;
        InspectorFrontendHost.events.dispatchEventToListeners(
            InspectorFrontendHostAPI.Events.FileSystemFilesChangedAddedRemoved,
            {changed: [], added: [fullPath], removed: []});
        return child;
    },

    setContent: function(content)
    {
        this.content = new Blob([content], {type: 'text/plain'});
        this._timestamp += 1000;
        var fullPath = this._fileSystem.fileSystemPath + this.fullPath;
        InspectorFrontendHost.events.dispatchEventToListeners(
            InspectorFrontendHostAPI.Events.FileSystemFilesChangedAddedRemoved,
            {changed: [fullPath], added: [], removed: []});
    },

    createReader: function()
    {
        return new InspectorTest.TestFileSystem.Reader(this._children);
    },

    createWriter: function(success, failure)
    {
        success(new InspectorTest.TestFileSystem.Writer(this));
    },

    file: function(callback)
    {
        callback(this.content);
    },

    getDirectory: function(path, noop, callback, errorCallback)
    {
        this.getEntry(path, noop, callback, errorCallback);
    },

    getFile: function(path, noop, callback, errorCallback)
    {
        this.getEntry(path, noop, callback, errorCallback);
    },

    getEntry: function(path, noop, callback, errorCallback)
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
        entry ? callback(entry) : errorCallback(new DOMException('Path not found: ' + path, 'NotFoundError'));
    },

    getMetadata: function(success, failure)
    {
        success({
            modificationTime: new Date(this._timestamp),
            size: this.isDirectory ? 0 : this.content.size
        });
    },

    moveTo: function(parent, newName, callback, errorCallback)
    {
        this._parent._children.remove(this);
        this._parent = parent;
        this._parent._children.push(this);
        this.name = newName;
        callback(this);
    },

    getParent: function(callback, errorCallback)
    {
        callback(this._parent);
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

InspectorTest.TestFileSystem.Writer = function(entry)
{
    this._entry = entry;
    this._modificationTimesDelta = 500;
}

InspectorTest.TestFileSystem.Writer.prototype = {
    write: function(blob)
    {
        this._entry._timestamp += this._modificationTimesDelta;
        this._entry.content = blob;
        if (this.onwriteend)
            this.onwriteend();
    },

    truncate: function(num)
    {
        this._entry._timestamp += this._modificationTimesDelta;
        this._entry.content = this._entry.content.slice(0, num);
        if (this.onwriteend)
            this.onwriteend();
    }
}

};
