// Copyright 2018 The Immersive Web Community Group
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

import {Material, RENDER_ORDER} from '../core/material.js';
import {Node} from '../core/node.js';
import {GeometryBuilderBase} from '../geometry/primitive-stream.js';

const GL = WebGLRenderingContext; // For enums

class PlaneMaterial extends Material {
  constructor(options = {}) {
    super();

    this.baseColor = this.defineUniform('baseColor', options.baseColor);

    this.renderOrder = RENDER_ORDER.TRANSPARENT;
    this.state.blend = true;
    this.state.blendFuncSrc = GL.ONE;
    this.state.blendFuncDst = GL.ONE_MINUS_SRC_ALPHA;
    this.state.depthFunc = GL.LEQUAL;
    this.state.depthMask = false;
    this.state.cullFace = false;
  }

  get materialName() {
    return 'PLANE';
  }

  get vertexSource() {
    return `
    attribute vec3 POSITION;
    attribute vec3 NORMAL;

    varying vec3 vLight;

    const vec3 lightDir = vec3(0.75, 0.5, 1.0);
    const vec3 ambientColor = vec3(0.5, 0.5, 0.5);
    const vec3 lightColor = vec3(0.75, 0.75, 0.75);

    vec4 vertex_main(mat4 proj, mat4 view, mat4 model) {
      vec3 normalRotated = vec3(model * vec4(NORMAL, 0.0));
      float lightFactor = max(dot(normalize(lightDir), normalRotated), 0.0);
      vLight = ambientColor + (lightColor * lightFactor);
      return proj * view * model * vec4(POSITION, 1.0);
    }`;
  }

  get fragmentSource() {
      return `
      precision mediump float;

      uniform vec4 baseColor;

      varying vec3 vLight;

      vec4 fragment_main() {
        return vec4(vLight, 1.0) * baseColor;
      }`;
  }
}

export class PlaneNode extends Node {
  constructor(options = {}) {
    super();
    if(!options.polygon)
      throw new Error(`Plane polygon must be specified.`);

    if(!options.baseColor)
      throw new Error(`Plane base color must be specified.`);

    this.baseColor = options.baseColor;
    this.polygon = options.polygon;

    this._material = new PlaneMaterial({baseColor : options.baseColor});

    this._renderer = null;
  }

  createPlanePrimitive(polygon) {
    // TODO: create new builder class for planes
    let planeBuilder = new GeometryBuilderBase();

    planeBuilder.primitiveStream.startGeometry();
    let numVertices = polygon.length;
    let firstVertex = planeBuilder.primitiveStream.nextVertexIndex;
    polygon.forEach(vertex => {
      planeBuilder.primitiveStream.pushVertex(vertex.x, vertex.y, vertex.z);
    });

    for(let i = 0; i < numVertices - 2; i++) {
      planeBuilder.primitiveStream.pushTriangle(firstVertex, firstVertex + i + 1, firstVertex + i + 2);
    }
    planeBuilder.primitiveStream.endGeometry();

    return planeBuilder.finishPrimitive(this._renderer);
  }

  onRendererChanged(renderer) {
    if(!this.polygon)
      throw new Error(`Polygon is not set on a plane where it should be!`);

    this._renderer = renderer;

    this.planeNode = this._renderer.createRenderPrimitive(
      this.createPlanePrimitive(this.polygon), this._material);
    this.addRenderPrimitive(this.planeNode);

    this.polygon = null;

    return this.waitForComplete();
  }

  onPlaneChanged(polygon) {
    if(this.polygon)
      throw new Error(`Polygon is set on a plane where it shouldn't be!`);

    let updatedPrimitive = this.createPlanePrimitive(polygon);

    this.planeNode.setPrimitive(updatedPrimitive);
    return this.planeNode.waitForComplete();
  }
}

