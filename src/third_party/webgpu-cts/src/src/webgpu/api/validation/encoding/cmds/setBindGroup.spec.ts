export const description = `
setBindGroup validation tests.

TODO: merge these notes and implement.
> (Note: If there are errors with using certain binding types in certain passes, test those in the file for that pass type, not here.)
>
> - state tracking (probably separate file)
>     - x= {compute pass, render pass}
>     - {null, compatible, incompatible} current pipeline (should have no effect without draw/dispatch)
>     - setBindGroup in different orders (e.g. 0,1,2 vs 2,0,1)
`;

import { poptions, params, pbool } from '../../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { range, unreachable } from '../../../../../common/framework/util/util.js';
import { kMinDynamicBufferOffsetAlignment } from '../../../../capability_info.js';
import {
  kProgrammableEncoderTypes,
  ProgrammableEncoderType,
  ValidationTest,
} from '../../validation_test.js';

class F extends ValidationTest {
  encoderTypeToStageFlag(encoderType: ProgrammableEncoderType): GPUShaderStageFlags {
    switch (encoderType) {
      case 'compute pass':
        return GPUShaderStage.COMPUTE;
      case 'render pass':
      case 'render bundle':
        return GPUShaderStage.FRAGMENT;
      default:
        unreachable('Unknown encoder type');
    }
  }

  createBindingResourceWithState(
    resourceType: 'texture' | 'buffer',
    state: 'valid' | 'destroyed'
  ): GPUBindingResource {
    switch (resourceType) {
      case 'texture': {
        const texture = this.createTextureWithState('valid');
        const view = texture.createView();
        if (state === 'destroyed') {
          texture.destroy();
        }
        return view;
      }
      case 'buffer':
        return {
          buffer: this.createBufferWithState(state, {
            size: 4,
            usage: GPUBufferUsage.STORAGE,
          }),
        };
      default:
        unreachable('unknown resource type');
    }
  }

  createBindGroup(
    state: 'valid' | 'invalid' | 'destroyed',
    resourceType: 'buffer' | 'texture',
    encoderType: ProgrammableEncoderType,
    indices: number[]
  ) {
    if (state === 'invalid') {
      this.device.pushErrorScope('validation');
      indices = new Array<number>(indices.length + 1).fill(0);
    }

    const layout = this.device.createBindGroupLayout({
      entries: indices.map(binding => ({
        binding,
        visibility: this.encoderTypeToStageFlag(encoderType),
        ...(resourceType === 'buffer' ? { buffer: { type: 'storage' } } : { texture: {} }),
      })),
    });
    const bindGroup = this.device.createBindGroup({
      layout,
      entries: indices.map(binding => ({
        binding,
        resource: this.createBindingResourceWithState(
          resourceType,
          state === 'destroyed' ? state : 'valid'
        ),
      })),
    });

    if (state === 'invalid') {
      this.device.popErrorScope();
    }
    return bindGroup;
  }
}

export const g = makeTestGroup(F);

g.test('state_and_binding_index')
  .desc('Tests that setBindGroup correctly handles {valid, invalid} bindGroups.')
  .cases(
    params()
      .combine(poptions('encoderType', kProgrammableEncoderTypes))
      .combine(poptions('state', ['valid', 'invalid', 'destroyed'] as const))
      .combine(poptions('resourceType', ['buffer', 'texture'] as const))
  )
  .fn(async t => {
    const { encoderType, state, resourceType } = t.params;
    const { maxBindGroups } = t.device.adapter.limits || { maxBindGroups: 4 };

    async function runTest(index: number) {
      const { encoder, finish } = t.createEncoder(encoderType);
      encoder.setBindGroup(index, t.createBindGroup(state, resourceType, encoderType, [index]));

      const shouldError = index >= maxBindGroups;

      if (!shouldError && state === 'destroyed') {
        t.device.pushErrorScope('validation');
        const commandBuffer = finish();
        const error = await t.device.popErrorScope();
        t.expect(error === null, `finish() should not fail, but failed with ${error}`);
        t.expectValidationError(() => {
          t.queue.submit([commandBuffer]);
        });
      } else {
        t.expectValidationError(() => {
          t.debug('here');
          finish();
        }, shouldError || state !== 'valid');
      }
    }

    // TODO: move to subcases() once we can query the device limits
    for (const index of [1, maxBindGroups - 1, maxBindGroups]) {
      t.debug(`test bind group index ${index}`);
      await runTest(index);
    }
  });

g.test('dynamic_offsets_passed_but_not_expected')
  .desc('Tests that setBindGroup correctly errors on unexpected dynamicOffsets.')
  .cases(poptions('encoderType', kProgrammableEncoderTypes))
  .fn(async t => {
    const { encoderType } = t.params;
    const bindGroup = t.createBindGroup('valid', 'buffer', encoderType, []);
    const dynamicOffsets = [0];

    const { encoder, finish } = t.createEncoder(encoderType);
    encoder.setBindGroup(0, bindGroup, dynamicOffsets);

    t.expectValidationError(() => {
      finish();
    });
  });

g.test('dynamic_offsets_match_expectations_in_pass_encoder')
  .desc('Tests that given dynamicOffsets match the specified bindGroup.')
  .cases(
    params()
      .combine(poptions('encoderType', kProgrammableEncoderTypes))
      .combine([
        { dynamicOffsets: [256, 0], _success: true }, // Dynamic offsets aligned
        { dynamicOffsets: [1, 2], _success: false }, // Dynamic offsets not aligned

        // Wrong number of dynamic offsets
        { dynamicOffsets: [256, 0, 0], _success: false },
        { dynamicOffsets: [256], _success: false },
        { dynamicOffsets: [], _success: false },

        // Dynamic uniform buffer out of bounds because of binding size
        { dynamicOffsets: [512, 0], _success: false },
        { dynamicOffsets: [1024, 0], _success: false },
        { dynamicOffsets: [0xffffffff, 0], _success: false },

        // Dynamic storage buffer out of bounds because of binding size
        { dynamicOffsets: [0, 512], _success: false },
        { dynamicOffsets: [0, 1024], _success: false },
        { dynamicOffsets: [0, 0xffffffff], _success: false },
      ])
      .combine(pbool('useU32array'))
  )
  .fn(async t => {
    const kBindingSize = 9;

    const bindGroupLayout = t.device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: GPUShaderStage.COMPUTE | GPUShaderStage.FRAGMENT,
          buffer: {
            type: 'uniform',
            hasDynamicOffset: true,
          },
        },
        {
          binding: 1,
          visibility: GPUShaderStage.COMPUTE | GPUShaderStage.FRAGMENT,
          buffer: {
            type: 'storage',
            hasDynamicOffset: true,
          },
        },
      ],
    });

    const uniformBuffer = t.device.createBuffer({
      size: 2 * kMinDynamicBufferOffsetAlignment + 8,
      usage: GPUBufferUsage.UNIFORM,
    });

    const storageBuffer = t.device.createBuffer({
      size: 2 * kMinDynamicBufferOffsetAlignment + 8,
      usage: GPUBufferUsage.STORAGE,
    });

    const bindGroup = t.device.createBindGroup({
      layout: bindGroupLayout,
      entries: [
        {
          binding: 0,
          resource: {
            buffer: uniformBuffer,
            size: kBindingSize,
          },
        },
        {
          binding: 1,
          resource: {
            buffer: storageBuffer,
            size: kBindingSize,
          },
        },
      ],
    });

    const { encoderType, dynamicOffsets, useU32array, _success } = t.params;

    const { encoder, finish } = t.createEncoder(encoderType);
    if (useU32array) {
      encoder.setBindGroup(0, bindGroup, new Uint32Array(dynamicOffsets), 0, dynamicOffsets.length);
    } else {
      encoder.setBindGroup(0, bindGroup, dynamicOffsets);
    }

    t.expectValidationError(() => {
      finish();
    }, !_success);
  });

g.test('u32array_start_and_length')
  .desc('Tests that dynamicOffsetsData(Start|Length) apply to the given Uint32Array.')
  .subcases(() => [
    // dynamicOffsetsDataLength > offsets.length
    {
      offsets: [0] as const,
      dynamicOffsetsDataStart: 0,
      dynamicOffsetsDataLength: 2,
      _success: false,
    },
    // dynamicOffsetsDataStart + dynamicOffsetsDataLength > offsets.length
    {
      offsets: [0] as const,
      dynamicOffsetsDataStart: 1,
      dynamicOffsetsDataLength: 1,
      _success: false,
    },
    {
      offsets: [0, 0] as const,
      dynamicOffsetsDataStart: 1,
      dynamicOffsetsDataLength: 1,
      _success: true,
    },
    {
      offsets: [0, 0, 0] as const,
      dynamicOffsetsDataStart: 1,
      dynamicOffsetsDataLength: 1,
      _success: true,
    },
    {
      offsets: [0, 0] as const,
      dynamicOffsetsDataStart: 0,
      dynamicOffsetsDataLength: 2,
      _success: true,
    },
  ])
  .fn(t => {
    const { offsets, dynamicOffsetsDataStart, dynamicOffsetsDataLength, _success } = t.params;
    const kBindingSize = 8;

    const bindGroupLayout = t.device.createBindGroupLayout({
      entries: range(dynamicOffsetsDataLength, i => ({
        binding: i,
        visibility: GPUShaderStage.FRAGMENT,
        buffer: {
          type: 'storage',
          hasDynamicOffset: true,
        },
      })),
    });

    const bindGroup = t.device.createBindGroup({
      layout: bindGroupLayout,
      entries: range(dynamicOffsetsDataLength, i => ({
        binding: i,
        resource: {
          buffer: t.createBufferWithState('valid', {
            size: kBindingSize,
            usage: GPUBufferUsage.STORAGE,
          }),
          size: kBindingSize,
        },
      })),
    });

    const { encoder, finish } = t.createEncoder('render pass');
    encoder.setBindGroup(
      0,
      bindGroup,
      new Uint32Array(offsets),
      dynamicOffsetsDataStart,
      dynamicOffsetsDataLength
    );

    t.expectValidationError(() => {
      finish();
    }, !_success);
  });
