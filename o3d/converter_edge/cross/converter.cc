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
#include "converter_edge/cross/converter.h"

#include <cmath>
#include <map>
#include <utility>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_ptr.h"
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
#include "core/cross/primitive.cc"
#include "import/cross/collada.h"
#include "import/cross/collada_conditioner.h"
#include "import/cross/file_output_stream_processor.h"
#include "import/cross/targz_generator.h"
#include "import/cross/archive_request.h"
#include "serializer/cross/serializer.h"
#include "utils/cross/file_path_utils.h"
#include "utils/cross/json_writer.h"
#include "utils/cross/string_writer.h"
#include "utils/cross/temporary_file.h"

namespace o3d {

namespace {
static const float kPi = 3.14159265358979f;
static const float kEpsilon = 0.0001f;

void AddBinaryElements(const Collada& collada,
                       TarGzGenerator* archive_generator) {
  const ColladaDataMap& data_map(collada.original_data_map());
  std::vector<FilePath> paths = data_map.GetOriginalDataFilenames();
  for (std::vector<FilePath>::const_iterator iter = paths.begin();
       iter != paths.end();
       ++iter) {
    const std::string& data = data_map.GetOriginalData(*iter);

    archive_generator->AddFile(FilePathToUTF8(*iter), data.size());
    archive_generator->AddFileBytes(
        reinterpret_cast<const uint8*>(data.c_str()),
        data.length());
  }
}
}  // end anonymous namespace

namespace converter {
// Constructor, make sure points order is unique.
// If x-coordinates are the same, compare y-coordinate.
// If y values are also the same, compare z.
Edge::Edge(Point3 p1, Point3 p2) {
  bool isRightOrder = false;
  if (fabs(p1.getX() - p2.getX()) < kEpsilon) {
    if (fabs(p1.getY() - p2.getY()) < kEpsilon) {
      if (p1.getZ() < p2.getZ())
        isRightOrder = true;
    } else {
      if (p1.getY() < p2.getY())
        isRightOrder = true;
    }
  } else {
    if (p1.getX() < p2.getX())
      isRightOrder = true;
  }
  if (isRightOrder) {
    pts.push_back(p1);
    pts.push_back(p2);
  } else {
    pts.push_back(p2);
    pts.push_back(p1);
  }
}

// less than operator overload, necessary function for edge-triangle map.
bool operator<(const Edge& left, const Edge& right) {
  // compare two edges by their actually coordinates, not indices.
  // Beceause two different indices may point to one actually coordinates.
  if (dist(left.pts[0], right.pts[0]) < kEpsilon) {
    if (fabs(left.pts[1].getX() - right.pts[1].getX()) < kEpsilon) {
      if (fabs(left.pts[1].getY() - right.pts[1].getY()) < kEpsilon) {
        return left.pts[1].getZ() < right.pts[1].getZ();
      }
      return left.pts[1].getY() < right.pts[1].getY();
    }
    return left.pts[1].getX() < right.pts[1].getX();
  } else {
    if (fabs(left.pts[0].getX() - right.pts[0].getX()) < kEpsilon) {
      if (fabs(left.pts[0].getY() - right.pts[0].getY()) < kEpsilon) {
        return left.pts[0].getZ() < right.pts[0].getZ();
      }
      return left.pts[0].getY() < right.pts[0].getY();
    }
    return left.pts[0].getX() < right.pts[0].getX();
  }
}

void CheckSharpEdge(const Edge& shared_edge,
                    const std::vector<Triangle>& triangle_list,
                    std::vector<Edge>* sharp_edges, float threshold) {
  for (size_t i = 0; i < triangle_list.size(); i++)
    for (size_t j = i + 1; j < triangle_list.size(); j++) {
      Triangle t1 = triangle_list[i];
      Triangle t2 = triangle_list[j];
      int same_vertices_count = 0;
      // Same triangle might be stored twice to represent inner and outer faces.
      // Check the order of indices of vertices to not mix inner and outer faces
      // togeter.
      std::vector<int> same_vertices_pos;
      for (int k = 0; k < 3; k++)
        for (int l = 0; l < 3; l++) {
          if (dist(t1.pts[k], t2.pts[l]) < kEpsilon) {
            same_vertices_count++;
            same_vertices_pos.push_back(k);
            same_vertices_pos.push_back(l);
          }
        }
      if (same_vertices_count != 2)
        continue;
      // check the order of positions to make sure triangles are on
      // the same face.
      int i1 = same_vertices_pos[2] - same_vertices_pos[0];
      int i2 = same_vertices_pos[3] - same_vertices_pos[1];
      // if triangles are on different faces.
      if (!(i1 * i2 == -1 || i1 * i2 == 2 || i1 * i2 == -4))
        continue;

      Vector3 v12 = t1.pts[1] - t1.pts[0];
      Vector3 v13 = t1.pts[2] - t1.pts[0];
      Vector3 n1 = cross(v12, v13);
      Vector3 v22 = t2.pts[1] - t2.pts[0];
      Vector3 v23 = t2.pts[2] - t2.pts[0];
      Vector3 n2 = cross(v22, v23);
      float iAngle = acos(dot(n1, n2) / (length(n1) * length(n2)));
      iAngle = iAngle * 180 / kPi;
      if (iAngle >= threshold) {
        sharp_edges->push_back(shared_edge);
        return;
      }
    }
}

// create a single color material and effect and attach it to
// sharp edge primitive.
Material* GetSingleColorMaterial(Pack::Ref pack, const Vector3& edge_color) {
  Material* singleColorMaterial = pack->Create<Material>();
  singleColorMaterial->set_name("singleColorMaterial");
  ParamString* lighting_param = singleColorMaterial->CreateParam<ParamString>(
              Collada::kLightingTypeParamName);
  lighting_param->set_value(Collada::kLightingTypeConstant);
  ParamFloat4* emissive_param = singleColorMaterial->CreateParam<ParamFloat4>(
                                Collada::kMaterialParamNameEmissive);
  emissive_param->set_value(Float4(edge_color.getX(), edge_color.getY(),
                            edge_color.getZ(), 1));

  return singleColorMaterial;
}

// insert edge-triangle pair to edge triangle map.
void InsertEdgeTrianglePair(const Edge& edge, const Triangle& triangle,
                            std::map<Edge, std::vector<Triangle>>* et_map) {
  std::map<Edge, std::vector<Triangle>>::iterator iter1 =
    et_map->find(edge);
  if (iter1 == et_map->end()) {
    std::vector<Triangle> same_edge_triangle_list;
    same_edge_triangle_list.push_back(triangle);
    et_map->insert(make_pair(edge, same_edge_triangle_list));
  } else {
    iter1->second.push_back(triangle);
  }
}

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

  scoped_ptr<Renderer> renderer(
      Renderer::CreateDefaultRenderer(&service_locator));
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

  // Add edges whose normals angle is larger than predefined threshold.
  if (options.enable_add_sharp_edge) {
    std::vector<Shape*> shapes = pack->GetByClass<Shape>();
    for (unsigned s = 0; s < shapes.size(); ++s) {
      Shape* shape = shapes[s];
      const ElementRefArray& elements = shape->GetElementRefs();
      for (unsigned e = 0; e < elements.size(); ++e) {
        if (elements[e]->IsA(Primitive::GetApparentClass())) {
          Primitive* primitive = down_cast<Primitive*>(elements[e].Get());
          // Get vertices and indices of this primitive.
          if (primitive->primitive_type() != Primitive::TRIANGLELIST)
            continue;
          FieldReadAccessor<Point3> vertices;
          FieldReadAccessorUnsignedInt indices;
          if (!GetVerticesAccessor(primitive, 0, &vertices))
            return false;

          unsigned int index_count;
          if (primitive->indexed()) {
            if (!Primitive::GetIndexCount(Primitive::TRIANGLELIST,
                                          primitive->number_primitives(),
                                          &index_count))
              continue;

            if (!GetIndicesAccessor(primitive, &indices,
                                    primitive->start_index(), index_count))
              continue;
            index_count = std::min(index_count, indices.max_index());
          } else {
            index_count = primitive->number_vertices();
            indices.InitializeJustCount(primitive->start_index(), index_count);
          }
          // If there are no vertices then exit early.
          if (vertices.max_index() == 0) {
            continue;
          }
          // generate triangle list.
          int prim = 0;
          std::vector<Triangle> triangle_list;
          std::vector<Point3> point_list;
          for (size_t i = 0; i < 48; i++) {
            point_list.push_back(vertices[i]);
          }
          for (unsigned int prim_base = 0; prim_base + 2 < index_count;
               prim_base += 3) {
            Triangle triangle(vertices[indices[prim_base + 0]],
                              vertices[indices[prim_base + 1]],
                              vertices[indices[prim_base + 2]]);
            triangle_list.push_back(triangle);
          }
          // build edge and triangle map.
          std::map<Edge, std::vector<Triangle>> edge_triangle_map;
          for (size_t i = 0; i < triangle_list.size(); i++) {
            Triangle triangle = triangle_list[i];
            Edge e1(triangle.pts[0], triangle.pts[1]);
            Edge e2(triangle.pts[1], triangle.pts[2]);
            Edge e3(triangle.pts[0], triangle.pts[2]);
            InsertEdgeTrianglePair(e1, triangle, &edge_triangle_map);
            InsertEdgeTrianglePair(e2, triangle, &edge_triangle_map);
            InsertEdgeTrianglePair(e3, triangle, &edge_triangle_map);
          }
          // go through the edge-triangle map.
          std::map<Edge, std::vector<Triangle>>::iterator iter;
          std::vector<Edge> sharp_edges;
          for (iter = edge_triangle_map.begin();
               iter != edge_triangle_map.end(); iter++) {
            if (iter->second.size() < 2)
              continue;
            CheckSharpEdge(iter->first, iter->second, &sharp_edges,
                           options.sharp_edge_threshold);
          }

          if (sharp_edges.size() > 0) {
            Primitive* sharp_edge_primitive = pack->Create<Primitive>();
            sharp_edge_primitive->SetOwner(shape);
            sharp_edge_primitive->set_name("sharp_edge_primitive");
            StreamBank* stream_bank = pack->Create<StreamBank>();

            VertexBuffer* vertex_buffer = pack->Create<VertexBuffer>();
            vertex_buffer->set_name("sharp_edges_vertex_buffer");
            size_t num_vertices = sharp_edges.size() * 2;

            Field* field = vertex_buffer->CreateField(
                           FloatField::GetApparentClass(), 3);
            if (!vertex_buffer->AllocateElements(static_cast<unsigned int>(
                                                 sharp_edges.size() * 2))) {
              O3D_ERROR(&service_locator) << "Failed to allocate vertex buffer";
              return NULL;
            }
            scoped_array<float> values(new float[num_vertices * 3]);
            for (unsigned int i = 0; i < num_vertices; i++) {
              Point3 currentPoint;
              if (i % 2 == 0)
                currentPoint = sharp_edges[i / 2].pts[0];
              else
                currentPoint = sharp_edges[i / 2].pts[1];
              values[i * 3 + 0] = currentPoint.getX();
              values[i * 3 + 1] = currentPoint.getY();
              values[i * 3 + 2] = currentPoint.getZ();
            }
            field->SetFromFloats(&values[0], 3, 0,
                                 static_cast<unsigned int>(num_vertices));
            stream_bank->SetVertexStream(Stream::POSITION, 0, field, 0);
            stream_bank->set_name("sharp_edges_stream_bank");

            Material* material =
                   GetSingleColorMaterial(pack, options.sharp_edge_color);
            sharp_edge_primitive->set_material(material);
            sharp_edge_primitive->set_primitive_type(Primitive::LINELIST);
            sharp_edge_primitive->set_number_vertices(static_cast<int>(
                                                      sharp_edges.size() * 2));
            sharp_edge_primitive->set_number_primitives(static_cast<int>(
                                                        sharp_edges.size()));
            sharp_edge_primitive->set_stream_bank(stream_bank);
          }
        }
      }
    }
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
  FILE* out_file = file_util::OpenFile(out_filename, "wb");
  if (out_file == NULL) {
    O3D_ERROR(&service_locator) << "Could not open output file \""
                                << FilePathToUTF8(out_filename).c_str()
                                << "\"";
    if (error_messages) {
      *error_messages += error_collector.errors();
    }
    return false;
  }

  // Create an archive file and serialize the JSON scene graph and assets to it.
  FileOutputStreamProcessor stream_processor(out_file);
  TarGzGenerator archive_generator(&stream_processor);

  archive_generator.AddFile(ArchiveRequest::O3D_MARKER,
                            ArchiveRequest::O3D_MARKER_CONTENT_LENGTH);
  archive_generator.AddFileBytes(
      reinterpret_cast<const uint8*>(ArchiveRequest::O3D_MARKER_CONTENT),
      ArchiveRequest::O3D_MARKER_CONTENT_LENGTH);

  // Serialize the created O3D scene graph to JSON.
  StringWriter out_writer(StringWriter::LF);
  JsonWriter json_writer(&out_writer, 2);
  if (!options.pretty_print) {
    json_writer.BeginCompacting();
  }
  Serializer serializer(&service_locator, &json_writer, &archive_generator);
  serializer.SerializePack(pack.Get());
  json_writer.Close();
  out_writer.Close();
  if (!options.pretty_print) {
    json_writer.EndCompacting();
  }

  String json = out_writer.ToString();

  archive_generator.AddFile("scene.json", json.length());
  archive_generator.AddFileBytes(reinterpret_cast<const uint8*>(json.c_str()),
                                 json.length());

  // Now add original data (e.g. compressed textures) collected during
  // the loading process.
  AddBinaryElements(collada, &archive_generator);

  archive_generator.Close(true);

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
      Renderer::CreateDefaultRenderer(&service_locator));
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
