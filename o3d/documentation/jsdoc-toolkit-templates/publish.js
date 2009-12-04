/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/**
 * @fileoverview The jsdoctoolkit loads this file and calls publish()
 */


var g_symbolSet;  // so we can look stuff up below.
var g_symbolArray;
var g_filePrefix;
var g_skipRE;
var g_validJSDOCTypes = {
  'boolean': true,
  'Event': true,
  'Element': true,
  'null': true,
  'number': true,
  'Number': true,
  'object': true,
  'Object': true,
  '*': true,
  '...': true,
  'RegExp': true,
  'string': true,
  'String': true,
  'XMLHttpRequest': true,
  'void': true,
  'undefined': true};
var g_unknownTypes = { };
var g_numErrors = 0;
var g_o3djsMode = false;
var g_outputMode;
var g_baseURL;
var g_topURL;
var g_templates = [];
var g_o3dPropertyRE = /^(\w+)\s+(\w+)\s+/;
var g_startContainerRE = /^(?:function\(|!function\(|[\(<{[])/;
var g_containerRE = /function\(|!function\(|[\(<{[]/;
var g_overloadRE = /xxxOVERLOADED\d+xxx/;
var g_overloadStr = 'xxxOVERLOADED';
var g_firstOverloadStr = 'xxxOVERLOADED0xxx';
var g_openCloseMap = {
  'function(': ')',
  '!function(': ')',
  '(': ')',
  '<': '>',
  '[': ']',
  '{': '}'};
var g_closeMap = {
  ')': true,
  '>': true,
  ']': true,
  '}': true};
var g_symbolsWithoutOverload = {};

/**
 * Called automatically by JsDoc Toolkit.
 * @param {SymbolSet} symbolSet Set of all symbols in all files.
 */
function publish(symbolSet) {
  try {
    publishInternal(symbolSet);
  } catch (e) {
    generateError(e);
  }

  if (g_numErrors > 0) {
    print('Num Errors: ' + g_numErrors);
    System.exit(1);
  }
}

/**
 * Called by us to catch errors.
 * @param {SymbolSet} symbolSet Set of all symbols in all files.
 */
function publishInternal(symbolSet) {
  publish.conf = {  // trailing slash expected for dirs
    ext: '.ezt',
    outDir: JSDOC.opt.d,
    templatesDir: JSDOC.opt.t,
    symbolsDir: '',
    exportsFile: JSDOC.opt.D.exportsFile,
    prefix: JSDOC.opt.D.prefix,
    mode: JSDOC.opt.D.mode};
  publish.conf.srcDir = publish.conf.outDir + 'src/';
  publish.conf.htmlDir = JSDOC.opt.D.htmlOutDir;
  g_baseURL = JSDOC.opt.D.baseURL;
  g_topURL = JSDOC.opt.D.topURL;
  g_outputMode = JSDOC.opt.D.mode;

  for (var key in publish.conf) {
    print ("publish.conf." + key + ": " + publish.conf[key])
  }

  if (publish.conf.mode == 'o3djs') {
    g_o3djsMode = true;
  }

  // In o3djs mode, don't generate docs for these.
  g_skipRE = new RegExp('^(o3d$|o3d\\.|Vectormath)');

  // Symbols we should always skip.
  var alwaysSkipRE = new RegExp('^(_global_|VectorMath|Aos)');

  // is source output is suppressed, just display the links to the source file
  if (JSDOC.opt.s && defined(Link) && Link.prototype._makeSrcLink) {
    Link.prototype._makeSrcLink = function(srcFilePath) {
      return '&lt;' + srcFilePath + '&gt;';
    }
  }

  // create the folders and subfolders to hold the output
  IO.mkPath((publish.conf.outDir).split('/'));
  IO.mkPath((publish.conf.htmlDir).split('/'));

  // used to allow Link to check the details of things being linked to
  Link.symbolSet = symbolSet;
  Link.base = '../';

  // used to allow other parts of this module to access database of symbols
  // and the file prefix.
  g_symbolSet = symbolSet;
  g_filePrefix = publish.conf.prefix;

  // create the required templates
  try {
    var templatesDir = publish.conf.templatesDir;
    var classTemplate = new JSDOC.JsPlate(templatesDir + 'class.tmpl');
    var exportsTemplate = new JSDOC.JsPlate(templatesDir + 'exports.tmpl');
    var membersTemplate = new JSDOC.JsPlate(templatesDir + 'members.tmpl');
    var classTreeTemplate = new JSDOC.JsPlate(templatesDir + 'classtree.tmpl');
    var fileListTemplate = new JSDOC.JsPlate(templatesDir + 'filelist.tmpl');
    var annotatedTemplate = new JSDOC.JsPlate(templatesDir + 'annotated.tmpl');
    var namespacesTemplate = new JSDOC.JsPlate(templatesDir +
                                               'namespaces.tmpl');
    var dotTemplate = new JSDOC.JsPlate(templatesDir + 'dot.tmpl');
  } catch(e) {
    generateError('Couldn\'t create the required templates: ' + e);
    System.exit(1);
  }

  // some utility filters
  function hasNoParent($) {return ($.memberOf == '')}
  function isaFile($) {return ($.is('FILE'))}
  function isaClass($) {return ($.is('CONSTRUCTOR') || $.isNamespace)}

  // get an array version of the symbolset, useful for filtering
  var symbols = symbolSet.toArray();
  g_symbolArray = symbols;

  // create the hilited source code files
  if (false) {
    var files = JSDOC.opt.srcFiles;
    for (var i = 0, l = files.length; i < l; i++) {
      var file = files[i];
      makeSrcFile(file, publish.conf.srcDir);
    }
  }

  // Comment this lines in to see all symbol information.
  //for (var ii = 0; ii < symbols.length; ++ii) {
  //  var symbol = symbols[ii];
  //  print('------[' + symbol.name + ']-------------------------------------');
  //  dumpObject(symbol, 5);
  //}

  // get a list of all the classes in the symbolset
  var classes = symbols.filter(isaClass).sort(makeSortby('alias'));
  var filteredClasses = [];
  var exports = '';

  // create each of the class pages
  for (var i = 0, l = classes.length; i < l; i++) {
    var symbol = classes[i];
    g_unknownTypes = { };

    symbol.events = symbol.getEvents();   // 1 order matters
    symbol.methods = symbol.getMethods(); // 2

    if ((g_o3djsMode && g_skipRE.test(symbol.alias)) ||
        alwaysSkipRE.test(symbol.alias)) {
      print('Skipping docs for  : ' + symbol.alias);
      continue;
    }

    print('Generating docs for: ' + symbol.alias);

    filteredClasses.push(symbol);

    // Comment these lines in to see what data is available to the templates.
    //if (symbol.name == 'Canvas') {
    //  print('------[' + symbol.name + ']-----------------------------------');
    //  dumpObject(symbol, 5);
    //}

    // <a href='symbol.source'>symbol.filename</a>
    symbol.source = symbol.srcFile;    // This is used as a link to the source
    symbol.filename = symbol.srcFile;  // This is display as the link.

    var output = '';
    output = classTemplate.process(symbol);

    IO.saveFile(publish.conf.outDir,
                (publish.conf.prefix + symbol.alias +
                 '_ref' + publish.conf.ext).toLowerCase(),
                output);
    IO.saveFile(publish.conf.htmlDir,
                (publish.conf.prefix + symbol.alias +
                 '_ref.html').toLowerCase(),
                output);

    var output = '';
    output = membersTemplate.process(symbol);
    IO.saveFile(publish.conf.outDir,
                (publish.conf.prefix + symbol.alias +
                 '_members' + publish.conf.ext).toLowerCase(),
                output);
    IO.saveFile(publish.conf.htmlDir,
                (publish.conf.prefix + symbol.alias +
                 '_members.html').toLowerCase(),
                output);

    if (publish.conf.exportsFile) {
      exports += exportsTemplate.process(symbol);
    }
  }

  var classTree = classTreeTemplate.process(filteredClasses);
  IO.saveFile(publish.conf.outDir, 'classtree.html', classTree);
  IO.saveFile(publish.conf.htmlDir, 'classtree.html', classTree);

  var fileList = fileListTemplate.process(symbols);
  IO.saveFile(publish.conf.outDir, 'filelist.html', fileList);
  IO.saveFile(publish.conf.htmlDir, 'filelist.html', fileList);

  var annotated = annotatedTemplate.process(filteredClasses);
  IO.saveFile(publish.conf.outDir, 'annotated' + publish.conf.ext, annotated);
  IO.saveFile(publish.conf.htmlDir, 'annotated.html', annotated);

  var namespaces = namespacesTemplate.process(filteredClasses);
  IO.saveFile(publish.conf.outDir, 'namespaces' + publish.conf.ext, namespaces);
  IO.saveFile(publish.conf.htmlDir, 'namespaces.html', namespaces);

  var dot = dotTemplate.process(filteredClasses);
  IO.saveFile(publish.conf.htmlDir, 'class_hierarchy.dot', dot);

  if (publish.conf.exportsFile) {
    print("Writing exports: " + publish.conf.exportsFile);
    var blankLineRE = /\n *\n/gm;
    while (blankLineRE.test(exports)) {
      exports = exports.replace(blankLineRE, '\n');
    }
    var parts = publish.conf.exportsFile.replace('\\', '/').
                match(/(.*?)\/([^\/]+)$/);
    IO.saveFile(parts[1], parts[2], exports);
  }
}

/**
 *  Gets just the first sentence (up to a full stop).
 *  Should not break on dotted variable names.
 *  @param {string} desc Description to extract summary from.
 *  @return {string} summary.
 */
function summarize(desc) {
  if (typeof desc != 'undefined')
    return desc.match(/([\w\W]+?\.)[^a-z0-9_$]/i) ? RegExp.$1 : desc;
}

/**
 * Makes a symbol sorter by some attribute.
 * @param {string} attribute to sort by.
 * @return {number} sorter result.
 */
function makeSortby(attribute) {
  return function(a, b) {
    if (a[attribute] != undefined && b[attribute] != undefined) {
      a = a[attribute].toLowerCase();
      b = b[attribute].toLowerCase();
      if (a < b) return -1;
      if (a > b) return 1;
      return 0;
    }
  }
}

/**
 * Pull in the contents of an external file at the given path.
 * @param {string} path Path of file relative to template directory.
 * @return {string} contents of file.
 */
function include(path) {
  var template = g_templates[path];
  if (!template) {
    try {
      template = new JSDOC.JsPlate(JSDOC.opt.t + path);
    } catch (e) {
      generateError('Could not include: ' + path + '\n' + e);
      template = '';
    }
    g_templates[path] = template;
  }
  var output = template.process({});
  return output;
}

/**
 * Turns a raw source file into a code-hilited page in the docs.
 * @param {string} path Path to source.
 * @param {string} srcDir path to place to store hilited page.
 * @param {string} opt_name to name output file.
 *
 */
function makeSrcFile(path, srcDir, opt_name) {
  if (JSDOC.opt.s) return;

  if (!opt_name) {
    opt_name = path.replace(/\.\.?[\\\/]/g, '').replace(/[\\\/]/g, '_');
    opt_name = opt_name.replace(/\:/g, '_');
  }

  var src = {path: path, name: opt_name, charset: IO.encoding, hilited: ''};

  if (defined(JSDOC.PluginManager)) {
    JSDOC.PluginManager.run('onPublishSrc', src);
  }

  if (src.hilited) {
    IO.saveFile(srcDir, opt_name + publish.conf.ext, src.hilited);
  }
}

/**
 * Builds output for displaying function parameters.
 * @param {Array} params Array of function params.
 * @return {string} string in format '(param1, param2)'.
 */
function makeSignature(params) {
  if (!params) return '()';
  var signature = '(' +
  params.filter(
    function($) {
      return $.name.indexOf('.') == -1; // don't show config params in signature
    }
  ).map(
    function($) {
      return $.name;
    }
  ).join(', ') + ')';
  return signature;
}

/**
 * Find symbol {@link ...} strings in text and turn into html links.
 * @param {string} str String to modify.
 * @return {string} modifed string.
 */
function resolveLinks(str) {
  str = str.replace(/\{@link ([^} ]+) ?\}/gi,
    function(match, symbolName) {
      return new Link().toSymbol(symbolName);
    }
  );

  return str;
}

/**
 * Makes a link for a symbol.
 *
 * @param {string} symbolName Name of symbol
 * @param {string} extra extra
 * @param {string} opt_bookmark Optional bookmark.
 */
function makeSymbolLink(symbolName, extra, opt_bookmark) {
  var prefix = g_filePrefix;
  if (g_o3djsMode && g_skipRE.test(symbolName)) {
    prefix = '../classo3d_1_1_';
  }
  return (prefix + symbolName + extra +
          '.html').toLowerCase() +
         (opt_bookmark ? '#' + opt_bookmark : '');
}

/**
 * Make link from symbol.
 * @param {Object} symbol Symbol from class database.
 * @param {string} opt_extra extra suffix to add before '.html'.
 * @return {string} url to symbol.
 */
function getLinkToSymbol(symbol, opt_extra) {
  opt_extra = opt_extra || '_ref';
  if (symbol.is('CONSTRUCTOR') || symbol.isNamespace) {
    return makeSymbolLink(symbol.alias, opt_extra);
  } else {
    var parentSymbol = getSymbol(symbol.memberOf);
    return makeSymbolLink(parentSymbol.alias, opt_extra, symbol.name);
  }
}

/**
 * Given a class alias, returns a link to its reference page
 * @param {string} classAlias Fully qualified name of class.
 * @return {string} url to class.
 */
function getLinkToClassByAlias(classAlias) {
  var symbol = getSymbol(classAlias);
  if (!symbol) {
    throw Error('no documentation for "' + classAlias + '"');
  }
  return getLinkToSymbol(symbol);
}

/**
 * Given a class alias, returns a link to its member reference page
 * @param {string} classAlias Fully qualified name of class.
 * @return {string} url to class in members file.
 */
function getLinkToClassMembersByAlias(classAlias) {
  var symbol = getSymbol(classAlias);
  return getLinkToSymbol(symbol, '_members');
}

/**
 * Given a class alias like o3djs.namespace.function returns an HTML string
 * with a link to each part (o3djs, namespace, function)
 * @param {string} classAlias Fully qualified alias of class.
 * @param {string} opt_cssClassId css class Id to put in class="" instead links.
 * @return {string} html with links to each class and parent.
 */
function getHierarchicalLinksToClassByAlias(classAlias, opt_cssClassId) {
  var parts = classAlias.split('.');
  var name = '';
  var html = '';
  var delim = '';
  var classId = '';
  if (opt_cssClassId) {
    classId = ' class="' + opt_cssClassId + '"';
  }
  for (var pp = 0; pp < parts.length; ++pp) {
    var part = parts[pp];
    name = name + delim + part;
    link = getLinkToClassByAlias(name);
    html = html + delim + '<a' + classId +
           ' href="' + link + '">' + part + '</a>';
    delim = '.';
  }
  return html;
}

/**
 * Dumps a javascript object.
 *
 * @param {Object} obj Object to dump.
 * @param {number} depth Depth to dump (0 = forever).
 * @param {string} opt_prefix Prefix to put before each line.
 */
function dumpObject(obj, depth, opt_prefix) {
  opt_prefix = opt_prefix || '';
  --depth;
  for (var prop in obj) {
    if (typeof obj[prop] != 'function') {
      dumpWithPrefix(prop + ' : ' + obj[prop], opt_prefix);
      if (depth != 0) {
        dumpObject(obj[prop], depth, opt_prefix + '    ');
      }
    }
  }
}

/**
 * Dumps a string, putting a prefix before each line
 * @param {string} str String to dump.
 * @param {string} prefix Prefix to put before each line.
 */
function dumpWithPrefix(str, prefix) {
  var parts = str.split('\n');
  for (var pp = 0; pp < parts.length; ++pp) {
    print(prefix + parts[pp]);
  }
}

/**
 * gets the type of a property.
 * @param {!object} property Property object.
 * @return {string} type of property.
 */
function getPropertyType(property) {
  if (property.type.length > 0) {
    return property.type;
  } else {
    var tag = property.comment.getTag('type');
    if (tag.length > 0) {
      return tag[0].type;
    } else {
      return 'undefined';
    }
  }
}

/**
 * Gets the parameters for a class.
 * Parameters are an o3d specific thing. We have to look for tags that
 * start with @o3dparameter
 * @param {!Symbol} symbol
 */
function getParameters(symbol) {
  var params = [];
  if (symbol.inheritsFrom.length) {
    params = getParameters(getSymbol(symbol.inheritsFrom[0]));
  }

  var tags = symbol.comment.getTag('o3dparameter');
  for (var ii = 0; ii < tags.length; ++ii) {
    var tag = tags[ii];
    var tagString = tag.toString();
    var parts = tagString.match(g_o3dPropertyRE);
    if (!parts) {
      generateError('Malformed o3dparameter specification for ' + symbol.alias +
                    ' : "' + tag + '"');
    } else {
      var descString = tagString.substr(parts[0].length);
      var param = {
        name: parts[1],
        type: 'o3d.' + parts[2],
        desc: descString,
        parent: symbol.alias,
      };
      params.push(param);
    }
  }
  return params;
}

/**
 * Returns whether or not the symbol is deprecated.  Apparently jsdoctoolkit is
 * supposed to extract this info for us but it's not so we have to do it
 * manually.
 * @param {!Symbol} symbol The symbol to check.
 * @return {boolean} True if the symbol is deprecated.
 */
function isDeprecated(symbol) {
  var tags = symbol.comment.getTag('deprecated');
  return tags.length > 0;
}

/**
 * Converts [ to [[] for ezt files.
 * Also converts '\n\n' to <br/></br>
 * @param {string} str to sanitize.
 * @return {string} Sanitized string.
 */
function sanitizeForEZT(str) {
  str = str.replace(/<pre>/g, '<pre class="prettyprint">');
  return str.replace(/\[/g, '[[]').replace(/\n\n/g, '<br/><br/>');
}

/**
 * Check if string starts with another string.
 * @param {string} str String to check.
 * @param {string} prefix Prefix to check for.
 * @return {boolean} True if str starts with prefix.
 */
function startsWith(str, prefix) {
  return str.substring(0, prefix.length) === prefix;
}

/**
 * Check if string ends with another string.
 * @param {string} str String to check.
 * @param {string} suffix Suffix to check for.
 * @return {boolean} True if str ends with suffix.
 */
function endsWith(str, suffix) {
  return str.substring(str.length - suffix.length) === suffix;
}

/**
 * Returns a string stripped of leading and trailing whitespace.
 * @param {string} str String to strip.
 * @return {string} stripped string.
 */
function stripWhitespace(str) {
  return str.replace(/^\s+/, '').replace(/\s+$/, '');
}

/**
 * Converts a camelCase name to underscore as in TypeOfFruit becomes
 * type_of_fruit.
 * @param {string} str CamelCase string.
 * @return {string} underscorified str.
 */
function camelCaseToUnderscore(str) {
  function toUnderscore(match) {
    return '_' + match.toLowerCase();
  }
  return str[0].toLowerCase() +
      str.substring(1).replace(/[A-Z]/g, toUnderscore);
}

/**
 * Prints a warning about an unknown type only once.
 * @param {string} place Use to print error message if type not found.
 * @param {string} type Type specification.
 */
function reportUnknownType(place, type) {
  if (!g_unknownTypes[type]) {
    g_unknownTypes[type] = true;
    generatePlaceError (place, 'reference to unknown type: "' + type + '"');
  }
}

/**
 * Gets index of closing character.
 * @param {string} str string to search.
 * @param {number} startIndex index to start searching at. Must be an opening
 *      character.
 * @return {number} Index of closing character or (-1) if not found.
 */
function getIndexOfClosingCharacter(str, startIndex) {
  var stack = [];
  if (!g_openCloseMap[str[startIndex]]) {
    throw 'startIndex does not point to opening character.';
  }
  var endIndex = str.length;
  while (startIndex < endIndex) {
    var c = str[startIndex];
    var closing = g_openCloseMap[c];
    if (closing) {
      stack.unshift(closing);
    } else {
      closing = g_closeMap[c];
      if (closing) {
        var expect = stack.shift()
        if (c != expect) {
          return -1;
        }
        if (stack.length == 0) {
          return startIndex;
        }
      }
    }
    ++startIndex;
  }
  return -1;
}

/**
 * Make's a name by concatenating strings.
 * @param {...[string]} strings to concatenate.
 * @return {string} Concatenated string.
 */
function makeName() {
  var str = '';
  for (var ii = 0; ii < arguments.length; ++ii) {
    if (str) {
      str += '.';
    }
    str += arguments[ii];
  }
  return str;
}

/**
 * Generates an error msg.
 * @param {string} msg.
 */
function generateError(msg) {
  ++g_numErrors;
  print('ERROR: ' + msg);
}

/**
 * Generates an error msg.
 * @param {string} place Use to print error message.
 * @param {string} msg.
 */
function generatePlaceError(place, msg) {
  generateError(place + ': ' + msg);
}

/**
 * Converts a reference to a single JSDOC type specification to an html link.
 * @param {string} place Use to print error message if type not found.
 * @param {string} str to linkify.
 * @return {string} linkified string.
 */
function linkifySingleType(place, type) {
  if (!type) {
    generatePlaceError(place, 'type is empty');
    return '';
  }
  type = stripWhitespace(type);
  var not = '';
  var equals = '';
  // Remove ! if it exists.
  if (type[0] == '!') {
    not = '!'
    type = type.substring(1);
  }
  if (endsWith(type, '=')) {
    equals = '=';
    type = type.substring(0, type.length - 1);
  }

  var link = type;

  // Check for array wrapper.
  if (startsWith(type, 'Array.<')) {
    var closingAngle = getIndexOfClosingCharacter(type, 6);
    if (closingAngle < 0) {
      generatePlaceError(place, 'Unmatched "<" in Array type : ' + type);
    } else {
      link = 'Array.&lt;' +
        linkifySingleType(place, type.substring(7, closingAngle)) + '>';
    }
  } else if (startsWith(type, 'Object.<')) {
    var closingAngle = getIndexOfClosingCharacter(type, 7);
    if (closingAngle < 0) {
      generatePlaceError(place, 'Unmatched "<" in Object type : ' + type);
    } else {
      var objectSpec = type.substring(8, closingAngle);
      var elements = objectSpec.split(/\s*,\s*/);
      if (elements.length != 2) {
        generatePlaceError(place, 'An Object spec must have exactly 2 types');
      }
      link = 'Object.&lt;' +
          linkifySingleType(place, elements[0]) + ', ' +
          linkifySingleType(place, elements[1]) + '>';
    }
  } else if (startsWith(type, 'function(')) {
    var closingParen = getIndexOfClosingCharacter(type, 8);
    if (closingParen < 0) {
      generatePlaceError(place, 'Unmatched "(" in function type : ' + type);
    } else {
      var end = type.substring(closingParen + 1);
      if (!startsWith(end, ': ')) {
        generatePlaceError(place,
            'Malformed return specification on function. Must be' +
            ' "function(args): type" including the space after the colon.');
      } else {
        var output = '';
        var argsStr = type.substring(9, closingParen);
        if (argsStr) {
          // TODO(gman): This needs to split taking parens, brackets and angle
          //    brackets into account.
          var args = argsStr.split(/ *, */);
          for (var ii = 0; ii < args.length; ++ii) {
            if (ii > 0) {
              output += ', ';
            }
            output += linkifyTypeSpec(place, args[ii]);
          }
        }
        link = 'function(' + output + '): ' +
               linkifyTypeSpec(place, end.substring(2));
      }
    }
  } else if (startsWith(type, '{') && endsWith(type, '}')) {
    // found a record.
    var elements = type.substr(1, type.length - 2).split(/\s*,\s*/);
    var output = '{';
    for (var ii = 0; ii < elements.length; ++ii) {
      if (ii > 0) {
        output += ', ';
      }
      var element = elements[ii];
      var colon = element.indexOf(': ');
      if (colon < 0) {
        generatePlaceError(place,
                           'Malformed record specification. Format must be ' +
                           '{id1: type1, id2: type2, ...}.');
        output += element;
      } else {
        var name = element.substring(0, colon);
        var subType = element.substring(colon + 2);
        output += name + ':&nbsp;' + linkifyTypeSpec(place, subType);
      }
    }
    link = output + '}';
  } else {
    var symbol = getSymbol(type);
    if (symbol) {
      link = '<a class="el" href="' + getLinkToSymbol(symbol) + '">' +
          type + '</a>';
    } else {
      // See if the symbol is a property or field.
      var found = false;
      var period = type.lastIndexOf('.');
      if (period >= 0 && type != '...') {
        var subType = type.substring(0, period);
        var member = type.substring(period + 1);
        symbol = getSymbol(subType);
        if (symbol) {
          if (symbol.hasMember(member) ||
              symbol.hasMember(member + g_firstOverloadStr)) {
            var field = type.substring(period + 1);
            link = '<a class="el" href="' + getLinkToSymbol(symbol) + '#' +
                field + '">' +  type + '</a>';
            found = true;
          }
        }
      }

      if (!found) {
        if (type[0] == '?') {
          type = type.substring(1);
        }
        if (!g_validJSDOCTypes[type]) {
          reportUnknownType(place, type);
        }
      }
    }
  }

  return not + link + equals;
}

/**
 * Splits a string by containers. A string like "ab(cd,ef)gh" will
 * be returned as ['ab', '(cd,ef)', 'gh']
 * @param {string} str String to split.
 * @return {!Array.<string>} The split string parts.
 */
function splitByContainers(str) {
  var parts = [];
  for (;;) {
    var match = str.match(g_containerRE);
    if (!match) {
      if (str.length > 0) {
        parts.push(str);
      }
      return parts;
    }
    var startIndex = str.indexOf(match);
    if (startIndex != 0) {
      parts.push(str.substring(0, startIndex));
    }
    var endIndex = getIndexOfClosingCharacter(
        str, startIndex + match.toString().length - 1);
    if (endIndex < 0) {
      throw 'no closing character for "' + str[startIndex] + '" in "' + str +
            '"';
    }
    var piece = str.substring(startIndex, endIndex + 1);
    parts.push(piece);
    str = str.substring(endIndex + 1);
  }
}

/**
 * Fix function specs.
 * The jsdoctoolkit wrongly converts ',' to | as in 'function(a, b)' to
 * 'function(a|b)' and '{id1: type1, id2: type2}' to '{id1: type1|id2: type2}'
 * so we need to put those back here (or fix jsdoctoolkit, the proper solution).
 * Worse, we have to distinguish between 'function(a|b)' and '(a|b)'. The former
 * needs to become 'function(a, b)' while the later needs to stay the same.
 *
 * @param {string} str JSDOC type specification string .
 * @param {boolean} opt_paren True if we are inside a paren.
 * @return {string} str with '|' converted back to ', ' unless the specification
 *     starts with '(' and ends with ')'. That's not a very good check beacuse
 *     you could 'function((string|number)) and this would fail to do the right
 *     thing.
 */
function fixSpecCommas(str, opt_paren) {
  var start = str.match(g_startContainerRE);
  if (start) {
    var startStr = start.toString();
    var closing = g_openCloseMap[startStr];
    if (closing) {
      var lastChar = str[str.length - 1];
      if (lastChar == closing) {
        var middle = str.substring(startStr.length, str.length - 1);
        return startStr +
          fixSpecCommas(middle, startStr === '(') +
          lastChar;
      }
    }
  }

  if (str.match(g_containerRE)) {
    var parts = splitByContainers(str);
    for (var ii = 0; ii < parts.length; ++ii) {
      parts[ii] = fixSpecCommas(parts[ii], opt_paren);
    }
    return parts.join('');
  } else {
    if (opt_paren) {
      return str;
    }
    return str.replace(/\|/g, ', ');
  }
}

/**
 * Converts a JSDOC type specification into html links. For example
 * '(!o3djs.math.Vector3|!o3djs.math.Vector4)' would change to
 * '(!<a href="??">o3djs.math.Vector3</a>
 * |!<a href="??">o3djs.math.Vector4</a>)'.
 * @param {string} place Use to print error message if type not found.
 * @param {string} str to linkify.
 * @return {string} linkified string.
 */
function linkifyTypeSpec(place, str) {
  var output = '';
  if (str) {
    try {
      var fixed = fixSpecCommas(str);
    } catch (e) {
      generatePlaceError(place, e);
    }
    // TODO: needs to split outside of parens and angle brackets.
    if (startsWith(fixed, '(') && endsWith(fixed, ')')) {
      var types = fixed.substring(1, fixed.length - 1).split('|');
      output += '(';
      for (var tt = 0; tt < types.length; ++tt) {
        if (tt > 0) {
          output += '|';
        }
        output += linkifySingleType(place, types[tt]);
      }
      output += ')';
    } else {
      output += linkifySingleType(place, fixed);
    }
  } else {
    generatePlaceError(place, 'missing type specification (' + str + ')');
  }
  return output;
}

/**
 * Same as linkifyTypeSpec but allows str to be undefined or ''.
 * @param {string} place Use to print error message if type not found.
 * @param {string} str to linkify.
 * @return {string} linkified string.
 */
function linkifyTypeSpecForReturn(place, str) {
  if (str) {
    return linkifyTypeSpec(place, str);
  }
  return '';
}

/**
 * Check if a symbol is an enum.
 * @param {!Symbol} symbol Symbol to check.
 * @return {boolean} true if symbol is an enum
 */
function isEnum(symbol) {
  if (symbol.isStatic && !symbol.isNamespace) {
    if (symbol.comment && symbol.comment.tagTexts) {
      for (var ii = 0; ii < symbol.comment.tagTexts.length; ++ii) {
        var tag = symbol.comment.tagTexts[ii];
        if (startsWith(tag.toString(), 'enum')) {
          return true;
        }
      }
    }
  }
  return false;
}

/**
 * Gets the public properties of a symbol.
 * @param {!Symbol} symbol The symbol to get properties from.
 * @return {!Array.<!Symbol>} The public properties of the symbol.
 */
function getPublicProperties(symbol) {
  var publicProperties = [];
  for (var ii = 0; ii < symbol.properties.length; ++ii) {
    var property = symbol.properties[ii];
    if (!property.isPrivate && !property.isNamespace && !isEnum(property)) {
      publicProperties.push(property);
    }
  }
  return publicProperties;
}

/**
 * Gets the public types of a symbol.
 * @param {!Symbol} symbol The symbol to get properties from.
 * @return {!Array.<!Symbol>} The public types of the symbol.
 */
function getPublicTypes(symbol) {
  var publicTypes = [];
  for (var ii = 0; ii < symbol.properties.length; ++ii) {
    var type = symbol.properties[ii];
    if (!type.isPrivate && !type.isNamespace && isEnum(type)) {
      publicTypes.push(type);
    }
  }
  for (var ii = 0; ii < g_symbolArray.length; ++ii) {
    var type = g_symbolArray[ii];
    if (type.memberOf == symbol.alias &&
        !type.isPrivate &&
        type.is('CONSTRUCTOR')) {
      publicTypes.push(type);
    }
  }
  return publicTypes;
}

/**
 * Gets a symbol for a type.
 * This is here mostly for debugging so you can insert a print before or after
 * each call to g_symbolSet.getSymbol.
 * @param {string} type fully qualified type.
 * @return {Symbol} The symbol object for the type or null if not found.
 */
function getSymbol(type) {
  return g_symbolSet.getSymbol(type);
}

/**
 * Gets the source path for a symbol starting from 'o3djs/'
 * @param {!Symbol} symbol The symbol to get the source name for.
 * @return {string} The name of the source file.
 */
function getSourcePath(symbol) {
  var path = symbol.srcFile.replace(/\\/g, '/');
  var index = path.indexOf('/o3djs/');
  return path.substring(index + 1);
}

/**
 * Gets the name of the parent of a symbol.
 * @param {!Symbol} symbol The symbol to get the parent of
 * @return {string} The parent's name.
 */
function getParentName(symbol) {
  var parent = getSymbol(symbol.memberOf);
  return parent.isNamespace ? symbol.memberOf : parent.alias;
}

/**
 * Gets a qualified name. Used for members. For namespaces will return the fully
 * qualified name. For objects will return just ObjectName.method
 * @param {!Symbol} method The method or property to get a qualified name for.
 * @return {string} The qualified name for the method or property.
 */
function getQualifiedName(method) {
  return getParentName(method) + '.' + method.name;
}

/**
 * Removes the "xxxOVERLOADEDxxx" part of a name
 * @param {string} name Name that may have a overloaded suffux.
 * @return {string} The name without the overloaded suffix.
 */
function getNonOverloadedName(name) {
  var index = name.indexOf(g_overloadStr);
  if (index >= 0) {
    return name.substring(0, index);
  }
  return name;
}

/**
 * Gets a Documentation name. For members of a namespace returns the fully
 * qualified name. For members of a class returns ClassName.name
 * @param {!Symbol} parent Symbol that we are making docs for.
 * @param {!Symbol} child Method or property of parent to get doc name for.
 * @return {string} Doc name for child.
 */
function getDocName(parent, child) {
  if (parent.isNamespace) {
    return child.memberOf + "." + getNonOverloadedName(child.name);
  }
  return parent.name + "." + getNonOverloadedName(child.name);
}

/**
 * Gets a symbol name for export. If the method or property is static returns
 * "Namespace.Class.method". If not returns "Namespace.Class.prototype.method".
 * @param {!Symbol} symbol Symbol to get name for.
 * @return {string} Name of symbol.
 */
function getSymbolNameForExport(symbol) {
  if (!symbol.memberOf) {
    return symbol.name;
  }
  return symbol.memberOf +
         ((symbol.isStatic || symbol.isNamespace) ? '.' : '.prototype.') +
         symbol.name;
}

/**
 * Gets the base URL for links.
 */
function getBaseURL() {
  return g_baseURL;
}

/**
 * Gets the top URL for links.
 */
function getTopURL() {
  return g_topURL;
}

/**
 * Returns the output mode.
 * @return {string} The output mode.
 */
function getOutputMode() {
  return g_outputMode;
}

/**
 * Returns true if we should write constructor docs.
 * @param {!Symbol} symbol The symbol that might have a constructor.
 * @return {boolean} true if we should write a constructor.
 */
function shouldWriteConstructor(symbol) {
  return g_outputMode != 'o3d' &&
         !symbol.isPrivate &&
         !symbol.isBuiltin() &&
         !symbol.isNamespace &&
         symbol.is('CONSTRUCTOR');
}

/**
 * Splits a camelCase word into an array of word parts.
 * @param {string} word camelCase word.
 * @return {!Array.<string>} The word split into word parts.
 */
function splitCamelCase(word) {
  var spacesAdded = word.replace(/([A-Z])/g, ' $1');
  return spacesAdded.split(' ');
}

/**
 * Breaks a word at max_length with hyphens. Assumes word is camelCase
 * @param {string} word Word to break.
 * @param {number} maxLength word will continue to be split until no part is
 *     this longer than this if possible.
 * @param {string} string to use to join split parts.
 * @return {string} The word split then joined with joinString.
 */
function hyphenateWord(word, maxLength, joinString) {
  var words = splitCamelCase(word);
  var hyphenated = '';
  var newWord = '';
  for (var ii = 0; ii < words.length; ++ii) {
    var part = words[ii];
    var temp = newWord + part;
    if (temp.length > maxLength) {
      hyphenated += newWord + joinString;
      newWord = '';
    }
    newWord += part;
  }
  hyphenated += newWord;
  return hyphenated;
}
