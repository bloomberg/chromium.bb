export const description = `
Tests for the general aspects of draw/drawIndexed/drawIndirect/drawIndexedIndirect.

Primitive topology tested in api/operation/render_pipeline/primitive_topology.spec.ts.
Index format tested in api/operation/command_buffer/render/state_tracking.spec.ts.

* arguments - Test that draw arguments are passed correctly.

TODO:
* default_arguments - Test defaults to draw / drawIndexed.
  - arg= {instance_count, first, first_instance, base_vertex}
  - mode= {draw, drawIndexed}
`;

import { params, pbool, poptions } from '../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { assert } from '../../../../common/framework/util/util.js';
import {
  GPUTest,
  TypedArrayBufferView,
  TypedArrayBufferViewConstructor,
} from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('arguments')
  .desc(
    `Test that draw arguments are passed correctly by drawing triangles in a grid.
Horizontally across the texture are triangles with increasing "primitive id".
Vertically down the screen are triangles with increasing instance id.
Increasing the |first| param should skip some of the beginning triangles on the horizontal axis.
Increasing the |first_instance| param should skip of the beginning triangles on the vertical axis.
The vertex buffer contains two sets of disjoint triangles, and base_vertex is used to select the second set.
The test checks that the center of all of the expected triangles is drawn, and the others are empty.
The fragment shader also writes out to a storage buffer. If the draw is zero-sized, check that no value is written.

Params:
  - first= {0, 3} - either the firstVertex or firstIndex
  - count= {0, 3, 6} - either the vertexCount or indexCount
  - first_instance= {0, 2}
  - instance_count= {0, 1, 4}
  - indexed= {true, false}
  - indirect= {true, false}
  - vertex_buffer_offset= {0, 32}
  - index_buffer_offset= {0, 16} - only for indexed draws
  - base_vertex= {0, 9} - only for indexed draws
  `
  )
  .cases(
    params()
      .combine(poptions('first', [0, 3] as const))
      .combine(poptions('count', [0, 3, 6] as const))
      .combine(poptions('first_instance', [0, 2] as const))
      .combine(poptions('instance_count', [0, 1, 4] as const))
      .combine(pbool('indexed'))
      .combine(pbool('indirect'))
      .combine(poptions('vertex_buffer_offset', [0, 32] as const))
      .expand(p => poptions('index_buffer_offset', p.indexed ? ([0, 16] as const) : [undefined]))
      .expand(p => poptions('base_vertex', p.indexed ? ([0, 9] as const) : [undefined]))
  )
  .fn(t => {
    const renderTargetSize = [72, 36];

    // The test will split up the render target into a grid where triangles of
    // increasing primitive id will be placed along the X axis, and triangles
    // of increasing instance id will be placed along the Y axis. The size of the
    // grid is based on the max primitive id and instance id used.
    const numX = 6;
    const numY = 6;
    const tileSizeX = renderTargetSize[0] / numX;
    const tileSizeY = renderTargetSize[1] / numY;

    // |\
    // |   \
    // |______\
    // Unit triangle shaped like this. 0-1 Y-down.
    const triangleVertices = /* prettier-ignore */ [
      0.0, 0.0,
      0.0, 1.0,
      1.0, 1.0,
    ];

    const renderTarget = t.device.createTexture({
      size: renderTargetSize,
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
      format: 'rgba8unorm',
    });

    const vertexModule = t.device.createShaderModule({
      code: `
struct Inputs {
  [[builtin(vertex_index)]] vertex_index : u32;
  [[builtin(instance_index)]] instance_id : u32;
  [[location(0)]] vertexPosition : vec2<f32>;
};

[[stage(vertex)]] fn vert_main(input : Inputs
  ) -> [[builtin(position)]] vec4<f32> {
  // 3u is the number of points in a triangle to convert from index
  // to id.
  var vertex_id : u32 = input.vertex_index / 3u;

  var x : f32 = (input.vertexPosition.x + f32(vertex_id)) / ${numX}.0;
  var y : f32 = (input.vertexPosition.y + f32(input.instance_id)) / ${numY}.0;

  // (0,1) y-down space to (-1,1) y-up NDC
  x = 2.0 * x - 1.0;
  y = -2.0 * y + 1.0;
  return vec4<f32>(x, y, 0.0, 1.0);
}
`,
    });

    const fragmentModule = t.device.createShaderModule({
      code: `
[[block]] struct Output {
  value : u32;
};

[[group(0), binding(0)]] var<storage> output : [[access(read_write)]] Output;

[[stage(fragment)]] fn frag_main() -> [[location(0)]] vec4<f32> {
  output.value = 1u;
  return vec4<f32>(0.0, 1.0, 0.0, 1.0);
}
`,
    });

    const pipeline = t.device.createRenderPipeline({
      vertex: {
        module: vertexModule,
        entryPoint: 'vert_main',
        buffers: [
          {
            attributes: [
              {
                shaderLocation: 0,
                format: 'float32x2',
                offset: 0,
              },
            ],
            arrayStride: 2 * Float32Array.BYTES_PER_ELEMENT,
          },
        ],
      },
      fragment: {
        module: fragmentModule,
        entryPoint: 'frag_main',
        targets: [
          {
            format: 'rgba8unorm',
          },
        ],
      },
    });

    const resultBuffer = t.device.createBuffer({
      size: Uint32Array.BYTES_PER_ELEMENT,
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
    });

    const resultBindGroup = t.device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [
        {
          binding: 0,
          resource: {
            buffer: resultBuffer,
          },
        },
      ],
    });

    const commandEncoder = t.device.createCommandEncoder();
    const renderPass = commandEncoder.beginRenderPass({
      colorAttachments: [
        {
          view: renderTarget.createView(),
          loadValue: [0, 0, 0, 0],
          storeOp: 'store',
        },
      ],
    });

    renderPass.setPipeline(pipeline);
    renderPass.setBindGroup(0, resultBindGroup);

    if (t.params.indexed) {
      // INDEXED DRAW
      assert(t.params.base_vertex !== undefined);
      assert(t.params.index_buffer_offset !== undefined);

      renderPass.setIndexBuffer(
        t.makeBufferWithContents(
          /* prettier-ignore */ new Uint32Array([
            // Offset the index buffer contents by empty data.
            ...new Array(t.params.index_buffer_offset / Uint32Array.BYTES_PER_ELEMENT),

            0,  1,  2, //
            3,  4,  5, //
            6,  7,  8, //
          ]),
          GPUBufferUsage.INDEX
        ),
        'uint32',
        t.params.index_buffer_offset
      );

      renderPass.setVertexBuffer(
        0,
        t.makeBufferWithContents(
          /* prettier-ignore */ new Float32Array([
            // Offset the vertex buffer contents by empty data.
            ...new Array(t.params.vertex_buffer_offset / Float32Array.BYTES_PER_ELEMENT),

            // selected with base_vertex=0
                                 // count=6
            ...triangleVertices, //   |   count=6;first=3
            ...triangleVertices, //   |       |
            ...triangleVertices, //           |

            // selected with base_vertex=9
                                 // count=6
            ...triangleVertices, //   |   count=6;first=3
            ...triangleVertices, //   |       |
            ...triangleVertices, //           |
          ]),
          GPUBufferUsage.VERTEX
        ),
        t.params.vertex_buffer_offset
      );

      const args = [
        t.params.count,
        t.params.instance_count,
        t.params.first,
        t.params.base_vertex,
        t.params.first_instance,
      ] as const;
      if (t.params.indirect) {
        renderPass.drawIndexedIndirect(
          t.makeBufferWithContents(new Uint32Array(args), GPUBufferUsage.INDIRECT),
          0
        );
      } else {
        renderPass.drawIndexed.apply(renderPass, [...args]);
      }
    } else {
      // NON-INDEXED DRAW
      renderPass.setVertexBuffer(
        0,
        t.makeBufferWithContents(
          /* prettier-ignore */ new Float32Array([
            // Offset the vertex buffer contents by empty data.
            ...new Array(t.params.vertex_buffer_offset / Float32Array.BYTES_PER_ELEMENT),

                                 // count=6
            ...triangleVertices, //   |   count=6;first=3
            ...triangleVertices, //   |       |
            ...triangleVertices, //           |
          ]),
          GPUBufferUsage.VERTEX
        ),
        t.params.vertex_buffer_offset
      );

      const args = [
        t.params.count,
        t.params.instance_count,
        t.params.first,
        t.params.first_instance,
      ] as const;
      if (t.params.indirect) {
        renderPass.drawIndirect(
          t.makeBufferWithContents(new Uint32Array(args), GPUBufferUsage.INDIRECT),
          0
        );
      } else {
        renderPass.draw.apply(renderPass, [...args]);
      }
    }

    renderPass.endPass();
    t.queue.submit([commandEncoder.finish()]);

    const green = new Uint8Array([0, 255, 0, 255]);
    const transparentBlack = new Uint8Array([0, 0, 0, 0]);

    const didDraw = t.params.count && t.params.instance_count;

    t.expectContents(resultBuffer, new Uint32Array([didDraw ? 1 : 0]));

    const baseVertex = t.params.base_vertex ?? 0;
    for (let primitiveId = 0; primitiveId < numX; ++primitiveId) {
      for (let instanceId = 0; instanceId < numY; ++instanceId) {
        let expectedColor = didDraw ? green : transparentBlack;
        if (
          primitiveId * 3 < t.params.first + baseVertex ||
          primitiveId * 3 >= t.params.first + baseVertex + t.params.count
        ) {
          expectedColor = transparentBlack;
        }

        if (
          instanceId < t.params.first_instance ||
          instanceId >= t.params.first_instance + t.params.instance_count
        ) {
          expectedColor = transparentBlack;
        }

        t.expectSinglePixelIn2DTexture(
          renderTarget,
          'rgba8unorm',
          {
            x: (1 / 3 + primitiveId) * tileSizeX,
            y: (2 / 3 + instanceId) * tileSizeY,
          },
          {
            exp: expectedColor,
          }
        );
      }
    }
  });

g.test('vertex_attributes,basic')
  .desc(
    `Test basic fetching of vertex attributes.
  Each vertex attribute is a single value and written out into a storage buffer.
  Tests that vertices with offsets/strides for instanced/non-instanced attributes are
  fetched correctly. Not all vertex formats are tested.

  Params:
  - vertex_attribute_count= {1, 4, 8, 16}
  - vertex_buffer_count={1, 4, 8} - where # attributes is > 0
  - vertex_format={uint32, float32}
  - step_mode= {undefined, vertex, instance, mixed} - where mixed only applies for vertex_buffer_count > 1
  `
  )
  .cases(
    params()
      .combine(poptions('vertex_attribute_count', [1, 4, 8, 16]))
      .combine(poptions('vertex_buffer_count', [1, 4, 8]))
      .combine(poptions('vertex_format', ['uint32', 'float32'] as const))
      .combine(poptions('step_mode', [undefined, 'vertex', 'instance', 'mixed'] as const))
      .unless(p => p.vertex_attribute_count < p.vertex_buffer_count)
      .unless(p => p.step_mode === 'mixed' && p.vertex_buffer_count <= 1)
  )
  .fn(t => {
    const vertexCount = 4;
    const instanceCount = 4;

    const attributesPerVertexBuffer =
      t.params.vertex_attribute_count / t.params.vertex_buffer_count;
    assert(Math.round(attributesPerVertexBuffer) === attributesPerVertexBuffer);

    let shaderLocation = 0;
    let attributeValue = 0;
    const bufferLayouts: GPUVertexBufferLayout[] = [];

    let ExpectedDataConstructor: TypedArrayBufferViewConstructor;
    switch (t.params.vertex_format) {
      case 'uint32':
        ExpectedDataConstructor = Uint32Array;
        break;
      case 'float32':
        ExpectedDataConstructor = Float32Array;
        break;
    }

    // Populate |bufferLayouts|, |vertexBufferData|, and |vertexBuffers|.
    // We will use this to both create the render pipeline, and produce the
    // expected data on the CPU.
    // Attributes in each buffer will be interleaved.
    const vertexBuffers: GPUBuffer[] = [];
    const vertexBufferData: TypedArrayBufferView[] = [];
    for (let b = 0; b < t.params.vertex_buffer_count; ++b) {
      const vertexBufferValues: number[] = [];

      let offset = 0;
      let stepMode = t.params.step_mode;

      // If stepMode is mixed, alternate between vertex and instance.
      if (stepMode === 'mixed') {
        stepMode = (['vertex', 'instance'] as const)[b % 2];
      }

      let vertexOrInstanceCount: number;
      switch (stepMode) {
        case undefined:
        case 'vertex':
          vertexOrInstanceCount = vertexCount;
          break;
        case 'instance':
          vertexOrInstanceCount = instanceCount;
          break;
      }

      const attributes: GPUVertexAttribute[] = [];
      for (let a = 0; a < attributesPerVertexBuffer; ++a) {
        const attribute: GPUVertexAttribute = {
          format: t.params.vertex_format,
          shaderLocation,
          offset,
        };
        attributes.push(attribute);

        offset += ExpectedDataConstructor.BYTES_PER_ELEMENT;
        shaderLocation += 1;
      }

      for (let v = 0; v < vertexOrInstanceCount; ++v) {
        for (let a = 0; a < attributesPerVertexBuffer; ++a) {
          vertexBufferValues.push(attributeValue);
          attributeValue += 1.234; // Values will get rounded later if we make a Uint32Array.
        }
      }

      bufferLayouts.push({
        attributes,
        arrayStride: offset,
        stepMode,
      });

      const data = new ExpectedDataConstructor(vertexBufferValues);
      vertexBufferData.push(data);
      vertexBuffers.push(t.makeBufferWithContents(data, GPUBufferUsage.VERTEX));
    }

    // Create an array of shader locations [0, 1, 2, 3, ...] for easy iteration.
    const shaderLocations = new Array(shaderLocation).fill(0).map((_, i) => i);

    // Create the expected data buffer.
    const expectedData = new ExpectedDataConstructor(
      vertexCount * instanceCount * shaderLocations.length
    );

    // Populate the expected data. This is a CPU-side version of what we expect the shader
    // to do.
    for (let vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
      for (let instanceIndex = 0; instanceIndex < instanceCount; ++instanceIndex) {
        bufferLayouts.forEach((bufferLayout, b) => {
          for (const attribute of bufferLayout.attributes) {
            const primitiveId = vertexCount * instanceIndex + vertexIndex;
            const outputIndex = primitiveId * shaderLocations.length + attribute.shaderLocation;

            let vertexOrInstanceIndex: number;
            switch (bufferLayout.stepMode) {
              case undefined:
              case 'vertex':
                vertexOrInstanceIndex = vertexIndex;
                break;
              case 'instance':
                vertexOrInstanceIndex = instanceIndex;
                break;
            }

            const view = new ExpectedDataConstructor(
              vertexBufferData[b].buffer,
              bufferLayout.arrayStride * vertexOrInstanceIndex + attribute.offset,
              1
            );
            expectedData[outputIndex] = view[0];
          }
        });
      }
    }

    let wgslFormat: string;
    switch (t.params.vertex_format) {
      case 'uint32':
        wgslFormat = 'u32';
        break;
      case 'float32':
        wgslFormat = 'f32';
        break;
    }

    const pipeline = t.device.createRenderPipeline({
      vertex: {
        module: t.device.createShaderModule({
          code: `
struct Inputs {
  [[builtin(vertex_index)]] vertexIndex : u32;
  [[builtin(instance_index)]] instanceIndex : u32;
${shaderLocations.map(i => `  [[location(${i})]] attrib${i} : ${wgslFormat};`).join('\n')}
};

struct Outputs {
  [[builtin(position)]] Position : vec4<f32>;
${shaderLocations.map(i => `  [[location(${i})]] outAttrib${i} : ${wgslFormat};`).join('\n')}
  [[location(${shaderLocations.length})]] primitiveId : u32;
};

[[stage(vertex)]] fn main(input : Inputs) -> Outputs {
  var output : Outputs;
${shaderLocations.map(i => `  output.outAttrib${i} = input.attrib${i};`).join('\n')}
  output.primitiveId = input.instanceIndex * ${instanceCount}u + input.vertexIndex;
  output.Position = vec4<f32>(0.0, 0.0, 0.5, 1.0);
  return output;
}
          `,
        }),
        entryPoint: 'main',
        buffers: bufferLayouts,
      },
      fragment: {
        module: t.device.createShaderModule({
          code: `
struct Inputs {
${shaderLocations.map(i => `  [[location(${i})]] attrib${i} : ${wgslFormat};`).join('\n')}
  [[location(${shaderLocations.length})]] primitiveId : u32;
};

struct OutPrimitive {
${shaderLocations.map(i => `  attrib${i} : ${wgslFormat};`).join('\n')}
};
[[block]] struct OutBuffer {
  primitives : [[stride(${shaderLocations.length * 4})]] array<OutPrimitive>;
};
[[group(0), binding(0)]] var<storage> outBuffer : [[access(read_write)]] OutBuffer;

[[stage(fragment)]] fn main(input : Inputs) {
${shaderLocations
  .map(i => `  outBuffer.primitives[input.primitiveId].attrib${i} = input.attrib${i};`)
  .join('\n')}
}
          `,
        }),
        entryPoint: 'main',
        targets: [
          {
            format: 'rgba8unorm',
          },
        ],
      },
      primitive: {
        topology: 'point-list',
      },
    });

    const resultBuffer = t.device.createBuffer({
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
      size: vertexCount * instanceCount * shaderLocations.length * 4,
    });

    const resultBindGroup = t.device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [
        {
          binding: 0,
          resource: {
            buffer: resultBuffer,
          },
        },
      ],
    });

    const commandEncoder = t.device.createCommandEncoder();
    const renderPass = commandEncoder.beginRenderPass({
      colorAttachments: [
        {
          // Dummy render attachment - not used.
          view: t.device
            .createTexture({
              usage: GPUTextureUsage.RENDER_ATTACHMENT,
              size: [1],
              format: 'rgba8unorm',
            })
            .createView(),
          loadValue: [0, 0, 0, 0],
          storeOp: 'store',
        },
      ],
    });

    renderPass.setPipeline(pipeline);
    renderPass.setBindGroup(0, resultBindGroup);
    for (let i = 0; i < t.params.vertex_buffer_count; ++i) {
      renderPass.setVertexBuffer(i, vertexBuffers[i]);
    }
    renderPass.draw(vertexCount, instanceCount);
    renderPass.endPass();
    t.device.queue.submit([commandEncoder.finish()]);

    t.expectContents(resultBuffer, expectedData);
  });

g.test('vertex_attributes,formats')
  .desc(
    `Test all vertex formats are fetched correctly.

    Runs a basic vertex shader which loads vertex data from two attributes which
    may have different formats. Write data out to a storage buffer and check that
    it was loaded correctly.

    Params:
      - vertex_format_1={...all_vertex_formats}
      - vertex_format_2={...all_vertex_formats}
  `
  )
  .unimplemented();
