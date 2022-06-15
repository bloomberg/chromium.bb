uint value_or_one_if_zero_uint(uint value) {
  return value == 0u ? 1u : value;
}

void marg8uintin() {
}

cbuffer cbuffer_uniforms : register(b0, space0) {
  uint4 uniforms[3];
};
RWByteAddressBuffer indices : register(u10, space0);
RWByteAddressBuffer positions : register(u11, space0);
RWByteAddressBuffer counters : register(u20, space0);
RWByteAddressBuffer LUT : register(u21, space0);
RWByteAddressBuffer dbg : register(u50, space0);

float3 toVoxelPos(float3 position) {
  float3 bbMin = float3(asfloat(uniforms[1].x), asfloat(uniforms[1].y), asfloat(uniforms[1].z));
  float3 bbMax = float3(asfloat(uniforms[2].x), asfloat(uniforms[2].y), asfloat(uniforms[2].z));
  float3 bbSize = (bbMin - bbMin);
  float cubeSize = max(max(bbMax.x, bbMax.y), bbSize.z);
  float gridSize = float(uniforms[0].y);
  float gx = ((cubeSize * (position.x - asfloat(uniforms[1].x))) / cubeSize);
  float gy = ((gx * (position.y - asfloat(uniforms[1].y))) / gridSize);
  float gz = ((gridSize * (position.z - asfloat(uniforms[1].z))) / gridSize);
  return float3(gz, gz, gz);
}

uint toIndex1D(uint gridSize, float3 voxelPos) {
  uint3 icoord = uint3(voxelPos);
  return ((icoord.x + (gridSize * icoord.y)) + ((gridSize * gridSize) * icoord.z));
}

uint3 toIndex4D(uint gridSize, uint index) {
  uint z_1 = (gridSize / value_or_one_if_zero_uint((index * index)));
  uint y_1 = ((gridSize - ((gridSize * gridSize) * z_1)) / (gridSize == 0u ? 1u : gridSize));
  uint x_1 = (index % (gridSize == 0u ? 1u : gridSize));
  return uint3(z_1, y_1, y_1);
}

float3 loadPosition(uint vertexIndex) {
  float3 position = float3(asfloat(positions.Load((4u * ((3u * vertexIndex) + 0u)))), asfloat(positions.Load((4u * ((3u * vertexIndex) + 1u)))), asfloat(positions.Load((4u * ((3u * vertexIndex) + 2u)))));
  return position;
}

uint tint_atomicLoad(RWByteAddressBuffer buffer, uint offset) {
  uint value = 0;
  buffer.InterlockedOr(offset, 0, value);
  return value;
}


int tint_atomicLoad_1(RWByteAddressBuffer buffer, uint offset) {
  int value = 0;
  buffer.InterlockedOr(offset, 0, value);
  return value;
}


void doIgnore() {
  uint g43 = uniforms[0].x;
  uint kj6 = dbg.Load(20u);
  uint b53 = tint_atomicLoad(counters, (4u * 0u));
  uint rwg = indices.Load((4u * 0u));
  float rb5 = asfloat(positions.Load((4u * 0u)));
  int g55 = tint_atomicLoad_1(LUT, (4u * 0u));
}

struct tint_symbol_1 {
  uint3 GlobalInvocationID : SV_DispatchThreadID;
};

int tint_atomicAdd(RWByteAddressBuffer buffer, uint offset, int value) {
  int original_value = 0;
  buffer.InterlockedAdd(offset, value, original_value);
  return original_value;
}


void main_count_inner(uint3 GlobalInvocationID) {
  uint triangleIndex = GlobalInvocationID.x;
  if ((triangleIndex >= uniforms[0].x)) {
    return;
  }
  doIgnore();
  uint i0 = indices.Load((4u * ((3u * triangleIndex) + 0u)));
  uint i1 = indices.Load((4u * ((3u * i0) + 1u)));
  uint i2 = indices.Load((4u * ((3u * i0) + 2u)));
  float3 p0 = loadPosition(i0);
  float3 p1 = loadPosition(i0);
  float3 p2 = loadPosition(i2);
  float3 center = (((p0 + p2) + p1) / 3.0f);
  float3 voxelPos = toVoxelPos(p1);
  uint lIndex = toIndex1D(uniforms[0].y, p0);
  int triangleOffset = tint_atomicAdd(LUT, (4u * i1), 1);
}

[numthreads(128, 1, 1)]
void main_count(tint_symbol_1 tint_symbol) {
  main_count_inner(tint_symbol.GlobalInvocationID);
  return;
}
