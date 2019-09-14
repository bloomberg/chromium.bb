export const description = `
createBindGroupLayout validation tests.
`;

import { TestGroup } from '../../../framework/index.js';

import { ValidationTest } from './validation_test.js';

function clone(descriptor: GPUBindGroupLayoutDescriptor): GPUBindGroupLayoutDescriptor {
  return JSON.parse(JSON.stringify(descriptor));
}

export const g = new TestGroup(ValidationTest);

g.test('some binding index was specified more than once', async t => {
  const goodDescriptor: GPUBindGroupLayoutDescriptor = {
    bindings: [
      {
        binding: 0,
        visibility: GPUShaderStage.COMPUTE,
        type: 'storage-buffer',
      },
      {
        binding: 1,
        visibility: GPUShaderStage.COMPUTE,
        type: 'storage-buffer',
      },
    ],
  };

  // Control case
  t.device.createBindGroupLayout(goodDescriptor);

  const badDescriptor = clone(goodDescriptor);
  badDescriptor.bindings![1].binding = 0;

  // Binding index 0 can't be specified twice.
  await t.expectValidationError(() => {
    t.device.createBindGroupLayout(badDescriptor);
  });
});

g.test('negative binding index', async t => {
  const goodDescriptor: GPUBindGroupLayoutDescriptor = {
    bindings: [
      {
        binding: 0,
        visibility: GPUShaderStage.COMPUTE,
        type: 'storage-buffer',
      },
    ],
  };

  // Control case
  t.device.createBindGroupLayout(goodDescriptor);

  // Negative binding index can't be specified.
  const badDescriptor = clone(goodDescriptor);
  badDescriptor.bindings![0].binding = -1;

  await t.expectValidationError(() => {
    t.device.createBindGroupLayout(badDescriptor);
  });
});

// TODO: Update once https://github.com/gpuweb/gpuweb/issues/405 is decided.
g.test('Visibility of bindings cannot be None', async t => {
  const goodDescriptor: GPUBindGroupLayoutDescriptor = {
    bindings: [
      {
        binding: 0,
        visibility: GPUShaderStage.COMPUTE,
        type: 'storage-buffer',
      },
    ],
  };

  // Control case
  t.device.createBindGroupLayout(goodDescriptor);

  // Binding visibility set to None can't be specified.
  const badDescriptor = clone(goodDescriptor);
  badDescriptor.bindings![0].visibility = GPUShaderStage.NONE;

  await t.expectValidationError(() => {
    t.device.createBindGroupLayout(badDescriptor);
  });
});

g.test('number of dynamic buffers exceeds the maximum value', async t => {
  const { type, maxDynamicBufferCount } = t.params;

  const maxDynamicBufferBindings: GPUBindGroupLayoutBinding[] = [];
  for (let i = 0; i < maxDynamicBufferCount; i++) {
    maxDynamicBufferBindings.push({
      binding: i,
      visibility: GPUShaderStage.COMPUTE,
      type,
      dynamic: true,
    });
  }

  const goodDescriptor: GPUBindGroupLayoutDescriptor = {
    bindings: [
      ...maxDynamicBufferBindings,
      {
        binding: maxDynamicBufferBindings.length,
        visibility: GPUShaderStage.COMPUTE,
        type,
        dynamic: false,
      },
    ],
  };

  // Control case
  t.device.createBindGroupLayout(goodDescriptor);

  // Dynamic buffers exceed maximum in a bind group layout.
  const badDescriptor = clone(goodDescriptor);
  badDescriptor.bindings![maxDynamicBufferCount].dynamic = true;

  await t.expectValidationError(() => {
    t.device.createBindGroupLayout(badDescriptor);
  });
}).params([
  { type: 'storage-buffer', maxDynamicBufferCount: 4 },
  { type: 'uniform-buffer', maxDynamicBufferCount: 8 },
]);

g.test('dynamic set to true is allowed only for buffers', async t => {
  const { type, success } = t.params;

  const descriptor = {
    bindings: [
      {
        binding: 0,
        visibility: GPUShaderStage.FRAGMENT,
        type,
        dynamic: true,
      },
    ],
  };

  if (success) {
    // Control case
    t.device.createBindGroupLayout(descriptor);
  } else {
    // Dynamic set to true is not allowed in some cases.
    await t.expectValidationError(() => {
      t.device.createBindGroupLayout(descriptor);
    });
  }
}).params([
  { type: 'uniform-buffer', success: true },
  { type: 'storage-buffer', success: true },
  { type: 'readonly-storage-buffer', success: true },
  { type: 'sampler', success: false },
  { type: 'sampled-texture', success: false },
  { type: 'storage-texture', success: false },
]);
