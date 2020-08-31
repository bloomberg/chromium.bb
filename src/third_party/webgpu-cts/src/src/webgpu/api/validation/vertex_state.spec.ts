export const description = `
vertexState validation tests.
`;

import * as C from '../../../common/constants.js';
import { TestGroup } from '../../../common/framework/test_group.js';

import { ValidationTest } from './validation_test.js';

const MAX_VERTEX_ATTRIBUTES: number = 16;
const MAX_VERTEX_BUFFER_END: number = 2048;
const MAX_VERTEX_BUFFER_ARRAY_STRIDE: number = 2048;
const MAX_VERTEX_BUFFERS: number = 16;

const SIZEOF_FLOAT = Float32Array.BYTES_PER_ELEMENT;

const VERTEX_SHADER_CODE_WITH_NO_INPUT = `
  #version 450
  void main() {
    gl_Position = vec4(0.0);
  }
`;

const FRAGMENT_SHADER_CODE = `
  #version 450
  layout(location = 0) out vec4 fragColor;
  void main() {
    fragColor = vec4(0.0, 1.0, 0.0, 1.0);
  }
`;

function clone<T extends GPUVertexStateDescriptor>(descriptor: T): T {
  return JSON.parse(JSON.stringify(descriptor));
}

class F extends ValidationTest {
  getDescriptor(
    vertexState: GPUVertexStateDescriptor,
    vertexShaderCode: string
  ): GPURenderPipelineDescriptor {
    const descriptor: GPURenderPipelineDescriptor = {
      vertexStage: {
        module: this.makeShaderModule('vertex', { glsl: vertexShaderCode }),
        entryPoint: 'main',
      },
      fragmentStage: {
        module: this.makeShaderModule('fragment', { glsl: FRAGMENT_SHADER_CODE }),
        entryPoint: 'main',
      },
      layout: this.device.createPipelineLayout({ bindGroupLayouts: [] }),
      primitiveTopology: 'triangle-list',
      colorStates: [{ format: 'rgba8unorm' }],
      vertexState,
    };
    return descriptor;
  }
}

export const g = new TestGroup(F);

g.test('an empty vertex input is valid', t => {
  const vertexState: GPUVertexStateDescriptor = {};
  const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
  t.device.createRenderPipeline(descriptor);
});

g.test('a null buffer is valid', t => {
  {
    // One null buffer is OK
    const vertexState: GPUVertexStateDescriptor = {
      vertexBuffers: [
        {
          arrayStride: 0,
          attributes: [],
        },
      ],
    };
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
  {
    //  One null buffer followed by a buffer is OK
    const vertexState: GPUVertexStateDescriptor = {
      vertexBuffers: [
        {
          arrayStride: 0,
          attributes: [],
        },
        {
          arrayStride: 0,
          attributes: [
            {
              format: 'float',
              offset: 0,
              shaderLocation: 0,
            },
          ],
        },
      ],
    };
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
  {
    //  One null buffer sitting between buffers is OK
    const vertexState: GPUVertexStateDescriptor = {
      vertexBuffers: [
        {
          arrayStride: 0,
          attributes: [
            {
              format: 'float',
              offset: 0,
              shaderLocation: 0,
            },
          ],
        },
        {
          arrayStride: 0,
          attributes: [],
        },
        {
          arrayStride: 0,
          attributes: [
            {
              format: 'float',
              offset: 0,
              shaderLocation: 1,
            },
          ],
        },
      ],
    };
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
});

g.test('pipeline vertex buffers are backed by attributes in vertex input', async t => {
  const vertexState: GPUVertexStateDescriptor = {
    vertexBuffers: [
      {
        arrayStride: 2 * SIZEOF_FLOAT,
        attributes: [
          {
            format: 'float',
            offset: 0,
            shaderLocation: 0,
          },
          {
            format: 'float',
            offset: 0,
            shaderLocation: 1,
          },
        ],
      },
    ],
  };
  {
    // Control case: pipeline with one input per attribute
    const code = `
      #version 450
      layout(location = 0) in vec4 a;
      layout(location = 1) in vec4 b;
      void main() {
          gl_Position = vec4(0.0);
      }
    `;
    const descriptor = t.getDescriptor(vertexState, code);
    t.device.createRenderPipeline(descriptor);
  }
  {
    // Check it is valid for the pipeline to use a subset of the VertexState
    const code = `
      #version 450
      layout(location = 0) in vec4 a;
      void main() {
          gl_Position = vec4(0.0);
      }
    `;
    const descriptor = t.getDescriptor(vertexState, code);
    t.device.createRenderPipeline(descriptor);
  }
  {
    // Check for an error when the pipeline uses an attribute not in the vertex input
    const code = `
      #version 450
      layout(location = 2) in vec4 a;
      void main() {
          gl_Position = vec4(0.0);
      }
    `;
    const descriptor = t.getDescriptor(vertexState, code);

    t.expectValidationError(() => {
      t.device.createRenderPipeline(descriptor);
    });
  }
});

g.test('an arrayStride of 0 is valid', t => {
  const vertexState = {
    vertexBuffers: [
      {
        arrayStride: 0,
        attributes: [
          {
            format: C.VertexFormat.Float,
            offset: 0,
            shaderLocation: 0,
          },
        ],
      },
    ],
  };
  {
    // Works ok without attributes
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
  {
    // Works ok with attributes at a large-ish offset
    vertexState.vertexBuffers[0].attributes[0].offset = 128;
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
});

g.test('offset should be within vertex buffer arrayStride if arrayStride is not zero', async t => {
  const vertexState = {
    vertexBuffers: [
      {
        arrayStride: 2 * SIZEOF_FLOAT,
        attributes: [
          {
            format: C.VertexFormat.Float,
            offset: 0,
            shaderLocation: 0,
          },
          {
            format: C.VertexFormat.Float,
            offset: SIZEOF_FLOAT,
            shaderLocation: 1,
          },
        ],
      },
    ],
  };
  {
    // Control case, setting correct arrayStride and offset
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
  {
    // Test vertex attribute offset exceed vertex buffer arrayStride range
    const badVertexState = clone(vertexState);
    badVertexState.vertexBuffers[0].attributes[1].format = C.VertexFormat.Float2;
    const descriptor = t.getDescriptor(badVertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);

    t.expectValidationError(() => {
      t.device.createRenderPipeline(descriptor);
    });
  }
  {
    // Test vertex attribute offset exceed vertex buffer arrayStride range
    const badVertexState = clone(vertexState);
    badVertexState.vertexBuffers[0].arrayStride = SIZEOF_FLOAT;
    const descriptor = t.getDescriptor(badVertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);

    t.expectValidationError(() => {
      t.device.createRenderPipeline(descriptor);
    });
  }
  {
    // It's OK if arrayStride is zero
    const goodVertexState = clone(vertexState);
    goodVertexState.vertexBuffers[0].arrayStride = 0;
    const descriptor = t.getDescriptor(goodVertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
});

g.test('check two attributes overlapping', async t => {
  const vertexState = {
    vertexBuffers: [
      {
        arrayStride: 2 * SIZEOF_FLOAT,
        attributes: [
          {
            format: C.VertexFormat.Float,
            offset: 0,
            shaderLocation: 0,
          },
          {
            format: C.VertexFormat.Float,
            offset: SIZEOF_FLOAT,
            shaderLocation: 1,
          },
        ],
      },
    ],
  };
  {
    // Control case, setting correct arrayStride and offset
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
  {
    // Test two attributes overlapping
    const badVertexState = clone(vertexState);
    badVertexState.vertexBuffers[0].attributes[0].format = C.VertexFormat.Int2;
    const descriptor = t.getDescriptor(badVertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);

    t.expectValidationError(() => {
      t.device.createRenderPipeline(descriptor);
    });
  }
});

g.test('check out of bounds condition on total number of vertex buffers', async t => {
  const vertexBuffers: GPUVertexBufferLayoutDescriptor[] = [];

  for (let i = 0; i < MAX_VERTEX_BUFFERS; i++) {
    vertexBuffers.push({
      arrayStride: 0,
      attributes: [
        {
          format: 'float',
          offset: 0,
          shaderLocation: i,
        },
      ],
    });
  }
  {
    // Control case, setting max vertex buffer number
    const vertexState: GPUVertexStateDescriptor = { vertexBuffers };
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
  {
    // Test vertex buffer number exceed the limit
    const vertexState: GPUVertexStateDescriptor = {
      vertexBuffers: [
        ...vertexBuffers,
        {
          arrayStride: 0,
          attributes: [
            {
              format: 'float',
              offset: 0,
              shaderLocation: MAX_VERTEX_BUFFERS,
            },
          ],
        },
      ],
    };
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);

    t.expectValidationError(() => {
      t.device.createRenderPipeline(descriptor);
    });
  }
});

g.test('check out of bounds on number of vertex attributes on a single vertex buffer', async t => {
  const vertexAttributes: GPUVertexAttributeDescriptor[] = [];

  for (let i = 0; i < MAX_VERTEX_ATTRIBUTES; i++) {
    vertexAttributes.push({
      format: 'float',
      offset: 0,
      shaderLocation: i,
    });
  }
  {
    // Control case, setting max vertex buffer number
    const vertexState: GPUVertexStateDescriptor = {
      vertexBuffers: [
        {
          arrayStride: 0,
          attributes: vertexAttributes,
        },
      ],
    };
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
  {
    // Test vertex attribute number exceed the limit
    const vertexState: GPUVertexStateDescriptor = {
      vertexBuffers: [
        {
          arrayStride: 0,
          attributes: [
            ...vertexAttributes,
            {
              format: 'float',
              offset: 0,
              shaderLocation: MAX_VERTEX_ATTRIBUTES,
            },
          ],
        },
      ],
    };
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);

    t.expectValidationError(() => {
      t.device.createRenderPipeline(descriptor);
    });
  }
});

g.test('check out of bounds on number of vertex attributes across vertex buffers', async t => {
  const vertexBuffers = [];
  for (let i = 0; i < MAX_VERTEX_ATTRIBUTES; i++) {
    vertexBuffers.push({
      arrayStride: 0,
      attributes: [{ format: C.VertexFormat.Float, offset: 0, shaderLocation: i }],
    });
  }

  {
    // Control case, setting max vertex buffer number
    const vertexState: GPUVertexStateDescriptor = { vertexBuffers };
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
  {
    // Test vertex attribute number exceed the limit
    vertexBuffers[MAX_VERTEX_ATTRIBUTES - 1].attributes.push({
      format: C.VertexFormat.Float,
      offset: 0,
      shaderLocation: MAX_VERTEX_ATTRIBUTES,
    });
    const vertexState: GPUVertexStateDescriptor = { vertexBuffers };
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);

    t.expectValidationError(() => {
      t.device.createRenderPipeline(descriptor);
    });
  }
});

g.test('check out of bounds condition on input strides', async t => {
  const vertexState = {
    vertexBuffers: [{ arrayStride: MAX_VERTEX_BUFFER_ARRAY_STRIDE, attributes: [] }],
  };
  {
    // Control case, setting max input arrayStride
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
  {
    // Test input arrayStride OOB
    vertexState.vertexBuffers[0].arrayStride = MAX_VERTEX_BUFFER_ARRAY_STRIDE + 4;
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);

    t.expectValidationError(() => {
      t.device.createRenderPipeline(descriptor);
    });
  }
});

g.test('check multiple of 4 bytes constraint on input arrayStride', async t => {
  const vertexState = {
    vertexBuffers: [
      {
        arrayStride: 4,
        attributes: [{ format: C.VertexFormat.Uchar2, offset: 0, shaderLocation: 0 }],
      },
    ],
  };
  {
    // Control case, setting input arrayStride 4 bytes
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
  {
    // Test input arrayStride not multiple of 4 bytes
    vertexState.vertexBuffers[0].arrayStride = 2;
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);

    t.expectValidationError(() => {
      t.device.createRenderPipeline(descriptor);
    });
  }
});

g.test('identical duplicate attributes are invalid', async t => {
  const vertexState = {
    vertexBuffers: [
      {
        arrayStride: 0,
        attributes: [{ format: C.VertexFormat.Float, offset: 0, shaderLocation: 0 }],
      },
    ],
  };
  {
    // Control case, setting attribute 0
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
  {
    // Oh no, attribute 0 is set twice
    vertexState.vertexBuffers[0].attributes.push({
      format: C.VertexFormat.Float,
      offset: 0,
      shaderLocation: 0,
    });
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);

    t.expectValidationError(() => {
      t.device.createRenderPipeline(descriptor);
    });
  }
});

g.test('we cannot set same shader location', async t => {
  {
    const vertexState = {
      vertexBuffers: [
        {
          arrayStride: 0,
          attributes: [
            { format: C.VertexFormat.Float, offset: 0, shaderLocation: 0 },
            { format: C.VertexFormat.Float, offset: SIZEOF_FLOAT, shaderLocation: 1 },
          ],
        },
      ],
    };
    {
      // Control case, setting different shader locations in two attributes
      const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
      t.device.createRenderPipeline(descriptor);
    }
    {
      // Test same shader location in two attributes in the same buffer
      vertexState.vertexBuffers[0].attributes[1].shaderLocation = 0;
      const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);

      t.expectValidationError(() => {
        t.device.createRenderPipeline(descriptor);
      });
    }
  }
  {
    const vertexState: GPUVertexStateDescriptor = {
      vertexBuffers: [
        {
          arrayStride: 0,
          attributes: [
            {
              format: 'float',
              offset: 0,
              shaderLocation: 0,
            },
          ],
        },
        {
          arrayStride: 0,
          attributes: [
            {
              format: 'float',
              offset: 0,
              shaderLocation: 0,
            },
          ],
        },
      ],
    };
    // Test same shader location in two attributes in different buffers
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);

    t.expectValidationError(() => {
      t.device.createRenderPipeline(descriptor);
    });
  }
});

g.test('check out of bounds condition on attribute shader location', async t => {
  const vertexState = {
    vertexBuffers: [
      {
        arrayStride: 0,
        attributes: [
          { format: C.VertexFormat.Float, offset: 0, shaderLocation: MAX_VERTEX_ATTRIBUTES - 1 },
        ],
      },
    ],
  };
  {
    // Control case, setting last attribute shader location
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
  {
    // Test attribute location OOB
    vertexState.vertexBuffers[0].attributes[0].shaderLocation = MAX_VERTEX_ATTRIBUTES;
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);

    t.expectValidationError(() => {
      t.device.createRenderPipeline(descriptor);
    });
  }
});

g.test('check attribute offset out of bounds', async t => {
  const vertexState = {
    vertexBuffers: [
      {
        arrayStride: 0,
        attributes: [
          {
            format: C.VertexFormat.Float2,
            offset: MAX_VERTEX_BUFFER_END - 2 * SIZEOF_FLOAT,
            shaderLocation: 0,
          },
        ],
      },
    ],
  };
  {
    // Control case, setting max attribute offset to MAX_VERTEX_BUFFER_END - 8
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
  {
    // Control case, setting attribute offset to 8
    vertexState.vertexBuffers[0].attributes[0].offset = 8;
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
  {
    // Test attribute offset out of bounds
    vertexState.vertexBuffers[0].attributes[0].offset = MAX_VERTEX_BUFFER_END - 4;
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);

    t.expectValidationError(() => {
      t.device.createRenderPipeline(descriptor);
    });
  }
});

g.test('check multiple of 4 bytes constraint on offset', async t => {
  const vertexState = {
    vertexBuffers: [
      {
        arrayStride: 0,
        attributes: [{ format: C.VertexFormat.Float, offset: SIZEOF_FLOAT, shaderLocation: 0 }],
      },
    ],
  };
  {
    // Control case, setting offset 4 bytes
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.device.createRenderPipeline(descriptor);
  }
  {
    // Test offset of 2 bytes with uchar2 format
    vertexState.vertexBuffers[0].attributes[0].offset = 2;
    vertexState.vertexBuffers[0].attributes[0].format = C.VertexFormat.Uchar2;
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);
    t.expectValidationError(() => {
      t.device.createRenderPipeline(descriptor);
    });
  }
  {
    // Test offset of 2 bytes with float format
    vertexState.vertexBuffers[0].attributes[0].offset = 2;
    vertexState.vertexBuffers[0].attributes[0].format = C.VertexFormat.Float;
    const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);

    t.expectValidationError(() => {
      t.device.createRenderPipeline(descriptor);
    });
  }
});

g.test('check attribute offset overflow', async t => {
  const vertexState: GPUVertexStateDescriptor = {
    vertexBuffers: [
      {
        arrayStride: 0,
        attributes: [{ format: 'float', offset: Number.MAX_SAFE_INTEGER, shaderLocation: 0 }],
      },
    ],
  };
  const descriptor = t.getDescriptor(vertexState, VERTEX_SHADER_CODE_WITH_NO_INPUT);

  t.expectValidationError(() => {
    t.device.createRenderPipeline(descriptor);
  });
});
