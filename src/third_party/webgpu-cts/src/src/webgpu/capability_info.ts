/* eslint-disable no-sparse-arrays */
import { ResolveType, ZipKeysWithValues } from '../common/framework/util/types.js';

import { GPUConst } from './constants.js';

type valueof<K> = K[keyof K];
type GPUTextureUsage = valueof<typeof GPUTextureUsage>;
type GPUBufferUsage = valueof<typeof GPUBufferUsage>;

function keysOf<T extends string>(obj: { [k in T]: unknown }): readonly T[] {
  return (Object.keys(obj) as unknown[]) as T[];
}

function numericKeysOf<T>(obj: object): readonly T[] {
  return (Object.keys(obj).map(n => Number(n)) as unknown[]) as T[];
}

/**
 * Creates an info lookup object from a more nicely-formatted table. See below for examples.
 *
 * Note: Using `as const` on the arguments to this function is necessary to infer the correct type.
 */
function makeTable<
  Members extends readonly string[],
  Defaults extends readonly unknown[],
  Table extends { readonly [k: string]: readonly unknown[] }
>(
  members: Members,
  defaults: Defaults,
  table: Table
): {
  readonly [k in keyof Table]: ResolveType<ZipKeysWithValues<Members, Table[k], Defaults>>;
} {
  const result: { [k: string]: { [m: string]: unknown } } = {};
  for (const [k, v] of Object.entries<readonly unknown[]>(table)) {
    const item: { [m: string]: unknown } = {};
    for (let i = 0; i < members.length; ++i) {
      item[members[i]] = v[i] ?? defaults[i];
    }
    result[k] = item;
  }
  /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
  return result as any;
}

// Buffers

export const kBufferSizeAlignment = 4;

export const kBufferUsageInfo: {
  readonly [k in GPUBufferUsage]: {};
} = /* prettier-ignore */ {
  [GPUConst.BufferUsage.MAP_READ]:      {},
  [GPUConst.BufferUsage.MAP_WRITE]:     {},
  [GPUConst.BufferUsage.COPY_SRC]:      {},
  [GPUConst.BufferUsage.COPY_DST]:      {},
  [GPUConst.BufferUsage.INDEX]:         {},
  [GPUConst.BufferUsage.VERTEX]:        {},
  [GPUConst.BufferUsage.UNIFORM]:       {},
  [GPUConst.BufferUsage.STORAGE]:       {},
  [GPUConst.BufferUsage.INDIRECT]:      {},
  [GPUConst.BufferUsage.QUERY_RESOLVE]: {},
};
export const kBufferUsages = numericKeysOf<GPUBufferUsage>(kBufferUsageInfo);

// Textures

export const kRegularTextureFormatInfo = /* prettier-ignore */ makeTable(
                          ['renderable', 'color', 'depth', 'stencil', 'storage', 'copySrc', 'copyDst', 'bytesPerBlock', 'blockWidth', 'blockHeight',              'extension'] as const,
                          [            ,    true,   false,     false,          ,      true,      true,                ,            1,             1,                         ] as const, {
  // 8-bit formats
  'r8unorm':              [        true,        ,        ,          ,     false,          ,          ,               1],
  'r8snorm':              [       false,        ,        ,          ,     false,          ,          ,               1],
  'r8uint':               [        true,        ,        ,          ,     false,          ,          ,               1],
  'r8sint':               [        true,        ,        ,          ,     false,          ,          ,               1],
  // 16-bit formats
  'r16uint':              [        true,        ,        ,          ,     false,          ,          ,               2],
  'r16sint':              [        true,        ,        ,          ,     false,          ,          ,               2],
  'r16float':             [        true,        ,        ,          ,     false,          ,          ,               2],
  'rg8unorm':             [        true,        ,        ,          ,     false,          ,          ,               2],
  'rg8snorm':             [       false,        ,        ,          ,     false,          ,          ,               2],
  'rg8uint':              [        true,        ,        ,          ,     false,          ,          ,               2],
  'rg8sint':              [        true,        ,        ,          ,     false,          ,          ,               2],
  // 32-bit formats
  'r32uint':              [        true,        ,        ,          ,      true,          ,          ,               4],
  'r32sint':              [        true,        ,        ,          ,      true,          ,          ,               4],
  'r32float':             [        true,        ,        ,          ,      true,          ,          ,               4],
  'rg16uint':             [        true,        ,        ,          ,     false,          ,          ,               4],
  'rg16sint':             [        true,        ,        ,          ,     false,          ,          ,               4],
  'rg16float':            [        true,        ,        ,          ,     false,          ,          ,               4],
  'rgba8unorm':           [        true,        ,        ,          ,      true,          ,          ,               4],
  'rgba8unorm-srgb':      [        true,        ,        ,          ,     false,          ,          ,               4],
  'rgba8snorm':           [       false,        ,        ,          ,      true,          ,          ,               4],
  'rgba8uint':            [        true,        ,        ,          ,      true,          ,          ,               4],
  'rgba8sint':            [        true,        ,        ,          ,      true,          ,          ,               4],
  'bgra8unorm':           [        true,        ,        ,          ,     false,          ,          ,               4],
  'bgra8unorm-srgb':      [        true,        ,        ,          ,     false,          ,          ,               4],
  // Packed 32-bit formats
  'rgb10a2unorm':         [        true,        ,        ,          ,     false,          ,          ,               4],
  'rg11b10ufloat':        [       false,        ,        ,          ,     false,          ,          ,               4],
  'rgb9e5ufloat':         [       false,        ,        ,          ,     false,          ,          ,               4],
  // 64-bit formats
  'rg32uint':             [        true,        ,        ,          ,      true,          ,          ,               8],
  'rg32sint':             [        true,        ,        ,          ,      true,          ,          ,               8],
  'rg32float':            [        true,        ,        ,          ,      true,          ,          ,               8],
  'rgba16uint':           [        true,        ,        ,          ,      true,          ,          ,               8],
  'rgba16sint':           [        true,        ,        ,          ,      true,          ,          ,               8],
  'rgba16float':          [        true,        ,        ,          ,      true,          ,          ,               8],
  // 128-bit formats
  'rgba32uint':           [        true,        ,        ,          ,      true,          ,          ,              16],
  'rgba32sint':           [        true,        ,        ,          ,      true,          ,          ,              16],
  'rgba32float':          [        true,        ,        ,          ,      true,          ,          ,              16],
} as const);
/* prettier-ignore */
const kTexFmtInfoHeader = ['renderable', 'color', 'depth', 'stencil', 'storage', 'copySrc', 'copyDst', 'bytesPerBlock', 'blockWidth', 'blockHeight',              'extension'] as const;
export const kSizedDepthStencilFormatInfo = /* prettier-ignore */ makeTable(kTexFmtInfoHeader,
                          [        true,   false,        ,          ,     false,          ,          ,                ,            1,             1,                         ] as const, {
  'depth32float':         [        true,   false,    true,     false,          ,     false,     false,               4],
} as const);
export const kUnsizedDepthStencilFormatInfo = /* prettier-ignore */ makeTable(kTexFmtInfoHeader,
                          [        true,   false,        ,          ,     false,          ,          ,       undefined,            1,             1,                         ] as const, {
  'depth24plus':          [            ,        ,    true,     false,          ,     false,     false],
  'depth24plus-stencil8': [            ,        ,    true,      true,          ,     false,     false],
} as const);
export const kCompressedTextureFormatInfo = /* prettier-ignore */ makeTable(kTexFmtInfoHeader,
                          [       false,    true,   false,     false,     false,      true,      true,                ,            4,             4,                         ] as const, {
  'bc1-rgba-unorm':       [            ,        ,        ,          ,          ,          ,          ,               8,            4,             4, 'texture-compression-bc'],
  'bc1-rgba-unorm-srgb':  [            ,        ,        ,          ,          ,          ,          ,               8,            4,             4, 'texture-compression-bc'],
  'bc2-rgba-unorm':       [            ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc2-rgba-unorm-srgb':  [            ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc3-rgba-unorm':       [            ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc3-rgba-unorm-srgb':  [            ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc4-r-unorm':          [            ,        ,        ,          ,          ,          ,          ,               8,            4,             4, 'texture-compression-bc'],
  'bc4-r-snorm':          [            ,        ,        ,          ,          ,          ,          ,               8,            4,             4, 'texture-compression-bc'],
  'bc5-rg-unorm':         [            ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc5-rg-snorm':         [            ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc6h-rgb-ufloat':      [            ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc6h-rgb-float':       [            ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc7-rgba-unorm':       [            ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc7-rgba-unorm-srgb':  [            ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
} as const);

export const kRegularTextureFormats = keysOf(kRegularTextureFormatInfo);
export const kSizedDepthStencilFormats = keysOf(kSizedDepthStencilFormatInfo);
export const kUnsizedDepthStencilFormats = keysOf(kUnsizedDepthStencilFormatInfo);
export const kCompressedTextureFormats = keysOf(kCompressedTextureFormatInfo);

export const kColorTextureFormatInfo = {
  ...kRegularTextureFormatInfo,
  ...kCompressedTextureFormatInfo,
} as const;
export type ColorTextureFormat = keyof typeof kColorTextureFormatInfo;
export const kColorTextureFormats = keysOf(kColorTextureFormatInfo);

export const kEncodableTextureFormatInfo = {
  ...kRegularTextureFormatInfo,
  ...kSizedDepthStencilFormatInfo,
} as const;
export type EncodableTextureFormat = keyof typeof kEncodableTextureFormatInfo;
export const kEncodableTextureFormats = keysOf(kEncodableTextureFormatInfo);

export const kSizedTextureFormatInfo = {
  ...kRegularTextureFormatInfo,
  ...kSizedDepthStencilFormatInfo,
  ...kCompressedTextureFormatInfo,
} as const;
export type SizedTextureFormat = keyof typeof kSizedTextureFormatInfo;
export const kSizedTextureFormats = keysOf(kSizedTextureFormatInfo);

export const kDepthStencilFormatInfo = {
  ...kSizedDepthStencilFormatInfo,
  ...kUnsizedDepthStencilFormatInfo,
} as const;
export type DepthStencilFormat = keyof typeof kDepthStencilFormatInfo;
export const kDepthStencilFormats = keysOf(kDepthStencilFormatInfo);

export const kUncompressedTextureFormatInfo = {
  ...kRegularTextureFormatInfo,
  ...kSizedDepthStencilFormatInfo,
  ...kUnsizedDepthStencilFormatInfo,
} as const;
export type UncompressedTextureFormat = keyof typeof kUncompressedTextureFormatInfo;
export const kUncompressedTextureFormats = keysOf(kUncompressedTextureFormatInfo);

export const kAllTextureFormatInfo = {
  ...kUncompressedTextureFormatInfo,
  ...kCompressedTextureFormatInfo,
} as const;
export const kAllTextureFormats = keysOf(kAllTextureFormatInfo);
// Assert every GPUTextureFormat is covered by one of the tables.
((x: { readonly [k in GPUTextureFormat]: {} }) => x)(kAllTextureFormatInfo);

export const kTextureDimensionInfo: {
  readonly [k in GPUTextureDimension]: {
    // Add fields as needed
  };
} = /* prettier-ignore */ {
  '1d': {},
  '2d': {},
  '3d': {},
};
export const kTextureDimensions = keysOf(kTextureDimensionInfo);

export const kTextureAspectInfo: {
  readonly [k in GPUTextureAspect]: {
    // Add fields as needed
  };
} = /* prettier-ignore */ {
  'all': {},
  'depth-only': {},
  'stencil-only': {},
};
export const kTextureAspects = keysOf(kTextureAspectInfo);

export const kTextureUsageInfo: {
  readonly [k in GPUTextureUsage]: {};
} = {
  [GPUConst.TextureUsage.COPY_SRC]: {},
  [GPUConst.TextureUsage.COPY_DST]: {},
  [GPUConst.TextureUsage.SAMPLED]: {},
  [GPUConst.TextureUsage.STORAGE]: {},
  [GPUConst.TextureUsage.OUTPUT_ATTACHMENT]: {},
};
export const kTextureUsages = numericKeysOf<GPUTextureUsage>(kTextureUsageInfo);

export const kTextureComponentTypeInfo: {
  readonly [k in GPUTextureComponentType]: {
    // Add fields as needed
  };
} = /* prettier-ignore */ {
  'float': {},
  'sint': {},
  'uint': {},
  'depth-comparison': {},
};
export const kTextureComponentTypes = keysOf(kTextureComponentTypeInfo);

// Texture View

export const kTextureViewDimensionInfo: {
  readonly [k in GPUTextureViewDimension]: {
    readonly storage: boolean;
    // Add fields as needed
  };
} = /* prettier-ignore */ {
  '1d':         { storage: true  },
  '2d':         { storage: true  },
  '2d-array':   { storage: true  },
  'cube':       { storage: false },
  'cube-array': { storage: false },
  '3d':         { storage: true  },
};
export const kTextureViewDimensions = keysOf(kTextureViewDimensionInfo);

// Typedefs for bindings

export type PerStageBindingLimitClass =
  | 'uniformBuf'
  | 'storageBuf'
  | 'sampler'
  | 'sampledTex'
  | 'storageTex';
export type PerPipelineBindingLimitClass = PerStageBindingLimitClass;

type ValidBindableResource =
  | 'uniformBuf'
  | 'storageBuf'
  | 'plainSamp'
  | 'compareSamp'
  | 'sampledTex'
  | 'sampledTexMS'
  | 'storageTex';
type ErrorBindableResource = 'errorBuf' | 'errorSamp' | 'errorTex';
export type BindableResource = ValidBindableResource | ErrorBindableResource;

type BufferBindingType = 'uniform-buffer' | 'storage-buffer' | 'readonly-storage-buffer';
type SamplerBindingType = 'sampler' | 'comparison-sampler';
type TextureBindingType =
  | 'sampled-texture'
  | 'multisampled-texture'
  | 'writeonly-storage-texture'
  | 'readonly-storage-texture';

// Bindings

export const kMaxBindingsPerBindGroup = 16;

export const kPerStageBindingLimits: {
  readonly [k in PerStageBindingLimitClass]: {
    readonly class: k;
    readonly max: number;
    // Add fields as needed
  };
} = /* prettier-ignore */ {
  'uniformBuf': { class: 'uniformBuf', max: 12, },
  'storageBuf': { class: 'storageBuf', max:  4, },
  'sampler':    { class: 'sampler',    max: 16, },
  'sampledTex': { class: 'sampledTex', max: 16, },
  'storageTex': { class: 'storageTex', max:  4, },
};

export const kPerPipelineBindingLimits: {
  readonly [k in PerPipelineBindingLimitClass]: {
    readonly class: k;
    readonly maxDynamic: number;
    // Add fields as needed
  };
} = /* prettier-ignore */ {
  'uniformBuf': { class: 'uniformBuf', maxDynamic: 8, },
  'storageBuf': { class: 'storageBuf', maxDynamic: 4, },
  'sampler':    { class: 'sampler',    maxDynamic: 0, },
  'sampledTex': { class: 'sampledTex', maxDynamic: 0, },
  'storageTex': { class: 'storageTex', maxDynamic: 0, },
};

const kBindableResource: {
  readonly [k in BindableResource]: {};
} = /* prettier-ignore */ {
  uniformBuf:   {},
  storageBuf:   {},
  plainSamp:    {},
  compareSamp:  {},
  sampledTex:   {},
  sampledTexMS: {},
  storageTex:   {},
  errorBuf:     {},
  errorSamp:    {},
  errorTex:     {},
};
export const kBindableResources = keysOf(kBindableResource);

interface BindingKindInfo {
  readonly resource: ValidBindableResource;
  readonly perStageLimitClass: typeof kPerStageBindingLimits[PerStageBindingLimitClass];
  readonly perPipelineLimitClass: typeof kPerPipelineBindingLimits[PerPipelineBindingLimitClass];
  // Add fields as needed
}

const kBindingKind: {
  readonly [k in ValidBindableResource]: BindingKindInfo;
} = /* prettier-ignore */ {
  uniformBuf:   { resource: 'uniformBuf',   perStageLimitClass: kPerStageBindingLimits.uniformBuf, perPipelineLimitClass: kPerPipelineBindingLimits.uniformBuf, },
  storageBuf:   { resource: 'storageBuf',   perStageLimitClass: kPerStageBindingLimits.storageBuf, perPipelineLimitClass: kPerPipelineBindingLimits.storageBuf, },
  plainSamp:    { resource: 'plainSamp',    perStageLimitClass: kPerStageBindingLimits.sampler,    perPipelineLimitClass: kPerPipelineBindingLimits.sampler,    },
  compareSamp:  { resource: 'compareSamp',  perStageLimitClass: kPerStageBindingLimits.sampler,    perPipelineLimitClass: kPerPipelineBindingLimits.sampler,    },
  sampledTex:   { resource: 'sampledTex',   perStageLimitClass: kPerStageBindingLimits.sampledTex, perPipelineLimitClass: kPerPipelineBindingLimits.sampledTex, },
  sampledTexMS: { resource: 'sampledTexMS', perStageLimitClass: kPerStageBindingLimits.sampledTex, perPipelineLimitClass: kPerPipelineBindingLimits.sampledTex, },
  storageTex:   { resource: 'storageTex',   perStageLimitClass: kPerStageBindingLimits.storageTex, perPipelineLimitClass: kPerPipelineBindingLimits.storageTex, },
};

// Binding type info

interface BindingTypeInfo extends BindingKindInfo {
  readonly validStages: GPUShaderStageFlags;
  // Add fields as needed
}
const kValidStagesAll = {
  validStages:
    GPUConst.ShaderStage.VERTEX | GPUConst.ShaderStage.FRAGMENT | GPUConst.ShaderStage.COMPUTE,
};
const kValidStagesStorageWrite = {
  validStages: GPUConst.ShaderStage.FRAGMENT | GPUConst.ShaderStage.COMPUTE,
};

export const kBufferBindingTypeInfo: {
  readonly [k in BufferBindingType]: {
    readonly usage: GPUBufferUsage;
    // Add fields as needed
  } & BindingTypeInfo;
} = /* prettier-ignore */ {
  'uniform-buffer':          { usage: GPUConst.BufferUsage.UNIFORM, ...kBindingKind.uniformBuf,  ...kValidStagesAll,          },
  'storage-buffer':          { usage: GPUConst.BufferUsage.STORAGE, ...kBindingKind.storageBuf,  ...kValidStagesStorageWrite, },
  'readonly-storage-buffer': { usage: GPUConst.BufferUsage.STORAGE, ...kBindingKind.storageBuf,  ...kValidStagesAll,          },
};
export const kBufferBindingTypes = keysOf(kBufferBindingTypeInfo);

export const kSamplerBindingTypeInfo: {
  readonly [k in SamplerBindingType]: {
    // Add fields as needed
  } & BindingTypeInfo;
} = /* prettier-ignore */ {
  'sampler':                   { ...kBindingKind.plainSamp,   ...kValidStagesAll,     },
  'comparison-sampler':        { ...kBindingKind.compareSamp, ...kValidStagesAll,     },
};
export const kSamplerBindingTypes = keysOf(kSamplerBindingTypeInfo);

export const kTextureBindingTypeInfo: {
  readonly [k in TextureBindingType]: {
    readonly usage: GPUTextureUsage;
    // Add fields as needed
  } & BindingTypeInfo;
} = /* prettier-ignore */ {
  'sampled-texture':           { usage: GPUConst.TextureUsage.SAMPLED, ...kBindingKind.sampledTex,    ...kValidStagesAll,          },
  'multisampled-texture':      { usage: GPUConst.TextureUsage.SAMPLED, ...kBindingKind.sampledTexMS,  ...kValidStagesAll,          },
  'writeonly-storage-texture': { usage: GPUConst.TextureUsage.STORAGE, ...kBindingKind.storageTex,    ...kValidStagesStorageWrite, },
  'readonly-storage-texture':  { usage: GPUConst.TextureUsage.STORAGE, ...kBindingKind.storageTex,    ...kValidStagesAll,          },
};
export const kTextureBindingTypes = keysOf(kTextureBindingTypeInfo);

// All binding types (merged from above)

export const kBindingTypeInfo: {
  readonly [k in GPUBindingType]: BindingTypeInfo;
} = {
  ...kBufferBindingTypeInfo,
  ...kSamplerBindingTypeInfo,
  ...kTextureBindingTypeInfo,
};
export const kBindingTypes = keysOf(kBindingTypeInfo);

export const kShaderStages: readonly GPUShaderStageFlags[] = [
  GPUConst.ShaderStage.VERTEX,
  GPUConst.ShaderStage.FRAGMENT,
  GPUConst.ShaderStage.COMPUTE,
];
export const kShaderStageCombinations: readonly GPUShaderStageFlags[] = [0, 1, 2, 3, 4, 5, 6, 7];

// TODO: Update with all possible sample counts when defined
// TODO: Switch existing tests to use kTextureSampleCounts
export const kTextureSampleCounts = [1, 4] as const;

// TODO: Update maximum color attachments when defined
export const kMaxColorAttachments = 4;
