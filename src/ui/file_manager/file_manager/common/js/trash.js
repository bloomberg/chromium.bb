// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Trash implementation is based on
 * https://specifications.freedesktop.org/trash-spec/trashspec-1.0.html.
 *
 * When you move /dir/hello.txt to trash, you get:
 *  .Trash/files/hello.txt
 *  .Trash/info/hello.trashinfo
 *
 * .Trash/files/hello.txt is the original file.  .Trash/files.hello.trashinfo is
 * a text file which looks like:
 *  [Trash Info]
 *  Path=/dir/hello.txt
 *  DeletionDate=2020-11-02T07:35:38.964Z
 *
 * TrashEntry combines both files for display.
 */

// clang-format off
// #import {VolumeManager} from '../../externs/volume_manager.m.js';
// #import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.m.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {util} from './util.m.js';
// #import {FakeEntryImpl, CombinedReaders} from './files_app_entry_types.m.js';
// #import {VolumeManagerCommon} from './volume_manager_types.m.js';
// clang-format on

/**
 * Configuration for where Trash is stored in a volume.
 */
/* #export */ class TrashConfig {
  /**
   * @param {VolumeManagerCommon.VolumeType} volumeType
   * @param {string} topDir Top directory of volume. Must end with a slash to
   *     make comparisons simpler.
   * @param {string} trashDir Trash directory. Must end with a slash to make
   *     comparisons simpler.
   * @param {boolean=} prefixPathWithRemoteMount Optional, if true, 'Path=' in
   *     *.trashinfo is prefixed with the volume.remoteMountPath. For crostini,
   *     this is the user's homedir (/home/<username>).
   */
  constructor(volumeType, topDir, trashDir, prefixPathWithRemoteMount = false) {
    this.id = `${volumeType}-${topDir}`;
    this.volumeType = volumeType;
    this.topDir = topDir;
    this.trashDir = trashDir;
    this.prefixPathWithRemoteMount = prefixPathWithRemoteMount;
    this.pathPrefix = '';
  }
}

/**
 * Volumes supported for Trash, and location of Trash dir. Items will be
 * searched in order.
 *
 * @type {!Array<!TrashConfig>}
 */
TrashConfig.CONFIG = [
  // MyFiles/Downloads is a separate volume on a physical device, and doing a
  // move from MyFiles/Downloads/<path> to MyFiles/.Trash actually does a
  // copy across volumes, so we have a dedicated MyFiles/Downloads/.Trash.
  new TrashConfig(
      VolumeManagerCommon.VolumeType.DOWNLOADS, '/Downloads/',
      '/Downloads/.Trash/'),
  new TrashConfig(VolumeManagerCommon.VolumeType.DOWNLOADS, '/', '/.Trash/'),
  new TrashConfig(
      VolumeManagerCommon.VolumeType.CROSTINI, '/', '/.local/share/Trash/',
      /*prefixPathWithRemoteMount=*/ true),
];

/**
 * Wrapper for /.Trash/files and /.Trash/info directories.
 */
/* #export */ class TrashDirs {
  /**
   * @param {!DirectoryEntry} files /.Trash/files directory entry.
   * @param {!DirectoryEntry} info /.Trash/info directory entry.
   */
  constructor(files, info) {
    this.files = files;
    this.info = info;
  }

  /**
   * Promise wrapper for FileSystemDirectoryEntry.getDirectory().
   *
   * @param {!DirectoryEntry} dirEntry current directory.
   * @param {string} path name of directory within dirEntry.
   * @param {boolean} create if true, directory is created if it does not exist.
   * @return {!Promise<?DirectoryEntry>} Promise which resolves with
   *     <dirEntry>/<path> or null if entry does not exist and create is false.
   */
  static getDirectory(dirEntry, path, create) {
    return new Promise((resolve, reject) => {
      dirEntry.getDirectory(path, {create}, resolve, () => resolve(null));
    });
  }

  /**
   * Get trash dirs from file system as specified in config.
   *
   * @param {!FileSystem} fileSystem File system from volume with trash.
   * @param {!TrashConfig} config Config specifying trash dir location.
   * @param {boolean} create if true, dirs are created if they do not exist.
   * @return {!Promise<?TrashDirs>} Promise which resolves with trash dirs, or
   *     null if dirs do not exist and create is false.
   */
  static async getTrashDirs(fileSystem, config, create) {
    let trashRoot = fileSystem.root;
    const parts = config.trashDir.split('/');
    for (const part of parts) {
      if (part) {
        trashRoot = await TrashDirs.getDirectory(trashRoot, part, create);
        if (!trashRoot) {
          return null;
        }
      }
    }
    const files = await TrashDirs.getDirectory(trashRoot, 'files', create);
    const info = await TrashDirs.getDirectory(trashRoot, 'info', create);
    return files && info ? new TrashDirs(files, info) : null;
  }
}

/**
 * Represents a file moved to trash. Combines the info from both .Trash/info and
 * ./Trash/files.
 *
 * @implements {FilesAppEntry}
 */
/* #export */ class TrashEntry {
  /**
   * @param {string} name Name of the file deleted.
   * @param {!Date} deletionDate DeletionDate of deleted file from infoEntry.
   * @param {!Entry} filesEntry trash files entry.
   * @param {!FileEntry} infoEntry trash info entry.
   */
  constructor(name, deletionDate, filesEntry, infoEntry) {
    this.name = name;
    this.filesEntry = filesEntry;
    this.infoEntry = infoEntry;

    /** @private */
    this.deletionDate_ = deletionDate;

    /** @override Entry */
    this.filesystem = filesEntry.filesystem;

    /** @override Entry */
    this.fullPath = filesEntry.fullPath;

    /** @override Entry */
    this.isDirectory = filesEntry.isDirectory;

    /** @override Entry  */
    this.isFile = filesEntry.isFile;

    /** @override FilesAppEntry */
    this.rootType = VolumeManagerCommon.RootType.TRASH;

    /** @override FilesAppEntry */
    this.type_name = 'TrashEntry';
  }

  /**
   * Use filesEntry toURL() so this entry can be used as that file to view,
   * copy, etc.
   *
   * @override Entry
   */
  toURL() {
    return this.filesEntry.toURL();
  }

  /**
   * Pass through to getMetadata() of filesEntry, keep size, but use
   * DeletionDate from infoEntry for modificationTime.
   *
   * @override Entry
   */
  getMetadata(success, error) {
    this.filesEntry.getMetadata(m => {
      success({modificationTime: this.deletionDate_, size: m.size});
    }, error);
  }

  /** @override Entry */
  getParent() {}

  /**
   * Remove filesEntry first, then remove infoEntry. Overrides Entry.
   *
   * @param {function()} success
   * @param {function(!FileError)=} error
   */
  remove(success, error) {
    this.filesEntry.remove(() => this.infoEntry.remove(success, error), error);
  }

  /**
   * Pass through to filesEntry. Overrides FileEntry.
   *
   * @param {function(!File)} success
   * @param {function(!FileError)=} error
   */
  file(success, error) {
    this.filesEntry.file(success, error);
  }

  /**
   * Pass through to filesEntry. Overrides DirectoryEntry.
   *
   * @return {!DirectoryReader}
   */
  createReader() {
    return this.filesEntry.createReader();
  }

  /**
   * Pass through to filesEntry. Overrides DirectoryEntry.
   *
   * @param {string} path
   * @param {!Object} options
   * @param {function(!File)} success
   * @param {function(!FileError)=} error
   */
  getDirectory(path, options, success, error) {
    this.filesEntry.createReader(path, options, success, error);
  }

  /**
   * Pass through to filesEntry. Overrides DirectoryEntry.
   *
   * @param {string} path
   * @param {!Object} options
   * @param {function(!File)} success
   * @param {function(!FileError)=} error
   */
  getFile(path, options, success, error) {
    this.filesEntry.getFile(path, options, success, error);
  }

  /**
   * Remove filesEntry first, then remove infoEntry. Overrides DirectoryEntry.
   *
   * @param {function()} success
   * @param {function(!FileError)=} error
   */
  removeRecursively(success, error) {
    this.filesEntry.removeRecursively(
        () => this.infoEntry.remove(success, error), error);
  }

  /**
   * We must set entry.isNativeType to true, so that this is not considered a
   * FakeEntry, and we are allowed to delete the item.
   *
   * @override FilesAppEntry
   */
  get isNativeType() {
    return true;
  }

  /** @override FilesAppEntry */
  getNativeEntry() {
    return this.filesEntry;
  }

  /**
   * Parse Path from info entry text, or null if parse fails.
   * @param {string} text text of info entry.
   * @return {?string} path or null if parse fails.
   */
  static parsePath(text) {
    const found = text.match(/^Path=(.*)/m);
    return found ? found[1] : null;
  }

  /**
   * Parse DeletionDate from info entry text, or null if parse fails.
   * @param {string} text text of info entry.
   * @return {?Date} deletion date or null if parse fails.
   */
  static parseDeletionDate(text) {
    const found = text.match(/^DeletionDate=(.*)/m);
    if (found) {
      const n = Date.parse(found[1]);
      if (!Number.isNaN(n)) {
        return new Date(n);
      }
    }
    return null;
  }
}

/**
 * Reads all entries in each of .Trash/info and .Trash/files and produces a
 * single stream of TrashEntry.
 *
 * @extends {DirectoryReader}
 */
class TrashDirectoryReader {
  /**
   * @param {!FileSystem} fileSystem trash file system.
   * @param {!TrashConfig} config trash config.
   * @param {string} rootLabel Label of trash root to prefix entries with.
   */
  constructor(fileSystem, config, rootLabel) {
    this.fileSystem_ = fileSystem;
    this.config_ = config;
    this.rootLabel_ = rootLabel;

    /** @private {!Object<!Entry>} all entries in .Trash/files keyed by name. */
    this.filesEntries_ = {};

    /**
     * DirectoryReader of .Trash/info which needs to be persisted across calls
     * to readEntries().
     *
     * @private {?DirectoryReader}
     */
    this.infoReader_ = null;
  }

  /**
   * Create a trash entry if infoEntry and matching files entry are valid, else
   * return null.
   *
   * @param {!FileEntry} infoEntry trash info entry.
   * @return {!Promise<TrashEntry?>}
   */
  async createTrashEntry_(infoEntry) {
    const error = (msg, text = '') => {
      console.warn(`${msg}: ${infoEntry.toURL()}: ${text}`);
      return null;
    };

    // Ignore any directories.
    if (!infoEntry.isFile) {
      return error('Ignoring unexpected trash info directory');
    }

    // Ignore any files not *.trashinfo.
    if (!infoEntry.name.endsWith('.trashinfo')) {
      return error('Ignoring unexpected trash info file');
    }

    const fileName = infoEntry.name.substring(0, infoEntry.name.length - 10);
    const filesEntry = this.filesEntries_[fileName];
    delete this.filesEntries_[fileName];

    // Ignore any .trashinfo file with no matching file entry.
    if (!filesEntry) {
      return error('Ignoring trash info file with no matching files entry');
    }

    let text;
    try {
      const file = await new Promise(
          (resolve, reject) => infoEntry.file(resolve, reject));
      text = await file.text();
    } catch (e) {
      return error('Ignoring removed file');
    }
    const path = TrashEntry.parsePath(text);
    if (!path) {
      return error('Ignoring trash info file with no Path', text);
    }

    const deletionDate = TrashEntry.parseDeletionDate(text);
    if (!deletionDate) {
      return error('Ignoring trash info file with invalid DeletionDate', text);
    }

    // Show the root label and the whole Path=<path> from infoEntry as the name.
    // This allows users to differentiate deleted files such as /a/hello.txt and
    // /b/hello.txt.
    const trashName = this.rootLabel_ + path.replace(/\//g, ' › ');
    return new TrashEntry(trashName, deletionDate, filesEntry, infoEntry);
  }

  /**
   * Async version of readEntries(). This function may be called multiple times
   * and returns an empty result to indicate end of stream.
   *
   * Reads all items in .Trash/files on first call and caches them. Then reads
   * 1 or more batches of infoReader until we have at least 1 valid result to
   * send, or reader is exhausted.
   *
   * @param {function(!Array<!Entry>)} success
   * @param {function(!FileError)=} error
   */
  async readEntriesAsync_(success, error) {
    const ls = (reader) => {
      return new Promise((resolve, reject) => {
        reader.readEntries(results => resolve(results), error => reject(error));
      });
    };

    // Read all of .Trash/files on first call.
    if (!this.infoReader_) {
      const trashDirs = await TrashDirs.getTrashDirs(
          this.fileSystem_, this.config_, /*create=*/ false);
      // If trash dirs do not yet exist, then return succesful empty read.
      if (!trashDirs) {
        return success([]);
      }

      // Get all entries in trash/files.
      const filesReader = trashDirs.files.createReader();
      try {
        while (true) {
          const entries = await ls(filesReader);
          if (!entries.length) {
            break;
          }
          entries.forEach(entry => this.filesEntries_[entry.name] = entry);
        }
      } catch (e) {
        console.warn('Error reading trash files entries', e);
        error(e);
        return;
      }

      this.infoReader_ = trashDirs.info.createReader();
    }

    // Consume infoReader which is initialized in the first call. Read from
    // .Trash/info until we have at least 1 result, or end of stream.
    const result = [];
    while (true) {
      let entries = [];
      try {
        entries = await ls(this.infoReader_);
      } catch (e) {
        console.warn('Error reading trash info entries', e);
        error(e);
        return;
      }
      for (const e of entries) {
        const trashEntry = await this.createTrashEntry_(e);
        if (trashEntry) {
          result.push(trashEntry);
        }
      }
      if (!entries.length || result.length) {
        break;
      }
    }
    success(result);
  }

  /** @override */
  readEntries(success, error) {
    this.readEntriesAsync_(success, error);
  }
}

/**
 * Root Trash entry sits inside "My files". It shows the combined entries of
 * trashes defined in TrashConfig.
 */
/* #export */ class TrashRootEntry extends FakeEntryImpl {
  /**
   * @param {!VolumeManager} volumeManager
   */
  constructor(volumeManager) {
    super('Trash', VolumeManagerCommon.RootType.TRASH);
    this.volumeManager_ = volumeManager;
  }

  /** @override */
  createReader() {
    const readers = [];
    TrashConfig.CONFIG.forEach(c => {
      const info =
          this.volumeManager_.getCurrentProfileVolumeInfo(c.volumeType);
      if (info && info.fileSystem) {
        const locationInfo =
            this.volumeManager_.getLocationInfo(info.fileSystem.root);
        const rootLabel = util.getRootTypeLabel(assert(locationInfo));
        readers.push(new TrashDirectoryReader(info.fileSystem, c, rootLabel));
      }
    });
    return new CombinedReaders(readers);
  }
}
