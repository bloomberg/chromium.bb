/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the logic for converting the scene graph to a
// JSON-encoded file that is stored in a zip archive.
#include "converter/cross/converter.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "converter/cross/renderer_stub.h"
#include "core/cross/class_manager.h"
#include "core/cross/client.h"
#include "core/cross/client_info.h"
#include "core/cross/effect.h"
#include "core/cross/error.h"
#include "core/cross/features.h"
#include "core/cross/object_manager.h"
#include "core/cross/pack.h"
#include "core/cross/renderer.h"
#include "core/cross/service_locator.h"
#include "core/cross/transform.h"
#include "core/cross/tree_traversal.h"
#include "core/cross/types.h"
#include "import/cross/collada.h"
#include "import/cross/collada_conditioner.h"
#include "import/cross/file_output_stream_processor.h"
#include "import/cross/iarchive_generator.h"
#include "import/cross/targz_generator.h"
#include "import/cross/archive_request.h"
#include "serializer/cross/serializer.h"
#include "utils/cross/file_path_utils.h"
#include "utils/cross/json_writer.h"
#include "utils/cross/string_writer.h"
#include "utils/cross/temporary_file.h"

namespace o3d {

namespace {
// Writes either to an archive file or to multiple flat files.
class MultiFileWriter : public IArchiveGenerator {
 public:
  virtual ~MultiFileWriter() {}

  // Takes the path of an archive file. If writing multiple flat
  // files, then for archive named archive.o3dtgz, creates directory
  // "archive/" and writes all files within.
  virtual bool OpenTopLevelFile(const FilePath& file_name,
                                const char* mode) = 0;

  // Closes the top-level file and any currently open sub-file.
  virtual void CloseTopLevelFile(bool success) = 0;

  int AddFileBytes(const uint8* data, size_t n);

  //----------------------------------------------------------------------
  // IArchiveGenerator
  // It seems these virtual functions need to be re-declared here.
  // (Compiler bugs on Mac OS X?)

  // AddFile either adds a file to the archive, or opens a new flat
  // file. If a sub-file is currently open, implicitly closes it.
  virtual bool AddFile(const String& file_name,
                       size_t file_size) = 0;

  // Writes data to the current sub-file.
  virtual int AddFileBytes(MemoryReadStream* stream, size_t n) = 0;

  // Closes the current sub-file.
  virtual void Close(bool success) = 0;

 protected:
  MultiFileWriter() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MultiFileWriter);
};

int MultiFileWriter::AddFileBytes(const uint8* data, size_t n) {
  MemoryReadStream stream(data, n);
  return AddFileBytes(&stream, n);
}

class TarGzWriter : public MultiFileWriter {
 public:
  TarGzWriter() {}
  virtual ~TarGzWriter();

  virtual bool OpenTopLevelFile(const FilePath& file_name,
                                const char* mode);
  virtual bool AddFile(const String& file_name,
                       size_t file_size);
  virtual int AddFileBytes(MemoryReadStream* stream,
                           size_t n);
  virtual void Close(bool success);
  virtual void CloseTopLevelFile(bool success);

 private:
  scoped_ptr<FileOutputStreamProcessor> stream_processor_;
  scoped_ptr<TarGzGenerator> archive_generator_;
  DISALLOW_COPY_AND_ASSIGN(TarGzWriter);
};

TarGzWriter::~TarGzWriter() {
  // Need to reset these in the right order.
  archive_generator_.reset();
  stream_processor_.reset();
}

bool TarGzWriter::OpenTopLevelFile(const FilePath& file_name,
                                   const char* mode) {
  FILE* out_file = file_util::OpenFile(file_name, mode);
  if (out_file == NULL) {
    return false;
  }
  stream_processor_.reset(new FileOutputStreamProcessor(out_file));
  archive_generator_.reset(new TarGzGenerator(stream_processor_.get()));
  return true;
}

bool TarGzWriter::AddFile(const String& file_name,
                          size_t file_size) {
  archive_generator_->AddFile(file_name, file_size);
  return true;
}

int TarGzWriter::AddFileBytes(MemoryReadStream* stream,
                              size_t n) {
  return archive_generator_->AddFileBytes(stream, n);
}

void TarGzWriter::Close(bool success) {
}

void TarGzWriter::CloseTopLevelFile(bool success) {
  archive_generator_->Close(success);
}

class FlatFileWriter : public MultiFileWriter {
 public:
  FlatFileWriter() {}
  virtual ~FlatFileWriter();

  virtual bool OpenTopLevelFile(const FilePath& file_name,
                                const char* mode);
  virtual bool AddFile(const String& file_name,
                       size_t file_size);
  virtual int AddFileBytes(MemoryReadStream* stream,
                           size_t n);
  virtual void Close(bool success);
  virtual void CloseTopLevelFile(bool success);

 private:
  FilePath destination_directory_;
  String mode_;
  scoped_ptr<FileOutputStreamProcessor> stream_processor_;
  DISALLOW_COPY_AND_ASSIGN(FlatFileWriter);
};

FlatFileWriter::~FlatFileWriter() {
}

bool FlatFileWriter::OpenTopLevelFile(const FilePath& file_name,
                                      const char* mode) {
  destination_directory_ = file_name.RemoveExtension();
  if (!file_util::CreateDirectory(destination_directory_)) {
    return false;
  }
  mode_ = String(mode);
  return true;
}

bool FlatFileWriter::AddFile(const String& file_name,
                             size_t file_size) {
  Close(true);
  FilePath path = destination_directory_.Append(UTF8ToFilePath(file_name));
  // We may need to make parent directories.
  FilePath parent_dir = path.DirName();
  if (!file_util::CreateDirectory(parent_dir)) {
    return false;
  }
  FILE* out_file = file_util::OpenFile(path, mode_.c_str());
  // We expect to be able to write the desired file.
  DCHECK_NE(out_file, static_cast<FILE*>(NULL));
  if (out_file == NULL) {
    return false;
  }
  stream_processor_.reset(new FileOutputStreamProcessor(out_file));
  return true;
}

int FlatFileWriter::AddFileBytes(MemoryReadStream* stream,
                                 size_t n) {
  if (!stream_processor_.get())
    return 0;
  StreamProcessor::Status status =
      stream_processor_->ProcessBytes(stream, n);
  if (status == StreamProcessor::FAILURE)
    return 0;
  return n;
}

void FlatFileWriter::Close(bool success) {
  if (stream_processor_.get()) {
    stream_processor_->Close(success);
    stream_processor_.reset();
  }
}

void FlatFileWriter::CloseTopLevelFile(bool success) {
  Close(success);
}

void AddBinaryElements(const Collada& collada,
                       MultiFileWriter* file_writer) {
  const ColladaDataMap& data_map(collada.original_data_map());
  std::vector<FilePath> paths = data_map.GetOriginalDataFilenames();
  for (std::vector<FilePath>::const_iterator iter = paths.begin();
       iter != paths.end();
       ++iter) {
    const std::string& data = data_map.GetOriginalData(*iter);

    file_writer->AddFile(FilePathToUTF8(*iter), data.size());
    file_writer->AddFileBytes(
        reinterpret_cast<const uint8*>(data.c_str()),
        data.length());
  }
}
}  // end anonymous namespace

namespace converter {
// Loads the Collada input file and writes it to the zipped JSON output file.
bool Convert(const FilePath& in_filename,
             const FilePath& out_filename,
             const Options& options,
             String *error_messages) {
  // Create a service locator and renderer.
  ServiceLocator service_locator;
  EvaluationCounter evaluation_counter(&service_locator);
  ClassManager class_manager(&service_locator);
  ClientInfoManager client_info_manager(&service_locator);
  ObjectManager object_manager(&service_locator);
  Profiler profiler(&service_locator);
  ErrorStatus error_status(&service_locator);
  Features features(&service_locator);

  Collada::Init(&service_locator);
  features.Init("MaxCapabilities");

  // Collect error messages.
  ErrorCollector error_collector(&service_locator);

  scoped_ptr<Renderer> renderer(RendererStub::CreateDefault(&service_locator));
  renderer->InitCommon();

  Pack::Ref pack(object_manager.CreatePack());
  Transform* root = pack->Create<Transform>();
  root->set_name(String(Serializer::ROOT_PREFIX) + String("root"));

  // Setup a ParamFloat to be the source to all animations in this file.
  ParamObject* param_object = pack->Create<ParamObject>();
  // This is some arbitrary name
  param_object->set_name(O3D_STRING_CONSTANT("animSourceOwner"));
  ParamFloat* param_float = param_object->CreateParam<ParamFloat>("animSource");

  Collada::Options collada_options;
  collada_options.condition_document = options.condition;
  collada_options.keep_original_data = true;
  collada_options.base_path = options.base_path;
  collada_options.file_paths = options.file_paths;
  collada_options.up_axis = options.up_axis;
  collada_options.convert_dds_to_png = options.convert_dds_to_png;
  collada_options.convert_cg_to_glsl = options.convert_cg_to_glsl;
  collada_options.converter_tool = options.converter_tool;
  Collada collada(pack.Get(), collada_options);
  bool result = collada.ImportFile(in_filename, root, param_float);
  if (!result || !error_collector.errors().empty()) {
    if (error_messages) {
      *error_messages += error_collector.errors();
    }
    return false;
  }

  // Remove the animation param_object (and indirectly the param_float)
  // if there is no animation.
  if (param_float->output_connections().empty()) {
    pack->RemoveObject(param_object);
  }

  // Mark all Samplers to use tri-linear filtering
  if (!options.keep_filters) {
    std::vector<Sampler*> samplers = pack->GetByClass<Sampler>();
    for (unsigned ii = 0; ii < samplers.size(); ++ii) {
      Sampler* sampler = samplers[ii];
      sampler->set_mag_filter(Sampler::LINEAR);
      sampler->set_min_filter(Sampler::LINEAR);
      sampler->set_mip_filter(Sampler::LINEAR);
    }
  }

  // Mark all Materials that are on Primitives that have no normals as constant.
  if (!options.keep_materials) {
    std::vector<Primitive*> primitives = pack->GetByClass<Primitive>();
    for (unsigned ii = 0; ii < primitives.size(); ++ii) {
      Primitive* primitive = primitives[ii];
      StreamBank* stream_bank = primitive->stream_bank();
      if (stream_bank && !stream_bank->GetVertexStream(Stream::NORMAL, 0)) {
        Material* material = primitive->material();
        if (material) {
          ParamString* lighting_param = material->GetParam<ParamString>(
              Collada::kLightingTypeParamName);
          if (lighting_param) {
            // If the lighting type is lambert, blinn, or phong
            // copy the diffuse color to the emissive since that's most likely
            // what the user wants to see.
            if (lighting_param->value().compare(
                   Collada::kLightingTypeLambert) == 0 ||
                lighting_param->value().compare(
                   Collada::kLightingTypeBlinn) == 0 ||
                lighting_param->value().compare(
                   Collada::kLightingTypePhong) == 0) {
              // There's 4 cases: (to bad they are not the same names)
              // 1) Diffuse -> Emissive
              // 2) DiffuseSampler -> Emissive
              // 3) Diffuse -> EmissiveSampler
              // 4) DiffuseSampler -> EmissiveSampler
              ParamFloat4* diffuse_param = material->GetParam<ParamFloat4>(
                  Collada::kMaterialParamNameDiffuse);
              ParamFloat4* emissive_param = material->GetParam<ParamFloat4>(
                  Collada::kMaterialParamNameEmissive);
              ParamSampler* diffuse_sampler_param =
                  material->GetParam<ParamSampler>(
                      Collada::kMaterialParamNameDiffuseSampler);
              ParamSampler* emissive_sampler_param =
                  material->GetParam<ParamSampler>(
                      Collada::kMaterialParamNameEmissive);
              Param* source_param = diffuse_param ?
                  static_cast<Param*>(diffuse_param) :
                  static_cast<Param*>(diffuse_sampler_param);
              Param* destination_param = emissive_param ?
                  static_cast<Param*>(emissive_param) :
                  static_cast<Param*>(emissive_sampler_param);
              if (!source_param->IsA(destination_param->GetClass())) {
                // The params do not match type so we need to make the emissive
                // Param the same as the diffuse Param.
                material->RemoveParam(destination_param);
                destination_param = material->CreateParamByClass(
                    diffuse_param ? Collada::kMaterialParamNameEmissive :
                                    Collada::kMaterialParamNameEmissiveSampler,
                    source_param->GetClass());
                DCHECK(destination_param);
              }
              destination_param->CopyDataFromParam(source_param);
            }
            lighting_param->set_value(Collada::kLightingTypeConstant);
          }
        }
      }
    }
  }

  // Attempt to open the output file.
  scoped_ptr<MultiFileWriter> file_writer;
  if (options.archive) {
    file_writer.reset(new TarGzWriter);
  } else {
    file_writer.reset(new FlatFileWriter);
  }

  // Create an archive file or individual flat files and serialize the
  // JSON scene graph and assets to it / them.
  if (!file_writer->OpenTopLevelFile(out_filename, "wb")) {
    O3D_ERROR(&service_locator) << "Could not open output file \""
                                << FilePathToUTF8(out_filename).c_str()
                                << "\"";
    if (error_messages) {
      *error_messages += error_collector.errors();
    }
    return false;
  }

  if (options.archive) {
    file_writer->AddFile(ArchiveRequest::kO3DMarker,
                         ArchiveRequest::kO3DMarkerContentLength);
    file_writer->AddFileBytes(
        reinterpret_cast<const uint8*>(ArchiveRequest::kO3DMarkerContent),
        ArchiveRequest::kO3DMarkerContentLength);
  }

  // Serialize the created O3D scene graph to JSON.
  StringWriter out_writer(StringWriter::LF);
  JsonWriter json_writer(&out_writer, 2);
  if (!options.pretty_print) {
    json_writer.BeginCompacting();
  }
  Serializer::Options serializer_options(
      options.binary ? Serializer::Options::kBinaryOutputOn :
                       Serializer::Options::kBinaryOutputOff);
  Serializer serializer(
      &service_locator, &json_writer, file_writer.get(), serializer_options);
  serializer.SerializePack(pack.Get());
  json_writer.Close();
  out_writer.Close();
  if (!options.pretty_print) {
    json_writer.EndCompacting();
  }

  String json = out_writer.ToString();

  file_writer->AddFile("scene.json", json.length());
  file_writer->AddFileBytes(reinterpret_cast<const uint8*>(json.c_str()),
                            json.length());

  // Now add original data (e.g. compressed textures) collected during
  // the loading process.
  AddBinaryElements(collada, file_writer.get());

  file_writer->CloseTopLevelFile(true);

  pack->Destroy();
  if (error_messages) {
    *error_messages = error_collector.errors();
  }
  return true;
}

// Loads the input shader file and validates it.
bool Verify(const FilePath& in_filename,
            const FilePath& out_filename,
            const Options& options,
            String* error_messages) {
  FilePath source_filename(in_filename);
  // Create a service locator and renderer.
  ServiceLocator service_locator;
  EvaluationCounter evaluation_counter(&service_locator);
  ClassManager class_manager(&service_locator);
  ObjectManager object_manager(&service_locator);
  Profiler profiler(&service_locator);
  ErrorStatus error_status(&service_locator);

  // Collect error messages.
  ErrorCollector error_collector(&service_locator);

  scoped_ptr<Renderer> renderer(
      RendererStub::CreateDefault(&service_locator));
  renderer->InitCommon();

  Pack::Ref pack(object_manager.CreatePack());
  Transform* root = pack->Create<Transform>();
  root->set_name(O3D_STRING_CONSTANT("root"));

  Collada::Options collada_options;
  collada_options.condition_document = options.condition;
  collada_options.keep_original_data = false;
  Collada collada(pack.Get(), collada_options);

  ColladaConditioner conditioner(&service_locator);
  String vertex_shader_entry_point;
  String fragment_shader_entry_point;
  TemporaryFile temp_file;
  if (options.condition) {
    if (!TemporaryFile::Create(&temp_file)) {
      O3D_ERROR(&service_locator) << "Could not create temporary file";
      if (error_messages) {
        *error_messages = error_collector.errors();
      }
      return false;
    }
    SamplerStateList state_list;
    if (!conditioner.RewriteShaderFile(NULL,
                                       in_filename,
                                       temp_file.path(),
                                       &state_list,
                                       &vertex_shader_entry_point,
                                       &fragment_shader_entry_point)) {
      O3D_ERROR(&service_locator) << "Could not rewrite shader file";
      if (error_messages) {
        *error_messages = error_collector.errors();
      }
      return false;
    }
    source_filename = temp_file.path();
  } else {
    source_filename = in_filename;
  }

  std::string shader_source_in;
  // Load file into memory
  if (!file_util::ReadFileToString(source_filename, &shader_source_in)) {
    O3D_ERROR(&service_locator) << "Could not read shader file "
                                << FilePathToUTF8(source_filename).c_str();
    if (error_messages) {
      *error_messages = error_collector.errors();
    }
    return false;
  }

  Effect::MatrixLoadOrder matrix_load_order;
  scoped_ptr<Effect> effect(pack->Create<Effect>());
  if (!effect->ValidateFX(shader_source_in,
                          &vertex_shader_entry_point,
                          &fragment_shader_entry_point,
                          &matrix_load_order)) {
    O3D_ERROR(&service_locator) << "Could not validate shader file";
    if (error_messages) {
      *error_messages = error_collector.errors();
    }
    return false;
  }

  if (!conditioner.CompileHLSL(shader_source_in,
                               vertex_shader_entry_point,
                               fragment_shader_entry_point)) {
    O3D_ERROR(&service_locator) << "Could not HLSL compile shader file";
    if (error_messages) {
      *error_messages = error_collector.errors();
    }
    return false;
  }

  if (!conditioner.CompileCg(in_filename,
                             shader_source_in,
                             vertex_shader_entry_point,
                             fragment_shader_entry_point)) {
    O3D_ERROR(&service_locator) << "Could not Cg compile shader file";
    if (error_messages) {
      *error_messages = error_collector.errors();
    }
    return false;
  }

  // If we've validated the file, then we write out the conditioned
  // shader to the given output file, if there is one.
  if (options.condition && !out_filename.empty()) {
    if (file_util::WriteFile(out_filename, shader_source_in.c_str(),
                             static_cast<int>(shader_source_in.size())) == -1) {
      O3D_ERROR(&service_locator) << "Warning: Could not write to output file '"
                                  << FilePathToUTF8(in_filename).c_str() << "'";
    }
  }
  if (error_messages) {
    *error_messages = error_collector.errors();
  }
  return true;
}
}  // end namespace converter
}  // end namespace o3d
