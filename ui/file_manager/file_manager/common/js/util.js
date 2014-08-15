// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for utility functions.
 */
var util = {};

/**
 * Returns a function that console.log's its arguments, prefixed by |msg|.
 *
 * @param {string} msg The message prefix to use in the log.
 * @param {function(...string)=} opt_callback A function to invoke after
 *     logging.
 * @return {function(...string)} Function that logs.
 */
util.flog = function(msg, opt_callback) {
  return function() {
    var ary = Array.apply(null, arguments);
    console.log(msg + ': ' + ary.join(', '));
    if (opt_callback)
      opt_callback.apply(null, arguments);
  };
};

/**
 * Returns a function that throws an exception that includes its arguments
 * prefixed by |msg|.
 *
 * @param {string} msg The message prefix to use in the exception.
 * @return {function(...string)} Function that throws.
 */
util.ferr = function(msg) {
  return function() {
    var ary = Array.apply(null, arguments);
    throw new Error(msg + ': ' + ary.join(', '));
  };
};

/**
 * @param {string} name File error name.
 * @return {string} Translated file error string.
 */
util.getFileErrorString = function(name) {
  var candidateMessageFragment;
  switch (name) {
    case 'NotFoundError':
      candidateMessageFragment = 'NOT_FOUND';
      break;
    case 'SecurityError':
      candidateMessageFragment = 'SECURITY';
      break;
    case 'NotReadableError':
      candidateMessageFragment = 'NOT_READABLE';
      break;
    case 'NoModificationAllowedError':
      candidateMessageFragment = 'NO_MODIFICATION_ALLOWED';
      break;
    case 'InvalidStateError':
      candidateMessageFragment = 'INVALID_STATE';
      break;
    case 'InvalidModificationError':
      candidateMessageFragment = 'INVALID_MODIFICATION';
      break;
    case 'PathExistsError':
      candidateMessageFragment = 'PATH_EXISTS';
      break;
    case 'QuotaExceededError':
      candidateMessageFragment = 'QUOTA_EXCEEDED';
      break;
  }

  return loadTimeData.getString('FILE_ERROR_' + candidateMessageFragment) ||
      loadTimeData.getString('FILE_ERROR_GENERIC');
};

/**
 * Mapping table for FileError.code style enum to DOMError.name string.
 *
 * @enum {string}
 * @const
 */
util.FileError = Object.freeze({
  ABORT_ERR: 'AbortError',
  INVALID_MODIFICATION_ERR: 'InvalidModificationError',
  INVALID_STATE_ERR: 'InvalidStateError',
  NO_MODIFICATION_ALLOWED_ERR: 'NoModificationAllowedError',
  NOT_FOUND_ERR: 'NotFoundError',
  NOT_READABLE_ERR: 'NotReadable',
  PATH_EXISTS_ERR: 'PathExistsError',
  QUOTA_EXCEEDED_ERR: 'QuotaExceededError',
  TYPE_MISMATCH_ERR: 'TypeMismatchError',
  ENCODING_ERR: 'EncodingError',
});

/**
 * @param {string} str String to escape.
 * @return {string} Escaped string.
 */
util.htmlEscape = function(str) {
  return str.replace(/[<>&]/g, function(entity) {
    switch (entity) {
      case '<': return '&lt;';
      case '>': return '&gt;';
      case '&': return '&amp;';
    }
  });
};

/**
 * @param {string} str String to unescape.
 * @return {string} Unescaped string.
 */
util.htmlUnescape = function(str) {
  return str.replace(/&(lt|gt|amp);/g, function(entity) {
    switch (entity) {
      case '&lt;': return '<';
      case '&gt;': return '>';
      case '&amp;': return '&';
    }
  });
};

/**
 * Iterates the entries contained by dirEntry, and invokes callback once for
 * each entry. On completion, successCallback will be invoked.
 *
 * @param {DirectoryEntry} dirEntry The entry of the directory.
 * @param {function(Entry, function())} callback Invoked for each entry.
 * @param {function()} successCallback Invoked on completion.
 * @param {function(FileError)} errorCallback Invoked if an error is found on
 *     directory entry reading.
 */
util.forEachDirEntry = function(
    dirEntry, callback, successCallback, errorCallback) {
  var reader = dirEntry.createReader();
  var iterate = function() {
    reader.readEntries(function(entries) {
      if (entries.length == 0) {
        successCallback();
        return;
      }

      AsyncUtil.forEach(
          entries,
          function(forEachCallback, entry) {
            // Do not pass index nor entries.
            callback(entry, forEachCallback);
          },
          iterate);
    }, errorCallback);
  };
  iterate();
};

/**
 * Reads contents of directory.
 * @param {DirectoryEntry} root Root entry.
 * @param {string} path Directory path.
 * @param {function(Array.<Entry>)} callback List of entries passed to callback.
 */
util.readDirectory = function(root, path, callback) {
  var onError = function(e) {
    callback([], e);
  };
  root.getDirectory(path, {create: false}, function(entry) {
    var reader = entry.createReader();
    var r = [];
    var readNext = function() {
      reader.readEntries(function(results) {
        if (results.length == 0) {
          callback(r, null);
          return;
        }
        r.push.apply(r, results);
        readNext();
      }, onError);
    };
    readNext();
  }, onError);
};

/**
 * Utility function to resolve multiple directories with a single call.
 *
 * The successCallback will be invoked once for each directory object
 * found.  The errorCallback will be invoked once for each
 * path that could not be resolved.
 *
 * The successCallback is invoked with a null entry when all paths have
 * been processed.
 *
 * @param {DirEntry} dirEntry The base directory.
 * @param {Object} params The parameters to pass to the underlying
 *     getDirectory calls.
 * @param {Array.<string>} paths The list of directories to resolve.
 * @param {function(!DirEntry)} successCallback The function to invoke for
 *     each DirEntry found.  Also invoked once with null at the end of the
 *     process.
 * @param {function(FileError)} errorCallback The function to invoke
 *     for each path that cannot be resolved.
 */
util.getDirectories = function(dirEntry, params, paths, successCallback,
                               errorCallback) {

  // Copy the params array, since we're going to destroy it.
  params = [].slice.call(params);

  var onComplete = function() {
    successCallback(null);
  };

  var getNextDirectory = function() {
    var path = paths.shift();
    if (!path)
      return onComplete();

    dirEntry.getDirectory(
      path, params,
      function(entry) {
        successCallback(entry);
        getNextDirectory();
      },
      function(err) {
        errorCallback(err);
        getNextDirectory();
      });
  };

  getNextDirectory();
};

/**
 * Utility function to resolve multiple files with a single call.
 *
 * The successCallback will be invoked once for each directory object
 * found.  The errorCallback will be invoked once for each
 * path that could not be resolved.
 *
 * The successCallback is invoked with a null entry when all paths have
 * been processed.
 *
 * @param {DirEntry} dirEntry The base directory.
 * @param {Object} params The parameters to pass to the underlying
 *     getFile calls.
 * @param {Array.<string>} paths The list of files to resolve.
 * @param {function(!FileEntry)} successCallback The function to invoke for
 *     each FileEntry found.  Also invoked once with null at the end of the
 *     process.
 * @param {function(FileError)} errorCallback The function to invoke
 *     for each path that cannot be resolved.
 */
util.getFiles = function(dirEntry, params, paths, successCallback,
                         errorCallback) {
  // Copy the params array, since we're going to destroy it.
  params = [].slice.call(params);

  var onComplete = function() {
    successCallback(null);
  };

  var getNextFile = function() {
    var path = paths.shift();
    if (!path)
      return onComplete();

    dirEntry.getFile(
      path, params,
      function(entry) {
        successCallback(entry);
        getNextFile();
      },
      function(err) {
        errorCallback(err);
        getNextFile();
      });
  };

  getNextFile();
};

/**
 * Renames the entry to newName.
 * @param {Entry} entry The entry to be renamed.
 * @param {string} newName The new name.
 * @param {function(Entry)} successCallback Callback invoked when the rename
 *     is successfully done.
 * @param {function(FileError)} errorCallback Callback invoked when an error
 *     is found.
 */
util.rename = function(entry, newName, successCallback, errorCallback) {
  entry.getParent(function(parent) {
    // Before moving, we need to check if there is an existing entry at
    // parent/newName, since moveTo will overwrite it.
    // Note that this way has some timing issue. After existing check,
    // a new entry may be create on background. However, there is no way not to
    // overwrite the existing file, unfortunately. The risk should be low,
    // assuming the unsafe period is very short.
    (entry.isFile ? parent.getFile : parent.getDirectory).call(
        parent, newName, {create: false},
        function(entry) {
          // The entry with the name already exists.
          errorCallback(util.createDOMError(util.FileError.PATH_EXISTS_ERR));
        },
        function(error) {
          if (error.name != util.FileError.NOT_FOUND_ERR) {
            // Unexpected error is found.
            errorCallback(error);
            return;
          }

          // No existing entry is found.
          entry.moveTo(parent, newName, successCallback, errorCallback);
        });
  }, errorCallback);
};

/**
 * Remove a file or a directory.
 * @param {Entry} entry The entry to remove.
 * @param {function()} onSuccess The success callback.
 * @param {function(FileError)} onError The error callback.
 */
util.removeFileOrDirectory = function(entry, onSuccess, onError) {
  if (entry.isDirectory)
    entry.removeRecursively(onSuccess, onError);
  else
    entry.remove(onSuccess, onError);
};

/**
 * Convert a number of bytes into a human friendly format, using the correct
 * number separators.
 *
 * @param {number} bytes The number of bytes.
 * @return {string} Localized string.
 */
util.bytesToString = function(bytes) {
  // Translation identifiers for size units.
  var UNITS = ['SIZE_BYTES',
               'SIZE_KB',
               'SIZE_MB',
               'SIZE_GB',
               'SIZE_TB',
               'SIZE_PB'];

  // Minimum values for the units above.
  var STEPS = [0,
               Math.pow(2, 10),
               Math.pow(2, 20),
               Math.pow(2, 30),
               Math.pow(2, 40),
               Math.pow(2, 50)];

  var str = function(n, u) {
    // TODO(rginda): Switch to v8Locale's number formatter when it's
    // available.
    return strf(u, n.toLocaleString());
  };

  var fmt = function(s, u) {
    var rounded = Math.round(bytes / s * 10) / 10;
    return str(rounded, u);
  };

  // Less than 1KB is displayed like '80 bytes'.
  if (bytes < STEPS[1]) {
    return str(bytes, UNITS[0]);
  }

  // Up to 1MB is displayed as rounded up number of KBs.
  if (bytes < STEPS[2]) {
    var rounded = Math.ceil(bytes / STEPS[1]);
    return str(rounded, UNITS[1]);
  }

  // This loop index is used outside the loop if it turns out |bytes|
  // requires the largest unit.
  var i;

  for (i = 2 /* MB */; i < UNITS.length - 1; i++) {
    if (bytes < STEPS[i + 1])
      return fmt(STEPS[i], UNITS[i]);
  }

  return fmt(STEPS[i], UNITS[i]);
};

/**
 * Utility function to read specified range of bytes from file
 * @param {File} file The file to read.
 * @param {number} begin Starting byte(included).
 * @param {number} end Last byte(excluded).
 * @param {function(File, Uint8Array)} callback Callback to invoke.
 * @param {function(FileError)} onError Error handler.
 */
util.readFileBytes = function(file, begin, end, callback, onError) {
  var fileReader = new FileReader();
  fileReader.onerror = onError;
  fileReader.onloadend = function() {
    callback(file, new ByteReader(fileReader.result));
  };
  fileReader.readAsArrayBuffer(file.slice(begin, end));
};

/**
 * Write a blob to a file.
 * Truncates the file first, so the previous content is fully overwritten.
 * @param {FileEntry} entry File entry.
 * @param {Blob} blob The blob to write.
 * @param {function(Event)} onSuccess Completion callback. The first argument is
 *     a 'writeend' event.
 * @param {function(FileError)} onError Error handler.
 */
util.writeBlobToFile = function(entry, blob, onSuccess, onError) {
  var truncate = function(writer) {
    writer.onerror = onError;
    writer.onwriteend = write.bind(null, writer);
    writer.truncate(0);
  };

  var write = function(writer) {
    writer.onwriteend = onSuccess;
    writer.write(blob);
  };

  entry.createWriter(truncate, onError);
};

/**
 * Returns a string '[Ctrl-][Alt-][Shift-][Meta-]' depending on the event
 * modifiers. Convenient for writing out conditions in keyboard handlers.
 *
 * @param {Event} event The keyboard event.
 * @return {string} Modifiers.
 */
util.getKeyModifiers = function(event) {
  return (event.ctrlKey ? 'Ctrl-' : '') +
         (event.altKey ? 'Alt-' : '') +
         (event.shiftKey ? 'Shift-' : '') +
         (event.metaKey ? 'Meta-' : '');
};

/**
 * @param {HTMLElement} element Element to transform.
 * @param {Object} transform Transform object,
 *                           contains scaleX, scaleY and rotate90 properties.
 */
util.applyTransform = function(element, transform) {
  element.style.webkitTransform =
      transform ? 'scaleX(' + transform.scaleX + ') ' +
                  'scaleY(' + transform.scaleY + ') ' +
                  'rotate(' + transform.rotate90 * 90 + 'deg)' :
      '';
};

/**
 * Makes filesystem: URL from the path.
 * @param {string} path File or directory path.
 * @return {string} URL.
 */
util.makeFilesystemUrl = function(path) {
  path = path.split('/').map(encodeURIComponent).join('/');
  var prefix = 'external';
  return 'filesystem:' + chrome.runtime.getURL(prefix + path);
};

/**
 * Extracts path from filesystem: URL.
 * @param {string} url Filesystem URL.
 * @return {string} The path.
 */
util.extractFilePath = function(url) {
  var match =
      /^filesystem:[\w-]*:\/\/[\w]*\/(external|persistent|temporary)(\/.*)$/.
      exec(url);
  var path = match && match[2];
  if (!path) return null;
  return decodeURIComponent(path);
};

/**
 * Traverses a directory tree whose root is the given entry, and invokes
 * callback for each entry. Upon completion, successCallback will be called.
 * On error, errorCallback will be called.
 *
 * @param {Entry} entry The root entry.
 * @param {function(Entry):boolean} callback Callback invoked for each entry.
 *     If this returns false, entries under it won't be traversed. Note that
 *     its siblings (and their children) will be still traversed.
 * @param {function()} successCallback Called upon successful completion.
 * @param {function(error)} errorCallback Called upon error.
 */
util.traverseTree = function(entry, callback, successCallback, errorCallback) {
  if (!callback(entry)) {
    successCallback();
    return;
  }

  util.forEachDirEntry(
      entry,
      function(child, iterationCallback) {
        util.traverseTree(child, callback, iterationCallback, errorCallback);
      },
      successCallback,
      errorCallback);
};

/**
 * A shortcut function to create a child element with given tag and class.
 *
 * @param {HTMLElement} parent Parent element.
 * @param {string=} opt_className Class name.
 * @param {string=} opt_tag Element tag, DIV is omitted.
 * @return {Element} Newly created element.
 */
util.createChild = function(parent, opt_className, opt_tag) {
  var child = parent.ownerDocument.createElement(opt_tag || 'div');
  if (opt_className)
    child.className = opt_className;
  parent.appendChild(child);
  return child;
};

/**
 * Updates the app state.
 *
 * @param {string} currentDirectoryURL Currently opened directory as an URL.
 *     If null the value is left unchanged.
 * @param {string} selectionURL Currently selected entry as an URL. If null the
 *     value is left unchanged.
 * @param {string|Object=} opt_param Additional parameters, to be stored. If
 *     null, then left unchanged.
 */
util.updateAppState = function(currentDirectoryURL, selectionURL, opt_param) {
  window.appState = window.appState || {};
  if (opt_param !== undefined && opt_param !== null)
    window.appState.params = opt_param;
  if (currentDirectoryURL !== null)
    window.appState.currentDirectoryURL = currentDirectoryURL;
  if (selectionURL !== null)
    window.appState.selectionURL = selectionURL;
  util.saveAppState();
};

/**
 * Returns a translated string.
 *
 * Wrapper function to make dealing with translated strings more concise.
 * Equivalent to loadTimeData.getString(id).
 *
 * @param {string} id The id of the string to return.
 * @return {string} The translated string.
 */
function str(id) {
  return loadTimeData.getString(id);
}

/**
 * Returns a translated string with arguments replaced.
 *
 * Wrapper function to make dealing with translated strings more concise.
 * Equivalent to loadTimeData.getStringF(id, ...).
 *
 * @param {string} id The id of the string to return.
 * @param {...string} var_args The values to replace into the string.
 * @return {string} The translated string with replaced values.
 */
function strf(id, var_args) {
  return loadTimeData.getStringF.apply(loadTimeData, arguments);
}

/**
 * Adapter object that abstracts away the the difference between Chrome app APIs
 * v1 and v2. Is only necessary while the migration to v2 APIs is in progress.
 * TODO(mtomasz): Clean up this. crbug.com/240606.
 */
util.platform = {
  /**
   * @return {boolean} True if Files.app is running as an open files or a select
   *     folder dialog. False otherwise.
   */
  runningInBrowser: function() {
    return !window.appID;
  },

  /**
   * @param {function(Object)} callback Function accepting a preference map.
   */
  getPreferences: function(callback) {
    chrome.storage.local.get(callback);
  },

  /**
   * @param {string} key Preference name.
   * @param {function(string)} callback Function accepting the preference value.
   */
  getPreference: function(key, callback) {
    chrome.storage.local.get(key, function(items) {
      callback(items[key]);
    });
  },

  /**
   * @param {string} key Preference name.
   * @param {string|Object} value Preference value.
   * @param {function()=} opt_callback Completion callback.
   */
  setPreference: function(key, value, opt_callback) {
    if (typeof value != 'string')
      value = JSON.stringify(value);

    var items = {};
    items[key] = value;
    chrome.storage.local.set(items, opt_callback);
  }
};

/**
 * Attach page load handler.
 * @param {function()} handler Application-specific load handler.
 */
util.addPageLoadHandler = function(handler) {
  document.addEventListener('DOMContentLoaded', function() {
    handler();
  });
};

/**
 * Save app launch data to the local storage.
 */
util.saveAppState = function() {
  if (window.appState)
    util.platform.setPreference(window.appID, window.appState);
};

/**
 *  AppCache is a persistent timestamped key-value storage backed by
 *  HTML5 local storage.
 *
 *  It is not designed for frequent access. In order to avoid costly
 *  localStorage iteration all data is kept in a single localStorage item.
 *  There is no in-memory caching, so concurrent access is _almost_ safe.
 *
 *  TODO(kaznacheev) Reimplement this based on Indexed DB.
 */
util.AppCache = function() {};

/**
 * Local storage key.
 */
util.AppCache.KEY = 'AppCache';

/**
 * Max number of items.
 */
util.AppCache.CAPACITY = 100;

/**
 * Default lifetime.
 */
util.AppCache.LIFETIME = 30 * 24 * 60 * 60 * 1000;  // 30 days.

/**
 * @param {string} key Key.
 * @param {function(number)} callback Callback accepting a value.
 */
util.AppCache.getValue = function(key, callback) {
  util.AppCache.read_(function(map) {
    var entry = map[key];
    callback(entry && entry.value);
  });
};

/**
 * Update the cache.
 *
 * @param {string} key Key.
 * @param {string} value Value. Remove the key if value is null.
 * @param {number=} opt_lifetime Maximum time to keep an item (in milliseconds).
 */
util.AppCache.update = function(key, value, opt_lifetime) {
  util.AppCache.read_(function(map) {
    if (value != null) {
      map[key] = {
        value: value,
        expire: Date.now() + (opt_lifetime || util.AppCache.LIFETIME)
      };
    } else if (key in map) {
      delete map[key];
    } else {
      return;  // Nothing to do.
    }
    util.AppCache.cleanup_(map);
    util.AppCache.write_(map);
  });
};

/**
 * @param {function(Object)} callback Callback accepting a map of timestamped
 *   key-value pairs.
 * @private
 */
util.AppCache.read_ = function(callback) {
  util.platform.getPreference(util.AppCache.KEY, function(json) {
    if (json) {
      try {
        callback(JSON.parse(json));
      } catch (e) {
        // The local storage item somehow got messed up, start fresh.
      }
    }
    callback({});
  });
};

/**
 * @param {Object} map A map of timestamped key-value pairs.
 * @private
 */
util.AppCache.write_ = function(map) {
  util.platform.setPreference(util.AppCache.KEY, JSON.stringify(map));
};

/**
 * Remove over-capacity and obsolete items.
 *
 * @param {Object} map A map of timestamped key-value pairs.
 * @private
 */
util.AppCache.cleanup_ = function(map) {
  // Sort keys by ascending timestamps.
  var keys = [];
  for (var key in map) {
    if (map.hasOwnProperty(key))
      keys.push(key);
  }
  keys.sort(function(a, b) { return map[a].expire > map[b].expire; });

  var cutoff = Date.now();

  var obsolete = 0;
  while (obsolete < keys.length &&
         map[keys[obsolete]].expire < cutoff) {
    obsolete++;
  }

  var overCapacity = Math.max(0, keys.length - util.AppCache.CAPACITY);

  var itemsToDelete = Math.max(obsolete, overCapacity);
  for (var i = 0; i != itemsToDelete; i++) {
    delete map[keys[i]];
  }
};

/**
 * Load an image.
 *
 * @param {Image} image Image element.
 * @param {string} url Source url.
 * @param {Object=} opt_options Hash array of options, eg. width, height,
 *     maxWidth, maxHeight, scale, cache.
 * @param {function()=} opt_isValid Function returning false iff the task
 *     is not valid and should be aborted.
 * @return {?number} Task identifier or null if fetched immediately from
 *     cache.
 */
util.loadImage = function(image, url, opt_options, opt_isValid) {
  return ImageLoaderClient.loadToImage(url,
                                      image,
                                      opt_options || {},
                                      function() {},
                                      function() { image.onerror(); },
                                      opt_isValid);
};

/**
 * Cancels loading an image.
 * @param {number} taskId Task identifier returned by util.loadImage().
 */
util.cancelLoadImage = function(taskId) {
  ImageLoaderClient.getInstance().cancel(taskId);
};

/**
 * Finds proerty descriptor in the object prototype chain.
 * @param {Object} object The object.
 * @param {string} propertyName The property name.
 * @return {Object} Property descriptor.
 */
util.findPropertyDescriptor = function(object, propertyName) {
  for (var p = object; p; p = Object.getPrototypeOf(p)) {
    var d = Object.getOwnPropertyDescriptor(p, propertyName);
    if (d)
      return d;
  }
  return null;
};

/**
 * Calls inherited property setter (useful when property is
 * overridden).
 * @param {Object} object The object.
 * @param {string} propertyName The property name.
 * @param {*} value Value to set.
 */
util.callInheritedSetter = function(object, propertyName, value) {
  var d = util.findPropertyDescriptor(Object.getPrototypeOf(object),
                                      propertyName);
  d.set.call(object, value);
};

/**
 * Returns true if the board of the device matches the given prefix.
 * @param {string} boardPrefix The board prefix to match against.
 *     (ex. "x86-mario". Prefix is used as the actual board name comes with
 *     suffix like "x86-mario-something".
 * @return {boolean} True if the board of the device matches the given prefix.
 */
util.boardIs = function(boardPrefix) {
  // The board name should be lower-cased, but making it case-insensitive for
  // backward compatibility just in case.
  var board = str('CHROMEOS_RELEASE_BOARD');
  var pattern = new RegExp('^' + boardPrefix, 'i');
  return board.match(pattern) != null;
};

/**
 * Adds an isFocused method to the current window object.
 */
util.addIsFocusedMethod = function() {
  var focused = true;

  window.addEventListener('focus', function() {
    focused = true;
  });

  window.addEventListener('blur', function() {
    focused = false;
  });

  /**
   * @return {boolean} True if focused.
   */
  window.isFocused = function() {
    return focused;
  };
};

/**
 * Makes a redirect to the specified Files.app's window from another window.
 * @param {number} id Window id.
 * @param {string} url Target url.
 * @return {boolean} True if the window has been found. False otherwise.
 */
util.redirectMainWindow = function(id, url) {
  // TODO(mtomasz): Implement this for Apps V2, once the photo importer is
  // restored.
  return false;
};

/**
 * Checks, if the Files.app's window is in a full screen mode.
 *
 * @param {AppWindow} appWindow App window to be maximized.
 * @return {boolean} True if the full screen mode is enabled.
 */
util.isFullScreen = function(appWindow) {
  if (appWindow) {
    return appWindow.isFullscreen();
  } else {
    console.error('App window not passed. Unable to check status of ' +
                  'the full screen mode.');
    return false;
  }
};

/**
 * Toggles the full screen mode.
 *
 * @param {AppWindow} appWindow App window to be maximized.
 * @param {boolean} enabled True for enabling, false for disabling.
 */
util.toggleFullScreen = function(appWindow, enabled) {
  if (appWindow) {
    if (enabled)
      appWindow.fullscreen();
    else
      appWindow.restore();
    return;
  }

  console.error(
      'App window not passed. Unable to toggle the full screen mode.');
};

/**
 * The type of a file operation.
 * @enum {string}
 * @const
 */
util.FileOperationType = Object.freeze({
  COPY: 'COPY',
  MOVE: 'MOVE',
  ZIP: 'ZIP',
});

/**
 * The type of a file operation error.
 * @enum {number}
 * @const
 */
util.FileOperationErrorType = Object.freeze({
  UNEXPECTED_SOURCE_FILE: 0,
  TARGET_EXISTS: 1,
  FILESYSTEM_ERROR: 2,
});

/**
 * The kind of an entry changed event.
 * @enum {number}
 * @const
 */
util.EntryChangedKind = Object.freeze({
  CREATED: 0,
  DELETED: 1,
});

/**
 * Obtains whether an entry is fake or not.
 * @param {!Entry|!Object} entry Entry or a fake entry.
 * @return {boolean} True if the given entry is fake.
 */
util.isFakeEntry = function(entry) {
  return !('getParent' in entry);
};

/**
 * Creates an instance of UserDOMError with given error name that looks like a
 * FileError except that it does not have the deprecated FileError.code member.
 *
 * TODO(uekawa): remove reference to FileError.
 *
 * @param {string} name Error name for the file error.
 * @return {UserDOMError} FileError instance
 */
util.createDOMError = function(name) {
  return new util.UserDOMError(name);
};

/**
 * Creates a DOMError-like object to be used in place of returning file errors.
 *
 * @param {string} name Error name for the file error.
 * @constructor
 */
util.UserDOMError = function(name) {
  /**
   * @type {string}
   * @private
   */
  this.name_ = name;
  Object.freeze(this);
};

util.UserDOMError.prototype = {
  /**
   * @return {string} File error name.
   */
  get name() {
    return this.name_;
  }
};

/**
 * Compares two entries.
 * @param {Entry|Object} entry1 The entry to be compared. Can be a fake.
 * @param {Entry|Object} entry2 The entry to be compared. Can be a fake.
 * @return {boolean} True if the both entry represents a same file or
 *     directory. Returns true if both entries are null.
 */
util.isSameEntry = function(entry1, entry2) {
  if (!entry1 && !entry2)
    return true;
  if (!entry1 || !entry2)
    return false;
  return entry1.toURL() === entry2.toURL();
};

/**
 * Compares two file systems.
 * @param {DOMFileSystem} fileSystem1 The file system to be compared.
 * @param {DOMFileSystem} fileSystem2 The file system to be compared.
 * @return {boolean} True if the both file systems are equal. Also, returns true
 *     if both file systems are null.
 */
util.isSameFileSystem = function(fileSystem1, fileSystem2) {
  if (!fileSystem1 && !fileSystem2)
    return true;
  if (!fileSystem1 || !fileSystem2)
    return false;
  return util.isSameEntry(fileSystem1.root, fileSystem2.root);
};

/**
 * Collator for sorting.
 * @type {Intl.Collator}
 */
util.collator = new Intl.Collator([], {usage: 'sort',
                                       numeric: true,
                                       sensitivity: 'base'});

/**
 * Compare by name. The 2 entries must be in same directory.
 * @param {Entry} entry1 First entry.
 * @param {Entry} entry2 Second entry.
 * @return {number} Compare result.
 */
util.compareName = function(entry1, entry2) {
  return util.collator.compare(entry1.name, entry2.name);
};

/**
 * Compare by path.
 * @param {Entry} entry1 First entry.
 * @param {Entry} entry2 Second entry.
 * @return {number} Compare result.
 */
util.comparePath = function(entry1, entry2) {
  return util.collator.compare(entry1.fullPath, entry2.fullPath);
};

/**
 * Checks if the child entry is a descendant of another entry. If the entries
 * point to the same file or directory, then returns false.
 *
 * @param {DirectoryEntry|Object} ancestorEntry The ancestor directory entry.
 *     Can be a fake.
 * @param {Entry|Object} childEntry The child entry. Can be a fake.
 * @return {boolean} True if the child entry is contained in the ancestor path.
 */
util.isDescendantEntry = function(ancestorEntry, childEntry) {
  if (!ancestorEntry.isDirectory)
    return false;
  if (!util.isSameFileSystem(ancestorEntry.filesystem, childEntry.filesystem))
    return false;
  if (util.isSameEntry(ancestorEntry, childEntry))
    return false;
  if (util.isFakeEntry(ancestorEntry) || util.isFakeEntry(childEntry))
    return false;

  // Check if the ancestor's path with trailing slash is a prefix of child's
  // path.
  var ancestorPath = ancestorEntry.fullPath;
  if (ancestorPath.slice(-1) !== '/')
    ancestorPath += '/';
  return childEntry.fullPath.indexOf(ancestorPath) === 0;
};

/**
 * Visit the URL.
 *
 * If the browser is opening, the url is opened in a new tag, otherwise the url
 * is opened in a new window.
 *
 * @param {string} url URL to visit.
 */
util.visitURL = function(url) {
  window.open(url);
};

/**
 * Returns normalized current locale, or default locale - 'en'.
 * @return {string} Current locale
 */
util.getCurrentLocaleOrDefault = function() {
  // chrome.i18n.getMessage('@@ui_locale') can't be used in packed app.
  // Instead, we pass it from C++-side with strings.
  return str('UI_LOCALE') || 'en';
};

/**
 * Converts array of entries to an array of corresponding URLs.
 * @param {Array.<Entry>} entries Input array of entries.
 * @return {Array.<string>} Output array of URLs.
 */
util.entriesToURLs = function(entries) {
  // TODO(mtomasz): Make all callers use entries instead of URLs, and then
  // remove this utility function.
  console.warn('Converting entries to URLs is deprecated.');
  return entries.map(function(entry) {
     return entry.toURL();
  });
};

/**
 * Converts array of URLs to an array of corresponding Entries.
 *
 * @param {Array.<string>} urls Input array of URLs.
 * @param {function(Array.<Entry>, Array.<URL>)=} opt_callback Completion
 *     callback with array of success Entries and failure URLs.
 * @return {Promise} Promise fulfilled with the object that has entries property
 *     and failureUrls property. The promise is never rejected.
 */
util.URLsToEntries = function(urls, opt_callback) {
  var promises = urls.map(function(url) {
    return new Promise(webkitResolveLocalFileSystemURL.bind(null, url)).
        then(function(entry) {
          return {entry: entry};
        }, function(failureUrl) {
          // Not an error. Possibly, the file is not accessible anymore.
          console.warn('Failed to resolve the file with url: ' + url + '.');
          return {failureUrl: url};
        });
  });
  var resultPromise = Promise.all(promises).then(function(results) {
    var entries = [];
    var failureUrls = [];
    for (var i = 0; i < results.length; i++) {
      if ('entry' in results[i])
        entries.push(results[i].entry);
      if ('failureUrl' in results[i]) {
        failureUrls.push(results[i].failureUrl);
      }
    }
    return {
      entries: entries,
      failureUrls: failureUrls
    };
  });

  // Invoke the callback. If opt_callback is specified, resultPromise is still
  // returned and fulfilled with a result.
  if (opt_callback) {
    resultPromise.then(function(result) {
      opt_callback(result.entries, result.failureUrls);
    }).
    catch(function(error) {
      console.error(
          'util.URLsToEntries is failed.',
          error.stack ? error.stack : error);
    });
  }

  return resultPromise;
};

/**
 * Returns whether the window is teleported or not.
 * @param {DOMWindow} window Window.
 * @return {Promise.<boolean>} Whether the window is teleported or not.
 */
util.isTeleported = function(window) {
  return new Promise(function(onFulfilled) {
    window.chrome.fileBrowserPrivate.getProfiles(function(profiles,
                                                          currentId,
                                                          displayedId) {
      onFulfilled(currentId !== displayedId);
    });
  });
};

/**
 * Sets up and shows the alert to inform a user the task is opened in the
 * desktop of the running profile.
 *
 * TODO(hirono): Move the function from the util namespace.
 * @param {cr.ui.AlertDialog} alertDialog Alert dialog to be shown.
 * @param {Array.<Entry>} entries List of opened entries.
 */
util.showOpenInOtherDesktopAlert = function(alertDialog, entries) {
  if (!entries.length)
    return;
  chrome.fileBrowserPrivate.getProfiles(function(profiles,
                                                 currentId,
                                                 displayedId) {
    // Find strings.
    var displayName;
    for (var i = 0; i < profiles.length; i++) {
      if (profiles[i].profileId === currentId) {
        displayName = profiles[i].displayName;
        break;
      }
    }
    if (!displayName) {
      console.warn('Display name is not found.');
      return;
    }

    var title = entries.size > 1 ?
        entries[0].name + '\u2026' /* ellipsis */ : entries[0].name;
    var message = strf(entries.size > 1 ?
                       'OPEN_IN_OTHER_DESKTOP_MESSAGE_PLURAL' :
                       'OPEN_IN_OTHER_DESKTOP_MESSAGE',
                       displayName,
                       currentId);

    // Show the dialog.
    alertDialog.showWithTitle(title, message);
  }.bind(this));
};

/**
 * Runs chrome.test.sendMessage in test environment. Does nothing if running
 * in production environment.
 *
 * @param {string} message Test message to send.
 */
util.testSendMessage = function(message) {
  var test = chrome.test || window.top.chrome.test;
  if (test)
    test.sendMessage(message);
};

/**
 * Returns the localized name for the root type. If not available, then returns
 * null.
 *
 * @param {VolumeManagerCommon.RootType} rootType The root type.
 * @return {?string} The localized name, or null if not available.
 */
util.getRootTypeLabel = function(rootType) {
  var str = function(id) {
    return loadTimeData.getString(id);
  };

  switch (rootType) {
    case VolumeManagerCommon.RootType.DOWNLOADS:
      return str('DOWNLOADS_DIRECTORY_LABEL');
    case VolumeManagerCommon.RootType.DRIVE:
      return str('DRIVE_MY_DRIVE_LABEL');
    case VolumeManagerCommon.RootType.DRIVE_OFFLINE:
      return str('DRIVE_OFFLINE_COLLECTION_LABEL');
    case VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME:
      return str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL');
    case VolumeManagerCommon.RootType.DRIVE_RECENT:
      return str('DRIVE_RECENT_COLLECTION_LABEL');
  }

  // Translation not found.
  return null;
};

/**
 * Extracts the extension of the path.
 *
 * Examples:
 * util.splitExtension('abc.ext') -> ['abc', '.ext']
 * util.splitExtension('a/b/abc.ext') -> ['a/b/abc', '.ext']
 * util.splitExtension('a/b') -> ['a/b', '']
 * util.splitExtension('.cshrc') -> ['', '.cshrc']
 * util.splitExtension('a/b.backup/hoge') -> ['a/b.backup/hoge', '']
 *
 * @param {string} path Path to be extracted.
 * @return {Array.<string>} Filename and extension of the given path.
 */
util.splitExtension = function(path) {
  var dotPosition = path.lastIndexOf('.');
  if (dotPosition <= path.lastIndexOf('/'))
    dotPosition = -1;

  var filename = dotPosition != -1 ? path.substr(0, dotPosition) : path;
  var extension = dotPosition != -1 ? path.substr(dotPosition) : '';
  return [filename, extension];
};

/**
 * Returns the localized name of the entry.
 *
 * @param {VolumeManager} volumeManager The volume manager.
 * @param {Entry} entry The entry to be retrieve the name of.
 * @return {?string} The localized name.
 */
util.getEntryLabel = function(volumeManager, entry) {
  var locationInfo = volumeManager.getLocationInfo(entry);

  if (locationInfo && locationInfo.isRootEntry) {
    switch (locationInfo.rootType) {
      case VolumeManagerCommon.RootType.DOWNLOADS:
        return str('DOWNLOADS_DIRECTORY_LABEL');
      case VolumeManagerCommon.RootType.DRIVE:
        return str('DRIVE_MY_DRIVE_LABEL');
      case VolumeManagerCommon.RootType.DRIVE_OFFLINE:
        return str('DRIVE_OFFLINE_COLLECTION_LABEL');
      case VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME:
        return str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL');
      case VolumeManagerCommon.RootType.DRIVE_RECENT:
        return str('DRIVE_RECENT_COLLECTION_LABEL');
      case VolumeManagerCommon.RootType.DRIVE_OTHER:
      case VolumeManagerCommon.RootType.DOWNLOADS:
      case VolumeManagerCommon.RootType.ARCHIVE:
      case VolumeManagerCommon.RootType.REMOVABLE:
      case VolumeManagerCommon.RootType.MTP:
      case VolumeManagerCommon.RootType.PROVIDED:
        return locationInfo.volumeInfo.label;
      default:
        console.error('Unsupported root type: ' + locationInfo.rootType);
        return locationInfo.volumeInfo.label;
    }
  }

  return entry.name;
};

/**
 * Checks if the specified set of allowed effects contains the given effect.
 * See: http://www.w3.org/TR/html5/editing.html#the-datatransfer-interface
 *
 * @param {string} effectAllowed The string denoting the set of allowed effects.
 * @param {string} dropEffect The effect to be checked.
 * @return {boolean} True if |dropEffect| is included in |effectAllowed|.
 */
util.isDropEffectAllowed = function(effectAllowed, dropEffect) {
  return effectAllowed === 'all' ||
      effectAllowed.toLowerCase().indexOf(dropEffect) !== -1;
};

/**
 * Verifies the user entered name for file or folder to be created or
 * renamed to. Name restrictions must correspond to File API restrictions
 * (see DOMFilePath::isValidPath). Curernt WebKit implementation is
 * out of date (spec is
 * http://dev.w3.org/2009/dap/file-system/file-dir-sys.html, 8.3) and going to
 * be fixed. Shows message box if the name is invalid.
 *
 * It also verifies if the name length is in the limit of the filesystem.
 *
 * @param {DirectoryEntry} parentEntry The URL of the parent directory entry.
 * @param {string} name New file or folder name.
 * @param {boolean} filterHiddenOn Whether to report the hidden file name error
 *     or not.
 * @return {Promise} Promise fulfilled on success, or rejected with the error
 *     message.
 */
util.validateFileName = function(parentEntry, name, filterHiddenOn) {
  var testResult = /[\/\\\<\>\:\?\*\"\|]/.exec(name);
  var msg;
  if (testResult)
    return Promise.reject(strf('ERROR_INVALID_CHARACTER', testResult[0]));
  else if (/^\s*$/i.test(name))
    return Promise.reject(str('ERROR_WHITESPACE_NAME'));
  else if (/^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$/i.test(name))
    return Promise.reject(str('ERROR_RESERVED_NAME'));
  else if (filterHiddenOn && name[0] == '.')
    return Promise.reject(str('ERROR_HIDDEN_NAME'));

  return new Promise(function(fulfill, reject) {
    chrome.fileBrowserPrivate.validatePathNameLength(
        parentEntry.toURL(),
        name,
        function(valid) {
          if (valid)
            fulfill();
          else
            reject(str('ERROR_LONG_NAME'));
        });
  });
};
