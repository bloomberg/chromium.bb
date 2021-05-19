export const description = `vertexState validation tests.`;

import { params, pbool, poptions } from '../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../common/framework/test_group.js';
import {
  kMaxVertexAttributes,
  kMaxVertexBufferArrayStride,
  kMaxVertexBuffers,
  kVertexFormats,
  kVertexFormatInfo,
} from '../../capability_info.js';

import { ValidationTest } from './validation_test.js';

const VERTEX_SHADER_CODE_WITH_NO_INPUT = `
  [[builtin(position)]] var<out> Position : vec4<f32>;
  [[stage(vertex)]] fn main() -> void {
    Position = vec4<f32>(0.0, 0.0, 0.0, 0.0);
  }
`;

function addTestAttributes(
  attributes: GPUVertexAttribute[],
  {
    testAttribute,
    testAttributeAtStart = true,
    extraAttributeCount = 0,
    extraAttributeSkippedLocations = [],
  }: {
    testAttribute?: GPUVertexAttribute;
    testAttributeAtStart?: boolean;
    extraAttributeCount?: Number;
    extraAttributeSkippedLocations?: Number[];
  }
) {
  // Add a bunch of dummy attributes each with a different location such that none of the locations
  // are in extraAttributeSkippedLocations
  let currentLocation = 0;
  let extraAttribsAdded = 0;
  while (extraAttribsAdded !== extraAttributeCount) {
    if (extraAttributeSkippedLocations.includes(currentLocation)) {
      currentLocation++;
      continue;
    }

    attributes.push({ format: 'float32', shaderLocation: currentLocation, offset: 0 });
    currentLocation++;
    extraAttribsAdded++;
  }

  // Add the test attribute at the start or the end of the attributes.
  if (testAttribute) {
    if (testAttributeAtStart) {
      attributes.unshift(testAttribute);
    } else {
      attributes.push(testAttribute);
    }
  }
}

class F extends ValidationTest {
  getDescriptor(
    buffers: Iterable<GPUVertexBufferLayout>,
    vertexShaderCode: string
  ): GPURenderPipelineDescriptor {
    const descriptor: GPURenderPipelineDescriptor = {
      vertex: {
        module: this.device.createShaderModule({ code: vertexShaderCode }),
        entryPoint: 'main',
        buffers,
      },
      fragment: {
        module: this.device.createShaderModule({
          code: `
            [[location(0)]] var<out> fragColor : vec4<f32>;
            [[stage(fragment)]] fn main() -> void {
              fragColor = vec4<f32>(0.0, 1.0, 0.0, 1.0);
              return;
            }`,
        }),
        entryPoint: 'main',
        targets: [{ format: 'rgba8unorm' }],
      },
      primitive: { topology: 'triangle-list' },
    };
    return descriptor;
  }

  testVertexState(
    success: boolean,
    buffers: Iterable<GPUVertexBufferLayout>,
    vertexShader: string = VERTEX_SHADER_CODE_WITH_NO_INPUT
  ) {
    const vsModule = this.device.createShaderModule({ code: vertexShader });
    const fsModule = this.device.createShaderModule({
      code: `
        [[location(0)]] var<out> fragColor : vec4<f32>;
        [[stage(fragment)]] fn main() -> void {
          fragColor = vec4<f32>(0.0, 1.0, 0.0, 1.0);
        }`,
    });

    this.expectValidationError(() => {
      this.device.createRenderPipeline({
        vertex: {
          module: vsModule,
          entryPoint: 'main',
          buffers,
        },
        fragment: {
          module: fsModule,
          entryPoint: 'main',
          targets: [{ format: 'rgba8unorm' }],
        },
        primitive: { topology: 'triangle-list' },
      });
    }, !success);
  }

  generateTestVertexShader(inputs: { type: string; location: number }[]): string {
    let interfaces = '';
    let body = '';

    let count = 0;
    for (const input of inputs) {
      interfaces += `[[location(${input.location})]] var<in> input${count} : ${input.type};\n`;
      body += `var i${count} : ${input.type} = input${count};\n`;
      count++;
    }

    return `
      [[builtin(position)]] var<out> Position : vec4<f32>;
      ${interfaces}
      [[stage(vertex)]] fn main() -> void {
        Position = vec4<f32>(0.0, 0.0, 0.0, 0.0);
        ${body}
      }
    `;
  }
}

export const g = makeTestGroup(F);

g.test('max_vertex_buffer_limit')
  .desc(
    `Test that only up to <maxVertexBuffers> vertex buffers are allowed.
   - Tests with 0, 1, limits, limits + 1 vertex buffers.
   - Tests with the last buffer having an attribute or not.
  This also happens to test that vertex buffers with no attributes are allowed and that a vertex state with no buffers is allowed.`
  )
  .subcases(() =>
    params()
      .combine(poptions('count', [0, 1, kMaxVertexBuffers, kMaxVertexBuffers + 1]))
      .combine(pbool('lastEmpty'))
  )
  .fn(t => {
    const { count, lastEmpty } = t.params;

    const vertexBuffers = [];
    for (let i = 0; i < count; i++) {
      if (lastEmpty || i !== count - 1) {
        vertexBuffers.push({ attributes: [], arrayStride: 0 });
      } else {
        vertexBuffers.push({
          attributes: [{ format: 'float32', offset: 0, shaderLocation: 0 }],
          arrayStride: 0,
        } as const);
      }
    }

    const success = count <= kMaxVertexBuffers;
    t.testVertexState(success, vertexBuffers);
  });

g.test('max_vertex_attribute_limit')
  .desc(
    `Test that only up to <maxVertexAttributes> vertex attributes are allowed.
   - Tests with 0, 1, limit, limits + 1 vertex attribute.
   - Tests with 0, 1, 4 attributes per buffer (with remaining attributes in the last buffer).`
  )
  .subcases(() =>
    params()
      .combine(poptions('attribCount', [0, 1, kMaxVertexAttributes, kMaxVertexAttributes + 1]))
      .combine(poptions('attribsPerBuffer', [0, 1, 4]))
  )
  .fn(t => {
    const { attribCount, attribsPerBuffer } = t.params;

    const vertexBuffers = [];

    let attribsAdded = 0;
    while (attribsAdded !== attribCount) {
      // Choose how many attributes to add for this buffer. The last buffer gets all remaining attributes.
      let targetCount = Math.min(attribCount, attribsAdded + attribsPerBuffer);
      if (vertexBuffers.length === kMaxVertexBuffers - 1) {
        targetCount = attribCount;
      }

      const attributes = [];
      while (attribsAdded !== targetCount) {
        attributes.push({ format: 'float32', offset: 0, shaderLocation: attribsAdded } as const);
        attribsAdded++;
      }

      vertexBuffers.push({ arrayStride: 0, attributes });
    }

    const success = attribCount <= kMaxVertexAttributes;
    t.testVertexState(success, vertexBuffers);
  });

g.test('max_vertex_buffer_array_stride_limit')
  .desc(
    `Test that the vertex buffer arrayStride must be at most <maxVertexBufferArrayStride>.
   - Test for various vertex buffer indices
   - Test for array strides 0, 4, 256, limit - 4, limit, limit + 4`
  )
  .subcases(() =>
    params()
      .combine(poptions('vertexBufferIndex', [0, 1, kMaxVertexBuffers - 1]))
      .combine(
        poptions('arrayStride', [
          0,
          4,
          256,
          kMaxVertexBufferArrayStride - 4,
          kMaxVertexBufferArrayStride,
          kMaxVertexBufferArrayStride + 4,
        ])
      )
  )
  .fn(t => {
    const { vertexBufferIndex, arrayStride } = t.params;

    const vertexBuffers = [];
    vertexBuffers[vertexBufferIndex] = { arrayStride, attributes: [] };

    const success = arrayStride <= kMaxVertexBufferArrayStride;
    t.testVertexState(success, vertexBuffers);
  });

g.test('vertex_buffer_array_stride_limit_alignment')
  .desc(
    `Test that the vertex buffer arrayStride must be a multiple of 4 (including 0).
   - Test for various vertex buffer indices
   - Test for array strides 0, 1, 2, 4, limit - 4, limit - 2, limit`
  )
  .subcases(() =>
    params()
      .combine(poptions('vertexBufferIndex', [0, 1, kMaxVertexBuffers - 1]))
      .combine(
        poptions('arrayStride', [
          0,
          1,
          2,
          4,
          kMaxVertexBufferArrayStride - 4,
          kMaxVertexBufferArrayStride - 2,
          kMaxVertexBufferArrayStride,
        ])
      )
  )
  .fn(t => {
    const { vertexBufferIndex, arrayStride } = t.params;

    const vertexBuffers = [];
    vertexBuffers[vertexBufferIndex] = { arrayStride, attributes: [] };

    const success = arrayStride % 4 === 0;
    t.testVertexState(success, vertexBuffers);
  });

g.test('vertex_attribute_shaderLocation_limit')
  .desc(
    `Test shaderLocation must be less than maxVertexAttributes.
   - Test for various vertex buffer indices
   - Test for various amounts of attributes in that vertex buffer
   - Test for shaderLocation 0, 1, limit - 1, limit`
  )
  .subcases(() =>
    params()
      .combine(poptions('vertexBufferIndex', [0, 1, kMaxVertexBuffers - 1]))
      .combine(poptions('extraAttributeCount', [0, 1, kMaxVertexAttributes - 1]))
      .combine(pbool('testAttributeAtStart'))
      .combine(
        poptions('testShaderLocation', [0, 1, kMaxVertexAttributes - 1, kMaxVertexAttributes])
      )
  )
  .fn(t => {
    const {
      vertexBufferIndex,
      extraAttributeCount,
      testShaderLocation,
      testAttributeAtStart,
    } = t.params;

    const attributes: GPUVertexAttribute[] = [];
    addTestAttributes(attributes, {
      testAttribute: { format: 'float32', offset: 0, shaderLocation: testShaderLocation },
      testAttributeAtStart,
      extraAttributeCount,
      extraAttributeSkippedLocations: [testShaderLocation],
    });

    const vertexBuffers = [];
    vertexBuffers[vertexBufferIndex] = { arrayStride: 256, attributes };

    const success = testShaderLocation < kMaxVertexAttributes;
    t.testVertexState(success, vertexBuffers);
  });

g.test('vertex_attribute_shaderLocation_unique')
  .desc(
    `Test that shaderLocation must be unique in the vertex state.
   - Test for various pairs of buffers that contain the potentially conflicting attributes
   - Test for the potentially conflicting attributes in various places in the buffers (with dummy attributes)
   - Test for various shaderLocations that conflict or not`
  )
  .subcases(() =>
    params()
      .combine(poptions('vertexBufferIndexA', [0, 1, kMaxVertexBuffers - 1]))
      .combine(poptions('vertexBufferIndexB', [0, 1, kMaxVertexBuffers - 1]))
      .combine(pbool('testAttributeAtStartA'))
      .combine(pbool('testAttributeAtStartB'))
      .combine(poptions('shaderLocationA', [0, 1, 7, kMaxVertexAttributes - 1]))
      .combine(poptions('shaderLocationB', [0, 1, 7, kMaxVertexAttributes - 1]))
      .combine(poptions('extraAttributeCount', [0, 4]))
  )
  .fn(t => {
    const {
      vertexBufferIndexA,
      vertexBufferIndexB,
      testAttributeAtStartA,
      testAttributeAtStartB,
      shaderLocationA,
      shaderLocationB,
      extraAttributeCount,
    } = t.params;

    // Depending on the params, the vertexBuffer for A and B can be the same or different. To support
    // both cases without code changes we treat `vertexBufferAttributes` as a map from indices to
    // vertex buffer descriptors, with A and B potentially reusing the same JS object if they have the
    // same index.
    const vertexBufferAttributes = [];
    vertexBufferAttributes[vertexBufferIndexA] = [];
    vertexBufferAttributes[vertexBufferIndexB] = [];

    // Add the dummy attributes for attribute A
    const attributesA = vertexBufferAttributes[vertexBufferIndexA];
    addTestAttributes(attributesA, {
      testAttribute: { format: 'float32', offset: 0, shaderLocation: shaderLocationA },
      testAttributeAtStart: testAttributeAtStartA,
      extraAttributeCount,
      extraAttributeSkippedLocations: [shaderLocationA, shaderLocationB],
    });

    // Add attribute B. Not that attributesB can be the same object as attributesA so they end
    // up in the same vertex buffer.
    const attributesB = vertexBufferAttributes[vertexBufferIndexB];
    addTestAttributes(attributesB, {
      testAttribute: { format: 'float32', offset: 0, shaderLocation: shaderLocationB },
      testAttributeAtStart: testAttributeAtStartB,
    });

    // Use the attributes to make the list of vertex buffers. Note that we might be setting the same vertex
    // buffer twice, but that only happens when it is the only vertex buffer.
    const vertexBuffers = [];
    vertexBuffers[vertexBufferIndexA] = { arrayStride: 256, attributes: attributesA };
    vertexBuffers[vertexBufferIndexB] = { arrayStride: 256, attributes: attributesB };

    // Note that an empty vertex shader will be used so errors only happens because of the conflict
    // in the vertex state.
    const success = shaderLocationA !== shaderLocationB;
    t.testVertexState(success, vertexBuffers);
  });

g.test('vertex_shader_input_location_limit')
  .desc(
    `Test that vertex shader's input's location decoration must be less than maxVertexAttributes.
   - Test for shaderLocation 0, 1, limit - 1, limit`
  )
  .subcases(() =>
    params().combine(
      poptions('testLocation', [0, 1, kMaxVertexAttributes - 1, kMaxVertexAttributes, -1, 2 ** 32])
    )
  )
  .fn(t => {
    const { testLocation } = t.params;

    const shader = t.generateTestVertexShader([
      {
        type: 'vec4<f32>',
        location: testLocation,
      },
    ]);

    const vertexBuffers = [
      {
        arrayStride: 512,
        attributes: [
          {
            format: 'float32',
            offset: 0,
            shaderLocation: testLocation,
          } as const,
        ],
      },
    ];

    const success = testLocation < kMaxVertexAttributes;
    t.testVertexState(success, vertexBuffers, shader);
  });

g.test('vertex_shader_input_location_in_vertex_state')
  .desc(
    `Test that a vertex shader defined in the shader must have a corresponding attribute in the vertex state.
       - Test for various input locations.
       - Test for the attribute in various places in the list of vertex buffer and various places inside the vertex buffer descriptor`
  )
  .subcases(() =>
    params()
      .combine(poptions('vertexBufferIndex', [0, 1, kMaxVertexBuffers - 1]))
      .combine(poptions('extraAttributeCount', [0, 1, kMaxVertexAttributes - 1]))
      .combine(pbool('testAttributeAtStart'))
      .combine(poptions('testShaderLocation', [0, 1, 4, 7, kMaxVertexAttributes - 1]))
  )
  .fn(t => {
    const {
      vertexBufferIndex,
      extraAttributeCount,
      testAttributeAtStart,
      testShaderLocation,
    } = t.params;
    // We have a shader using `testShaderLocation`.
    const shader = t.generateTestVertexShader([
      {
        type: 'vec4<f32>',
        location: testShaderLocation,
      },
    ]);

    const attributes: GPUVertexAttribute[] = [];
    const vertexBuffers = [];
    vertexBuffers[vertexBufferIndex] = { arrayStride: 256, attributes };

    // Fill attributes with a bunch of attributes for other locations.
    // Using that vertex state is invalid because the vertex state doesn't contain the test location
    addTestAttributes(attributes, {
      extraAttributeCount,
      extraAttributeSkippedLocations: [testShaderLocation],
    });
    t.testVertexState(false, vertexBuffers, shader);

    // Add an attribute for the test location and try again.
    addTestAttributes(attributes, {
      testAttribute: { format: 'float32', shaderLocation: testShaderLocation, offset: 0 },
      testAttributeAtStart,
    });
    t.testVertexState(true, vertexBuffers, shader);
  });

g.test('vertex_shader_type_matches_attribute_format')
  .desc(
    `
    Test that the vertex shader declaration must have a type compatible with the vertex format.
     - Test for all formats.
     - Test for all combinations of u/i/f32 with and without vectors.`
  )
  .cases(poptions('format', kVertexFormats))
  .subcases(() =>
    params()
      .combine(poptions('shaderBaseType', ['u32', 'i32', 'f32']))
      .expand(p => {
        return poptions('shaderType', [
          p.shaderBaseType,
          `vec2<${p.shaderBaseType}>`,
          `vec3<${p.shaderBaseType}>`,
          `vec4<${p.shaderBaseType}>`,
        ]);
      })
  )
  .fn(t => {
    const { format, shaderBaseType, shaderType } = t.params;
    const shader = t.generateTestVertexShader([
      {
        type: shaderType,
        location: 0,
      },
    ]);

    const requiredBaseType = {
      sint: 'i32',
      uint: 'u32',
      snorm: 'f32',
      unorm: 'f32',
      float: 'f32',
    }[kVertexFormatInfo[format].type];

    const success = requiredBaseType === shaderBaseType;
    t.testVertexState(
      success,
      [
        {
          arrayStride: 0,
          attributes: [{ offset: 0, shaderLocation: 0, format }],
        },
      ],
      shader
    );
  });

g.test('vertex_attribute_offset_alignment')
  .desc(
    `
    Test that vertex attribute offsets must be aligned to the format's component byte size.
    - Test for all formats.
    - Test for various arrayStrides and offsets within that stride
    - Test for various vertex buffer indices
    - Test for various amounts of attributes in that vertex buffer`
  )
  .cases(
    params()
      .combine(poptions('format', kVertexFormats))
      .combine(poptions('arrayStride', [256, kMaxVertexBufferArrayStride]))
      .expand(p => {
        const { bytesPerComponent, componentCount } = kVertexFormatInfo[p.format];
        const formatSize = bytesPerComponent * componentCount;
        const halfAlignment = Math.floor(bytesPerComponent / 2);

        return poptions(
          'offset',
          new Set([
            0,
            halfAlignment,
            bytesPerComponent,
            p.arrayStride - formatSize,
            p.arrayStride - formatSize - halfAlignment,
          ])
        );
      })
  )
  .subcases(() =>
    params()
      .combine(poptions('vertexBufferIndex', [0, 1, kMaxVertexBuffers - 1]))
      .combine(poptions('extraAttributeCount', [0, 1, kMaxVertexAttributes - 1]))
      .combine(pbool('testAttributeAtStart'))
  )
  .fn(t => {
    const {
      format,
      arrayStride,
      offset,
      vertexBufferIndex,
      extraAttributeCount,
      testAttributeAtStart,
    } = t.params;

    const attributes: GPUVertexAttribute[] = [];
    addTestAttributes(attributes, {
      testAttribute: { format, offset, shaderLocation: 0 },
      testAttributeAtStart,
      extraAttributeCount,
      extraAttributeSkippedLocations: [0],
    });

    const vertexBuffers = [];
    vertexBuffers[vertexBufferIndex] = { arrayStride, attributes };

    const success = offset % kVertexFormatInfo[format].bytesPerComponent === 0;
    t.testVertexState(success, vertexBuffers);
  });

g.test('vertex_attribute_contained_in_stride')
  .desc(
    `
    Test that vertex attribute [offset, offset + formatSize) must be contained in the arrayStride if arrayStride is not 0:
    - Test for all formats.
    - Test for various arrayStrides and offsets within that stride
    - Test for various vertex buffer indices
    - Test for various amounts of attributes in that vertex buffer`
  )
  .cases(poptions('format', kVertexFormats))
  .subcases(({ format }) =>
    params()
      .combine(
        poptions('arrayStride', [
          0,
          256,
          kMaxVertexBufferArrayStride - 4,
          kMaxVertexBufferArrayStride,
        ])
      )
      .expand(p => {
        // Compute a bunch of test offsets to test.
        const { bytesPerComponent, componentCount } = kVertexFormatInfo[format];
        const formatSize = bytesPerComponent * componentCount;
        const offsetsToTest = [0, bytesPerComponent];

        // arrayStride = 0 is a special case because for the offset validation it acts the same
        // as arrayStride = kMaxVertexBufferArrayStride. We branch so as to avoid adding negative
        // offsets that would cause an IDL exception to be thrown instead of a validation error.
        if (p.arrayStride === 0) {
          offsetsToTest.push(kMaxVertexBufferArrayStride - formatSize);
          offsetsToTest.push(kMaxVertexBufferArrayStride - formatSize + bytesPerComponent);
        } else {
          offsetsToTest.push(p.arrayStride - formatSize);
          offsetsToTest.push(p.arrayStride - formatSize + bytesPerComponent);
        }

        return poptions('offset', offsetsToTest);
      })
      .combine(poptions('vertexBufferIndex', [0, 1, kMaxVertexBuffers - 1]))
      .combine(poptions('extraAttributeCount', [0, 1, kMaxVertexAttributes - 1]))
      .combine(pbool('testAttributeAtStart'))
  )
  .fn(t => {
    const {
      format,
      arrayStride,
      offset,
      vertexBufferIndex,
      extraAttributeCount,
      testAttributeAtStart,
    } = t.params;

    const attributes: GPUVertexAttribute[] = [];
    addTestAttributes(attributes, {
      testAttribute: { format, offset, shaderLocation: 0 },
      testAttributeAtStart,
      extraAttributeCount,
      extraAttributeSkippedLocations: [0],
    });

    const vertexBuffers = [];
    vertexBuffers[vertexBufferIndex] = { arrayStride, attributes };

    const formatInfo = kVertexFormatInfo[format];
    const formatSize = formatInfo.bytesPerComponent * formatInfo.componentCount;
    const limit = arrayStride === 0 ? kMaxVertexBufferArrayStride : arrayStride;

    const success = offset + formatSize <= limit;
    t.testVertexState(success, vertexBuffers);
  });

g.test('many_attributes_overlapping')
  .desc(`Test that it is valid to have many vertex attributes overlap`)
  .fn(async t => {
    // Create many attributes, each of them intersects with at least 3 others.
    const attributes = [];
    const formats = ['float32x4', 'uint32x4', 'sint32x4'] as const;
    for (let i = 0; i < kMaxVertexAttributes; i++) {
      attributes.push({ format: formats[i % 3], offset: i * 4, shaderLocation: i } as const);
    }

    t.testVertexState(true, [{ arrayStride: 0, attributes }]);
  });
