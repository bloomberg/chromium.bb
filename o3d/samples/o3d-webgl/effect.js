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
 * EffectParameterInfo holds information about the Parameters an Effect needs.
 * o3d.Effect.getParameterInfo
 */
o3d.EffectParameterInfo = function() { };
o3d.inherit('EffectParameterInfo', 'NamedObject');


/**
 * The name of the parameter.
 * @type {string}
 */
o3d.EffectParameterInfo.prototype.name = '';



/**
 * The type of the parameter.
 * @type {string}
 */
o3d.EffectParameterInfo.prototype.className = '';



/**
 * The number of elements.  Non-zero for array types, zero for non-array types.
 * @type {number}
 */
o3d.EffectParameterInfo.prototype.numElements = 0;



/**
 * The semantic of the parameter. This is always in UPPERCASE.
 * @type {o3d.Stream.Semantic}
 */
o3d.EffectParameterInfo.prototype.semantic = o3d.Stream.UNKNOWN_SEMANTIC;


/**
 * If this is a standard parameter (SAS) this will be the name of the type
 * of Param needed. Otherwise it will be the empty string.
 * 
 * Standard Parameters are generally handled automatically by o3d but you
 * can supply your own if you have a unique situation.
 * 
 * @type {string}
 */
o3d.EffectParameterInfo.prototype.sas_class_name = '';



/**
 * EffectStreamInfo holds information about the Streams an Effect needs.
 * @param {o3d.Stream.Semantic} opt_semantic The semantic of the stream 
 * @param {number} opt_semantic_index
 * @constructor
 */
o3d.EffectStreamInfo = function(opt_semantic, opt_semantic_index) {
  o3d.NamedObject.call(this);
  if (!opt_semantic) {
    opt_semantic = o3d.Stream.UNKNOWN_SEMANTIC;
  }
  if (!opt_semantic_index) {
    opt_semantic_index = 0;
  }
  this.semantic = opt_semantic;
  this.opt_semantic_index = opt_semantic_index;
};
o3d.inherit('EffectStreamInfo', 'NamedObject');


/**
 * The semantic of the stream.
 * @type {!o3d.Stream.Semantic}
 */
o3d.EffectStreamInfo.prototype.semantic = o3d.Stream.UNKNOWN_SEMANTIC;



/**
 * The semantic index of the stream.
 * @type {number}
 */
o3d.EffectStreamInfo.prototype.semanticIndex = 0;


/**
 * An Effect contains a vertex and pixel shader.
 * @constructor
 */
o3d.Effect = function() {
  o3d.ParamObject.call(this);
  this.program_ = null;
};
o3d.inherit('Effect', 'ParamObject');


/**
 * Indicates whether the vertex shader has been loaded, so we can
 * postpone linking until both shaders are in.
 * 
 * @type {boolean}
 */
o3d.Effect.prototype.vertexShaderLoaded_ = false;


/**
 * Indicates whether the fragment shader has been loaded, so we can
 * postpone linking until both shaders are in.
 * 
 * @type {boolean}
 */
o3d.Effect.prototype.fragmentShaderLoaded_ = false;


/**
 * Binds standard attribute locations for the shader.
 */
o3d.Effect.prototype.bindAttributesAndLinkIfReady = function() {
  if (this.vertexShaderLoaded_ && this.fragmentShaderLoaded_) {
    var attributes = ['position', 'normal', 'tangent', 'binormal', 'color',
                      'texCoord0', 'texCoord1', 'texCoord2', 'texCoord3',
                      'texCoord4', 'texCoord5', 'texCoord6', 'texCoord7'];
    for (var i = 0; i < attributes.length; ++i) {
      this.gl.bindAttribLocation(this.program_, i, attributes[i]);
    }
    this.gl.linkProgram(this.program_);
    this.getAllUniforms();
  }
};


/**
 * Helper function for loadVertexShaderFromString and
 * loadPixelShaderFromString that takes the type as an argument.
 * @param {string} shaderString The shader code.
 * @param {number} type The type of the shader: either
 *    VERTEX_SHADER or FRAGMENT_SHADER.
 */
o3d.Effect.prototype.loadShaderFromString = function(shaderString, type) {
  if (!this.program_) {
    this.program_ = this.gl.createProgram();
  }
  var shader = this.gl.createShader(type);
  this.gl.shaderSource(shader, shaderString);
  this.gl.compileShader(shader);

  var log = this.gl.getShaderInfoLog(shader);
  if (log != '') {
    this.gl.client.error_callback(
        'Shader compile failed with error log:\n' + log);
  }

  this.gl.attachShader(this.program_, shader);
};


/**
 * Loads a glsl vertex shader for this effect from a string.
 * @param {string} shaderString The string.
 */
o3d.Effect.prototype.loadVertexShaderFromString =
    function(shaderString) {
  this.loadShaderFromString(shaderString, this.gl.VERTEX_SHADER);
  this.vertexShaderLoaded_ = true;
  o3d.Effect.prototype.bindAttributesAndLinkIfReady();
};


/**
 * Loads a glsl vertex shader for this effect from a string.
 * @param {string} shaderString The string.
 */
o3d.Effect.prototype.loadPixelShaderFromString =
    function(shaderString) {
  this.loadShaderFromString(shaderString, this.gl.FRAGMENT_SHADER);
  this.fragmentShaderLoaded_ = true;
  this.bindAttributesAndLinkIfReady();
};


/**
 * Iterates through the active uniforms of the program and gets the
 * location of each one and stores them by name in the uniforms
 * object.
 */
o3d.Effect.prototype.getAllUniforms =
    function() {
  this.uniforms = {};
  var numUniforms = this.gl.getProgramParameter(
      this.program_, this.gl.ACTIVE_UNIFORMS);
  for (var i = 0; i < numUniforms; ++i) {
    var info = this.gl.getActiveUniform(this.program_, i);
    this.uniforms[info.name] = {info:info,
        location:this.gl.getUniformLocation(this.program_, info.name)};
  }
};


/**
 * For each of the effect's uniform parameters, creates corresponding
 * parameters on the given ParamObject. Skips SAS Parameters.
 * 
 * If a Param with the same name but the wrong type already exists on the
 * given ParamObject createUniformParameters will attempt to replace it with
 * one of the correct type.
 * 
 * Note: The most common thing to pass to this function is a Material but
 * depending on your application it may be more appropriate to pass in a
 * Transform, Effect, Element or DrawElement.
 * 
 * @param {!o3d.ParamObject} param_object The param object on which the
 *     new paramters will be created.
 */
o3d.Effect.prototype.createUniformParameters =
    function(param_object) {
  var sasNames = {'world': true,
                  'view': true,
                  'projection': true,
                  'worldView': true,
                  'worldProjection': true,
                  'worldViewProjection': true,
                  'worldInverse': true,
                  'viewInverse': true,
                  'projectionInverse': true,
                  'worldViewInverse': true,
                  'worldProjectionInverse': true,
                  'worldViewProjectionInverse': true,
                  'worldTranspose': true,
                  'viewTranspose': true,
                  'projectionTranspose': true,
                  'worldViewTranspose': true,
                  'worldProjectionTranspose': true,
                  'worldViewProjectionTranspose': true,
                  'worldInverseTranspose': true,
                  'viewInverseTranspose': true,
                  'projectionInverseTranspose': true,
                  'worldViewInverseTranspose': true,
                  'worldProjectionInverseTranspose': true,
                  'worldViewProjectionInverseTranspose': true};

  for (name in this.uniforms) {
    var info = this.uniforms[name].info;

    if (sasNames[name])
      continue;

    var paramType = '';
    switch (info.type) {
      case this.gl.FLOAT:
        paramType = 'ParamFloat';
        break;
      case this.gl.FLOAT_VEC2:
        paramType = 'ParamFloat2';
        break;
      case this.gl.FLOAT_VEC3:
        paramType = 'ParamFloat3';
        break;
      case this.gl.FLOAT_VEC4:
        paramType = 'ParamFloat4';
        break;
      case this.gl.INT:
        paramType = 'ParamInteger';
        break;
      case this.gl.BOOL:
        paramType = 'ParamBoolean';
        break;
      case this.gl.FLOAT_MAT4:
        paramType = 'ParamMatrix4';
        break;
      case this.gl.SAMPLER_2D:
        paramType = 'ParamSampler';
        break;
      case this.gl.SAMPLER_CUBE:
        paramType = 'ParamSampler';
        break;
    }

    param_object.createParam(info.name, paramType);
  }
};


/**
 * For each of the effect's uniform parameters, if it is a SAS parameter
 * creates corresponding StandardParamMatrix4 parameters on the given
 * ParamObject.  Note that SAS parameters are handled automatically by the
 * rendering system. so except in some rare cases there is no reason to call
 * this function.  Also be aware that the StandardParamMatrix4 Paramters like
 * WorldViewProjectionParamMatrix4, etc.. are only valid during rendering.
 * At all other times they will not return valid values.
 * 
 * If a Param with the same name but the wrong type already exists on the
 * given ParamObject CreateSASParameters will attempt to replace it with
 * one of the correct type.
 * 
 * @param {!o3d.ParamObject} param_object The param object on which the new
 *     paramters will be created.
 */
o3d.Effect.prototype.createSASParameters =
    function(param_object) {
  o3d.notImplemented();
};


/**
 * Gets info about the parameters this effect needs.
 * @return {!Array.<!o3d.EffectParameterInfo>} an array of
 *     EffectParameterInfo objects.
 */
o3d.Effect.prototype.getParameterInfo = function() {
  o3d.notImplemented();
  return [];
};


/**
 * Gets info about the streams this effect needs.
 * @return {!Array.<!o3d.EffectStreamInfo>} an array of
 *     EffectStreamInfo objects.
 */
o3d.Effect.prototype.getStreamInfo = function() {
  var r = [];
  // TODO(petersont): This is a stub, will remove later and replace with
  // something that actually gets its streams from the shader.
  var standard_semantic_index_pairs = [
    {semantic: o3d.Stream.POSITION, index: 0},
    {semantic: o3d.Stream.NORMAL, index: 0},
    {semantic: o3d.Stream.COLOR, index: 0},
    {semantic: o3d.Stream.TEXCOORD, index: 0}
  ];

  for (var i = 0; i < standard_semantic_index_pairs.length; ++i) {
    var p = standard_semantic_index_pairs[i];
    r.push(new o3d.EffectStreamInfo(p.semantic, p.index));
  }
  return r;
};


/**
 * Searches the objects in the given list for parameters to apply to the
 * uniforms defined on this effects program, and applies them, favoring
 * the objects nearer the begining of the list.
 * 
 * @param {!Array.<!o3d.ParamObject>} object_list The param objects to search.
 */
o3d.Effect.prototype.searchForParams = function(object_list) {
  var filled_map = {};
  for (name in this.uniforms) {
    filled_map[name] = false;
  }
  this.gl.useProgram(this.program_);
  for (var i = 0; i < object_list.length; ++i) {
    var obj = object_list[i];
    for (name in this.uniforms) {
      var uniformInfo = this.uniforms[name];
      if (filled_map[name]) {
        continue;
      }
      var param = obj.getParam(name);
      if (param) {
        param.applyToLocation(this.gl, uniformInfo.location);
        filled_map[name] = true;
      }
    }
  }

  for (name in this.uniforms) {
    if (!filled_map[name]) {
      throw ('Uniform param not filled: '+name);
    }
  }
};


/**
 * @type {number}
 */
o3d.Effect.MatrixLoadOrder = goog.typedef;

/**
 *  MatrixLoadOrder,
 *  ROW_MAJOR,  Matrix parameters are loaded in row-major order (DX-style).
 *  COLUMN_MAJOR,   Matrix parameters are loaded in column-major order
 *     (OpenGL-style).
 */
o3d.Effect.ROW_MAJOR = 0;
o3d.Effect.COLUMN_MAJOR = 1;


/**
 * The order in which matrix data is loaded to the GPU.
 * @type {o3d.Effect.MatrixLoadOrder}
 */
o3d.Effect.prototype.matrix_load_order_ = o3d.Effect.ROW_MAJOR;


/**
 * The source for the shaders on this Effect.
 * @type {string}
 */
o3d.Effect.prototype.source_ = '';


