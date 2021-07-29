export const description = `

TODO: check overlap with api,operation,rendering,draw:vertex_attributes,basic before implementing

- Tests that render N points, using a generated pipeline with:
  (1) a vertex shader that has necessary vertex inputs and a static array of
  expected data (as indexed by vertexID + instanceID * verticesPerInstance),
  which checks they're equal and sends the bool to the fragment shader;
  (2) a fragment shader which writes the result out to a storage buffer
  (or renders a red/green fragment if we can't do fragmentStoresAndAtomics,
  maybe with some depth or stencil test magic to do the '&&' of all fragments).
    - Fill some GPUBuffers with testable data, e.g.
      [[1.0, 2.0, ...], [-1.0, -2.0, ...]], for use as vertex buffers.
    - With no/trivial indexing
        - Either non-indexed, or indexed with a passthrough index buffer ([0, 1, 2, ...])
            - Of either format
            - If non-indexed, index format has no effect
        - Vertex data is read from the buffer correctly
            - Several vertex buffers with several attributes each
                - Two setVertexBuffers pointing at the same GPUBuffer (if possible)
                    - Overlapping, non-overlapping
                - Overlapping attributes (iff that's supposed to work)
                - Overlapping vertex buffer elements
                  (an attribute offset + its size > arrayStride)
                  (iff that's supposed to work)
                - Discontiguous vertex buffer slots, e.g.
                  [1, some large number (API doesn't practically allow huge numbers here)]
                - Discontiguous shader locations, e.g.
                  [2, some large number (max if possible)]
             - Bind everything possible up to limits
                 - Also with maxed out attributes?
             - x= all vertex formats
        - Maybe a test of one buffer with two attributes, with every possible
          pair of vertex formats
    - With indexing. For each index format:
        - Indices are read from the buffer correctly
            - setIndexBuffer offset
        - For each vertex format:
            - Basic test with several vertex buffers and several attributes

TODO: Test more corner case values for Float16 / Float32 (INF, NaN, +-0, ...) and reduce the
float tolerance.
`;

import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { unreachable } from '../../../../common/util/util.js';
import {
  kMaxVertexAttributes,
  kMaxVertexBufferArrayStride,
  kMaxVertexBuffers,
  kVertexFormatInfo,
  kVertexFormats,
} from '../../../capability_info.js';
import { GPUTest } from '../../../gpu_test.js';
import { float32ToFloat16Bits, normalizedIntegerAsFloat } from '../../../util/conversion.js';
import { align, clamp } from '../../../util/math.js';

// These types mirror the structure of GPUVertexBufferLayout but allow defining the extra
// dictionary members at the GPUVertexBufferLayout and GPUVertexAttribute level. The are used
// like so:
//
//   VertexState<{arrayStride: number}, {format: VertexFormat}>
//   VertexBuffer<{arrayStride: number}, {format: VertexFormat}>
//   VertexAttrib<{format: VertexFormat}>
type VertexAttrib<A> = A & { shaderLocation: number };
type VertexBuffer<V, A> = V & {
  slot: number;
  attributes: VertexAttrib<A>[];
};
type VertexState<V, A> = VertexBuffer<V, A>[];

type VertexLayoutState<V, A> = VertexState<
  { stepMode: GPUInputStepMode; arrayStride: number } & V,
  { format: GPUVertexFormat; offset: number } & A
>;

function mapBufferAttribs<V, A1, A2>(
  buffer: VertexBuffer<V, A1>,
  f: (v: V, a: VertexAttrib<A1>) => A2
): VertexBuffer<V, A2> {
  const newAttributes: VertexAttrib<A2>[] = [];
  for (const a of buffer.attributes) {
    newAttributes.push({
      shaderLocation: a.shaderLocation,
      ...f(buffer, a),
    });
  }

  return { ...buffer, attributes: newAttributes };
}

function mapStateAttribs<V, A1, A2>(
  buffers: VertexState<V, A1>,
  f: (v: V, a: VertexAttrib<A1>) => A2
): VertexState<V, A2> {
  return buffers.map(b => mapBufferAttribs(b, f));
}

type TestData = {
  shaderBaseType: string;
  floatTolerance?: number;
  // The number of vertex components in the vertexData (expectedData might contain more because
  // it is padded to 4 components).
  testComponentCount: number;
  // The data that will be in the uniform buffer and used to check the vertex inputs.
  expectedData: ArrayBuffer;
  // The data that will be in the vertex buffer.
  vertexData: ArrayBuffer;
};

class VertexStateTest extends GPUTest {
  // Generate for VS + FS (entrypoints vsMain / fsMain) that for each attribute will check that its
  // value corresponds to what's expected (as provided by a uniform buffer per attribute) and then
  // renders each vertex at position (vertexIndex, instanceindex) with either 1 (success) or
  // a negative number corresponding to the check number (in case you need to debug a failure).
  makeTestWGSL(
    buffers: VertexState<
      { stepMode: GPUInputStepMode },
      {
        format: GPUVertexFormat;
        shaderBaseType: string;
        shaderComponentCount?: number;
        floatTolerance?: number;
      }
    >,
    vertexCount: number,
    instanceCount: number
  ): string {
    let vsInputs = '';
    let vsChecks = '';
    let vsBindings = '';

    for (const b of buffers) {
      for (const a of b.attributes) {
        const format = kVertexFormatInfo[a.format];
        const shaderComponentCount = a.shaderComponentCount ?? format.componentCount;
        const i = a.shaderLocation;

        // shaderType is either a scalar type like f32 or a vecN<scalarType>
        let shaderType = a.shaderBaseType;
        if (shaderComponentCount !== 1) {
          shaderType = `vec${shaderComponentCount}<${shaderType}>`;
        }

        let maxCount = `${vertexCount}`;
        let indexBuiltin = `input.vertexIndex`;
        if (b.stepMode === 'instance') {
          maxCount = `${instanceCount}`;
          indexBuiltin = `input.instanceIndex`;
        }

        vsInputs += `  [[location(${i})]] attrib${i} : ${shaderType};\n`;
        vsBindings += `[[block]] struct S${i} { data : array<vec4<${a.shaderBaseType}>, ${maxCount}>; };\n`;
        vsBindings += `[[group(0), binding(${i})]] var<uniform> providedData${i} : S${i};\n`;

        // Generate the all the checks for the attributes.
        for (let component = 0; component < shaderComponentCount; component++) {
          // Components are filled with (0, 0, 0, 1) if they aren't provided data from the pipeline.
          if (component >= format.componentCount) {
            const expected = component === 3 ? '1' : '0';
            vsChecks += `  check(input.attrib${i}[${component}] == ${a.shaderBaseType}(${expected}));\n`;
            continue;
          }

          // Check each component individually, with special handling of tolerance for floats.
          const attribComponent =
            shaderComponentCount === 1 ? `input.attrib${i}` : `input.attrib${i}[${component}]`;
          const providedData = `providedData${i}.data[${indexBuiltin}][${component}]`;
          if (format.type === 'uint' || format.type === 'sint') {
            vsChecks += `  check(${attribComponent} == ${providedData});\n`;
          } else {
            vsChecks += `  check(floatsSimilar(${attribComponent}, ${providedData}, f32(${
              a.floatTolerance ?? 0
            })));\n`;
          }
        }
      }
    }

    return `
struct Inputs {
${vsInputs}
  [[builtin(vertex_index)]] vertexIndex: u32;
  [[builtin(instance_index)]] instanceIndex: u32;
};

${vsBindings}

var<private> vsResult : i32 = 1;
var<private> checkIndex : i32 = 0;
fn check(success : bool) {
  if (!success) {
    vsResult = -checkIndex;
  }
  checkIndex = checkIndex + 1;
}

fn floatsSimilar(a : f32, b : f32, tolerance : f32) -> bool {
  if (isNan(a) && isNan(b)) {
    return true;
  }

  if (isInf(a) && isInf(b) && sign(a) == sign(b)) {
    return true;
  }

  if (isInf(a) || isInf(b)) {
    return false;
  }

  // TODO do we check for + and - 0?
  return abs(a - b) < tolerance;
}

fn doTest(input : Inputs) {
${vsChecks}
}

struct VSOutputs {
  [[location(0)]] result : i32;
  [[builtin(position)]] position : vec4<f32>;
};

[[stage(vertex)]] fn vsMain(input : Inputs) -> VSOutputs {
  doTest(input);

  // Place that point at pixel (vertexIndex, instanceIndex) in a framebuffer of size
  // (vertexCount , instanceCount).
  var output : VSOutputs;
  output.position = vec4<f32>(
    ((f32(input.vertexIndex) + 0.5) / ${vertexCount}.0 * 2.0) - 1.0,
    ((f32(input.instanceIndex) + 0.5) / ${instanceCount}.0 * 2.0) - 1.0,
    0.0, 1.0
  );
  output.result = vsResult;
  return output;
}

[[stage(fragment)]] fn fsMain([[location(0)]] result : i32) -> [[location(0)]] i32 {
  return result;
}
    `;
  }

  makeTestPipeline(
    buffers: VertexState<
      { stepMode: GPUInputStepMode; arrayStride: number },
      {
        offset: number;
        format: GPUVertexFormat;
        shaderBaseType: string;
        shaderComponentCount?: number;
        floatTolerance?: number;
      }
    >,
    vertexCount: number,
    instanceCount: number
  ): GPURenderPipeline {
    const module = this.device.createShaderModule({
      code: this.makeTestWGSL(buffers, vertexCount, instanceCount),
    });

    const bufferLayouts: GPUVertexBufferLayout[] = [];
    for (const b of buffers) {
      bufferLayouts[b.slot] = b;
    }

    return this.device.createRenderPipeline({
      vertex: {
        module,
        entryPoint: 'vsMain',
        buffers: bufferLayouts,
      },
      primitive: {
        topology: 'point-list',
      },
      fragment: {
        module,
        entryPoint: 'fsMain',
        targets: [
          {
            format: 'r32sint',
          },
        ],
      },
    });
  }

  // Runs the render pass drawing points in a vertexCount*instanceCount rectangle, then check each
  // of produced a value of 1 which means that the tests in the shader passed.
  submitRenderPass(
    pipeline: GPURenderPipeline,
    buffers: VertexState<{ buffer: GPUBuffer; vbOffset?: number }, {}>,
    expectedData: GPUBindGroup,
    vertexCount: number,
    instanceCount: number
  ) {
    const testTexture = this.device.createTexture({
      format: 'r32sint',
      size: [vertexCount, instanceCount],
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
    });

    const encoder = this.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: testTexture.createView(),
          loadValue: [0, 0, 0, 0],
          storeOp: 'store',
        },
      ],
    });

    pass.setPipeline(pipeline);
    pass.setBindGroup(0, expectedData);
    for (const buffer of buffers) {
      pass.setVertexBuffer(buffer.slot, buffer.buffer, buffer.vbOffset ?? 0);
    }
    pass.draw(vertexCount, instanceCount);
    pass.endPass();

    this.device.queue.submit([encoder.finish()]);

    this.expectSingleColor(testTexture, 'r32sint', {
      size: [vertexCount, instanceCount, 1],
      exp: { R: 1 },
    });
  }

  // Generate TestData for the format with interesting test values.
  // TODO cache the result on the fixture?
  generateTestData(format: GPUVertexFormat): TestData {
    const formatInfo = kVertexFormatInfo[format];
    const bitSize = formatInfo.bytesPerComponent * 8;

    switch (formatInfo.type) {
      case 'float': {
        const data = [0.0, 1.0, -1.0, 1000, 42.42, -18.7, 25.17];
        const expectedData = new Float32Array(data).buffer;
        const vertexData =
          bitSize === 32
            ? expectedData
            : bitSize === 16
            ? new Uint16Array(data.map(float32ToFloat16Bits)).buffer
            : unreachable();

        return {
          shaderBaseType: 'f32',
          testComponentCount: data.length,
          expectedData,
          vertexData,
          floatTolerance: 0.05,
        };
      }

      case 'sint': {
        /* prettier-ignore */
        const data = [
          0, 1, 2, 3, 4, 5,
          -1, -2, -3, -4, -5,
          Math.pow(2, bitSize - 2),
          Math.pow(2, bitSize - 1) - 1, // max value
          -Math.pow(2, bitSize - 2),
          -Math.pow(2, bitSize - 1), // min value
        ];
        const expectedData = new Int32Array(data).buffer;
        const vertexData =
          bitSize === 32
            ? expectedData
            : bitSize === 16
            ? new Int16Array(data).buffer
            : new Int8Array(data).buffer;

        return {
          shaderBaseType: 'i32',
          testComponentCount: data.length,
          expectedData,
          vertexData,
        };
      }

      case 'uint': {
        /* prettier-ignore */
        const data = [
          0, 1, 2, 3, 4, 5,
          Math.pow(2, bitSize - 1),
          Math.pow(2, bitSize) - 1, // max value
        ];
        const expectedData = new Uint32Array(data).buffer;
        const vertexData =
          bitSize === 32
            ? expectedData
            : bitSize === 16
            ? new Uint16Array(data).buffer
            : new Uint8Array(data).buffer;

        return {
          shaderBaseType: 'u32',
          testComponentCount: data.length,
          expectedData,
          vertexData,
        };
      }

      case 'snorm': {
        /* prettier-ignore */
        const data = [
          0, 1, 2, 3, 4, 5,
          -1, -2, -3, -4, -5,
          Math.pow(2,bitSize - 2),
          Math.pow(2,bitSize - 1) - 1, // max value
          -Math.pow(2,bitSize - 2),
          -Math.pow(2,bitSize - 1), // min value
        ];
        const vertexData =
          bitSize === 16
            ? new Int16Array(data).buffer
            : bitSize === 8
            ? new Int8Array(data).buffer
            : unreachable();

        return {
          shaderBaseType: 'f32',
          testComponentCount: data.length,
          expectedData: new Float32Array(data.map(v => normalizedIntegerAsFloat(v, bitSize, true)))
            .buffer,
          vertexData,
          floatTolerance: 0.1 * normalizedIntegerAsFloat(1, bitSize, true),
        };
      }

      case 'unorm': {
        /* prettier-ignore */
        const data = [
          0, 1, 2, 3, 4, 5,
          Math.pow(2, bitSize - 1),
          Math.pow(2, bitSize) - 1, // max value
        ];
        const vertexData =
          bitSize === 16
            ? new Uint16Array(data).buffer
            : bitSize === 8
            ? new Uint8Array(data).buffer
            : unreachable();

        return {
          shaderBaseType: 'f32',
          testComponentCount: data.length,
          expectedData: new Float32Array(data.map(v => normalizedIntegerAsFloat(v, bitSize, false)))
            .buffer,
          vertexData: vertexData!,
          floatTolerance: 0.1 * normalizedIntegerAsFloat(1, bitSize, false),
        };
      }
    }
  }

  // The TestData generated for a format might not contain enough data for all the vertices we are
  // going to draw, so we expand them by adding additional copies of the vertexData as needed.
  // expectedData is a bit different because it also needs to be unpacked to have `componentCount`
  // components every 4 components (because the shader uses vec4 for the expected data).
  expandTestData(data: TestData, maxCount: number, componentCount: number): TestData {
    const vertexComponentSize = data.vertexData.byteLength / data.testComponentCount;
    const expectedComponentSize = data.expectedData.byteLength / data.testComponentCount;

    const expandedVertexData = new Uint8Array(maxCount * componentCount * vertexComponentSize);
    const expandedExpectedData = new Uint8Array(4 * maxCount * expectedComponentSize);

    for (let index = 0; index < maxCount; index++) {
      for (let component = 0; component < componentCount; component++) {
        // If only we had some builtin JS memcpy function between ArrayBuffers...
        const targetVertexOffset = (index * componentCount + component) * vertexComponentSize;
        const sourceVertexOffset = targetVertexOffset % data.vertexData.byteLength;
        expandedVertexData.set(
          new Uint8Array(data.vertexData, sourceVertexOffset, vertexComponentSize),
          targetVertexOffset
        );

        const targetExpectedOffset = (index * 4 + component) * expectedComponentSize;
        const sourceExpectedOffset =
          ((index * componentCount + component) * expectedComponentSize) %
          data.expectedData.byteLength;
        expandedExpectedData.set(
          new Uint8Array(data.expectedData, sourceExpectedOffset, expectedComponentSize),
          targetExpectedOffset
        );
      }
    }

    return {
      shaderBaseType: data.shaderBaseType,
      testComponentCount: maxCount * componentCount,
      floatTolerance: data.floatTolerance,
      expectedData: expandedExpectedData.buffer,
      vertexData: expandedVertexData.buffer,
    };
  }

  // Copies `size` bytes from `source` to `target` starting at `offset` each `targetStride`.
  // (the data in `source` is assumed packed)
  interleaveVertexDataInto(
    target: ArrayBuffer,
    source: ArrayBuffer,
    { targetStride, offset, size }: { targetStride: number; offset: number; size: number }
  ) {
    const t = new Uint8Array(target);
    for (
      let sourceOffset = 0, targetOffset = offset;
      sourceOffset < source.byteLength;
      sourceOffset += size, targetOffset += targetStride
    ) {
      const a = new Uint8Array(source, sourceOffset, size);
      t.set(a, targetOffset);
    }
  }

  createPipelineAndTestData<V, A>(
    state: VertexLayoutState<V, A>,
    vertexCount: number,
    instanceCount: number
  ): {
    pipeline: GPURenderPipeline;
    testData: VertexLayoutState<V, A & TestData>;
  } {
    // Gather the test data and some additional test state for attribs.
    const pipelineAndTestState = mapStateAttribs(state, (buffer, attrib) => {
      const maxCount = buffer.stepMode === 'instance' ? instanceCount : vertexCount;
      const formatInfo = kVertexFormatInfo[attrib.format];

      let testData = this.generateTestData(attrib.format);
      // TODO this will not work for arrayStride 0
      testData = this.expandTestData(testData, maxCount, formatInfo.componentCount);

      return {
        ...testData,
        ...attrib,
      };
    });

    // Create the pipeline from the test data.
    return {
      testData: pipelineAndTestState,
      pipeline: this.makeTestPipeline(pipelineAndTestState, vertexCount, instanceCount),
    };
  }

  createExpectedBG(state: VertexState<{}, TestData>, pipeline: GPURenderPipeline): GPUBindGroup {
    // Create the bindgroups from that test data
    const bgEntries: GPUBindGroupEntry[] = [];

    for (const buffer of state) {
      for (const attrib of buffer.attributes) {
        const expectedDataBuffer = this.makeBufferWithContents(
          new Uint8Array(attrib.expectedData),
          GPUBufferUsage.UNIFORM
        );
        bgEntries.push({
          binding: attrib.shaderLocation,
          resource: { buffer: expectedDataBuffer },
        });
      }
    }

    return this.device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: bgEntries,
    });
  }

  createVertexBuffers(
    state: VertexLayoutState<{ vbOffset?: number }, TestData>,
    vertexCount: number,
    instanceCount: number
  ): VertexState<{ buffer: GPUBuffer; vbOffset?: number }, {}> {
    // Create the vertex buffers
    const vertexBuffers: VertexState<{ buffer: GPUBuffer; vbOffset?: number }, {}> = [];

    for (const buffer of state) {
      const maxCount = buffer.stepMode === 'instance' ? instanceCount : vertexCount;

      // Fill the vertex data with garbage so that we don't get `0` (which could be a test value)
      // if the vertex shader loads the vertex data incorrectly.
      const vertexData = new ArrayBuffer(buffer.arrayStride * maxCount + (buffer.vbOffset ?? 0));
      new Uint8Array(vertexData).fill(0xc4);

      for (const attrib of buffer.attributes) {
        const formatInfo = kVertexFormatInfo[attrib.format];
        this.interleaveVertexDataInto(vertexData, attrib.vertexData, {
          targetStride: buffer.arrayStride,
          offset: (buffer.vbOffset ?? 0) + attrib.offset,
          size: formatInfo.componentCount * formatInfo.bytesPerComponent,
        });
      }

      vertexBuffers.push({
        slot: buffer.slot,
        buffer: this.makeBufferWithContents(new Uint8Array(vertexData), GPUBufferUsage.VERTEX),
        vbOffset: buffer.vbOffset,
        attributes: [],
      });
    }

    return vertexBuffers;
  }

  runTest(
    buffers: VertexLayoutState<{ vbOffset?: number }, { shaderComponentCount?: number }>,
    // Default to using 20 vertices and 20 instances so that we cover each of the test data at least
    // once (at the time of writing the largest testData has 16 values).
    vertexCount: number = 20,
    instanceCount: number = 20
  ) {
    const { testData, pipeline } = this.createPipelineAndTestData(
      buffers,
      vertexCount,
      instanceCount
    );
    const expectedDataBG = this.createExpectedBG(testData, pipeline);
    const vertexBuffers = this.createVertexBuffers(testData, vertexCount, instanceCount);
    this.submitRenderPass(pipeline, vertexBuffers, expectedDataBG, vertexCount, instanceCount);
  }
}

export const g = makeTestGroup(VertexStateTest);

g.test('vertexFormat_to_shaderFormat_conversion')
  .desc(
    `Test that the raw data passed in vertex buffers is correctly converted to the input type in the shader. Test for:
  - all formats
  - 1 to 4 components in the shader's input type (unused components are filled with 0 and except the 4th with 1)
  - various locations
  - various slots`
  )
  .params(u =>
    u //
      .combine('format', kVertexFormats)
      .combine('shaderComponentCount', [1, 2, 3, 4])
      .beginSubcases()
      .combine('slot', [0, 1, kMaxVertexBuffers - 1])
      .combine('shaderLocation', [0, 1, kMaxVertexAttributes - 1])
  )
  .fn(t => {
    const { format, shaderComponentCount, slot, shaderLocation } = t.params;
    t.runTest([
      {
        slot,
        arrayStride: 16,
        stepMode: 'vertex',
        attributes: [
          {
            shaderLocation,
            format,
            offset: 0,
            shaderComponentCount,
          },
        ],
      },
    ]);
  });

g.test('setVertexBufferOffset_and_attributeOffset')
  .desc(
    `Test that the vertex buffer offset and attribute offset in the vertex state are applied correctly. Test for:
  - all formats
  - various setVertexBuffer offsets
  - various attribute offsets in a fixed arrayStride`
  )
  .params(u =>
    u //
      .combine('format', kVertexFormats)
      .beginSubcases()
      .combine('vbOffset', [0, 4, 400, 1004])
      .combine('arrayStride', [128])
      .expand('offset', p => {
        const formatInfo = kVertexFormatInfo[p.format];
        const componentSize = formatInfo.bytesPerComponent;
        const formatSize = componentSize * formatInfo.componentCount;
        return [
          0,
          componentSize,
          componentSize * 2,
          componentSize * 3,
          p.arrayStride / 2,
          p.arrayStride - formatSize - componentSize * 3,
          p.arrayStride - formatSize - componentSize * 2,
          p.arrayStride - formatSize - componentSize,
          p.arrayStride - formatSize,
        ];
      })
  )
  .fn(t => {
    const { format, vbOffset, arrayStride, offset } = t.params;
    t.runTest([
      {
        slot: 0,
        arrayStride,
        stepMode: 'vertex',
        vbOffset,
        attributes: [
          {
            shaderLocation: 0,
            format,
            offset,
          },
        ],
      },
    ]);
  });

g.test('nonZeroArrayStride_and_attributeOffset')
  .desc(
    `Test that the array stride and attribute offset in the vertex state are applied correctly. Test for:
  - all formats
  - various array strides
  - various attribute offsets in a fixed arrayStride`
  )
  .params(u =>
    u //
      .combine('format', kVertexFormats)
      .beginSubcases()
      .expand('arrayStride', p => {
        const formatInfo = kVertexFormatInfo[p.format];
        const componentSize = formatInfo.bytesPerComponent;
        const formatSize = componentSize * formatInfo.componentCount;

        return [align(formatSize, 4), align(formatSize, 4) + 4, kMaxVertexBufferArrayStride];
      })
      .expand('offset', p => {
        const formatInfo = kVertexFormatInfo[p.format];
        const componentSize = formatInfo.bytesPerComponent;
        const formatSize = componentSize * formatInfo.componentCount;
        return new Set(
          [
            0,
            componentSize,
            p.arrayStride / 2,
            p.arrayStride - formatSize - componentSize,
            p.arrayStride - formatSize,
          ].map(offset => clamp(offset, { min: 0, max: p.arrayStride - formatSize }))
        );
      })
  )
  .fn(t => {
    const { format, arrayStride, offset } = t.params;
    t.runTest([
      {
        slot: 0,
        arrayStride,
        stepMode: 'vertex',
        attributes: [
          {
            shaderLocation: 0,
            format,
            offset,
          },
        ],
      },
    ]);
  });

g.test('buffersWithVaryingStepMode')
  .desc(
    `Test buffers with varying step modes in the same vertex state.
  - Various combination of step modes`
  )
  .paramsSubcasesOnly(u =>
    u //
      .combine('stepModes', [
        ['instance'],
        ['vertex', 'vertex', 'instance'],
        ['instance', 'vertex', 'instance'],
        ['vertex', 'instance', 'vertex', 'vertex'],
      ])
  )
  .fn(t => {
    const { stepModes } = t.params;
    const state = (stepModes as GPUInputStepMode[]).map((stepMode, i) => ({
      slot: i,
      arrayStride: 4,
      stepMode,
      attributes: [
        {
          shaderLocation: i,
          format: 'float32' as const,
          offset: 0,
        },
      ],
    }));
    t.runTest(state);
  });
