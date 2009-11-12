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
#include "precompile.h"

#include "conditioner.h"

#include <cstdio>
#include <map>
#include <vector>

const float kEpsilon = 0.001f;
static const float kPi = 3.14159265358979f;
FCDMaterial* edge_material = NULL;
FCDEffect* edge_effect = NULL;

// Constructor, make sure points order is unique. If x-coordinate is
// the same, compare y-coordinate. If y value is also the same, compare z.
Edge::Edge(Point3 p1, Point3 p2, uint32 i1, uint32 i2) {
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
    indices.push_back(i1);
    indices.push_back(i2);
  } else {
    pts.push_back(p2);
    pts.push_back(p1);
    indices.push_back(i2);
    indices.push_back(i1);
  }
}

// less than operator overload, necessary function for edge-triangle map.
bool operator<(const Edge& left, const Edge& right) {
  // compare two edges by their actually coordinates.
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

// go through triangles who share this edge. And check whether
// the max normal angle is larger than the threshold.
void CheckSharpEdge(const Edge& shared_edge,
                    const std::vector<Triangle>& triangle_list,
                    std::vector<Edge>* sharp_edges, float threshold) {
  for (size_t i = 0; i < triangle_list.size(); i++)
    for (size_t j = i + 1; j < triangle_list.size(); j++) {
      Triangle t1 = triangle_list[i];
      Triangle t2 = triangle_list[j];
      int same_vertices_count = 0;
      // Same triangle might be stored twice to represent inner and
      // outer faces. Check the order of indices of vertices to not
      // mix inner and outer faces togeter.
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

NodeInstance* CreateInstanceTree(FCDSceneNode *node) {
  NodeInstance *instance = new NodeInstance(node);
  NodeInstance::NodeInstanceList &children = instance->children();
  for (size_t i = 0; i < node->GetChildrenCount(); ++i) {
    FCDSceneNode *child_node = node->GetChild(i);
    NodeInstance *child_instance = CreateInstanceTree(child_node);
    children.push_back(child_instance);
  }
  return instance;
}

// go through all polygons in geom_instance, and add all sharp edges
// as a new polygon in geom. And also, add material and effect based
// on the given sharpEdgeColor option.
void BuildSharpEdge(FCDocument* doc, FCDGeometryInstance* geom_instance,
                    const Options& options) {
  FCDGeometry* geom = static_cast<FCDGeometry*>(geom_instance->GetEntity());
  if (!(geom && geom->IsMesh()))
    return;
  FCDGeometryMesh* mesh = geom->GetMesh();
  FCDGeometryPolygonsTools::Triangulate(mesh);
  FCDGeometryPolygonsTools::GenerateUniqueIndices(mesh, NULL, NULL);

  size_t num_polygons = mesh->GetPolygonsCount();
  size_t num_indices = mesh->GetFaceVertexCount();
  if (num_polygons <= 0 || num_indices <= 0) return;
  FCDGeometrySource* pos_source =
      mesh->FindSourceByType(FUDaeGeometryInput::POSITION);
  if (pos_source == NULL) return;
  size_t num_vertices = pos_source->GetValueCount();
  float* pos_source_data = pos_source->GetData();
  std::vector<Point3> point_list;
  for (size_t i = 0; i + 2 < num_vertices * 3; i += 3) {
    Point3 point(pos_source_data[i + 0],
                 pos_source_data[i + 1],
                 pos_source_data[i + 2]);
    point_list.push_back(point);
  }

  for (size_t p = 0; p < num_polygons; ++p) {
    FCDGeometryPolygons* polys = mesh->GetPolygons(p);

    if (polys->GetPrimitiveType() != FCDGeometryPolygons::POLYGONS)
      continue;
    FCDGeometryPolygonsInput* input = polys->GetInput(0);
    size_t size = input->GetIndexCount();
    if (size == 0) continue;
    // meshed triangle list.
    size_t vertices_per_primitive = 3;
    if (size % vertices_per_primitive != 0) {
      continue;
    }
    uint32* indices = input->GetIndices();
    size_t indexCount = input->GetIndexCount();
    // create triangle list.
    std::vector<Triangle> triangle_list;
    for (size_t i = 0; i + 2 < indexCount; i += 3) {
      Triangle triangle(point_list[indices[i + 0]],
                        point_list[indices[i + 1]],
                        point_list[indices[i + 2]],
                        indices[i + 0],
                        indices[i + 1],
                        indices[i + 2]);
      triangle_list.push_back(triangle);
    }
    std::map<Edge, std::vector<Triangle>> edge_triangle_map;
    for (size_t i = 0; i < triangle_list.size(); i++) {
      Triangle triangle = triangle_list[i];
      Edge e1(triangle.pts[0], triangle.pts[1],
              triangle.indices[0], triangle.indices[1]);
      Edge e2(triangle.pts[1], triangle.pts[2],
              triangle.indices[1], triangle.indices[2]);
      Edge e3(triangle.pts[0], triangle.pts[2],
              triangle.indices[0], triangle.indices[2]);
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
                     options.soften_edge_threshold);
    }
    if (sharp_edges.size() > 0) {
      FCDGeometryPolygons* edge_polys = mesh->AddPolygons();
      edge_polys->AddFaceVertexCount(sharp_edges.size() * 2);
      edge_polys->SetPrimitiveType(FCDGeometryPolygons::LINES);
      FCDGeometrySource* vertex_source = mesh->GetVertexSource(0);
      FCDGeometryPolygonsInput* edge_input =
        edge_polys->AddInput(vertex_source, 0);
      FUDaeGeometryInput::Semantic semantic = edge_input->GetSemantic();
      if (edge_input == NULL)
        return;
      FCDGeometrySource* edge_source = edge_input->GetSource();
      if (edge_source == NULL)
        return;
      for (size_t i = 0; i < sharp_edges.size(); i++) {
        edge_input->AddIndex(sharp_edges[i].indices[0]);
        edge_input->AddIndex(sharp_edges[i].indices[1]);
      }
      edge_input->SetIndexCount(sharp_edges.size() * 2);
      edge_input->SetOffset(0);

      if (edge_material == NULL) {
        // add material to material lib.
        FCDMaterialLibrary* material_library = doc->GetMaterialLibrary();
        FCDEffectLibrary* effect_library = doc->GetEffectLibrary();
        edge_material =
          static_cast<FCDMaterial*>(material_library->AddEntity());
        edge_material->SetDaeId("o3d_hard_edge_materialID");
        edge_material->SetName(L"o3d_hard_edge_material");
        edge_effect = static_cast<FCDEffect*>(effect_library->AddEntity());
        edge_effect->SetDaeId("o3d_hard_edge_effectID");
        edge_effect->SetName(L"o3d_hard_edge_effect");
        if (edge_effect == NULL || edge_material == NULL)
          return;
        edge_material->SetEffect(edge_effect);
        FCDEffectStandard* edge_effect_profile =
          static_cast<FCDEffectStandard*>(edge_effect->AddProfile(
                                          FUDaeProfileType::COMMON));
        edge_effect_profile->SetLightingType(FCDEffectStandard::LAMBERT);
        edge_effect_profile->
          SetEmissionColor(FMVector4(options.sharp_edge_color.getX(),
                                     options.sharp_edge_color.getY(),
                                     options.sharp_edge_color.getZ(), 1));
      }
      // add material instance to visual scenes lib.
      FCDMaterialInstance* edge_material_instance =
        geom_instance->AddMaterialInstance(edge_material, edge_polys);
      edge_material_instance->SetSemantic(L"o3d_hard_edge_material");
      edge_polys->SetMaterialSemantic(L"o3d_hard_edge_material");
    }
  }
}

// go through the collada tree and import instance. if found a
// geometry instance, call BuildSharpEdge function.
bool ImportTreeInstances(FCDocument* doc,
                         NodeInstance *node_instance,
                         const Options& options) {
  FCDSceneNode *node = node_instance->node();
  // recursively import the rest of the nodes in the tree
  const NodeInstance::NodeInstanceList &children = node_instance->children();
  for (size_t i = 0; i < children.size(); ++i) {
    if (!ImportTreeInstances(doc, children[i], options)) {
      return false;
    }
  }
  for (size_t i = 0; i < node->GetInstanceCount(); ++i) {
    FCDEntityInstance* instance = node->GetInstance(i);
    FCDCamera* camera(NULL);
    FCDGeometryInstance* geom_instance(NULL);
    FCDMaterialInstance* mat_instance(NULL);
    // Import each node based on what kind of entity it is
    switch (instance->GetEntityType()) {
      case FCDEntity::GEOMETRY: {
        // geometry entity
        geom_instance = static_cast<FCDGeometryInstance*>(instance);
        BuildSharpEdge(doc, geom_instance, options);
        break;
      }
      case FCDEntity::CAMERA:
      case FCDEntity::CONTROLLER:
      default: break;
    }
  }
  return true;
}

bool ConditionDoc(FCDocument* doc, const Options& options) {
  // The root of the instance node tree.
  NodeInstance* instance_root_;
  bool status = false;
  // Import the scene objects, starting at the root.
  FCDSceneNode* scene = doc->GetVisualSceneInstance();
  if (scene) {
    instance_root_ = CreateInstanceTree(scene);
    if (ImportTreeInstances(doc, instance_root_, options)) {
      status = true;
    }
    delete instance_root_;
  }
  return status;
}

bool Condition(const wchar_t* in_filename, const wchar_t* out_filename,
               const Options& options) {
  FCollada::Initialize();
  FCDocument* doc = FCollada::NewTopDocument();
  bool retval = false;
  if (doc) {
    // Load and parse the COLLADA file.
    if (FCollada::LoadDocumentFromFile(doc, in_filename)) {
      doc->SetFileUrl(out_filename);

      // condition it.
      retval = ConditionDoc(doc, options);
      if (retval) {
        FCollada::SaveDocument(doc, out_filename);
      }
    } else {
      printf("Error:  couldn't open the input file.\n");
    }
    doc->Release();
  } else {
    printf("Internal error:  Couldn't create FCollada document.\n");
  }
  FCollada::Release();
  return retval;
}