export const description = `
createBindGroupLayout validation tests.

TODO: make sure tests are complete.
`;

import { poptions, params } from '../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../common/framework/test_group.js';
import {
  kMaxBindingsPerBindGroup,
  kShaderStages,
  kShaderStageCombinations,
  kTextureViewDimensions,
  allBindingEntries,
  bindingTypeInfo,
  bufferBindingTypeInfo,
  kBufferBindingTypes,
  BGLEntry,
} from '../../capability_info.js';

import { ValidationTest } from './validation_test.js';

function clone<T extends GPUBindGroupLayoutDescriptor>(descriptor: T): T {
  return JSON.parse(JSON.stringify(descriptor));
}

export const g = makeTestGroup(ValidationTest);

g.test('duplicate_bindings')
  .desc('Test that uniqueness of binding numbers across entries is enforced.')
  .subcases(() => [
    { bindings: [0, 1], _valid: true },
    { bindings: [0, 0], _valid: false },
  ])
  .fn(async t => {
    const { bindings, _valid } = t.params;
    const entries: Array<GPUBindGroupLayoutEntry> = [];

    for (const binding of bindings) {
      entries.push({
        binding,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: 'storage' as const },
      });
    }

    t.expectValidationError(() => {
      t.device.createBindGroupLayout({
        entries,
      });
    }, !_valid);
  });

g.test('visibility')
  .desc(
    `
    Test that only the appropriate combinations of visibilities are allowed for each resource type.
    - Test each possible combination of shader stage visibilities.
    - Test each type of bind group resource.`
  )
  .cases(poptions('visibility', kShaderStageCombinations))
  .subcases(() => poptions('entry', allBindingEntries(false)))
  .fn(async t => {
    const { visibility, entry } = t.params;
    const info = bindingTypeInfo(entry);

    const success = (visibility & ~info.validStages) === 0;

    t.expectValidationError(() => {
      t.device.createBindGroupLayout({
        entries: [{ binding: 0, visibility, ...entry }],
      });
    }, !success);
  });

g.test('multisampled_validation')
  .desc('Test that multisampling is only allowed with "2d" view dimensions.')
  .subcases(() => poptions('viewDimension', [undefined, ...kTextureViewDimensions]))
  .fn(async t => {
    const { viewDimension } = t.params;

    const success = viewDimension === '2d' || viewDimension === undefined;

    t.expectValidationError(() => {
      t.device.createBindGroupLayout({
        entries: [
          {
            binding: 0,
            visibility: GPUShaderStage.COMPUTE,
            texture: { multisampled: true, viewDimension },
          },
        ],
      });
    }, !success);
  });

g.test('max_dynamic_buffers')
  .desc(
    `
    Test that limits on the maximum number of dynamic buffers are enforced.
    - Test creation of a bind group layout using the maximum number of dynamic buffers works.
    - Test creation of a bind group layout using the maximum number of dynamic buffers + 1 fails.
    - TODO(#230): Update to enforce per-stage and per-pipeline-layout limits on BGLs as well.`
  )
  .cases(poptions('type', kBufferBindingTypes))
  .subcases(() =>
    params()
      .combine(poptions('extraDynamicBuffers', [0, 1]))
      .combine(poptions('staticBuffers', [0, 1]))
  )
  .fn(async t => {
    const { type, extraDynamicBuffers, staticBuffers } = t.params;
    const info = bufferBindingTypeInfo({ type });

    const dynamicBufferCount = info.perPipelineLimitClass.maxDynamic + extraDynamicBuffers;

    const entries = [];
    for (let i = 0; i < dynamicBufferCount; i++) {
      entries.push({
        binding: i,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type, hasDynamicOffset: true },
      });
    }

    for (let i = dynamicBufferCount; i < dynamicBufferCount + staticBuffers; i++) {
      entries.push({
        binding: i,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type, hasDynamicOffset: false },
      });
    }

    const descriptor = {
      entries,
    };

    t.expectValidationError(() => {
      t.device.createBindGroupLayout(descriptor);
    }, extraDynamicBuffers > 0);
  });

/**
 * One bind group layout will be filled with kPerStageBindingLimit[...] of the type |type|.
 * For each item in the array returned here, a case will be generated which tests a pipeline
 * layout with one extra bind group layout with one extra binding. That extra binding will have:
 *
 *   - If extraTypeSame, any of the binding types which counts toward the same limit as |type|.
 *     (i.e. 'storage-buffer' <-> 'readonly-storage-buffer').
 *   - Otherwise, an arbitrary other type.
 */
function* pickExtraBindingTypesForPerStage(
  entry: BGLEntry,
  extraTypeSame: boolean
): IterableIterator<BGLEntry> {
  if (extraTypeSame) {
    const info = bindingTypeInfo(entry);
    for (const extra of allBindingEntries(false)) {
      const extraInfo = bindingTypeInfo(extra);
      if (info.perStageLimitClass.class === extraInfo.perStageLimitClass.class) {
        yield extra;
      }
    }
  } else {
    return entry.sampler ? { texture: {} } : { sampler: {} };
  }
}

function subcasesForMaxResourcesPerStageTests(caseParams: { maxedEntry: BGLEntry }) {
  return params()
    .combine(poptions('maxedVisibility', kShaderStages))
    .filter(p => (bindingTypeInfo(caseParams.maxedEntry).validStages & p.maxedVisibility) !== 0)
    .expand(function* () {
      yield* poptions('extraEntry', pickExtraBindingTypesForPerStage(caseParams.maxedEntry, true));
      yield* poptions('extraEntry', pickExtraBindingTypesForPerStage(caseParams.maxedEntry, false));
    })
    .combine(poptions('extraVisibility', kShaderStages))
    .filter(p => (bindingTypeInfo(p.extraEntry).validStages & p.extraVisibility) !== 0);
}

// Should never fail unless kMaxBindingsPerBindGroup is exceeded, because the validation for
// resources-of-type-per-stage is in pipeline layout creation.
g.test('max_resources_per_stage,in_bind_group_layout')
  .desc(
    `
    Test that the maximum number of bindings of a given type per-stage cannot be exceeded in a
    single bind group layout.
    - Test each binding type.
    - Test that creation of a bind group layout using the maximum number of bindings works.
    - Test that creation of a bind group layout using the maximum number of bindings + 1 fails.
    - TODO(#230): Update to enforce per-stage and per-pipeline-layout limits on BGLs as well.`
  )
  .cases(poptions('maxedEntry', allBindingEntries(false)))
  .subcases(cp => subcasesForMaxResourcesPerStageTests(cp))
  .fn(async t => {
    const { maxedEntry, extraEntry, maxedVisibility, extraVisibility } = t.params;
    const maxedTypeInfo = bindingTypeInfo(maxedEntry);
    const maxedCount = maxedTypeInfo.perStageLimitClass.max;

    const maxResourceBindings: GPUBindGroupLayoutEntry[] = [];
    for (let i = 0; i < maxedCount; i++) {
      maxResourceBindings.push({
        binding: i,
        visibility: maxedVisibility,
        ...maxedEntry,
      });
    }

    const goodDescriptor = { entries: maxResourceBindings };

    // Control
    t.device.createBindGroupLayout(goodDescriptor);

    const newDescriptor = clone(goodDescriptor);
    newDescriptor.entries.push({
      binding: maxedCount,
      visibility: extraVisibility,
      ...extraEntry,
    });

    const shouldError = maxedCount >= kMaxBindingsPerBindGroup;

    t.expectValidationError(() => {
      t.device.createBindGroupLayout(newDescriptor);
    }, shouldError);
  });

// One pipeline layout can have a maximum number of each type of binding *per stage* (which is
// different for each type). Test that the max works, then add one more binding of same-or-different
// type and same-or-different visibility.
g.test('max_resources_per_stage,in_pipeline_layout')
  .desc(
    `
    Test that the maximum number of bindings of a given type per-stage cannot be exceeded across
    multiple bind group layouts when creating a pipeline layout.
    - Test each binding type.
    - Test that creation of a pipeline using the maximum number of bindings works.
    - Test that creation of a pipeline using the maximum number of bindings + 1 fails.
  `
  )
  .cases(poptions('maxedEntry', allBindingEntries(false)))
  .subcases(cp => subcasesForMaxResourcesPerStageTests(cp))
  .fn(async t => {
    const { maxedEntry, extraEntry, maxedVisibility, extraVisibility } = t.params;
    const maxedTypeInfo = bindingTypeInfo(maxedEntry);
    const maxedCount = maxedTypeInfo.perStageLimitClass.max;
    const extraTypeInfo = bindingTypeInfo(extraEntry);

    const maxResourceBindings: GPUBindGroupLayoutEntry[] = [];
    for (let i = 0; i < maxedCount; i++) {
      maxResourceBindings.push({
        binding: i,
        visibility: maxedVisibility,
        ...maxedEntry,
      });
    }

    const goodLayout = t.device.createBindGroupLayout({ entries: maxResourceBindings });

    // Control
    t.device.createPipelineLayout({ bindGroupLayouts: [goodLayout] });

    const extraLayout = t.device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: extraVisibility,
          ...extraEntry,
        },
      ],
    });

    // Some binding types use the same limit, e.g. 'storage-buffer' and 'readonly-storage-buffer'.
    const newBindingCountsTowardSamePerStageLimit =
      (maxedVisibility & extraVisibility) !== 0 &&
      maxedTypeInfo.perStageLimitClass.class === extraTypeInfo.perStageLimitClass.class;
    const layoutExceedsPerStageLimit = newBindingCountsTowardSamePerStageLimit;

    t.expectValidationError(() => {
      t.device.createPipelineLayout({ bindGroupLayouts: [goodLayout, extraLayout] });
    }, layoutExceedsPerStageLimit);
  });
