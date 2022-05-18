// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/xfa_basic_data.h"

#include <iterator>
#include <utility>

#include "fxjs/xfa/cjx_boolean.h"
#include "fxjs/xfa/cjx_container.h"
#include "fxjs/xfa/cjx_datawindow.h"
#include "fxjs/xfa/cjx_delta.h"
#include "fxjs/xfa/cjx_desc.h"
#include "fxjs/xfa/cjx_draw.h"
#include "fxjs/xfa/cjx_encrypt.h"
#include "fxjs/xfa/cjx_eventpseudomodel.h"
#include "fxjs/xfa/cjx_exclgroup.h"
#include "fxjs/xfa/cjx_extras.h"
#include "fxjs/xfa/cjx_field.h"
#include "fxjs/xfa/cjx_form.h"
#include "fxjs/xfa/cjx_handler.h"
#include "fxjs/xfa/cjx_hostpseudomodel.h"
#include "fxjs/xfa/cjx_instancemanager.h"
#include "fxjs/xfa/cjx_layoutpseudomodel.h"
#include "fxjs/xfa/cjx_logpseudomodel.h"
#include "fxjs/xfa/cjx_manifest.h"
#include "fxjs/xfa/cjx_model.h"
#include "fxjs/xfa/cjx_node.h"
#include "fxjs/xfa/cjx_occur.h"
#include "fxjs/xfa/cjx_packet.h"
#include "fxjs/xfa/cjx_script.h"
#include "fxjs/xfa/cjx_signaturepseudomodel.h"
#include "fxjs/xfa/cjx_source.h"
#include "fxjs/xfa/cjx_subform.h"
#include "fxjs/xfa/cjx_textnode.h"
#include "fxjs/xfa/cjx_tree.h"
#include "fxjs/xfa/cjx_treelist.h"
#include "fxjs/xfa/cjx_wsdlconnection.h"
#include "fxjs/xfa/cjx_xfa.h"
#include "xfa/fxfa/fxfa_basic.h"

namespace {

struct PacketTableRecord {
  uint32_t hash;
  XFA_PACKETINFO info;
};

const PacketTableRecord kPacketTable[] = {
#undef PCKT____
#define PCKT____(a, b, c, d, e, f) \
  {a, {XFA_PacketType::c, XFA_PacketMatch::e, XFA_PacketSupport::f, b, d}},
#include "xfa/fxfa/parser/packets.inc"
#undef PCKT____
};

struct ElementRecord {
  uint32_t hash;  // Hashed as wide string.
  XFA_Element element;
  XFA_Element parent;
};

// Contains read-only data that do not require relocation.
// Parts that require relocation are in `kElementNames` below.
constexpr ElementRecord kElementRecords[] = {
#undef ELEM____
#define ELEM____(a, b, c, d) {a, XFA_Element::c, XFA_Element::d},
#include "xfa/fxfa/parser/elements.inc"
#undef ELEM____
};

constexpr const char* kElementNames[] = {
#undef ELEM____
#define ELEM____(a, b, c, d) b,
#include "xfa/fxfa/parser/elements.inc"
#undef ELEM____
};

static_assert(std::size(kElementRecords) == std::size(kElementNames),
              "Size mismatch");

struct AttributeRecord {
  uint32_t hash;  // Hashed as wide string.
  XFA_Attribute attribute;
  XFA_ScriptType script_type;
};

// Contains read-only data that do not require relocation.
// Parts that require relocation are in `kAttributeNames` below.
constexpr AttributeRecord kAttributeRecords[] = {
#undef ATTR____
#define ATTR____(a, b, c, d) {a, XFA_Attribute::c, XFA_ScriptType::d},
#include "xfa/fxfa/parser/attributes.inc"
#undef ATTR____
};

constexpr const char* kAttributeNames[] = {
#undef ATTR____
#define ATTR____(a, b, c, d) b,
#include "xfa/fxfa/parser/attributes.inc"
#undef ATTR____
};

static_assert(std::size(kAttributeRecords) == std::size(kAttributeNames),
              "Size mismatch");

struct AttributeValueRecord {
  // Associated entry in `kAttributeValueNames` hashed as WideString.
  uint32_t uHash;
  XFA_AttributeValue eName;
};

// Contains read-only data that do not require relocation.
// Parts that require relocation are in `kAttributeValueNames` below.
constexpr AttributeValueRecord kAttributeValueRecords[] = {
#undef VALUE____
#define VALUE____(a, b, c) {a, XFA_AttributeValue::c},
#include "xfa/fxfa/parser/attribute_values.inc"
#undef VALUE____
};

constexpr const char* kAttributeValueNames[] = {
#undef VALUE____
#define VALUE____(a, b, c) b,
#include "xfa/fxfa/parser/attribute_values.inc"
#undef VALUE____
};

static_assert(std::size(kAttributeValueRecords) ==
                  std::size(kAttributeValueNames),
              "Size mismatch");

struct ElementAttributeRecord {
  XFA_Element element;
  XFA_Attribute attribute;
};

// Contains read-only data that do not require relocation.
// Parts that require relocation are in `kElementAttributeCallbacks` below.
constexpr ElementAttributeRecord kElementAttributeRecords[] = {
#undef ELEM_ATTR____
#define ELEM_ATTR____(a, b, c) {XFA_Element::a, XFA_Attribute::b},
#include "xfa/fxfa/parser/element_attributes.inc"
#undef ELEM_ATTR____
};

constexpr XFA_ATTRIBUTE_CALLBACK kElementAttributeCallbacks[] = {
#undef ELEM_ATTR____
#define ELEM_ATTR____(a, b, c) c##_static,
#include "xfa/fxfa/parser/element_attributes.inc"
#undef ELEM_ATTR____
};

static_assert(std::size(kElementAttributeRecords) ==
                  std::size(kElementAttributeCallbacks),
              "Size mismatch");

}  // namespace

XFA_PACKETINFO XFA_GetPacketByIndex(XFA_PacketType ePacket) {
  return kPacketTable[static_cast<uint8_t>(ePacket)].info;
}

absl::optional<XFA_PACKETINFO> XFA_GetPacketByName(WideStringView wsName) {
  uint32_t hash = FX_HashCode_GetW(wsName);
  auto* elem = std::lower_bound(
      std::begin(kPacketTable), std::end(kPacketTable), hash,
      [](const PacketTableRecord& a, uint32_t hash) { return a.hash < hash; });
  if (elem != std::end(kPacketTable) && wsName.EqualsASCII(elem->info.name))
    return elem->info;
  return absl::nullopt;
}

ByteStringView XFA_ElementToName(XFA_Element elem) {
  return kElementNames[static_cast<size_t>(elem)];
}

XFA_Element XFA_GetElementByName(WideStringView name) {
  uint32_t hash = FX_HashCode_GetW(name);
  auto* elem = std::lower_bound(
      std::begin(kElementRecords), std::end(kElementRecords), hash,
      [](const ElementRecord& a, uint32_t hash) { return a.hash < hash; });
  if (elem == std::end(kElementRecords))
    return XFA_Element::Unknown;

  size_t index = std::distance(std::begin(kElementRecords), elem);
  return name.EqualsASCII(kElementNames[index]) ? elem->element
                                                : XFA_Element::Unknown;
}

ByteStringView XFA_AttributeToName(XFA_Attribute attr) {
  return kAttributeNames[static_cast<size_t>(attr)];
}

absl::optional<XFA_ATTRIBUTEINFO> XFA_GetAttributeByName(WideStringView name) {
  uint32_t hash = FX_HashCode_GetW(name);
  auto* elem = std::lower_bound(
      std::begin(kAttributeRecords), std::end(kAttributeRecords), hash,
      [](const AttributeRecord& a, uint32_t hash) { return a.hash < hash; });
  if (elem == std::end(kAttributeRecords))
    return absl::nullopt;

  size_t index = std::distance(std::begin(kAttributeRecords), elem);
  if (!name.EqualsASCII(kAttributeNames[index]))
    return absl::nullopt;

  XFA_ATTRIBUTEINFO result;
  result.attribute = elem->attribute;
  result.eValueType = elem->script_type;
  return result;
}

ByteStringView XFA_AttributeValueToName(XFA_AttributeValue item) {
  return kAttributeValueNames[static_cast<int32_t>(item)];
}

absl::optional<XFA_AttributeValue> XFA_GetAttributeValueByName(
    WideStringView name) {
  auto* it =
      std::lower_bound(std::begin(kAttributeValueRecords),
                       std::end(kAttributeValueRecords), FX_HashCode_GetW(name),
                       [](const AttributeValueRecord& arg, uint32_t hash) {
                         return arg.uHash < hash;
                       });
  if (it == std::end(kAttributeValueRecords))
    return absl::nullopt;

  size_t index = std::distance(std::begin(kAttributeValueRecords), it);
  if (!name.EqualsASCII(kAttributeValueNames[index]))
    return absl::nullopt;

  return it->eName;
}

absl::optional<XFA_SCRIPTATTRIBUTEINFO> XFA_GetScriptAttributeByName(
    XFA_Element element,
    WideStringView attribute_name) {
  absl::optional<XFA_ATTRIBUTEINFO> attr =
      XFA_GetAttributeByName(attribute_name);
  if (!attr.has_value())
    return absl::nullopt;

  while (element != XFA_Element::Unknown) {
    auto compound_key = std::make_pair(element, attr.value().attribute);
    auto* it = std::lower_bound(
        std::begin(kElementAttributeRecords),
        std::end(kElementAttributeRecords), compound_key,
        [](const ElementAttributeRecord& arg,
           const std::pair<XFA_Element, XFA_Attribute>& key) {
          return std::make_pair(arg.element, arg.attribute) < key;
        });
    if (it != std::end(kElementAttributeRecords) &&
        compound_key == std::make_pair(it->element, it->attribute)) {
      XFA_SCRIPTATTRIBUTEINFO result;
      result.attribute = attr.value().attribute;
      result.eValueType = attr.value().eValueType;
      size_t index = std::distance(std::begin(kElementAttributeRecords), it);
      result.callback = kElementAttributeCallbacks[index];
      return result;
    }
    element = kElementRecords[static_cast<size_t>(element)].parent;
  }
  return absl::nullopt;
}
