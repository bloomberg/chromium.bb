export const description = `
  createBindGroup validation tests.

  TODO: Ensure sure tests cover all createBindGroup validation rules.
`;

import { poptions, params } from '../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { unreachable } from '../../../common/framework/util/util.js';
import {
  allBindingEntries,
  bindingTypeInfo,
  kBindableResources,
  kTextureUsages,
  kTextureViewDimensions,
  sampledAndStorageBindingEntries,
  texBindingTypeInfo,
} from '../../capability_info.js';
import { GPUConst } from '../../constants.js';

import { ValidationTest } from './validation_test.js';

function clone<T extends GPUTextureDescriptor>(descriptor: T): T {
  return JSON.parse(JSON.stringify(descriptor));
}

export const g = makeTestGroup(ValidationTest);

g.test('binding_count_mismatch')
  .desc('Test that the number of entries must match the number of entries in the BindGroupLayout.')
  .subcases(() =>
    params()
      .combine(poptions('layoutEntryCount', [1, 2, 3]))
      .combine(poptions('bindGroupEntryCount', [1, 2, 3]))
  )
  .fn(async t => {
    const { layoutEntryCount, bindGroupEntryCount } = t.params;

    const layoutEntries: Array<GPUBindGroupLayoutEntry> = [];
    for (let i = 0; i < layoutEntryCount; ++i) {
      layoutEntries.push({
        binding: i,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: 'storage' },
      });
    }
    const bindGroupLayout = t.device.createBindGroupLayout({ entries: layoutEntries });

    const entries: Array<GPUBindGroupEntry> = [];
    for (let i = 0; i < bindGroupEntryCount; ++i) {
      entries.push({
        binding: i,
        resource: { buffer: t.getStorageBuffer() },
      });
    }

    const shouldError = layoutEntryCount !== bindGroupEntryCount;
    t.expectValidationError(() => {
      t.device.createBindGroup({
        entries,
        layout: bindGroupLayout,
      });
    }, shouldError);
  });

g.test('binding_must_be_present_in_layout')
  .desc(
    'Test that the binding slot for each entry matches a binding slot defined in the BindGroupLayout.'
  )
  .subcases(() =>
    params()
      .combine(poptions('layoutBinding', [0, 1, 2]))
      .combine(poptions('binding', [0, 1, 2]))
  )
  .fn(async t => {
    const { layoutBinding, binding } = t.params;

    const bindGroupLayout = t.device.createBindGroupLayout({
      entries: [
        { binding: layoutBinding, visibility: GPUShaderStage.COMPUTE, buffer: { type: 'storage' } },
      ],
    });

    const descriptor = {
      entries: [{ binding, resource: { buffer: t.getStorageBuffer() } }],
      layout: bindGroupLayout,
    };

    const shouldError = layoutBinding !== binding;
    t.expectValidationError(() => {
      t.device.createBindGroup(descriptor);
    }, shouldError);
  });

g.test('binding_must_contain_resource_defined_in_layout')
  .desc(
    'Test that only the resource type specified in the BindGroupLayout is allowed for each entry.'
  )
  .subcases(() =>
    params()
      .combine(poptions('resourceType', kBindableResources))
      .combine(poptions('entry', allBindingEntries(false)))
  )
  .fn(t => {
    const { resourceType, entry } = t.params;
    const info = bindingTypeInfo(entry);

    const layout = t.device.createBindGroupLayout({
      entries: [{ binding: 0, visibility: GPUShaderStage.COMPUTE, ...entry }],
    });

    const resource = t.getBindingResource(resourceType);

    const resourceBindingMatches = info.resource === resourceType;
    t.expectValidationError(() => {
      t.device.createBindGroup({ layout, entries: [{ binding: 0, resource }] });
    }, !resourceBindingMatches);
  });

g.test('texture_binding_must_have_correct_usage')
  .desc('Tests that texture bindings must have the correct usage.')
  .subcases(() =>
    params()
      .combine(poptions('entry', sampledAndStorageBindingEntries(false)))
      .combine(poptions('usage', kTextureUsages))
      .unless(({ entry, usage }) => {
        const info = texBindingTypeInfo(entry);
        // Can't create the texture for this (usage=STORAGE and sampleCount=4), so skip.
        return usage === GPUConst.TextureUsage.STORAGE && info.resource === 'sampledTexMS';
      })
  )
  .fn(async t => {
    const { entry, usage } = t.params;
    const info = texBindingTypeInfo(entry);

    const bindGroupLayout = t.device.createBindGroupLayout({
      entries: [{ binding: 0, visibility: GPUShaderStage.FRAGMENT, ...entry }],
    });

    const descriptor = {
      size: { width: 16, height: 16, depthOrArrayLayers: 1 },
      format: 'rgba8unorm' as const,
      usage,
      sampleCount: info.resource === 'sampledTexMS' ? 4 : 1,
    };
    const resource = t.device.createTexture(descriptor).createView();

    const shouldError = usage !== info.usage;
    t.expectValidationError(() => {
      t.device.createBindGroup({
        entries: [{ binding: 0, resource }],
        layout: bindGroupLayout,
      });
    }, shouldError);
  });

g.test('texture_must_have_correct_component_type')
  .desc(
    `
    Tests that texture bindings must have a format that matches the sample type specified in the BindGroupLayout.
    - Tests a compatible format for every sample type
    - Tests an incompatible format for every sample type`
  )
  .cases(poptions('sampleType', ['float', 'sint', 'uint'] as const))
  .fn(async t => {
    const { sampleType } = t.params;

    const bindGroupLayout = t.device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: GPUShaderStage.FRAGMENT,
          texture: { sampleType },
        },
      ],
    });

    let format: GPUTextureFormat;
    if (sampleType === 'float') {
      format = 'r8unorm';
    } else if (sampleType === 'sint') {
      format = 'r8sint';
    } else if (sampleType === 'uint') {
      format = 'r8uint';
    } else {
      unreachable('Unexpected texture component type');
    }

    const goodDescriptor = {
      size: { width: 16, height: 16, depthOrArrayLayers: 1 },
      format,
      usage: GPUTextureUsage.SAMPLED,
    };

    // Control case
    t.device.createBindGroup({
      entries: [
        {
          binding: 0,
          resource: t.device.createTexture(goodDescriptor).createView(),
        },
      ],
      layout: bindGroupLayout,
    });

    function* mismatchedTextureFormats(): Iterable<GPUTextureFormat> {
      if (sampleType !== 'float') {
        yield 'r8unorm';
      }
      if (sampleType !== 'sint') {
        yield 'r8sint';
      }
      if (sampleType !== 'uint') {
        yield 'r8uint';
      }
    }

    // Mismatched texture binding formats are not valid.
    for (const mismatchedTextureFormat of mismatchedTextureFormats()) {
      const badDescriptor: GPUTextureDescriptor = clone(goodDescriptor);
      badDescriptor.format = mismatchedTextureFormat;

      t.expectValidationError(() => {
        t.device.createBindGroup({
          entries: [{ binding: 0, resource: t.device.createTexture(badDescriptor).createView() }],
          layout: bindGroupLayout,
        });
      });
    }
  });

g.test('texture_must_have_correct_dimension')
  .desc(
    `
    Test that bound texture views match the dimensions supplied in the BindGroupLayout
    - Test for every GPUTextureViewDimension`
  )
  .cases(poptions('viewDimension', kTextureViewDimensions))
  .subcases(() => poptions('dimension', kTextureViewDimensions))
  .fn(async t => {
    const { viewDimension, dimension } = t.params;
    const bindGroupLayout = t.device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: GPUShaderStage.FRAGMENT,
          texture: { viewDimension },
        },
      ],
    });

    const texture = t.device.createTexture({
      size: { width: 16, height: 16, depthOrArrayLayers: 6 },
      format: 'rgba8unorm' as const,
      usage: GPUTextureUsage.SAMPLED,
    });

    const shouldError = viewDimension !== dimension;
    const arrayLayerCount = dimension === '2d' ? 1 : undefined;
    const textureView = texture.createView({ dimension, arrayLayerCount });

    t.expectValidationError(() => {
      t.device.createBindGroup({
        entries: [{ binding: 0, resource: textureView }],
        layout: bindGroupLayout,
      });
    }, shouldError);
  });

g.test('buffer_offset_and_size_for_bind_groups_match')
  .desc(
    `
    Test that a buffer binding's [offset, offset + size) must be contained in the BindGroup entry's buffer.
    - Test for various offsets and sizes
    - TODO(#234): disallow zero-sized bindings`
  )
  .subcases(() => [
    { offset: 0, size: 512, _success: true }, // offset 0 is valid
    { offset: 256, size: 256, _success: true }, // offset 256 (aligned) is valid

    // Touching the end of the buffer
    { offset: 0, size: 1024, _success: true },
    { offset: 0, size: undefined, _success: true },
    { offset: 256 * 3, size: 256, _success: true },
    { offset: 256 * 3, size: undefined, _success: true },

    // Zero-sized bindings
    { offset: 0, size: 0, _success: true },
    { offset: 256, size: 0, _success: true },
    { offset: 1024, size: 0, _success: true },
    { offset: 1024, size: undefined, _success: true },

    // Unaligned buffer offset is invalid
    { offset: 1, size: 256, _success: false },
    { offset: 1, size: undefined, _success: false },
    { offset: 128, size: 256, _success: false },
    { offset: 255, size: 256, _success: false },

    // Out-of-bounds
    { offset: 256 * 5, size: 0, _success: false }, // offset is OOB
    { offset: 0, size: 256 * 5, _success: false }, // size is OOB
    { offset: 1024, size: 1, _success: false }, // offset+size is OOB
  ])
  .fn(async t => {
    const { offset, size, _success } = t.params;

    const bindGroupLayout = t.device.createBindGroupLayout({
      entries: [{ binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: 'storage' } }],
    });

    const buffer = t.device.createBuffer({
      size: 1024,
      usage: GPUBufferUsage.STORAGE,
    });

    const descriptor = {
      entries: [
        {
          binding: 0,
          resource: { buffer, offset, size },
        },
      ],
      layout: bindGroupLayout,
    };

    if (_success) {
      // Control case
      t.device.createBindGroup(descriptor);
    } else {
      // Buffer offset and/or size don't match in bind groups.
      t.expectValidationError(() => {
        t.device.createBindGroup(descriptor);
      });
    }
  });

g.test('minBindingSize')
  .desc('Tests that minBindingSize is correctly enforced.')
  .subcases(() =>
    params()
      .combine(poptions('minBindingSize', [undefined, 4, 256]))
      .expand(({ minBindingSize }) =>
        poptions(
          'size',
          minBindingSize !== undefined
            ? [minBindingSize - 1, minBindingSize, minBindingSize + 1]
            : [4, 256]
        )
      )
  )
  .fn(t => {
    const { size, minBindingSize } = t.params;

    const bindGroupLayout = t.device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: GPUShaderStage.FRAGMENT,
          buffer: {
            type: 'storage',
            minBindingSize,
          },
        },
      ],
    });

    const storageBuffer = t.device.createBuffer({
      size,
      usage: GPUBufferUsage.STORAGE,
    });

    t.expectValidationError(() => {
      t.device.createBindGroup({
        layout: bindGroupLayout,
        entries: [
          {
            binding: 0,
            resource: {
              buffer: storageBuffer,
            },
          },
        ],
      });
    }, minBindingSize !== undefined && size < minBindingSize);
  });
