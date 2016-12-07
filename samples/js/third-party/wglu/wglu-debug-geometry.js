/*
Copyright (c) 2016, Brandon Jones.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

var WGLUDebugGeometry = (function() {

  "use strict";

  var debugGeomVS = [
    "uniform mat4 projectionMat;",
    "uniform mat4 viewMat;",
    "uniform mat4 modelMat;",
    "uniform mat3 normalMat;",
    "attribute vec3 position;",
    "attribute vec3 normal;",
    "varying vec3 v_normal;",

    "void main() {",
    "  gl_Position = projectionMat * viewMat * modelMat * vec4( position, 1.0 );",
    "  v_normal = normalMat * normal;",
    "}",
  ].join("\n");

  // Simple shading with a single light source, this uses half-lambert
  // shading to keep things from getting too dark in the unlit areas.
  // It's not physically based but looks ok.
  var debugGeomFS = [
    "precision mediump float;",
    "uniform vec4 color;",
    "uniform vec3 light;",
    "varying vec3 v_normal;",

    "void main() {",
    "  vec3 normal = normalize(v_normal);",
    "  gl_FragColor = ((dot(light, normal) * 0.5 + 0.5) * 0.8 + 0.2) * color;",
    "}",
  ].join("\n");

  var DebugGeometry = function(gl) {
    this.gl = gl;

    this.projMat = mat4.create();
    this.viewMat = mat4.create();
    this.modelMat = mat4.create();
    this.normalMat = mat3.create();

    this.program = new WGLUProgram(gl);
    this.program.attachShaderSource(debugGeomVS, gl.VERTEX_SHADER);
    this.program.attachShaderSource(debugGeomFS, gl.FRAGMENT_SHADER);
    this.program.bindAttribLocation({ position: 0 });
    this.program.bindAttribLocation({ normal: 1 });
    this.program.link();

    var verts = [];
    var indices = [];

    //
    // Cube Geometry
    //
    this.cubeIndexOffset = indices.length;

    var size = 0.5;
    // Bottom
    var idx = verts.length / 6.0;
    indices.push(idx, idx+1, idx+2);
    indices.push(idx, idx+2, idx+3);

    verts.push(-size, -size, -size, 0, -1, 0);
    verts.push(+size, -size, -size, 0, -1, 0);
    verts.push(+size, -size, +size, 0, -1, 0);
    verts.push(-size, -size, +size, 0, -1, 0);

    // Top
    idx = verts.length / 6.0;
    indices.push(idx, idx+2, idx+1);
    indices.push(idx, idx+3, idx+2);

    verts.push(-size, +size, -size, 0, 1, 0);
    verts.push(+size, +size, -size, 0, 1, 0);
    verts.push(+size, +size, +size, 0, 1, 0);
    verts.push(-size, +size, +size, 0, 1, 0);

    // Left
    idx = verts.length / 6.0;
    indices.push(idx, idx+2, idx+1);
    indices.push(idx, idx+3, idx+2);

    verts.push(-size, -size, -size, -1, 0, 0);
    verts.push(-size, +size, -size, -1, 0, 0);
    verts.push(-size, +size, +size, -1, 0, 0);
    verts.push(-size, -size, +size, -1, 0, 0);

    // Right
    idx = verts.length / 6.0;
    indices.push(idx, idx+1, idx+2);
    indices.push(idx, idx+2, idx+3);

    verts.push(+size, -size, -size, 1, 0, 0);
    verts.push(+size, +size, -size, 1, 0, 0);
    verts.push(+size, +size, +size, 1, 0, 0);
    verts.push(+size, -size, +size, 1, 0, 0);

    // Back
    idx = verts.length / 6.0;
    indices.push(idx, idx+2, idx+1);
    indices.push(idx, idx+3, idx+2);

    verts.push(-size, -size, -size, 0, 0, -1);
    verts.push(+size, -size, -size, 0, 0, -1);
    verts.push(+size, +size, -size, 0, 0, -1);
    verts.push(-size, +size, -size, 0, 0, -1);

    // Front
    idx = verts.length / 6.0;
    indices.push(idx, idx+1, idx+2);
    indices.push(idx, idx+2, idx+3);

    verts.push(-size, -size, +size, 0, 0, 1);
    verts.push(+size, -size, +size, 0, 0, 1);
    verts.push(+size, +size, +size, 0, 0, 1);
    verts.push(-size, +size, +size, 0, 0, 1);

    this.cubeIndexCount = indices.length - this.cubeIndexOffset;

    //
    // Cone Geometry
    //
    this.coneIndexOffset = indices.length;

    var size = 0.5;
    var conePointVertex = verts.length / 6.0;
    var coneBaseVertex = conePointVertex+1;
    var coneSegments = 64;

    // Cone side vertices
    for (var i = 0; i < coneSegments; ++i) {
        idx = verts.length / 6.0;
        indices.push(idx, idx + 1, idx + 2);
        var rad = ((Math.PI * 2) / coneSegments) * i;
        var rad2 = ((Math.PI * 2) / coneSegments) * (i + 1);
        verts.push(Math.sin(rad) * (size / 2), -size, Math.cos(rad) * (size / 2),
                   Math.sin(rad), 0.25, Math.cos(rad));

        verts.push(Math.sin(rad2) * (size / 2), -size, Math.cos(rad2) * (size / 2),
                   Math.sin(rad2), 0.25, Math.cos(rad2));

        verts.push(0, size, 0,
                   Math.sin((rad + rad2) / 2), 0.25, Math.cos((rad + rad2) / 2));
    }

    // Base triangles
    var baseCenterIdx = verts.length / 6.0;
    verts.push(0, -size, 0, 0, -1, 0);
    for (var i = 0; i < coneSegments; ++i) {
      idx = verts.length / 6.0;
      indices.push(baseCenterIdx, idx, idx + 1);
      var rad = ((Math.PI * 2) / coneSegments) * i;
      var rad2 = ((Math.PI * 2) / coneSegments) * (i + 1);
      verts.push(Math.sin(rad2) * (size / 2.0), -size, Math.cos(rad2) * (size  / 2.0), 0, -1, 0);
      verts.push(Math.sin(rad) * (size / 2.0), -size, Math.cos(rad) * (size  / 2.0), 0, -1, 0);
    }

    this.coneIndexCount = indices.length - this.coneIndexOffset;

    //
    // Rect geometry
    //
    this.rectIndexOffset = indices.length;

    idx = verts.length / 6.0;
    indices.push(idx, idx+1, idx+2, idx+3, idx);

    verts.push(0, 0, 0, 0, 0, -1);
    verts.push(1, 0, 0, 0, 0, -1);
    verts.push(1, 1, 0, 0, 0, -1);
    verts.push(0, 1, 0, 0, 0, -1);

    this.rectIndexCount = indices.length - this.rectIndexOffset;

    this.vertBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, this.vertBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(verts), gl.STATIC_DRAW);

    this.indexBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.indexBuffer);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(indices), gl.STATIC_DRAW);
  };

  DebugGeometry.prototype.bind = function(projectionMat, viewMat) {
    var gl = this.gl;
    var program = this.program;

    program.use();

    gl.uniformMatrix4fv(program.uniform.projectionMat, false, projectionMat);
    gl.uniformMatrix4fv(program.uniform.viewMat, false, viewMat);

    gl.bindBuffer(gl.ARRAY_BUFFER, this.vertBuffer);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.indexBuffer);

    gl.enableVertexAttribArray(program.attrib.position);

    gl.vertexAttribPointer(program.attrib.position, 3, gl.FLOAT, false, 24, 0);
    gl.vertexAttribPointer(program.attrib.normal, 3, gl.FLOAT, false, 24, 12);
  };

  DebugGeometry.prototype.bindOrtho = function() {
    mat4.ortho(this.projMat, 0, this.gl.canvas.width, this.gl.canvas.height, 0, 0.1, 1024);
    mat4.identity(this.viewMat);
    this.bind(this.projMat, this.viewMat);
  };

  DebugGeometry.prototype._bindUniformsRaw = function(model, color, light) {
    if (!color) { color = [1, 0, 0, 1]; }
    if (!light) { light = [0.75, 0.5, 1.0]; }  // Should match vr-cube-sea.js
    var lightVec = vec3.fromValues(light[0], light[1], light[2]);
    vec3.normalize(lightVec, lightVec);

    mat3.normalFromMat4(this.normalMat, model);

    this.gl.uniformMatrix4fv(this.program.uniform.modelMat, false, model);
    this.gl.uniformMatrix3fv(this.program.uniform.normalMat, false, this.normalMat);
    this.gl.uniform4fv(this.program.uniform.color, color);
    this.gl.uniform3fv(this.program.uniform.light, lightVec);
  };

  DebugGeometry.prototype._bindUniforms = function(orientation, position, scale, color, light) {
    if (!position) { position = [0, 0, 0]; }
    if (!orientation) { orientation = [0, 0, 0, 1]; }
    if (!scale) { scale = [1, 1, 1]; }

    mat4.fromRotationTranslationScale(this.modelMat, orientation, position, scale);
    this._bindUniformsRaw(this.modelMat, color, light);
  };

  DebugGeometry.prototype.drawCube = function(orientation, position, size, color) {
    var gl = this.gl;

    if (!size) { size = 1; }
    this._bindUniforms(orientation, position, [size, size, size], color);
    gl.drawElements(gl.TRIANGLES, this.cubeIndexCount, gl.UNSIGNED_SHORT, this.cubeIndexOffset * 2.0);
  };

  DebugGeometry.prototype.drawBox = function(orientation, position, scale, color) {
    var gl = this.gl;

    this._bindUniforms(orientation, position, scale, color);
    gl.drawElements(gl.TRIANGLES, this.cubeIndexCount, gl.UNSIGNED_SHORT, this.cubeIndexOffset * 2.0);
  };

  DebugGeometry.prototype.drawBoxWithMatrix = function(mat, color) {
    var gl = this.gl;

    this._bindUniformsRaw(mat, color);
    gl.drawElements(gl.TRIANGLES, this.cubeIndexCount, gl.UNSIGNED_SHORT, this.cubeIndexOffset * 2.0);
  };

  DebugGeometry.prototype.drawRect = function(x, y, width, height, color) {
    var gl = this.gl;

    this._bindUniforms(null, [x, y, -1], [width, height, 1], color);
    gl.drawElements(gl.LINE_STRIP, this.rectIndexCount, gl.UNSIGNED_SHORT, this.rectIndexOffset * 2.0);
  };

  DebugGeometry.prototype.drawCone = function(orientation, position, size, color) {
    var gl = this.gl;

    if (!size) { size = 1; }
    this._bindUniforms(orientation, position, [size, size, size], color);
    gl.drawElements(gl.TRIANGLES, this.coneIndexCount, gl.UNSIGNED_SHORT, this.coneIndexOffset * 2.0);
  };

  DebugGeometry.prototype.drawConeWithMatrix = function(mat, color) {
    var gl = this.gl;

    this._bindUniformsRaw(mat, color);
    gl.drawElements(gl.TRIANGLES, this.coneIndexCount, gl.UNSIGNED_SHORT, this.coneIndexOffset * 2.0);
  };

  return DebugGeometry;
})();
