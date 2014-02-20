# Copyright (C) 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
# Copyright (C) 2006 Anders Carlsson <andersca@mac.com>
# Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
# Copyright (C) 2006 Alexey Proskuryakov <ap@webkit.org>
# Copyright (C) 2006 Apple Computer, Inc.
# Copyright (C) 2007, 2008, 2009, 2012 Google Inc.
# Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>
# Copyright (C) Research In Motion Limited 2010. All rights reserved.
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
# Copyright (C) 2012 Ericsson AB. All rights reserved.
# Copyright (C) 2013, 2014 Samsung Electronics. All rights reserved.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
#

package Block;

# Sample code:
#   my $outer = new Block("Free Name 1", "namespace Foo {", "} // namespace Foo");
#   $outer->add("    void foo() {}");
#   my $inner = new Block("Free Name 2", "namespace Bar {", "} // namespace Bar");
#   $inner->add("    void bar() {}");
#   $outer->add($inner);
#   print $outer->toString();
#
# Output code:
#   namespace Foo {
#       void foo() {}
#   namespace Bar {
#       void bar() {}
#   } // namespace Bar
#   } // namespace Foo

sub new
{
    my $package = shift;
    my $name = shift || "Anonymous block";
    my $header = shift || "";
    my $footer = shift || "";

    my $object = {
        "name" => $name,
        "header" => [$header],
        "footer" => [$footer],
        "contents" => [],
    };
    bless $object, $package;
    return $object;
}

sub addHeader
{
    my $object = shift;
    my $header = shift || "";

    push(@{$object->{header}}, $header);
}

sub addFooter
{
    my $object = shift;
    my $footer = shift || "";

    push(@{$object->{footer}}, $footer);
}

sub add
{
    my $object = shift;
    my $content = shift || "";

    push(@{$object->{contents}}, $content);
}

sub toString
{
    my $object = shift;

    my $header = join "", @{$object->{header}};
    my $footer = join "", @{$object->{footer}};
    my $code = "";
    $code .= "/* BEGIN " . $object->{name} . " */\n" if $verbose;
    $code .=  $header . "\n" if $header;
    for my $content (@{$object->{contents}}) {
        if (ref($content) eq "Block") {
            $code .= $content->toString();
        } else {
            $code .= $content;
        }
    }
    $code .=  $footer . "\n" if $footer;
    $code .= "/* END " . $object->{name} . " */\n" if $verbose;
    return $code;
}


package code_generator_v8;

use strict;
use Cwd;
use File::Basename;
use File::Find;
use File::Spec;

my $idlDocument;
my $idlDirectories;
my $preprocessor;
my $verbose;
my $interfaceIdlFiles;
my $writeFileOnlyIfChanged;
my $sourceRoot;

# Cache of IDL file pathnames.
my $idlFiles;
my $cachedInterfaces = {};

my %implIncludes = ();
my %headerIncludes = ();

# Header code structure:
# Root                    ... Copyright, include duplication check
#   Conditional           ... #if FEATURE ... #endif  (to be removed soon)
#     Includes
#     NameSpaceWebCore
#       Class
#         ClassPublic
#         ClassPrivate
my %header;

# Implementation code structure:
# Root                    ... Copyright
#   Conditional           ... #if FEATURE ... #endif  (to be removed soon)
#     Includes
#     NameSpaceWebCore
#     NameSpaceInternal   ... namespace ${implClassName}V8Internal in case of non-callback
my %implementation;

# The integer primitive types, a map from an IDL integer type to its
# binding-level type name.
#
# NOTE: For the unsigned types, the "UI" prefix is used (and not
# "Ui"), so as to match onto the naming of V8Binding conversion
# methods (and not the Typed Array naming scheme for unsigned types.)
my %integerTypeHash = ("byte" => "Int8",
                       "octet" => "UInt8",
                       "short" => "Int16",
                       "long" => "Int32",
                       "long long" => "Int64",
                       "unsigned short" => "UInt16",
                       "unsigned long" => "UInt32",
                       "unsigned long long" => "UInt64"
                      );

# Other primitive types
#
# Promise is not yet in the Web IDL spec but is going to be speced
# as primitive types in the future.
# Since V8 dosn't provide Promise primitive object currently,
# primitiveTypeHash doesn't contain Promise.
my %primitiveTypeHash = ("Date" => 1,
                         "DOMString" => 1,
                         "DOMTimeStamp" => 1,  # typedef unsigned long long
                         "boolean" => 1,
                         "void" => 1,
                         "float" => 1,
                         "double" => 1,
                        );

my %nonWrapperTypes = ("CompareHow" => 1,
                       "Dictionary" => 1,
                       "EventHandler" => 1,
                       "EventListener" => 1,
                       "MediaQueryListListener" => 1,
                       "NodeFilter" => 1,
                       "SerializedScriptValue" => 1,
                       "any" => 1,
                      );

my %typedArrayHash = ("ArrayBuffer" => [],
                      "ArrayBufferView" => [],
                      "Uint8Array" => ["unsigned char", "v8::kExternalUnsignedByteArray"],
                      "Uint8ClampedArray" => ["unsigned char", "v8::kExternalPixelArray"],
                      "Uint16Array" => ["unsigned short", "v8::kExternalUnsignedShortArray"],
                      "Uint32Array" => ["unsigned int", "v8::kExternalUnsignedIntArray"],
                      "Int8Array" => ["signed char", "v8::kExternalByteArray"],
                      "Int16Array" => ["short", "v8::kExternalShortArray"],
                      "Int32Array" => ["int", "v8::kExternalIntArray"],
                      "Float32Array" => ["float", "v8::kExternalFloatArray"],
                      "Float64Array" => ["double", "v8::kExternalDoubleArray"],
                     );

my %domNodeTypes = ("Attr" => 1,
                    "CDATASection" => 1,
                    "CharacterData" => 1,
                    "Comment" => 1,
                    "Document" => 1,
                    "DocumentFragment" => 1,
                    "DocumentType" => 1,
                    "Element" => 1,
                    "Entity" => 1,
                    "HTMLDocument" => 1,
                    "Node" => 1,
                    "Notation" => 1,
                    "ProcessingInstruction" => 1,
                    "ShadowRoot" => 1,
                    "Text" => 1,
                    "TestNode" => 1,
                    "TestInterfaceDocument" => 1,
                    "TestInterfaceNode" => 1,
                    "XMLDocument" => 1,
                   );

my %callbackFunctionTypeHash = ();

my %enumTypeHash = ();

my %svgAttributesInHTMLHash = ("class" => 1, "id" => 1, "onabort" => 1, "onclick" => 1,
                               "onerror" => 1, "onload" => 1, "onmousedown" => 1,
                               "onmouseenter" => 1, "onmouseleave" => 1,
                               "onmousemove" => 1, "onmouseout" => 1, "onmouseover" => 1,
                               "onmouseup" => 1, "onresize" => 1, "onscroll" => 1,
                               "onunload" => 1);

my %svgTypeNewPropertyImplementation = (
    "SVGAngle" => 1,
    "SVGLength" => 1,
    "SVGLengthList" => 1,
    "SVGNumber" => 1,
    "SVGNumberList" => 1,
    "SVGPoint" => 1,
    "SVGPointList" => 1,
    "SVGPreserveAspectRatio" => 1,
    "SVGRect" => 1,
    "SVGString" => 1,
    "SVGStringList" => 1,
);

my %svgTypeNeedingTearOff = (
    "SVGMatrix" => "SVGMatrixTearOff",
    "SVGPathSegList" => "SVGPathSegListPropertyTearOff",
    "SVGTransform" => "SVGPropertyTearOff<SVGTransform>",
    "SVGTransformList" => "SVGTransformListPropertyTearOff"
);

my %svgTypeWithWritablePropertiesNeedingTearOff = (
    "SVGMatrix" => 1
);

# Default license header
my $licenseHeader = <<EOF;
/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

// This file has been auto-generated by code_generator_v8.pm. DO NOT MODIFY!

EOF

sub new
{
    my $object = shift;
    my $reference = { };

    $idlDocument = shift;
    $idlDirectories = shift;
    $preprocessor = shift;
    $verbose = shift;
    $interfaceIdlFiles = shift;
    $writeFileOnlyIfChanged = shift;

    $sourceRoot = dirname(dirname(dirname(Cwd::abs_path($0))));

    bless($reference, $object);
    return $reference;
}


sub IDLFileForInterface
{
    my $interfaceName = shift;

    unless ($idlFiles) {
        my @directories = map { $_ = "$sourceRoot/$_" if -d "$sourceRoot/$_"; $_ } @$idlDirectories;
        push(@directories, ".");

        $idlFiles = { };
        foreach my $idlFile (@$interfaceIdlFiles) {
            $idlFiles->{fileparse(basename($idlFile), ".idl")} = $idlFile;
        }

        my $wanted = sub {
            $idlFiles->{$1} = $File::Find::name if /^([A-Z].*)\.idl$/;
            $File::Find::prune = 1 if /^\../;
        };
        find($wanted, @directories);
    }

    return $idlFiles->{$interfaceName};
}

sub ParseInterface
{
    my $interfaceName = shift;

    if (exists $cachedInterfaces->{$interfaceName}) {
        return $cachedInterfaces->{$interfaceName};
    }

    # Step #1: Find the IDL file associated with 'interface'
    my $filename = IDLFileForInterface($interfaceName)
      or die("Could NOT find IDL file for interface \"$interfaceName\" $!\n");

    print "  |  |>  Parsing parent IDL \"$filename\" for interface \"$interfaceName\"\n" if $verbose;

    # Step #2: Parse the found IDL file (in quiet mode).
    my $parser = idl_parser->new(1);
    my $document = $parser->Parse($filename, $preprocessor);

    foreach my $interface (@{$document->interfaces}) {
        if ($interface->name eq $interfaceName or $interface->isPartial) {
            $cachedInterfaces->{$interfaceName} = $interface;
            return $interface;
        }
    }

    die("Could NOT find interface definition for $interfaceName in $filename");
}

sub GenerateInterface
{
    my $object = shift;
    my $interface = shift;

    %callbackFunctionTypeHash = map { $_->name => $_ } @{$idlDocument->callbackFunctions};
    %enumTypeHash = map { $_->name => $_->values } @{$idlDocument->enumerations};
    my $v8ClassName = GetV8ClassName($interface);
    my $defineName = $v8ClassName . "_h";
    my $internalNamespace = GetImplName($interface) . "V8Internal";

    my $conditionalString = GenerateConditionalString($interface);
    my $conditionalIf = "";
    my $conditionalEndif = "";
    if ($conditionalString) {
        $conditionalIf = "#if ${conditionalString}";
        $conditionalEndif = "#endif // ${conditionalString}";
    }

    $header{root} = new Block("ROOT", "", "");
    # FIXME: newlines should be generated by Block::toString().
    $header{conditional} = new Block("Conditional", "$conditionalIf", $conditionalEndif ? "$conditionalEndif" : "");
    $header{includes} = new Block("Includes", "", "");
    $header{nameSpaceWebCore} = new Block("Namespace WebCore", "\nnamespace WebCore {\n", "}");
    $header{class} = new Block("Class definition", "", "");
    $header{classPublic} = new Block("Class public:", "public:", "");
    $header{classPrivate} = new Block("Class private:", "private:", "");

    $header{root}->add($header{conditional});
    $header{conditional}->add($header{includes});
    $header{conditional}->add($header{nameSpaceWebCore});
    $header{nameSpaceWebCore}->add($header{class});
    $header{class}->add($header{classPublic});
    $header{class}->add($header{classPrivate});

    # - Add default header template
    $header{root}->addHeader($licenseHeader);
    $header{root}->addHeader("#ifndef $defineName\n#define $defineName\n");
    $header{root}->addFooter("#endif // $defineName");

    $implementation{root} = new Block("ROOT", "", "");
    $implementation{conditional} = new Block("Conditional", $conditionalIf, $conditionalEndif);
    $implementation{includes} = new Block("Includes", "", "");

    # FIXME: newlines should be generated by Block::toString().
    my $nameSpaceWebCoreBegin = "namespace WebCore {\n";
    my $nameSpaceWebCoreEnd = "} // namespace WebCore";
    $nameSpaceWebCoreBegin = "$nameSpaceWebCoreBegin\n" unless $interface->isCallback;
    $nameSpaceWebCoreBegin = "\n$nameSpaceWebCoreBegin" if $interface->isCallback;
    $implementation{nameSpaceWebCore} = new Block("Namespace WebCore", $nameSpaceWebCoreBegin, $nameSpaceWebCoreEnd);
    $implementation{nameSpaceInternal} = new Block("Internal namespace", "namespace $internalNamespace {\n", "} // namespace $internalNamespace\n");

    $implementation{root}->add($implementation{conditional});
    $implementation{conditional}->add($implementation{includes});
    $implementation{conditional}->add($implementation{nameSpaceWebCore});
    if (!$interface->isCallback) {
        $implementation{nameSpaceWebCore}->add($implementation{nameSpaceInternal});
    }

    # - Add default header template
    $implementation{root}->addHeader($licenseHeader);
    $implementation{root}->addHeader("#include \"config.h\"");
    $implementation{includes}->add("#include \"${v8ClassName}.h\"\n\n");

    # Start actual generation
    if ($interface->isCallback) {
        $object->GenerateCallbackHeader($interface);
        $object->GenerateCallbackImplementation($interface);
    } else {
        $object->GenerateHeader($interface);
        $object->GenerateImplementation($interface);
    }
}

sub AddToImplIncludes
{
    my $header = shift;
    $implIncludes{$header} = 1;
}

sub AddToHeaderIncludes
{
    my @includes = @_;

    for my $include (@includes) {
        $headerIncludes{$include} = 1;
    }
}

sub AddIncludesForType
{
    my $type = shift;

    return if IsPrimitiveType($type) or IsEnumType($type) or $type eq "object";

    $type = $1 if $type =~ /SVG\w+TearOff<(\w+)>/;

    # Handle non-wrapper types explicitly
    if ($type eq "any" || IsCallbackFunctionType($type)) {
        AddToImplIncludes("bindings/v8/ScriptValue.h");
    } elsif ($type eq "CompareHow") {
    } elsif ($type eq "Dictionary") {
        AddToImplIncludes("bindings/v8/Dictionary.h");
    } elsif ($type eq "EventHandler") {
        AddToImplIncludes("bindings/v8/V8AbstractEventListener.h");
        AddToImplIncludes("bindings/v8/V8EventListenerList.h");
    } elsif ($type eq "EventListener") {
        AddToImplIncludes("bindings/v8/BindingSecurity.h");
        AddToImplIncludes("bindings/v8/V8EventListenerList.h");
        AddToImplIncludes("core/frame/DOMWindow.h");
    } elsif ($type eq "MediaQueryListListener") {
        AddToImplIncludes("core/css/MediaQueryListListener.h");
    } elsif ($type eq "Promise") {
        AddToImplIncludes("bindings/v8/ScriptPromise.h");
    } elsif ($type eq "SerializedScriptValue") {
        AddToImplIncludes("bindings/v8/SerializedScriptValue.h");
    } elsif (IsTypedArrayType($type)) {
        AddToImplIncludes("bindings/v8/custom/V8${type}Custom.h");
    } elsif (my $arrayType = GetArrayType($type)) {
        AddIncludesForType($arrayType);
    } elsif (my $sequenceType = GetSequenceType($type)) {
        AddIncludesForType($sequenceType);
    } elsif (IsUnionType($type)) {
        for my $unionMemberType (@{$type->unionMemberTypes}) {
            AddIncludesForType($unionMemberType);
        }
    } else {
        AddToImplIncludes("V8${type}.h");
    }
}

sub HeaderFilesForInterface
{
    my $interfaceName = shift;
    my $implClassName = shift;

    my @includes = ();
    if (IsPrimitiveType($interfaceName) or IsEnumType($interfaceName) or IsCallbackFunctionType($interfaceName)) {
        # Not interface type, no header files
    } else {
        my $idlFilename = Cwd::abs_path(IDLFileForInterface($interfaceName)) or die("Could NOT find IDL file for interface \"$interfaceName\" $!\n");
        my $idlRelPath = File::Spec->abs2rel($idlFilename, $sourceRoot);
        push(@includes, dirname($idlRelPath) . "/" . $implClassName . ".h");
    }
    return @includes;
}

sub NeedsVisitDOMWrapper
{
    my $interface = shift;
    return ExtendedAttributeContains($interface->extendedAttributes->{"Custom"}, "VisitDOMWrapper") || $interface->extendedAttributes->{"SetWrapperReferenceFrom"} || $interface->extendedAttributes->{"SetWrapperReferenceTo"} || SVGTypeNeedsToHoldContextElement($interface->name);
}

sub GenerateVisitDOMWrapper
{
    my $interface = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    if (ExtendedAttributeContains($interface->extendedAttributes->{"Custom"}, "VisitDOMWrapper")) {
        return;
    }

    my $nativeType = GetNativeTypeForConversions($interface);
    my $code = <<END;
void ${v8ClassName}::visitDOMWrapper(void* object, const v8::Persistent<v8::Object>& wrapper, v8::Isolate* isolate)
{
    ${nativeType}* impl = fromInternalPointer(object);
END
    my $needSetWrapperReferenceContext = SVGTypeNeedsToHoldContextElement($interface->name) || $interface->extendedAttributes->{"SetWrapperReferenceTo"};
    if ($needSetWrapperReferenceContext) {
        $code .= <<END;
    v8::Local<v8::Object> creationContext = v8::Local<v8::Object>::New(isolate, wrapper);
    V8WrapperInstantiationScope scope(creationContext, isolate);
END
    }
    if (SVGTypeNeedsToHoldContextElement($interface->name)) {
        AddToImplIncludes("V8SVGPathElement.h");
        $code .= <<END;
    if (impl->contextElement()) {
        if (!DOMDataStore::containsWrapper<V8SVGElement>(impl->contextElement(), isolate))
            wrap(impl->contextElement(), creationContext, isolate);
        DOMDataStore::setWrapperReference<V8SVGElement>(wrapper, impl->contextElement(), isolate);
    }
END
    }
    for my $setReference (@{$interface->extendedAttributes->{"SetWrapperReferenceTo"}}) {
        my $setReferenceType = $setReference->type;
        my $setReferenceV8Type = "V8".$setReferenceType;

        my ($svgPropertyType, $svgListPropertyType, $svgNativeType) = GetSVGPropertyTypes($setReferenceType);
        if ($svgPropertyType) {
            $setReferenceType = $svgNativeType;
        }

        my $setReferenceName = $setReference->name;

        AddIncludesForType($setReferenceType);
        $code .= <<END;
    ${setReferenceType}* ${setReferenceName} = impl->${setReferenceName}();
    if (${setReferenceName}) {
        if (!DOMDataStore::containsWrapper<${setReferenceV8Type}>(${setReferenceName}, isolate))
            wrap(${setReferenceName}, creationContext, isolate);
        DOMDataStore::setWrapperReference<${setReferenceV8Type}>(wrapper, ${setReferenceName}, isolate);
    }
END
    }

    my $isReachableMethod = $interface->extendedAttributes->{"SetWrapperReferenceFrom"};
    if ($isReachableMethod) {
        AddToImplIncludes("bindings/v8/V8GCController.h");
        AddToImplIncludes("core/dom/Element.h");
        $code .= <<END;
    if (Node* owner = impl->${isReachableMethod}()) {
        Node* root = V8GCController::opaqueRootForGC(owner, isolate);
        isolate->SetReferenceFromGroup(v8::UniqueId(reinterpret_cast<intptr_t>(root)), wrapper);
        return;
    }
END
    }

    $code .= <<END;
    setObjectGroup(object, wrapper, isolate);
}

END
    $implementation{nameSpaceWebCore}->add($code);
}

sub GetSVGPropertyTypes
{
    my $implType = shift;

    my $svgPropertyType;
    my $svgListPropertyType;
    my $svgNativeType;

    return ($svgPropertyType, $svgListPropertyType, $svgNativeType) if not $implType =~ /SVG/;

    $svgNativeType = GetSVGTypeNeedingTearOff($implType);
    return ($svgPropertyType, $svgListPropertyType, $svgNativeType) if not $svgNativeType;

    # Append space to avoid compilation errors when using  PassRefPtr<$svgNativeType>
    $svgNativeType = "$svgNativeType ";

    my $svgWrappedNativeType = GetSVGWrappedTypeNeedingTearOff($implType);
    if ($svgNativeType =~ /SVGPropertyTearOff/) {
        $svgPropertyType = $svgWrappedNativeType;
        AddToHeaderIncludes("core/svg/properties/SVGAnimatedPropertyTearOff.h");
    } elsif ($svgNativeType =~ /SVGMatrixTearOff/) {
        $svgPropertyType = $svgWrappedNativeType;
        AddToHeaderIncludes("core/svg/properties/SVGMatrixTearOff.h");
        AddToHeaderIncludes("core/svg/properties/SVGAnimatedPropertyTearOff.h");
    } elsif ($svgNativeType =~ /SVGListPropertyTearOff/ or $svgNativeType =~ /SVGStaticListPropertyTearOff/ or $svgNativeType =~ /SVGTransformListPropertyTearOff/) {
        $svgListPropertyType = $svgWrappedNativeType;
        AddToHeaderIncludes("core/svg/properties/SVGAnimatedListPropertyTearOff.h");
    } elsif ($svgNativeType =~ /SVGPathSegListPropertyTearOff/) {
        $svgListPropertyType = $svgWrappedNativeType;
        AddToHeaderIncludes("core/svg/properties/SVGPathSegListPropertyTearOff.h");
    }

    return ($svgPropertyType, $svgListPropertyType, $svgNativeType);
}

sub GetIndexedGetterFunction
{
    my $interface = shift;

    return GetSpecialAccessorFunctionForType($interface, "getter", "unsigned long", 1);
}

sub GetIndexedSetterFunction
{
    my $interface = shift;

    return GetSpecialAccessorFunctionForType($interface, "setter", "unsigned long", 2);
}

sub GetIndexedDeleterFunction
{
    my $interface = shift;

    return GetSpecialAccessorFunctionForType($interface, "deleter", "unsigned long", 1);
}

sub GetNamedGetterFunction
{
    my $interface = shift;
    return GetSpecialAccessorFunctionForType($interface, "getter", "DOMString", 1);
}

sub GetNamedSetterFunction
{
    my $interface = shift;
    return GetSpecialAccessorFunctionForType($interface, "setter", "DOMString", 2);
}

sub GetNamedDeleterFunction
{
    my $interface = shift;
    return GetSpecialAccessorFunctionForType($interface, "deleter", "DOMString", 1);
}

sub GetSpecialAccessorFunctionForType
{
    my $interface = shift;
    my $special = shift;
    my $firstParameterType = shift;
    my $numberOfParameters = shift;

    foreach my $function (@{$interface->functions}) {
        my $specials = $function->specials;
        my $specialExists = grep { $_ eq $special } @$specials;
        my $parameters = $function->parameters;
        if ($specialExists and scalar(@$parameters) == $numberOfParameters and $parameters->[0]->type eq $firstParameterType) {
            return $function;
        }
    }

    return 0;
}

sub GetV8StringResourceMode
{
    my $extendedAttributes = shift;

    # Blink uses the non-standard identifier NullString instead of Web IDL
    # standard EmptyString, in [TreatNullAs=NullString] and [TreatUndefinedAs=NullString],
    # and does not support [TreatUndefinedAs=Null] or [TreatUndefinedAs=Missing]
    # https://sites.google.com/a/chromium.org/dev/blink/webidl/blink-idl-extended-attributes#TOC-TreatNullAs-a-p-TreatUndefinedAs-a-p-
    my $mode = "";
    if (($extendedAttributes->{"TreatNullAs"} and $extendedAttributes->{"TreatNullAs"} eq "NullString") and ($extendedAttributes->{"TreatUndefinedAs"} and $extendedAttributes->{"TreatUndefinedAs"} eq "NullString")) {
        $mode = "WithUndefinedOrNullCheck";
    } elsif ($extendedAttributes->{"TreatNullAs"} and $extendedAttributes->{"TreatNullAs"} eq "NullString") {
        $mode = "WithNullCheck";
    }
    return $mode;
}

sub GenerateHeader
{
    my $object = shift;
    my $interface = shift;

    my $interfaceName = $interface->name;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    LinkOverloadedFunctions($interface);

    # Ensure the IsDOMNodeType function is in sync.
    die("IsDOMNodeType is out of date with respect to $interfaceName") if IsDOMNodeType($interfaceName) != InheritsInterface($interface, "Node");

    my ($svgPropertyType, $svgListPropertyType, $svgNativeType) = GetSVGPropertyTypes($interfaceName);

    my $parentInterface = $interface->parent;
    AddToHeaderIncludes("V8${parentInterface}.h") if $parentInterface;
    AddToHeaderIncludes("bindings/v8/WrapperTypeInfo.h");
    AddToHeaderIncludes("bindings/v8/V8Binding.h");
    AddToHeaderIncludes("bindings/v8/V8DOMWrapper.h");
    AddToHeaderIncludes("heap/Handle.h");
    AddToHeaderIncludes(HeaderFilesForInterface($interfaceName, $implClassName));
    foreach my $headerInclude (sort keys(%headerIncludes)) {
        $header{includes}->add("#include \"${headerInclude}\"\n");
    }

    $header{nameSpaceWebCore}->addHeader("\ntemplate<typename PropertyType> class SVGPropertyTearOff;\n") if $svgPropertyType;
    if ($svgNativeType) {
        if ($svgNativeType =~ /SVGStaticListPropertyTearOff/) {
            $header{nameSpaceWebCore}->addHeader("\ntemplate<typename PropertyType> class SVGStaticListPropertyTearOff;\n");
        } else {
            $header{nameSpaceWebCore}->addHeader("\ntemplate<typename PropertyType> class SVGListPropertyTearOff;\n");
        }
    }

    $header{nameSpaceWebCore}->addHeader("\nclass Dictionary;") if $interface->extendedAttributes->{"EventConstructor"};

    my $nativeType = GetNativeTypeForConversions($interface);
    if ($interface->extendedAttributes->{"NamedConstructor"}) {
        $header{nameSpaceWebCore}->addHeader(<<END);

class V8${nativeType}Constructor {
public:
    static v8::Handle<v8::FunctionTemplate> domTemplate(v8::Isolate*, WrapperWorldType);
    static const WrapperTypeInfo wrapperTypeInfo;
};
END
    }

    $header{class}->addHeader("class $v8ClassName {");
    $header{class}->addFooter("};");

    $header{classPublic}->add(<<END);
    static bool hasInstance(v8::Handle<v8::Value>, v8::Isolate*);
    static v8::Handle<v8::FunctionTemplate> domTemplate(v8::Isolate*, WrapperWorldType);
    static ${nativeType}* toNative(v8::Handle<v8::Object> object)
    {
        return fromInternalPointer(object->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex));
    }
    static ${nativeType}* toNativeWithTypeCheck(v8::Isolate*, v8::Handle<v8::Value>);
    static const WrapperTypeInfo wrapperTypeInfo;
END

    $header{classPublic}->add("    static void derefObject(void*);\n");

    if (NeedsVisitDOMWrapper($interface)) {
        $header{classPublic}->add("    static void visitDOMWrapper(void*, const v8::Persistent<v8::Object>&, v8::Isolate*);\n");
    }

    if (InheritsExtendedAttribute($interface, "ActiveDOMObject")) {
        $header{classPublic}->add("    static ActiveDOMObject* toActiveDOMObject(v8::Handle<v8::Object>);\n");
    }

    if (InheritsInterface($interface, "EventTarget")) {
        $header{classPublic}->add("    static EventTarget* toEventTarget(v8::Handle<v8::Object>);\n");
    }

    if ($interfaceName eq "Window") {
        $header{classPublic}->add(<<END);
    static v8::Handle<v8::ObjectTemplate> getShadowObjectTemplate(v8::Isolate*, WrapperWorldType);
END
    }

    my @perContextEnabledFunctions;
    foreach my $function (@{$interface->functions}) {
        my $name = $function->name;
        next if $name eq "";
        my $attrExt = $function->extendedAttributes;

        if (HasCustomMethod($attrExt) && $function->{overloadIndex} == 1) {
            my $conditionalString = GenerateConditionalString($function);
            $header{classPublic}->add("#if ${conditionalString}\n") if $conditionalString;
            $header{classPublic}->add(<<END);
    static void ${name}MethodCustom(const v8::FunctionCallbackInfo<v8::Value>&);
END
            $header{classPublic}->add("#endif // ${conditionalString}\n") if $conditionalString;
        }
        if ($attrExt->{"PerContextEnabled"}) {
            push(@perContextEnabledFunctions, $function);
        }
    }

    if (IsConstructable($interface)) {
        $header{classPublic}->add("    static void constructorCallback(const v8::FunctionCallbackInfo<v8::Value>&);\n");
END
    }
    if (HasCustomConstructor($interface)) {
        $header{classPublic}->add("    static void constructorCustom(const v8::FunctionCallbackInfo<v8::Value>&);\n");
    }

    my @perContextEnabledAttributes;
    foreach my $attribute (@{$interface->attributes}) {
        my $name = $attribute->name;
        my $attrExt = $attribute->extendedAttributes;
        my $conditionalString = GenerateConditionalString($attribute);
        if (HasCustomGetter($attrExt) && !$attrExt->{"ImplementedBy"}) {
            $header{classPublic}->add("#if ${conditionalString}\n") if $conditionalString;
            $header{classPublic}->add(<<END);
    static void ${name}AttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>&);
END
            $header{classPublic}->add("#endif // ${conditionalString}\n") if $conditionalString;
        }
        if (HasCustomSetter($attribute) && !$attrExt->{"ImplementedBy"}) {
            $header{classPublic}->add("#if ${conditionalString}\n") if $conditionalString;
            $header{classPublic}->add(<<END);
    static void ${name}AttributeSetterCustom(v8::Local<v8::Value>, const v8::PropertyCallbackInfo<void>&);
END
            $header{classPublic}->add("#endif // ${conditionalString}\n") if $conditionalString;
        }
        if ($attrExt->{"PerContextEnabled"}) {
            push(@perContextEnabledAttributes, $attribute);
        }
    }

    GenerateHeaderNamedAndIndexedPropertyAccessors($interface);
    GenerateHeaderLegacyCallAsFunction($interface);
    GenerateHeaderCustomInternalFieldIndices($interface);

    my $toWrappedType;
    my $fromWrappedType;
    if ($interface->parent) {
        my $v8ParentClassName = "V8" . $interface->parent;
        $toWrappedType = "${v8ParentClassName}::toInternalPointer(impl)";
        $fromWrappedType = "static_cast<${nativeType}*>(${v8ParentClassName}::fromInternalPointer(object))";
    } else {
        $toWrappedType = "impl";
        $fromWrappedType = "static_cast<${nativeType}*>(object)";
    }

    $header{classPublic}->add(<<END);
    static inline void* toInternalPointer(${nativeType}* impl)
    {
        return $toWrappedType;
    }

    static inline ${nativeType}* fromInternalPointer(void* object)
    {
        return $fromWrappedType;
    }
END

    if ($interface->name eq "Window") {
        $header{classPublic}->add(<<END);
    static bool namedSecurityCheckCustom(v8::Local<v8::Object> host, v8::Local<v8::Value> key, v8::AccessType, v8::Local<v8::Value> data);
    static bool indexedSecurityCheckCustom(v8::Local<v8::Object> host, uint32_t index, v8::AccessType, v8::Local<v8::Value> data);
END
    }

    if (@perContextEnabledAttributes) {
        $header{classPublic}->add(<<END);
    static void installPerContextEnabledProperties(v8::Handle<v8::Object>, ${nativeType}*, v8::Isolate*);
END
    } else {
        $header{classPublic}->add(<<END);
    static void installPerContextEnabledProperties(v8::Handle<v8::Object>, ${nativeType}*, v8::Isolate*) { }
END
    }

    if (@perContextEnabledFunctions) {
        $header{classPublic}->add(<<END);
    static void installPerContextEnabledMethods(v8::Handle<v8::Object>, v8::Isolate*);
END
    } else {
        $header{classPublic}->add(<<END);
    static void installPerContextEnabledMethods(v8::Handle<v8::Object>, v8::Isolate*) { }
END
    }

    if ($interfaceName eq "HTMLElement") {
        $header{classPublic}->add(<<END);
    friend v8::Handle<v8::Object> createV8HTMLWrapper(HTMLElement*, v8::Handle<v8::Object> creationContext, v8::Isolate*);
    friend v8::Handle<v8::Object> createV8HTMLDirectWrapper(HTMLElement*, v8::Handle<v8::Object> creationContext, v8::Isolate*);
END
    } elsif ($interfaceName eq "SVGElement") {
        $header{classPublic}->add(<<END);
    friend v8::Handle<v8::Object> createV8SVGWrapper(SVGElement*, v8::Handle<v8::Object> creationContext, v8::Isolate*);
    friend v8::Handle<v8::Object> createV8SVGDirectWrapper(SVGElement*, v8::Handle<v8::Object> creationContext, v8::Isolate*);
    friend v8::Handle<v8::Object> createV8SVGFallbackWrapper(SVGElement*, v8::Handle<v8::Object> creationContext, v8::Isolate*);
END
    } elsif ($interfaceName eq "HTMLUnknownElement") {
        $header{classPublic}->add(<<END);
    friend v8::Handle<v8::Object> createV8HTMLFallbackWrapper(HTMLUnknownElement*, v8::Handle<v8::Object> creationContext, v8::Isolate*);
END
    } elsif ($interfaceName eq "Element") {
        $header{classPublic}->add(<<END);
    // This is a performance optimization hack. See V8Element::wrap.
    friend v8::Handle<v8::Object> wrap(Node*, v8::Handle<v8::Object> creationContext, v8::Isolate*);
END
    }
    $header{classPublic}->add("\n");  # blank line to separate classPrivate

    my $customToV8 = ExtendedAttributeContains($interface->extendedAttributes->{"Custom"}, "ToV8");
    my $passRefPtrType = GetPassRefPtrType($interface);
    if (!$customToV8) {
        $header{classPrivate}->add(<<END);
    friend v8::Handle<v8::Object> wrap(${nativeType}*, v8::Handle<v8::Object> creationContext, v8::Isolate*);
    static v8::Handle<v8::Object> createWrapper(${passRefPtrType}, v8::Handle<v8::Object> creationContext, v8::Isolate*);
END
    }

    if ($customToV8) {
        $header{nameSpaceWebCore}->add(<<END);

class ${nativeType};
v8::Handle<v8::Value> toV8(${nativeType}*, v8::Handle<v8::Object> creationContext, v8::Isolate*);

template<class CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& callbackInfo, ${nativeType}* impl)
{
    v8SetReturnValue(callbackInfo, toV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template<class CallbackInfo>
inline void v8SetReturnValueForMainWorld(const CallbackInfo& callbackInfo, ${nativeType}* impl)
{
     v8SetReturnValue(callbackInfo, toV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template<class CallbackInfo, class Wrappable>
inline void v8SetReturnValueFast(const CallbackInfo& callbackInfo, ${nativeType}* impl, Wrappable*)
{
     v8SetReturnValue(callbackInfo, toV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}
END
    } else {
        if (NeedsSpecialWrap($interface)) {
            $header{nameSpaceWebCore}->add(<<END);

v8::Handle<v8::Object> wrap(${nativeType}* impl, v8::Handle<v8::Object> creationContext, v8::Isolate*);
END
        } else {
            $header{nameSpaceWebCore}->add(<<END);

inline v8::Handle<v8::Object> wrap(${nativeType}* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    ASSERT(!DOMDataStore::containsWrapper<${v8ClassName}>(impl, isolate));
    return ${v8ClassName}::createWrapper(impl, creationContext, isolate);
}
END
        }

        $header{nameSpaceWebCore}->add(<<END);

inline v8::Handle<v8::Value> toV8(${nativeType}* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (UNLIKELY(!impl))
        return v8::Null(isolate);
    v8::Handle<v8::Value> wrapper = DOMDataStore::getWrapper<${v8ClassName}>(impl, isolate);
    if (!wrapper.IsEmpty())
        return wrapper;
    return wrap(impl, creationContext, isolate);
}

template<typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& callbackInfo, ${nativeType}* impl)
{
    if (UNLIKELY(!impl)) {
        v8SetReturnValueNull(callbackInfo);
        return;
    }
    if (DOMDataStore::setReturnValueFromWrapper<${v8ClassName}>(callbackInfo.GetReturnValue(), impl))
        return;
    v8::Handle<v8::Object> wrapper = wrap(impl, callbackInfo.Holder(), callbackInfo.GetIsolate());
    v8SetReturnValue(callbackInfo, wrapper);
}

template<typename CallbackInfo>
inline void v8SetReturnValueForMainWorld(const CallbackInfo& callbackInfo, ${nativeType}* impl)
{
    ASSERT(worldType(callbackInfo.GetIsolate()) == MainWorld);
    if (UNLIKELY(!impl)) {
        v8SetReturnValueNull(callbackInfo);
        return;
    }
    if (DOMDataStore::setReturnValueFromWrapperForMainWorld<${v8ClassName}>(callbackInfo.GetReturnValue(), impl))
        return;
    v8::Handle<v8::Value> wrapper = wrap(impl, callbackInfo.Holder(), callbackInfo.GetIsolate());
    v8SetReturnValue(callbackInfo, wrapper);
}

template<class CallbackInfo, class Wrappable>
inline void v8SetReturnValueFast(const CallbackInfo& callbackInfo, ${nativeType}* impl, Wrappable* wrappable)
{
    if (UNLIKELY(!impl)) {
        v8SetReturnValueNull(callbackInfo);
        return;
    }
    if (DOMDataStore::setReturnValueFromWrapperFast<${v8ClassName}>(callbackInfo.GetReturnValue(), impl, callbackInfo.Holder(), wrappable))
        return;
    v8::Handle<v8::Object> wrapper = wrap(impl, callbackInfo.Holder(), callbackInfo.GetIsolate());
    v8SetReturnValue(callbackInfo, wrapper);
}
END
    }

    $header{nameSpaceWebCore}->add(<<END);

inline v8::Handle<v8::Value> toV8($passRefPtrType impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8(impl.get(), creationContext, isolate);
}

template<class CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& callbackInfo, $passRefPtrType impl)
{
    v8SetReturnValue(callbackInfo, impl.get());
}

template<class CallbackInfo>
inline void v8SetReturnValueForMainWorld(const CallbackInfo& callbackInfo, $passRefPtrType impl)
{
    v8SetReturnValueForMainWorld(callbackInfo, impl.get());
}

template<class CallbackInfo, class Wrappable>
inline void v8SetReturnValueFast(const CallbackInfo& callbackInfo, $passRefPtrType impl, Wrappable* wrappable)
{
    v8SetReturnValueFast(callbackInfo, impl.get(), wrappable);
}

END

    if ($interface->extendedAttributes->{"EventConstructor"}) {
        $header{nameSpaceWebCore}->add("bool initialize${implClassName}(${implClassName}Init&, const Dictionary&, ExceptionState&, const v8::FunctionCallbackInfo<v8::Value>& info, const String& = \"\");\n\n");
    }
}

sub GetInternalFields
{
    my $interface = shift;

    my @customInternalFields = ();
    # Event listeners on DOM nodes are explicitly supported in the GC controller.
    if (!InheritsInterface($interface, "Node") &&
        InheritsInterface($interface, "EventTarget")) {
        push(@customInternalFields, "eventListenerCacheIndex");
    }
    # Persistent handle is stored in the last internal field.
    # FIXME: Remove this internal field. Since we need either of a persistent handle
    # (if the object is in oilpan) or a C++ pointer to the DOM object (if the object is not in oilpan),
    # we can share the internal field between the two cases.
    if (IsWillBeGarbageCollectedType($interface->name)) {
        push(@customInternalFields, "persistentHandleIndex");
    }
    return @customInternalFields;
}

sub GenerateHeaderCustomInternalFieldIndices
{
    my $interface = shift;
    my @customInternalFields = GetInternalFields($interface);
    my $customFieldCounter = 0;
    foreach my $customInternalField (@customInternalFields) {
        $header{classPublic}->add(<<END);
    static const int ${customInternalField} = v8DefaultWrapperInternalFieldCount + ${customFieldCounter};
END
        $customFieldCounter++;
    }
    $header{classPublic}->add(<<END);
    static const int internalFieldCount = v8DefaultWrapperInternalFieldCount + ${customFieldCounter};
END
}

sub GenerateHeaderNamedAndIndexedPropertyAccessors
{
    my $interface = shift;

    my $indexedGetterFunction = GetIndexedGetterFunction($interface);
    my $hasCustomIndexedGetter = $indexedGetterFunction && $indexedGetterFunction->extendedAttributes->{"Custom"};

    my $indexedSetterFunction = GetIndexedSetterFunction($interface);
    my $hasCustomIndexedSetter = $indexedSetterFunction && $indexedSetterFunction->extendedAttributes->{"Custom"};

    my $indexedDeleterFunction = GetIndexedDeleterFunction($interface);
    my $hasCustomIndexedDeleters = $indexedDeleterFunction && $indexedDeleterFunction->extendedAttributes->{"Custom"};

    my $namedGetterFunction = GetNamedGetterFunction($interface);
    my $hasCustomNamedGetter = $namedGetterFunction && HasCustomPropertyGetter($namedGetterFunction->extendedAttributes);

    my $namedSetterFunction = GetNamedSetterFunction($interface);
    my $hasCustomNamedSetter = $namedSetterFunction && $namedSetterFunction->extendedAttributes->{"Custom"};

    my $namedDeleterFunction = GetNamedDeleterFunction($interface);
    my $hasCustomNamedDeleter = $namedDeleterFunction && $namedDeleterFunction->extendedAttributes->{"Custom"};

    my $namedEnumeratorFunction = $namedGetterFunction && !$namedGetterFunction->extendedAttributes->{"NotEnumerable"};
    my $hasCustomNamedEnumerator = $namedEnumeratorFunction && ExtendedAttributeContains($namedGetterFunction->extendedAttributes->{"Custom"}, "PropertyEnumerator");
    my $hasCustomNamedQuery = $namedEnumeratorFunction && ExtendedAttributeContains($namedGetterFunction->extendedAttributes->{"Custom"}, "PropertyQuery");

    if ($hasCustomIndexedGetter) {
        $header{classPublic}->add("    static void indexedPropertyGetterCustom(uint32_t, const v8::PropertyCallbackInfo<v8::Value>&);\n");
    }

    if ($hasCustomIndexedSetter) {
        $header{classPublic}->add("    static void indexedPropertySetterCustom(uint32_t, v8::Local<v8::Value>, const v8::PropertyCallbackInfo<v8::Value>&);\n");
    }

    if ($hasCustomIndexedDeleters) {
        $header{classPublic}->add("    static void indexedPropertyDeleterCustom(uint32_t, const v8::PropertyCallbackInfo<v8::Boolean>&);\n");
    }

    if ($hasCustomNamedGetter) {
        $header{classPublic}->add("    static void namedPropertyGetterCustom(v8::Local<v8::String>, const v8::PropertyCallbackInfo<v8::Value>&);\n");
    }

    if ($hasCustomNamedSetter) {
        $header{classPublic}->add("    static void namedPropertySetterCustom(v8::Local<v8::String>, v8::Local<v8::Value>, const v8::PropertyCallbackInfo<v8::Value>&);\n");
    }

    if ($hasCustomNamedQuery) {
        $header{classPublic}->add("    static void namedPropertyQueryCustom(v8::Local<v8::String>, const v8::PropertyCallbackInfo<v8::Integer>&);\n");
    }

    if ($hasCustomNamedDeleter) {
        $header{classPublic}->add("    static void namedPropertyDeleterCustom(v8::Local<v8::String>, const v8::PropertyCallbackInfo<v8::Boolean>&);\n");
    }

    if ($hasCustomNamedEnumerator) {
        $header{classPublic}->add("    static void namedPropertyEnumeratorCustom(const v8::PropertyCallbackInfo<v8::Array>&);\n");
    }
}

sub GenerateHeaderLegacyCallAsFunction
{
    my $interface = shift;

    if (ExtendedAttributeContains($interface->extendedAttributes->{"Custom"}, "LegacyCallAsFunction")) {
        $header{classPublic}->add("    static void legacyCallCustom(const v8::FunctionCallbackInfo<v8::Value>&);\n");
    }
}

sub HasActivityLogging
{
    my $forMainWorldSuffix = shift;
    my $attrExt = shift;
    my $access = shift;

    if (!$attrExt->{"ActivityLogging"}) {
        return 0;
    }
    my $logAllAccess = ($attrExt->{"ActivityLogging"} =~ /^For/);  # No prefix, starts with For*Worlds suffix
    my $logGetter = ($attrExt->{"ActivityLogging"} =~ /^Getter/);
    my $logSetter = ($attrExt->{"ActivityLogging"} =~ /^Setter/);
    my $logOnlyIsolatedWorlds = ($attrExt->{"ActivityLogging"} =~ /ForIsolatedWorlds$/);

    if ($logOnlyIsolatedWorlds && $forMainWorldSuffix eq "ForMainWorld") {
        return 0;
    }
    return $logAllAccess || ($logGetter && $access eq "Getter") || ($logSetter && $access eq "Setter");
}

sub IsConstructable
{
    my $interface = shift;

    return $interface->extendedAttributes->{"CustomConstructor"} || $interface->extendedAttributes->{"Constructor"} || $interface->extendedAttributes->{"EventConstructor"};
}

sub HasCustomConstructor
{
    my $interface = shift;

    return $interface->extendedAttributes->{"CustomConstructor"};
}

sub HasCustomGetter
{
    my $attrExt = shift;
    my $custom = $attrExt->{"Custom"};
    return $custom && ($custom eq "VALUE_IS_MISSING" || $custom eq "Getter");
}

sub HasCustomSetter
{
    my $attribute = shift;
    my $custom = $attribute->extendedAttributes->{"Custom"};
    return $custom && ($custom eq "VALUE_IS_MISSING" || $custom eq "Setter") && !IsReadonly($attribute);
}

sub HasCustomMethod
{
    my $attrExt = shift;
    return $attrExt->{"Custom"};
}

sub HasCustomPropertyGetter
{
    my $attrExt = shift;
    my $custom = $attrExt->{"Custom"};
    return $custom && ($custom eq "VALUE_IS_MISSING" || ExtendedAttributeContains($custom, "PropertyGetter"));
}

sub IsReadonly
{
    my $attribute = shift;
    my $attrExt = $attribute->extendedAttributes;
    return $attribute->isReadOnly && !$attrExt->{"Replaceable"} && !$attrExt->{"PutForwards"};
}

sub GetV8ClassName
{
    my $interface = shift;
    return "V8" . $interface->name;
}

sub GetImplName
{
    my $interfaceOrAttributeOrFunction = shift;
    return $interfaceOrAttributeOrFunction->extendedAttributes->{"ImplementedAs"} || $interfaceOrAttributeOrFunction->name;
}

sub GetImplNameFromImplementedBy
{
    my $implementedBy = shift;

    my $interface = ParseInterface($implementedBy);

    return $interface->extendedAttributes->{"ImplementedAs"} || $implementedBy;
}

sub GenerateDomainSafeFunctionGetter
{
    my $function = shift;
    my $interface = shift;
    my $forMainWorldSuffix = shift;

    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $funcName = $function->name;

    my $functionLength = GetFunctionLength($function);
    my $signature = "v8::Signature::New(info.GetIsolate(), ${v8ClassName}::domTemplate(info.GetIsolate(), currentWorldType))";
    if ($function->extendedAttributes->{"DoNotCheckSignature"}) {
        $signature = "v8::Local<v8::Signature>()";
    }

    my $newTemplateParams = "${implClassName}V8Internal::${funcName}MethodCallback${forMainWorldSuffix}, v8Undefined(), $signature";

    AddToImplIncludes("bindings/v8/BindingSecurity.h");
    $implementation{nameSpaceInternal}->add(<<END);
static void ${funcName}OriginSafeMethodGetter${forMainWorldSuffix}(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    static int domTemplateKey; // This address is used for a key to look up the dom template.
    WrapperWorldType currentWorldType = worldType(info.GetIsolate());
    V8PerIsolateData* data = V8PerIsolateData::from(info.GetIsolate());
    v8::Handle<v8::FunctionTemplate> privateTemplate = data->domTemplate(currentWorldType, &domTemplateKey, $newTemplateParams, $functionLength);

    v8::Handle<v8::Object> holder = info.This()->FindInstanceInPrototypeChain(${v8ClassName}::domTemplate(info.GetIsolate(), currentWorldType));
    if (holder.IsEmpty()) {
        // This is only reachable via |object.__proto__.func|, in which case it
        // has already passed the same origin security check
        v8SetReturnValue(info, privateTemplate->GetFunction());
        return;
    }
    ${implClassName}* imp = ${v8ClassName}::toNative(holder);
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), imp->frame(), DoNotReportSecurityError)) {
        static int sharedTemplateKey; // This address is used for a key to look up the dom template.
        v8::Handle<v8::FunctionTemplate> sharedTemplate = data->domTemplate(currentWorldType, &sharedTemplateKey, $newTemplateParams, $functionLength);
        v8SetReturnValue(info, sharedTemplate->GetFunction());
        return;
    }

    v8::Local<v8::Value> hiddenValue = getHiddenValue(info.GetIsolate(), info.This(), "${funcName}");
    if (!hiddenValue.IsEmpty()) {
        v8SetReturnValue(info, hiddenValue);
        return;
    }

    v8SetReturnValue(info, privateTemplate->GetFunction());
}

END
    $implementation{nameSpaceInternal}->add(<<END);
static void ${funcName}OriginSafeMethodGetterCallback${forMainWorldSuffix}(v8::Local<v8::String>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMGetter");
    ${implClassName}V8Internal::${funcName}OriginSafeMethodGetter${forMainWorldSuffix}(info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}

END
}

sub GenerateDomainSafeFunctionSetter
{
    my $interface = shift;

    my $interfaceName = $interface->name();
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    AddToImplIncludes("bindings/v8/BindingSecurity.h");
    $implementation{nameSpaceInternal}->add(<<END);
static void ${implClassName}OriginSafeMethodSetter(v8::Local<v8::String> name, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<void>& info)
{
    v8::Handle<v8::Object> holder = info.This()->FindInstanceInPrototypeChain(${v8ClassName}::domTemplate(info.GetIsolate(), worldType(info.GetIsolate())));
    if (holder.IsEmpty())
        return;
    ${implClassName}* imp = ${v8ClassName}::toNative(holder);
    v8::String::Utf8Value attributeName(name);
    ExceptionState exceptionState(ExceptionState::SetterContext, *attributeName, "${interfaceName}", info.Holder(), info.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), imp->frame(), exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }

    setHiddenValue(info.GetIsolate(), info.This(), name, jsValue);
}

static void ${implClassName}OriginSafeMethodSetterCallback(v8::Local<v8::String> name, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<void>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMSetter");
    ${implClassName}V8Internal::${implClassName}OriginSafeMethodSetter(name, jsValue, info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}

END
}

sub GenerateConstructorGetter
{
    my $interface = shift;
    my $implClassName = GetImplName($interface);

    $implementation{nameSpaceInternal}->add(<<END);
static void ${implClassName}ConstructorGetter(v8::Local<v8::String>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    v8::Handle<v8::Value> data = info.Data();
    ASSERT(data->IsExternal());
    V8PerContextData* perContextData = V8PerContextData::from(info.Holder()->CreationContext());
    if (!perContextData)
        return;
    v8SetReturnValue(info, perContextData->constructorForType(WrapperTypeInfo::unwrap(data)));
}

END
}

sub GenerateFeatureObservation
{
    my $measureAs = shift;

    if ($measureAs) {
        AddToImplIncludes("core/frame/UseCounter.h");
        return "    UseCounter::count(callingExecutionContext(info.GetIsolate()), UseCounter::${measureAs});\n";
    }

    return "";
}

sub GenerateDeprecationNotification
{
    my $deprecateAs = shift;
    if ($deprecateAs) {
        AddToImplIncludes("core/frame/UseCounter.h");
        return "    UseCounter::countDeprecation(callingExecutionContext(info.GetIsolate()), UseCounter::${deprecateAs});\n";
    }
    return "";
}

sub GenerateActivityLogging
{
    my $accessType = shift;
    my $interface = shift;
    my $propertyName = shift;

    my $interfaceName = $interface->name;

    AddToImplIncludes("bindings/v8/V8DOMActivityLogger.h");

    my $code = "";
    if ($accessType eq "Method") {
        $code .= <<END;
    V8PerContextData* contextData = V8PerContextData::from(info.GetIsolate()->GetCurrentContext());
    if (contextData && contextData->activityLogger()) {
        Vector<v8::Handle<v8::Value> > loggerArgs = toNativeArguments<v8::Handle<v8::Value> >(info, 0);
        contextData->activityLogger()->log("${interfaceName}.${propertyName}", info.Length(), loggerArgs.data(), "${accessType}");
    }
END
    } elsif ($accessType eq "Setter") {
        $code .= <<END;
    V8PerContextData* contextData = V8PerContextData::from(info.GetIsolate()->GetCurrentContext());
    if (contextData && contextData->activityLogger()) {
        v8::Handle<v8::Value> loggerArg[] = { jsValue };
        contextData->activityLogger()->log("${interfaceName}.${propertyName}", 1, &loggerArg[0], "${accessType}");
    }
END
    } elsif ($accessType eq "Getter") {
        $code .= <<END;
    V8PerContextData* contextData = V8PerContextData::from(info.GetIsolate()->GetCurrentContext());
    if (contextData && contextData->activityLogger())
        contextData->activityLogger()->log("${interfaceName}.${propertyName}", 0, 0, "${accessType}");
END
    } else {
        die "Unrecognized activity logging access type";
    }

    return $code;
}

sub GenerateNormalAttributeGetterCallback
{
    my $attribute = shift;
    my $interface = shift;
    my $forMainWorldSuffix = shift;
    my $exposeJSAccessors = shift;

    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $attrExt = $attribute->extendedAttributes;
    my $attrName = $attribute->name;

    my $conditionalString = GenerateConditionalString($attribute);
    my $code = "";
    $code .= "#if ${conditionalString}\n" if $conditionalString;

    if ($exposeJSAccessors) {
        $code .= "static void ${attrName}AttributeGetterCallback${forMainWorldSuffix}(const v8::FunctionCallbackInfo<v8::Value>& info)\n";
    } else {
        $code .= "static void ${attrName}AttributeGetterCallback${forMainWorldSuffix}(v8::Local<v8::String>, const v8::PropertyCallbackInfo<v8::Value>& info)\n";
    }
    $code .= "{\n";
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"Blink\", \"DOMGetter\");\n";
    $code .= GenerateFeatureObservation($attrExt->{"MeasureAs"});
    $code .= GenerateDeprecationNotification($attrExt->{"DeprecateAs"});
    if (HasActivityLogging($forMainWorldSuffix, $attrExt, "Getter")) {
        $code .= GenerateActivityLogging("Getter", $interface, "${attrName}");
    }
    if (HasCustomGetter($attrExt)) {
        $code .= "    ${v8ClassName}::${attrName}AttributeGetterCustom(info);\n";
    } else {
        $code .= "    ${implClassName}V8Internal::${attrName}AttributeGetter${forMainWorldSuffix}(info);\n";
    }
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"V8\", \"V8Execution\");\n";
    $code .= "}\n";
    $code .= "#endif // ${conditionalString}\n" if $conditionalString;
    $code .= "\n";

    $implementation{nameSpaceInternal}->add($code);
}

sub GetCachedAttribute
{
    my $attribute = shift;
    my $attrExt = $attribute->extendedAttributes;
    if (($attribute->type eq "any" || $attribute->type eq "SerializedScriptValue") && $attrExt->{"CachedAttribute"}) {
        return $attrExt->{"CachedAttribute"};
    }
    return "";
}

sub GenerateNormalAttributeGetter
{
    my $attribute = shift;
    my $interface = shift;
    my $forMainWorldSuffix = shift;
    my $exposeJSAccessors = shift;

    my $interfaceName = $interface->name;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $attrExt = $attribute->extendedAttributes;
    my $attrName = $attribute->name;
    my $attrType = $attribute->type;
    my $attrCached = GetCachedAttribute($attribute);

    if (HasCustomGetter($attrExt)) {
        return;
    }

    AssertNotSequenceType($attrType);
    my $nativeType = GetNativeType($attribute->type, $attribute->extendedAttributes, "");
    my $svgNativeType = GetSVGTypeNeedingTearOff($interfaceName);

    my $conditionalString = GenerateConditionalString($attribute);
    my $code = "";
    $code .= "#if ${conditionalString}\n" if $conditionalString;
    if ($exposeJSAccessors) {
        $code .= "static void ${attrName}AttributeGetter${forMainWorldSuffix}(const v8::FunctionCallbackInfo<v8::Value>& info)\n";
    } else {
        $code .= "static void ${attrName}AttributeGetter${forMainWorldSuffix}(const v8::PropertyCallbackInfo<v8::Value>& info)\n";
    }
    $code .= "{\n";
    if ($svgNativeType) {
        my $svgWrappedNativeType = GetSVGWrappedTypeNeedingTearOff($interfaceName);
        if ($svgWrappedNativeType =~ /List/) {
            $code .= <<END;
    $svgNativeType* imp = ${v8ClassName}::toNative(info.Holder());
END
        } else {
            $code .= <<END;
    $svgNativeType* wrapper = ${v8ClassName}::toNative(info.Holder());
    $svgWrappedNativeType& impInstance = wrapper->propertyReference();
    $svgWrappedNativeType* imp = &impInstance;
END
        }
    } elsif ($attrExt->{"OnPrototype"} || $attrExt->{"Unforgeable"}) {
        if ($interfaceName eq "Window") {
            $code .= <<END;
    v8::Handle<v8::Object> holder = info.Holder();
END
        } else {
            # perform lookup first
            $code .= <<END;
    v8::Handle<v8::Object> holder = info.This()->FindInstanceInPrototypeChain(${v8ClassName}::domTemplate(info.GetIsolate(), worldType(info.GetIsolate())));
    if (holder.IsEmpty())
        return;
END
        }
        $code .= <<END;
    ${implClassName}* imp = ${v8ClassName}::toNative(holder);
END
    } else {
        my $reflect = $attribute->extendedAttributes->{"Reflect"};
        my $url = $attribute->extendedAttributes->{"URL"};
        if ($reflect && !$url && InheritsInterface($interface, "Node") && $attrType eq "DOMString") {
            # Generate super-compact call for regular attribute getter:
            my ($functionName, @arguments) = GetterExpression($interfaceName, $attribute);
            my $getterExpression = "imp->${functionName}(" . join(", ", @arguments) . ")";
            $code .= "    Element* imp = V8Element::toNative(info.Holder());\n";
            if ($attribute->extendedAttributes->{"ReflectOnly"}) {
                $code .= "    String resultValue = ${getterExpression};\n";
                $code .= GenerateReflectOnlyCheck($attribute->extendedAttributes, "    ");
                $getterExpression = "resultValue";
            }
            $code .= "    v8SetReturnValueString(info, ${getterExpression}, info.GetIsolate());\n";
            $code .= "}\n";
            $code .= "#endif // ${conditionalString}\n" if $conditionalString;
            $code .= "\n";
            $implementation{nameSpaceInternal}->add($code);
            return;
            # Skip the rest of the function!
        }
        my $imp = 0;
        if ($attrCached) {
            $imp = 1;
            $code .= <<END;
    v8::Handle<v8::String> propertyName = v8AtomicString(info.GetIsolate(), "${attrName}");
    ${implClassName}* imp = ${v8ClassName}::toNative(info.Holder());
    if (!imp->$attrCached()) {
        v8::Handle<v8::Value> jsValue = getHiddenValue(info.GetIsolate(), info.Holder(), propertyName);
        if (!jsValue.IsEmpty()) {
            v8SetReturnValue(info, jsValue);
            return;
        }
    }
END
        }
        if (!$attribute->isStatic && !$imp) {
            $code .= <<END;
    ${implClassName}* imp = ${v8ClassName}::toNative(info.Holder());
END
        }
    }

    my $raisesException = $attribute->extendedAttributes->{"RaisesException"};
    my $useExceptions = 1 if $raisesException && ($raisesException eq "VALUE_IS_MISSING" or $raisesException eq "Getter");
    if ($useExceptions || $attribute->extendedAttributes->{"CheckSecurity"}) {
        $code .= "    ExceptionState exceptionState(ExceptionState::GetterContext, \"${attrName}\", \"${interfaceName}\", info.Holder(), info.GetIsolate());\n";
    }

    # Generate security checks if necessary
    if ($attribute->extendedAttributes->{"CheckSecurity"}) {
        AddToImplIncludes("bindings/v8/BindingSecurity.h");
        $code .= "    if (!BindingSecurity::shouldAllowAccessToNode(info.GetIsolate(), imp->" . GetImplName($attribute) . "(), exceptionState)) {\n";
        $code .= "        v8SetReturnValueNull(info);\n";
        $code .= "        exceptionState.throwIfNeeded();\n";
        $code .= "        return;\n";
        $code .= "    }\n";
    }

    my $isNullable = $attribute->isNullable;
    if ($isNullable) {
        $code .= "    bool isNull = false;\n";
    }

    my $getterString;
    my ($functionName, @arguments) = GetterExpression($interfaceName, $attribute);
    push(@arguments, "isNull") if $isNullable;
    push(@arguments, "exceptionState") if $useExceptions;
    if ($attribute->extendedAttributes->{"ImplementedBy"}) {
        my $implementedBy = $attribute->extendedAttributes->{"ImplementedBy"};
        my $implementedByImplName = GetImplNameFromImplementedBy($implementedBy);
        AddToImplIncludes(HeaderFilesForInterface($implementedBy, $implementedByImplName));
        unshift(@arguments, "imp") if !$attribute->isStatic;
        $functionName = "${implementedByImplName}::${functionName}";
    } elsif ($attribute->isStatic) {
        $functionName = "${implClassName}::${functionName}";
    } else {
        $functionName = "imp->${functionName}";
    }
    my ($arg, $subCode) = GenerateCallWith($attribute->extendedAttributes->{"CallWith"}, "    ", 0);
    $code .= $subCode;
    unshift(@arguments, @$arg);
    $getterString = "${functionName}(" . join(", ", @arguments) . ")";

    my $expression;
    if ($attribute->type eq "EventHandler" && $interface->name eq "Window") {
        $code .= "    if (!imp->document())\n";
        $code .= "        return;\n";
    }

    if ($useExceptions || $isNullable) {
        if ($nativeType =~ /^V8StringResource/) {
            $code .= "    " . ConvertToV8StringResource($attribute, $nativeType, "cppValue", $getterString) . ";\n";
        } else {
            $code .= "    $nativeType jsValue = $getterString;\n";
            $getterString = "jsValue";
        }

        if ($isNullable) {
            $code .= "    if (isNull) {\n";
            $code .= "        v8SetReturnValueNull(info);\n";
            $code .= "        return;\n";
            $code .= "    }\n";
        }

        if ($useExceptions) {
            if ($useExceptions) {
                $code .= "    if (UNLIKELY(exceptionState.throwIfNeeded()))\n";
                $code .= "        return;\n";
            }

            if (ExtendedAttributeContains($attribute->extendedAttributes->{"CallWith"}, "ScriptState")) {
                $code .= "    if (state.hadException()) {\n";
                $code .= "        throwError(state.exception(), info.GetIsolate());\n";
                $code .= "        return;\n";
                $code .= "    }\n";
            }
        }

        $expression = "jsValue";
        $expression .= ".release()" if (IsRefPtrType($attrType));
    } else {
        # Can inline the function call into the return statement to avoid overhead of using a Ref<> temporary
        $expression = $getterString;
    }

    if (ShouldKeepAttributeAlive($interface, $attribute, $attrType)) {
        my $arrayType = GetArrayType($attrType);
        if ($arrayType) {
            $code .= "    v8SetReturnValue(info, v8Array(${getterString}, info.GetIsolate()));\n";
            $code .= "}\n\n";
            $implementation{nameSpaceInternal}->add($code);
            return;
        }

        # Check for a wrapper in the wrapper cache. If there is one, we know that a hidden reference has already
        # been created. If we don't find a wrapper, we create both a wrapper and a hidden reference.
        my $nativeReturnType = GetNativeType($attrType);
        my $v8ReturnType = "V8" . $attrType;
        $code .= "    $nativeReturnType result = ${getterString};\n";
        if ($forMainWorldSuffix) {
            $code .= "    if (result && DOMDataStore::setReturnValueFromWrapper${forMainWorldSuffix}<${v8ReturnType}>(info.GetReturnValue(), result.get()))\n";
        } else {
            $code .= "    if (result && DOMDataStore::setReturnValueFromWrapper<${v8ReturnType}>(info.GetReturnValue(), result.get()))\n";
        }
        $code .= "        return;\n";
        $code .= "    v8::Handle<v8::Value> wrapper = toV8(result.get(), info.Holder(), info.GetIsolate());\n";
        $code .= "    if (!wrapper.IsEmpty()) {\n";
        $code .= "        setHiddenValue(info.GetIsolate(), info.Holder(), \"${attrName}\", wrapper);\n";
        $code .= "        v8SetReturnValue(info, wrapper);\n";
        $code .= "    }\n";
        $code .= "}\n";
        $code .= "#endif // ${conditionalString}\n" if $conditionalString;
        $code .= "\n";
        $implementation{nameSpaceInternal}->add($code);
        return;
    }

    if ((IsSVGAnimatedType($interfaceName) or $interfaceName eq "SVGViewSpec") and IsSVGTypeNeedingTearOff($attrType)) {
        AddToImplIncludes("V8$attrType.h");
        my $svgNativeType = GetSVGTypeNeedingTearOff($attrType);
        # Convert from abstract SVGProperty to real type, so the right toJS() method can be invoked.
        if ($forMainWorldSuffix eq "ForMainWorld") {
            $code .= "    v8SetReturnValueForMainWorld(info, static_cast<$svgNativeType*>($expression));\n";
        } else {
            $code .= "    v8SetReturnValueFast(info, static_cast<$svgNativeType*>($expression), imp);\n";
        }
    } elsif (IsSVGTypeNeedingTearOff($attrType) and not $interfaceName =~ /List$/) {
        AddToImplIncludes("V8$attrType.h");
        AddToImplIncludes("core/svg/properties/SVGPropertyTearOff.h");
        my $tearOffType = GetSVGTypeNeedingTearOff($attrType);
        my $wrappedValue;
        if (IsSVGTypeWithWritablePropertiesNeedingTearOff($attrType) and not defined $attribute->extendedAttributes->{"Immutable"}) {
            my $getter = $expression;
            $getter =~ s/imp->//;
            $getter =~ s/\(\)//;

            my $updateMethod = "&${implClassName}::update" . FirstLetterToUpperCase($getter);

            my $selfIsTearOffType = IsSVGTypeNeedingTearOff($interfaceName);
            if ($selfIsTearOffType) {
                AddToImplIncludes("core/svg/properties/SVGMatrixTearOff.h");
                # FIXME: Don't create a new one everytime we access the matrix property. This means, e.g, === won't work.
                $wrappedValue = "WTF::getPtr(SVGMatrixTearOff::create(wrapper, $expression))";
            } else {
                AddToImplIncludes("core/svg/properties/SVGStaticPropertyTearOff.h");
                $tearOffType =~ s/SVGPropertyTearOff</SVGStaticPropertyTearOff<$implClassName, /;

                $wrappedValue = "WTF::getPtr(${tearOffType}::create(imp, $expression, $updateMethod))";
            }
        } elsif ($tearOffType =~ /SVGStaticListPropertyTearOff/) {
                $wrappedValue = "WTF::getPtr(${tearOffType}::create(imp, $expression))";
        } elsif ($tearOffType =~ /SVG(Point|PathSeg)List/) {
                $wrappedValue = "WTF::getPtr($expression)";
        } else {
                $wrappedValue = "WTF::getPtr(${tearOffType}::create($expression))";
        }
        if ($forMainWorldSuffix eq "ForMainWorld") {
            $code .= "    v8SetReturnValueForMainWorld(info, $wrappedValue);\n";
        } else {
            $code .= "    v8SetReturnValueFast(info, $wrappedValue, imp);\n";
        }
    } elsif ($attrCached) {
        if ($attribute->type eq "SerializedScriptValue") {
        $code .= <<END;
    RefPtr<SerializedScriptValue> serialized = $getterString;
    ScriptValue jsValue = serialized ? serialized->deserialize() : v8::Handle<v8::Value>(v8::Null(info.GetIsolate()));
    setHiddenValue(info.GetIsolate(), info.Holder(), propertyName, jsValue);
    v8SetReturnValue(info, jsValue);
END
        } else {
            if (!$useExceptions && !$isNullable) {
                $code .= <<END;
    ScriptValue jsValue = $getterString;
END
            }
            $code .= <<END;
    setHiddenValue(info.GetIsolate(), info.Holder(), propertyName, jsValue.v8Value());
    v8SetReturnValue(info, jsValue.v8Value());
END
        }
    } elsif ($attribute->type eq "EventHandler") {
        AddToImplIncludes("bindings/v8/V8AbstractEventListener.h");
        # FIXME: Pass the main world ID for main-world-only getters.
        my ($functionName, @arguments) = GetterExpression($interfaceName, $attribute);
        my $implementedBy = $attribute->extendedAttributes->{"ImplementedBy"};
        if ($implementedBy) {
            my $implementedByImplName = GetImplNameFromImplementedBy($implementedBy);
            $functionName = "${implementedByImplName}::${functionName}";
            push(@arguments, "imp");
        } else {
            $functionName = "imp->${functionName}";
        }
        $code .= "    EventListener* jsValue = ${functionName}(" . join(", ", @arguments) . ");\n";
        $code .= "    v8SetReturnValue(info, jsValue ? v8::Handle<v8::Value>(V8AbstractEventListener::cast(jsValue)->getListenerObject(imp->executionContext())) : v8::Handle<v8::Value>(v8::Null(info.GetIsolate())));\n";
    } else {
        my $nativeValue = NativeToJSValue($attribute->type, $attribute->extendedAttributes, $expression, "    ", "", "info.GetIsolate()", "info", "imp", $forMainWorldSuffix, "return");
        $code .= "${nativeValue}\n";
    }

    $code .= "}\n";  # end of getter
    $code .= "#endif // ${conditionalString}\n" if $conditionalString;
    $code .= "\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub ShouldKeepAttributeAlive
{
    my ($interface, $attribute, $returnType) = @_;
    my $attrName = $attribute->name;

    # For readonly attributes (including Replaceable), as a general rule, for
    # performance reasons we keep the attribute wrapper alive while the owner
    # wrapper is alive, because the attribute never changes.
    return 0 if !IsWrapperType($returnType);
    return 0 if !IsReadonly($attribute) && !$attribute->extendedAttributes->{"Replaceable"};

    # However, there are a couple of exceptions.

    # Node lifetime is managed by object grouping.
    return 0 if InheritsInterface($interface, "Node");
    return 0 if IsDOMNodeType($returnType);

    # To avoid adding a reference to itself.
    # FIXME: Introduce [DoNotKeepAttributeAliveForGC] and remove this hack
    # depending on the attribute name.
    return 0 if $attrName eq "self";

    # FIXME: Remove these hard-coded hacks.
    return 0 if $returnType eq "EventTarget";
    return 0 if $returnType eq "Window";
    return 0 if $returnType =~ /SVG/;
    return 0 if $returnType =~ /HTML/;

    return 1;
}

sub GenerateReplaceableAttributeSetterCallback
{
    my $interface = shift;
    my $implClassName = GetImplName($interface);

    my $code = "";
    $code .= "static void ${implClassName}ReplaceableAttributeSetterCallback(v8::Local<v8::String> name, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<void>& info)\n";
    $code .= "{\n";
    $code .= GenerateFeatureObservation($interface->extendedAttributes->{"MeasureAs"});
    $code .= GenerateDeprecationNotification($interface->extendedAttributes->{"DeprecateAs"});
    $code .= GenerateCustomElementInvocationScopeIfNeeded($interface->extendedAttributes);
    if (HasActivityLogging("", $interface->extendedAttributes, "Setter")) {
         die "IDL error: ActivityLog attribute cannot exist on a ReplacableAttributeSetterCallback";
    }
    $code .= "    ${implClassName}V8Internal::${implClassName}ReplaceableAttributeSetter(name, jsValue, info);\n";
    $code .= "}\n\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateReplaceableAttributeSetter
{
    my $interface = shift;

    my $interfaceName = $interface->name();
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    my $code = "";
    $code .= <<END;
static void ${implClassName}ReplaceableAttributeSetter(v8::Local<v8::String> name, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<void>& info)
{
END
    if ($interface->extendedAttributes->{"CheckSecurity"}) {
        AddToImplIncludes("bindings/v8/BindingSecurity.h");
        $code .= <<END;
    ${implClassName}* imp = ${v8ClassName}::toNative(info.Holder());
    v8::String::Utf8Value attributeName(name);
    ExceptionState exceptionState(ExceptionState::SetterContext, *attributeName, "${interfaceName}", info.Holder(), info.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), imp->frame(), exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }
END
    }

    $code .= <<END;
    info.This()->ForceSet(name, jsValue);
}

END
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateCustomElementInvocationScopeIfNeeded
{
    my $code = "";
    my $ext = shift;

    if ($ext->{"CustomElementCallbacks"} or $ext->{"Reflect"}) {
        AddToImplIncludes("core/dom/custom/CustomElementCallbackDispatcher.h");
        $code .= <<END;
    CustomElementCallbackDispatcher::CallbackDeliveryScope deliveryScope;
END
    }
    return $code;
}

sub GenerateNormalAttributeSetterCallback
{
    my $attribute = shift;
    my $interface = shift;
    my $forMainWorldSuffix = shift;
    my $exposeJSAccessors = shift;

    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $attrExt = $attribute->extendedAttributes;
    my $attrName = $attribute->name;

    my $conditionalString = GenerateConditionalString($attribute);
    my $code = "";
    $code .= "#if ${conditionalString}\n" if $conditionalString;

    if ($exposeJSAccessors) {
        $code .= "static void ${attrName}AttributeSetterCallback${forMainWorldSuffix}(const v8::FunctionCallbackInfo<v8::Value>& info)\n";
        $code .= "{\n";
        $code .= "    v8::Local<v8::Value> jsValue = info[0];\n";
    } else {
        $code .= "static void ${attrName}AttributeSetterCallback${forMainWorldSuffix}(v8::Local<v8::String>, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<void>& info)\n";
        $code .= "{\n";
    }
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"Blink\", \"DOMSetter\");\n";
    $code .= GenerateFeatureObservation($attrExt->{"MeasureAs"});
    $code .= GenerateDeprecationNotification($attrExt->{"DeprecateAs"});
    if (HasActivityLogging($forMainWorldSuffix, $attrExt, "Setter")) {
        $code .= GenerateActivityLogging("Setter", $interface, "${attrName}");
    }
    $code .= GenerateCustomElementInvocationScopeIfNeeded($attrExt);
    if (HasCustomSetter($attribute)) {
        $code .= "    ${v8ClassName}::${attrName}AttributeSetterCustom(jsValue, info);\n";
    } else {
        $code .= "    ${implClassName}V8Internal::${attrName}AttributeSetter${forMainWorldSuffix}(jsValue, info);\n";
    }
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"V8\", \"V8Execution\");\n";
    $code .= "}\n";
    $code .= "#endif // ${conditionalString}\n" if $conditionalString;
    $code .= "\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub FindAttributeWithName
{
    my $interface = shift;
    my $attrName = shift;

    foreach my $attribute (@{$interface->attributes}) {
        if ($attribute->name eq $attrName) {
            return $attribute;
        }
    }
}

sub GenerateNormalAttributeSetter
{
    my $attribute = shift;
    my $interface = shift;
    my $forMainWorldSuffix = shift;
    my $exposeJSAccessors = shift;

    my $interfaceName = $interface->name;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $attrName = $attribute->name;
    my $attrExt = $attribute->extendedAttributes;
    my $attrType = $attribute->type;
    my $attrCached = GetCachedAttribute($attribute);

    if (HasCustomSetter($attribute)) {
        return;
    }

    my $conditionalString = GenerateConditionalString($attribute);
    my $code = "";
    $code .= "#if ${conditionalString}\n" if $conditionalString;
    if ($exposeJSAccessors) {
        $code .= "static void ${attrName}AttributeSetter${forMainWorldSuffix}(v8::Local<v8::Value> jsValue, const v8::FunctionCallbackInfo<v8::Value>& info)\n";
    } else {
        $code .= "static void ${attrName}AttributeSetter${forMainWorldSuffix}(v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<void>& info)\n";
    }
    $code .= "{\n";

    my $raisesException = $attribute->extendedAttributes->{"RaisesException"};
    my $useExceptions = 1 if $raisesException && ($raisesException eq "VALUE_IS_MISSING" or $raisesException eq "Setter");
    my $hasStrictTypeChecking = 1 if ($interface->extendedAttributes->{"StrictTypeChecking"} || $attribute->extendedAttributes->{"StrictTypeChecking"}) && IsWrapperType($attrType);  # Currently only actually check interface types

    # Can throw exceptions from accessors or during type conversion.
    my $isIntegerType = IsIntegerType($attribute->type);

    # We throw exceptions using 'ExceptionState' if the attribute explicitly
    # claims that exceptions may be raised, or if a strict type check might
    # fail, or if we're dealing with SVG, which does strange things with
    # tearoffs and read-only wrappers.
    if ($useExceptions or $hasStrictTypeChecking or GetSVGTypeNeedingTearOff($interfaceName) or GetSVGTypeNeedingTearOff($attrType) or $isIntegerType) {
        $code .= "    ExceptionState exceptionState(ExceptionState::SetterContext, \"${attrName}\", \"${interfaceName}\", info.Holder(), info.GetIsolate());\n";
    }

    # If the "StrictTypeChecking" extended attribute is present, and the
    # attribute's type is an interface type, then if the incoming value does not
    # implement that interface, a TypeError is thrown rather than silently
    # passing NULL to the C++ code.
    # Per the Web IDL and ECMAScript specifications, incoming values can always
    # be converted to both strings and numbers, so do not throw TypeError if the
    # attribute is of these types.
    if ($hasStrictTypeChecking) {
        $code .= "    if (!isUndefinedOrNull(jsValue) && !V8${attrType}::hasInstance(jsValue, info.GetIsolate())) {\n";
        $code .= "        exceptionState.throwTypeError(\"The provided value is not of type '${attrType}'.\");\n";
        $code .= "        exceptionState.throwIfNeeded();\n";
        $code .= "        return;\n";
        $code .= "    }\n";
    }

    my $svgNativeType = GetSVGTypeNeedingTearOff($interfaceName);
    if ($svgNativeType) {
        my $svgWrappedNativeType = GetSVGWrappedTypeNeedingTearOff($interfaceName);
        if ($svgWrappedNativeType =~ /List$/) {
            $code .= <<END;
    $svgNativeType* imp = ${v8ClassName}::toNative(info.Holder());
END
        } else {
            $code .= "    $svgNativeType* wrapper = ${v8ClassName}::toNative(info.Holder());\n";
            $code .= "    if (wrapper->isReadOnly()) {\n";
            $code .= "        exceptionState.throwDOMException(NoModificationAllowedError, \"The attribute is read-only.\");\n";
            $code .= "        exceptionState.throwIfNeeded();\n";
            $code .= "        return;\n";
            $code .= "    }\n";
            $code .= "    $svgWrappedNativeType& impInstance = wrapper->propertyReference();\n";
            $code .= "    $svgWrappedNativeType* imp = &impInstance;\n";
        }
    } elsif ($attrExt->{"OnPrototype"}) {
        $code .= <<END;
    ${implClassName}* imp = ${v8ClassName}::toNative(info.Holder());
END
    } elsif($attrExt->{"PutForwards"}) {
        my $destinationAttrName = $attrExt->{"PutForwards"};
        my $destinationInterface = ParseInterface($attrType);
        die "[PutForwards=x] value must be a wrapper type" unless $destinationInterface;
        my $destinationAttribute = FindAttributeWithName($destinationInterface, $destinationAttrName);
        die "[PutForwards=x] could not find $destinationAttrName in interface $attrType" unless $destinationAttribute;
        $code .= <<END;
    ${implClassName}* proxyImp = ${v8ClassName}::toNative(info.Holder());
    ${attrType}* imp = proxyImp->${attrName}();
    if (!imp)
        return;
END
        # Override attribute and fall through to forward setter call.
        $attribute = $destinationAttribute;
        $attrName = $attribute->name;
        $attrType = $attribute->type;
        $attrExt = $attribute->extendedAttributes;
    } else {
        my $reflect = $attribute->extendedAttributes->{"Reflect"};
        if ($reflect && InheritsInterface($interface, "Node") && $attrType eq "DOMString") {
            # Generate super-compact call for regular attribute setter:
            my $contentAttributeName = $reflect eq "VALUE_IS_MISSING" ? lc $attrName : $reflect;
            my $namespace = NamespaceForAttributeName($interfaceName, $contentAttributeName);
            my $mode = GetV8StringResourceMode($attribute->extendedAttributes);
            AddToImplIncludes("${namespace}.h");
            $code .= "    Element* imp = V8Element::toNative(info.Holder());\n";
            $code .= "    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<$mode>, cppValue, jsValue);\n";
            # Attr (not Attribute) used in content attributes
            $code .= "    imp->setAttribute(${namespace}::${contentAttributeName}Attr, cppValue);\n";
            $code .= "}\n";
            $code .= "#endif // ${conditionalString}\n" if $conditionalString;
            $code .= "\n";
            $implementation{nameSpaceInternal}->add($code);
            return;
            # Skip the rest of the function!
        }

        if (!$attribute->isStatic) {
            $code .= <<END;
    ${implClassName}* imp = ${v8ClassName}::toNative(info.Holder());
END
        }
    }

    my $nativeType = GetNativeType($attribute->type, $attribute->extendedAttributes, "parameter");
    if ($attribute->type eq "EventHandler") {
        if ($interface->name eq "Window") {
            $code .= "    if (!imp->document())\n";
            $code .= "        return;\n";
        }
    } else {
        my $asSetterValue = 0;
        $code .= JSValueToNativeStatement($attribute->type, $attribute->extendedAttributes, $asSetterValue, "jsValue", "cppValue", "    ", "info.GetIsolate()");
    }

    if (IsEnumType($attrType)) {
        # setter ignores invalid enumeration values
        my @enumValues = ValidEnumValues($attrType);
        my @validEqualities = ();
        foreach my $enumValue (@enumValues) {
            push(@validEqualities, "string == \"$enumValue\"");
        }
        my $enumValidationExpression = join(" || ", @validEqualities);
        $code .= <<END;
    String string = cppValue;
    if (!($enumValidationExpression))
        return;
END
    }

    my $expression = "cppValue";
    my $returnType = $attribute->type;
    if (IsRefPtrType($returnType) && !GetArrayType($returnType)) {
        $expression = "WTF::getPtr(" . $expression . ")";
    }

    $code .= GenerateCustomElementInvocationScopeIfNeeded($attribute->extendedAttributes);

    my $returnSvgNativeType = GetSVGTypeNeedingTearOff($returnType);
    if ($returnSvgNativeType) {
        $code .= <<END;
    if (!$expression) {
        exceptionState.throwTypeError(\"The provided value is not of type '$returnType'.\");
        exceptionState.throwIfNeeded();
        return;
    }
END
        $expression = $expression . "->propertyReference()";
    }

    if ($attribute->type eq "EventHandler") {
        my $implementedBy = $attribute->extendedAttributes->{"ImplementedBy"};
        my $implementedByImplName;
        if ($implementedBy) {
            $implementedByImplName = GetImplNameFromImplementedBy($implementedBy);
        }
        if (!InheritsInterface($interface, "Node")) {
            my $attrImplName = GetImplName($attribute);
            my @arguments;
            if ($implementedBy) {
                $attrImplName = "${implementedByImplName}::${attrImplName}";
                push(@arguments, "imp");
            } else {
                $attrImplName = "imp->${attrImplName}";
            }
            $code .= "    moveEventListenerToNewWrapper(info.Holder(), ${attrImplName}(" . join(", ", @arguments) . "), jsValue, ${v8ClassName}::eventListenerCacheIndex, info.GetIsolate());\n";
        }
        my ($functionName, @arguments) = SetterExpression($interfaceName, $attribute);
        if ($implementedBy) {
            $functionName = "${implementedByImplName}::${functionName}";
            push(@arguments, "imp");
        } else {
            $functionName = "imp->${functionName}";
        }
        if (($interfaceName eq "Window" or $interfaceName eq "WorkerGlobalScope") and $attribute->name eq "onerror") {
            AddToImplIncludes("bindings/v8/V8ErrorHandler.h");
            push(@arguments, "V8EventListenerList::findOrCreateWrapper<V8ErrorHandler>(jsValue, true, info.GetIsolate())");
        } else {
            push(@arguments, "V8EventListenerList::getEventListener(jsValue, true, ListenerFindOrCreate)");
        }
        $code .= "    ${functionName}(" . join(", ", @arguments) . ");\n";
    } else {
        my ($functionName, @arguments) = SetterExpression($interfaceName, $attribute);
        push(@arguments, $expression);
        push(@arguments, "exceptionState") if $useExceptions;
        if ($attribute->extendedAttributes->{"ImplementedBy"}) {
            my $implementedBy = $attribute->extendedAttributes->{"ImplementedBy"};
            my $implementedByImplName = GetImplNameFromImplementedBy($implementedBy);
            AddToImplIncludes(HeaderFilesForInterface($implementedBy, $implementedByImplName));
            unshift(@arguments, "imp") if !$attribute->isStatic;
            $functionName = "${implementedByImplName}::${functionName}";
        } elsif ($attribute->isStatic) {
            $functionName = "${implClassName}::${functionName}";
        } else {
            $functionName = "imp->${functionName}";
        }
        my ($arg, $subCode) = GenerateCallWith($attribute->extendedAttributes->{"SetterCallWith"} || $attribute->extendedAttributes->{"CallWith"}, "    ", 1);
        $code .= $subCode;
        unshift(@arguments, @$arg);
        $code .= "    ${functionName}(" . join(", ", @arguments) . ");\n";
    }

    if ($useExceptions) {
        $code .= "    exceptionState.throwIfNeeded();\n";
    }

    if (ExtendedAttributeContains($attribute->extendedAttributes->{"CallWith"}, "ScriptState")) {
        $code .= "    if (state.hadException())\n";
        $code .= "        throwError(state.exception(), info.GetIsolate());\n";
    }

    if ($svgNativeType) {
        if ($useExceptions) {
            $code .= "    if (!exceptionState.hadException())\n";
            $code .= "        wrapper->commitChange();\n";
        } else {
            $code .= "    wrapper->commitChange();\n";
        }
    }

    if ($attrCached) {
        $code .= <<END;
    deleteHiddenValue(info.GetIsolate(), info.Holder(), "${attrName}"); // Invalidate the cached value.
END
    }

    $code .= "}\n";  # end of setter
    $code .= "#endif // ${conditionalString}\n" if $conditionalString;
    $code .= "\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub NullOrOptionalCheck
{
    my $parameter = shift;
    my $value = shift;

    # If undefined is passed for an optional argument, the argument should be
    # treated as missing; otherwise undefined is not allowed.
    if ($parameter->isNullable) {
        if ($parameter->isOptional) {
            return "isUndefinedOrNull($value)";
        }
        return "${value}->IsNull()";
    }
    if ($parameter->isOptional) {
        return "${value}->IsUndefined()";
    }
}

sub GenerateParametersCheckExpression
{
    my $numParameters = shift;
    my $function = shift;

    my @andExpression = ();
    push(@andExpression, "info.Length() == $numParameters");
    my $parameterIndex = 0;
    foreach my $parameter (@{$function->parameters}) {
        last if $parameterIndex >= $numParameters;
        my $value = "info[$parameterIndex]";
        my $type = $parameter->type;

        my $undefinedOrNullCheck = NullOrOptionalCheck($parameter, $value);

        # Only DOMString, wrapper types, and (to some degree) non-wrapper types
        # are checked.
        #
        # FIXME: If distinguishing non-primitive type from primitive type,
        # (e.g., sequence<DOMString> from DOMString or Dictionary from double)
        # the non-primitive type must appear *first* in the IDL file,
        # since we're not adding a check to primitive types.
        # This can be avoided if compute overloads for whole overload set at
        # once, rather than one method at a time, but that requires a complete
        # rewrite of this algorithm.
        #
        # For DOMString with StrictTypeChecking only Null, Undefined and Object
        # are accepted for compatibility. Otherwise, no restrictions are made to
        # match the non-overloaded behavior.
        #
        # FIXME: Implement WebIDL overload resolution algorithm.
        # https://code.google.com/p/chromium/issues/detail?id=293561
        if ($type eq "DOMString") {
            if ($parameter->extendedAttributes->{"StrictTypeChecking"}) {
                push(@andExpression, "isUndefinedOrNull(${value}) || ${value}->IsString() || ${value}->IsObject()");
            }
        } elsif (IsCallbackInterface($parameter->type)) {
            # For Callbacks only checks if the value is null or function.
            push(@andExpression, "${value}->IsNull() || ${value}->IsFunction()");
        } elsif (GetArrayOrSequenceType($type)) {
            if ($parameter->isNullable) {
                push(@andExpression, "${value}->IsNull() || ${value}->IsArray()");
            } else {
                push(@andExpression, "${value}->IsArray()");
            }
        } elsif (IsWrapperType($type)) {
            if ($parameter->isNullable) {
                push(@andExpression, "${value}->IsNull() || V8${type}::hasInstance($value, info.GetIsolate())");
            } else {
                push(@andExpression, "V8${type}::hasInstance($value, info.GetIsolate())");
            }
        } elsif ($nonWrapperTypes{$type}) {
            # Non-wrapper types are just objects: we don't distinguish type
            if ($undefinedOrNullCheck) {
                push(@andExpression, "$undefinedOrNullCheck || ${value}->IsObject()");
            } else {
                push(@andExpression, "${value}->IsObject()");
            }
        }

        $parameterIndex++;
    }
    @andExpression = map { "($_)" } @andExpression;
    my $res = "(" . join(" && ", @andExpression) . ")";
    return $res;
}

# As per Web IDL specification, the length of a function Object is
# its number of mandatory parameters.
sub GetFunctionLength
{
    my $function = shift;

    my $numMandatoryParams = 0;
    foreach my $parameter (@{$function->parameters}) {
        # Abort as soon as we find the first optional parameter as no mandatory
        # parameter can follow an optional one.
        last if $parameter->isOptional;
        $numMandatoryParams++;
    }
    return $numMandatoryParams;
}

sub GenerateFunctionParametersCheck
{
    my $function = shift;

    my @orExpression = ();
    my $numParameters = 0;
    my $hasVariadic = 0;
    my $numMandatoryParams = @{$function->parameters};
    foreach my $parameter (@{$function->parameters}) {
        if ($parameter->isOptional) {
            push(@orExpression, GenerateParametersCheckExpression($numParameters, $function));
            $numMandatoryParams--;
        }
        if ($parameter->isVariadic) {
            $hasVariadic = 1;
            last;
        }
        $numParameters++;
    }
    if (!$hasVariadic) {
        push(@orExpression, GenerateParametersCheckExpression($numParameters, $function));
    }
    return ($numMandatoryParams, join(" || ", @orExpression));
}

sub GenerateOverloadedFunction
{
    my $function = shift;
    my $interface = shift;
    my $forMainWorldSuffix = shift;

    # Generate code for choosing the correct overload to call. Overloads are
    # chosen based on the total number of arguments passed and the type of
    # values passed in non-primitive argument slots. When more than a single
    # overload is applicable, precedence is given according to the order of
    # declaration in the IDL.

    my $name = $function->name;
    my $interfaceName = $interface->name;
    my $implClassName = GetImplName($interface);

    my $conditionalString = GenerateConditionalString($function);
    my $leastNumMandatoryParams = 255;

    my $code = "";
    $code .= "#if ${conditionalString}\n\n" if $conditionalString;
    $code .= <<END;
static void ${name}Method${forMainWorldSuffix}(const v8::FunctionCallbackInfo<v8::Value>& info)
{
END
    foreach my $overload (@{$function->{overloads}}) {
        my ($numMandatoryParams, $parametersCheck) = GenerateFunctionParametersCheck($overload);
        $leastNumMandatoryParams = $numMandatoryParams if ($numMandatoryParams < $leastNumMandatoryParams);
        $code .= "    if ($parametersCheck) {\n";
        my $overloadedIndexString = $overload->{overloadIndex};
        $code .= "        ${name}${overloadedIndexString}Method${forMainWorldSuffix}(info);\n";
        $code .= "        return;\n";
        $code .= "    }\n";
    }
    if ($leastNumMandatoryParams >= 1) {
        $code .= "    ExceptionState exceptionState(ExceptionState::ExecutionContext, \"${name}\", \"${interfaceName}\", info.Holder(), info.GetIsolate());\n";
        $code .= "    if (UNLIKELY(info.Length() < $leastNumMandatoryParams)) {\n";
        $code .= "        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments($leastNumMandatoryParams, info.Length()));\n";
        $code .= "        exceptionState.throwIfNeeded();\n";
        $code .= "        return;\n";
        $code .= "    }\n";
        $code .= <<END;
    exceptionState.throwTypeError(\"No function was found that matched the signature provided.\");
    exceptionState.throwIfNeeded();
END
    } else {
        $code .=<<END;
    throwTypeError(ExceptionMessages::failedToExecute(\"${name}\", \"${interfaceName}\", \"No function was found that matched the signature provided.\"), info.GetIsolate());
END
    }
    $code .= "}\n\n";
    $code .= "#endif // ${conditionalString}\n\n" if $conditionalString;
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateFunctionCallback
{
    my $function = shift;
    my $interface = shift;
    my $forMainWorldSuffix = shift;

    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $name = $function->name;

    if ($name eq "") {
        return;
    }

    my $conditionalString = GenerateConditionalString($function);
    my $code = "";
    $code .= "#if ${conditionalString}\n" if $conditionalString;
    $code .= <<END;
static void ${name}MethodCallback${forMainWorldSuffix}(const v8::FunctionCallbackInfo<v8::Value>& info)
{
END
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"Blink\", \"DOMMethod\");\n";
    $code .= GenerateFeatureObservation($function->extendedAttributes->{"MeasureAs"});
    $code .= GenerateDeprecationNotification($function->extendedAttributes->{"DeprecateAs"});
    if (HasActivityLogging($forMainWorldSuffix, $function->extendedAttributes, "Access")) {
        $code .= GenerateActivityLogging("Method", $interface, "${name}");
    }
    if (HasCustomMethod($function->extendedAttributes)) {
        $code .= "    ${v8ClassName}::${name}MethodCustom(info);\n";
    } else {
        $code .= "    ${implClassName}V8Internal::${name}Method${forMainWorldSuffix}(info);\n";
    }
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"V8\", \"V8Execution\");\n";
    $code .= "}\n";
    $code .= "#endif // ${conditionalString}\n" if $conditionalString;
    $code .= "\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateFunction
{
    my $function = shift;
    my $interface = shift;
    my $forMainWorldSuffix = shift;

    my $interfaceName = $interface->name;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $name = $function->name;
    my $unoverloadedName = $function->name;
    my $implName = GetImplName($function);
    my $funcExt = $function->extendedAttributes;

    # Add includes for types even if custom
    my $returnType = $function->type;
    AddIncludesForType($returnType);
    foreach my $parameter (@{$function->parameters}) {
        my $paramType = $parameter->type;
        AddIncludesForType($paramType);
    }

    if (HasCustomMethod($funcExt) || $name eq "") {
        return;
    }

    if (@{$function->{overloads}} > 1) {
        # Append a number to an overloaded method's name to make it unique:
        $name = $name . $function->{overloadIndex};
    }

    my $conditionalString = GenerateConditionalString($function);
    my $code = "";
    $code .= "#if ${conditionalString}\n" if $conditionalString;
    $code .= "static void ${name}Method${forMainWorldSuffix}(const v8::FunctionCallbackInfo<v8::Value>& info)\n";
    $code .= "{\n";

    # We throw exceptions using 'ExceptionState' for a function if:
    #   - it explicitly claims that exceptions may be raised (or should be if type checks fail.)
    #   - event listeners.
    #   - security-checking.
    #   - weird SVG stuff.
    #   - takes a parameter that might raise an exception on conversion.
    #
    my $isEventListener = $name eq "addEventListener" || $name eq "removeEventListener";
    my $isEventDispatcher = $name eq "dispatchEvent";
    my $isSecurityCheckNecessary = $interface->extendedAttributes->{"CheckSecurity"} && !$function->extendedAttributes->{"DoNotCheckSecurity"};
    my $raisesExceptions = $function->extendedAttributes->{"RaisesException"};
    my ($svgPropertyType, $svgListPropertyType, $svgNativeType) = GetSVGPropertyTypes($interfaceName);
    my $isNonListSVGType = $svgNativeType && !($interfaceName =~ /List$/);

    my $hasExceptionState = $raisesExceptions || $isEventListener || $isSecurityCheckNecessary || $isNonListSVGType || HasExceptionRaisingParameter($function);
    if ($hasExceptionState) {
        $code .= "    ExceptionState exceptionState(ExceptionState::ExecutionContext, \"${unoverloadedName}\", \"${interfaceName}\", info.Holder(), info.GetIsolate());\n";
    }

    if ($isEventListener || $isEventDispatcher) {
        AddToImplIncludes("bindings/v8/BindingSecurity.h");
        AddToImplIncludes("bindings/v8/V8EventListenerList.h");
        AddToImplIncludes("core/frame/DOMWindow.h");
        $code .= <<END;
    EventTarget* impl = ${v8ClassName}::toNative(info.Holder());
    if (DOMWindow* window = impl->toDOMWindow()) {
        if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), window->frame(), exceptionState)) {
            exceptionState.throwIfNeeded();
            return;
        }
        if (!window->document())
            return;
    }
END
    }
    if ($isEventListener) {
        my $lookupType = ($name eq "addEventListener") ? "OrCreate" : "Only";
        my $passRefPtrHandling = ($name eq "addEventListener") ? "" : ".get()";
        my $hiddenValueAction = ($name eq "addEventListener") ? "addHiddenValueToArray" : "removeHiddenValueFromArray";

        $code .= <<END;
    RefPtr<EventListener> listener = V8EventListenerList::getEventListener(info[1], false, ListenerFind${lookupType});
    if (listener) {
        V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<WithNullCheck>, eventName, info[0]);
        impl->${implName}(eventName, listener${passRefPtrHandling}, info[2]->BooleanValue());
        if (!impl->toNode())
            ${hiddenValueAction}(info.Holder(), info[1], ${v8ClassName}::eventListenerCacheIndex, info.GetIsolate());
    }
}
END
        $code .= "#endif // ${conditionalString}\n" if $conditionalString;
        $code .= "\n";
        $implementation{nameSpaceInternal}->add($code);
        return;
    }

    $code .= GenerateArgumentsCountCheck($function, $interface, $hasExceptionState);

    if ($svgNativeType) {
        my $nativeClassName = GetNativeType($interfaceName);
        if ($interfaceName =~ /List$/) {
            $code .= "    $nativeClassName imp = ${v8ClassName}::toNative(info.Holder());\n";
        } else {
            AddToImplIncludes("core/dom/ExceptionCode.h");
            $code .= "    $nativeClassName wrapper = ${v8ClassName}::toNative(info.Holder());\n";
            $code .= "    if (wrapper->isReadOnly()) {\n";
            $code .= "        exceptionState.throwDOMException(NoModificationAllowedError, \"The object is read-only.\");\n";
            $code .= "        exceptionState.throwIfNeeded();\n";
            $code .= "        return;\n";
            $code .= "    }\n";
            my $svgWrappedNativeType = GetSVGWrappedTypeNeedingTearOff($interfaceName);
            $code .= "    $svgWrappedNativeType& impInstance = wrapper->propertyReference();\n";
            $code .= "    $svgWrappedNativeType* imp = &impInstance;\n";
        }
    } elsif (!$function->isStatic) {
        $code .= <<END;
    ${implClassName}* imp = ${v8ClassName}::toNative(info.Holder());
END
    }

    $code .= GenerateCustomElementInvocationScopeIfNeeded($funcExt);

    # Check domain security if needed
    if ($isSecurityCheckNecessary) {
        # We have not find real use cases yet.
        AddToImplIncludes("bindings/v8/BindingSecurity.h");
        $code .= <<END;
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), imp->frame(), exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }
END
    }

    if ($function->extendedAttributes->{"CheckSecurity"}) {
        AddToImplIncludes("bindings/v8/BindingSecurity.h");
        $code .= "    if (!BindingSecurity::shouldAllowAccessToNode(info.GetIsolate(), imp->" . GetImplName($function) . "(exceptionState), exceptionState)) {\n";
        $code .= "        v8SetReturnValueNull(info);\n";
        $code .= "        exceptionState.throwIfNeeded();\n";
        $code .= "        return;\n";
        $code .= "    }\n";
END
    }

    my ($parameterCheckString, $paramIndex, %replacements) = GenerateParametersCheck($function, $interface, $forMainWorldSuffix, $hasExceptionState);
    $code .= $parameterCheckString;

    # Build the function call string.
    $code .= GenerateFunctionCallString($function, $paramIndex, "    ", $interface, $forMainWorldSuffix, $hasExceptionState, %replacements);
    $code .= "}\n";
    $code .= "#endif // ${conditionalString}\n" if $conditionalString;
    $code .= "\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateCallWith
{
    my $callWith = shift;
    return ([], "") unless $callWith;
    my $indent = shift;
    my $returnVoid = shift;
    my $function = shift;
    my $code = "";

    my @callWithArgs;
    if (ExtendedAttributeContains($callWith, "ScriptState")) {
        $code .= $indent . "ScriptState* currentState = ScriptState::current();\n";
        $code .= $indent . "if (!currentState)\n";
        $code .= $indent . "    return" . ($returnVoid ? "" : " v8Undefined()") . ";\n";
        $code .= $indent . "ScriptState& state = *currentState;\n";
        push(@callWithArgs, "&state");
        AddToImplIncludes("bindings/v8/ScriptState.h");
    }
    if (ExtendedAttributeContains($callWith, "ExecutionContext")) {
        $code .= $indent . "ExecutionContext* scriptContext = currentExecutionContext(info.GetIsolate());\n";
        push(@callWithArgs, "scriptContext");
    }
    if ($function and ExtendedAttributeContains($callWith, "ScriptArguments")) {
        $code .= $indent . "RefPtr<ScriptArguments> scriptArguments(createScriptArguments(info, " . @{$function->parameters} . "));\n";
        push(@callWithArgs, "scriptArguments.release()");
        AddToImplIncludes("bindings/v8/ScriptCallStackFactory.h");
        AddToImplIncludes("core/inspector/ScriptArguments.h");
    }
    if (ExtendedAttributeContains($callWith, "ActiveWindow")) {
        push(@callWithArgs, "callingDOMWindow(info.GetIsolate())");
    }
    if (ExtendedAttributeContains($callWith, "FirstWindow")) {
        push(@callWithArgs, "enteredDOMWindow(info.GetIsolate())");
    }
    return ([@callWithArgs], $code);
}

sub GenerateArgumentsCountCheck
{
    my $function = shift;
    my $interface = shift;
    my $hasExceptionState = shift;

    my $functionName = $function->name;
    my $interfaceName = $interface->name;
    my $implClassName = GetImplName($interface);

    my $isConstructor = $functionName eq "Constructor" || $functionName eq "NamedConstructor";
    my $numMandatoryParams = 0;
    my $allowNonOptional = 1;
    foreach my $param (@{$function->parameters}) {
        if ($param->isOptional or $param->isVariadic) {
            $allowNonOptional = 0;
        } else {
            die "An argument must not be declared to be optional unless all subsequent arguments to the operation are also optional." if !$allowNonOptional;
            $numMandatoryParams++;
        }
    }
    return "" unless $numMandatoryParams;

    my $argumentsCountCheckString = "";
    $argumentsCountCheckString .= "    if (UNLIKELY(info.Length() < $numMandatoryParams)) {\n";
    if ($hasExceptionState) {
        $argumentsCountCheckString .= "        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments($numMandatoryParams, info.Length()));\n";
        $argumentsCountCheckString .= "        exceptionState.throwIfNeeded();\n";
    } elsif ($isConstructor) {
        $argumentsCountCheckString .= "        throwTypeError(ExceptionMessages::failedToConstruct(\"$interfaceName\", ExceptionMessages::notEnoughArguments($numMandatoryParams, info.Length())), info.GetIsolate());\n";
    } else {
        $argumentsCountCheckString .= "        throwTypeError(ExceptionMessages::failedToExecute(\"$functionName\", \"$interfaceName\", ExceptionMessages::notEnoughArguments($numMandatoryParams, info.Length())), info.GetIsolate());\n";
    }
    $argumentsCountCheckString .= "        return;\n";
    $argumentsCountCheckString .= "    }\n";
    return $argumentsCountCheckString;
}

sub GenerateParametersCheck
{
    my $function = shift;
    my $interface = shift;
    my $forMainWorldSuffix = shift;
    my $hasExceptionState = shift;
    my $style = shift || "new";

    my $functionName = $function->name;
    my $interfaceName = $interface->name;
    my $implClassName = GetImplName($interface);

    my $parameterCheckString = "";
    my $paramIndex = 0;
    my %replacements = ();

    foreach my $parameter (@{$function->parameters}) {
        my $humanFriendlyIndex = $paramIndex + 1;
        my $nativeType = GetNativeType($parameter->type, $parameter->extendedAttributes, "parameter");

        # Optional arguments without [Default=...] should generate an early call with fewer arguments.
        # Optional Dictionary arguments always considered to have default of empty dictionary.
        if ($parameter->isOptional && !$parameter->extendedAttributes->{"Default"} && $nativeType ne "Dictionary" && !IsCallbackInterface($parameter->type)) {
            $parameterCheckString .= <<END;
    if (UNLIKELY(info.Length() <= $paramIndex)) {
END
            $parameterCheckString .= GenerateFunctionCallString($function, $paramIndex, "    " x 2, $interface, $forMainWorldSuffix, $hasExceptionState, %replacements);
            $parameterCheckString .= <<END;
        return;
    }
END
        }

        my $parameterName = $parameter->name;
        if (IsCallbackInterface($parameter->type)) {
            my $v8ClassName = "V8" . $parameter->type;
            AddToImplIncludes("$v8ClassName.h");
            if ($parameter->isOptional) {
                $parameterCheckString .= "    OwnPtr<" . $parameter->type . "> $parameterName;\n";
                $parameterCheckString .= "    if (info.Length() > $paramIndex && !isUndefinedOrNull(info[$paramIndex])) {\n";
                $parameterCheckString .= "        if (!info[$paramIndex]->IsFunction()) {\n";
                if ($hasExceptionState) {
                    $parameterCheckString .= "            exceptionState.throwTypeError(\"The callback provided as parameter $humanFriendlyIndex is not a function.\");\n";
                    $parameterCheckString .= "            exceptionState.throwIfNeeded();\n";
                } else {
                    $parameterCheckString .= "            throwTypeError(ExceptionMessages::failedToExecute(\"$functionName\", \"$interfaceName\", \"The callback provided as parameter $humanFriendlyIndex is not a function.\"), info.GetIsolate());\n";
                }
                $parameterCheckString .= "            return;\n";
                $parameterCheckString .= "        }\n";
                $parameterCheckString .= "        $parameterName = ${v8ClassName}::create(v8::Handle<v8::Function>::Cast(info[$paramIndex]), currentExecutionContext(info.GetIsolate()));\n";
                $parameterCheckString .= "    }\n";
            } else {
                $parameterCheckString .= "    if (info.Length() <= $paramIndex || ";
                if ($parameter->isNullable) {
                    $parameterCheckString .= "!(info[$paramIndex]->IsFunction() || info[$paramIndex]->IsNull())";
                } else {
                    $parameterCheckString .= "!info[$paramIndex]->IsFunction()";
                }
                $parameterCheckString .= ") {\n";
                if ($hasExceptionState) {
                    $parameterCheckString .= "        exceptionState.throwTypeError(\"The callback provided as parameter $humanFriendlyIndex is not a function.\");\n";
                    $parameterCheckString .= "        exceptionState.throwIfNeeded();\n";
                } else {
                    $parameterCheckString .= "        throwTypeError(ExceptionMessages::failedToExecute(\"$functionName\", \"$interfaceName\", \"The callback provided as parameter $humanFriendlyIndex is not a function.\"), info.GetIsolate());\n";
                }
                $parameterCheckString .= "        return;\n";
                $parameterCheckString .= "    }\n";
                $parameterCheckString .= "    OwnPtr<" . $parameter->type . "> $parameterName = ";
                $parameterCheckString .= "info[$paramIndex]->IsNull() ? nullptr : " if $parameter->isNullable;
                $parameterCheckString .= "${v8ClassName}::create(v8::Handle<v8::Function>::Cast(info[$paramIndex]), currentExecutionContext(info.GetIsolate()));\n";
            }
        } elsif ($parameter->extendedAttributes->{"Clamp"}) {
                my $nativeValue = "${parameterName}NativeValue";
                my $idlType = $parameter->type;
                $parameterCheckString .= "    $nativeType $parameterName = 0;\n";
                $parameterCheckString .= "    V8TRYCATCH_VOID(double, $nativeValue, info[$paramIndex]->NumberValue());\n";
                $parameterCheckString .= "    if (!std::isnan($nativeValue))\n";
                $parameterCheckString .= "        $parameterName = clampTo<$idlType>($nativeValue);\n";
        } elsif ($parameter->type eq "SerializedScriptValue") {
            AddToImplIncludes("bindings/v8/SerializedScriptValue.h");
            $parameterCheckString .= "    $nativeType $parameterName = SerializedScriptValue::create(info[$paramIndex], 0, 0, exceptionState, info.GetIsolate());\n";
            $parameterCheckString .= "    if (exceptionState.throwIfNeeded())\n";
            $parameterCheckString .= "        return;\n";
        } elsif ($parameter->isVariadic) {
            my $nativeElementType = GetNativeType($parameter->type);
            if ($nativeElementType =~ />$/) {
                $nativeElementType .= " ";
            }

            my $argType = $parameter->type;
            if (IsWrapperType($argType)) {
                $parameterCheckString .= "    Vector<$nativeElementType> $parameterName;\n";
                $parameterCheckString .= "    for (int i = $paramIndex; i < info.Length(); ++i) {\n";
                $parameterCheckString .= "        if (!V8${argType}::hasInstance(info[i], info.GetIsolate())) {\n";
                if ($hasExceptionState) {
                    $parameterCheckString .= "            exceptionState.throwTypeError(\"parameter $humanFriendlyIndex is not of type \'$argType\'.\");\n";
                    $parameterCheckString .= "            exceptionState.throwIfNeeded();\n";
                } else {
                    $parameterCheckString .= "            throwTypeError(ExceptionMessages::failedToExecute(\"$functionName\", \"$interfaceName\", \"parameter $humanFriendlyIndex is not of type \'$argType\'.\"), info.GetIsolate());\n";
                }
                $parameterCheckString .= "            return;\n";
                $parameterCheckString .= "        }\n";
                $parameterCheckString .= "        $parameterName.append(V8${argType}::toNative(v8::Handle<v8::Object>::Cast(info[i])));\n";
                $parameterCheckString .= "    }\n";
            } else {
                $parameterCheckString .= "    V8TRYCATCH_VOID(Vector<$nativeElementType>, $parameterName, toNativeArguments<$nativeElementType>(info, $paramIndex));\n";
            }
        } elsif ($nativeType =~ /^V8StringResource/) {
            my $default = defined $parameter->extendedAttributes->{"Default"} ? $parameter->extendedAttributes->{"Default"} : "";
            my $jsValue = $parameter->isOptional && $default eq "NullString" ? "argumentOrNull(info, $paramIndex)" : "info[$paramIndex]";
            my $stringResourceParameterName = $parameterName;
            my $isNullable = IsNullableParameter($parameter);
            if ($isNullable) {
                $parameterCheckString .= "    bool ${parameterName}IsNull = $jsValue->IsNull();\n";
                $stringResourceParameterName .= "StringResource";
            }
            $parameterCheckString .= JSValueToNativeStatement($parameter->type, $parameter->extendedAttributes, $humanFriendlyIndex, $jsValue, $stringResourceParameterName, "    ", "info.GetIsolate()");
            $parameterCheckString .= "    String $parameterName = $stringResourceParameterName;\n" if $isNullable;
            if (IsEnumType($parameter->type)) {
                my @enumValues = ValidEnumValues($parameter->type);
                my @validEqualities = ();
                foreach my $enumValue (@enumValues) {
                    push(@validEqualities, "string == \"$enumValue\"");
                }
                my $enumValidationExpression = join(" || ", @validEqualities);
                $parameterCheckString .=  "    String string = $parameterName;\n";
                $parameterCheckString .= "    if (!($enumValidationExpression)) {\n";
                if ($hasExceptionState) {
                    $parameterCheckString .= "        exceptionState.throwTypeError(\"parameter $humanFriendlyIndex (\'\" + string + \"\') is not a valid enum value.\");\n";
                    $parameterCheckString .= "        exceptionState.throwIfNeeded();\n";
                } else {
                    $parameterCheckString .= "        throwTypeError(ExceptionMessages::failedToExecute(\"$functionName\", \"$interfaceName\", \"parameter $humanFriendlyIndex (\'\" + string + \"\') is not a valid enum value.\"), info.GetIsolate());\n";
                }
                $parameterCheckString .= "        return;\n";
                $parameterCheckString .= "    }\n";
            }
        } else {
            # If the [StrictTypeChecking] extended attribute is present, type
            # check interface type arguments for correct type and nullability.
            #
            # If the argument is passed, and is not |undefined| or |null|, then
            # it must implement the interface type, otherwise throw a TypeError
            # If the parameter is nullable, then both |undefined| and |null|
            # pass a NULL pointer to the C++ code, otherwise these also throw.
            # Without [StrictTypeChecking], in all these cases NULL is silently
            # passed to the C++ code.
            #
            # Per the Web IDL and ECMAScript specifications, incoming values
            # can always be converted to primitive types and strings (including
            # |undefined| and |null|), so do not throw TypeError for these.
            if ($function->extendedAttributes->{"StrictTypeChecking"} || $interface->extendedAttributes->{"StrictTypeChecking"}) {
                my $argValue = "info[$paramIndex]";
                my $argType = $parameter->type;
                if (IsWrapperType($argType)) {
                    my $undefinedNullCheck = $parameter->isNullable ? " !isUndefinedOrNull($argValue) &&" : "";
                    $parameterCheckString .= "    if (info.Length() > $paramIndex &&$undefinedNullCheck !V8${argType}::hasInstance($argValue, info.GetIsolate())) {\n";
                    if ($hasExceptionState) {
                        $parameterCheckString .= "        exceptionState.throwTypeError(\"parameter $humanFriendlyIndex is not of type \'$argType\'.\");\n";
                        $parameterCheckString .= "        exceptionState.throwIfNeeded();\n";
                    }else {
                        $parameterCheckString .= "        throwTypeError(ExceptionMessages::failedToExecute(\"$functionName\", \"$interfaceName\", \"parameter $humanFriendlyIndex is not of type \'$argType\'.\"), info.GetIsolate());\n";
                    }
                    $parameterCheckString .= "        return;\n";
                    $parameterCheckString .= "    }\n";
                }
            }
            my $default = defined $parameter->extendedAttributes->{"Default"} ? $parameter->extendedAttributes->{"Default"} : "";
            my $jsValue = $parameter->isOptional && $default eq "NullString" ? "argumentOrNull(info, $paramIndex)" : "info[$paramIndex]";
            my $isNullable = IsNullableParameter($parameter);
            $parameterCheckString .= "    bool ${parameterName}IsNull = $jsValue->IsNull();\n" if $isNullable;
            $parameterCheckString .= JSValueToNativeStatement($parameter->type, $parameter->extendedAttributes, $humanFriendlyIndex, $jsValue, $parameterName, "    ", "info.GetIsolate()");
            if ($nativeType eq 'Dictionary' or $nativeType eq 'ScriptPromise') {
                $parameterCheckString .= "    if (!$parameterName.isUndefinedOrNull() && !$parameterName.isObject()) {\n";
                if ($hasExceptionState) {
                    $parameterCheckString .= "        exceptionState.throwTypeError(\"parameter ${humanFriendlyIndex} ('${parameterName}') is not an object.\");\n";
                    $parameterCheckString .= "        exceptionState.throwIfNeeded();\n";
                } elsif ($functionName eq "Constructor") {
                    $parameterCheckString .= "        throwTypeError(ExceptionMessages::failedToConstruct(\"$interfaceName\", \"parameter ${humanFriendlyIndex} ('${parameterName}') is not an object.\"), info.GetIsolate());\n";
                } else {
                    $parameterCheckString .= "        throwTypeError(ExceptionMessages::failedToExecute(\"$functionName\", \"$interfaceName\", \"parameter ${humanFriendlyIndex} ('${parameterName}') is not an object.\"), info.GetIsolate());\n";
                }
                $parameterCheckString .= "        return;\n";
                $parameterCheckString .= "    }\n";
            }
        }

        $paramIndex++;
    }
    return ($parameterCheckString, $paramIndex, %replacements);
}

sub GenerateOverloadedConstructorCallback
{
    my $interface = shift;
    my $interfaceName = $interface->name;
    my $implClassName = GetImplName($interface);

    my $code .= <<END;
static void constructor(const v8::FunctionCallbackInfo<v8::Value>& info)
{
END
    my $leastNumMandatoryParams = 255;
    foreach my $constructor (@{$interface->constructors}) {
        my $name = "constructor" . $constructor->overloadedIndex;
        my ($numMandatoryParams, $parametersCheck) = GenerateFunctionParametersCheck($constructor);
        $leastNumMandatoryParams = $numMandatoryParams if ($numMandatoryParams < $leastNumMandatoryParams);
        $code .= "    if ($parametersCheck) {\n";
        $code .= "        ${implClassName}V8Internal::${name}(info);\n";
        $code .= "        return;\n";
        $code .= "    }\n";
    }
    if ($leastNumMandatoryParams >= 1) {
        $code .= <<END;
    ExceptionState exceptionState(ExceptionState::ConstructionContext, \"${interfaceName}\", info.Holder(), info.GetIsolate());
    if (UNLIKELY(info.Length() < $leastNumMandatoryParams)) {
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments($leastNumMandatoryParams, info.Length()));
        exceptionState.throwIfNeeded();
        return;
    }
    exceptionState.throwTypeError(\"No matching constructor signature.\");
    exceptionState.throwIfNeeded();
END
    } else {
        $code .= <<END;
    throwTypeError(ExceptionMessages::failedToConstruct(\"${interfaceName}\", \"No matching constructor signature.\"), info.GetIsolate());
END
    }
    $code .= "}\n\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateSingleConstructorCallback
{
    my $interface = shift;
    my $function = shift;

    my $interfaceName = $interface->name;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $overloadedIndexString = "";
    if ($function->overloadedIndex > 0) {
        $overloadedIndexString .= $function->overloadedIndex;
    }

    my $constructorRaisesException = $interface->extendedAttributes->{"RaisesException"} && $interface->extendedAttributes->{"RaisesException"} eq "Constructor";
    my $hasExceptionState = $function->extendedAttributes->{"RaisesException"} || $constructorRaisesException || HasExceptionRaisingParameter($function);

    my @beforeArgumentList;
    my @afterArgumentList;
    my $code = "";
    $code .= <<END;
static void constructor${overloadedIndexString}(const v8::FunctionCallbackInfo<v8::Value>& info)
{
END

    if ($hasExceptionState) {
        $code .= "    ExceptionState exceptionState(ExceptionState::ConstructionContext, \"${interfaceName}\", info.Holder(), info.GetIsolate());\n";
    }
    if ($function->overloadedIndex == 0) {
        $code .= GenerateArgumentsCountCheck($function, $interface, $hasExceptionState);
    }

    # FIXME: Currently [Constructor(...)] does not yet support optional arguments without [Default=...]
    my ($parameterCheckString, $paramIndex, %replacements) = GenerateParametersCheck($function, $interface, "", $hasExceptionState);
    $code .= $parameterCheckString;

    if ($interface->extendedAttributes->{"ConstructorCallWith"}) {
        if (ExtendedAttributeContains($interface->extendedAttributes->{"ConstructorCallWith"}, "ExecutionContext")) {
            push(@beforeArgumentList, "context");
            $code .= "    ExecutionContext* context = currentExecutionContext(info.GetIsolate());\n";
        }
        if (ExtendedAttributeContains($interface->extendedAttributes->{"ConstructorCallWith"}, "Document")) {
            push(@beforeArgumentList, "document");
            $code .= "    Document& document = *toDocument(currentExecutionContext(info.GetIsolate()));\n";
        }
    }

    if ($constructorRaisesException) {
        push(@afterArgumentList, "exceptionState");
    }

    my @argumentList;
    my $index = 0;
    foreach my $parameter (@{$function->parameters}) {
        last if $index eq $paramIndex;
        if ($replacements{$parameter->name}) {
            push(@argumentList, $replacements{$parameter->name});
        } else {
            push(@argumentList, $parameter->name);
        }
        $index++;
    }

    my $argumentString = join(", ", @beforeArgumentList, @argumentList, @afterArgumentList);
    my $refPtrType = IsWillBeGarbageCollectedType($interfaceName) ? "RefPtrWillBeRawPtr<$implClassName>" : "RefPtr<$implClassName>";
    $code .= "    $refPtrType impl = ${implClassName}::create(${argumentString});\n";
    $code .= "    v8::Handle<v8::Object> wrapper = info.Holder();\n";

    if ($constructorRaisesException) {
        $code .= "    if (exceptionState.throwIfNeeded())\n";
        $code .= "        return;\n";
    }

    $code .= <<END;

    V8DOMWrapper::associateObjectWithWrapper<${v8ClassName}>(impl.release(), &${v8ClassName}::wrapperTypeInfo, wrapper, info.GetIsolate(), WrapperConfiguration::Dependent);
    v8SetReturnValue(info, wrapper);
}

END
    $implementation{nameSpaceInternal}->add($code);
}

# The Web IDL specification states that Interface objects for interfaces MUST have a property named
# "length" that returns the length of the shortest argument list of the entries in the effective
# overload set for constructors. In other words, use the lowest number of mandatory arguments among
# all constructors.
sub GetInterfaceLength
{
    my $interface = shift;

    my $leastConstructorLength = 0;
    if ($interface->extendedAttributes->{"EventConstructor"}) {
        $leastConstructorLength = 1;
    } elsif ($interface->extendedAttributes->{"Constructor"} || $interface->extendedAttributes->{"CustomConstructor"}) {
        my @constructors = @{$interface->constructors};
        my @customConstructors = @{$interface->customConstructors};
        $leastConstructorLength = 255;
        foreach my $constructor (@constructors, @customConstructors) {
            my $constructorLength = GetFunctionLength($constructor);
            $leastConstructorLength = $constructorLength if ($constructorLength < $leastConstructorLength);
        }
    }

    return $leastConstructorLength;
}

sub GenerateConstructorCallback
{
    my $interface = shift;

    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $code = "";
    $code .= "void ${v8ClassName}::constructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info)\n";
    $code .= "{\n";
    $code .= "    TRACE_EVENT_SCOPED_SAMPLING_STATE(\"Blink\", \"DOMConstructor\");\n";
    $code .= GenerateFeatureObservation($interface->extendedAttributes->{"MeasureAs"});
    $code .= GenerateDeprecationNotification($interface->extendedAttributes->{"DeprecateAs"});
    $code .= GenerateConstructorHeader($interface->name);
    if (HasCustomConstructor($interface)) {
        $code .= "    ${v8ClassName}::constructorCustom(info);\n";
    } else {
        $code .= "    ${implClassName}V8Internal::constructor(info);\n";
    }
    $code .= "}\n\n";
    $implementation{nameSpaceWebCore}->add($code);
}

sub GenerateConstructor
{
    my $interface = shift;

    if (@{$interface->constructors} == 1) {
        GenerateSingleConstructorCallback($interface, @{$interface->constructors}[0]);
    } else {
        foreach my $constructor (@{$interface->constructors}) {
            GenerateSingleConstructorCallback($interface, $constructor);
        }
        GenerateOverloadedConstructorCallback($interface);
    }
}

sub GenerateEventConstructor
{
    my $interface = shift;

    my $interfaceName = $interface->name;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    my $constructorRaisesException = $interface->extendedAttributes->{"RaisesException"} && $interface->extendedAttributes->{"RaisesException"} eq "Constructor";

    my @anyAttributeNames;
    foreach my $attribute (@{$interface->attributes}) {
        if ($attribute->type eq "any") {
            push(@anyAttributeNames, $attribute->name);
        }
    }

    AddToImplIncludes("bindings/v8/Dictionary.h");
    $implementation{nameSpaceInternal}->add(<<END);
static void constructor(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ConstructionContext, \"${interfaceName}\", info.Holder(), info.GetIsolate());
    if (info.Length() < 1) {
        exceptionState.throwTypeError("An event name must be provided.");
        exceptionState.throwIfNeeded();
        return;
    }

    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, type, info[0]);
END

    foreach my $attrName (@anyAttributeNames) {
        $implementation{nameSpaceInternal}->add("    v8::Local<v8::Value> ${attrName};\n");
    }

    $implementation{nameSpaceInternal}->add(<<END);
    ${implClassName}Init eventInit;
    if (info.Length() >= 2) {
        V8TRYCATCH_VOID(Dictionary, options, Dictionary(info[1], info.GetIsolate()));
        if (!initialize${implClassName}(eventInit, options, exceptionState, info)) {
            exceptionState.throwIfNeeded();
            return;
        }
END

    # Store 'any'-typed properties on the wrapper to avoid leaking them between isolated worlds.
    foreach my $attrName (@anyAttributeNames) {
        $implementation{nameSpaceInternal}->add(<<END);
        options.get("${attrName}", ${attrName});
        if (!${attrName}.IsEmpty())
            setHiddenValue(info.GetIsolate(), info.Holder(), "${attrName}", ${attrName});
END
    }

    $implementation{nameSpaceInternal}->add(<<END);
    }
END

    my $exceptionStateArgument = "";
    if ($constructorRaisesException) {
        ${exceptionStateArgument} = ", exceptionState";
    }

    $implementation{nameSpaceInternal}->add(<<END);
    RefPtr<${implClassName}> event = ${implClassName}::create(type, eventInit${exceptionStateArgument});
END

    if ($constructorRaisesException) {
        $implementation{nameSpaceInternal}->add(<<END);
    if (exceptionState.throwIfNeeded())
        return;
END
    }

    if (@anyAttributeNames) {
        # Separate check to simplify Python code
        AddToImplIncludes("bindings/v8/SerializedScriptValue.h");
    }
    if (@anyAttributeNames && $interface->name ne 'ErrorEvent') {
        # If we're in an isolated world, create a SerializedScriptValue and
        # store it in the event for later cloning if the property is accessed
        # from another world.
        # The main world case is handled lazily (in Custom code).
        #
        # We do not clone Error objects (exceptions), for 2 reasons:
        # 1) Errors carry a reference to the isolated world's global object,
        #    and thus passing it around would cause leakage.
        # 2) Errors cannot be cloned (or serialized):
        # http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#safe-passing-of-structured-data
        $implementation{nameSpaceInternal}->add("    if (DOMWrapperWorld::current(info.GetIsolate())->isIsolatedWorld()) {\n");
        foreach my $attrName (@anyAttributeNames) {
            my $setter = "setSerialized" . FirstLetterToUpperCase($attrName);
            $implementation{nameSpaceInternal}->add(<<END);
        if (!${attrName}.IsEmpty())
            event->${setter}(SerializedScriptValue::createAndSwallowExceptions(${attrName}, info.GetIsolate()));
END
        }
        $implementation{nameSpaceInternal}->add("    }\n\n");
    }

    $implementation{nameSpaceInternal}->add(<<END);
    v8::Handle<v8::Object> wrapper = info.Holder();
    V8DOMWrapper::associateObjectWithWrapper<${v8ClassName}>(event.release(), &${v8ClassName}::wrapperTypeInfo, wrapper, info.GetIsolate(), WrapperConfiguration::Dependent);
    v8SetReturnValue(info, wrapper);
}

END

    my $code = "";
    $code .= <<END;
bool initialize${implClassName}(${implClassName}Init& eventInit, const Dictionary& options, ExceptionState& exceptionState, const v8::FunctionCallbackInfo<v8::Value>& info, const String& forEventName)
{
    Dictionary::ConversionContext conversionContext(forEventName.isEmpty() ? String("${interfaceName}") : forEventName, "", exceptionState);
END

    if ($interface->parent) {
        my $interfaceBase = $interface->parent;
        $code .= <<END;
    if (!initialize${interfaceBase}(eventInit, options, exceptionState, info, forEventName.isEmpty() ? String("${interfaceName}") : forEventName))
        return false;

END
    }

    foreach my $attribute (@{$interface->attributes}) {
        if ($attribute->extendedAttributes->{"InitializedByEventConstructor"}) {
            if ($attribute->type ne "any") {
                my $attributeName = $attribute->name;
                my $attributeImplName = GetImplName($attribute);

                my $isNullable = $attribute->isNullable ? "true" : "false";
                my $dictionaryGetter = "options.convert(conversionContext.setConversionType(\"" . $attribute->type . "\", $isNullable), \"$attributeName\", eventInit.$attributeImplName)";
                my $deprecation = $attribute->extendedAttributes->{"DeprecateAs"};
                if ($deprecation) {
                    $code .= "    if ($dictionaryGetter) {\n";
                    $code .= "        if (options.hasProperty(\"$attributeName\"))\n";
                    $code .= "        " . GenerateDeprecationNotification($deprecation);
                    $code .= "    } else {\n";
                    $code .= "        return false;\n";
                    $code .= "    }\n";
                } else {
                    $code .= "    if (!$dictionaryGetter)\n";
                    $code .= "        return false;\n";
                }
            }
        }
    }

    $code .= <<END;
    return true;
}

END
    $implementation{nameSpaceWebCore}->add($code);
}

sub GenerateNamedConstructor
{
    my $function = shift;
    my $interface = shift;

    my $interfaceName = $interface->name;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $constructorRaisesException = $interface->extendedAttributes->{"RaisesException"} && $interface->extendedAttributes->{"RaisesException"} eq "Constructor";
    my $raisesExceptions = $function->extendedAttributes->{"RaisesException"} || $constructorRaisesException || HasExceptionRaisingParameter($function);

    my $maybeObserveFeature = GenerateFeatureObservation($function->extendedAttributes->{"MeasureAs"});
    my $maybeDeprecateFeature = GenerateDeprecationNotification($function->extendedAttributes->{"DeprecateAs"});

    my @beforeArgumentList;
    my @afterArgumentList;

    my $toActiveDOMObject = InheritsExtendedAttribute($interface, "ActiveDOMObject") ? "${v8ClassName}::toActiveDOMObject" : "0";
    my $toEventTarget = InheritsInterface($interface, "EventTarget") ? "${v8ClassName}::toEventTarget" : "0";
    my $derefObject = "${v8ClassName}::derefObject";
    my $isGarbageCollected = IsWillBeGarbageCollectedType($interfaceName) ? "true" : "false";

    $implementation{nameSpaceWebCore}->add(<<END);
const WrapperTypeInfo ${v8ClassName}Constructor::wrapperTypeInfo = { gin::kEmbedderBlink, ${v8ClassName}Constructor::domTemplate, $derefObject, $toActiveDOMObject, $toEventTarget, 0, ${v8ClassName}::installPerContextEnabledMethods, 0, WrapperTypeObjectPrototype, $isGarbageCollected };

END

    my $code = "";
    $code .= <<END;
static void ${v8ClassName}ConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
END
    $code .= $maybeObserveFeature if $maybeObserveFeature;
    $code .= $maybeDeprecateFeature if $maybeDeprecateFeature;
    $code .= GenerateConstructorHeader($function->extendedAttributes->{"NamedConstructor"});
    $code .= <<END;
    Document* document = currentDocument(info.GetIsolate());
    ASSERT(document);

    // Make sure the document is added to the DOM Node map. Otherwise, the ${implClassName} instance
    // may end up being the only node in the map and get garbage-collected prematurely.
    toV8(document, info.Holder(), info.GetIsolate());

END

    my $hasExceptionState = $raisesExceptions;
    if ($hasExceptionState) {
        $code .= "    ExceptionState exceptionState(ExceptionState::ConstructionContext, \"${interfaceName}\", info.Holder(), info.GetIsolate());\n";
    }
    $code .= GenerateArgumentsCountCheck($function, $interface, $hasExceptionState);

    my ($parameterCheckString, $paramIndex, %replacements) = GenerateParametersCheck($function, $interface, "", $hasExceptionState);
    $code .= $parameterCheckString;

    push(@beforeArgumentList, "*document");

    if ($constructorRaisesException) {
        push(@afterArgumentList, "exceptionState");
    }

    my @argumentList;
    my $index = 0;
    foreach my $parameter (@{$function->parameters}) {
        last if $index eq $paramIndex;
        if ($replacements{$parameter->name}) {
            push(@argumentList, $replacements{$parameter->name});
        } else {
            push(@argumentList, $parameter->name);
        }
        $index++;
    }

    my $argumentString = join(", ", @beforeArgumentList, @argumentList, @afterArgumentList);
    $code .= "    RefPtr<${implClassName}> impl = ${implClassName}::createForJSConstructor(${argumentString});\n";
    $code .= "    v8::Handle<v8::Object> wrapper = info.Holder();\n";

    if ($constructorRaisesException) {
        $code .= "    if (exceptionState.throwIfNeeded())\n";
        $code .= "        return;\n";
    }

    $code .= <<END;

    V8DOMWrapper::associateObjectWithWrapper<${v8ClassName}>(impl.release(), &${v8ClassName}Constructor::wrapperTypeInfo, wrapper, info.GetIsolate(), WrapperConfiguration::Dependent);
    v8SetReturnValue(info, wrapper);
}

END
    $implementation{nameSpaceWebCore}->add($code);

    $code = <<END;
v8::Handle<v8::FunctionTemplate> ${v8ClassName}Constructor::domTemplate(v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    static int domTemplateKey; // This address is used for a key to look up the dom template.
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    v8::Local<v8::FunctionTemplate> result = data->existingDOMTemplate(currentWorldType, &domTemplateKey);
    if (!result.IsEmpty())
        return result;

    TRACE_EVENT_SCOPED_SAMPLING_STATE("Blink", "BuildDOMTemplate");
    v8::EscapableHandleScope scope(isolate);
    result = v8::FunctionTemplate::New(isolate, ${v8ClassName}ConstructorCallback);

    v8::Local<v8::ObjectTemplate> instanceTemplate = result->InstanceTemplate();
    instanceTemplate->SetInternalFieldCount(${v8ClassName}::internalFieldCount);
    result->SetClassName(v8AtomicString(isolate, "${implClassName}"));
    result->Inherit(${v8ClassName}::domTemplate(isolate, currentWorldType));
    data->setDOMTemplate(currentWorldType, &domTemplateKey, result);

    return scope.Escape(result);
}

END
    $implementation{nameSpaceWebCore}->add($code);
}

sub GenerateConstructorHeader
{
    my $constructorName = shift;

    AddToImplIncludes("bindings/v8/V8ObjectConstructor.h");
    my $content = <<END;
    if (!info.IsConstructCall()) {
        throwTypeError(ExceptionMessages::failedToConstruct("$constructorName", "Please use the 'new' operator, this DOM object constructor cannot be called as a function."), info.GetIsolate());
        return;
    }

    if (ConstructorMode::current() == ConstructorMode::WrapExistingObject) {
        v8SetReturnValue(info, info.Holder());
        return;
    }

END
    return $content;
}

sub GenerateAttributeConfigurationArray
{
    my $interface = shift;
    my $attributes = shift;
    my $exposeJSAccessors = shift;

    my $code = "";
    foreach my $attribute (@$attributes) {
        my $conditionalString = GenerateConditionalString($attribute);
        my $subCode = "";
        $subCode .= "#if ${conditionalString}\n" if $conditionalString;
        $subCode .= GenerateAttributeConfiguration($interface, $attribute, ",", "", $exposeJSAccessors);
        $subCode .= "#endif // ${conditionalString}\n" if $conditionalString;
        $code .= $subCode;
    }
    return $code;
}

sub GenerateAttributeConfigurationParameters
{
    my $interface = shift;
    my $attribute = shift;
    my $attrName = $attribute->name;
    my $attrExt = $attribute->extendedAttributes;
    my $implClassName = GetImplName($interface);

    my @accessControlList;
    if (my $doNotCheckSecurity = $attrExt->{"DoNotCheckSecurity"}) {
        if ($doNotCheckSecurity eq "Getter") {
            push(@accessControlList, "v8::ALL_CAN_READ");
        } elsif ($doNotCheckSecurity eq "Setter") {
            push(@accessControlList, "v8::ALL_CAN_WRITE");
        } else {
            push(@accessControlList, "v8::ALL_CAN_READ");
            if (!IsReadonly($attribute)) {
                push(@accessControlList, "v8::ALL_CAN_WRITE");
            }
        }
    }
    if ($attrExt->{"Unforgeable"}) {
        push(@accessControlList, "v8::PROHIBITS_OVERWRITING");
    }
    @accessControlList = ("v8::DEFAULT") unless @accessControlList;
    my $accessControl = "static_cast<v8::AccessControl>(" . join(" | ", @accessControlList) . ")";

    my $customAccessor = HasCustomGetter($attrExt) || HasCustomSetter($attribute) || "";
    if ($customAccessor eq "VALUE_IS_MISSING") {
        # use the naming convension, interface + (capitalize) attr name
        $customAccessor = $implClassName . "::" . $attrName;
    }

    my $getter;
    my $setter;
    my $getterForMainWorld;
    my $setterForMainWorld;

    my $isConstructor = ($attribute->type =~ /Constructor$/);

    # Check attributes.
    # As per Web IDL specification, constructor properties on the ECMAScript global object should be
    # configurable and should not be enumerable.
    my @propAttributeList;
    if ($attrExt->{"NotEnumerable"} || $isConstructor) {
        push(@propAttributeList, "v8::DontEnum");
    }
    if ($attrExt->{"Unforgeable"} && !$isConstructor) {
        push(@propAttributeList, "v8::DontDelete");
    }
    @propAttributeList = ("v8::None") unless @propAttributeList;
    my $propAttribute = join(" | ", @propAttributeList);

    my $onProto = "0 /* on instance */";
    my $data = "0";  # no data

    # Constructor
    if ($isConstructor) {
        my $constructorType = $attribute->type;
        $constructorType =~ s/Constructor$//;

        # For NamedConstructors we do not generate a header file. The code for the NamedConstructor
        # gets generated when we generate the code for its interface.
        if ($constructorType !~ /Constructor$/) {
            AddToImplIncludes("V8${constructorType}.h");
        }
        $data = "const_cast<WrapperTypeInfo*>(&V8${constructorType}::wrapperTypeInfo)";
        $getter = "${implClassName}V8Internal::${implClassName}ConstructorGetter";
        $setter = "${implClassName}V8Internal::${implClassName}ReplaceableAttributeSetterCallback";
        $getterForMainWorld = "0";
        $setterForMainWorld = "0";
    } else {
        # Default Getter and Setter
        $getter = "${implClassName}V8Internal::${attrName}AttributeGetterCallback";
        $setter = "${implClassName}V8Internal::${attrName}AttributeSetterCallback";
        $getterForMainWorld = "${getter}ForMainWorld";
        $setterForMainWorld = "${setter}ForMainWorld";

        if (!HasCustomSetter($attribute) && !$attrExt->{"PutForwards"} && $attrExt->{"Replaceable"}) {
            $setter = "${implClassName}V8Internal::${implClassName}ReplaceableAttributeSetterCallback";
            $setterForMainWorld = "0";
        }
    }

    # Read only attributes
    if (IsReadonly($attribute)) {
        $setter = "0";
        $setterForMainWorld = "0";
    }

    # An accessor can be installed on the prototype
    if ($attrExt->{"OnPrototype"}) {
        $onProto = "1 /* on prototype */";
    }

    if (!$attrExt->{"PerWorldBindings"}) {
      $getterForMainWorld = "0";
      $setterForMainWorld = "0";
    }

    return ($attrName, $getter, $setter, $getterForMainWorld, $setterForMainWorld, $data, $accessControl, "static_cast<v8::PropertyAttribute>($propAttribute)", $onProto);
}

sub GenerateAttributeConfiguration
{
    my $interface = shift;
    my $attribute = shift;
    my $delimiter = shift;
    my $indent = shift;
    my $exposeJSAccessors = shift;

    my $code = "";

    my ($attrName, $getter, $setter, $getterForMainWorld, $setterForMainWorld, $data, $accessControl, $propAttribute, $onProto) = GenerateAttributeConfigurationParameters($interface, $attribute, $exposeJSAccessors);
    if ($exposeJSAccessors) {
        $code .= $indent . "    {\"$attrName\", $getter, $setter, $getterForMainWorld, $setterForMainWorld, $data, $accessControl, $propAttribute}" . $delimiter . "\n";
    } else {
        $code .= $indent . "    {\"$attrName\", $getter, $setter, $getterForMainWorld, $setterForMainWorld, $data, $accessControl, $propAttribute, $onProto}" . $delimiter . "\n";
    }
    return $code;
}

sub GenerateStaticAttribute
{
    my $interface = shift;
    my $attribute = shift;
    my $attrExt = $attribute->extendedAttributes;
    my $code = "";

    my ($attrName, $getter, $setter, $getterForMainWorld, $setterForMainWorld, $data, $accessControl, $propAttribute, $onProto) = GenerateAttributeConfigurationParameters($interface, $attribute);

    die "Static attributes do not support optimized getters or setters for the main world" if $getterForMainWorld || $setterForMainWorld;

    my $conditionalString = GenerateConditionalString($attribute);

    $code .= "#if ${conditionalString}\n" if $conditionalString;
    $code .= "    functionTemplate->SetNativeDataProperty(v8AtomicString(isolate, \"$attrName\"), $getter, $setter, v8::External::New(isolate, $data), $propAttribute, v8::Handle<v8::AccessorSignature>(), $accessControl);\n";
    $code .= "#endif // ${conditionalString}\n" if $conditionalString;

    return $code;
}

sub IsStandardFunction
{
    my $interface = shift;
    my $function = shift;

    my $interfaceName = $interface->name;
    my $attrExt = $function->extendedAttributes;
    return 0 if $attrExt->{"Unforgeable"};
    return 0 if $function->isStatic;
    return 0 if $attrExt->{"RuntimeEnabled"};
    return 0 if $attrExt->{"PerContextEnabled"};
    return 0 if $attrExt->{"DoNotCheckSignature"};
    return 0 if ($attrExt->{"DoNotCheckSecurity"} && ($interface->extendedAttributes->{"CheckSecurity"} || $interfaceName eq "Window"));
    return 0 if $attrExt->{"NotEnumerable"};
    return 0 if $attrExt->{"ReadOnly"};
    return 1;
}

sub GenerateNonStandardFunction
{
    my $interface = shift;
    my $function = shift;
    my $code = "";

    my $implClassName = GetImplName($interface);
    my $attrExt = $function->extendedAttributes;
    my $name = $function->name;

    my $property_attributes = "v8::DontDelete";
    if ($attrExt->{"NotEnumerable"}) {
        $property_attributes .= " | v8::DontEnum";
    }
    if ($attrExt->{"ReadOnly"}) {
        $property_attributes .= " | v8::ReadOnly";
    }

    my $commentInfo = "Function '$name' (Extended Attributes: '" . join(' ', keys(%{$attrExt})) . "')";

    my $template = "prototypeTemplate";
    if ($attrExt->{"Unforgeable"}) {
        $template = "instanceTemplate";
    }
    if ($function->isStatic) {
        $template = "functionTemplate";
    }

    my $conditional4 = "";  # 4 space indent context
    my $conditional8 = "";  # 8 space indent context
    if ($attrExt->{"RuntimeEnabled"}) {
        # Only call Set()/SetAccessor() if this method should be enabled
        my $runtimeEnabledFunction = GetRuntimeEnabledFunctionName($function);
        $conditional4 = "if (${runtimeEnabledFunction}())\n        ";
    }
    if ($attrExt->{"PerContextEnabled"}) {
        # Only call Set()/SetAccessor() if this method should be enabled
        my $contextEnabledFunction = GetContextEnabledFunctionName($function);
        $conditional4 = "if (${contextEnabledFunction}(impl->document()))\n        ";
    }
    $conditional8 = $conditional4 . "    " if $conditional4 ne "";

    if ($interface->extendedAttributes->{"CheckSecurity"} && $attrExt->{"DoNotCheckSecurity"}) {
        my $setter = $attrExt->{"ReadOnly"} ? "0" : "${implClassName}V8Internal::${implClassName}OriginSafeMethodSetterCallback";
        # Functions that are marked DoNotCheckSecurity are always readable but if they are changed
        # and then accessed on a different domain we do not return the underlying value but instead
        # return a new copy of the original function. This is achieved by storing the changed value
        # as hidden property.
        if ($function->extendedAttributes->{"PerWorldBindings"}) {
            $code .= <<END;
    if (currentWorldType == MainWorld) {
        ${conditional8}$template->SetAccessor(v8AtomicString(isolate, "$name"), ${implClassName}V8Internal::${name}OriginSafeMethodGetterCallbackForMainWorld, ${setter}, v8Undefined(), v8::ALL_CAN_READ, static_cast<v8::PropertyAttribute>($property_attributes));
    } else {
        ${conditional8}$template->SetAccessor(v8AtomicString(isolate, "$name"), ${implClassName}V8Internal::${name}OriginSafeMethodGetterCallback, ${setter}, v8Undefined(), v8::ALL_CAN_READ, static_cast<v8::PropertyAttribute>($property_attributes));
    }
END
        } else {
            $code .= "    ${conditional4}$template->SetAccessor(v8AtomicString(isolate, \"$name\"), ${implClassName}V8Internal::${name}OriginSafeMethodGetterCallback, ${setter}, v8Undefined(), v8::ALL_CAN_READ, static_cast<v8::PropertyAttribute>($property_attributes));\n";
        }

        return $code;
    }

    my $signature = "defaultSignature";
    if ($attrExt->{"DoNotCheckSignature"} || $function->isStatic) {
       $signature = "v8::Local<v8::Signature>()";
    }

    my $conditionalString = GenerateConditionalString($function);
    $code .= "#if ${conditionalString}\n" if $conditionalString;

    if ($property_attributes eq "v8::DontDelete") {
        $property_attributes = "";
    } else {
        $property_attributes = ", static_cast<v8::PropertyAttribute>($property_attributes)";
    }

    if ($template eq "prototypeTemplate" && $conditional4 eq "" && $signature eq "defaultSignature" && $property_attributes eq "") {
        die "This shouldn't happen: Class '$implClassName' $commentInfo\n";
    }

    my $functionLength = GetFunctionLength($function);

    if ($function->extendedAttributes->{"PerWorldBindings"}) {
        $code .= "    if (currentWorldType == MainWorld) {\n";
        $code .= "        ${conditional8}$template->Set(v8AtomicString(isolate, \"$name\"), v8::FunctionTemplate::New(isolate, ${implClassName}V8Internal::${name}MethodCallbackForMainWorld, v8Undefined(), ${signature}, $functionLength)$property_attributes);\n";
        $code .= "    } else {\n";
        $code .= "        ${conditional8}$template->Set(v8AtomicString(isolate, \"$name\"), v8::FunctionTemplate::New(isolate, ${implClassName}V8Internal::${name}MethodCallback, v8Undefined(), ${signature}, $functionLength)$property_attributes);\n";
        $code .= "    }\n";
    } else {
        $code .= "    ${conditional4}$template->Set(v8AtomicString(isolate, \"$name\"), v8::FunctionTemplate::New(isolate, ${implClassName}V8Internal::${name}MethodCallback, v8Undefined(), ${signature}, $functionLength)$property_attributes);\n";
    }
    $code .= "#endif // ${conditionalString}\n" if $conditionalString;
    return $code;
}

sub GenerateIsNullExpression
{
    my $type = shift;
    my $variableName = shift;
    if (IsUnionType($type)) {
        my $types = $type->unionMemberTypes;
        my @expression = ();
        for my $i (0 .. scalar(@$types)-1) {
            my $unionMemberEnabledVariable = $variableName . $i . "Enabled";
            push @expression, "!${unionMemberEnabledVariable}";
        }
        return join " && ", @expression;
    }
    if (IsRefPtrType($type)) {
        return "!${variableName}";
    } elsif ($type eq "DOMString") {
        return "${variableName}.isNull()";
    } elsif ($type eq "Promise") {
        return "${variableName}.isNull()";
    } else {
        return "";
    }
}

sub GenerateIfElseStatement
{
    my $type = shift;
    my $outputVariableName = shift;
    my $conditions = shift;
    my $statements = shift;

    my $code = "";
    if (@$conditions == 1) {
        $code .= "    ${type} ${outputVariableName} = " . $statements->[0] . "\n";
    } else {
        $code .= "    ${type} ${outputVariableName};\n";
        for my $i (0 .. @$conditions - 1) {
            my $token = "else if";
            $token = "if" if $i == 0;
            $token = "else" if $i == @$conditions - 1;
            $code .= "    ${token}";
            $code .= " (" . $conditions->[$i] . ")" if $conditions->[$i];
            $code .= "\n";
            $code .= "        ${outputVariableName} = " . $statements->[$i] . "\n";
        }
    }
    return $code;
}

sub GenerateImplementationIndexedPropertyAccessors
{
    my $interface = shift;
    my $interfaceName = $interface->name;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    my $indexedGetterFunction = GetIndexedGetterFunction($interface);
    if ($indexedGetterFunction) {
        # includes for return type even if custom
        my $returnType = $indexedGetterFunction->type;
        AddIncludesForType($returnType);
        my $hasCustomIndexedGetter = $indexedGetterFunction->extendedAttributes->{"Custom"};
        if (!$hasCustomIndexedGetter) {
            GenerateImplementationIndexedPropertyGetter($interface, $indexedGetterFunction);
        }
        GenerateImplementationIndexedPropertyGetterCallback($interface, $hasCustomIndexedGetter);
    }

    my $indexedSetterFunction = GetIndexedSetterFunction($interface);
    if ($indexedSetterFunction) {
        AddIncludesForType($indexedSetterFunction->parameters->[1]->type);  # includes for argument type even if custom
        my $hasCustomIndexedSetter = $indexedSetterFunction->extendedAttributes->{"Custom"};
        if (!$hasCustomIndexedSetter) {
            GenerateImplementationIndexedPropertySetter($interface, $indexedSetterFunction);
        }
        GenerateImplementationIndexedPropertySetterCallback($interface, $hasCustomIndexedSetter);
    }

    my $indexedDeleterFunction = GetIndexedDeleterFunction($interface);
    if ($indexedDeleterFunction) {
        my $hasCustomIndexedDeleter = $indexedDeleterFunction->extendedAttributes->{"Custom"};
        if (!$hasCustomIndexedDeleter) {
            GenerateImplementationIndexedPropertyDeleter($interface, $indexedDeleterFunction);
        }
        GenerateImplementationIndexedPropertyDeleterCallback($interface, $hasCustomIndexedDeleter);
    }

    my $indexedEnumeratorFunction = $indexedGetterFunction;
    $indexedEnumeratorFunction = 0 if $indexedGetterFunction && $indexedGetterFunction->extendedAttributes->{"NotEnumerable"};

    my $indexedQueryFunction = 0;
    # If there is an enumerator, there MUST be a query method to properly communicate property attributes.
    my $hasQuery = $indexedQueryFunction || $indexedEnumeratorFunction;

    my $setOn = "Instance";

    # V8 has access-check callback API (see ObjectTemplate::SetAccessCheckCallbacks) and it's used on Window
    # instead of deleters or enumerators. In addition, the getter should be set on prototype template, to
    # get implementation straight out of the Window prototype regardless of what prototype is actually set
    # on the object.
    if ($interfaceName eq "Window") {
        $setOn = "Prototype";
    }

    my $code = "";
    if ($indexedGetterFunction || $indexedSetterFunction || $indexedDeleterFunction || $indexedEnumeratorFunction || $hasQuery) {
        $code .= "    functionTemplate->${setOn}Template()->SetIndexedPropertyHandler(${implClassName}V8Internal::indexedPropertyGetterCallback";
        $code .= $indexedSetterFunction ? ", ${implClassName}V8Internal::indexedPropertySetterCallback" : ", 0";
        $code .= ", 0"; # IndexedPropertyQuery -- not being used at the moment.
        $code .= $indexedDeleterFunction ? ", ${implClassName}V8Internal::indexedPropertyDeleterCallback" : ", 0";
        $code .= $indexedEnumeratorFunction ? ", indexedPropertyEnumerator<${implClassName}>" : ", 0";
        $code .= ");\n";
    }

    return $code;
}

sub GenerateImplementationIndexedPropertyGetter
{
    my $interface = shift;
    my $indexedGetterFunction = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $methodName = GetImplName($indexedGetterFunction) || "anonymousIndexedGetter";

    my $returnType = $indexedGetterFunction->type;
    my $nativeType = GetNativeType($returnType);
    my $nativeValue = "result";
    $nativeValue .= ".release()" if (IsRefPtrType($returnType));
    my $isNull = GenerateIsNullExpression($returnType, "result");
    my $returnJSValueCode = NativeToJSValue($indexedGetterFunction->type, $indexedGetterFunction->extendedAttributes, $nativeValue, "    ", "", "info.GetIsolate()", "info", "imp", "", "return");
    my $raisesExceptions = $indexedGetterFunction->extendedAttributes->{"RaisesException"};
    my $methodCallCode = GenerateMethodCall($returnType, "result", "imp->${methodName}", "index", $raisesExceptions);
    my $getterCode = "static void indexedPropertyGetter(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)\n";
    $getterCode .= "{\n";
    $getterCode .= "    ${implClassName}* imp = ${v8ClassName}::toNative(info.Holder());\n";
    if ($raisesExceptions) {
        $getterCode .= "    ExceptionState exceptionState(info.Holder(), info.GetIsolate());\n";
    }
    $getterCode .= $methodCallCode . "\n";
    if ($raisesExceptions) {
        $getterCode .= "    if (exceptionState.throwIfNeeded())\n";
        $getterCode .= "        return;\n";
    }
    if (IsUnionType($returnType)) {
        $getterCode .= "${returnJSValueCode}\n";
        $getterCode .= "    return;\n";
    } else {
        $getterCode .= "    if (${isNull})\n";
        $getterCode .= "        return;\n";
        $getterCode .= $returnJSValueCode . "\n";
    }
    $getterCode .= "}\n\n";
    $implementation{nameSpaceInternal}->add($getterCode);
}

sub GenerateImplementationIndexedPropertyGetterCallback
{
    my $interface = shift;
    my $hasCustom = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    my $code = "static void indexedPropertyGetterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)\n";
    $code .= "{\n";
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"Blink\", \"DOMIndexedProperty\");\n";
    if ($hasCustom) {
        $code .= "    ${v8ClassName}::indexedPropertyGetterCustom(index, info);\n";
    } else {
        $code .= "    ${implClassName}V8Internal::indexedPropertyGetter(index, info);\n";
    }
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"V8\", \"V8Execution\");\n";
    $code .= "}\n\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateImplementationIndexedPropertySetterCallback
{
    my $interface = shift;
    my $hasCustom = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    my $code = "static void indexedPropertySetterCallback(uint32_t index, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<v8::Value>& info)\n";
    $code .= "{\n";
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"Blink\", \"DOMIndexedProperty\");\n";
    if ($hasCustom) {
        $code .= "    ${v8ClassName}::indexedPropertySetterCustom(index, jsValue, info);\n";
    } else {
        $code .= "    ${implClassName}V8Internal::indexedPropertySetter(index, jsValue, info);\n";
    }
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"V8\", \"V8Execution\");\n";
    $code .= "}\n\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateImplementationIndexedPropertyDeleterCallback
{
    my $interface = shift;
    my $hasCustom = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    my $code = "static void indexedPropertyDeleterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info)\n";
    $code .= "{\n";
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"Blink\", \"DOMIndexedProperty\");\n";
    if ($hasCustom) {
        $code .= "    ${v8ClassName}::indexedPropertyDeleterCustom(index, info);\n";
    } else {
        $code .= "    ${implClassName}V8Internal::indexedPropertyDeleter(index, info);\n";
    }
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"V8\", \"V8Execution\");\n";
    $code .= "}\n\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateImplementationIndexedPropertySetter
{
    my $interface = shift;
    my $indexedSetterFunction = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $methodName = GetImplName($indexedSetterFunction) || "anonymousIndexedSetter";
    my $interfaceName = $interface->name;

    my $type = $indexedSetterFunction->parameters->[1]->type;
    my $raisesExceptions = $indexedSetterFunction->extendedAttributes->{"RaisesException"};
    my $treatNullAs = $indexedSetterFunction->parameters->[1]->extendedAttributes->{"TreatNullAs"};
    my $treatUndefinedAs = $indexedSetterFunction->parameters->[1]->extendedAttributes->{"TreatUndefinedAs"};

    my $code = "static void indexedPropertySetter(uint32_t index, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<v8::Value>& info)\n";
    $code .= "{\n";

    my $asSetterValue = 0;
    $code .= "    ${implClassName}* imp = ${v8ClassName}::toNative(info.Holder());\n";
    $code .= JSValueToNativeStatement($indexedSetterFunction->parameters->[1]->type, $indexedSetterFunction->extendedAttributes, $asSetterValue, "jsValue", "propertyValue", "    ", "info.GetIsolate()");

    my $extraArguments = "";
    if ($raisesExceptions || IsIntegerType($type)) {
        $code .= "    ExceptionState exceptionState(info.Holder(), info.GetIsolate());\n";
        if ($raisesExceptions) {
            $extraArguments = ", exceptionState";
        }
    }

    if ($indexedSetterFunction->extendedAttributes->{"StrictTypeChecking"} && IsWrapperType($type)) {
        $code .= <<END;
    if (!isUndefinedOrNull(jsValue) && !V8${type}::hasInstance(jsValue, info.GetIsolate())) {
        exceptionState.throwTypeError("The provided value is not of type '$type'.");
        exceptionState.throwIfNeeded();
        return;
    }
END
    }

    $code .= "    bool result = imp->${methodName}(index, propertyValue$extraArguments);\n";

    if ($raisesExceptions) {
        $code .= "    if (exceptionState.throwIfNeeded())\n";
        $code .= "        return;\n";
    }
    $code .= "    if (!result)\n";
    $code .= "        return;\n";
    $code .= "    v8SetReturnValue(info, jsValue);\n";
    $code .= "}\n\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateImplementationNamedPropertyAccessors
{
    my $interface = shift;

    my $interfaceName = $interface->name;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    my $namedGetterFunction = GetNamedGetterFunction($interface);
    my $namedEnumeratorFunction = $namedGetterFunction && !$namedGetterFunction->extendedAttributes->{"NotEnumerable"};

    if ($namedGetterFunction) {
        # includes for return type even if custom
        my $returnType = $namedGetterFunction->type;
        AddIncludesForType($returnType);
        my $hasCustomNamedGetter = HasCustomPropertyGetter($namedGetterFunction->extendedAttributes);
        if (!$hasCustomNamedGetter) {
            GenerateImplementationNamedPropertyGetter($interface, $namedGetterFunction);
        }
        GenerateImplementationNamedPropertyGetterCallback($interface, $hasCustomNamedGetter);
    }

    my $namedSetterFunction = GetNamedSetterFunction($interface);
    if ($namedSetterFunction) {
        AddIncludesForType($namedSetterFunction->parameters->[1]->type);  # includes for argument type even if custom
        my $hasCustomNamedSetter = $namedSetterFunction->extendedAttributes->{"Custom"};
        if (!$hasCustomNamedSetter) {
            GenerateImplementationNamedPropertySetter($interface, $namedSetterFunction);
        }
        GenerateImplementationNamedPropertySetterCallback($interface, $hasCustomNamedSetter);
    }

    if ($namedEnumeratorFunction) {
        my $hasCustomNamedQuery = ExtendedAttributeContains($namedGetterFunction->extendedAttributes->{"Custom"}, "PropertyQuery");
        if (!$hasCustomNamedQuery) {
            GenerateImplementationNamedPropertyQuery($interface);
        }
        GenerateImplementationNamedPropertyQueryCallback($interface, $hasCustomNamedQuery);
    }

    my $namedDeleterFunction = GetNamedDeleterFunction($interface);
    if ($namedDeleterFunction) {
        my $hasCustomNamedDeleter = $namedDeleterFunction->extendedAttributes->{"Custom"};
        if (!$hasCustomNamedDeleter) {
            GenerateImplementationNamedPropertyDeleter($interface, $namedDeleterFunction);
        }
        GenerateImplementationNamedPropertyDeleterCallback($interface, $hasCustomNamedDeleter);
    }

    if ($namedEnumeratorFunction) {
        my $hasCustomNamedEnumerator = ExtendedAttributeContains($namedGetterFunction->extendedAttributes->{"Custom"}, "PropertyEnumerator");
        if (!$hasCustomNamedEnumerator) {
            GenerateImplementationNamedPropertyEnumerator($interface);
        }
        GenerateImplementationNamedPropertyEnumeratorCallback($interface, $hasCustomNamedEnumerator);
    }

    my $subCode = "";
    if ($namedGetterFunction || $namedSetterFunction || $namedDeleterFunction || $namedEnumeratorFunction) {
        my $setOn = "Instance";

        # V8 has access-check callback API (see ObjectTemplate::SetAccessCheckCallbacks) and it's used on Window
        # instead of deleters or enumerators. In addition, the getter should be set on prototype template, to
        # get implementation straight out of the Window prototype regardless of what prototype is actually set
        # on the object.
        if ($interfaceName eq "Window") {
            $setOn = "Prototype";
        }

        $subCode .= "    functionTemplate->${setOn}Template()->SetNamedPropertyHandler(";
        $subCode .= $namedGetterFunction ? "${implClassName}V8Internal::namedPropertyGetterCallback, " : "0, ";
        $subCode .= $namedSetterFunction ? "${implClassName}V8Internal::namedPropertySetterCallback, " : "0, ";
        $subCode .= $namedEnumeratorFunction ? "${implClassName}V8Internal::namedPropertyQueryCallback, " : "0, ";
        $subCode .= $namedDeleterFunction ? "${implClassName}V8Internal::namedPropertyDeleterCallback, " : "0, ";
        $subCode .= $namedEnumeratorFunction ? "${implClassName}V8Internal::namedPropertyEnumeratorCallback" : "0";
        $subCode .= ");\n";
    }

    return $subCode;
}

sub GenerateImplementationNamedPropertyGetterCallback
{
    my $interface = shift;
    my $hasCustom = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    my $code = "static void namedPropertyGetterCallback(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)\n";
    $code .= "{\n";
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"Blink\", \"DOMNamedProperty\");\n";
    if ($hasCustom) {
        $code .= "    ${v8ClassName}::namedPropertyGetterCustom(name, info);\n";
    } else {
        $code .= "    ${implClassName}V8Internal::namedPropertyGetter(name, info);\n";
    }
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"V8\", \"V8Execution\");\n";
    $code .= "}\n\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateImplementationNamedPropertySetterCallback
{
    my $interface = shift;
    my $hasCustom = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    my $code = "static void namedPropertySetterCallback(v8::Local<v8::String> name, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<v8::Value>& info)\n";
    $code .= "{\n";
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"Blink\", \"DOMNamedProperty\");\n";
    if ($hasCustom) {
        $code .= "    ${v8ClassName}::namedPropertySetterCustom(name, jsValue, info);\n";
    } else {
        $code .= "    ${implClassName}V8Internal::namedPropertySetter(name, jsValue, info);\n";
    }
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"V8\", \"V8Execution\");\n";
    $code .= "}\n\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateImplementationNamedPropertyDeleterCallback
{
    my $interface = shift;
    my $hasCustom = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    my $code = "static void namedPropertyDeleterCallback(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Boolean>& info)\n";
    $code .= "{\n";
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"Blink\", \"DOMNamedProperty\");\n";
    if ($hasCustom) {
        $code .= "    ${v8ClassName}::namedPropertyDeleterCustom(name, info);\n";
    } else {
        $code .= "    ${implClassName}V8Internal::namedPropertyDeleter(name, info);\n";
    }
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"V8\", \"V8Execution\");\n";
    $code .= "}\n\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateImplementationNamedPropertyEnumeratorCallback
{
    my $interface = shift;
    my $hasCustom = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    my $code = "static void namedPropertyEnumeratorCallback(const v8::PropertyCallbackInfo<v8::Array>& info)\n";
    $code .= "{\n";
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"Blink\", \"DOMNamedProperty\");\n";
    if ($hasCustom) {
        $code .= "    ${v8ClassName}::namedPropertyEnumeratorCustom(info);\n";
    } else {
        $code .= "    ${implClassName}V8Internal::namedPropertyEnumerator(info);\n";
    }
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"V8\", \"V8Execution\");\n";
    $code .= "}\n\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateImplementationNamedPropertyQueryCallback
{
    my $interface = shift;
    my $hasCustom = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    my $code = "static void namedPropertyQueryCallback(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Integer>& info)\n";
    $code .= "{\n";
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"Blink\", \"DOMNamedProperty\");\n";
    if ($hasCustom) {
        $code .= "    ${v8ClassName}::namedPropertyQueryCustom(name, info);\n";
    } else {
        $code .= "    ${implClassName}V8Internal::namedPropertyQuery(name, info);\n";
    }
    $code .= "    TRACE_EVENT_SET_SAMPLING_STATE(\"V8\", \"V8Execution\");\n";
    $code .= "}\n\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateMethodCall
{
    my $returnType = shift; # string or UnionType
    my $returnName = shift;
    my $functionExpression = shift;
    my $firstArgument = shift;
    my $raisesExceptions = shift;

    my @arguments = ();
    push @arguments, $firstArgument;
    if ($raisesExceptions) {
        push @arguments, "exceptionState";
    }

    if (IsUnionType($returnType)) {
        my $code = "";
        my @extraArguments = ();
        for my $i (0..scalar(@{$returnType->unionMemberTypes})-1) {
            my $unionMemberType = $returnType->unionMemberTypes->[$i];
            my $nativeType = GetNativeType($unionMemberType);
            my $unionMemberVariable = $returnName . $i;
            my $unionMemberEnabledVariable = $returnName . $i . "Enabled";
            $code .= "    bool ${unionMemberEnabledVariable} = false;\n";
            $code .= "    ${nativeType} ${unionMemberVariable};\n";
            push @extraArguments, $unionMemberEnabledVariable;
            push @extraArguments, $unionMemberVariable;
        }
        push @arguments, @extraArguments;
        $code .= "    ${functionExpression}(" . (join ", ", @arguments) . ");";
        return $code;
    } else {
        my $nativeType = GetNativeType($returnType);
        return "    ${nativeType} result = ${functionExpression}(" . (join ", ", @arguments) . ");"
    }
}

sub GenerateImplementationNamedPropertyGetter
{
    my $interface = shift;
    my $namedGetterFunction = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $methodName = GetImplName($namedGetterFunction) || "anonymousNamedGetter";

    my $returnType = $namedGetterFunction->type;
    my $isNull = GenerateIsNullExpression($returnType, "result");
    my $nativeValue = "result";
    $nativeValue .= ".release()" if (IsRefPtrType($returnType));
    my $returnJSValueCode = NativeToJSValue($namedGetterFunction->type, $namedGetterFunction->extendedAttributes, $nativeValue, "    ", "", "info.GetIsolate()", "info", "imp", "", "return");
    my $raisesExceptions = $namedGetterFunction->extendedAttributes->{"RaisesException"};
    my $methodCallCode = GenerateMethodCall($returnType, "result", "imp->${methodName}", "propertyName", $raisesExceptions);

    my $code = "static void namedPropertyGetter(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)\n";
    $code .= "{\n";
    if (!$interface->extendedAttributes->{"OverrideBuiltins"}) {
        $code .= "    if (info.Holder()->HasRealNamedProperty(name))\n";
        $code .= "        return;\n";
        $code .= "    if (!info.Holder()->GetRealNamedPropertyInPrototypeChain(name).IsEmpty())\n";
        $code .= "        return;\n";
        $code .= "\n";
    }
    $code .= "    ${implClassName}* imp = ${v8ClassName}::toNative(info.Holder());\n";
    $code .= "    AtomicString propertyName = toCoreAtomicString(name);\n";
    if ($raisesExceptions) {
        $code .= "    ExceptionState exceptionState(info.Holder(), info.GetIsolate());\n";
    }
    $code .= $methodCallCode . "\n";
    if ($raisesExceptions) {
        $code .= "    if (exceptionState.throwIfNeeded())\n";
        $code .= "        return;\n";
    }
    $code .= "    if (${isNull})\n";
    $code .= "        return;\n";
    $code .= $returnJSValueCode . "\n";
    $code .= "}\n\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateImplementationNamedPropertySetter
{
    my $interface = shift;
    my $namedSetterFunction = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $methodName = GetImplName($namedSetterFunction) || "anonymousNamedSetter";
    my $interfaceName = $interface->name;

    my $type = $namedSetterFunction->parameters->[1]->type;
    my $raisesExceptions = $namedSetterFunction->extendedAttributes->{"RaisesException"};
    my $treatNullAs = $namedSetterFunction->parameters->[1]->extendedAttributes->{"TreatNullAs"};
    my $treatUndefinedAs = $namedSetterFunction->parameters->[1]->extendedAttributes->{"TreatUndefinedAs"};

    my $code = "static void namedPropertySetter(v8::Local<v8::String> name, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<v8::Value>& info)\n";
    $code .= "{\n";
    if (!$interface->extendedAttributes->{"OverrideBuiltins"}) {
        $code .= "    if (info.Holder()->HasRealNamedProperty(name))\n";
        $code .= "        return;\n";
        $code .= "    if (!info.Holder()->GetRealNamedPropertyInPrototypeChain(name).IsEmpty())\n";
        $code .= "        return;\n";
        $code .= "\n";
    }

    my $asSetterValue = 0;
    $code .= "    ${implClassName}* imp = ${v8ClassName}::toNative(info.Holder());\n";
    $code .= JSValueToNativeStatement($namedSetterFunction->parameters->[0]->type, $namedSetterFunction->extendedAttributes, $asSetterValue, "name", "propertyName", "    ", "info.GetIsolate()");
    $code .= JSValueToNativeStatement($namedSetterFunction->parameters->[1]->type, $namedSetterFunction->extendedAttributes, $asSetterValue, "jsValue", "propertyValue", "    ", "info.GetIsolate()");

    my $extraArguments = "";
    if ($raisesExceptions || IsIntegerType($type)) {
        $code .= "    ExceptionState exceptionState(info.Holder(), info.GetIsolate());\n";
        if ($raisesExceptions) {
            $extraArguments = ", exceptionState";
        }
    }

    $code .= "    bool result = imp->$methodName(propertyName, propertyValue$extraArguments);\n";
    if ($raisesExceptions) {
        $code .= "    if (exceptionState.throwIfNeeded())\n";
        $code .= "        return;\n";
    }
    $code .= "    if (!result)\n";
    $code .= "        return;\n";
    $code .= "    v8SetReturnValue(info, jsValue);\n";
    $code .= "}\n\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateImplementationIndexedPropertyDeleter
{
    my $interface = shift;
    my $indexedDeleterFunction = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $methodName = GetImplName($indexedDeleterFunction) || "anonymousIndexedDeleter";

    my $raisesExceptions = $indexedDeleterFunction->extendedAttributes->{"RaisesException"};

    my $code = "static void indexedPropertyDeleter(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info)\n";
    $code .= "{\n";
    $code .= "    ${implClassName}* imp = ${v8ClassName}::toNative(info.Holder());\n";
    my $extraArguments = "";
    if ($raisesExceptions) {
        $code .= "    ExceptionState exceptionState(info.Holder(), info.GetIsolate());\n";
        $extraArguments = ", exceptionState";
    }
    $code .= "    DeleteResult result = imp->${methodName}(index$extraArguments);\n";
    if ($raisesExceptions) {
        $code .= "    if (exceptionState.throwIfNeeded())\n";
        $code .= "        return;\n";
    }
    $code .= "    if (result != DeleteUnknownProperty)\n";
    $code .= "        return v8SetReturnValueBool(info, result == DeleteSuccess);\n";
    $code .= "}\n\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateImplementationNamedPropertyDeleter
{
    my $interface = shift;
    my $namedDeleterFunction = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $methodName = GetImplName($namedDeleterFunction) || "anonymousNamedDeleter";

    my $raisesExceptions = $namedDeleterFunction->extendedAttributes->{"RaisesException"};

    my $code = "static void namedPropertyDeleter(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Boolean>& info)\n";
    $code .= "{\n";
    $code .= "    ${implClassName}* imp = ${v8ClassName}::toNative(info.Holder());\n";
    $code .= "    AtomicString propertyName = toCoreAtomicString(name);\n";
    my $extraArguments = "";
    if ($raisesExceptions) {
        $code .= "    ExceptionState exceptionState(info.Holder(), info.GetIsolate());\n";
        $extraArguments .= ", exceptionState";
    }
    $code .= "    DeleteResult result = imp->${methodName}(propertyName$extraArguments);\n";
    if ($raisesExceptions) {
        $code .= "    if (exceptionState.throwIfNeeded())\n";
        $code .= "        return;\n";
    }
    $code .= "    if (result != DeleteUnknownProperty)\n";
    $code .= "        return v8SetReturnValueBool(info, result == DeleteSuccess);\n";
    $code .= "}\n\n";
    $implementation{nameSpaceInternal}->add($code);
}

sub GenerateImplementationNamedPropertyEnumerator
{
    my $interface = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    $implementation{nameSpaceInternal}->add(<<END);
static void namedPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info)
{
    ${implClassName}* imp = ${v8ClassName}::toNative(info.Holder());
    Vector<String> names;
    ExceptionState exceptionState(info.Holder(), info.GetIsolate());
    imp->namedPropertyEnumerator(names, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    v8::Handle<v8::Array> v8names = v8::Array::New(info.GetIsolate(), names.size());
    for (size_t i = 0; i < names.size(); ++i)
        v8names->Set(v8::Integer::New(info.GetIsolate(), i), v8String(info.GetIsolate(), names[i]));
    v8SetReturnValue(info, v8names);
}

END
}

sub GenerateImplementationNamedPropertyQuery
{
    my $interface = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    $implementation{nameSpaceInternal}->add(<<END);
static void namedPropertyQuery(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Integer>& info)
{
    ${implClassName}* imp = ${v8ClassName}::toNative(info.Holder());
    AtomicString propertyName = toCoreAtomicString(name);
    ExceptionState exceptionState(info.Holder(), info.GetIsolate());
    bool result = imp->namedPropertyQuery(propertyName, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    if (!result)
        return;
    v8SetReturnValueInt(info, v8::None);
}

END
}

sub GenerateImplementationLegacyCallAsFunction
{
    my $interface = shift;
    my $code = "";

    my $v8ClassName = GetV8ClassName($interface);

    if (ExtendedAttributeContains($interface->extendedAttributes->{"Custom"}, "LegacyCallAsFunction")) {
        $code .= "    functionTemplate->InstanceTemplate()->SetCallAsFunctionHandler(${v8ClassName}::legacyCallCustom);\n";
    }
    return $code;
}

sub GenerateImplementationMarkAsUndetectable
{
    my $interface = shift;
    my $code = "";

    # Needed for legacy support of document.all
    if ($interface->name eq "HTMLAllCollection")
    {
        $code .= "    functionTemplate->InstanceTemplate()->MarkAsUndetectable();\n";
    }
    return $code;
}

sub GenerateImplementation
{
    my $object = shift;
    my $interface = shift;
    my $interfaceName = $interface->name;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);
    my $nativeType = GetNativeTypeForConversions($interface);

    AddToImplIncludes("RuntimeEnabledFeatures.h");
    AddToImplIncludes("bindings/v8/ExceptionState.h");
    AddToImplIncludes("core/dom/ContextFeatures.h");
    AddToImplIncludes("core/dom/Document.h");
    AddToImplIncludes("platform/TraceEvent.h");
    AddToImplIncludes("wtf/GetPtr.h");
    AddToImplIncludes("wtf/RefPtr.h");

    AddIncludesForType($interfaceName);
    if ($interface->extendedAttributes->{"CheckSecurity"}) {
        AddToImplIncludes("bindings/v8/BindingSecurity.h");
    }

    my $toActiveDOMObject = InheritsExtendedAttribute($interface, "ActiveDOMObject") ? "${v8ClassName}::toActiveDOMObject" : "0";
    my $toEventTarget = InheritsInterface($interface, "EventTarget") ? "${v8ClassName}::toEventTarget" : "0";
    my $visitDOMWrapper = NeedsVisitDOMWrapper($interface) ? "${v8ClassName}::visitDOMWrapper" : "0";
    my $derefObject = "${v8ClassName}::derefObject";

    # Find the super descriptor.
    my $parentClass = "";
    my $parentClassTemplate = "";
    if ($interface->parent) {
        my $parent = $interface->parent;
        $parentClass = "V8" . $parent;
        $parentClassTemplate = $parentClass . "::domTemplate(isolate, currentWorldType)";
    }

    my $parentClassInfo = $parentClass ? "&${parentClass}::wrapperTypeInfo" : "0";
    my $WrapperTypePrototype = $interface->isException ? "WrapperTypeExceptionPrototype" : "WrapperTypeObjectPrototype";

    if (!IsSVGTypeNeedingTearOff($interfaceName)) {
        my $code = <<END;
static void initializeScriptWrappableForInterface(${implClassName}* object)
{
    if (ScriptWrappable::wrapperCanBeStoredInObject(object))
        ScriptWrappable::setTypeInfoInObject(object, &${v8ClassName}::wrapperTypeInfo);
    else
        ASSERT_NOT_REACHED();
}

} // namespace WebCore

// In ScriptWrappable::init, the use of a local function declaration has an issue on Windows:
// the local declaration does not pick up the surrounding namespace. Therefore, we provide this function
// in the global namespace.
// (More info on the MSVC bug here: http://connect.microsoft.com/VisualStudio/feedback/details/664619/the-namespace-of-local-function-declarations-in-c)
END

    if (GetNamespaceForInterface($interface) eq "WebCore") {
        $code .= "void webCoreInitializeScriptWrappableForInterface(WebCore::${implClassName}* object)\n";
    } else {
        $code .= "void webCoreInitializeScriptWrappableForInterface(${implClassName}* object)\n";
    }

    $code .= <<END;
{
    WebCore::initializeScriptWrappableForInterface(object);
}

namespace WebCore {
END
        $implementation{nameSpaceWebCore}->addHeader($code);
    }

    my $code = "const WrapperTypeInfo ${v8ClassName}::wrapperTypeInfo = { gin::kEmbedderBlink, ${v8ClassName}::domTemplate, $derefObject, $toActiveDOMObject, $toEventTarget, ";
    $code .= "$visitDOMWrapper, ${v8ClassName}::installPerContextEnabledMethods, $parentClassInfo, $WrapperTypePrototype, ";
    $code .= IsWillBeGarbageCollectedType($interfaceName) ? "true" : "false";
    $code .=  " };\n";
    $implementation{nameSpaceWebCore}->addHeader($code);

    $implementation{nameSpaceInternal}->add("template <typename T> void V8_USE(T) { }\n\n");

    my $hasConstructors = 0;
    my $hasReplaceable = 0;

    # Generate property accessors for attributes.
    for (my $index = 0; $index < @{$interface->attributes}; $index++) {
        my $attribute = @{$interface->attributes}[$index];
        my $attrType = $attribute->type;
        my $attrExt = $attribute->extendedAttributes;

        # Generate special code for the constructor attributes.
        if ($attrType =~ /Constructor$/) {
            if (!HasCustomGetter($attrExt)) {
                $hasConstructors = 1;
            }
            next;
        }

        AddIncludesForType($attrType);

        if ($attrType eq "EventHandler" && $interfaceName eq "Window") {
            $attrExt->{"OnPrototype"} = 1;
        }

        if ($attrType eq "SerializedScriptValue") {
            AddToImplIncludes("bindings/v8/SerializedScriptValue.h");
        }

        my $isReplaceable = 0;
        if (!HasCustomSetter($attribute) && !$attrExt->{"PutForwards"} && $attrExt->{"Replaceable"}) {
            $isReplaceable = 1;
            $hasReplaceable = 1;
        }

        my $exposeJSAccessors = $attrExt->{"ExposeJSAccessors"};
        my @worldSuffixes = ("");
        if ($attrExt->{"PerWorldBindings"}) {
            push(@worldSuffixes, "ForMainWorld");
        }
        foreach my $worldSuffix (@worldSuffixes) {
            GenerateNormalAttributeGetter($attribute, $interface, $worldSuffix, $exposeJSAccessors);
            GenerateNormalAttributeGetterCallback($attribute, $interface, $worldSuffix, $exposeJSAccessors);
            if (!$isReplaceable && !IsReadonly($attribute)) {
                GenerateNormalAttributeSetter($attribute, $interface, $worldSuffix, $exposeJSAccessors);
                GenerateNormalAttributeSetterCallback($attribute, $interface, $worldSuffix, $exposeJSAccessors);
            }
        }
    }

    if ($hasConstructors) {
        GenerateConstructorGetter($interface);
    }

    if ($hasConstructors || $hasReplaceable) {
        GenerateReplaceableAttributeSetter($interface);
        GenerateReplaceableAttributeSetterCallback($interface);
    }

    if (NeedsVisitDOMWrapper($interface)) {
        GenerateVisitDOMWrapper($interface);
    }

    if ($interface->extendedAttributes->{"CheckSecurity"} && $interface->name ne "Window") {
        GenerateSecurityCheckFunctions($interface);
    }

    my @perContextEnabledFunctions;
    my @normalFunctions;
    my $needsDomainSafeFunctionSetter = 0;
    # Generate methods for functions.
    foreach my $function (@{$interface->functions}) {
        next if $function->name eq "";  # Skip anonymous special operations
        my @worldSuffixes = ("");
        if ($function->extendedAttributes->{"PerWorldBindings"}) {
            push(@worldSuffixes, "ForMainWorld");
        }
        foreach my $worldSuffix (@worldSuffixes) {
            GenerateFunction($function, $interface, $worldSuffix);
            if ($function->{overloadIndex} == @{$function->{overloads}}) {
                if ($function->{overloadIndex} > 1) {
                    GenerateOverloadedFunction($function, $interface, $worldSuffix);
                }
                GenerateFunctionCallback($function, $interface, $worldSuffix);
            }

            # If the function does not need domain security check, we need to
            # generate an access getter that returns different function objects
            # for different calling context.
            if ($interface->extendedAttributes->{"CheckSecurity"} && $function->extendedAttributes->{"DoNotCheckSecurity"} && (!HasCustomMethod($function->extendedAttributes) || $function->{overloadIndex} == 1)) {
                GenerateDomainSafeFunctionGetter($function, $interface, $worldSuffix);
                if (!$function->extendedAttributes->{"ReadOnly"}) {
                    $needsDomainSafeFunctionSetter = 1;
                }
            }
        }

        # Separate out functions that are enabled per context so we can process them specially.
        if ($function->extendedAttributes->{"PerContextEnabled"}) {
            push(@perContextEnabledFunctions, $function);
        } else {
            push(@normalFunctions, $function);
        }
    }

    if ($needsDomainSafeFunctionSetter) {
        GenerateDomainSafeFunctionSetter($interface);
    }

    # Attributes
    my $attributes = $interface->attributes;

    # For the Window interface we partition the attributes into the
    # ones that disallows shadowing and the rest.
    my @disallowsShadowing;
    # Also separate out attributes that are enabled at runtime so we can process them specially.
    my @runtimeEnabledAttributes;
    my @perContextEnabledAttributes;
    my @normalAttributes;
    my @normalAccessors;
    my @staticAttributes;
    foreach my $attribute (@$attributes) {

        if ($attribute->isStatic) {
            push(@staticAttributes, $attribute);
        } elsif ($interfaceName eq "Window" && $attribute->extendedAttributes->{"Unforgeable"}) {
            push(@disallowsShadowing, $attribute);
        } elsif ($attribute->extendedAttributes->{"PerContextEnabled"}) {
            push(@perContextEnabledAttributes, $attribute);
        } elsif ($attribute->extendedAttributes->{"RuntimeEnabled"}) {
            push(@runtimeEnabledAttributes, $attribute);
        } elsif ($attribute->extendedAttributes->{"ExposeJSAccessors"}) {
            push(@normalAccessors, $attribute);
        } else {
            push(@normalAttributes, $attribute);
        }
    }
    AddToImplIncludes("bindings/v8/V8DOMConfiguration.h");
    # Put the attributes that disallow shadowing on the shadow object.
    if (@disallowsShadowing) {
        my $code = "";
        $code .= "static const V8DOMConfiguration::AttributeConfiguration shadowAttributes[] = {\n";
        $code .= GenerateAttributeConfigurationArray($interface, \@disallowsShadowing);
        $code .= "};\n\n";
        $implementation{nameSpaceWebCore}->add($code);
    }

    my $hasAttributes = 0;
    if (@normalAttributes) {
        $hasAttributes = 1;
        my $code = "";
        $code .= "static const V8DOMConfiguration::AttributeConfiguration ${v8ClassName}Attributes[] = {\n";
        $code .= GenerateAttributeConfigurationArray($interface, \@normalAttributes);
        $code .= "};\n\n";
        $implementation{nameSpaceWebCore}->add($code);
    }

    my $hasAccessors = 0;
    if (@normalAccessors) {
        $hasAccessors = 1;
        my $code = "";
        $code .= "static const V8DOMConfiguration::AccessorConfiguration ${v8ClassName}Accessors[] = {\n";
        $code .= GenerateAttributeConfigurationArray($interface, \@normalAccessors, "accessor");
        $code .= "};\n\n";
        $implementation{nameSpaceWebCore}->add($code);
    }

    # Setup table of standard callback functions
    my $hasFunctions = 0;
    $code = "";
    foreach my $function (@normalFunctions) {
        # Only one table entry is needed for overloaded functions:
        next if $function->{overloadIndex} > 1;
        # Don't put any nonstandard functions into this table:
        next if !IsStandardFunction($interface, $function);
        next if $function->name eq "";
        if (!$hasFunctions) {
            $hasFunctions = 1;
            $code .= "static const V8DOMConfiguration::MethodConfiguration ${v8ClassName}Methods[] = {\n";
        }
        my $name = $function->name;
        my $methodForMainWorld = "0";
        if ($function->extendedAttributes->{"PerWorldBindings"}) {
            $methodForMainWorld = "${implClassName}V8Internal::${name}MethodCallbackForMainWorld";
        }
        my $functionLength = GetFunctionLength($function);
        my $conditionalString = GenerateConditionalString($function);
        $code .= "#if ${conditionalString}\n" if $conditionalString;
        $code .= <<END;
    {"$name", ${implClassName}V8Internal::${name}MethodCallback, ${methodForMainWorld}, ${functionLength}},
END
        $code .= "#endif // ${conditionalString}\n" if $conditionalString;
    }
    $code .= "};\n\n"  if $hasFunctions;
    $implementation{nameSpaceWebCore}->add($code);

    my $hasConstants = 0;
    if (@{$interface->constants}) {
        $hasConstants = 1;
    }

    if (!HasCustomConstructor($interface)) {
        if ($interface->extendedAttributes->{"NamedConstructor"}) {
            GenerateNamedConstructor(@{$interface->constructors}[0], $interface);
        } elsif ($interface->extendedAttributes->{"Constructor"}) {
            GenerateConstructor($interface);
        } elsif ($interface->extendedAttributes->{"EventConstructor"}) {
            GenerateEventConstructor($interface);
        }
    }
    if (IsConstructable($interface)) {
        GenerateConstructorCallback($interface);
    }

    my $accessCheck = "";
    if ($interface->extendedAttributes->{"CheckSecurity"} && $interfaceName ne "Window") {
        $accessCheck = "instanceTemplate->SetAccessCheckCallbacks(${implClassName}V8Internal::namedSecurityCheck, ${implClassName}V8Internal::indexedSecurityCheck, v8::External::New(isolate, const_cast<WrapperTypeInfo*>(&${v8ClassName}::wrapperTypeInfo)));";
    }

    # For the Window interface, generate the shadow object template
    # configuration method.
    if ($interfaceName eq "Window") {
        $implementation{nameSpaceWebCore}->add(<<END);
static void configureShadowObjectTemplate(v8::Handle<v8::ObjectTemplate> templ, v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    V8DOMConfiguration::installAttributes(templ, v8::Handle<v8::ObjectTemplate>(), shadowAttributes, WTF_ARRAY_LENGTH(shadowAttributes), isolate, currentWorldType);

    // Install a security handler with V8.
    templ->SetAccessCheckCallbacks(V8Window::namedSecurityCheckCustom, V8Window::indexedSecurityCheckCustom, v8::External::New(isolate, const_cast<WrapperTypeInfo*>(&V8Window::wrapperTypeInfo)));
    templ->SetInternalFieldCount(V8Window::internalFieldCount);
}

END
    }

    if (!$parentClassTemplate) {
        $parentClassTemplate = "v8::Local<v8::FunctionTemplate>()";
    }

    # Generate the template configuration method
    $code =  <<END;
static void configure${v8ClassName}Template(v8::Handle<v8::FunctionTemplate> functionTemplate, v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    functionTemplate->ReadOnlyPrototype();

    v8::Local<v8::Signature> defaultSignature;
END

    # Define constants, attributes, accessors and operations.
    my $runtimeEnabledIndent = "";
    if ($interface->extendedAttributes->{"RuntimeEnabled"}) {
        my $runtimeEnabledFunction = GetRuntimeEnabledFunctionName($interface);
        $runtimeEnabledIndent = "    ";
        $code .= <<END;
    if (!${runtimeEnabledFunction}())
        defaultSignature = V8DOMConfiguration::installDOMClassTemplate(functionTemplate, \"\", $parentClassTemplate, ${v8ClassName}::internalFieldCount, 0, 0, 0, 0, 0, 0, isolate, currentWorldType);
    else
END
    }
    $code .= $runtimeEnabledIndent . "    defaultSignature = V8DOMConfiguration::installDOMClassTemplate(functionTemplate, \"${interfaceName}\", $parentClassTemplate, ${v8ClassName}::internalFieldCount,\n";
    $code .= $runtimeEnabledIndent . "        " . ($hasAttributes ? "${v8ClassName}Attributes, WTF_ARRAY_LENGTH(${v8ClassName}Attributes),\n" : "0, 0,\n");
    $code .= $runtimeEnabledIndent . "        " . ($hasAccessors ? "${v8ClassName}Accessors, WTF_ARRAY_LENGTH(${v8ClassName}Accessors),\n" : "0, 0,\n");
    $code .= $runtimeEnabledIndent . "        " . ($hasFunctions ? "${v8ClassName}Methods, WTF_ARRAY_LENGTH(${v8ClassName}Methods),\n" : "0, 0,\n");
    $code .= $runtimeEnabledIndent . "        isolate, currentWorldType);\n";

    if (IsConstructable($interface)) {
        $code .= "    functionTemplate->SetCallHandler(${v8ClassName}::constructorCallback);\n";
        my $interfaceLength = GetInterfaceLength($interface);
        $code .= "    functionTemplate->SetLength(${interfaceLength});\n";
    }

        $code .=  <<END;
    v8::Local<v8::ObjectTemplate> ALLOW_UNUSED instanceTemplate = functionTemplate->InstanceTemplate();
    v8::Local<v8::ObjectTemplate> ALLOW_UNUSED prototypeTemplate = functionTemplate->PrototypeTemplate();
END

    if ($accessCheck) {
        $code .=  "    $accessCheck\n";
    }

    # Define runtime enabled attributes.
    foreach my $runtimeEnabledAttribute (@runtimeEnabledAttributes) {
        my $runtimeEnabledFunction = GetRuntimeEnabledFunctionName($runtimeEnabledAttribute);
        my $conditionalString = GenerateConditionalString($runtimeEnabledAttribute);
        $code .= "#if ${conditionalString}\n" if $conditionalString;
        $code .= "    if (${runtimeEnabledFunction}()) {\n";
        $code .= "        static const V8DOMConfiguration::AttributeConfiguration attributeConfiguration =\\\n";
        $code .= GenerateAttributeConfiguration($interface, $runtimeEnabledAttribute, ";", "    ");
        $code .= <<END;
        V8DOMConfiguration::installAttribute(instanceTemplate, prototypeTemplate, attributeConfiguration, isolate, currentWorldType);
    }
END
        $code .= "#endif // ${conditionalString}\n" if $conditionalString;
    }

    my @runtimeEnabledConstants;
    if ($hasConstants) {
        # Define constants.
        $code .= "    static const V8DOMConfiguration::ConstantConfiguration ${v8ClassName}Constants[] = {\n";
        foreach my $constant (@{$interface->constants}) {
            my $name = $constant->name;
            my $value = $constant->value;
            my $attrExt = $constant->extendedAttributes;
            my $implementedBy = $attrExt->{"ImplementedBy"};
            if ($implementedBy) {
                my $implementedByImplName = GetImplNameFromImplementedBy($implementedBy);
                AddToImplIncludes(HeaderFilesForInterface($implementedBy, $implementedByImplName));
            }
            if ($attrExt->{"RuntimeEnabled"}) {
                push(@runtimeEnabledConstants, $constant);
            } else {
                $code .= <<END;
        {"${name}", $value},
END
            }
        }
        $code .= "    };\n";
        $code .= <<END;
    V8DOMConfiguration::installConstants(functionTemplate, prototypeTemplate, ${v8ClassName}Constants, WTF_ARRAY_LENGTH(${v8ClassName}Constants), isolate);
END
        # Define runtime enabled constants.
        foreach my $runtimeEnabledConstant (@runtimeEnabledConstants) {
            my $runtimeEnabledFunction = GetRuntimeEnabledFunctionName($runtimeEnabledConstant);
            my $name = $runtimeEnabledConstant->name;
            my $value = $runtimeEnabledConstant->value;
            $code .= "    if (${runtimeEnabledFunction}()) {\n";
            $code .= <<END;
        static const V8DOMConfiguration::ConstantConfiguration constantConfiguration = {"${name}", static_cast<signed int>(${value})};
        V8DOMConfiguration::installConstants(functionTemplate, prototypeTemplate, &constantConfiguration, 1, isolate);
END
            $code .= "    }\n";
        }
        $code .= join "", GenerateCompileTimeCheckForEnumsIfNeeded($interface);
    }

    $code .= GenerateImplementationIndexedPropertyAccessors($interface);
    $code .= GenerateImplementationNamedPropertyAccessors($interface);
    $code .= GenerateImplementationLegacyCallAsFunction($interface);
    $code .= GenerateImplementationMarkAsUndetectable($interface);

    # Define operations.
    my $total_functions = 0;
    foreach my $function (@normalFunctions) {
        # Only one accessor is needed for overloaded operations.
        next if $function->{overloadIndex} > 1;
        next if $function->name eq "";

        $total_functions++;
        next if IsStandardFunction($interface, $function);
        $code .= GenerateNonStandardFunction($interface, $function);
    }

    # Define static attributes.
    foreach my $attribute (@staticAttributes) {
        $code .= GenerateStaticAttribute($interface, $attribute);
    }

    # Special cases
    if ($interfaceName eq "Window") {
        $code .= <<END;

    prototypeTemplate->SetInternalFieldCount(V8Window::internalFieldCount);
    functionTemplate->SetHiddenPrototype(true);
    instanceTemplate->SetInternalFieldCount(V8Window::internalFieldCount);
    // Set access check callbacks, but turned off initially.
    // When a context is detached from a frame, turn on the access check.
    // Turning on checks also invalidates inline caches of the object.
    instanceTemplate->SetAccessCheckCallbacks(V8Window::namedSecurityCheckCustom, V8Window::indexedSecurityCheckCustom, v8::External::New(isolate, const_cast<WrapperTypeInfo*>(&V8Window::wrapperTypeInfo)), false);
END
    }
    if ($interfaceName eq "HTMLDocument" or $interfaceName eq "DedicatedWorkerGlobalScope" or $interfaceName eq "SharedWorkerGlobalScope" or $interfaceName eq "ServiceWorkerGlobalScope") {
        $code .= <<END;
    functionTemplate->SetHiddenPrototype(true);
END
    }

    $code .= <<END;

    // Custom toString template
    functionTemplate->Set(v8AtomicString(isolate, "toString"), V8PerIsolateData::current()->toStringTemplate());
}

END
    $implementation{nameSpaceWebCore}->add($code);

    AddToImplIncludes("bindings/v8/V8ObjectConstructor.h");
    $implementation{nameSpaceWebCore}->add(<<END);
v8::Handle<v8::FunctionTemplate> ${v8ClassName}::domTemplate(v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    V8PerIsolateData::TemplateMap::iterator result = data->templateMap(currentWorldType).find(&wrapperTypeInfo);
    if (result != data->templateMap(currentWorldType).end())
        return result->value.newLocal(isolate);

    TRACE_EVENT_SCOPED_SAMPLING_STATE("Blink", "BuildDOMTemplate");
    v8::EscapableHandleScope handleScope(isolate);
    v8::Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(isolate, V8ObjectConstructor::isValidConstructorMode);
    configure${v8ClassName}Template(templ, isolate, currentWorldType);
    data->templateMap(currentWorldType).add(&wrapperTypeInfo, UnsafePersistent<v8::FunctionTemplate>(isolate, templ));
    return handleScope.Escape(templ);
}

bool ${v8ClassName}::hasInstance(v8::Handle<v8::Value> jsValue, v8::Isolate* isolate)
{
    return V8PerIsolateData::from(isolate)->hasInstanceInMainWorld(&wrapperTypeInfo, jsValue)
        || V8PerIsolateData::from(isolate)->hasInstanceInNonMainWorld(&wrapperTypeInfo, jsValue);
}

${nativeType}* ${v8ClassName}::toNativeWithTypeCheck(v8::Isolate* isolate, v8::Handle<v8::Value> value)
{
    return hasInstance(value, isolate) ? fromInternalPointer(v8::Handle<v8::Object>::Cast(value)->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex)) : 0;
}

END

    if (@perContextEnabledAttributes) {
        my $code = "";
        $code .= <<END;
void ${v8ClassName}::installPerContextEnabledProperties(v8::Handle<v8::Object> instanceTemplate, ${nativeType}* impl, v8::Isolate* isolate)
{
    v8::Local<v8::Object> prototypeTemplate = v8::Local<v8::Object>::Cast(instanceTemplate->GetPrototype());
END

        # Define per-context enabled attributes.
        foreach my $perContextEnabledAttribute (@perContextEnabledAttributes) {
            my $contextEnabledFunction = GetContextEnabledFunctionName($perContextEnabledAttribute);
            $code .= "    if (${contextEnabledFunction}(impl->document())) {\n";

            $code .= "        static const V8DOMConfiguration::AttributeConfiguration attributeConfiguration =\\\n";
            $code .= GenerateAttributeConfiguration($interface, $perContextEnabledAttribute, ";", "    ");
            $code .= <<END;
        V8DOMConfiguration::installAttribute(instanceTemplate, prototypeTemplate, attributeConfiguration, isolate);
END
            $code .= "    }\n";
        }
        $code .= <<END;
}

END
        $implementation{nameSpaceWebCore}->add($code);
    }

    if (@perContextEnabledFunctions) {
        my $code = "";
        $code .= <<END;
void ${v8ClassName}::installPerContextEnabledMethods(v8::Handle<v8::Object> prototypeTemplate, v8::Isolate* isolate)
{
END
        # Define per-context enabled operations.
        $code .=  <<END;
    v8::Local<v8::Signature> defaultSignature = v8::Signature::New(isolate, domTemplate(isolate, worldType(isolate)));

    ExecutionContext* context = toExecutionContext(prototypeTemplate->CreationContext());
END

        foreach my $perContextEnabledFunction (@perContextEnabledFunctions) {
            my $contextEnabledFunction = GetContextEnabledFunctionName($perContextEnabledFunction);
            my $functionLength = GetFunctionLength($perContextEnabledFunction);
            my $conditionalString = GenerateConditionalString($perContextEnabledFunction);
            $code .= "\n#if ${conditionalString}\n" if $conditionalString;
            $code .= "    if (context && context->isDocument() && ${contextEnabledFunction}(toDocument(context)))\n";
            my $name = $perContextEnabledFunction->name;
            $code .= <<END;
        prototypeTemplate->Set(v8AtomicString(isolate, "$name"), v8::FunctionTemplate::New(isolate, ${implClassName}V8Internal::${name}MethodCallback, v8Undefined(), defaultSignature, $functionLength)->GetFunction());
END
            $code .= "#endif // ${conditionalString}\n" if $conditionalString;
        }

        $code .= <<END;
}

END
        $implementation{nameSpaceWebCore}->add($code);
    }

    if (InheritsExtendedAttribute($interface, "ActiveDOMObject")) {
        $implementation{nameSpaceWebCore}->add(<<END);
ActiveDOMObject* ${v8ClassName}::toActiveDOMObject(v8::Handle<v8::Object> wrapper)
{
    return toNative(wrapper);
}

END
    }

    if (InheritsInterface($interface, "EventTarget")) {
        $implementation{nameSpaceWebCore}->add(<<END);
EventTarget* ${v8ClassName}::toEventTarget(v8::Handle<v8::Object> object)
{
    return toNative(object);
}

END
    }

    if ($interfaceName eq "Window") {
        $implementation{nameSpaceWebCore}->add(<<END);
v8::Handle<v8::ObjectTemplate> V8Window::getShadowObjectTemplate(v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    if (currentWorldType == MainWorld) {
        DEFINE_STATIC_LOCAL(v8::Persistent<v8::ObjectTemplate>, V8WindowShadowObjectCacheForMainWorld, ());
        if (V8WindowShadowObjectCacheForMainWorld.IsEmpty()) {
            TRACE_EVENT_SCOPED_SAMPLING_STATE("Blink", "BuildDOMTemplate");
            v8::Handle<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
            configureShadowObjectTemplate(templ, isolate, currentWorldType);
            V8WindowShadowObjectCacheForMainWorld.Reset(isolate, templ);
            return templ;
        }
        return v8::Local<v8::ObjectTemplate>::New(isolate, V8WindowShadowObjectCacheForMainWorld);
    } else {
        DEFINE_STATIC_LOCAL(v8::Persistent<v8::ObjectTemplate>, V8WindowShadowObjectCacheForNonMainWorld, ());
        if (V8WindowShadowObjectCacheForNonMainWorld.IsEmpty()) {
            TRACE_EVENT_SCOPED_SAMPLING_STATE("Blink", "BuildDOMTemplate");
            v8::Handle<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
            configureShadowObjectTemplate(templ, isolate, currentWorldType);
            V8WindowShadowObjectCacheForNonMainWorld.Reset(isolate, templ);
            return templ;
        }
        return v8::Local<v8::ObjectTemplate>::New(isolate, V8WindowShadowObjectCacheForNonMainWorld);
    }
}

END
    }

    GenerateSpecialWrap($interface, $v8ClassName);
    GenerateToV8Converters($interface, $v8ClassName);

    $implementation{nameSpaceWebCore}->add("void ${v8ClassName}::derefObject(void* object)\n");
    $implementation{nameSpaceWebCore}->add("{\n");
    if (IsWillBeGarbageCollectedType($interface->name)) {
        $implementation{nameSpaceWebCore}->add("#if !ENABLE(OILPAN)\n");
    }
    $implementation{nameSpaceWebCore}->add("    fromInternalPointer(object)->deref();\n");
    if (IsWillBeGarbageCollectedType($interface->name)) {
        $implementation{nameSpaceWebCore}->add("#endif // !ENABLE(OILPAN)\n");
    }
    $implementation{nameSpaceWebCore}->add("}\n\n");

    $implementation{nameSpaceWebCore}->add(<<END);
template<>
v8::Handle<v8::Value> toV8NoInline(${nativeType}* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8(impl, creationContext, isolate);
}

END
}

sub GenerateHeaderContentHeader
{
    my $interface = shift;
    my $v8ClassName = GetV8ClassName($interface);
    my $conditionalString = GenerateConditionalString($interface);

    my @headerContentHeader = split("\r", $licenseHeader);

    push(@headerContentHeader, "#ifndef ${v8ClassName}" . "_h\n");
    push(@headerContentHeader, "#define ${v8ClassName}" . "_h\n\n");
    push(@headerContentHeader, "#if ${conditionalString}\n") if $conditionalString;
    return join "", @headerContentHeader;
}

sub GenerateCallbackHeader
{
    my $object = shift;
    my $interface = shift;

    my $interfaceName = $interface->name;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    my @includes = ();
    push(@includes, "bindings/v8/ActiveDOMCallback.h");
    push(@includes, "bindings/v8/DOMWrapperWorld.h");
    push(@includes, "bindings/v8/ScopedPersistent.h");
    push(@includes, HeaderFilesForInterface($interfaceName, $implClassName));
    for my $include (sort @includes) {
        $header{includes}->add("#include \"$include\"\n");
    }
    $header{nameSpaceWebCore}->addHeader("\nclass ExecutionContext;\n");
    $header{class}->addHeader("class $v8ClassName FINAL : public $implClassName, public ActiveDOMCallback {");
    $header{class}->addFooter("};\n");

    $header{classPublic}->add(<<END);
    static PassOwnPtr<${v8ClassName}> create(v8::Handle<v8::Function> callback, ExecutionContext* context)
    {
        ASSERT(context);
        return adoptPtr(new ${v8ClassName}(callback, context));
    }

    virtual ~${v8ClassName}();

END

    # Functions
    my $numFunctions = @{$interface->functions};
    if ($numFunctions > 0) {
        foreach my $function (@{$interface->functions}) {
            my $code = "    virtual " . GetNativeTypeForCallbacks($function->type) . " " . $function->name . "(";

            my @args = ();
            if (ExtendedAttributeContains($function->extendedAttributes->{"CallWith"}, "ThisValue")) {
                push(@args, GetNativeType("any") . " thisValue");
            }
            my @params = @{$function->parameters};
            foreach my $param (@params) {
                push(@args, GetNativeTypeForCallbacks($param->type) . " " . $param->name);
            }
            $code .= join(", ", @args);
            $code .= ") OVERRIDE;\n";
            $header{classPublic}->add($code);
        }
    }

    $header{classPrivate}->add(<<END);
    ${v8ClassName}(v8::Handle<v8::Function>, ExecutionContext*);

    v8::Isolate* m_isolate;
    ScopedPersistent<v8::Function> m_callback;
    RefPtr<DOMWrapperWorld> m_world;
END
}

sub GenerateCallbackImplementation
{
    my $object = shift;
    my $interface = shift;
    my $v8ClassName = GetV8ClassName($interface);

    AddToImplIncludes("core/dom/ExecutionContext.h");
    AddToImplIncludes("bindings/v8/V8Binding.h");
    AddToImplIncludes("bindings/v8/V8Callback.h");
    AddToImplIncludes("wtf/Assertions.h");
    AddToImplIncludes("wtf/GetPtr.h");
    AddToImplIncludes("wtf/RefPtr.h");

    $implementation{nameSpaceWebCore}->add(<<END);
${v8ClassName}::${v8ClassName}(v8::Handle<v8::Function> callback, ExecutionContext* context)
    : ActiveDOMCallback(context)
    , m_isolate(toIsolate(context))
    , m_callback(m_isolate, callback)
    , m_world(DOMWrapperWorld::current(m_isolate))
{
}

END

    $implementation{nameSpaceWebCore}->add(<<END);
${v8ClassName}::~${v8ClassName}()
{
}

END

    # Functions
    my $numFunctions = @{$interface->functions};
    if ($numFunctions > 0) {
        foreach my $function (@{$interface->functions}) {
            my $code = "";
            my @params = @{$function->parameters};

            # Add includes for types even if custom
            AddIncludesForType($function->type);
            foreach my $param (@params) {
                AddIncludesForType($param->type);
            }
            next if $function->extendedAttributes->{"Custom"};

            die "We only support callbacks that return boolean or void values.\n" unless ($function->type eq "boolean" || $function->type eq "void");
            my $defaultReturn = $function->type eq "boolean" ? " true" : "";
            $code .= GetNativeTypeForCallbacks($function->type) . " ${v8ClassName}::" . $function->name . "(";
            my $callWithThisValue = ExtendedAttributeContains($function->extendedAttributes->{"CallWith"}, "ThisValue");

            my @args = ();
            if ($callWithThisValue) {
                push(@args, GetNativeTypeForCallbacks("any") . " thisValue");
            }
            foreach my $param (@params) {
                push(@args, GetNativeTypeForCallbacks($param->type) . " " . $param->name);
            }
            $code .= join(", ", @args);

            $code .= ")\n";
            $code .= "{\n";
            $code .= "    if (!canInvokeCallback())\n";
            $code .= "        return${defaultReturn};\n\n";
            $code .= "    v8::HandleScope handleScope(m_isolate);\n\n";
            $code .= "    v8::Handle<v8::Context> v8Context = toV8Context(executionContext(), m_world.get());\n";
            $code .= "    if (v8Context.IsEmpty())\n";
            $code .= "        return${defaultReturn};\n\n";
            $code .= "    v8::Context::Scope scope(v8Context);\n";

            my $thisObjectHandle = "";
            if ($callWithThisValue) {
                $code .= "    v8::Handle<v8::Value> thisHandle = thisValue.v8Value();\n";
                $code .= "    if (thisHandle.IsEmpty()) {\n";
                $code .= "        if (!isScriptControllerTerminating())\n";
                $code .= "            CRASH();\n";
                $code .= "        return${defaultReturn};\n";
                $code .= "    }\n";
                $code .= "    ASSERT(thisHandle->IsObject());\n";
                $thisObjectHandle = "v8::Handle<v8::Object>::Cast(thisHandle), ";
            }
            @args = ();
            foreach my $param (@params) {
                my $paramName = $param->name;
                $code .= NativeToJSValue($param->type, $param->extendedAttributes, $paramName, "    ", "v8::Handle<v8::Value> ${paramName}Handle =", "m_isolate", "") . "\n";
                $code .= "    if (${paramName}Handle.IsEmpty()) {\n";
                $code .= "        if (!isScriptControllerTerminating())\n";
                $code .= "            CRASH();\n";
                $code .= "        return${defaultReturn};\n";
                $code .= "    }\n";
                push(@args, "${paramName}Handle");
            }

            if (scalar(@args) > 0) {
                $code .= "    v8::Handle<v8::Value> argv[] = { ";
                $code .= join(", ", @args);
                $code .= " };\n\n";
            } else {
                $code .= "    v8::Handle<v8::Value> *argv = 0;\n\n";
            }
            $code .= "    ";
            if ($function->type eq "boolean") {
                $code .= "return ";
            }
            $code .= "invokeCallback(m_callback.newLocal(m_isolate), ${thisObjectHandle}" . scalar(@args) . ", argv, executionContext(), m_isolate);\n";
            $code .= "}\n\n";
            $implementation{nameSpaceWebCore}->add($code);
        }
    }
}

sub GenerateSpecialWrap
{
    my $interface = shift;
    my $v8ClassName = shift;
    my $nativeType = GetNativeTypeForConversions($interface);

    my $specialWrap = $interface->extendedAttributes->{"SpecialWrapFor"};
    my $isDocument = InheritsInterface($interface, "Document");
    if (!$specialWrap && !$isDocument) {
        return;
    }

    my $code = "";
    $code .= <<END;
v8::Handle<v8::Object> wrap(${nativeType}* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
END
    if ($specialWrap) {
        foreach my $type (split(/\s*\|\s*/, $specialWrap)) {
            AddToImplIncludes("V8${type}.h");
            $code .= <<END;
    if (impl->is${type}())
        return wrap(to${type}(impl), creationContext, isolate);
END
        }
    }
    $code .= <<END;
    v8::Handle<v8::Object> wrapper = ${v8ClassName}::createWrapper(impl, creationContext, isolate);
END
    if ($isDocument) {
        AddToImplIncludes("bindings/v8/ScriptController.h");
        AddToImplIncludes("bindings/v8/V8WindowShell.h");
        $code .= <<END;
    if (wrapper.IsEmpty())
        return wrapper;
    DOMWrapperWorld* world = DOMWrapperWorld::current(isolate);
    if (world->isMainWorld()) {
        if (Frame* frame = impl->frame())
            frame->script().windowShell(world)->updateDocumentWrapper(wrapper);
    }
END
    }
    $code .= <<END;
    return wrapper;
}

END
    $implementation{nameSpaceWebCore}->add($code);
}

sub GenerateToV8Converters
{
    my $interface = shift;
    my $v8ClassName = shift;
    my $interfaceName = $interface->name;

    if (ExtendedAttributeContains($interface->extendedAttributes->{"Custom"}, "ToV8")) {
        return;
    }

    my $passRefPtrType = GetPassRefPtrType($interface);

    # FIXME: Do we really need to treat /SVG/ as dependent DOM objects?
    my $wrapperConfiguration = "WrapperConfiguration::Independent";
    if (InheritsExtendedAttribute($interface, "ActiveDOMObject")
        || InheritsExtendedAttribute($interface, "DependentLifetime")
        || NeedsVisitDOMWrapper($interface)
        || $v8ClassName =~ /SVG/) {
        $wrapperConfiguration = "WrapperConfiguration::Dependent";
    }

    my $code = "";
    $code .= <<END;
v8::Handle<v8::Object> ${v8ClassName}::createWrapper(${passRefPtrType} impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    ASSERT(!DOMDataStore::containsWrapper<${v8ClassName}>(impl.get(), isolate));
    if (ScriptWrappable::wrapperCanBeStoredInObject(impl.get())) {
        const WrapperTypeInfo* actualInfo = ScriptWrappable::getTypeInfoFromObject(impl.get());
        // Might be a XXXConstructor::wrapperTypeInfo instead of an XXX::wrapperTypeInfo. These will both have
        // the same object de-ref functions, though, so use that as the basis of the check.
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(actualInfo->derefObjectFunction == wrapperTypeInfo.derefObjectFunction);
    }

END

    if (InheritsInterface($interface, "Document")) {
        AddToImplIncludes("bindings/v8/ScriptController.h");
        AddToImplIncludes("core/frame/Frame.h");
        $code .= <<END;
    if (Frame* frame = impl->frame()) {
        if (frame->script().initializeMainWorld()) {
            // initializeMainWorld may have created a wrapper for the object, retry from the start.
            v8::Handle<v8::Object> wrapper = DOMDataStore::getWrapper<${v8ClassName}>(impl.get(), isolate);
            if (!wrapper.IsEmpty())
                return wrapper;
        }
    }
END
    }

    $code .= <<END;
    v8::Handle<v8::Object> wrapper = V8DOMWrapper::createWrapper(creationContext, &wrapperTypeInfo, toInternalPointer(impl.get()), isolate);
    if (UNLIKELY(wrapper.IsEmpty()))
        return wrapper;

END
    if (InheritsInterface($interface, "AudioBuffer")) {
      AddToImplIncludes("modules/webaudio/AudioBuffer.h");
      $code .= <<END;
    for (unsigned i = 0, n = impl->numberOfChannels(); i < n; i++) {
        Float32Array* channelData = impl->getChannelData(i);
        channelData->buffer()->setDeallocationObserver(V8ArrayBufferDeallocationObserver::instanceTemplate());
    }
END
    }


    $code .= <<END;
    installPerContextEnabledProperties(wrapper, impl.get(), isolate);
    V8DOMWrapper::associateObjectWithWrapper<$v8ClassName>(impl, &wrapperTypeInfo, wrapper, isolate, $wrapperConfiguration);
    return wrapper;
}

END
    $implementation{nameSpaceWebCore}->add($code);
}

sub GenerateSecurityCheckFunctions
{
    my $interface = shift;
    my $implClassName = GetImplName($interface);
    my $v8ClassName = GetV8ClassName($interface);

    AddToImplIncludes("bindings/v8/BindingSecurity.h");
    $implementation{nameSpaceInternal}->add(<<END);
bool indexedSecurityCheck(v8::Local<v8::Object> host, uint32_t index, v8::AccessType type, v8::Local<v8::Value>)
{
    $implClassName* imp =  ${v8ClassName}::toNative(host);
    return BindingSecurity::shouldAllowAccessToFrame(v8::Isolate::GetCurrent(), imp->frame(), DoNotReportSecurityError);
}

END
    $implementation{nameSpaceInternal}->add(<<END);
bool namedSecurityCheck(v8::Local<v8::Object> host, v8::Local<v8::Value> key, v8::AccessType type, v8::Local<v8::Value>)
{
    $implClassName* imp =  ${v8ClassName}::toNative(host);
    return BindingSecurity::shouldAllowAccessToFrame(v8::Isolate::GetCurrent(), imp->frame(), DoNotReportSecurityError);
}

END
}

sub GetNativeTypeForConversions
{
    my $interface = shift;
    my $implClassName = GetImplName($interface);
    $implClassName = GetSVGTypeNeedingTearOff($interface->name) if IsSVGTypeNeedingTearOff($interface->name);
    return $implClassName;
}

sub GetNamespaceForInterface
{
    my $interface = shift;
    return "WTF" if IsTypedArrayType($interface->name);
    return "WebCore";
}

sub GenerateFunctionCallString
{
    my $function = shift;
    my $numberOfParameters = shift;
    my $indent = shift;
    my $interface = shift;
    my $forMainWorldSuffix = shift;
    my $hasExceptionState = shift;
    my %replacements = @_;

    my $interfaceName = $interface->name;
    my $implClassName = GetImplName($interface);
    my $name = GetImplName($function);
    my $returnType = $function->type;
    my $code = "";
    my $isSVGTearOffType = (IsSVGTypeNeedingTearOff($returnType) and not $interfaceName =~ /List$/);

    my $index = 0;
    my $humanFriendlyIndex = $index + 1;

    my @arguments;
    my $functionName;
    my $implementedBy = $function->extendedAttributes->{"ImplementedBy"};
    if ($implementedBy) {
        my $implementedByImplName = GetImplNameFromImplementedBy($implementedBy);
        AddToImplIncludes(HeaderFilesForInterface($implementedBy, $implementedByImplName));
        unshift(@arguments, "imp") if !$function->isStatic;
        $functionName = "${implementedByImplName}::${name}";
    } elsif ($function->isStatic) {
        $functionName = "${implClassName}::${name}";
    } else {
        $functionName = "imp->${name}";
    }

    my $callWith = $function->extendedAttributes->{"CallWith"};
    my ($callWithArgs, $subCode) = GenerateCallWith($callWith, $indent, 1, $function);
    $code .= $subCode;
    unshift(@arguments, @$callWithArgs);
    $index += @$callWithArgs;
    $humanFriendlyIndex = $index + 1;

    $numberOfParameters += @$callWithArgs;

    foreach my $parameter (@{$function->parameters}) {
        if ($index eq $numberOfParameters) {
            last;
        }
        my $paramName = $parameter->name;
        my $paramType = $parameter->type;

        if ($replacements{$paramName}) {
            push @arguments, $replacements{$paramName};
        } elsif (IsSVGTypeNeedingTearOff($parameter->type) and not $interfaceName =~ /List$/) {
            push @arguments, "$paramName->propertyReference()";
            if ($hasExceptionState) {
                $code .= <<END;
    if (!$paramName) {
        exceptionState.throwTypeError(\"parameter $humanFriendlyIndex is not of type '${ \$parameter->type }'.\");
        exceptionState.throwIfNeeded();
        return;
    }
END
            } else {
                $code .= <<END;
    if (!$paramName) {
        throwTypeError(ExceptionMessages::failedToExecute(\"$name\", \"$interfaceName\", \"parameter $humanFriendlyIndex is not of type '${ \$parameter->type }'.\"), info.GetIsolate());
        return;
    }
END
           }
        } elsif ($parameter->type eq "SVGMatrix" and $interfaceName eq "SVGTransformList") {
            push @arguments, "$paramName.get()";
        } elsif (IsNullableParameter($parameter)) {
            push @arguments, "${paramName}IsNull ? 0 : &$paramName";
        } elsif (IsCallbackInterface($paramType) or $paramType eq "NodeFilter" or $paramType eq "XPathNSResolver") {
            push @arguments, "$paramName.release()";
        } else {
            push @arguments, $paramName;
        }
        $index++;
        $humanFriendlyIndex = $index + 1;
    }

    # Support for returning a union type.
    if (IsUnionType($returnType)) {
        my $types = $returnType->unionMemberTypes;
        for my $i (0 .. scalar(@$types)-1) {
            my $unionMemberType = $types->[$i];
            my $unionMemberNativeType = GetNativeType($unionMemberType);
            my $unionMemberNumber = $i + 1;
            my $unionMemberVariable = "result" . $i;
            my $unionMemberEnabledVariable = "result" . $i . "Enabled";
            $code .= "    bool ${unionMemberEnabledVariable} = false;\n";
            $code .= "    ${unionMemberNativeType} ${unionMemberVariable};\n";
            push @arguments, $unionMemberEnabledVariable;
            push @arguments, $unionMemberVariable;
        }
    }

    if ($function->extendedAttributes->{"RaisesException"}) {
        push @arguments, "exceptionState";
    }

    my $functionString = "$functionName(" . join(", ", @arguments) . ")";

    my $return = "result";
    my $returnIsRef = IsRefPtrType($returnType);

    if ($returnType eq "void" || IsUnionType($returnType)) {
        $code .= $indent . "$functionString;\n";
    } elsif (ExtendedAttributeContains($callWith, "ScriptState") or $function->extendedAttributes->{"RaisesException"}) {
        my $nativeReturnType = GetNativeType($returnType, {}, "");
        $nativeReturnType = GetSVGWrappedTypeNeedingTearOff($returnType) if $isSVGTearOffType;

        $code .= $indent . $nativeReturnType . " result = $functionString;\n";
    } else {
        # Can inline the function call into the return statement to avoid overhead of using a Ref<> temporary
        $return = $functionString;
        $returnIsRef = 0;

        if ($interfaceName eq "SVGTransformList" and IsRefPtrType($returnType)) {
            $return = "WTF::getPtr(" . $return . ")";
        }
    }

    if ($function->extendedAttributes->{"RaisesException"}) {
        $code .= $indent . "if (exceptionState.throwIfNeeded())\n";
        $code .= $indent . "    return;\n";
    }

    if (ExtendedAttributeContains($callWith, "ScriptState")) {
        $code .= $indent . "if (state.hadException()) {\n";
        $code .= $indent . "    v8::Local<v8::Value> exception = state.exception();\n";
        $code .= $indent . "    state.clearException();\n";
        $code .= $indent . "    throwError(exception, info.GetIsolate());\n";
        $code .= $indent . "    return;\n";
        $code .= $indent . "}\n";
    }

    if ($isSVGTearOffType) {
        AddToImplIncludes("V8$returnType.h");
        AddToImplIncludes("core/svg/properties/SVGPropertyTearOff.h");
        my $svgNativeType = GetSVGTypeNeedingTearOff($returnType);
        # FIXME: Update for all ScriptWrappables.
        if (IsDOMNodeType($interfaceName)) {
            if ($forMainWorldSuffix eq "ForMainWorld") {
                $code .= $indent . "v8SetReturnValueForMainWorld(info, WTF::getPtr(${svgNativeType}::create($return)));\n";
            } else {
                $code .= $indent . "v8SetReturnValueFast(info, WTF::getPtr(${svgNativeType}::create($return)), imp);\n";
            }
        } else {
            $code .= $indent . "v8SetReturnValue${forMainWorldSuffix}(info, WTF::getPtr(${svgNativeType}::create($return)));\n";
        }
        return $code;
    }

    # If the implementing class is a POD type, commit changes
    if (IsSVGTypeNeedingTearOff($interfaceName) and not $interfaceName =~ /List$/) {
        $code .= $indent . "wrapper->commitChange();\n";
    }

    $return .= ".release()" if ($returnIsRef);

    my $nativeValue;
    # FIXME: Update for all ScriptWrappables.
    if (IsDOMNodeType($interfaceName)) {
        $nativeValue = NativeToJSValue($function->type, $function->extendedAttributes, $return, $indent, "", "info.GetIsolate()", "info", "imp", $forMainWorldSuffix, "return");
    } else {
        $nativeValue = NativeToJSValue($function->type, $function->extendedAttributes, $return, $indent, "", "info.GetIsolate()", "info", 0, $forMainWorldSuffix, "return");
    }

    $code .= $nativeValue . "\n" if $nativeValue;  # Skip blank line for void return type

    return $code;
}

sub GetNativeType
{
    my $type = shift;
    my $extendedAttributes = shift;
    my $isParameter = shift;

    my $svgNativeType = GetSVGTypeNeedingTearOff($type);
    if ($svgNativeType) {
        if ($svgNativeType =~ /List$/) {
            return "${svgNativeType}*";
        } else {
            return "RefPtr<${svgNativeType} >";
        }
    }

    return "float" if $type eq "float";
    return "double" if $type eq "double";
    return "int" if $type eq "long" or $type eq "int" or $type eq "short" or $type eq "byte";
    if ($type eq "unsigned long" or $type eq "unsigned int" or $type eq "unsigned short" or $type eq "octet") {
        return "unsigned";
    }
    return "long long" if $type eq "long long";
    return "unsigned long long" if $type eq "unsigned long long";
    return "bool" if $type eq "boolean";

    if (($type eq "DOMString" || IsEnumType($type)) and $isParameter) {
        my $mode = GetV8StringResourceMode($extendedAttributes);
        # FIXME: Add the case for 'elsif ($attributeOrParameter->extendedAttributes->{"TreatUndefinedAs"} and $attributeOrParameter->extendedAttributes->{"TreatUndefinedAs"} eq "NullString"))'.
        return "V8StringResource<$mode>";
    }

    return "String" if $type eq "DOMString" or IsEnumType($type);

    return "ScriptPromise" if $type eq "Promise";

    return "Range::CompareHow" if $type eq "CompareHow";
    return "DOMTimeStamp" if $type eq "DOMTimeStamp";
    return "double" if $type eq "Date";
    return "ScriptValue" if $type eq "any" or IsCallbackFunctionType($type);
    return "Dictionary" if $type eq "Dictionary";

    die "UnionType is not supported" if IsUnionType($type);

    if (IsTypedArrayType($type)) {
        return $isParameter ? "${type}*" : "RefPtr<${type}>";
    }

    # We need to check [ImplementedAs] extended attribute for wrapper types.
    return "RefPtrWillBeRawPtr<$type>" if $type eq "XPathNSResolver";  # FIXME: Remove custom bindings for XPathNSResolver.
    if (IsWrapperType($type)) {
        my $interface = ParseInterface($type);
        my $implClassName = GetImplName($interface);
        if ($isParameter) {
            return "$implClassName*";
        } elsif (IsWillBeGarbageCollectedType($interface->name)) {
            return "RefPtrWillBeRawPtr<$implClassName>";
        } else {
            return "RefPtr<$implClassName>";
        }
    }
    return "RefPtr<$type>" if IsRefPtrType($type) and (not $isParameter or $nonWrapperTypes{$type});

    my $arrayOrSequenceType = GetArrayOrSequenceType($type);

    if ($arrayOrSequenceType) {
        my $nativeType = GetNativeType($arrayOrSequenceType);
        $nativeType .= " " if ($nativeType =~ />$/);
        return "Vector<${nativeType}>";
    }

    # Default, assume native type is a pointer with same type name as idl type
    return "${type}*";
}

sub GetNativeTypeForCallbacks
{
    my $type = shift;
    return "const String&" if $type eq "DOMString";
    return "PassRefPtr<SerializedScriptValue>" if $type eq "SerializedScriptValue";
    return "void" if $type eq "void";

    # Callbacks use raw pointers, so pass isParameter = 1
    my $nativeType = GetNativeType($type, {}, "parameter");
    return "const $nativeType&" if $nativeType =~ /^Vector/;
    return $nativeType;
}

sub JSValueToNativeStatement
{
    my $type = shift;
    my $extendedAttributes = shift;
    my $argIndexOrZero = shift;
    my $jsValue = shift;
    my $variableName = shift;
    my $indent = shift;
    my $getIsolate = shift;

    my $nativeType = GetNativeType($type, $extendedAttributes, "parameter");
    my $native_value = JSValueToNative($type, $extendedAttributes, $argIndexOrZero, $jsValue, $getIsolate);
    my $code = "";
    if ($type eq "DOMString" || IsEnumType($type)) {
        die "Wrong native type passed: $nativeType" unless $nativeType =~ /^V8StringResource/;
        if ($type eq "DOMString" or IsEnumType($type)) {
            $code .= $indent . "V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID($nativeType, $variableName, $native_value);\n"
        } else {
            $code .= $indent . "$nativeType $variableName($native_value, true);\n";
        }
    } elsif (IsIntegerType($type)) {
        $code .= $indent . "V8TRYCATCH_EXCEPTION_VOID($nativeType, $variableName, $native_value, exceptionState);\n";
    } else {
        $code .= $indent . "V8TRYCATCH_VOID($nativeType, $variableName, $native_value);\n";
    }
    return $code;
}


sub JSValueToNative
{
    my $type = shift;
    my $extendedAttributes = shift;
    # Argument position (1-indexed) or 0 if for a setter's value.
    my $argIndexOrZero = shift;
    my $value = shift;
    my $getIsolate = shift;

    return "$value->BooleanValue()" if $type eq "boolean";
    return "static_cast<$type>($value->NumberValue())" if $type eq "float" or $type eq "double";

    if (IsIntegerType($type)) {
        my $conversion = "to" . $integerTypeHash{$type} . "($value";
        if ($extendedAttributes->{"EnforceRange"}) {
            return "${conversion}, EnforceRange, exceptionState)";
        } else {
            return "${conversion}, exceptionState)";
        }
    }
    return "static_cast<Range::CompareHow>($value->Int32Value())" if $type eq "CompareHow";
    return "toCoreDate($value)" if $type eq "Date";

    if ($type eq "DOMString" or IsEnumType($type)) {
        return $value;
    }

    if ($type eq "SerializedScriptValue") {
        AddToImplIncludes("bindings/v8/SerializedScriptValue.h");
        return "SerializedScriptValue::create($value, $getIsolate)";
    }

    if ($type eq "Dictionary") {
        AddToImplIncludes("bindings/v8/Dictionary.h");
        return "Dictionary($value, $getIsolate)";
    }

    if ($type eq "any" || IsCallbackFunctionType($type)) {
        AddToImplIncludes("bindings/v8/ScriptValue.h");
        return "ScriptValue($value, $getIsolate)";
    }

    if ($type eq "Promise") {
        AddToImplIncludes("bindings/v8/ScriptPromise.h");
        return "ScriptPromise($value, $getIsolate)";
    }

    if ($type eq "NodeFilter") {
        return "toNodeFilter($value, $getIsolate)";
    }

    if ($type eq "Window") {
        return "toDOMWindow($value, $getIsolate)";
    }

    if ($type eq "MediaQueryListListener") {
        AddToImplIncludes("core/css/MediaQueryListListener.h");
        return "MediaQueryListListener::create(ScriptValue(" . $value . ", $getIsolate))";
    }

    if ($type eq "EventTarget") {
        return "V8DOMWrapper::isDOMWrapper($value) ? toWrapperTypeInfo(v8::Handle<v8::Object>::Cast($value))->toEventTarget(v8::Handle<v8::Object>::Cast($value)) : 0";
    }

    if (IsTypedArrayType($type)) {
        AddIncludesForType($type);
        return "$value->Is${type}() ? V8${type}::toNative(v8::Handle<v8::${type}>::Cast($value)) : 0"
    }

    if ($type eq "XPathNSResolver") {
        return "toXPathNSResolver($value, $getIsolate)";
    }

    my $arrayOrSequenceType = GetArrayOrSequenceType($type);

    if ($arrayOrSequenceType) {
        if (IsRefPtrType($arrayOrSequenceType)) {
            AddToImplIncludes("V8${arrayOrSequenceType}.h");
            return "(toRefPtrNativeArray<${arrayOrSequenceType}, V8${arrayOrSequenceType}>($value, $argIndexOrZero, $getIsolate))";
        }
        return "toNativeArray<" . GetNativeType($arrayOrSequenceType) . ">($value, $argIndexOrZero, $getIsolate)";
    }

    AddIncludesForType($type);

    AddToImplIncludes("V8${type}.h");
    return "V8${type}::toNativeWithTypeCheck(info.GetIsolate(), $value)";
}

sub IsUnionType
{
    my $type = shift; # string or UnionType
    if(ref($type) eq "UnionType") {
        die "Currently only 2 values of non-union type is supported as union type.\n" unless @{$type->unionMemberTypes} == 2;
        return 1;
    }
    return 0;
}

sub IsWrapperType
{
    my $type = shift;
    return 0 if GetArrayType($type);
    return 0 if GetSequenceType($type);
    return 0 if IsCallbackFunctionType($type);
    return 0 if IsEnumType($type);
    return 0 if IsPrimitiveType($type);
    return 0 if $type eq "Promise";
    return !$nonWrapperTypes{$type};
}

sub IsCallbackInterface
{
    my $type = shift;
    return 0 unless IsWrapperType($type);
    return 0 if IsTypedArrayType($type);

    my $idlFile = IDLFileForInterface($type)
        or die("Could NOT find IDL file for interface \"$type\"!\n");

    open FILE, "<", $idlFile;
    my @lines = <FILE>;
    close FILE;

    my $fileContents = join('', @lines);
    return ($fileContents =~ /callback\s+interface\s+(\w+)\s*{/gs);
}

sub GetNativeTypeOfTypedArray
{
    my $interface = shift;
    my $interfaceName = $interface->name;
    die "TypedArray of unknown type is found" unless $typedArrayHash{$interface->name};
    return @{$typedArrayHash{$interface->name}};
}

sub IsDOMNodeType
{
    my $type = shift;

    return 1 if $domNodeTypes{$type};
    return 1 if $type =~ /^HTML.*Element$/;
    return 1 if $type =~ /^SVG.*Element$/;

    return 0;
}


sub NativeToJSValue
{
    my $type = shift;
    my $extendedAttributes = shift;
    my $nativeValue = shift;
    my $indent = shift;  # added before every line
    my $receiver = shift;  # "return" or "<variableName> ="
    my $getIsolate = shift;
    die "An Isolate is mandatory for native value => JS value conversion." unless $getIsolate;
    my $getCallbackInfo = shift || "";
    my $creationContext = $getCallbackInfo ? "${getCallbackInfo}.Holder()" : "v8::Handle<v8::Object>()";
    my $getScriptWrappable = shift || "";
    my $forMainWorldSuffix = shift || "";
    my $returnValueArg = shift || 0;
    my $isReturnValue = $returnValueArg eq "return";

    if (IsUnionType($type)) {
        my $types = $type->unionMemberTypes;
        my @codes = ();
        for my $i (0 .. scalar(@$types)-1) {
            my $unionMemberType = $types->[$i];
            my $unionMemberNumber = $i + 1;
            my $unionMemberVariable = $nativeValue . $i;
            my $unionMemberEnabledVariable = $nativeValue . $i . "Enabled";
            my $unionMemberNativeValue = $unionMemberVariable;
            $unionMemberNativeValue .= ".release()" if (IsRefPtrType($unionMemberType));
            my $returnJSValueCode = NativeToJSValue($unionMemberType, $extendedAttributes, $unionMemberNativeValue, $indent . "    ", $receiver, $getIsolate, $getCallbackInfo, $getScriptWrappable, $forMainWorldSuffix, $returnValueArg);
            my $code = "";
            if ($isReturnValue) {
              $code .= "${indent}if (${unionMemberEnabledVariable}) {\n";
              $code .= "${returnJSValueCode}\n";
              $code .= "${indent}    return;\n";
              $code .= "${indent}}";
            } else {
              $code .= "${indent}if (${unionMemberEnabledVariable})\n";
              $code .= "${returnJSValueCode}";
            }
            push @codes, $code;
        }
        if ($isReturnValue) {
            # Fall back to returning null if none of the union members results are returned.
            push @codes, "${indent}v8SetReturnValueNull(${getCallbackInfo});";
        }
        return join "\n", @codes;
    }

    if ($type eq "boolean") {
        return "${indent}v8SetReturnValueBool(${getCallbackInfo}, ${nativeValue});" if $isReturnValue;
        return "$indent$receiver v8Boolean($nativeValue, $getIsolate);";
    }

    if ($type eq "void") { # equivalent to v8Undefined()
        return "" if $isReturnValue;
        return "$indent$receiver v8Undefined();"
    }

    # HTML5 says that unsigned reflected attributes should be in the range
    # [0, 2^31). When a value isn't in this range, a default value (or 0)
    # should be returned instead.
    if ($extendedAttributes->{"Reflect"} and ($type eq "unsigned long" or $type eq "unsigned short")) {
        $nativeValue =~ s/getUnsignedIntegralAttribute/getIntegralAttribute/g;
        return "${indent}v8SetReturnValueUnsigned(${getCallbackInfo}, std::max(0, ${nativeValue}));" if $isReturnValue;
        return "$indent$receiver v8::Integer::NewFromUnsigned($getIsolate, std::max(0, " . $nativeValue . "));";
    }

    my $nativeType = GetNativeType($type);
    if ($nativeType eq "int") {
        return "${indent}v8SetReturnValueInt(${getCallbackInfo}, ${nativeValue});" if $isReturnValue;
        return "$indent$receiver v8::Integer::New($getIsolate, $nativeValue);";
    }

    if ($nativeType eq "unsigned") {
        return "${indent}v8SetReturnValueUnsigned(${getCallbackInfo}, ${nativeValue});" if $isReturnValue;
        return "$indent$receiver v8::Integer::NewFromUnsigned($getIsolate, $nativeValue);";
    }

    if ($type eq "Date") {
        return "${indent}v8SetReturnValue(${getCallbackInfo}, v8DateOrNull($nativeValue, $getIsolate));" if $isReturnValue;
        return "$indent$receiver v8DateOrNull($nativeValue, $getIsolate);"
    }

    # long long and unsigned long long are not representable in ECMAScript.
    if ($type eq "long long" or $type eq "unsigned long long" or $type eq "DOMTimeStamp") {
        return "${indent}v8SetReturnValue(${getCallbackInfo}, static_cast<double>($nativeValue));" if $isReturnValue;
        return "$indent$receiver v8::Number::New($getIsolate, static_cast<double>($nativeValue));";
    }

    if ($type eq "DOMString" && $extendedAttributes->{"ReflectOnly"}) {
        my $code = "${indent}String resultValue = ${nativeValue};\n";
        $code .= GenerateReflectOnlyCheck($extendedAttributes, ${indent});
        return "${code}${indent}v8SetReturnValueString(${getCallbackInfo}, resultValue, $getIsolate);" if $isReturnValue;
        return "${code}$indent$receiver resultValue";
    }

    my $conv = $extendedAttributes->{"TreatReturnedNullStringAs"};
    if (($type eq "DOMString" || IsEnumType($type)) && $isReturnValue) {
        my $functionSuffix = "";
        if (defined $conv) {
            if ($conv eq "Null") {
                $functionSuffix = "OrNull";
            } elsif ($conv eq "Undefined") {
                $functionSuffix = "OrUndefined";
            } else {
                die "Unknown value for TreatReturnedNullStringAs extended attribute";
            }
        }
        return "${indent}v8SetReturnValueString${functionSuffix}(${getCallbackInfo}, $nativeValue, $getIsolate);";
    }

    if ($type eq "DOMString" or IsEnumType($type)) {
        my $returnValue = "";
        if (defined $conv) {
            if ($conv eq "Null") {
                $returnValue = "v8StringOrNull($nativeValue, $getIsolate)";
            } elsif ($conv eq "Undefined") {
                $returnValue = "v8StringOrUndefined($nativeValue, $getIsolate)";
            } else {
                die "Unknown value for TreatReturnedNullStringAs extended attribute";
            }
        } else {
            $returnValue = "v8String($getIsolate, $nativeValue)";
        }
        return "$indent$receiver $returnValue;";
    }

    if (IsPrimitiveType($type)) {
        die "unexpected type $type" if not ($type eq "float" or $type eq "double");
        return "${indent}v8SetReturnValue(${getCallbackInfo}, ${nativeValue});" if $isReturnValue;
        return "$indent$receiver v8::Number::New($getIsolate, $nativeValue);";
    }

    if ($nativeType eq "ScriptValue" or $nativeType eq "ScriptPromise") {
        return "${indent}v8SetReturnValue(${getCallbackInfo}, ${nativeValue}.v8Value());" if $isReturnValue;
        return "$indent$receiver $nativeValue.v8Value();";
    }

    my $arrayOrSequenceType = GetArrayOrSequenceType($type);

    if ($arrayOrSequenceType) {
        if (IsRefPtrType($arrayOrSequenceType)) {
            AddIncludesForType($arrayOrSequenceType);
        }
        return "${indent}v8SetReturnValue(${getCallbackInfo}, v8Array($nativeValue, $getIsolate));" if $isReturnValue;
        return "$indent$receiver v8Array($nativeValue, $getIsolate);";
    }

    AddIncludesForType($type);

    if ($type eq "SerializedScriptValue") {
        my $returnValue = "$nativeValue ? $nativeValue->deserialize() : v8::Handle<v8::Value>(v8::Null($getIsolate))";
        return "${indent}v8SetReturnValue(${getCallbackInfo}, $returnValue);" if $isReturnValue;
        return "$indent$receiver $returnValue;";
    }

    if ($getScriptWrappable) {
        # FIXME: Use safe handles
        if ($isReturnValue) {
            if ($forMainWorldSuffix eq "ForMainWorld") {
                return "${indent}v8SetReturnValueForMainWorld(${getCallbackInfo}, $nativeValue);";
            }
            return "${indent}v8SetReturnValueFast(${getCallbackInfo}, $nativeValue, $getScriptWrappable);";
        }
    }
    # FIXME: Use safe handles
    return "${indent}v8SetReturnValue(${getCallbackInfo}, $nativeValue);" if $isReturnValue;
    return "$indent$receiver toV8($nativeValue, $creationContext, $getIsolate);";
}

sub WriteData
{
    my $object = shift;
    my $interface = shift;
    my $outputDirectory = shift;

    my $name = $interface->name;
    my $headerFileName = "$outputDirectory/V8$name.h";
    my $implFileName = "$outputDirectory/V8$name.cpp";

    my @includes = ();
    foreach my $include (keys %implIncludes) {
        push @includes, "\"$include\"";
    }

    #FIXME: do not treat main header special
    my $mainInclude = "\"V8$name.h\"";
    foreach my $include (sort @includes) {
        $implementation{includes}->add("#include $include\n") unless $include eq $mainInclude;
    }
    $implementation{includes}->add("\n") unless $interface->isCallback;
    WriteFileIfChanged($implFileName, $implementation{root}->toString());

    %implIncludes = ();

    WriteFileIfChanged($headerFileName, $header{root}->toString());
}

sub ConvertToV8StringResource
{
    my $attributeOrParameter = shift;
    my $nativeType = shift;
    my $variableName = shift;
    my $value = shift;

    die "Wrong native type passed: $nativeType" unless $nativeType =~ /^V8StringResource/;
    if ($attributeOrParameter->type eq "DOMString" or IsEnumType($attributeOrParameter->type)) {
        return "V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID($nativeType, $variableName, $value);"
    } else {
        return "$nativeType $variableName($value, true);";
    }
}

# Returns the RuntimeEnabledFeatures function name that is hooked up to check if a method/attribute is enabled.
sub GetRuntimeEnabledFunctionName
{
    my $signature = shift;

    # Given [RuntimeEnabled=FeatureName],
    # return RuntimeEnabledFeatures::{featureName}Enabled;
    my $featureName = ToMethodName($signature->extendedAttributes->{"RuntimeEnabled"});
    return "RuntimeEnabledFeatures::${featureName}Enabled";
}

sub GetContextEnabledFunctionName
{
    my $signature = shift;

    # Given [PerContextEnabled=FeatureName],
    # return ContextFeatures::{featureName}Enabled
    my $featureName = ToMethodName($signature->extendedAttributes->{"PerContextEnabled"});
    return "ContextFeatures::${featureName}Enabled";
}

sub GetPassRefPtrType
{
    my $interface = shift;
    my $nativeType = GetNativeTypeForConversions($interface);

    my $willBe = IsWillBeGarbageCollectedType($interface->name) ? "WillBeRawPtr" : "";
    my $extraSpace = $nativeType =~ />$/ ? " " : "";
    return "PassRefPtr${willBe}<${nativeType}${extraSpace}>";
}

sub WriteFileIfChanged
{
    my $fileName = shift;
    my $contents = shift;

    if (-f $fileName && $writeFileOnlyIfChanged) {
        open FH, "<", $fileName or die "Couldn't open $fileName: $!\n";
        my @lines = <FH>;
        my $oldContents = join "", @lines;
        close FH;
        return if $contents eq $oldContents;
    }
    open FH, ">", $fileName or die "Couldn't open $fileName: $!\n";
    print FH $contents;
    close FH;
}

sub ForAllParents
{
    my $interface = shift;
    my $beforeRecursion = shift;
    my $afterRecursion = shift;

    my $recurse;
    $recurse = sub {
        my $currentInterface = shift;

        if ($currentInterface->parent) {
            my $parentInterface = ParseInterface($currentInterface->parent);
            if ($beforeRecursion) {
                &$beforeRecursion($parentInterface) eq 'prune' and return;
            }
            &$recurse($parentInterface);
            &$afterRecursion($parentInterface) if $afterRecursion;
        }
    };

    &$recurse($interface);
}

sub IsPrimitiveType
{
    my $type = shift;

    return 1 if $integerTypeHash{$type};
    return 1 if $primitiveTypeHash{$type};
    return 0;
}

sub IsIntegerType
{
    my $type = shift;

    return 1 if $integerTypeHash{$type};
    return 0;
}

sub IsCallbackFunctionType
{
    my $type = shift;

    return 1 if $callbackFunctionTypeHash{$type};
    return 0;
}

sub IsEnumType
{
    my $type = shift;

    return 1 if $enumTypeHash{$type};
    return 0;
}

sub ValidEnumValues
{
    my $type = shift;

    return @{$enumTypeHash{$type}};
}

sub IsSVGTypeNeedingTearOff
{
    my $type = shift;

    return 1 if $svgTypeNeedingTearOff{$type};
    return 0;
}

sub IsSVGTypeWithWritablePropertiesNeedingTearOff
{
    my $type = shift;

    return 1 if $svgTypeWithWritablePropertiesNeedingTearOff{$type};
    return 0;
}

sub IsTypedArrayType
{
    my $type = shift;
    return 1 if $typedArrayHash{$type};
    return 0;
}

sub IsWillBeGarbageCollectedType
{
    my $interfaceName = shift;
    return 0 unless IsWrapperType($interfaceName);
    my $interface = ParseInterface($interfaceName);
    return InheritsExtendedAttribute($interface, "WillBeGarbageCollected");
}

sub IsRefPtrType
{
    my $type = shift;

    return 0 if $type eq "any";
    return 0 if IsPrimitiveType($type);
    return 0 if GetArrayType($type);
    return 0 if GetSequenceType($type);
    return 0 if $type eq "Promise";
    return 0 if IsCallbackFunctionType($type);
    return 0 if IsEnumType($type);
    return 0 if IsUnionType($type);
    return 0 if $type eq "Dictionary";

    return 1;
}

sub IsNullableParameter
{
    my $parameter = shift;

    return $parameter->isNullable && !IsRefPtrType($parameter->type) && $parameter->type ne "Dictionary";
}

sub GetSVGTypeNeedingTearOff
{
    my $type = shift;

    return $svgTypeNeedingTearOff{$type} if exists $svgTypeNeedingTearOff{$type};
    return undef;
}

sub GetSVGWrappedTypeNeedingTearOff
{
    my $type = shift;

    my $svgTypeNeedingTearOff = GetSVGTypeNeedingTearOff($type);
    return $svgTypeNeedingTearOff if not $svgTypeNeedingTearOff;

    if ($svgTypeNeedingTearOff =~ /SVGPropertyTearOff/) {
        $svgTypeNeedingTearOff =~ s/SVGPropertyTearOff<//;
    } elsif ($svgTypeNeedingTearOff =~ /SVGListPropertyTearOff/) {
        $svgTypeNeedingTearOff =~ s/SVGListPropertyTearOff<//;
    } elsif ($svgTypeNeedingTearOff =~ /SVGStaticListPropertyTearOff/) {
        $svgTypeNeedingTearOff =~ s/SVGStaticListPropertyTearOff<//;
    }  elsif ($svgTypeNeedingTearOff =~ /SVGTransformListPropertyTearOff/) {
        $svgTypeNeedingTearOff =~ s/SVGTransformListPropertyTearOff<//;
    } elsif ($svgTypeNeedingTearOff =~ /SVGMatrixTearOff/) {
        $svgTypeNeedingTearOff = 'SVGMatrix';
    }

    $svgTypeNeedingTearOff =~ s/>//;
    return $svgTypeNeedingTearOff;
}

sub IsSVGAnimatedType
{
    my $type = shift;

    return 0 if $svgTypeNewPropertyImplementation{$type};

    return $type =~ /^SVGAnimated/;
}

sub SVGTypeNeedsToHoldContextElement
{
    my $type = shift;

    return IsSVGTypeNeedingTearOff($type) || IsSVGAnimatedType($type);
}

sub GetSequenceType
{
    my $type = shift;

    return $1 if $type =~ /^sequence<([\w\d_\s]+)>.*/;
    return "";
}

sub GetArrayType
{
    my $type = shift;

    return $1 if $type =~ /^([\w\d_\s]+)\[\]/;
    return "";
}

sub GetArrayOrSequenceType
{
    my $type = shift;

    return GetArrayType($type) || GetSequenceType($type);
}

sub AssertNotSequenceType
{
    my $type = shift;
    die "Sequences must not be used as the type of an attribute, constant or exception field." if GetSequenceType($type);
}

sub FirstLetterToUpperCase
{
    my $param = shift;
    my $ret = ucfirst($param);
    # Capitalize initial acronym, e.g., xml -> setXML
    $ret =~ s/Xml/XML/ if $ret =~ /^Xml/;
    $ret =~ s/Css/CSS/ if $ret =~ /^Css/;
    $ret =~ s/Html/HTML/ if $ret =~ /^Html/;
    $ret =~ s/Ime/IME/ if $ret =~ /^Ime/;
    $ret =~ s/Svg/SVG/ if $ret =~ /^Svg/;
    return $ret;
}

# URL becomes url, but SetURL becomes setURL.
sub ToMethodName
{
    my $param = shift;
    my $ret = lcfirst($param);
    $ret =~ s/hTML/html/ if $ret =~ /^hTML/;
    $ret =~ s/iME/ime/ if $ret =~ /^iME/;
    $ret =~ s/uRL/url/ if $ret =~ /^uRL/;
    $ret =~ s/jS/js/ if $ret =~ /^jS/;
    $ret =~ s/xML/xml/ if $ret =~ /^xML/;
    $ret =~ s/xSLT/xslt/ if $ret =~ /^xSLT/;
    $ret =~ s/cSS/css/ if $ret =~ /^cSS/;

    # For HTML5 FileSystem API Flags attributes.
    # (create is widely used to instantiate an object and must be avoided.)
    $ret =~ s/^create/isCreate/ if $ret =~ /^create$/;
    $ret =~ s/^exclusive/isExclusive/ if $ret =~ /^exclusive$/;

    return $ret;
}

sub NamespaceForAttributeName
{
    my ($interfaceName, $attributeName) = @_;
    return "SVGNames" if $interfaceName =~ /^SVG/ && !$svgAttributesInHTMLHash{$attributeName};
    return "HTMLNames";
}

# Identifies overloaded functions and for each function adds an array with
# links to its respective overloads (including itself).
sub LinkOverloadedFunctions
{
    my $interface = shift;

    my %nameToFunctionsMap = ();
    foreach my $function (@{$interface->functions}) {
        my $name = $function->name;
        $nameToFunctionsMap{$name} = [] if !exists $nameToFunctionsMap{$name} or !$name;  # Nameless functions cannot be overloaded
        push(@{$nameToFunctionsMap{$name}}, $function);
        $function->{overloads} = $nameToFunctionsMap{$name};
        $function->{overloadIndex} = @{$nameToFunctionsMap{$name}};
    }
}

sub AttributeNameForGetterAndSetter
{
    my $attribute = shift;

    my $attributeName = GetImplName($attribute);
    if ($attribute->extendedAttributes->{"ImplementedAs"}) {
        $attributeName = $attribute->extendedAttributes->{"ImplementedAs"};
    }
    my $attributeType = $attribute->type;

    return $attributeName;
}

sub ContentAttributeName
{
    my ($interfaceName, $attribute) = @_;

    my $contentAttributeName = $attribute->extendedAttributes->{"Reflect"};
    return undef if !$contentAttributeName;

    $contentAttributeName = lc AttributeNameForGetterAndSetter($attribute) if $contentAttributeName eq "VALUE_IS_MISSING";

    my $namespace = NamespaceForAttributeName($interfaceName, $contentAttributeName);

    AddToImplIncludes("${namespace}.h");
    # Attr (not Attribute) used in core content attributes
    return "${namespace}::${contentAttributeName}Attr";
}

sub GetterExpression
{
    my ($interfaceName, $attribute) = @_;

    my $contentAttributeName = ContentAttributeName($interfaceName, $attribute);

    if (!$contentAttributeName) {
        return (ToMethodName(AttributeNameForGetterAndSetter($attribute)));
    }

    my $functionName;
    if ($attribute->extendedAttributes->{"URL"}) {
        $functionName = "getURLAttribute";
    } elsif ($attribute->type eq "boolean") {
        $functionName = "fastHasAttribute";
    } elsif ($attribute->type eq "long") {
        $functionName = "getIntegralAttribute";
    } elsif ($attribute->type eq "unsigned long") {
        $functionName = "getUnsignedIntegralAttribute";
    } else {
        if ($contentAttributeName eq "HTMLNames::idAttr") {
            $functionName = "getIdAttribute";
            $contentAttributeName = "";
        } elsif ($contentAttributeName eq "HTMLNames::nameAttr") {
            $functionName = "getNameAttribute";
            $contentAttributeName = "";
        } elsif ($contentAttributeName eq "HTMLNames::classAttr") {
            $functionName = "getClassAttribute";
            $contentAttributeName = "";
        } else {
            # We cannot use fast attributes for animated SVG types.
            $functionName = IsSVGAnimatedType($attribute->type) ? "getAttribute" : "fastGetAttribute";
        }
    }

    return ($functionName, $contentAttributeName);
}

sub SetterExpression
{
    my ($interfaceName, $attribute) = @_;

    my $contentAttributeName = ContentAttributeName($interfaceName, $attribute);

    if (!$contentAttributeName) {
        return ("set" . FirstLetterToUpperCase(AttributeNameForGetterAndSetter($attribute)));
    }

    my $functionName;
    if ($attribute->type eq "boolean") {
        $functionName = "setBooleanAttribute";
    } elsif ($attribute->type eq "long") {
        $functionName = "setIntegralAttribute";
    } elsif ($attribute->type eq "unsigned long") {
        $functionName = "setUnsignedIntegralAttribute";
    } else {
        $functionName = "setAttribute";
    }

    return ($functionName, $contentAttributeName);
}

sub GenerateConditionalString
{
    my $node = shift;

    my $conditional = $node->extendedAttributes->{"Conditional"};
    if ($conditional) {
        return GenerateConditionalStringFromAttributeValue($conditional);
    } else {
        return "";
    }
}

sub GenerateConditionalStringFromAttributeValue
{
    my $conditional = shift;

    my $operator = ($conditional =~ /&/ ? '&' : ($conditional =~ /\|/ ? '|' : ''));
    if ($operator) {
        # Avoid duplicated conditions.
        my %conditions;
        map { $conditions{$_} = 1 } split('\\' . $operator, $conditional);
        return "ENABLE(" . join(") $operator$operator ENABLE(", sort keys %conditions) . ")";
    } else {
        return "ENABLE(" . $conditional . ")";
    }
}

sub GenerateCompileTimeCheckForEnumsIfNeeded
{
    my $interface = shift;
    my $implClassName = GetImplName($interface);
    my @checks = ();
    # If necessary, check that all constants are available as enums with the same value.
    if (!$interface->extendedAttributes->{"DoNotCheckConstants"} && @{$interface->constants}) {
        foreach my $constant (@{$interface->constants}) {
            my $reflect = $constant->extendedAttributes->{"Reflect"};
            my $name = $reflect ? $reflect : $constant->name;
            my $value = $constant->value;

            if ($constant->extendedAttributes->{"ImplementedBy"}) {
                my $implementedByImplName = GetImplNameFromImplementedBy($constant->extendedAttributes->{"ImplementedBy"});
                push(@checks, "    COMPILE_ASSERT($value == " . $implementedByImplName . "::$name, TheValueOf${implClassName}_${name}DoesntMatchWithImplementation);\n");
            } else {
                push(@checks, "    COMPILE_ASSERT($value == ${implClassName}::$name, TheValueOf${implClassName}_${name}DoesntMatchWithImplementation);\n");
            }
        }
    }
    return @checks;
}

sub GenerateReflectOnlyCheck
{
    my $extendedAttributes = shift;
    my $indent = shift;

    my $attributeValueList = $extendedAttributes->{"ReflectOnly"};
    my @knownValues = split(quotemeta("|"), $attributeValueList);

    my $missingValueDefault = $extendedAttributes->{"ReflectMissing"};
    if ($missingValueDefault) {
        if (!grep { $_ eq $missingValueDefault } @knownValues) {
            die "The [ReflectMissing] attribute value '${missingValueDefault}' is not a known value ";
        }
        $missingValueDefault = "resultValue = \"${missingValueDefault}\"";
    } else {
        $missingValueDefault = "";
    }

    my $invalidValueDefault = $extendedAttributes->{"ReflectInvalid"};
    if ($invalidValueDefault) {
        if (!grep { $_ eq $invalidValueDefault } @knownValues) {
            die "The [ReflectInvalid] attribute value '${invalidValueDefault}' is not a known value ";
        }
        $invalidValueDefault = "resultValue = \"${invalidValueDefault}\"";
    } else {
        $invalidValueDefault = "resultValue = \"\"";
    }

    my @normalizeAttributeCode = ();

    # Attributes without a value (div empty-attribute>) can be
    # separately reflected to some value.

    my $isAttributeValueMissing = "resultValue.isEmpty()";
    my $emptyValueDefault = $extendedAttributes->{"ReflectEmpty"};
    if ($emptyValueDefault) {
        if (!grep { $_ eq $emptyValueDefault } @knownValues) {
            die "The [ReflectEmpty] attribute value '${emptyValueDefault}' is not a known value ";
        }
        $isAttributeValueMissing = "resultValue.isNull()";
        push(@normalizeAttributeCode, "${indent}} else if (resultValue.isEmpty()) {");
        push(@normalizeAttributeCode, "${indent}    resultValue = \"$emptyValueDefault\";");
    }

    # Attribute is limited to only known values: check that the attribute
    # value is one of those..and if not, set it to the empty string.
    # ( http://www.whatwg.org/specs/web-apps/current-work/#limited-to-only-known-values )
    #
    foreach my $knownValue (@knownValues) {
        push(@normalizeAttributeCode, "${indent}} else if (equalIgnoringCase(resultValue, \"$knownValue\")) {");
        push(@normalizeAttributeCode, "${indent}    resultValue = \"$knownValue\";");
    }
    my $normalizeAttributeValue = join("\n", @normalizeAttributeCode);
    my $code .= <<END;
${indent}if ($isAttributeValueMissing) {
${indent}    $missingValueDefault;
${normalizeAttributeValue}
${indent}} else {
${indent}    $invalidValueDefault;
${indent}}
END
        return "${code}";
}

sub ExtendedAttributeContains
{
    my $extendedAttributeValue = shift;
    return 0 unless $extendedAttributeValue;
    my $keyword = shift;

    my @extendedAttributeValues = split /\s*(\&|\|)\s*/, $extendedAttributeValue;
    return grep { $_ eq $keyword } @extendedAttributeValues;
}

sub InheritsInterface
{
    my $interface = shift;
    my $interfaceName = shift;
    my $found = 0;

    return 1 if $interfaceName eq $interface->name;
    ForAllParents($interface, sub {
        my $currentInterface = shift;
        if ($currentInterface->name eq $interfaceName) {
            $found = 1;
        }
        return 1 if $found;
    }, 0);

    return $found;
}

sub InheritsExtendedAttribute
{
    my $interface = shift;
    my $extendedAttribute = shift;
    my $found = 0;

    return 1 if $interface->extendedAttributes->{$extendedAttribute};
    ForAllParents($interface, sub {
        my $currentInterface = shift;
        if ($currentInterface->extendedAttributes->{$extendedAttribute}) {
            $found = 1;
        }
        return 1 if $found;
    }, 0);

    return $found;
}

sub NeedsSpecialWrap
{
    my $interface = shift;

    return 1 if ExtendedAttributeContains($interface->extendedAttributes->{"Custom"}, "ToV8");
    return 1 if ExtendedAttributeContains($interface->extendedAttributes->{"Custom"}, "Wrap");
    return 1 if $interface->extendedAttributes->{"SpecialWrapFor"};
    return 1 if InheritsInterface($interface, "Document");

    return 0;
}

sub HasExceptionRaisingParameter
{
    my $function = shift;

    foreach my $parameter (@{$function->parameters}) {
        if ($parameter->type eq "SerializedScriptValue") {
            return 1;
        } elsif (IsIntegerType($parameter->type)) {
            return 1;
        }
    }
    return 0;
}

1;
