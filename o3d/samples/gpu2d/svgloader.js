/*
 * Copyright 2010, Google Inc.
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
 * @fileoverview Loads SVG files and creates o3djs.gpu2d.Paths from
 * them, inserting all of the associated geometric nodes under the
 * passed Transform.
 * <P>
 * This file depends on the O3D APIs, o3djs.base, o3djs.io, o3djs.math,
 * o3djs.gpu2d, and the XML for &lt;SCRIPT&gt; parser available from
 * http://xmljs.sourceforge.net/ .
 */

/**
 * Constructs a new SVGLoader.
 * @constructor
 */
function SVGLoader() {
}

/**
 * Helper function to defer the execution of another function.
 * @param {function(): void} func function to execute later.
 * @private
 */
function runLater_(func) {
  setTimeout(func, 0);
}

/**
 * Loads an SVG file at the given URL. The file is downloaded in the
 * background. Graphical objects are allocated in the given Pack, and
 * created materials are registered under the given DrawList,
 * typically the zOrderedDrawList from an
 * o3djs.rendergraph.ViewInfo. The created Shapes are parented under
 * the given Transform. If the optional completion callback is
 * specified, it is called once the file has been downloaded and
 * processed.
 * @param {string} url URL of the SVG file to load.
 * @param {boolean} flipY Whether the Y coordinates should be flipped
 *     to better match the 3D coordinate system.
 * @param {!o3d.Pack} pack Pack to manage created objects.
 * @param {!o3d.DrawList} drawList DrawList to use for created
 *     materials.
 * @param {!o3d.Transform} transform Transform under which to place
 *     created Shapes.
 * @param {function(string, *): void}
 *     opt_completionCallback Optional completion callback which is
 *     called after the file has been processed. The URL of the file
 *     is passed as the first argument. The second argument indicates
 *     whether the file was loaded successfully (true) or not (false).
 *     The third argument is an error detail if the file failed to
 *     load successfully.
 */
SVGLoader.prototype.load = function(url,
                                    flipY,
                                    pack,
                                    drawList,
                                    transform,
                                    opt_completionCallback) {
  var that = this;
  o3djs.io.loadTextFile(url, function(text, exception) {
    if (exception) {
      runLater_(function() {
        if (opt_completionCallback)
          opt_completionCallback(url,
                                 false,
                                 e);
      });
    } else {
      runLater_(function() {
        try {
          that.parse_(text,
                      flipY,
                      pack,
                      drawList,
                      transform);
          if (opt_completionCallback)
            opt_completionCallback(url, true, null);
        } catch (e) {
          if (window.console)
            window.console.log(e);
          if (opt_completionCallback)
            opt_completionCallback(url, false, e);
        }
      });
    }
  });
};

/**
 * Does the parsing of the SVG file.
 * @param {string} svgText the text of the SVG file.
 * @param {boolean} flipY Whether the Y coordinates should be flipped
 *     to better match the 3D coordinate system.
 * @param {!o3d.Pack} pack Pack to manage created objects.
 * @param {!o3d.DrawList} drawList DrawList to use for created
 *     materials.
 * @param {!o3d.Transform} transform Transform under which to place
 *     created Shapes.
 * @private
 */
SVGLoader.prototype.parse_ = function(svgText,
                                      flipY,
                                      pack,
                                      drawList,
                                      transform) {
  /**
   * The pack in which to create shapes, materials, etc.
   * @type {!o3d.Pack}
   * @private
   */
  this.pack_ = pack;

  /**
   * Stack of matrices. Entering a new graphics context pushes a new
   * matrix.
   * @type {!Array.<!o3djs.math.Matrix4>}
   * @private
   */
  this.matrixStack_ = [];

  /**
   * Stack of transforms. Entering a new graphics context pushes a new
   * transform.
   * @type {!Array.<!o3d.Transform>}
   * @private
   */
  this.transformStack_ = [];

  /**
   * The current polygon offset. Each successive path parsed
   * increments this.
   * @type {number}
   * @private
   */
  this.polygonOffset_ = 0;

  this.matrixStack_.push(o3djs.math.identity(4));
  this.transformStack_.push(transform);
  var parser = new SAXDriver();
  var eventHandler = new SVGSAXHandler_(this,
                                        parser,
                                        flipY,
                                        pack,
                                        drawList);
  parser.setDocumentHandler(eventHandler);
  parser.parse(svgText);
};

/**
 * Returns the current transform.
 * @return {!o3d.Transform}
 * @private
 */
SVGLoader.prototype.currentTransform_ = function() {
  var len = this.transformStack_.length;
  return this.transformStack_[len - 1];
};

/**
 * Returns the current matrix.
 * @return {!o3djs.math.Matrix4}
 * @private
 */
SVGLoader.prototype.currentMatrix_ = function() {
  var len = this.matrixStack_.length;
  return this.matrixStack_[len - 1];
};

/**
 * Sets the current matrix.
 * @param {!o3djs.math.Matrix4} matrix the new current matrix.
 * @private
 */
SVGLoader.prototype.setCurrentMatrix_ = function(matrix) {
  var length = this.matrixStack_.length;
  this.matrixStack_[length - 1] = matrix;
  this.transformStack_[length - 1].localMatrix = matrix;
};

/**
 * Pushes a new transform / matrix pair.
 * @private
 */
SVGLoader.prototype.pushTransform_ = function() {
  this.matrixStack_.push(o3djs.math.identity(4));
  var xform = this.pack_.createObject('o3d.Transform');
  xform.parent = this.currentTransform_();
  this.transformStack_.push(xform);
};

/**
 * Pops a transform / matrix pair.
 * @private
 */
SVGLoader.prototype.popTransform_ = function() {
  this.matrixStack_.pop();
  this.transformStack_.pop();
};

/**
 * Supports the "matrix" command in the graphics context; not yet
 * implemented.
 * @param {number} a matrix element
 * @param {number} b matrix element
 * @param {number} c matrix element
 * @param {number} d matrix element
 * @param {number} e matrix element
 * @param {number} f matrix element
 * @private
 */
SVGLoader.prototype.matrix_ = function(a, b, c, d, e, f) {
  // TODO(kbr): implement
  throw 'matrix command not yet implemented';
};

/**
 * Supports the "translate" command in the graphics context.
 * @param {number} x x translation
 * @param {number} y y translation
 * @private
 */
SVGLoader.prototype.translate_ = function(x, y) {
  var tmp = o3djs.math.matrix4.translation([x, y, 0]);
  this.setCurrentMatrix_(
      o3djs.math.mulMatrixMatrix(tmp, this.currentMatrix_()));
};

/**
 * Supports the "scale" command in the graphics context; not yet
 * implemented.
 * @param {number} sx x scale
 * @param {number} sy y scale
 * @private
 */
SVGLoader.prototype.scale_ = function(sx, sy) {
  var tmp = o3djs.math.matrix4.scaling([sx, sy, 1]);
  this.setCurrentMatrix_(
      o3djs.math.mulMatrixMatrix(tmp, this.currentMatrix_()));
};

/**
 * Supports the "rotate" command in the graphics context.
 * @param {number} angle angle to rotate, in degrees.
 * @param {number} cx x component of rotation center.
 * @param {number} cy y component of rotation center.
 * @private
 */
SVGLoader.prototype.rotate_ = function(angle, cx, cy) {
  var rot = o3djs.math.matrix4.rotationZ(o3djs.math.degToRad(angle));
  if (cx || cy) {
    var xlate1 = o3djs.math.matrix4.translation([cx, cy, 0]);
    var xlate2 = o3djs.math.matrix4.translation([-cx, -cy, 0]);
    rot = o3djs.math.mulMatrixMatrix(xlate2, rot);
    rot = o3djs.math.mulMatrixMatrix(rot, xlate1);
  }
  this.setCurrentMatrix_(rot);
};

/**
 * Supports the "skewX" command in the graphics context; not yet
 * implemented.
 * @param {number} angle skew X angle, in degrees.
 * @private
 */
SVGLoader.prototype.skewX_ = function(angle) {
  // TODO(kbr): implement
  throw 'skewX command not yet implemented';
};

/**
 * Supports the "skewY" command in the graphics context; not yet
 * implemented.
 * @param {number} angle skew Y angle, in degrees.
 * @private
 */
SVGLoader.prototype.skewY_ = function(angle) {
  // TODO(kbr): implement
  throw 'skewY command not yet implemented';
};

/**
 * Parses the data from an SVG path element, constructing an
 * o3djs.gpu2d.Path from it.
 * @param {string} pathData the path's data (the "d" attribute).
 * @param {number} lineNumber the line number of the current parse,
 *     for error reporting.
 * @param {boolean} flipY Whether the Y coordinates should be flipped
 *     to better match the 3D coordinate system.
 * @param {!o3d.Pack} pack Pack to manage created objects.
 * @param {!o3d.DrawList} drawList DrawList to use for created
 *     materials.
 * @private
 */
SVGLoader.prototype.parsePath_ = function(pathData,
                                          lineNumber,
                                          flipY,
                                          pack,
                                          drawList) {
  var parser = new PathDataParser_(this,
                                   lineNumber,
                                   flipY,
                                   pack,
                                   drawList);
  var path = parser.parse(pathData);
  if (this.fill_) {
    path.setFill(this.fill_);
  }
  path.setPolygonOffset(-3 * this.polygonOffset_, -4 * this.polygonOffset_);
  ++this.polygonOffset_;
  this.currentTransform_().addShape(path.shape);
};

/**
 * Parses the style from an SVG path element or graphics context,
 * preparing to set it on the next created o3djs.gpu2d.Path. If it
 * doesn't know how to handle it, the default color of solid black
 * will be used.
 * @param {string} styleData the string containing the "style"
 *     attribute.
 * @param {number} lineNumber the line number of the current parse,
 *     for error reporting.
 * @param {!o3d.Pack} pack Pack to manage created objects.
 * @private
 */
SVGLoader.prototype.parseStyle_ = function(styleData,
                                           lineNumber,
                                           pack) {
  this.fill_ = null;
  var portions = styleData.split(';');
  for (var i = 0; i < portions.length; i++) {
    var keyVal = portions[i].split(':');
    var key = keyVal[0];
    var val = keyVal[1];
    if (key == 'stroke') {
      // TODO(kbr): support strokes
    } else if (key == 'stroke-width') {
      // TODO(kbr): support stroke width
    } else if (key == 'fill') {
      if (val.charAt(0) == '#') {
        var r = parseInt(val.substr(1, 2), 16);
        var g = parseInt(val.substr(3, 2), 16);
        var b = parseInt(val.substr(5, 2), 16);
        var fill = o3djs.gpu2d.createColor(pack,
                                           r / 255.0,
                                           g / 255.0,
                                           b / 255.0,
                                           1.0);
        this.fill_ = fill;
      } else if (val.substr(0, 4) == 'rgb(' &&
                 val.charAt(val.length - 1) == ')') {
        var rgbStrings = val.substr(4, val.length - 5).split(',');
        var r = parseInt(rgbStrings[0]);
        var g = parseInt(rgbStrings[1]);
        var b = parseInt(rgbStrings[2]);
        var fill = o3djs.gpu2d.createColor(pack,
                                           r / 255.0,
                                           g / 255.0,
                                           b / 255.0,
                                           1.0);
        this.fill_ = fill;
      }
    }
  }
};

/**
 * Parses the data from an SVG transform attribute, storing the result
 * in the current o3d.Transform.
 * @param {string} data the transform's data.
 * @param {number} lineNumber the line number of the current parse,
 *     for error reporting.
 * @private
 */
SVGLoader.prototype.parseTransform_ = function(data,
                                               lineNumber) {
  var parser = new TransformDataParser_(this,
                                        lineNumber);
  parser.parse(data);
};

//----------------------------------------------------------------------
// BaseDataParser -- base class for parsers dealing with SVG data

/**
 * Base class for parsers dealing with SVG data.
 * @param {SVGLoader} loader The SVG loader.
 * @param {number} lineNumber the line number of the current parse,
 *     for error reporting.
 * @constructor
 * @private
 */
BaseDataParser_ = function(loader,
                           lineNumber) {
  this.loader_ = loader;
  this.lineNumber_ = lineNumber;
  this.putBackToken_ = null;
  this.singleLetterWords_ = false;
}

/**
 * Types of tokens.
 * @enum
 * @private
 */
BaseDataParser_.TokenTypes = {
  WORD: 1,
  NUMBER: 2,
  LPAREN: 3,
  RPAREN: 4,
  NONE: 5
};

/**
 * Parses the given SVG data.
 * @param {string} data the SVG data.
 * @private
 */
BaseDataParser_.prototype.parse = function(data) {
  var parseState = {
    // First index of current token
    firstIndex: 0,
    // Last index of current token (exclusive)
    lastIndex: 0,
    // Line number of the path element
    lineNumber: this.lineNumber_
  };
  var done = false;
  while (!done) {
    var tok = this.nextToken_(parseState, data);
    switch (tok.kind) {
      case BaseDataParser_.TokenTypes.WORD:
        // Allow the parsing to override the specification of the last
        // command
        this.setLastCommand_(tok.val);
        this.parseWord_(parseState, data, tok.val);
        break;
      case BaseDataParser_.TokenTypes.NUMBER:
        // Assume this is a repeat of the last command
        this.putBack_(tok);
        this.parseWord_(parseState, data, this.lastCommand_);
        break;
      default:
        done = true;
        break;
    }
  }
};

/**
 * Sets the last parsed command.
 * @param {string} command the last parsed command.
 * @private
 */
BaseDataParser_.prototype.setLastCommand_ = function(command) {
  this.lastCommand_ = command;
};

/**
 * Returns true if the given character is a whitespace or separator.
 * @param {string} c the character to test.
 * @private
 */
BaseDataParser_.prototype.isWhitespaceOrSeparator_ = function(c) {
  return (c == ',' ||
          c == ' ' ||
          c == '\t' ||
          c == '\r' ||
          c == '\n');
};

/**
 * Puts back a token to be consumed during the next iteration. There
 * is only a one-token put back buffer.
 * @param {!{kind: BaseDataParser_TokenTypes, val: string}} tok The
 *     token to put back.
 * @private
 */
BaseDataParser_.prototype.putBack_ = function(tok) {
  this.putBackToken_ = tok;
};

/**
 * Returns the next token.
 * @param {!{firstIndex: number, lastIndex: number, lineNumber:
 *     number}} parseState The parse state.
 * @param {string} data The data being parsed.
 * @private
 */
BaseDataParser_.prototype.nextToken_ = function(parseState, data) {
  if (this.putBackToken_) {
    var tmp = this.putBackToken_;
    this.putBackToken_ = null;
    return tmp;
  }
  parseState.firstIndex = parseState.lastIndex;
  if (parseState.firstIndex < data.length) {
    // Eat whitespace and separators
    while (true) {
      var curChar = data.charAt(parseState.firstIndex);
      if (this.isWhitespaceOrSeparator_(curChar)) {
        ++parseState.firstIndex;
        if (parseState.firstIndex >= data.length)
          break;
      } else {
        break;
      }
    }
  }
  if (parseState.firstIndex >= data.length)
    return { kind: BaseDataParser_.TokenTypes.NONE, val: null };
  parseState.lastIndex = parseState.firstIndex;
  // Surround the next token
  var curChar = data.charAt(parseState.lastIndex++);
  if (curChar == '-' ||
      curChar == '.' ||
      (curChar >= '0' && curChar <= '9')) {
    while (true) {
      var t = data.charAt(parseState.lastIndex);
      if (t == '.' ||
          (t >= '0' && t <= '9')) {
        ++parseState.lastIndex;
      } else {
        break;
      }
    }
    // See whether an exponential format follows: i.e. 136e-3
    if (data.charAt(parseState.lastIndex) == 'e') {
      ++parseState.lastIndex;
      if (data.charAt(parseState.lastIndex) == '-') {
        ++parseState.lastIndex;
      }
      while (true) {
        var t = data.charAt(parseState.lastIndex);
        if (t >= '0' && t <= '9') {
          ++parseState.lastIndex;
        } else {
          break;
        }
      }
    }
    return { kind: BaseDataParser_.TokenTypes.NUMBER,
             val: parseFloat(data.substring(parseState.firstIndex,
                                            parseState.lastIndex)) };
  } else if ((curChar >= 'A' && curChar <= 'Z') ||
             (curChar >= 'a' && curChar <= 'z')) {
    if (!this.singleLetterWords_) {
      // Consume all adjacent letters -- this is satisfactory for the
      // grammar of the "transform" attribute, but not the "d"
      // attribute
      while (true) {
        var t = data.charAt(parseState.lastIndex);
        if ((t >= 'A' && t <= 'Z') ||
            (t >= 'a' && t <= 'z')) {
          ++parseState.lastIndex;
        } else {
          break;
        }
      }
    }
    return { kind: BaseDataParser_.TokenTypes.WORD,
             val: data.substring(parseState.firstIndex,
                                 parseState.lastIndex) };
  } else if (curChar == '(') {
    return { kind: BaseDataParser_.TokenTypes.LPAREN,
             val: data.substring(parseState.firstIndex,
                                 parseState.lastIndex) };
  } else if (curChar == ')') {
    return { kind: BaseDataParser_.TokenTypes.RPAREN,
             val: data.substring(parseState.firstIndex,
                                 parseState.lastIndex) };
  }
  throw 'Expected number or word at line ' + parseState.lineNumber;
};

/**
 * Verifies that the next token is of the given kind, throwing an
 * exception if not.
 * @param {!{firstIndex: number, lastIndex: number, lineNumber:
 *     number}} parseState The parse state.
 * @param {string} data The data being parsed.
 * @param {BaseDataParser_TokenTypes} tokenType The expected token
 *     type.
 */
BaseDataParser_.prototype.expect_ = function(parseState, data, tokenType) {
  var tok = this.nextToken_(parseState, data);
  if (tok.kind != tokenType) {
    throw 'At line number ' + parseState.lineNumber +
        ': expected token type ' + tokenType +
        ', got ' + tok.kind;
  }
};

/**
 * Parses a series of floating-point numbers.
 * @param {!{firstIndex: number, lastIndex: number, lineNumber:
 *     number}} parseState The parse state.
 * @param {string} data The data being parsed.
 * @param {number} numFloats The number of floating-point numbers to
 *     parse.
 * @return {!Array.<number>}
 * @private
 */
BaseDataParser_.prototype.parseFloats_ = function(parseState,
                                                  data,
                                                  numFloats) {
  var result = [];
  for (var i = 0; i < numFloats; ++i) {
    var tok = this.nextToken_(parseState, data);
    if (tok.kind != BaseDataParser_.TokenTypes.NUMBER)
      throw "Expected number at line " +
          parseState.lineNumber +
          ", character " +
          parseState.firstIndex +
          "; got \"" + tok.val + "\"";
    result.push(tok.val);
  }
  return result;
};

//----------------------------------------------------------------------
// PathDataParser

/**
 * Parser for the data in an SVG path.
 * @param {SVGLoader} loader The SVG loader.
 * @param {number} lineNumber the line number of the current parse,
 *     for error reporting.
 * @param {boolean} flipY Whether the Y coordinates should be flipped
 *     to better match the 3D coordinate system.
 * @param {!o3d.Pack} pack Pack to manage created objects.
 * @param {!o3d.DrawList} drawList DrawList to use for created
 *     materials.
 * @constructor
 * @extends {BaseDataParser_}
 * @private
 */
PathDataParser_ = function(loader,
                           lineNumber,
                           flipY,
                           pack,
                           drawList) {
  BaseDataParser_.call(this, loader, lineNumber);
  this.flipY_ = flipY;
  this.pack_ = pack;
  this.drawList_ = drawList;
  this.curX_ = 0;
  this.curY_ = 0;
  this.singleLetterWords_ = true;
};

o3djs.base.inherit(PathDataParser_, BaseDataParser_);

/**
 * Parses data from an SVG path.
 * @param {string} data SVG path data.
 * @private
 */
PathDataParser_.prototype.parse = function(data) {
  this.path_ = o3djs.gpu2d.createPath(this.pack_, this.drawList_);
  BaseDataParser_.prototype.parse.call(this, data);
  this.path_.update();
  return this.path_;
};

/**
 * Parses the data for the given word (command) from the path data.
 * @param {!{firstIndex: number, lastIndex: number, lineNumber:
 *     number}} parseState The parse state.
 * @param {string} data The data being parsed.
 * @param {string} word The character for the current command.
 * @private
 */
PathDataParser_.prototype.parseWord_ = function(parseState,
                                                data,
                                                word) {
  if (word == 'M' || word == 'm') {
    var absolute = (word == 'M');
    var coords = this.parseFloats_(parseState, data, 2);
    this.doYFlip_(coords);
    if (!absolute) {
      this.makeAbsolute_(coords);
    }
    this.path_.moveTo(coords[0], coords[1]);
    this.curX_ = coords[0];
    this.curY_ = coords[1];
    // This is a horrible portion of the SVG spec
    if (absolute) {
      this.setLastCommand_('L');
    } else {
      this.setLastCommand_('l');
    }
  } else if (word == 'L' || word == 'l') {
    var absolute = (word == 'L');
    var coords = this.parseFloats_(parseState, data, 2);
    this.doYFlip_(coords);
    if (!absolute) {
      this.makeAbsolute_(coords);
    }
    this.path_.lineTo(coords[0], coords[1]);
    this.curX_ = coords[0];
    this.curY_ = coords[1];
  } else if (word == 'H' || word == 'h') {
    var absolute = (word == 'H');
    var coords = this.parseFloats_(parseState, data, 1);
    if (!absolute) {
      coords[0] += this.curX_;
    }
    this.path_.lineTo(coords[0], this.curY_);
    this.curX_ = coords[0];
  } else if (word == 'V' || word == 'v') {
    var absolute = (word == 'V');
    var coords = this.parseFloats_(parseState, data, 1);
    if (this.flipY_) {
      coords[0] = -coords[0];
    }
    if (!absolute) {
      coords[0] += this.curY_;
    }
    this.path_.lineTo(this.curX_, coords[0]);
    this.curY_ = coords[0];
  } else if (word == 'Q' || word == 'q') {
    var absolute = (word == 'Q');
    var coords = this.parseFloats_(parseState, data, 4);
    this.doYFlip_(coords);
    if (!absolute) {
      this.makeAbsolute_(coords);
    }
    this.path_.quadraticTo(coords[0], coords[1], coords[2], coords[3]);
    this.curX_ = coords[2];
    this.curY_ = coords[3];
    // TODO(kbr): support shorthand quadraticTo (T/t)
  } else if (word == 'C' || word == 'c') {
    var absolute = (word == 'C');
    var coords = this.parseFloats_(parseState, data, 6);
    this.doYFlip_(coords);
    if (!absolute) {
      this.makeAbsolute_(coords);
    }
    this.path_.cubicTo(coords[0], coords[1],
                       coords[2], coords[3],
                       coords[4], coords[5]);
    this.curX_ = coords[4];
    this.curY_ = coords[5];
    // TODO(kbr): support shorthand cubicTo (S/s)
  } else if (word == 'Z' || word == 'z') {
    this.path_.close();
  } else {
    throw 'Unknown word ' + word + ' at line ' + parseState.lineNumber;
  }
};

/**
 * Negates the Y coordinates of the passed 2D coordinate list if the
 * flipY flag is set on this parser.
 * @param {!Array.<number>} coords Array of 2D coordinates.
 * @private
 */

PathDataParser_.prototype.doYFlip_ = function(coords) {
  if (this.flipY_) {
    for (var i = 0; i < coords.length; i += 2) {
      coords[i+1] = -coords[i+1];
    }
  }
};

/**
 * Transforms relative coordinates to absolute coordinates.
 * @param {!Array.<number>} coords Array of 2D coordinates.
 * @private
 */
PathDataParser_.prototype.makeAbsolute_ = function(coords) {
  for (var i = 0; i < coords.length; i += 2) {
    coords[i] += this.curX_;
    coords[i+1] += this.curY_;
  }
};

//----------------------------------------------------------------------
// TransformDataParser

/**
 * Parser for the data in an SVG transform.
 * @param {SVGLoader} loader The SVG loader.
 * @param {number} lineNumber the line number of the current parse,
 *     for error reporting.
 * @constructor
 * @extends {BaseDataParser_}
 * @private
 */
TransformDataParser_ = function(loader,
                                lineNumber) {
  BaseDataParser_.call(this, loader, lineNumber);
};

o3djs.base.inherit(TransformDataParser_, BaseDataParser_);

/**
 * Parses the data for the given word (command) from the path data.
 * @param {!{firstIndex: number, lastIndex: number, lineNumber:
 *     number}} parseState The parse state.
 * @param {string} data The data being parsed.
 * @param {string} word The character for the current command.
 * @private
 */
TransformDataParser_.prototype.parseWord_ = function(parseState,
                                                     data,
                                                     word) {
  if (!((word == 'matrix') ||
        (word == 'translate') ||
        (word == 'scale') ||
        (word == 'rotate') ||
        (word == 'skewX') ||
        (word == 'skewY'))) {
    throw 'Unknown transform definition ' + word +
        ' at line ' + parseState.lineNumber;
  }

  this.expect_(parseState, data, BaseDataParser_.TokenTypes.LPAREN);
  // Some of the commands take a variable number of arguments
  var floats;
  switch (word) {
    case 'matrix':
      floats = this.parseFloats_(parseState, data, 6);
      this.loader_.matrix_(floats[0], floats[1],
                           floats[2], floats[3],
                           floats[4], floats[5]);
      break;
    case 'translate':
      floats = this.parseFloats_(parseState, data, 1);
      var tok = this.nextToken_(parseState, data);
      if (tok.kind == BaseDataParser_.TokenTypes.NUMBER) {
        floats.push(tok.val);
      } else {
        this.putBack_(tok);
        floats.push(0);
      }
      this.loader_.translate_(floats[0], floats[1]);
      break;
    case 'scale':
      floats = this.parseFloats_(parseState, data, 1);
      var tok = this.nextToken_(parseState, data);
      if (tok.kind == BaseDataParser_.TokenTypes.NUMBER) {
        floats.push(tok.val);
      } else {
        this.putBack_(tok);
        floats.push(floats[0]);
      }
      this.loader_.scale_(floats[0], floats[1]);
      break;
    case 'rotate':
      floats = this.parseFloats_(parseState, data, 1);
      var tok = this.nextToken_(parseState, data);
      this.putBack_(tok);
      if (tok.kind == BaseDataParser_.TokenTypes.NUMBER) {
        floats = floats.concat(this.parseFloats_(parseState, data, 2));
      } else {
        floats.push(0);
        floats.push(0);
      }
      this.loader_.rotate_(floats[0], floats[1], floats[2]);
      break;
    case 'skewX':
      floats = this.parseFloats_(parseState, data, 1);
      this.loader_.skewX_(floats[0]);
      break;
    case 'skewY':
      floats = this.parseFloats_(parseState, data, 1);
      this.loader_.skewY_(floats[0]);
      break;
    default:
      throw 'Unknown word ' + word + ' at line ' + parseState.lineNumber;
  }
  this.expect_(parseState, data, BaseDataParser_.TokenTypes.RPAREN);
};

//----------------------------------------------------------------------
// SVGSaxHandler

/**
 * Handler which integrates with the XMLJS SAX parser.
 * @param {!SVGLoader} loader The SVG loader.
 * @param {Object} parser the XMLJS SAXDriver.
 * @param {boolean} flipY Whether the Y coordinates should be flipped
 *     to better match the 3D coordinate system.
 * @param {!o3d.Pack} pack Pack to manage created objects.
 * @param {!o3d.DrawList} drawList DrawList to use for created
 *     materials.
 * @constructor
 * @private
 */
SVGSAXHandler_ = function(loader, parser, flipY, pack, drawList) {
  this.loader_ = loader;
  this.parser_ = parser;
  this.flipY_ = flipY;
  this.pack_ = pack;
  this.drawList_ = drawList;
};

/**
 * Handler for the start of an element.
 * @param {string} name the name of the element.
 * @param {Object} attributes the XMLJS attributes object.
 * @private
 */
SVGSAXHandler_.prototype.startElement = function(name, attributes) {
  switch (name) {
    case 'path':
      var pushed = false;
      var transformData = attributes.getValueByName('transform');
      if (transformData) {
        pushed = true;
        this.loader_.pushTransform_();
        this.loader_.parseTransform_(transformData,
                                     this.parser_.getLineNumber());
      }
      if (attributes.getValueByName('style')) {
        this.loader_.parseStyle_(attributes.getValueByName('style'),
                                 this.parser_.getLineNumber(),
                                 this.pack_);
      }
      this.loader_.parsePath_(attributes.getValueByName('d'),
                              this.parser_.getLineNumber(),
                              this.flipY_,
                              this.pack_,
                              this.drawList_);
      if (pushed) {
        this.loader_.popTransform_();
      }
      break;
    case 'g':
      this.loader_.pushTransform_();
      if (attributes.getValueByName('style')) {
        this.loader_.parseStyle_(attributes.getValueByName('style'),
                                 this.parser_.getLineNumber(),
                                 this.pack_);
      }
      var data = attributes.getValueByName('transform');
      if (data) {
        this.loader_.parseTransform_(data,
                                     this.parser_.getLineNumber());
      }
      break;
    default:
      // No other commands supported yet
      break;
  }
};

/**
 * Handler for the end of an element.
 * @param {string} name the name of the element.
 * @private
 */
SVGSAXHandler_.prototype.endElement = function(name) {
  if (name == 'g') {
    this.loader_.popTransform_();
  }
};

