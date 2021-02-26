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

// Buffers

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

export type RegularTextureFormat =
  | 'r8unorm'
  | 'r8snorm'
  | 'r8uint'
  | 'r8sint'
  | 'r16uint'
  | 'r16sint'
  | 'r16float'
  | 'rg8unorm'
  | 'rg8snorm'
  | 'rg8uint'
  | 'rg8sint'
  | 'r32uint'
  | 'r32sint'
  | 'r32float'
  | 'rg16uint'
  | 'rg16sint'
  | 'rg16float'
  | 'rgba8unorm'
  | 'rgba8unorm-srgb'
  | 'rgba8snorm'
  | 'rgba8uint'
  | 'rgba8sint'
  | 'bgra8unorm'
  | 'bgra8unorm-srgb'
  | 'rgb10a2unorm'
  | 'rg11b10float'
  | 'rg32uint'
  | 'rg32sint'
  | 'rg32float'
  | 'rgba16uint'
  | 'rgba16sint'
  | 'rgba16float'
  | 'rgba32uint'
  | 'rgba32sint'
  | 'rgba32float';
export type SizedDepthStencilFormat = 'depth32float';
export type UnsizedDepthStencilFormat = 'depth24plus' | 'depth24plus-stencil8';
export type CompressedTextureFormat =
  | 'bc1-rgba-unorm'
  | 'bc1-rgba-unorm-srgb'
  | 'bc2-rgba-unorm'
  | 'bc2-rgba-unorm-srgb'
  | 'bc3-rgba-unorm'
  | 'bc3-rgba-unorm-srgb'
  | 'bc4-r-unorm'
  | 'bc4-r-snorm'
  | 'bc5-rg-unorm'
  | 'bc5-rg-snorm'
  | 'bc6h-rgb-ufloat'
  | 'bc6h-rgb-sfloat'
  | 'bc7-rgba-unorm'
  | 'bc7-rgba-unorm-srgb';

type TextureFormatInfo = {
  readonly renderable: boolean;
  readonly color: boolean;
  readonly depth: boolean;
  readonly stencil: boolean;
  readonly storage: boolean;
  readonly copySrc: boolean;
  readonly copyDst: boolean;
  readonly bytesPerBlock?: number;
  readonly blockWidth: number;
  readonly blockHeight: number;
  readonly extension?: GPUExtensionName;
  // Add fields as needed
};

export const kRegularTextureFormatInfo: {
  readonly [k in RegularTextureFormat]: {
    color: true;
    bytesPerBlock: number;
    blockWidth: 1;
    blockHeight: 1;
  } & TextureFormatInfo;
} = /* prettier-ignore */ {
  // 8-bit formats
  'r8unorm':                { renderable:  true, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  1, blockWidth: 1, blockHeight: 1 },
  'r8snorm':                { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  1, blockWidth: 1, blockHeight: 1 },
  'r8uint':                 { renderable:  true, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  1, blockWidth: 1, blockHeight: 1 },
  'r8sint':                 { renderable:  true, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  1, blockWidth: 1, blockHeight: 1 },
  // 16-bit formats
  'r16uint':                { renderable:  true, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  2, blockWidth: 1, blockHeight: 1 },
  'r16sint':                { renderable:  true, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  2, blockWidth: 1, blockHeight: 1 },
  'r16float':               { renderable:  true, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  2, blockWidth: 1, blockHeight: 1 },
  'rg8unorm':               { renderable:  true, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  2, blockWidth: 1, blockHeight: 1 },
  'rg8snorm':               { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  2, blockWidth: 1, blockHeight: 1 },
  'rg8uint':                { renderable:  true, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  2, blockWidth: 1, blockHeight: 1 },
  'rg8sint':                { renderable:  true, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  2, blockWidth: 1, blockHeight: 1 },
  // 32-bit formats
  'r32uint':                { renderable:  true, color:  true, depth: false, stencil: false, storage:  true, copySrc:  true, copyDst:  true, bytesPerBlock:  4, blockWidth: 1, blockHeight: 1 },
  'r32sint':                { renderable:  true, color:  true, depth: false, stencil: false, storage:  true, copySrc:  true, copyDst:  true, bytesPerBlock:  4, blockWidth: 1, blockHeight: 1 },
  'r32float':               { renderable:  true, color:  true, depth: false, stencil: false, storage:  true, copySrc:  true, copyDst:  true, bytesPerBlock:  4, blockWidth: 1, blockHeight: 1 },
  'rg16uint':               { renderable:  true, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  4, blockWidth: 1, blockHeight: 1 },
  'rg16sint':               { renderable:  true, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  4, blockWidth: 1, blockHeight: 1 },
  'rg16float':              { renderable:  true, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  4, blockWidth: 1, blockHeight: 1 },
  'rgba8unorm':             { renderable:  true, color:  true, depth: false, stencil: false, storage:  true, copySrc:  true, copyDst:  true, bytesPerBlock:  4, blockWidth: 1, blockHeight: 1 },
  'rgba8unorm-srgb':        { renderable:  true, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  4, blockWidth: 1, blockHeight: 1 },
  'rgba8snorm':             { renderable: false, color:  true, depth: false, stencil: false, storage:  true, copySrc:  true, copyDst:  true, bytesPerBlock:  4, blockWidth: 1, blockHeight: 1 },
  'rgba8uint':              { renderable:  true, color:  true, depth: false, stencil: false, storage:  true, copySrc:  true, copyDst:  true, bytesPerBlock:  4, blockWidth: 1, blockHeight: 1 },
  'rgba8sint':              { renderable:  true, color:  true, depth: false, stencil: false, storage:  true, copySrc:  true, copyDst:  true, bytesPerBlock:  4, blockWidth: 1, blockHeight: 1 },
  'bgra8unorm':             { renderable:  true, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  4, blockWidth: 1, blockHeight: 1 },
  'bgra8unorm-srgb':        { renderable:  true, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  4, blockWidth: 1, blockHeight: 1 },
  // Packed 32-bit formats
  'rgb10a2unorm':           { renderable:  true, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  4, blockWidth: 1, blockHeight: 1 },
  'rg11b10float':           { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  4, blockWidth: 1, blockHeight: 1 },
  // 64-bit formats
  'rg32uint':               { renderable:  true, color:  true, depth: false, stencil: false, storage:  true, copySrc:  true, copyDst:  true, bytesPerBlock:  8, blockWidth: 1, blockHeight: 1 },
  'rg32sint':               { renderable:  true, color:  true, depth: false, stencil: false, storage:  true, copySrc:  true, copyDst:  true, bytesPerBlock:  8, blockWidth: 1, blockHeight: 1 },
  'rg32float':              { renderable:  true, color:  true, depth: false, stencil: false, storage:  true, copySrc:  true, copyDst:  true, bytesPerBlock:  8, blockWidth: 1, blockHeight: 1 },
  'rgba16uint':             { renderable:  true, color:  true, depth: false, stencil: false, storage:  true, copySrc:  true, copyDst:  true, bytesPerBlock:  8, blockWidth: 1, blockHeight: 1 },
  'rgba16sint':             { renderable:  true, color:  true, depth: false, stencil: false, storage:  true, copySrc:  true, copyDst:  true, bytesPerBlock:  8, blockWidth: 1, blockHeight: 1 },
  'rgba16float':            { renderable:  true, color:  true, depth: false, stencil: false, storage:  true, copySrc:  true, copyDst:  true, bytesPerBlock:  8, blockWidth: 1, blockHeight: 1 },
  // 128-bit formats
  'rgba32uint':             { renderable:  true, color:  true, depth: false, stencil: false, storage:  true, copySrc:  true, copyDst:  true, bytesPerBlock: 16, blockWidth: 1, blockHeight: 1 },
  'rgba32sint':             { renderable:  true, color:  true, depth: false, stencil: false, storage:  true, copySrc:  true, copyDst:  true, bytesPerBlock: 16, blockWidth: 1, blockHeight: 1 },
  'rgba32float':            { renderable:  true, color:  true, depth: false, stencil: false, storage:  true, copySrc:  true, copyDst:  true, bytesPerBlock: 16, blockWidth: 1, blockHeight: 1 },
} as const;
export const kRegularTextureFormats = keysOf(kRegularTextureFormatInfo);

export const kSizedDepthStencilFormatInfo: {
  readonly [k in SizedDepthStencilFormat]: {
    readonly renderable: true;
    readonly color: false;
    readonly bytesPerBlock: number;
    readonly blockWidth: 1;
    readonly blockHeight: 1;
  } & TextureFormatInfo;
} = /* prettier-ignore */ {
  'depth32float': { renderable:  true, color: false, depth:  true, stencil: false, storage: false, copySrc:  true, copyDst: false, bytesPerBlock:  4, blockWidth: 1, blockHeight: 1 },
};
export const kSizedDepthStencilFormats = keysOf(kSizedDepthStencilFormatInfo);

export const kUnsizedDepthStencilFormatInfo: {
  readonly [k in UnsizedDepthStencilFormat]: {
    readonly renderable: true;
    readonly color: false;
    readonly blockWidth: 1;
    readonly blockHeight: 1;
  } & TextureFormatInfo;
} = /* prettier-ignore */ {
  'depth24plus':          { renderable:  true, color: false, depth:  true, stencil: false, storage: false, copySrc: false, copyDst: false, blockWidth: 1, blockHeight: 1 },
  'depth24plus-stencil8': { renderable:  true, color: false, depth:  true, stencil:  true, storage: false, copySrc: false, copyDst: false, blockWidth: 1, blockHeight: 1 },
} as const;
export const kUnsizedDepthStencilFormats = keysOf(kUnsizedDepthStencilFormatInfo);

export const kCompressedTextureFormatInfo: {
  readonly [k in CompressedTextureFormat]: {
    readonly renderable: false;
    readonly color: true;
    readonly bytesPerBlock: number;
    readonly extension: GPUExtensionName;
  } & TextureFormatInfo;
} = /* prettier-ignore */ {
  // BC formats
  'bc1-rgba-unorm':      { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  8, blockWidth: 4, blockHeight: 4, extension: 'texture-compression-bc' },
  'bc1-rgba-unorm-srgb': { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  8, blockWidth: 4, blockHeight: 4, extension: 'texture-compression-bc' },
  'bc2-rgba-unorm':      { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock: 16, blockWidth: 4, blockHeight: 4, extension: 'texture-compression-bc' },
  'bc2-rgba-unorm-srgb': { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock: 16, blockWidth: 4, blockHeight: 4, extension: 'texture-compression-bc' },
  'bc3-rgba-unorm':      { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock: 16, blockWidth: 4, blockHeight: 4, extension: 'texture-compression-bc' },
  'bc3-rgba-unorm-srgb': { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock: 16, blockWidth: 4, blockHeight: 4, extension: 'texture-compression-bc' },
  'bc4-r-unorm':         { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  8, blockWidth: 4, blockHeight: 4, extension: 'texture-compression-bc' },
  'bc4-r-snorm':         { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock:  8, blockWidth: 4, blockHeight: 4, extension: 'texture-compression-bc' },
  'bc5-rg-unorm':        { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock: 16, blockWidth: 4, blockHeight: 4, extension: 'texture-compression-bc' },
  'bc5-rg-snorm':        { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock: 16, blockWidth: 4, blockHeight: 4, extension: 'texture-compression-bc' },
  'bc6h-rgb-ufloat':     { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock: 16, blockWidth: 4, blockHeight: 4, extension: 'texture-compression-bc' },
  'bc6h-rgb-sfloat':     { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock: 16, blockWidth: 4, blockHeight: 4, extension: 'texture-compression-bc' },
  'bc7-rgba-unorm':      { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock: 16, blockWidth: 4, blockHeight: 4, extension: 'texture-compression-bc' },
  'bc7-rgba-unorm-srgb': { renderable: false, color:  true, depth: false, stencil: false, storage: false, copySrc:  true, copyDst:  true, bytesPerBlock: 16, blockWidth: 4, blockHeight: 4, extension: 'texture-compression-bc' },
};
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

export const kAllTextureFormatInfo: {
  readonly [k in GPUTextureFormat]: TextureFormatInfo;
} = {
  ...kUncompressedTextureFormatInfo,
  ...kCompressedTextureFormatInfo,
} as const;
export const kAllTextureFormats = keysOf(kAllTextureFormatInfo);

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
  | 'storageTex';
type ErrorBindableResource = 'errorBuf' | 'errorSamp' | 'errorTex';
export type BindableResource = ValidBindableResource | ErrorBindableResource;

type BufferBindingType = 'uniform-buffer' | 'storage-buffer' | 'readonly-storage-buffer';
type SamplerBindingType = 'sampler' | 'comparison-sampler';
type TextureBindingType =
  | 'sampled-texture'
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
  uniformBuf: {}, storageBuf: {}, plainSamp: {}, compareSamp: {}, sampledTex: {}, storageTex: {},
  errorBuf: {}, errorSamp: {}, errorTex: {},
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
  uniformBuf:  { resource: 'uniformBuf',  perStageLimitClass: kPerStageBindingLimits.uniformBuf, perPipelineLimitClass: kPerPipelineBindingLimits.uniformBuf, },
  storageBuf:  { resource: 'storageBuf',  perStageLimitClass: kPerStageBindingLimits.storageBuf, perPipelineLimitClass: kPerPipelineBindingLimits.storageBuf, },
  plainSamp:   { resource: 'plainSamp',   perStageLimitClass: kPerStageBindingLimits.sampler,    perPipelineLimitClass: kPerPipelineBindingLimits.sampler,    },
  compareSamp: { resource: 'compareSamp', perStageLimitClass: kPerStageBindingLimits.sampler,    perPipelineLimitClass: kPerPipelineBindingLimits.sampler,    },
  sampledTex:  { resource: 'sampledTex',  perStageLimitClass: kPerStageBindingLimits.sampledTex, perPipelineLimitClass: kPerPipelineBindingLimits.sampledTex, },
  storageTex:  { resource: 'storageTex',  perStageLimitClass: kPerStageBindingLimits.storageTex, perPipelineLimitClass: kPerPipelineBindingLimits.storageTex, },
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
  'sampled-texture':           { usage: GPUConst.TextureUsage.SAMPLED, ...kBindingKind.sampledTex,  ...kValidStagesAll,          },
  'writeonly-storage-texture': { usage: GPUConst.TextureUsage.STORAGE, ...kBindingKind.storageTex,  ...kValidStagesStorageWrite, },
  'readonly-storage-texture':  { usage: GPUConst.TextureUsage.STORAGE, ...kBindingKind.storageTex,  ...kValidStagesAll,          },
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
