export const description = `
createPipelineLayout validation tests.
`;

import { TestGroup } from '../../../framework/index.js';

import { ValidationTest } from './validation_test.js';

function clone(descriptor: GPUBindGroupLayoutDescriptor): GPUBindGroupLayoutDescriptor {
  return JSON.parse(JSON.stringify(descriptor));
}

export const g = new TestGroup(ValidationTest);

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

  const maxDynamicBufferBindGroupLayout = t.device.createBindGroupLayout({
    bindings: maxDynamicBufferBindings,
  });

  const goodDescriptor: GPUBindGroupLayoutDescriptor = {
    bindings: [
      {
        binding: 0,
        visibility: GPUShaderStage.COMPUTE,
        type,
        dynamic: false,
      },
    ],
  };

  const goodPipelineLayoutDescriptor = {
    bindGroupLayouts: [
      maxDynamicBufferBindGroupLayout,
      t.device.createBindGroupLayout(goodDescriptor),
    ],
  };

  // Control case
  t.device.createPipelineLayout(goodPipelineLayoutDescriptor);

  // Check dynamic buffers exceed maximum in pipeline layout.
  const badDescriptor = clone(goodDescriptor);
  badDescriptor.bindings![0].dynamic = true;

  const badPipelineLayoutDescriptor = {
    bindGroupLayouts: [
      maxDynamicBufferBindGroupLayout,
      t.device.createBindGroupLayout(badDescriptor),
    ],
  };

  await t.expectValidationError(() => {
    t.device.createPipelineLayout(badPipelineLayoutDescriptor);
  });
}).params([
  { type: 'storage-buffer', maxDynamicBufferCount: 4 },
  { type: 'uniform-buffer', maxDynamicBufferCount: 8 },
]);

// TODO: test max BGLs (4 works, 5 doesn't)
