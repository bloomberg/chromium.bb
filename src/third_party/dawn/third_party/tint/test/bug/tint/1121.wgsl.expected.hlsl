uint atomicAdd_1(RWByteAddressBuffer buffer, uint offset, uint value) {
  uint original_value = 0;
  buffer.InterlockedAdd(offset, value, original_value);
  return original_value;
}

RWByteAddressBuffer lightsBuffer : register(u0, space0);

RWByteAddressBuffer tileLightId : register(u0, space1);

cbuffer cbuffer_config : register(b0, space2) {
  uint4 config[2];
};

cbuffer cbuffer_uniforms : register(b0, space3) {
  uint4 uniforms[11];
};

struct tint_symbol_1 {
  uint3 GlobalInvocationID : SV_DispatchThreadID;
};

float4x4 tint_symbol_6(uint4 buffer[11], uint offset) {
  const uint scalar_offset = ((offset + 0u)) / 4;
  const uint scalar_offset_1 = ((offset + 16u)) / 4;
  const uint scalar_offset_2 = ((offset + 32u)) / 4;
  const uint scalar_offset_3 = ((offset + 48u)) / 4;
  return float4x4(asfloat(buffer[scalar_offset / 4]), asfloat(buffer[scalar_offset_1 / 4]), asfloat(buffer[scalar_offset_2 / 4]), asfloat(buffer[scalar_offset_3 / 4]));
}

void main_inner(uint3 GlobalInvocationID) {
  uint index = GlobalInvocationID.x;
  if ((index >= config[0].x)) {
    return;
  }
  lightsBuffer.Store(((32u * index) + 4u), asuint(((asfloat(lightsBuffer.Load(((32u * index) + 4u))) - 0.100000001f) + (0.001f * (float(index) - (64.0f * floor((float(index) / 64.0f))))))));
  if ((asfloat(lightsBuffer.Load(((32u * index) + 4u))) < asfloat(uniforms[0].y))) {
    lightsBuffer.Store(((32u * index) + 4u), asuint(asfloat(uniforms[1].y)));
  }
  float4x4 M = tint_symbol_6(uniforms, 96u);
  float viewNear = (-(M[3][2]) / (-1.0f + M[2][2]));
  float viewFar = (-(M[3][2]) / (1.0f + M[2][2]));
  float4 lightPos = asfloat(lightsBuffer.Load4((32u * index)));
  lightPos = mul(lightPos, tint_symbol_6(uniforms, 32u));
  lightPos = (lightPos / lightPos.w);
  float lightRadius = asfloat(lightsBuffer.Load(((32u * index) + 28u)));
  float4 boxMin = (lightPos - float4(float3((lightRadius).xxx), 0.0f));
  float4 boxMax = (lightPos + float4(float3((lightRadius).xxx), 0.0f));
  float4 frustumPlanes[6] = (float4[6])0;
  frustumPlanes[4] = float4(0.0f, 0.0f, -1.0f, viewNear);
  frustumPlanes[5] = float4(0.0f, 0.0f, 1.0f, -(viewFar));
  const int TILE_SIZE = 16;
  const int TILE_COUNT_X = 2;
  {
    for(int y_1 = 0; (y_1 < 2); y_1 = (y_1 + 1)) {
      {
        for(int x_1 = 0; (x_1 < TILE_COUNT_X); x_1 = (x_1 + 1)) {
          int2 tilePixel0Idx = int2((x_1 * TILE_SIZE), (y_1 * TILE_SIZE));
          float2 floorCoord = (((2.0f * float2(tilePixel0Idx)) / asfloat(uniforms[10]).xy) - float2((1.0f).xx));
          float2 ceilCoord = (((2.0f * float2((tilePixel0Idx + int2((TILE_SIZE).xx)))) / asfloat(uniforms[10]).xy) - float2((1.0f).xx));
          float2 viewFloorCoord = float2((((-(viewNear) * floorCoord.x) - (M[2][0] * viewNear)) / M[0][0]), (((-(viewNear) * floorCoord.y) - (M[2][1] * viewNear)) / M[1][1]));
          float2 viewCeilCoord = float2((((-(viewNear) * ceilCoord.x) - (M[2][0] * viewNear)) / M[0][0]), (((-(viewNear) * ceilCoord.y) - (M[2][1] * viewNear)) / M[1][1]));
          frustumPlanes[0] = float4(1.0f, 0.0f, (-(viewFloorCoord.x) / viewNear), 0.0f);
          frustumPlanes[1] = float4(-1.0f, 0.0f, (viewCeilCoord.x / viewNear), 0.0f);
          frustumPlanes[2] = float4(0.0f, 1.0f, (-(viewFloorCoord.y) / viewNear), 0.0f);
          frustumPlanes[3] = float4(0.0f, -1.0f, (viewCeilCoord.y / viewNear), 0.0f);
          float dp = 0.0f;
          {
            for(uint i = 0u; (i < 6u); i = (i + 1u)) {
              float4 p = float4(0.0f, 0.0f, 0.0f, 0.0f);
              if ((frustumPlanes[i].x > 0.0f)) {
                p.x = boxMax.x;
              } else {
                p.x = boxMin.x;
              }
              if ((frustumPlanes[i].y > 0.0f)) {
                p.y = boxMax.y;
              } else {
                p.y = boxMin.y;
              }
              if ((frustumPlanes[i].z > 0.0f)) {
                p.z = boxMax.z;
              } else {
                p.z = boxMin.z;
              }
              p.w = 1.0f;
              dp = (dp + min(0.0f, dot(p, frustumPlanes[i])));
            }
          }
          if ((dp >= 0.0f)) {
            uint tileId = uint((x_1 + (y_1 * TILE_COUNT_X)));
            bool tint_tmp = (tileId < 0u);
            if (!tint_tmp) {
              tint_tmp = (tileId >= config[0].y);
            }
            if ((tint_tmp)) {
              continue;
            }
            uint offset = atomicAdd_1(tileLightId, (260u * tileId), 1u);
            if ((offset >= config[1].x)) {
              continue;
            }
            tileLightId.Store((((260u * tileId) + 4u) + (4u * offset)), asuint(GlobalInvocationID.x));
          }
        }
      }
    }
  }
}

[numthreads(64, 1, 1)]
void main(tint_symbol_1 tint_symbol) {
  main_inner(tint_symbol.GlobalInvocationID);
  return;
}
