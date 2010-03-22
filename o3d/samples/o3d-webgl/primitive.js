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
 * A Primitive is a type of Element that is made from a list of points,
 * lines or triangles that use a single material.
 * 
 * @param opt_streamBank o3d.StreamBank The StreamBank used by this
 *     Primitive.
 * @constructor
 */
o3d.Primitive = function(opt_streamBank) {
  o3d.Element.call(this);
  this.streamBank = opt_streamBank;
};
o3d.inherit('Primitive', 'Element');


/**
 * @type {number}
 */
o3d.Primitive.Type = goog.typedef;

/**
 * Type of geometric primitives used by the Primitive.
 */
o3d.Primitive.POINTLIST = 1;
o3d.Primitive.LINELIST = 2;
o3d.Primitive.LINESTRIP = 3;
o3d.Primitive.TRIANGLELIST = 4;
o3d.Primitive.TRIANGLESTRIP = 5;
o3d.Primitive.TRIANGLEFAN = 6;



/**
 * The type of primitive the primitive is (i.e., POINTLIST, LINELIST,
 * TRIANGLELIST, etc.)
 * 
 * @type {o3d.Primitive.Type}
 */
o3d.Primitive.prototype.primitiveType = o3d.Primitive.TRIANGLELIST;



/**
 * The number of vertices the primitive has.
 * 
 * @type {number}
 */
o3d.Primitive.prototype.numberVertices = 0;



/**
 * The number of rendering primitives (i.e., triangles, points, lines) the
 * primitive has.
 * 
 * @type {number}
 */
o3d.Primitive.prototype.numberPrimitives = 0;



/**
 * The index of the first vertex to render.
 * Default = 0.
 * 
 * @type {number}
 */
o3d.Primitive.prototype.startIndex = 0;



/**
 * The stream bank this primitive uses for vertices.
 * @type {o3d.StreamBank}
 */
o3d.Primitive.prototype.streamBank = null;



/**
 * The index buffer for the primitive. If null the primitive is non-indexed.
 * @type {number}
 */
o3d.Primitive.prototype.indexBuffer = null;



/**
 * Binds the vertex and index streams required to draw the shape.
 */
o3d.Primitive.prototype.render = function() {
  var streamBank = this.streamBank;
  var indexBuffer = this.indexBuffer;

  var enabled_attribs = [];

  for (var semantic = 0;
       semantic < streamBank.vertexStreams.length;
       ++semantic) {
    var streams = streamBank.vertexStreams[semantic];
    if (streams && streams.length) {
      for (var semantic_index = 0;
           semantic_index < streams.length;
           ++semantic_index) {
        var gl_index = semantic + semantic_index - 1;
        var stream = streams[semantic_index];
        var field = stream.field;
        var buffer = field.buffer;

        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, buffer.gl_buffer_);
        this.gl.enableVertexAttribArray(gl_index);
        enabled_attribs.push(gl_index);

        // TODO(petersont): Change that hard-coded 4 down there.
        this.gl.vertexAttribPointer(
            gl_index, field.numComponents, this.gl.FLOAT, false,
            buffer.totalComponents * 4, field.offset_ * 4);
      }
    }
  }

  // TODO(petersont): Change the hard-coded 3 and triangles too.
  this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, indexBuffer.gl_buffer_);
  this.gl.drawElements(this.gl.TRIANGLES,
                       this.numberPrimitives * 3,
                       this.gl.UNSIGNED_SHORT,
                       0);

  for (var i = 0; i < enabled_attribs.length; ++i) {
    this.gl.disableVertexAttribArray(enabled_attribs[i]);
  }
};
