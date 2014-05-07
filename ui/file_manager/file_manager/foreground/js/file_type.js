// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace object for file type utility functions.
 */
var FileType = {};

/**
 * Description of known file types.
 * Pair type-subtype defines order when sorted by file type.
 */
FileType.types = [
  // Images
  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'JPEG',
   pattern: /\.jpe?g$/i},
  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'BMP',
   pattern: /\.bmp$/i},
  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'GIF',
   pattern: /\.gif$/i},
  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'ICO',
   pattern: /\.ico$/i},
  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'PNG',
   pattern: /\.png$/i},
  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'WebP',
   pattern: /\.webp$/i},
  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'TIFF',
   pattern: /\.tiff?$/i},

  // Video
  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: '3GP',
   pattern: /\.3gp$/i},
  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'AVI',
   pattern: /\.avi$/i},
  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'QuickTime',
   pattern: /\.mov$/i},
  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'MKV',
   pattern: /\.mkv$/i},
  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'MPEG',
   pattern: /\.m(p4|4v|pg|peg|pg4|peg4)$/i},
  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'OGG',
   pattern: /\.og(m|v|x)$/i},
  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'WebM',
   pattern: /\.webm$/i},

  // Audio
  {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'AMR',
   pattern: /\.amr$/i},
  {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'FLAC',
   pattern: /\.flac$/i},
  {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'MP3',
   pattern: /\.mp3$/i},
  {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'MPEG',
   pattern: /\.m4a$/i},
  {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'OGG',
   pattern: /\.og(a|g)$/i},
  {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'WAV',
   pattern: /\.wav$/i},

  // Text
  {type: 'text', name: 'PLAIN_TEXT_FILE_TYPE', subtype: 'TXT',
   pattern: /\.txt$/i},

  // Archive
  {type: 'archive', name: 'ZIP_ARCHIVE_FILE_TYPE', subtype: 'ZIP',
   pattern: /\.zip$/i},
  {type: 'archive', name: 'RAR_ARCHIVE_FILE_TYPE', subtype: 'RAR',
   pattern: /\.rar$/i},
  {type: 'archive', name: 'TAR_ARCHIVE_FILE_TYPE', subtype: 'TAR',
   pattern: /\.tar$/i},
  {type: 'archive', name: 'TAR_BZIP2_ARCHIVE_FILE_TYPE', subtype: 'TBZ2',
   pattern: /\.(tar\.bz2|tbz|tbz2)$/i},
  {type: 'archive', name: 'TAR_GZIP_ARCHIVE_FILE_TYPE', subtype: 'TGZ',
   pattern: /\.(tar\.|t)gz$/i},

  // Hosted docs.
  {type: 'hosted', icon: 'gdoc', name: 'GDOC_DOCUMENT_FILE_TYPE',
   subtype: 'doc', pattern: /\.gdoc$/i},
  {type: 'hosted', icon: 'gsheet', name: 'GSHEET_DOCUMENT_FILE_TYPE',
   subtype: 'sheet', pattern: /\.gsheet$/i},
  {type: 'hosted', icon: 'gslides', name: 'GSLIDES_DOCUMENT_FILE_TYPE',
   subtype: 'slides', pattern: /\.gslides$/i},
  {type: 'hosted', icon: 'gdraw', name: 'GDRAW_DOCUMENT_FILE_TYPE',
   subtype: 'draw', pattern: /\.gdraw$/i},
  {type: 'hosted', icon: 'gtable', name: 'GTABLE_DOCUMENT_FILE_TYPE',
   subtype: 'table', pattern: /\.gtable$/i},
  {type: 'hosted', icon: 'glink', name: 'GLINK_DOCUMENT_FILE_TYPE',
   subtype: 'glink', pattern: /\.glink$/i},
  {type: 'hosted', icon: 'gform', name: 'GFORM_DOCUMENT_FILE_TYPE',
   subtype: 'form', pattern: /\.gform$/i},

  // Others
  {type: 'document', icon: 'pdf', name: 'PDF_DOCUMENT_FILE_TYPE',
   subtype: 'PDF', pattern: /\.pdf$/i},
  {type: 'document', name: 'HTML_DOCUMENT_FILE_TYPE',
   subtype: 'HTML', pattern: /\.(html?|mht|mhtml)$/i},
  {type: 'document', icon: 'word', name: 'WORD_DOCUMENT_FILE_TYPE',
   subtype: 'Word', pattern: /\.(doc|docx)$/i},
  {type: 'document', icon: 'ppt', name: 'POWERPOINT_PRESENTATION_FILE_TYPE',
   subtype: 'PPT', pattern: /\.(ppt|pptx)$/i},
  {type: 'document', icon: 'excel', name: 'EXCEL_FILE_TYPE',
   subtype: 'Excel', pattern: /\.(xls|xlsx)$/i}
];

/**
 * A special type for directory.
 */
FileType.DIRECTORY = {name: 'FOLDER', type: '.folder', icon: 'folder'};

/**
 * Returns the file path extension for a given file.
 *
 * @param {Entry} entry Reference to the file.
 * @return {string} The extension including a leading '.', or empty string if
 *     not found.
 */
FileType.getExtension = function(entry) {
  // No extension for a directory.
  if (entry.isDirectory)
    return '';

  var extensionStartIndex = entry.name.lastIndexOf('.');
  if (extensionStartIndex === -1 ||
      extensionStartIndex === entry.name.length - 1) {
    return '';
  }

  return entry.name.substr(extensionStartIndex);
};

/**
 * Gets the file type object for a given file name (base name). Use getType()
 * if possible, since this method can't recognize directories.
 *
 * @param {string} name Name of the file.
 * @return {Object} The matching file type object or an empty object.
 */
FileType.getTypeForName = function(name) {
  var types = FileType.types;
  for (var i = 0; i < types.length; i++) {
    if (types[i].pattern.test(name))
      return types[i];
  }

  // Unknown file type.
  var extension = util.splitExtension(name)[1];
  if (extension === '') {
    return { name: 'NO_EXTENSION_FILE_TYPE', type: 'UNKNOWN', icon: '' };
  }
  // subtype is the extension excluding the first dot.
  return { name: 'GENERIC_FILE_TYPE', type: 'UNKNOWN',
           subtype: extension.substr(1).toUpperCase(), icon: '' };
};

/**
 * Gets the file type object for a given file.
 * @param {Entry} entry Reference to the file.
 * @return {Object} The matching file type object or an empty object.
 */
FileType.getType = function(entry) {
  if (entry.isDirectory)
    return FileType.DIRECTORY;

  var types = FileType.types;
  for (var i = 0; i < types.length; i++) {
    if (types[i].pattern.test(entry.name))
      return types[i];
  }

  // Unknown file type.
  var extension = FileType.getExtension(entry);
  if (extension === '') {
    return { name: 'NO_EXTENSION_FILE_TYPE', type: 'UNKNOWN', icon: '' };
  }
  // subtype is the extension excluding the first dot.
  return { name: 'GENERIC_FILE_TYPE', type: 'UNKNOWN',
           subtype: extension.substr(1).toUpperCase(), icon: '' };
};

/**
 * @param {Object} fileType Type object returned by FileType.getType().
 * @return {string} Localized string representation of file type.
 */
FileType.typeToString = function(fileType) {
  if (fileType.subtype)
    return strf(fileType.name, fileType.subtype);
  else
    return str(fileType.name);
};

/**
 * Gets the media type for a given file.
 *
 * @param {Entry} entry Reference to the file.
 * @return {string} The value of 'type' property from one of the elements in
 *     FileType.types or undefined.
 */
FileType.getMediaType = function(entry) {
  return FileType.getType(entry).type;
};

/**
 * @param {Entry} entry Reference to the file.
 * @return {boolean} True if audio file.
 */
FileType.isAudio = function(entry) {
  return FileType.getMediaType(entry) === 'audio';
};

/**
 * @param {Entry} entry Reference to the file.
 * @return {boolean} True if image file.
 */
FileType.isImage = function(entry) {
  return FileType.getMediaType(entry) === 'image';
};

/**
 * @param {Entry} entry Reference to the file.
 * @return {boolean} True if video file.
 */
FileType.isVideo = function(entry) {
  return FileType.getMediaType(entry) === 'video';
};


/**
 * Files with more pixels won't have preview.
 * @param {Entry} entry Reference to the file.
 * @return {boolean} True if image or video.
 */
FileType.isImageOrVideo = function(entry) {
  var type = FileType.getMediaType(entry);
  return type === 'image' || type === 'video';
};

/**
 * @param {Entry} entry Reference to the file.
 * @return {boolean} Returns true if the file is hosted.
 */
FileType.isHosted = function(entry) {
  return FileType.getType(entry).type === 'hosted';
};

/**
 * @param {Entry} entry Reference to the file.
 * @return {string} Returns string that represents the file icon.
 *     It refers to a file 'images/filetype_' + icon + '.png'.
 */
FileType.getIcon = function(entry) {
  var fileType = FileType.getType(entry);
  return fileType.icon || fileType.type || 'unknown';
};

