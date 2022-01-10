// Copyright 2021 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tests/perf_tests/DawnPerfTest.h"

#include "utils/WGPUHelpers.h"

namespace {
    constexpr uint32_t kTileSize = 32u;

    const std::string& kMatMulFloatHeader = R"(
        [[block]] struct Uniforms {
            dimAOuter : u32;
            dimInner : u32;
            dimBOuter : u32;
        };
        [[block]] struct Matrix {
            numbers: array<f32>;
        };

        [[group(0), binding(0)]] var<storage, read> firstMatrix : Matrix;
        [[group(0), binding(1)]] var<storage, read> secondMatrix : Matrix;
        [[group(0), binding(2)]] var<storage, write> resultMatrix : Matrix;
        [[group(0), binding(3)]] var<uniform> uniforms : Uniforms;

        fn mm_readA(row : u32, col : u32) -> f32  {
            if (row < uniforms.dimAOuter && col < uniforms.dimInner)
            {
                let result : f32 = firstMatrix.numbers[row * uniforms.dimInner + col];
                return result;
            }
            return 0.;
        }

        fn mm_readB(row : u32, col : u32) -> f32 {
            if (row < uniforms.dimInner && col < uniforms.dimBOuter)
            {
                let result : f32 = secondMatrix.numbers[row * uniforms.dimBOuter + col];
                return result;
            }
            return 0.;
        }

        fn mm_write(row : u32, col : u32, value : f32) {
            if (row < uniforms.dimAOuter && col < uniforms.dimBOuter)
            {
                let index : u32 = col + row * uniforms.dimBOuter;
                resultMatrix.numbers[index] = value;
            }
        }

        let RowPerThread : u32 = 4u;
        let ColPerThread : u32 = 4u;
        let TileAOuter : u32 = 32u;
        let TileBOuter : u32 = 32u;
        let TileInner : u32 = 32u;)";

    const std::string& kMatMulFloatSharedArray1D = R"(
        var<workgroup> mm_Asub : array<f32, 1024>;
        var<workgroup> mm_Bsub : array<f32, 1024>;)";
    const std::string& kMatMulFloatSharedArray2D = R"(
        var<workgroup> mm_Asub : array<array<f32, 32>, 32>;
        var<workgroup> mm_Bsub : array<array<f32, 32>, 32>;)";
    const std::string& kMatMulFloatBodyPart1 = R"(
        [[stage(compute), workgroup_size(8, 8, 1)]]
        fn main([[builtin(local_invocation_id)]] local_id : vec3<u32>,
                [[builtin(global_invocation_id)]] global_id  : vec3<u32>) {
            let tileRow : u32 = local_id.y * RowPerThread;
            let tileCol : u32 = local_id.x * ColPerThread;

            let globalRow : u32 = global_id.y * RowPerThread;
            let globalCol : u32 = global_id.x * ColPerThread;

            let numTiles : u32 = (uniforms.dimInner - 1u) / TileInner + 1u;

            var acc: array<f32, 16>;
            var ACached : f32;
            var BCached : array<f32, 4>;

            // Without this initialization strange values show up in acc.
            // TODO: Remove it once the following bug is fixed.
            // https://bugs.chromium.org/p/tint/issues/detail?id=759
            for (var index : u32 = 0u; index < RowPerThread * ColPerThread; index = index + 1u) {
                acc[index] = 0.;
            }

            let ColPerThreadA : u32 = TileInner / 8u;
            let tileColA : u32 = local_id.x * ColPerThreadA;
            let RowPerThreadB : u32 = TileInner / 8u;
            let tileRowB : u32 = local_id.y * RowPerThreadB;

            // Loop over shared dimension.
            for (var t : u32 = 0u; t < numTiles; t = t + 1u) {
                // Load one tile of A into local memory.
                for (var innerRow : u32 = 0u; innerRow < RowPerThread; innerRow = innerRow + 1u) {
                for (var innerCol : u32 = 0u; innerCol < ColPerThreadA; innerCol = innerCol + 1u) {
                    let inputRow : u32 = tileRow + innerRow;
                    let inputCol : u32 = tileColA + innerCol;)";
    const std::string& kMatMulFloatBodyPart2Array1D = R"(
                    let index : u32 = inputRow * TileInner + inputCol;
                    mm_Asub[index] = mm_readA(globalRow + innerRow, t * TileInner + inputCol);
                }
                }
                // Load one tile of B into local memory.
                for (var innerRow : u32 = 0u; innerRow < RowPerThreadB; innerRow = innerRow + 1u) {
                for (var innerCol : u32 = 0u; innerCol < ColPerThread; innerCol = innerCol + 1u) {
                    let inputRow : u32 = tileRowB + innerRow;
                    let inputCol : u32 = tileCol + innerCol;
                    let index : u32 = inputRow * TileBOuter + inputCol;

                    mm_Bsub[index] = mm_readB(t * TileInner + inputRow, globalCol + innerCol);;
                }
                }

                workgroupBarrier();

                // Compute acc values for a single thread.
                for (var k : u32 = 0u; k < TileInner; k = k + 1u) {
                    for (var inner : u32 = 0u; inner < ColPerThread; inner = inner + 1u) {
                        BCached[inner] = mm_Bsub[k * TileBOuter + tileCol + inner];
                    }

                    for (var innerRow : u32 = 0u; innerRow < RowPerThread; innerRow = innerRow + 1u) {
                        ACached = mm_Asub[(tileRow + innerRow) * TileInner + k];)";
    const std::string& kMatMulFloatBodyPart2Array2D = R"(
                    mm_Asub[inputRow][inputCol] = mm_readA(globalRow + innerRow, t * TileInner + inputCol);
                }
                }
                // Load one tile of B into local memory.
                for (var innerRow : u32 = 0u; innerRow < RowPerThreadB; innerRow = innerRow + 1u) {
                for (var innerCol : u32 = 0u; innerCol < ColPerThread; innerCol = innerCol + 1u) {
                    let inputRow : u32 = tileRowB + innerRow;
                    let inputCol : u32 = tileCol + innerCol;

                    mm_Bsub[innerCol][inputCol] = mm_readB(t * TileInner + inputRow, globalCol + innerCol);;
                }
                }

                workgroupBarrier();

                // Compute acc values for a single thread.
                for (var k : u32 = 0u; k < TileInner; k = k + 1u) {
                    for (var inner : u32 = 0u; inner < ColPerThread; inner = inner + 1u) {
                        BCached[inner] = mm_Bsub[k][tileCol + inner];
                    }

                    for (var innerRow : u32 = 0u; innerRow < RowPerThread; innerRow = innerRow + 1u) {
                        ACached = mm_Asub[tileRow + innerRow][k];)";
    const std::string& kMatMulFloatBodyPart3 = R"(
                        for (var innerCol : u32 = 0u; innerCol < ColPerThread; innerCol = innerCol + 1u) {
                            let index : u32 = innerRow * ColPerThread + innerCol;
                            acc[index] = acc[index] + ACached * BCached[innerCol];
                        }
                    }
                }

                workgroupBarrier();
            }

            for (var innerRow : u32 = 0u; innerRow < RowPerThread; innerRow = innerRow + 1u) {
            for (var innerCol : u32 = 0u; innerCol < ColPerThread; innerCol = innerCol + 1u) {
                let index : u32 = innerRow * ColPerThread + innerCol;
                mm_write(globalRow + innerRow,
                         globalCol + innerCol,
                         acc[index]);
            }
            }
        })";
    const std::string& kMatMulFloatOneDimensionalSharedArray =
        kMatMulFloatHeader + kMatMulFloatSharedArray1D + kMatMulFloatBodyPart1 +
        kMatMulFloatBodyPart2Array1D + kMatMulFloatBodyPart3;

    const std::string& kMatMulFloatTwoDimensionalSharedArray =
        kMatMulFloatHeader + kMatMulFloatSharedArray2D + kMatMulFloatBodyPart1 +
        kMatMulFloatBodyPart2Array2D + kMatMulFloatBodyPart3;

    // The vec4 version requires that dimInner and dimBOuter are divisible by 4.
    const std::string& kMatMulVec4Header = R"(
        [[block]] struct Uniforms {
            dimAOuter : u32;
            dimInner : u32;
            dimBOuter : u32;
        };
        [[block]] struct Matrix {
            numbers: array<vec4<f32>>;
        };

        [[group(0), binding(0)]] var<storage, read> firstMatrix : Matrix;
        [[group(0), binding(1)]] var<storage, read> secondMatrix : Matrix;
        [[group(0), binding(2)]] var<storage, write> resultMatrix : Matrix;
        [[group(0), binding(3)]] var<uniform> uniforms : Uniforms;

        fn mm_readA(row : u32, col : u32) -> vec4<f32>  {
            if (row < uniforms.dimAOuter && col < uniforms.dimInner)
            {
                let result : vec4<f32> = firstMatrix.numbers[row * uniforms.dimInner / 4u + col];
                return result;
            }
            return vec4<f32>(0.0, 0.0, 0.0, 0.0);
        }

        fn mm_readB(row : u32, col : u32) -> vec4<f32> {
            if (row < uniforms.dimInner && col < uniforms.dimBOuter)
            {
                let result : vec4<f32> = secondMatrix.numbers[row * uniforms.dimBOuter / 4u + col];
                return result;
            }
            return vec4<f32>(0.0, 0.0, 0.0, 0.0);
        }

        fn mm_write(row : u32, col : u32, value : vec4<f32>) {
            if (row < uniforms.dimAOuter && col < uniforms.dimBOuter)
            {
                let index : u32 = col + row * uniforms.dimBOuter / 4u;
                resultMatrix.numbers[index] = value;
            }
        }

        let RowPerThread : u32 = 4u;
        let ColPerThread : u32 = 4u;
        let TileOuter : u32 = 32u;
        let TileInner : u32 = 32u;)";
    const std::string& kMatMulVec4SharedArray1D = R"(
        var<workgroup> mm_Asub : array<vec4<f32>, 256>;
        var<workgroup> mm_Bsub : array<vec4<f32>, 256>;)";
    const std::string& kMatMulVec4SharedArray2D = R"(
        var<workgroup> mm_Asub : array<array<vec4<f32>, 8>, 32>;
        var<workgroup> mm_Bsub : array<array<vec4<f32>, 8>, 32>;)";
    const std::string& kMatMulVec4BodyPart1 = R"(
        [[stage(compute), workgroup_size(8, 8, 1)]]
        fn main([[builtin(local_invocation_id)]] local_id : vec3<u32>,
                [[builtin(global_invocation_id)]] global_id  : vec3<u32>) {
            let tileRow : u32 = local_id.y * RowPerThread;
            let tileCol : u32 = local_id.x;

            let globalRow : u32 = global_id.y * RowPerThread;
            let globalCol : u32 = global_id.x;

            let numTiles : u32 = (uniforms.dimInner - 1u) / TileInner + 1u;

            var acc: array<vec4<f32>, 4>;
            var ACached : vec4<f32>;
            var BCached : array<vec4<f32>, 4>;

            // Without this initialization strange values show up in acc.
            // TODO: Remove it once the following bug is fixed.
            // https://bugs.chromium.org/p/tint/issues/detail?id=759
            for (var index : u32 = 0u; index < RowPerThread; index = index + 1u) {
                acc[index] = vec4<f32>(0.0, 0.0, 0.0, 0.0);
            }

            var globalColA : u32 = tileCol;
            let RowPerThreadB : u32 = TileInner / 8u;
            let tileRowB : u32 = local_id.y * RowPerThreadB;

            // Loop over shared dimension.
            for (var t : u32 = 0u; t < numTiles; t = t + 1u) {
                // Load one tile of A into local memory.
                for (var innerRow : u32 = 0u; innerRow < RowPerThread; innerRow = innerRow + 1u) {
                    let inputRow : u32 = tileRow + innerRow;
                    let inputCol : u32 = tileCol;)";
    const std::string& kMatMulVec4BodyPart2Array1D = R"(
                    let index : u32 = inputRow * TileInner / ColPerThread + inputCol;
                    mm_Asub[index] = mm_readA(globalRow + innerRow, globalColA);
                }
                globalColA = globalColA + TileInner / ColPerThread;

                // Load one tile of B into local memory.
                for (var innerRow : u32 = 0u; innerRow < RowPerThreadB; innerRow = innerRow + 1u) {
                    let inputRow : u32 = tileRowB + innerRow;
                    let inputCol : u32 = tileCol;
                    let index : u32 = inputRow * TileOuter / ColPerThread + inputCol;
                    mm_Bsub[index] = mm_readB(t * TileInner + inputRow, globalCol);;
                }

                workgroupBarrier();

                // Compute acc values for a single thread.
                for (var k : u32 = 0u; k < TileInner / ColPerThread; k = k + 1u) {
                    BCached[0] = mm_Bsub[(k * ColPerThread) * (TileOuter / ColPerThread) + tileCol];
                    BCached[1] = mm_Bsub[(k * ColPerThread + 1u) * (TileOuter / ColPerThread) + tileCol];
                    BCached[2] = mm_Bsub[(k * ColPerThread + 2u) * (TileOuter / ColPerThread) + tileCol];
                    BCached[3] = mm_Bsub[(k * ColPerThread + 3u) * (TileOuter / ColPerThread) + tileCol];

                    for (var i : u32 = 0u; i < RowPerThread; i = i + 1u) {
                        ACached = mm_Asub[(tileRow + i) * (TileInner / ColPerThread) + k];)";
    const std::string& kMatMulVec4BodyPart2Array2D = R"(
                    mm_Asub[inputRow][inputCol] = mm_readA(globalRow + innerRow, globalColA);
                }
                globalColA = globalColA + TileInner / ColPerThread;

                // Load one tile of B into local memory.
                for (var innerRow : u32 = 0u; innerRow < RowPerThreadB; innerRow = innerRow + 1u) {
                    let inputRow : u32 = tileRowB + innerRow;
                    let inputCol : u32 = tileCol;
                    mm_Bsub[inputRow][inputCol] = mm_readB(t * TileInner + inputRow, globalCol);;
                }

                workgroupBarrier();

                // Compute acc values for a single thread.
                for (var k : u32 = 0u; k < TileInner / ColPerThread; k = k + 1u) {
                    BCached[0] = mm_Bsub[k * ColPerThread][tileCol];
                    BCached[1] = mm_Bsub[k * ColPerThread + 1u][tileCol];
                    BCached[2] = mm_Bsub[k * ColPerThread + 2u][tileCol];
                    BCached[3] = mm_Bsub[k * ColPerThread + 3u][tileCol];

                    for (var i : u32 = 0u; i < RowPerThread; i = i + 1u) {
                        ACached = mm_Asub[tileRow + i][k];)";
    const std::string& kMatMulVec4BodyPart3 = R"(
                        acc[i] = BCached[0] * ACached.x + acc[i];
                        acc[i] = BCached[1] * ACached.y + acc[i];
                        acc[i] = BCached[2] * ACached.z + acc[i];
                        acc[i] = BCached[3] * ACached.w + acc[i];
                    }
                }

                workgroupBarrier();
            }

            for (var innerRow : u32 = 0u; innerRow < RowPerThread; innerRow = innerRow + 1u) {
                mm_write(globalRow + innerRow,
                         globalCol,
                         acc[innerRow]);
            }
        })";

    const std::string& kMatMulVec4OneDimensionalSharedArray =
        kMatMulVec4Header + kMatMulVec4SharedArray1D + kMatMulVec4BodyPart1 +
        kMatMulVec4BodyPart2Array1D + kMatMulVec4BodyPart3;

    const std::string& kMatMulVec4TwoDimensionalSharedArray =
        kMatMulVec4Header + kMatMulVec4SharedArray2D + kMatMulVec4BodyPart1 +
        kMatMulVec4BodyPart2Array2D + kMatMulVec4BodyPart3;

    constexpr unsigned int kNumIterations = 50;

    enum class MatMulMethod {
        MatMulFloatOneDimSharedArray,
        MatMulFloatTwoDimSharedArray,
        MatMulVec4OneDimSharedArray,
        MatMulVec4TwoDimSharedArray
    };

    std::ostream& operator<<(std::ostream& ostream, const MatMulMethod& matMulMethod) {
        switch (matMulMethod) {
            case MatMulMethod::MatMulFloatOneDimSharedArray:
                ostream << "MatMulFloatOneDimSharedArray";
                break;
            case MatMulMethod::MatMulFloatTwoDimSharedArray:
                ostream << "MatMulFloatTwoDimSharedArray";
                break;
            case MatMulMethod::MatMulVec4OneDimSharedArray:
                ostream << "MatMulVec4OneDimSharedArray";
                break;
            case MatMulMethod::MatMulVec4TwoDimSharedArray:
                ostream << "MatMulVec4TwoDimSharedArray";
                break;
        }
        return ostream;
    }

    using DimAOuter = uint32_t;
    using DimInner = uint32_t;
    using DimBOuter = uint32_t;
    DAWN_TEST_PARAM_STRUCT(ShaderRobustnessParams, MatMulMethod, DimAOuter, DimInner, DimBOuter);

}  // namespace

// Test the execution time of matrix multiplication (A [dimAOuter, dimInner] * B [dimInner,
// dimBOuter]) on the GPU and see the difference between robustness on and off.
class ShaderRobustnessPerf : public DawnPerfTestWithParams<ShaderRobustnessParams> {
  public:
    ShaderRobustnessPerf()
        : DawnPerfTestWithParams(kNumIterations, 1),
          mDimAOuter(GetParam().mDimAOuter),
          mDimInner(GetParam().mDimInner),
          mDimBOuter(GetParam().mDimBOuter) {
    }
    ~ShaderRobustnessPerf() override = default;

    void SetUp() override;

  private:
    void Step() override;

    wgpu::BindGroup mBindGroup;
    wgpu::ComputePipeline mPipeline;
    uint32_t mDimAOuter;
    uint32_t mDimInner;
    uint32_t mDimBOuter;
};

void ShaderRobustnessPerf::SetUp() {
    DawnPerfTestWithParams<ShaderRobustnessParams>::SetUp();

    // TODO(crbug.com/dawn/786): D3D12_Microsoft_Basic_Render_Driver_CPU
    DAWN_SUPPRESS_TEST_IF(IsD3D12() && IsWARP());

    // TODO(crbug.com/dawn/945): Generation via SPIRV-Cross fails
    DAWN_SUPPRESS_TEST_IF(IsOpenGL());

    const size_t dataASize = mDimAOuter * mDimInner;
    std::vector<float> dataA(dataASize);
    uint64_t byteASize = sizeof(float) * dataA.size();
    // It's ok to use all zeros to do the matrix multiplication for performance test.
    wgpu::Buffer bufA =
        utils::CreateBufferFromData(device, dataA.data(), byteASize, wgpu::BufferUsage::Storage);

    const size_t dataBSize = mDimInner * mDimBOuter;
    std::vector<float> dataB(dataBSize);
    uint64_t byteBSize = sizeof(float) * dataB.size();
    wgpu::Buffer bufB =
        utils::CreateBufferFromData(device, dataB.data(), byteBSize, wgpu::BufferUsage::Storage);

    uint64_t byteDstSize = sizeof(float) * mDimAOuter * mDimBOuter;
    wgpu::BufferDescriptor desc = {};
    desc.usage = wgpu::BufferUsage::Storage;
    desc.size = byteDstSize;
    wgpu::Buffer dst = device.CreateBuffer(&desc);

    uint32_t uniformData[] = {mDimAOuter, mDimInner, mDimBOuter};
    wgpu::Buffer uniformBuffer = utils::CreateBufferFromData(
        device, uniformData, sizeof(uniformData), wgpu::BufferUsage::Uniform);

    wgpu::ShaderModule module;
    switch (GetParam().mMatMulMethod) {
        case MatMulMethod::MatMulFloatOneDimSharedArray: {
            module =
                utils::CreateShaderModule(device, kMatMulFloatOneDimensionalSharedArray.c_str());
            break;
        }

        case MatMulMethod::MatMulFloatTwoDimSharedArray: {
            module =
                utils::CreateShaderModule(device, kMatMulFloatTwoDimensionalSharedArray.c_str());
            break;
        }

        case MatMulMethod::MatMulVec4OneDimSharedArray: {
            module =
                utils::CreateShaderModule(device, kMatMulVec4OneDimensionalSharedArray.c_str());
            break;
        }

        case MatMulMethod::MatMulVec4TwoDimSharedArray: {
            module =
                utils::CreateShaderModule(device, kMatMulVec4TwoDimensionalSharedArray.c_str());
            break;
        }
    }

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.compute.module = module;
    csDesc.compute.entryPoint = "main";
    mPipeline = device.CreateComputePipeline(&csDesc);

    mBindGroup = utils::MakeBindGroup(device, mPipeline.GetBindGroupLayout(0),
                                      {
                                          {0, bufA, 0, byteASize},
                                          {1, bufB, 0, byteBSize},
                                          {2, dst, 0, byteDstSize},
                                          {3, uniformBuffer, 0, sizeof(uniformData)},
                                      });
}

void ShaderRobustnessPerf::Step() {
    wgpu::CommandBuffer commands;
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(mPipeline);
        pass.SetBindGroup(0, mBindGroup);
        for (unsigned int i = 0; i < kNumIterations; ++i) {
            pass.Dispatch(ceil(float(mDimBOuter) / float(kTileSize)),
                          ceil(float(mDimAOuter) / float(kTileSize)), 1);
        }
        pass.EndPass();

        commands = encoder.Finish();
    }

    queue.Submit(1, &commands);
}

TEST_P(ShaderRobustnessPerf, Run) {
    RunTest();
}

DAWN_INSTANTIATE_TEST_P(ShaderRobustnessPerf,
                        {D3D12Backend(), D3D12Backend({"disable_robustness"}, {}), MetalBackend(),
                         MetalBackend({"disable_robustness"}, {}), OpenGLBackend(),
                         OpenGLBackend({"disable_robustness"}, {}), VulkanBackend(),
                         VulkanBackend({"disable_robustness"}, {})},
                        {MatMulMethod::MatMulFloatOneDimSharedArray,
                         MatMulMethod::MatMulFloatTwoDimSharedArray,
                         MatMulMethod::MatMulVec4OneDimSharedArray,
                         MatMulMethod::MatMulVec4TwoDimSharedArray},
                        {512u},
                        {512u},
                        {512u});
