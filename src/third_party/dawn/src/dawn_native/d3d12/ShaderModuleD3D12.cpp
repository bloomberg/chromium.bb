// Copyright 2017 The Dawn Authors
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

#include "dawn_native/d3d12/ShaderModuleD3D12.h"

#include "common/Assert.h"
#include "common/BitSetIterator.h"
#include "common/Log.h"
#include "dawn_native/SpirvUtils.h"
#include "dawn_native/TintUtils.h"
#include "dawn_native/d3d12/BindGroupLayoutD3D12.h"
#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/PipelineLayoutD3D12.h"
#include "dawn_native/d3d12/PlatformFunctions.h"
#include "dawn_native/d3d12/UtilsD3D12.h"

#include <d3dcompiler.h>

#include <spirv_hlsl.hpp>

// Tint include must be after spirv_hlsl.hpp, because spirv-cross has its own
// version of spirv_headers. We also need to undef SPV_REVISION because SPIRV-Cross
// is at 3 while spirv-headers is at 4.
#undef SPV_REVISION
#include <tint/tint.h>

namespace dawn_native { namespace d3d12 {

    namespace {
        std::vector<const wchar_t*> GetDXCArguments(uint32_t compileFlags, bool enable16BitTypes) {
            std::vector<const wchar_t*> arguments;
            if (compileFlags & D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY) {
                arguments.push_back(L"/Gec");
            }
            if (compileFlags & D3DCOMPILE_IEEE_STRICTNESS) {
                arguments.push_back(L"/Gis");
            }
            if (compileFlags & D3DCOMPILE_OPTIMIZATION_LEVEL2) {
                switch (compileFlags & D3DCOMPILE_OPTIMIZATION_LEVEL2) {
                    case D3DCOMPILE_OPTIMIZATION_LEVEL0:
                        arguments.push_back(L"/O0");
                        break;
                    case D3DCOMPILE_OPTIMIZATION_LEVEL2:
                        arguments.push_back(L"/O2");
                        break;
                    case D3DCOMPILE_OPTIMIZATION_LEVEL3:
                        arguments.push_back(L"/O3");
                        break;
                }
            }
            if (compileFlags & D3DCOMPILE_DEBUG) {
                arguments.push_back(L"/Zi");
            }
            if (compileFlags & D3DCOMPILE_PACK_MATRIX_ROW_MAJOR) {
                arguments.push_back(L"/Zpr");
            }
            if (compileFlags & D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR) {
                arguments.push_back(L"/Zpc");
            }
            if (compileFlags & D3DCOMPILE_AVOID_FLOW_CONTROL) {
                arguments.push_back(L"/Gfa");
            }
            if (compileFlags & D3DCOMPILE_PREFER_FLOW_CONTROL) {
                arguments.push_back(L"/Gfp");
            }
            if (compileFlags & D3DCOMPILE_RESOURCES_MAY_ALIAS) {
                arguments.push_back(L"/res_may_alias");
            }

            if (enable16BitTypes) {
                // enable-16bit-types are only allowed in -HV 2018 (default)
                arguments.push_back(L"/enable-16bit-types");
            }

            arguments.push_back(L"-HV");
            arguments.push_back(L"2018");

            return arguments;
        }

    }  // anonymous namespace

    ResultOrError<ComPtr<IDxcBlob>> CompileShaderDXC(Device* device,
                                                     SingleShaderStage stage,
                                                     const std::string& hlslSource,
                                                     const char* entryPoint,
                                                     uint32_t compileFlags) {
        ComPtr<IDxcLibrary> dxcLibrary = device->GetDxcLibrary();

        ComPtr<IDxcBlobEncoding> sourceBlob;
        DAWN_TRY(CheckHRESULT(dxcLibrary->CreateBlobWithEncodingOnHeapCopy(
                                  hlslSource.c_str(), hlslSource.length(), CP_UTF8, &sourceBlob),
                              "DXC create blob"));

        ComPtr<IDxcCompiler> dxcCompiler = device->GetDxcCompiler();

        std::wstring entryPointW;
        DAWN_TRY_ASSIGN(entryPointW, ConvertStringToWstring(entryPoint));

        std::vector<const wchar_t*> arguments =
            GetDXCArguments(compileFlags, device->IsExtensionEnabled(Extension::ShaderFloat16));

        ComPtr<IDxcOperationResult> result;
        DAWN_TRY(CheckHRESULT(
            dxcCompiler->Compile(sourceBlob.Get(), nullptr, entryPointW.c_str(),
                                 device->GetDeviceInfo().shaderProfiles[stage].c_str(),
                                 arguments.data(), arguments.size(), nullptr, 0, nullptr, &result),
            "DXC compile"));

        HRESULT hr;
        DAWN_TRY(CheckHRESULT(result->GetStatus(&hr), "DXC get status"));

        if (FAILED(hr)) {
            ComPtr<IDxcBlobEncoding> errors;
            DAWN_TRY(CheckHRESULT(result->GetErrorBuffer(&errors), "DXC get error buffer"));

            std::string message = std::string("DXC compile failed with ") +
                                  static_cast<char*>(errors->GetBufferPointer());
            return DAWN_INTERNAL_ERROR(message);
        }

        ComPtr<IDxcBlob> compiledShader;
        DAWN_TRY(CheckHRESULT(result->GetResult(&compiledShader), "DXC get result"));
        return std::move(compiledShader);
    }

    ResultOrError<ComPtr<ID3DBlob>> CompileShaderFXC(Device* device,
                                                     SingleShaderStage stage,
                                                     const std::string& hlslSource,
                                                     const char* entryPoint,
                                                     uint32_t compileFlags) {
        const char* targetProfile = nullptr;
        switch (stage) {
            case SingleShaderStage::Vertex:
                targetProfile = "vs_5_1";
                break;
            case SingleShaderStage::Fragment:
                targetProfile = "ps_5_1";
                break;
            case SingleShaderStage::Compute:
                targetProfile = "cs_5_1";
                break;
        }

        ComPtr<ID3DBlob> compiledShader;
        ComPtr<ID3DBlob> errors;

        const PlatformFunctions* functions = device->GetFunctions();
        if (FAILED(functions->d3dCompile(hlslSource.c_str(), hlslSource.length(), nullptr, nullptr,
                                         nullptr, entryPoint, targetProfile, compileFlags, 0,
                                         &compiledShader, &errors))) {
            std::string message = std::string("D3D compile failed with ") +
                                  static_cast<char*>(errors->GetBufferPointer());
            return DAWN_INTERNAL_ERROR(message);
        }

        return std::move(compiledShader);
    }

    // static
    ResultOrError<Ref<ShaderModule>> ShaderModule::Create(Device* device,
                                                          const ShaderModuleDescriptor* descriptor,
                                                          ShaderModuleParseResult* parseResult) {
        Ref<ShaderModule> module = AcquireRef(new ShaderModule(device, descriptor));
        DAWN_TRY(module->Initialize(parseResult));
        return module;
    }

    ShaderModule::ShaderModule(Device* device, const ShaderModuleDescriptor* descriptor)
        : ShaderModuleBase(device, descriptor) {
    }

    MaybeError ShaderModule::Initialize(ShaderModuleParseResult* parseResult) {
        ScopedTintICEHandler scopedICEHandler(GetDevice());
        return InitializeBase(parseResult);
    }

    ResultOrError<std::string> ShaderModule::TranslateToHLSLWithTint(
        const char* entryPointName,
        SingleShaderStage stage,
        PipelineLayout* layout,
        std::string* remappedEntryPointName,
        FirstOffsetInfo* firstOffsetInfo) const {
        ASSERT(!IsError());

        ScopedTintICEHandler scopedICEHandler(GetDevice());

        using BindingRemapper = tint::transform::BindingRemapper;
        using BindingPoint = tint::transform::BindingPoint;
        BindingRemapper::BindingPoints bindingPoints;
        BindingRemapper::AccessControls accessControls;

        const EntryPointMetadata::BindingInfoArray& moduleBindingInfo =
            GetEntryPoint(entryPointName).bindings;

        // d3d12::BindGroupLayout packs the bindings per HLSL register-space.
        // We modify the Tint AST to make the "bindings" decoration match the
        // offset chosen by d3d12::BindGroupLayout so that Tint produces HLSL
        // with the correct registers assigned to each interface variable.
        for (BindGroupIndex group : IterateBitSet(layout->GetBindGroupLayoutsMask())) {
            const BindGroupLayout* bgl = ToBackend(layout->GetBindGroupLayout(group));
            const auto& bindingOffsets = bgl->GetBindingOffsets();
            const auto& groupBindingInfo = moduleBindingInfo[group];
            for (const auto& it : groupBindingInfo) {
                BindingNumber binding = it.first;
                auto const& bindingInfo = it.second;
                BindingIndex bindingIndex = bgl->GetBindingIndex(binding);
                uint32_t bindingOffset = bindingOffsets[bindingIndex];
                BindingPoint srcBindingPoint{static_cast<uint32_t>(group),
                                             static_cast<uint32_t>(binding)};
                BindingPoint dstBindingPoint{static_cast<uint32_t>(group), bindingOffset};
                if (srcBindingPoint != dstBindingPoint) {
                    bindingPoints.emplace(srcBindingPoint, dstBindingPoint);
                }

                // Declaring a read-only storage buffer in HLSL but specifying a
                // storage buffer in the BGL produces the wrong output.
                // Force read-only storage buffer bindings to be treated as UAV
                // instead of SRV.
                const bool forceStorageBufferAsUAV =
                    (bindingInfo.buffer.type == wgpu::BufferBindingType::ReadOnlyStorage &&
                     bgl->GetBindingInfo(bindingIndex).buffer.type ==
                         wgpu::BufferBindingType::Storage);
                if (forceStorageBufferAsUAV) {
                    accessControls.emplace(srcBindingPoint, tint::ast::AccessControl::kReadWrite);
                }
            }
        }

        std::ostringstream errorStream;
        errorStream << "Tint HLSL failure:" << std::endl;

        tint::transform::Manager transformManager;
        tint::transform::DataMap transformInputs;

        if (GetDevice()->IsRobustnessEnabled()) {
            transformManager.Add<tint::transform::BoundArrayAccessors>();
        }
        if (stage == SingleShaderStage::Vertex) {
            transformManager.Add<tint::transform::FirstIndexOffset>();
            transformInputs.Add<tint::transform::FirstIndexOffset::BindingPoint>(
                layout->GetFirstIndexOffsetShaderRegister(),
                layout->GetFirstIndexOffsetRegisterSpace());
        }
        transformManager.Add<tint::transform::BindingRemapper>();
        transformManager.Add<tint::transform::Renamer>();
        transformManager.Add<tint::transform::Hlsl>();

        // D3D12 registers like `t3` and `c3` have the same bindingOffset number in the
        // remapping but should not be considered a collision because they have different types.
        const bool mayCollide = true;
        transformInputs.Add<BindingRemapper::Remappings>(std::move(bindingPoints),
                                                         std::move(accessControls), mayCollide);

        tint::Program program;
        tint::transform::DataMap transformOutputs;
        DAWN_TRY_ASSIGN(program, RunTransforms(&transformManager, GetTintProgram(), transformInputs,
                                               &transformOutputs, nullptr));

        if (auto* data = transformOutputs.Get<tint::transform::FirstIndexOffset::Data>()) {
            firstOffsetInfo->usesVertexIndex = data->has_vertex_index;
            if (firstOffsetInfo->usesVertexIndex) {
                firstOffsetInfo->vertexIndexOffset = data->first_vertex_offset;
            }
            firstOffsetInfo->usesInstanceIndex = data->has_instance_index;
            if (firstOffsetInfo->usesInstanceIndex) {
                firstOffsetInfo->instanceIndexOffset = data->first_instance_offset;
            }
        }

        if (auto* data = transformOutputs.Get<tint::transform::Renamer::Data>()) {
            auto it = data->remappings.find(entryPointName);
            if (it == data->remappings.end()) {
                return DAWN_VALIDATION_ERROR("Could not find remapped name for entry point.");
            }
            *remappedEntryPointName = it->second;
        } else {
            return DAWN_VALIDATION_ERROR("Transform output missing renamer data.");
        }

        tint::writer::hlsl::Generator generator(&program);
        // TODO: Switch to GenerateEntryPoint once HLSL writer supports it.
        if (!generator.Generate()) {
            errorStream << "Generator: " << generator.error() << std::endl;
            return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
        }

        return generator.result();
    }

    ResultOrError<std::string> ShaderModule::TranslateToHLSLWithSPIRVCross(
        const char* entryPointName,
        SingleShaderStage stage,
        PipelineLayout* layout) const {
        ASSERT(!IsError());

        // If these options are changed, the values in DawnSPIRVCrossHLSLFastFuzzer.cpp need to
        // be updated.
        spirv_cross::CompilerGLSL::Options options_glsl;
        // Force all uninitialized variables to be 0, otherwise they will fail to compile
        // by FXC.
        options_glsl.force_zero_initialized_variables = true;

        spirv_cross::CompilerHLSL::Options options_hlsl;
        if (GetDevice()->IsToggleEnabled(Toggle::UseDXC)) {
            options_hlsl.shader_model = ToBackend(GetDevice())->GetDeviceInfo().shaderModel;
        } else {
            options_hlsl.shader_model = 51;
        }

        if (GetDevice()->IsExtensionEnabled(Extension::ShaderFloat16)) {
            options_hlsl.enable_16bit_types = true;
        }
        // PointCoord and PointSize are not supported in HLSL
        // TODO (hao.x.li@intel.com): The point_coord_compat and point_size_compat are
        // required temporarily for https://bugs.chromium.org/p/dawn/issues/detail?id=146,
        // but should be removed once WebGPU requires there is no gl_PointSize builtin.
        // See https://github.com/gpuweb/gpuweb/issues/332
        options_hlsl.point_coord_compat = true;
        options_hlsl.point_size_compat = true;
        options_hlsl.nonwritable_uav_texture_as_srv = true;

        spirv_cross::CompilerHLSL compiler(GetSpirv());
        compiler.set_common_options(options_glsl);
        compiler.set_hlsl_options(options_hlsl);
        compiler.set_entry_point(entryPointName, ShaderStageToExecutionModel(stage));

        const EntryPointMetadata::BindingInfoArray& moduleBindingInfo =
            GetEntryPoint(entryPointName).bindings;

        for (BindGroupIndex group : IterateBitSet(layout->GetBindGroupLayoutsMask())) {
            const BindGroupLayout* bgl = ToBackend(layout->GetBindGroupLayout(group));
            const auto& bindingOffsets = bgl->GetBindingOffsets();
            const auto& groupBindingInfo = moduleBindingInfo[group];
            for (const auto& it : groupBindingInfo) {
                const EntryPointMetadata::ShaderBindingInfo& bindingInfo = it.second;
                BindingNumber bindingNumber = it.first;
                BindingIndex bindingIndex = bgl->GetBindingIndex(bindingNumber);

                // Declaring a read-only storage buffer in HLSL but specifying a storage buffer in
                // the BGL produces the wrong output. Force read-only storage buffer bindings to
                // be treated as UAV instead of SRV.
                const bool forceStorageBufferAsUAV =
                    (bindingInfo.buffer.type == wgpu::BufferBindingType::ReadOnlyStorage &&
                     bgl->GetBindingInfo(bindingIndex).buffer.type ==
                         wgpu::BufferBindingType::Storage);

                uint32_t bindingOffset = bindingOffsets[bindingIndex];
                compiler.set_decoration(bindingInfo.id, spv::DecorationBinding, bindingOffset);
                if (forceStorageBufferAsUAV) {
                    compiler.set_hlsl_force_storage_buffer_as_uav(
                        static_cast<uint32_t>(group), static_cast<uint32_t>(bindingNumber));
                }
            }
        }

        return compiler.compile();
    }

    ResultOrError<CompiledShader> ShaderModule::Compile(const char* entryPointName,
                                                        SingleShaderStage stage,
                                                        PipelineLayout* layout,
                                                        uint32_t compileFlags) {
        Device* device = ToBackend(GetDevice());

        // Compile the source shader to HLSL.
        std::string hlslSource;
        std::string remappedEntryPoint;
        CompiledShader compiledShader = {};
        if (device->IsToggleEnabled(Toggle::UseTintGenerator)) {
            DAWN_TRY_ASSIGN(hlslSource, TranslateToHLSLWithTint(entryPointName, stage, layout,
                                                                &remappedEntryPoint,
                                                                &compiledShader.firstOffsetInfo));
            entryPointName = remappedEntryPoint.c_str();
        } else {
            DAWN_TRY_ASSIGN(hlslSource,
                            TranslateToHLSLWithSPIRVCross(entryPointName, stage, layout));

            // Note that the HLSL will always use entryPoint "main" under
            // SPIRV-cross.
            entryPointName = "main";
        }

        // Use HLSL source as the input for the key since it does need to know about the pipeline
        // layout. The pipeline layout is only required if we key from WGSL: two different pipeline
        // layouts could be used to produce different shader blobs and the wrong shader blob could
        // be loaded since the pipeline layout was missing from the key.
        // The compiler flags or version used could also produce different HLSL source. HLSL key
        // needs both to ensure the shader cache key is unique to the HLSL source.
        // TODO(dawn:549): Consider keying from WGSL and serialize the pipeline layout it used.
        PersistentCacheKey shaderCacheKey;
        DAWN_TRY_ASSIGN(shaderCacheKey,
                        CreateHLSLKey(entryPointName, stage, hlslSource, compileFlags));

        DAWN_TRY_ASSIGN(compiledShader.cachedShader,
                        device->GetPersistentCache()->GetOrCreate(
                            shaderCacheKey, [&](auto doCache) -> MaybeError {
                                if (device->IsToggleEnabled(Toggle::UseDXC)) {
                                    DAWN_TRY_ASSIGN(compiledShader.compiledDXCShader,
                                                    CompileShaderDXC(device, stage, hlslSource,
                                                                     entryPointName, compileFlags));
                                } else {
                                    DAWN_TRY_ASSIGN(compiledShader.compiledFXCShader,
                                                    CompileShaderFXC(device, stage, hlslSource,
                                                                     entryPointName, compileFlags));
                                }
                                const D3D12_SHADER_BYTECODE shader =
                                    compiledShader.GetD3D12ShaderBytecode();
                                doCache(shader.pShaderBytecode, shader.BytecodeLength);
                                return {};
                            }));

        return std::move(compiledShader);
    }

    D3D12_SHADER_BYTECODE CompiledShader::GetD3D12ShaderBytecode() const {
        if (cachedShader.buffer != nullptr) {
            return {cachedShader.buffer.get(), cachedShader.bufferSize};
        } else if (compiledFXCShader != nullptr) {
            return {compiledFXCShader->GetBufferPointer(), compiledFXCShader->GetBufferSize()};
        } else if (compiledDXCShader != nullptr) {
            return {compiledDXCShader->GetBufferPointer(), compiledDXCShader->GetBufferSize()};
        }
        UNREACHABLE();
        return {};
    }

    ResultOrError<PersistentCacheKey> ShaderModule::CreateHLSLKey(const char* entryPointName,
                                                                  SingleShaderStage stage,
                                                                  const std::string& hlslSource,
                                                                  uint32_t compileFlags) const {
        std::stringstream stream;

        // Prefix the key with the type to avoid collisions from another type that could have the
        // same key.
        stream << static_cast<uint32_t>(PersistentKeyType::Shader);

        // Provide "guard" strings that the user cannot provide to help ensure the generated HLSL
        // used to create this key is not being manufactured by the user to load the wrong shader
        // blob.
        // These strings can be HLSL comments because Tint does not emit HLSL comments.
        // TODO(dawn:549): Replace guards strings with something more secure.
        constexpr char kStartGuard[] = "// Start shader autogenerated by Dawn.";
        constexpr char kEndGuard[] = "// End shader autogenerated by Dawn.";
        ASSERT(hlslSource.find(kStartGuard) == std::string::npos);
        ASSERT(hlslSource.find(kEndGuard) == std::string::npos);

        stream << kStartGuard << "\n";
        stream << hlslSource;
        stream << "\n" << kEndGuard;

        stream << compileFlags;

        // Add the HLSL compiler version for good measure.
        // Prepend the compiler name to ensure the version is always unique.
        if (GetDevice()->IsToggleEnabled(Toggle::UseDXC)) {
            uint64_t dxCompilerVersion;
            DAWN_TRY_ASSIGN(dxCompilerVersion, GetDXCompilerVersion());
            stream << "DXC" << dxCompilerVersion;
        } else {
            stream << "FXC" << GetD3DCompilerVersion();
        }

        // If the source contains multiple entry points, ensure they are cached seperately
        // per stage since DX shader code can only be compiled per stage using the same
        // entry point.
        stream << static_cast<uint32_t>(stage);
        stream << entryPointName;

        return PersistentCacheKey(std::istreambuf_iterator<char>{stream},
                                  std::istreambuf_iterator<char>{});
    }

    ResultOrError<uint64_t> ShaderModule::GetDXCompilerVersion() const {
        ComPtr<IDxcValidator> dxcValidator = ToBackend(GetDevice())->GetDxcValidator();

        ComPtr<IDxcVersionInfo> versionInfo;
        DAWN_TRY(CheckHRESULT(dxcValidator.As(&versionInfo),
                              "D3D12 QueryInterface IDxcValidator to IDxcVersionInfo"));

        uint32_t compilerMajor, compilerMinor;
        DAWN_TRY(CheckHRESULT(versionInfo->GetVersion(&compilerMajor, &compilerMinor),
                              "IDxcVersionInfo::GetVersion"));

        // Pack both into a single version number.
        return (uint64_t(compilerMajor) << uint64_t(32)) + compilerMinor;
    }

    uint64_t ShaderModule::GetD3DCompilerVersion() const {
        return D3D_COMPILER_VERSION;
    }
}}  // namespace dawn_native::d3d12
