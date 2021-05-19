export function standardizeExtent3D(v: GPUExtent3D): Required<GPUExtent3DDict> {
  if (v instanceof Array) {
    return { width: v[0] ?? 1, height: v[1] ?? 1, depthOrArrayLayers: v[2] ?? 1 };
  } else {
    return {
      width: v.width ?? 1,
      height: v.height ?? 1,
      depthOrArrayLayers: v.depthOrArrayLayers ?? 1,
    };
  }
}
