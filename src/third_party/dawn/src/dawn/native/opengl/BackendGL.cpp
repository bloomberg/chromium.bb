// Copyright 2019 The Dawn Authors
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

#include "dawn/native/opengl/BackendGL.h"

#include "dawn/common/GPUInfo.h"
#include "dawn/common/Log.h"
#include "dawn/native/Instance.h"
#include "dawn/native/OpenGLBackend.h"
#include "dawn/native/opengl/DeviceGL.h"

#include <cstring>

namespace dawn::native::opengl {

    namespace {

        struct Vendor {
            const char* vendorName;
            uint32_t vendorId;
        };

        const Vendor kVendors[] = {{"ATI", gpu_info::kVendorID_AMD},
                                   {"ARM", gpu_info::kVendorID_ARM},
                                   {"Imagination", gpu_info::kVendorID_ImgTec},
                                   {"Intel", gpu_info::kVendorID_Intel},
                                   {"NVIDIA", gpu_info::kVendorID_Nvidia},
                                   {"Qualcomm", gpu_info::kVendorID_Qualcomm}};

        uint32_t GetVendorIdFromVendors(const char* vendor) {
            uint32_t vendorId = 0;
            for (const auto& it : kVendors) {
                // Matching vendor name with vendor string
                if (strstr(vendor, it.vendorName) != nullptr) {
                    vendorId = it.vendorId;
                    break;
                }
            }
            return vendorId;
        }

        void KHRONOS_APIENTRY OnGLDebugMessage(GLenum source,
                                               GLenum type,
                                               GLuint id,
                                               GLenum severity,
                                               GLsizei length,
                                               const GLchar* message,
                                               const void* userParam) {
            const char* sourceText;
            switch (source) {
                case GL_DEBUG_SOURCE_API:
                    sourceText = "OpenGL";
                    break;
                case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
                    sourceText = "Window System";
                    break;
                case GL_DEBUG_SOURCE_SHADER_COMPILER:
                    sourceText = "Shader Compiler";
                    break;
                case GL_DEBUG_SOURCE_THIRD_PARTY:
                    sourceText = "Third Party";
                    break;
                case GL_DEBUG_SOURCE_APPLICATION:
                    sourceText = "Application";
                    break;
                case GL_DEBUG_SOURCE_OTHER:
                    sourceText = "Other";
                    break;
                default:
                    sourceText = "UNKNOWN";
                    break;
            }

            const char* severityText;
            switch (severity) {
                case GL_DEBUG_SEVERITY_HIGH:
                    severityText = "High";
                    break;
                case GL_DEBUG_SEVERITY_MEDIUM:
                    severityText = "Medium";
                    break;
                case GL_DEBUG_SEVERITY_LOW:
                    severityText = "Low";
                    break;
                case GL_DEBUG_SEVERITY_NOTIFICATION:
                    severityText = "Notification";
                    break;
                default:
                    severityText = "UNKNOWN";
                    break;
            }

            if (type == GL_DEBUG_TYPE_ERROR) {
                dawn::WarningLog() << "OpenGL error:"
                                   << "\n    Source: " << sourceText      //
                                   << "\n    ID: " << id                  //
                                   << "\n    Severity: " << severityText  //
                                   << "\n    Message: " << message;

                // Abort on an error when in Debug mode.
                UNREACHABLE();
            }
        }

    }  // anonymous namespace

    // The OpenGL backend's Adapter.

    class Adapter : public AdapterBase {
      public:
        Adapter(InstanceBase* instance, wgpu::BackendType backendType)
            : AdapterBase(instance, backendType) {
        }

        MaybeError InitializeGLFunctions(void* (*getProc)(const char*)) {
            // Use getProc to populate the dispatch table
            return mFunctions.Initialize(getProc);
        }

        ~Adapter() override = default;

        // AdapterBase Implementation
        bool SupportsExternalImages() const override {
            // Via dawn::native::opengl::WrapExternalEGLImage
            return GetBackendType() == wgpu::BackendType::OpenGLES;
        }

      private:
        MaybeError InitializeImpl() override {
            if (mFunctions.GetVersion().IsES()) {
                ASSERT(GetBackendType() == wgpu::BackendType::OpenGLES);
            } else {
                ASSERT(GetBackendType() == wgpu::BackendType::OpenGL);
            }

            // Use the debug output functionality to get notified about GL errors
            // TODO(cwallez@chromium.org): add support for the KHR_debug and ARB_debug_output
            // extensions
            bool hasDebugOutput = mFunctions.IsAtLeastGL(4, 3) || mFunctions.IsAtLeastGLES(3, 2);

            if (GetInstance()->IsBackendValidationEnabled() && hasDebugOutput) {
                mFunctions.Enable(GL_DEBUG_OUTPUT);
                mFunctions.Enable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

                // Any GL error; dangerous undefined behavior; any shader compiler and linker errors
                mFunctions.DebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_HIGH,
                                               0, nullptr, GL_TRUE);

                // Severe performance warnings; GLSL or other shader compiler and linker warnings;
                // use of currently deprecated behavior
                mFunctions.DebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_MEDIUM,
                                               0, nullptr, GL_TRUE);

                // Performance warnings from redundant state changes; trivial undefined behavior
                // This is disabled because we do an incredible amount of redundant state changes.
                mFunctions.DebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW, 0,
                                               nullptr, GL_FALSE);

                // Any message which is not an error or performance concern
                mFunctions.DebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                                               GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr,
                                               GL_FALSE);
                mFunctions.DebugMessageCallback(&OnGLDebugMessage, nullptr);
            }

            // Set state that never changes between devices.
            mFunctions.Enable(GL_DEPTH_TEST);
            mFunctions.Enable(GL_SCISSOR_TEST);
            mFunctions.Enable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
            if (mFunctions.GetVersion().IsDesktop()) {
                // These are not necessary on GLES. The functionality is enabled by default, and
                // works by specifying sample counts and SRGB textures, respectively.
                mFunctions.Enable(GL_MULTISAMPLE);
                mFunctions.Enable(GL_FRAMEBUFFER_SRGB);
            }
            mFunctions.Enable(GL_SAMPLE_MASK);

            mName = reinterpret_cast<const char*>(mFunctions.GetString(GL_RENDERER));

            // Workaroud to find vendor id from vendor name
            const char* vendor = reinterpret_cast<const char*>(mFunctions.GetString(GL_VENDOR));
            mVendorId = GetVendorIdFromVendors(vendor);

            mDriverDescription = std::string("OpenGL version ") +
                                 reinterpret_cast<const char*>(mFunctions.GetString(GL_VERSION));

            if (mName.find("SwiftShader") != std::string::npos) {
                mAdapterType = wgpu::AdapterType::CPU;
            }

            return {};
        }

        MaybeError InitializeSupportedFeaturesImpl() override {
            // TextureCompressionBC
            {
                // BC1, BC2 and BC3 are not supported in OpenGL or OpenGL ES core features.
                bool supportsS3TC =
                    mFunctions.IsGLExtensionSupported("GL_EXT_texture_compression_s3tc") ||
                    (mFunctions.IsGLExtensionSupported("GL_EXT_texture_compression_dxt1") &&
                     mFunctions.IsGLExtensionSupported("GL_ANGLE_texture_compression_dxt3") &&
                     mFunctions.IsGLExtensionSupported("GL_ANGLE_texture_compression_dxt5"));

                // COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT and
                // COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT requires both GL_EXT_texture_sRGB and
                // GL_EXT_texture_compression_s3tc on desktop OpenGL drivers.
                // (https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_sRGB.txt)
                bool supportsTextureSRGB = mFunctions.IsGLExtensionSupported("GL_EXT_texture_sRGB");

                // GL_EXT_texture_compression_s3tc_srgb is an extension in OpenGL ES.
                // NVidia GLES drivers don't support this extension, but they do support
                // GL_NV_sRGB_formats. (Note that GL_EXT_texture_sRGB does not exist on ES.
                // GL_EXT_sRGB does (core in ES 3.0), but it does not automatically provide S3TC
                // SRGB support even if S3TC is supported; see
                // https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_sRGB.txt.)
                bool supportsS3TCSRGB =
                    mFunctions.IsGLExtensionSupported("GL_EXT_texture_compression_s3tc_srgb") ||
                    mFunctions.IsGLExtensionSupported("GL_NV_sRGB_formats");

                // BC4 and BC5
                bool supportsRGTC =
                    mFunctions.IsAtLeastGL(3, 0) ||
                    mFunctions.IsGLExtensionSupported("GL_ARB_texture_compression_rgtc") ||
                    mFunctions.IsGLExtensionSupported("GL_EXT_texture_compression_rgtc");

                // BC6 and BC7
                bool supportsBPTC =
                    mFunctions.IsAtLeastGL(4, 2) ||
                    mFunctions.IsGLExtensionSupported("GL_ARB_texture_compression_bptc") ||
                    mFunctions.IsGLExtensionSupported("GL_EXT_texture_compression_bptc");

                if (supportsS3TC && (supportsTextureSRGB || supportsS3TCSRGB) && supportsRGTC &&
                    supportsBPTC) {
                    mSupportedFeatures.EnableFeature(dawn::native::Feature::TextureCompressionBC);
                }
                mSupportedFeatures.EnableFeature(Feature::Depth24UnormStencil8);
            }

            return {};
        }

        MaybeError InitializeSupportedLimitsImpl(CombinedLimits* limits) override {
            GetDefaultLimits(&limits->v1);
            return {};
        }

        ResultOrError<Ref<DeviceBase>> CreateDeviceImpl(
            const DeviceDescriptor* descriptor) override {
            // There is no limit on the number of devices created from this adapter because they can
            // all share the same backing OpenGL context.
            return Device::Create(this, descriptor, mFunctions);
        }

        OpenGLFunctions mFunctions;
    };

    // Implementation of the OpenGL backend's BackendConnection

    Backend::Backend(InstanceBase* instance, wgpu::BackendType backendType)
        : BackendConnection(instance, backendType) {
    }

    std::vector<Ref<AdapterBase>> Backend::DiscoverDefaultAdapters() {
        // The OpenGL backend needs at least "getProcAddress" to discover an adapter.
        return {};
    }

    ResultOrError<std::vector<Ref<AdapterBase>>> Backend::DiscoverAdapters(
        const AdapterDiscoveryOptionsBase* optionsBase) {
        // TODO(cwallez@chromium.org): For now only create a single OpenGL adapter because don't
        // know how to handle MakeCurrent.
        DAWN_INVALID_IF(mCreatedAdapter, "The OpenGL backend can only create a single adapter.");

        ASSERT(static_cast<wgpu::BackendType>(optionsBase->backendType) == GetType());
        const AdapterDiscoveryOptions* options =
            static_cast<const AdapterDiscoveryOptions*>(optionsBase);

        DAWN_INVALID_IF(options->getProc == nullptr,
                        "AdapterDiscoveryOptions::getProc must be set");

        Ref<Adapter> adapter = AcquireRef(
            new Adapter(GetInstance(), static_cast<wgpu::BackendType>(optionsBase->backendType)));
        DAWN_TRY(adapter->InitializeGLFunctions(options->getProc));
        DAWN_TRY(adapter->Initialize());

        mCreatedAdapter = true;
        std::vector<Ref<AdapterBase>> adapters{std::move(adapter)};
        return std::move(adapters);
    }

    BackendConnection* Connect(InstanceBase* instance, wgpu::BackendType backendType) {
        return new Backend(instance, backendType);
    }

}  // namespace dawn::native::opengl
