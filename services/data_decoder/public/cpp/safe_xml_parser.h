// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_XML_PARSER_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_XML_PARSER_H_

#include <initializer_list>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/optional.h"

namespace base {
class Value;
}

namespace service_manager {
class Connector;
}

namespace data_decoder {

// Callback invoked when parsing with ParseXml has finished.
// |value| contains the base::Value dictionary structure representing the parsed
// XML. See xml_parser.mojom for an example.
// If the parsing failed, |error| contains an error message and |value| is null.
using XmlParserCallback =
    base::OnceCallback<void(std::unique_ptr<base::Value> value,
                            const base::Optional<std::string>& error)>;

// Parses |unsafe_xml| safely in a utility process and invokes |callback| when
// done. The XML document is transformed into a base::Value dictionary
// structure, with special keys holding the tag name and child nodes.
// |connector| is the connector provided by the service manager and is used to
// retrieve the XML parser service. It's commonly retrieved from a service
// manager connection context object that the embedder provides.
void ParseXml(service_manager::Connector* connector,
              const std::string& unsafe_xml,
              XmlParserCallback callback);

// Below are convenience methods for handling the elements returned by
// ParseXml().

// Returns true if |element| is an XML element with a tag name |name|, false
// otherwise.
bool IsXmlElementNamed(const base::Value& element, const std::string& name);

// Returns true if |element| is an XML element with a type |type|, false
// otherwise. Valid types are data_decoder::mojom::XmlParser::kElementType,
// kTextNodeType or kCDataNodeType.
bool IsXmlElementOfType(const base::Value& element, const std::string& type);

// Sets |name| with the tag name of |element| and returns true.
// Returns false if |element| does not represent a node with a tag or is not a
// valid XML element.
bool GetXmlElementTagName(const base::Value& element, std::string* name);

// Sets |text| with the text of |element| and returns true.
// Returns false if |element| does not contain any text (if it's not a text or
// CData node).
bool GetXmlElementText(const base::Value& element, std::string* text);

// Returns the number of children of |element| named |name|.
int GetXmlElementChildrenCount(const base::Value& element,
                               const std::string& name);

// Returns the first child of |element| with the type |type|, or null if there
// are no children with that type.
// |type| are data_decoder::mojom::XmlParser::kElementType, kTextNodeType or
// kCDataNodeType.
const base::Value* GetXmlElementChildWithType(const base::Value& element,
                                              const std::string& type);

// Returns the first child of |element| with the tag |tag|, or null if there
// are no children with that tag.
const base::Value* GetXmlElementChildWithTag(const base::Value& element,
                                             const std::string& tag);

// Returns the value of the element path |path| starting at |element|, or null
// if any element in |path| is missing. Note that if there are more than one
// element matching any part of the path, the first one is used and
// |unique_path| is set to false. It is set to true otherwise and can be null if
// not needed.
const base::Value* FindXmlElementPath(
    const base::Value& element,
    std::initializer_list<base::StringPiece> path,
    bool* unique_path);

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_XML_PARSER_H_
