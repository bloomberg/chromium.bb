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
# Copyright (C) 2013 Samsung Electronics. All rights reserved.
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


package CodeGeneratorV8;

use strict;
use Cwd;
use File::Basename;
use File::Find;
use File::Spec;

my $idlDocument;
my $idlDirectories;
my $preprocessor;
my $defines;
my $verbose;
my $dependentIdlFiles;
my $sourceRoot;

# Cache of IDL file pathnames.
my $idlFiles;
my $cachedInterfaces = {};

my @headerContent = ();
my @implContentHeader = ();
my @implContent = ();
my @implContentInternals = ();
my %implIncludes = ();
my %headerIncludeFiles = ();

# Header code structure:
# Root                    ... Copyright, include duplication check
#   Conditional           ... #if FEATURE ... #endif  (to be removed soon)
#     Includes
#     NameSpaceWebCore
#       Class
#         ClassPublic
#         ClassPrivate
my %header;

my %primitiveTypeHash = ( "boolean" => 1,
                          "void" => 1,
                          "Date" => 1,
                          "short" => 1,
                          "long" => 1,
                          "long long" => 1,
                          "unsigned short" => 1,
                          "unsigned long" => 1,
                          "unsigned long long" => 1,
                          "float" => 1,
                          "double" => 1);

my %enumTypeHash = ();

my %svgAnimatedTypeHash = ("SVGAnimatedAngle" => 1, "SVGAnimatedBoolean" => 1,
                           "SVGAnimatedEnumeration" => 1, "SVGAnimatedInteger" => 1,
                           "SVGAnimatedLength" => 1, "SVGAnimatedLengthList" => 1,
                           "SVGAnimatedNumber" => 1, "SVGAnimatedNumberList" => 1,
                           "SVGAnimatedPreserveAspectRatio" => 1,
                           "SVGAnimatedRect" => 1, "SVGAnimatedString" => 1,
                           "SVGAnimatedTransformList" => 1);

my %svgAttributesInHTMLHash = ("class" => 1, "id" => 1, "onabort" => 1, "onclick" => 1,
                               "onerror" => 1, "onload" => 1, "onmousedown" => 1,
                               "onmousemove" => 1, "onmouseout" => 1, "onmouseover" => 1,
                               "onmouseup" => 1, "onresize" => 1, "onscroll" => 1,
                               "onunload" => 1);

my %svgTypeNeedingTearOff = (
    "SVGAngle" => "SVGPropertyTearOff<SVGAngle>",
    "SVGLength" => "SVGPropertyTearOff<SVGLength>",
    "SVGLengthList" => "SVGListPropertyTearOff<SVGLengthList>",
    "SVGMatrix" => "SVGPropertyTearOff<SVGMatrix>",
    "SVGNumber" => "SVGPropertyTearOff<float>",
    "SVGNumberList" => "SVGListPropertyTearOff<SVGNumberList>",
    "SVGPathSegList" => "SVGPathSegListPropertyTearOff",
    "SVGPoint" => "SVGPropertyTearOff<FloatPoint>",
    "SVGPointList" => "SVGListPropertyTearOff<SVGPointList>",
    "SVGPreserveAspectRatio" => "SVGPropertyTearOff<SVGPreserveAspectRatio>",
    "SVGRect" => "SVGPropertyTearOff<FloatRect>",
    "SVGStringList" => "SVGStaticListPropertyTearOff<SVGStringList>",
    "SVGTransform" => "SVGPropertyTearOff<SVGTransform>",
    "SVGTransformList" => "SVGTransformListPropertyTearOff"
);

my %svgTypeWithWritablePropertiesNeedingTearOff = (
    "SVGPoint" => 1,
    "SVGMatrix" => 1
);

# Default .h template
my $headerTemplate = <<EOF;
/*
    This file is part of the Blink open source project.
    This file has been auto-generated by CodeGeneratorV8.pm. DO NOT MODIFY!

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/
EOF

sub new
{
    my $object = shift;
    my $reference = { };

    $idlDocument = shift;
    $idlDirectories = shift;
    $preprocessor = shift;
    $defines = shift;
    $verbose = shift;
    $dependentIdlFiles = shift;
    $sourceRoot = getcwd();

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
        foreach my $idlFile (@$dependentIdlFiles) {
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

sub HeaderFileForInterface
{
    my $interfaceName = shift;

    my $idlFilename = IDLFileForInterface($interfaceName)
        or die("Could NOT find IDL file for interface \"$interfaceName\" $!\n");

    $idlFilename = "bindings/" . File::Spec->abs2rel($idlFilename, $sourceRoot);
    $idlFilename =~ s/idl$/h/;
    return $idlFilename;
}

sub ParseInterface
{
    my $interfaceName = shift;

    return undef if $interfaceName eq 'Object';

    if (exists $cachedInterfaces->{$interfaceName}) {
        return $cachedInterfaces->{$interfaceName};
    }

    # Step #1: Find the IDL file associated with 'interface'
    my $filename = IDLFileForInterface($interfaceName)
        or die("Could NOT find IDL file for interface \"$interfaceName\" $!\n");

    print "  |  |>  Parsing parent IDL \"$filename\" for interface \"$interfaceName\"\n" if $verbose;

    # Step #2: Parse the found IDL file (in quiet mode).
    my $parser = IDLParser->new(1);
    my $document = $parser->Parse($filename, $defines, $preprocessor);

    foreach my $interface (@{$document->interfaces}) {
        if ($interface->name eq $interfaceName) {
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

    %enumTypeHash = map { $_->name => $_->values } @{$idlDocument->enumerations};
    my $interfaceName = $interface->name;
    my $v8InterfaceName = "V8" . $interface->name;
    my $defineName = $v8InterfaceName . "_h";

    my $conditionalString = GenerateConditionalString($interface);
    my $conditionalIf = "";
    my $conditionalEndif = "";
    if ($conditionalString) {
        $conditionalIf = "#if ${conditionalString}";
        $conditionalEndif = "#endif // ${conditionalString}\n";
    }

    $header{root} = new Block("ROOT", "", "");
    $header{conditional} = new Block("Conditional", "$conditionalIf", "$conditionalEndif");
    $header{includes} = new Block("Includes", "", "");
    $header{nameSpaceWebCore} = new Block("Namespace WebCore", "\nnamespace WebCore {\n", "}\n");
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
    $header{root}->addHeader($headerTemplate . "\n");
    $header{root}->addHeader("#ifndef $defineName\n#define $defineName\n");
    $header{root}->addFooter("#endif // $defineName");

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
    my $conditional = shift;

    if (not $conditional) {
        $implIncludes{$header} = 1;
    } elsif (not exists($implIncludes{$header})) {
        $implIncludes{$header} = $conditional;
    } else {
        my $oldValue = $implIncludes{$header};
        if ($oldValue ne 1) {
            my %newValue = ();
            $newValue{$conditional} = 1;
            foreach my $condition (split(/\|/, $oldValue)) {
                $newValue{$condition} = 1;
            }
            $implIncludes{$header} = join("|", sort keys %newValue);
        }
    }
}

sub AddToHeaderIncludes
{
    my $header = shift;
    $headerIncludeFiles{$header} = 1;
}

sub AddInterfaceToImplIncludes
{
    my $interface = shift;
    my $include = HeaderFileForInterface($interface);

    AddToImplIncludes($include);
}

sub AddToHeader
{
    my $code = shift;
    push(@headerContent, $code);
}

sub AddToImplContentInternals
{
    my $code = shift;
    push(@implContentInternals, $code);
}

sub AddToImplContent
{
    my $code = shift;
    push(@implContent, $code);
}

sub AddIncludesForType
{
    my $type = shift;

    # When we're finished with the one-file-per-class
    # reorganization, we won't need these special cases.
    if (IsTypedArrayType($type)) {
        AddToImplIncludes("wtf/${type}.h");
    }
    if (!IsPrimitiveType($type) and $type ne "DOMString" and !SkipIncludeHeader($type) and $type ne "Date") {
        # default, include the same named file
        AddToImplIncludes(GetV8HeaderName(${type}));

        if ($type =~ /SVGPathSeg/) {
            my $joinedName = $type;
            $joinedName =~ s/Abs|Rel//;
            AddToImplIncludes("core/svg/${joinedName}.h");
        }
    }

    # additional includes (things needed to compile the bindings but not the header)

    if ($type eq "CanvasRenderingContext2D") {
        AddToImplIncludes("core/html/canvas/CanvasGradient.h");
        AddToImplIncludes("core/html/canvas/CanvasPattern.h");
        AddToImplIncludes("core/html/canvas/CanvasStyle.h");
    }

    if ($type eq "CanvasGradient" or $type eq "XPathNSResolver") {
        AddToImplIncludes("wtf/text/WTFString.h");
    }

    if ($type eq "CSSStyleSheet" or $type eq "StyleSheet") {
        AddToImplIncludes("core/css/CSSImportRule.h");
    }

    if ($type eq "CSSStyleDeclaration") {
        AddToImplIncludes("core/css/StylePropertySet.h");
    }

    if ($type eq "Plugin" or $type eq "PluginArray" or $type eq "MimeTypeArray") {
        # So we can get String -> AtomicString conversion for namedItem().
        AddToImplIncludes("wtf/text/AtomicString.h");
    }
}

sub NeedsCustomOpaqueRootForGC
{
    my $interface = shift;
    return GetGenerateIsReachable($interface) || GetCustomIsReachable($interface);
}

sub GetGenerateIsReachable
{
    my $interface = shift;
    return $interface->extendedAttributes->{"GenerateIsReachable"} || ""
}

sub GetCustomIsReachable
{
    my $interface = shift;
    return $interface->extendedAttributes->{"CustomIsReachable"};
}

sub GenerateOpaqueRootForGC
{
    my $interface = shift;
    my $interfaceName = $interface->name;

    if (GetCustomIsReachable($interface)) {
        return;
    }

    my $code = <<END;
void* V8${interfaceName}::opaqueRootForGC(void* object, v8::Persistent<v8::Object> wrapper, v8::Isolate* isolate)
{
    ASSERT(!wrapper.IsIndependent(isolate));
    ${interfaceName}* impl = static_cast<${interfaceName}*>(object);
END
    if (GetGenerateIsReachable($interface) eq  "ImplDocument" ||
        GetGenerateIsReachable($interface) eq  "ImplElementRoot" ||
        GetGenerateIsReachable($interface) eq  "ImplOwnerRoot" ||
        GetGenerateIsReachable($interface) eq  "ImplOwnerNodeRoot") {

        AddToImplIncludes("bindings/v8/V8GCController.h");

        my $methodName;
        $methodName = "document" if (GetGenerateIsReachable($interface) eq "ImplDocument");
        $methodName = "element" if (GetGenerateIsReachable($interface) eq "ImplElementRoot");
        $methodName = "owner" if (GetGenerateIsReachable($interface) eq "ImplOwnerRoot");
        $methodName = "ownerNode" if (GetGenerateIsReachable($interface) eq "ImplOwnerNodeRoot");

        $code .= <<END;
    if (Node* owner = impl->${methodName}())
        return V8GCController::opaqueRootForGC(owner, isolate);
END
    }

    $code .= <<END;
    return object;
}

END
    AddToImplContent($code);
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
        AddToImplIncludes("core/svg/properties/SVGAnimatedPropertyTearOff.h");
    } elsif ($svgNativeType =~ /SVGListPropertyTearOff/ or $svgNativeType =~ /SVGStaticListPropertyTearOff/) {
        $svgListPropertyType = $svgWrappedNativeType;
        AddToHeaderIncludes("core/svg/properties/SVGAnimatedListPropertyTearOff.h");
        AddToHeaderIncludes("core/svg/properties/SVGStaticListPropertyTearOff.h");
    } elsif ($svgNativeType =~ /SVGTransformListPropertyTearOff/) {
        $svgListPropertyType = $svgWrappedNativeType;
        AddToHeaderIncludes("core/svg/properties/SVGAnimatedListPropertyTearOff.h");
        AddToHeaderIncludes("core/svg/properties/SVGTransformListPropertyTearOff.h");
    } elsif ($svgNativeType =~ /SVGPathSegListPropertyTearOff/) {
        $svgListPropertyType = $svgWrappedNativeType;
        AddToHeaderIncludes("core/svg/properties/SVGPathSegListPropertyTearOff.h");
    }

    if ($svgPropertyType) {
        $svgPropertyType = "SVGPoint" if $svgPropertyType eq "FloatPoint";
    }

    return ($svgPropertyType, $svgListPropertyType, $svgNativeType);
}

sub MakeDummyFunction
{
    my $returnType = shift;
    my $methodName = shift;
    my $function = domFunction->new();
    $function->signature(domSignature->new());
    $function->signature->type($returnType);
    $function->signature->name($methodName);
    $function->signature->extendedAttributes({});
    return $function;
}

sub GetIndexedGetterFunction
{
    my $interface = shift;

    # FIXME: Expose indexed getter of WebKitCSSMixFunctionValue by removing this special case
    # because CSSValueList(which is parent of WebKitCSSMixFunctionValue) has indexed property getter.
    if ($interface->name eq "WebKitCSSMixFunctionValue") {
        return 0;
    }

    # FIXME: add getter method to WebKitCSSKeyframesRule.idl or remove special case of WebKitCSSKeyframesRule.idl
    # Currently return type and method name is hard coded because it can not be obtained from IDL.
    if ($interface->name eq "WebKitCSSKeyframesRule") {
        return MakeDummyFunction("WebKitCSSKeyframeRule", "item")
    }

    return GetSpecialGetterFunctionForType($interface, "unsigned long");
}

sub GetNamedGetterFunction
{
    my $interface = shift;

    return GetSpecialGetterFunctionForType($interface, "DOMString");
}

sub GetSpecialGetterFunctionForType
{
    my $interface = shift;
    my $type = shift;

    my @interfaces = ($interface);
    ForAllParents($interface, sub {
        my $currentInterface = shift;
        push(@interfaces, $currentInterface);
    });

    foreach my $currentInterface (@interfaces) {
        foreach my $function (@{$currentInterface->functions}) {
            my $specials = $function->signature->specials;
            my $getterExists = grep { $_ eq "getter" } @$specials;
            my $parameters = $function->parameters;
            if ($getterExists and scalar(@$parameters) == 1 and $parameters->[0]->type eq $type ) {
                return $function;
            }
        }
    }

    return 0;
}

sub GenerateHeader
{
    my $object = shift;
    my $interface = shift;

    my $interfaceName = $interface->name;
    my $v8InterfaceName = "V8$interfaceName";

    # Copy contents of parent interfaces except the first parent.
    my @parents;
    AddMethodsConstantsAndAttributesFromParentInterfaces($interface, \@parents, 1);
    LinkOverloadedFunctions($interface);

    # Ensure the IsDOMNodeType function is in sync.
    die("IsDOMNodeType is out of date with respect to $interfaceName") if IsDOMNodeType($interfaceName) != InheritsInterface($interface, "Node");

    my $hasDependentLifetime = $interface->extendedAttributes->{"DependentLifetime"} || InheritsExtendedAttribute($interface, "ActiveDOMObject") || GetGenerateIsReachable($interface) || $v8InterfaceName =~ /SVG/;
    if (!$hasDependentLifetime) {
        foreach (@{$interface->parents}) {
            my $parent = $_;
            AddToHeaderIncludes("V8${parent}.h");
        }
    }

    AddToHeaderIncludes("bindings/v8/WrapperTypeInfo.h");
    AddToHeaderIncludes("bindings/v8/V8Binding.h");
    AddToHeaderIncludes("bindings/v8/V8DOMWrapper.h");
    AddToHeaderIncludes("v8.h");
    AddToHeaderIncludes("wtf/HashMap.h");
    AddToHeaderIncludes("wtf/text/StringHash.h");

    my $headerClassInclude = GetHeaderClassInclude($interfaceName);
    AddToHeaderIncludes($headerClassInclude) if $headerClassInclude;

    my ($svgPropertyType, $svgListPropertyType, $svgNativeType) = GetSVGPropertyTypes($interfaceName);

    foreach my $headerInclude (sort keys(%headerIncludeFiles)) {
        if ($headerInclude =~ /wtf|v8\.h/) {
            $header{includes}->add("#include \<${headerInclude}\>\n");
        } else {
            $header{includes}->add("#include \"${headerInclude}\"\n");
        }
    }

    $header{nameSpaceWebCore}->addHeader("\ntemplate<typename PropertyType> class SVGPropertyTearOff;\n") if $svgPropertyType;
    if ($svgNativeType) {
        if ($svgNativeType =~ /SVGStaticListPropertyTearOff/) {
            $header{nameSpaceWebCore}->addHeader("\ntemplate<typename PropertyType> class SVGStaticListPropertyTearOff;\n");
        } else {
            $header{nameSpaceWebCore}->addHeader("\ntemplate<typename PropertyType> class SVGListPropertyTearOff;\n");
        }
    }

    $header{nameSpaceWebCore}->addHeader("class FloatRect;\n") if $svgPropertyType && $svgPropertyType eq "FloatRect";
    $header{nameSpaceWebCore}->addHeader("\nclass Dictionary;") if IsConstructorTemplate($interface, "Event");

    my $nativeType = GetNativeTypeForConversions($interface);
    if ($interface->extendedAttributes->{"NamedConstructor"}) {
        $header{nameSpaceWebCore}->addHeader(<<END);

class V8${nativeType}Constructor {
public:
    static v8::Persistent<v8::FunctionTemplate> GetTemplate(v8::Isolate*, WrapperWorldType);
    static WrapperTypeInfo info;
};
END
    }

    $header{class}->addHeader("class $v8InterfaceName {");
    $header{class}->addFooter("};");

    my $code = "";
    $code .= "    static const bool hasDependentLifetime = ";
    if ($hasDependentLifetime) {
        $code .= "true;\n";
    } elsif (@{$interface->parents}) {
        # Even if this type doesn't have the [DependentLifetime] attribute its parents may.
        # Let the compiler statically determine this for us.
        my $separator = "";
        foreach (@{$interface->parents}) {
            my $parent = $_;
            AddToHeaderIncludes("V8${parent}.h");
            $code .= "${separator}V8${parent}::hasDependentLifetime";
            $separator = " || ";
        }
        $code .= ";\n";
    } else {
        $code .= "false;\n";
    }
    $header{classPublic}->add($code);

    my $fromFunctionOpening = "";
    my $fromFunctionClosing = "";
    if ($interface->extendedAttributes->{"WrapAsFunction"}) {
        $fromFunctionOpening = "V8DOMWrapper::fromFunction(";
        $fromFunctionClosing = ")";
    }

    $header{classPublic}->add(<<END);
    static bool HasInstance(v8::Handle<v8::Value>, v8::Isolate*, WrapperWorldType);
    static bool HasInstanceInAnyWorld(v8::Handle<v8::Value>, v8::Isolate*);
    static v8::Persistent<v8::FunctionTemplate> GetTemplate(v8::Isolate*, WrapperWorldType);
    static ${nativeType}* toNative(v8::Handle<v8::Object> object)
    {
        return reinterpret_cast<${nativeType}*>(${fromFunctionOpening}object${fromFunctionClosing}->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex));
    }
    static void derefObject(void*);
    static WrapperTypeInfo info;
END

    if (NeedsCustomOpaqueRootForGC($interface)) {
        $header{classPublic}->add("    static void* opaqueRootForGC(void*, v8::Persistent<v8::Object>, v8::Isolate*);\n");
    }

    if (InheritsExtendedAttribute($interface, "ActiveDOMObject")) {
        $header{classPublic}->add("    static ActiveDOMObject* toActiveDOMObject(v8::Handle<v8::Object>);\n");
    }

    if (InheritsExtendedAttribute($interface, "EventTarget")) {
        $header{classPublic}->add("    static EventTarget* toEventTarget(v8::Handle<v8::Object>);\n");
    }

    if ($interfaceName eq "DOMWindow") {
        $header{classPublic}->add(<<END);
    static v8::Persistent<v8::ObjectTemplate> GetShadowObjectTemplate(v8::Isolate*, WrapperWorldType);
END
    }

    if ($interfaceName eq "HTMLDocument") {
      $header{classPublic}->add(<<END);
    static v8::Local<v8::Object> wrapInShadowObject(v8::Local<v8::Object> wrapper, Node* impl, v8::Isolate*);
END
    }

    my @enabledPerContextFunctions;
    foreach my $function (@{$interface->functions}) {
        my $name = $function->signature->name;
        my $attrExt = $function->signature->extendedAttributes;

        if (HasCustomMethod($attrExt) && !$attrExt->{"ImplementedBy"} && $function->{overloadIndex} == 1) {
            my $conditionalString = GenerateConditionalString($function->signature);
            $header{classPublic}->add("#if ${conditionalString}\n") if $conditionalString;
            $header{classPublic}->add(<<END);
    static v8::Handle<v8::Value> ${name}MethodCustom(const v8::Arguments&);
END
            $header{classPublic}->add("#endif // ${conditionalString}\n") if $conditionalString;
        }
        if ($attrExt->{"EnabledPerContext"}) {
            push(@enabledPerContextFunctions, $function);
        }
    }

    if (IsConstructable($interface)) {
        $header{classPublic}->add("    static v8::Handle<v8::Value> constructorCallback(const v8::Arguments&);\n");
END
    }
    if (HasCustomConstructor($interface)) {
        $header{classPublic}->add("    static v8::Handle<v8::Value> constructorCustom(const v8::Arguments&);\n");
    }

    my @enabledPerContextAttributes;
    foreach my $attribute (@{$interface->attributes}) {
        my $name = $attribute->signature->name;
        my $attrExt = $attribute->signature->extendedAttributes;
        my $conditionalString = GenerateConditionalString($attribute->signature);
        if (HasCustomGetter($attrExt) && !$attrExt->{"ImplementedBy"}) {
            $header{classPublic}->add("#if ${conditionalString}\n") if $conditionalString;
            $header{classPublic}->add(<<END);
    static v8::Handle<v8::Value> ${name}AttrGetterCustom(v8::Local<v8::String> name, const v8::AccessorInfo&);
END
            $header{classPublic}->add("#endif // ${conditionalString}\n") if $conditionalString;
        }
        if (HasCustomSetter($attrExt) && !$attrExt->{"ImplementedBy"}) {
            $header{classPublic}->add("#if ${conditionalString}\n") if $conditionalString;
            $header{classPublic}->add(<<END);
    static void ${name}AttrSetterCustom(v8::Local<v8::String> name, v8::Local<v8::Value>, const v8::AccessorInfo&);
END
            $header{classPublic}->add("#endif // ${conditionalString}\n") if $conditionalString;
        }
        if ($attrExt->{"EnabledPerContext"}) {
            push(@enabledPerContextAttributes, $attribute);
        }
    }

    GenerateHeaderNamedAndIndexedPropertyAccessors($interface);
    GenerateHeaderCustomCall($interface);
    GenerateHeaderCustomInternalFieldIndices($interface);

    if ($interface->name eq "DOMWindow") {
        $header{classPublic}->add(<<END);
    static bool namedSecurityCheckCustom(v8::Local<v8::Object> host, v8::Local<v8::Value> key, v8::AccessType, v8::Local<v8::Value> data);
    static bool indexedSecurityCheckCustom(v8::Local<v8::Object> host, uint32_t index, v8::AccessType, v8::Local<v8::Value> data);
END
    }

    if (@enabledPerContextAttributes) {
        $header{classPublic}->add(<<END);
    static void installPerContextProperties(v8::Handle<v8::Object>, ${nativeType}*, v8::Isolate*);
END
    } else {
        $header{classPublic}->add(<<END);
    static void installPerContextProperties(v8::Handle<v8::Object>, ${nativeType}*, v8::Isolate*) { }
END
    }

    if (@enabledPerContextFunctions) {
        $header{classPublic}->add(<<END);
    static void installPerContextPrototypeProperties(v8::Handle<v8::Object>, v8::Isolate*);
END
    } else {
        $header{classPublic}->add(<<END);
    static void installPerContextPrototypeProperties(v8::Handle<v8::Object>, v8::Isolate*) { }
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

    my $noToV8 = $interface->extendedAttributes->{"DoNotGenerateToV8"};
    my $noWrap = $interface->extendedAttributes->{"DoNotGenerateWrap"} || $noToV8;
    if (!$noWrap) {
        my $createWrapperArgumentType = GetPassRefPtrType($nativeType);
        $header{classPrivate}->add(<<END);
    friend v8::Handle<v8::Object> wrap(${nativeType}*, v8::Handle<v8::Object> creationContext, v8::Isolate*);
    static v8::Handle<v8::Object> createWrapper(${createWrapperArgumentType}, v8::Handle<v8::Object> creationContext, v8::Isolate*);
END
    }

    $header{nameSpaceWebCore}->add(<<END);

template<>
class WrapperTypeTraits<${nativeType} > {
public:
    static WrapperTypeInfo* info() { return &${v8InterfaceName}::info; }
};

END

    my $customWrap = $interface->extendedAttributes->{"CustomToV8"};
    if ($noToV8) {
        die "Can't suppress toV8 for subclass\n" if @parents;
    } elsif ($noWrap) {
        die "Must have custom toV8\n" if !$customWrap;
        $header{nameSpaceWebCore}->add(<<END);
class ${nativeType};
v8::Handle<v8::Value> toV8(${nativeType}*, v8::Handle<v8::Object> creationContext, v8::Isolate*);
v8::Handle<v8::Value> toV8ForMainWorld(${nativeType}*, v8::Handle<v8::Object> creationContext, v8::Isolate*);

template<class HolderContainer, class Wrappable>
inline v8::Handle<v8::Value> toV8Fast(${nativeType}* impl, const HolderContainer& container, Wrappable*)
{
    return toV8(impl, container.Holder(), container.GetIsolate());
}

template<class HolderContainer, class Wrappable>
inline v8::Handle<v8::Value> toV8FastForMainWorld(${nativeType}* impl, const HolderContainer& container, Wrappable*)
{
    return toV8ForMainWorld(impl, container.Holder(), container.GetIsolate());
}
END
    } else {

        my $createWrapperCall = $customWrap ? "${v8InterfaceName}::wrap" : "${v8InterfaceName}::createWrapper";
        my $returningWrapper = $interface->extendedAttributes->{"WrapAsFunction"} ? "V8DOMWrapper::toFunction(wrapper)" : "wrapper";
        my $returningCreatedWrapperOpening = $interface->extendedAttributes->{"WrapAsFunction"} ? "V8DOMWrapper::toFunction(" : "";
        my $returningCreatedWrapperClosing = $interface->extendedAttributes->{"WrapAsFunction"} ? ", \"${interfaceName}\", isolate)" : "";

        if ($customWrap) {
            $header{nameSpaceWebCore}->add(<<END);

v8::Handle<v8::Object> wrap(${nativeType}* impl, v8::Handle<v8::Object> creationContext, v8::Isolate*);
END
        } else {
            $header{nameSpaceWebCore}->add(<<END);

inline v8::Handle<v8::Object> wrap(${nativeType}* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    ASSERT(DOMDataStore::getWrapper(impl, isolate).IsEmpty());
    return ${returningCreatedWrapperOpening}$createWrapperCall(impl, creationContext, isolate)${returningCreatedWrapperClosing};
}
END
        }

        $header{nameSpaceWebCore}->add(<<END);

inline v8::Handle<v8::Value> toV8(${nativeType}* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (UNLIKELY(!impl))
        return v8NullWithCheck(isolate);
    v8::Handle<v8::Value> wrapper = DOMDataStore::getWrapper(impl, isolate);
    if (!wrapper.IsEmpty())
        return $returningWrapper;
    return wrap(impl, creationContext, isolate);
}

inline v8::Handle<v8::Value> toV8ForMainWorld(${nativeType}* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(worldType(isolate) == MainWorld);
    if (UNLIKELY(!impl))
        return v8NullWithCheck(isolate);
    v8::Handle<v8::Value> wrapper = DOMDataStore::getWrapperForMainWorld(impl);
    if (!wrapper.IsEmpty())
        return $returningWrapper;
    return wrap(impl, creationContext, isolate);
}

template<class HolderContainer, class Wrappable>
inline v8::Handle<v8::Value> toV8Fast(${nativeType}* impl, const HolderContainer& container, Wrappable* wrappable)
{
    if (UNLIKELY(!impl))
        return v8Null(container.GetIsolate());
    v8::Handle<v8::Object> wrapper = DOMDataStore::getWrapperFast(impl, container, wrappable);
    if (!wrapper.IsEmpty())
        return $returningWrapper;
    return wrap(impl, container.Holder(), container.GetIsolate());
}

template<class HolderContainer, class Wrappable>
inline v8::Handle<v8::Value> toV8FastForMainWorld(${nativeType}* impl, const HolderContainer& container, Wrappable* wrappable)
{
    ASSERT(worldType(container.GetIsolate()) == MainWorld);
    if (UNLIKELY(!impl))
        return v8Null(container.GetIsolate());
    v8::Handle<v8::Object> wrapper = DOMDataStore::getWrapperForMainWorld(impl);
    if (!wrapper.IsEmpty())
        return $returningWrapper;
    return wrap(impl, container.Holder(), container.GetIsolate());
}

template<class HolderContainer, class Wrappable>
inline v8::Handle<v8::Value> toV8FastForMainWorld(PassRefPtr< ${nativeType} > impl, const HolderContainer& container, Wrappable* wrappable)
{
    return toV8FastForMainWorld(impl.get(), container, wrappable);
}

END
    }

    $header{nameSpaceWebCore}->add(<<END);

template<class HolderContainer, class Wrappable>
inline v8::Handle<v8::Value> toV8Fast(PassRefPtr< ${nativeType} > impl, const HolderContainer& container, Wrappable* wrappable)
{
    return toV8Fast(impl.get(), container, wrappable);
}

inline v8::Handle<v8::Value> toV8(PassRefPtr< ${nativeType} > impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8(impl.get(), creationContext, isolate);
}

END

    if (IsConstructorTemplate($interface, "Event")) {
        $header{nameSpaceWebCore}->add("bool fill${interfaceName}Init(${interfaceName}Init&, const Dictionary&);\n\n");
    }
}

sub GetInternalFields
{
    my $interface = shift;

    my @customInternalFields = ();
    # Event listeners on DOM nodes are explicitly supported in the GC controller.
    if (!InheritsInterface($interface, "Node") &&
        InheritsExtendedAttribute($interface, "EventTarget")) {
        push(@customInternalFields, "eventListenerCacheIndex");
    }
    return @customInternalFields;
}

sub GetHeaderClassInclude
{
    my $v8InterfaceName = shift;
    if ($v8InterfaceName =~ /SVGPathSeg/) {
        $v8InterfaceName =~ s/Abs|Rel//;
        return "core/svg/${v8InterfaceName}.h";
    }
    return "wtf/${v8InterfaceName}.h" if IsTypedArrayType($v8InterfaceName);
    return "" if (SkipIncludeHeader($v8InterfaceName));
    return HeaderFileForInterface($v8InterfaceName);
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
    my $interfaceName = $interface->name;
    my $hasIndexedGetter = GetIndexedGetterFunction($interface) || $interface->extendedAttributes->{"CustomIndexedGetter"};
    my $hasCustomIndexedSetter = $interface->extendedAttributes->{"CustomIndexedSetter"};
    my $hasCustomNamedGetter = GetNamedGetterFunction($interface) || $interface->extendedAttributes->{"CustomNamedGetter"};
    my $hasCustomNamedSetter = $interface->extendedAttributes->{"CustomNamedSetter"};
    my $hasCustomDeleters = $interface->extendedAttributes->{"CustomDeleteProperty"};
    my $hasCustomEnumerator = $interface->extendedAttributes->{"CustomEnumerateProperty"};

    if ($hasIndexedGetter) {
        $header{classPublic}->add(<<END);
    static v8::Handle<v8::Value> indexedPropertyGetter(uint32_t, const v8::AccessorInfo&);
END
    }

    if ($hasCustomIndexedSetter) {
        $header{classPublic}->add(<<END);
    static v8::Handle<v8::Value> indexedPropertySetter(uint32_t, v8::Local<v8::Value>, const v8::AccessorInfo&);
END
    }
    if ($hasCustomDeleters) {
        $header{classPublic}->add(<<END);
    static v8::Handle<v8::Boolean> indexedPropertyDeleter(uint32_t, const v8::AccessorInfo&);
END
    }
    if ($hasCustomNamedGetter) {
        $header{classPublic}->add(<<END);
    static v8::Handle<v8::Value> namedPropertyGetter(v8::Local<v8::String>, const v8::AccessorInfo&);
END
    }
    if ($hasCustomNamedSetter) {
        $header{classPublic}->add(<<END);
    static v8::Handle<v8::Value> namedPropertySetter(v8::Local<v8::String>, v8::Local<v8::Value>, const v8::AccessorInfo&);
END
    }
    if ($hasCustomDeleters) {
        $header{classPublic}->add(<<END);
    static v8::Handle<v8::Boolean> namedPropertyDeleter(v8::Local<v8::String>, const v8::AccessorInfo&);
END
    }
    if ($hasCustomEnumerator) {
        $header{classPublic}->add(<<END);
    static v8::Handle<v8::Array> namedPropertyEnumerator(const v8::AccessorInfo&);
    static v8::Handle<v8::Integer> namedPropertyQuery(v8::Local<v8::String>, const v8::AccessorInfo&);
END
    }
}

sub GenerateHeaderCustomCall
{
    my $interface = shift;

    if ($interface->extendedAttributes->{"CustomCall"}) {
        $header{classPublic}->add("    static v8::Handle<v8::Value> callAsFunctionCallback(const v8::Arguments&);\n");
    }
    if ($interface->name eq "Location") {
        $header{classPublic}->add("    static v8::Handle<v8::Value> assignAttrGetterCustom(v8::Local<v8::String> name, const v8::AccessorInfo&);\n");
        $header{classPublic}->add("    static v8::Handle<v8::Value> reloadAttrGetterCustom(v8::Local<v8::String> name, const v8::AccessorInfo&);\n");
        $header{classPublic}->add("    static v8::Handle<v8::Value> replaceAttrGetterCustom(v8::Local<v8::String> name, const v8::AccessorInfo&);\n");
    }
}

sub HasActivityLogging
{
    my $forMainWorldSuffix = shift;
    my $attrExt = shift;
    my $access = shift;

    if (!$attrExt->{"ActivityLog"}) {
        return 0;
    }
    my $logAllAccess = ($attrExt->{"ActivityLog"} =~ /^Access/);
    my $logGetter = ($attrExt->{"ActivityLog"} =~ /^Getter/);
    my $logSetter = ($attrExt->{"ActivityLog"} =~ /^Setter/);
    my $logOnlyIsolatedWorlds = ($attrExt->{"ActivityLog"} =~ /ForIsolatedWorlds$/);

    if ($logOnlyIsolatedWorlds && $forMainWorldSuffix eq "ForMainWorld") {
        return 0;
    }
    return $logAllAccess || ($logGetter && $access eq "Getter") || ($logSetter && $access eq "Setter");
}

sub IsConstructable
{
    my $interface = shift;

    return $interface->extendedAttributes->{"CustomConstructor"} || $interface->extendedAttributes->{"Constructor"} || $interface->extendedAttributes->{"ConstructorTemplate"};
}

sub HasCustomConstructor
{
    my $interface = shift;

    return $interface->extendedAttributes->{"CustomConstructor"};
}

sub HasCustomGetter
{
    my $attrExt = shift;
    return $attrExt->{"Custom"} || $attrExt->{"CustomGetter"};
}

sub HasCustomSetter
{
    my $attrExt = shift;
    return $attrExt->{"Custom"} || $attrExt->{"CustomSetter"};
}

sub HasCustomMethod
{
    my $attrExt = shift;
    return $attrExt->{"Custom"};
}

sub IsReadonly
{
    my $attribute = shift;
    my $attrExt = $attribute->signature->extendedAttributes;
    return $attribute->isReadOnly && !$attrExt->{"Replaceable"};
}

sub GenerateDomainSafeFunctionGetter
{
    my $function = shift;
    my $interfaceName = shift;

    my $v8InterfaceName = "V8" . $interfaceName;
    my $funcName = $function->signature->name;

    my $functionLength = GetFunctionLength($function);
    my $signature = "v8::Signature::New(V8PerIsolateData::from(info.GetIsolate())->rawTemplate(&" . $v8InterfaceName . "::info, currentWorldType))";
    if ($function->signature->extendedAttributes->{"DoNotCheckSignature"}) {
        $signature = "v8::Local<v8::Signature>()";
    }

    my $newTemplateParams = "${interfaceName}V8Internal::${funcName}MethodCallback, v8Undefined(), $signature";

    AddToImplIncludes("core/page/Frame.h");
    AddToImplContentInternals(<<END);
static v8::Handle<v8::Value> ${funcName}AttrGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    // This is only for getting a unique pointer which we can pass to privateTemplate.
    static const char* privateTemplateUniqueKey = "${funcName}PrivateTemplate";
    WrapperWorldType currentWorldType = worldType(info.GetIsolate());
    V8PerIsolateData* data = V8PerIsolateData::from(info.GetIsolate());
    v8::Persistent<v8::FunctionTemplate> privateTemplate = data->privateTemplate(currentWorldType, &privateTemplateUniqueKey, $newTemplateParams, $functionLength);

    v8::Handle<v8::Object> holder = info.This()->FindInstanceInPrototypeChain(${v8InterfaceName}::GetTemplate(info.GetIsolate(), currentWorldType));
    if (holder.IsEmpty()) {
        // can only reach here by 'object.__proto__.func', and it should passed
        // domain security check already
        return privateTemplate->GetFunction();
    }
    ${interfaceName}* imp = ${v8InterfaceName}::toNative(holder);
    if (!BindingSecurity::shouldAllowAccessToFrame(imp->frame(), DoNotReportSecurityError)) {
        static const char* sharedTemplateUniqueKey = "${funcName}SharedTemplate";
        v8::Persistent<v8::FunctionTemplate> sharedTemplate = data->privateTemplate(currentWorldType, &sharedTemplateUniqueKey, $newTemplateParams, $functionLength);
        return sharedTemplate->GetFunction();
    }

    v8::Local<v8::Value> hiddenValue = info.This()->GetHiddenValue(name);
    if (!hiddenValue.IsEmpty())
        return hiddenValue;

    return privateTemplate->GetFunction();
}

END
    AddToImplContentInternals(<<END);
static v8::Handle<v8::Value> ${funcName}AttrGetterCallback(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    return ${interfaceName}V8Internal::${funcName}AttrGetter(name, info);
}

END
}

sub GenerateDomainSafeFunctionSetter
{
    my $interfaceName = shift;
    my $v8InterfaceName = "V8" . $interfaceName;

    AddToImplContentInternals(<<END);
static void ${interfaceName}DomainSafeFunctionSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    v8::Handle<v8::Object> holder = info.This()->FindInstanceInPrototypeChain(${v8InterfaceName}::GetTemplate(info.GetIsolate(), worldType(info.GetIsolate())));
    if (holder.IsEmpty())
        return;
    ${interfaceName}* imp = ${v8InterfaceName}::toNative(holder);
    if (!BindingSecurity::shouldAllowAccessToFrame(imp->frame()))
        return;

    info.This()->SetHiddenValue(name, value);
}

END
}

sub GenerateConstructorGetter
{
    my $interface = shift;
    my $interfaceName = $interface->name;

    AddToImplContentInternals(<<END);
static v8::Handle<v8::Value> ${interfaceName}ConstructorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    v8::Handle<v8::Value> data = info.Data();
    ASSERT(data->IsExternal());
    V8PerContextData* perContextData = V8PerContextData::from(info.Holder()->CreationContext());
    if (!perContextData)
        return v8Undefined();
    return perContextData->constructorForType(WrapperTypeInfo::unwrap(data));
}
END
}

sub GenerateFeatureObservation
{
    my $measureAs = shift;

    if ($measureAs) {
        AddToImplIncludes("core/page/UseCounter.h");
        return "    UseCounter::count(activeDOMWindow(), UseCounter::${measureAs});\n";
    }

    return "";
}

sub GenerateDeprecationNotification
{
    my $deprecateAs = shift;
    if ($deprecateAs) {
        AddToImplIncludes("core/page/PageConsole.h");
        return "    UseCounter::countDeprecation(activeDOMWindow(), UseCounter::${deprecateAs});\n";
    }
    return "";
}

sub GenerateActivityLogging
{
    my $accessType = shift;
    my $interface = shift;
    my $propertyName = shift;

    my $visibleInterfaceName = GetVisibleInterfaceName($interface);

    AddToImplIncludes("bindings/v8/V8Binding.h");
    AddToImplIncludes("bindings/v8/V8DOMActivityLogger.h");
    AddToImplIncludes("wtf/Vector.h");

    my $code = "";
    if ($accessType eq "Method") {
        $code .= <<END;
    V8PerContextData* contextData = V8PerContextData::from(args.GetIsolate()->GetCurrentContext());
    if (contextData && contextData->activityLogger()) {
        Vector<v8::Handle<v8::Value> > loggerArgs = toVectorOfArguments(args);
        contextData->activityLogger()->log("${visibleInterfaceName}.${propertyName}", args.Length(), loggerArgs.data(), "${accessType}");
    }
END
    } elsif ($accessType eq "Setter") {
        $code .= <<END;
    V8PerContextData* contextData = V8PerContextData::from(info.GetIsolate()->GetCurrentContext());
    if (contextData && contextData->activityLogger()) {
        v8::Handle<v8::Value> loggerArg[] = { value };
        contextData->activityLogger()->log("${visibleInterfaceName}.${propertyName}", 1, &loggerArg[0], "${accessType}");
    }
END
    } elsif ($accessType eq "Getter") {
        $code .= <<END;
    V8PerContextData* contextData = V8PerContextData::from(info.GetIsolate()->GetCurrentContext());
    if (contextData && contextData->activityLogger())
        contextData->activityLogger()->log("${visibleInterfaceName}.${propertyName}", 0, 0, "${accessType}");
END
    } else {
        die "Unrecognized activity logging access type";
    }

    return $code;
}

sub GenerateNormalAttrGetterCallback
{
    my $attribute = shift;
    my $interface = shift;
    my $forMainWorldSuffix = shift;

    my $interfaceName = $interface->name;
    my $v8InterfaceName = "V8$interfaceName";
    my $attrExt = $attribute->signature->extendedAttributes;
    my $attrName = $attribute->signature->name;

    my $conditionalString = GenerateConditionalString($attribute->signature);
    my $code = "";
    $code .= "#if ${conditionalString}\n\n" if $conditionalString;

    $code .= "static v8::Handle<v8::Value> ${attrName}AttrGetterCallback${forMainWorldSuffix}(v8::Local<v8::String> name, const v8::AccessorInfo& info)\n";
    $code .= "{\n";
    $code .= GenerateFeatureObservation($attrExt->{"MeasureAs"});
    $code .= GenerateDeprecationNotification($attrExt->{"DeprecateAs"});
    if (HasActivityLogging($forMainWorldSuffix, $attrExt, "Getter")) {
        $code .= GenerateActivityLogging("Getter", $interface, "${attrName}");
    }
    if (HasCustomGetter($attrExt)) {
        $code .= "    return ${v8InterfaceName}::${attrName}AttrGetterCustom(name, info);\n";
    } else {
        $code .= "    return ${interfaceName}V8Internal::${attrName}AttrGetter${forMainWorldSuffix}(name, info);\n";
    }
    $code .= "}\n\n";
    $code .= "#endif // ${conditionalString}\n\n" if $conditionalString;

    AddToImplContentInternals($code);
}

sub GenerateNormalAttrGetter
{
    my $attribute = shift;
    my $interface = shift;
    my $forMainWorldSuffix = shift;

    my $interfaceName = $interface->name;
    my $v8InterfaceName = "V8$interfaceName";
    my $attrExt = $attribute->signature->extendedAttributes;
    my $attrName = $attribute->signature->name;
    my $attrType = $attribute->signature->type;

    if (HasCustomGetter($attrExt)) {
        return;
    }

    AssertNotSequenceType($attrType);
    my $getterStringUsesImp = $interfaceName ne "SVGNumber";
    my $nativeType = GetNativeTypeFromSignature($attribute->signature, -1);
    my $svgNativeType = GetSVGTypeNeedingTearOff($interfaceName);

    my $conditionalString = GenerateConditionalString($attribute->signature);
    my $code = "";
    $code .= "#if ${conditionalString}\n\n" if $conditionalString;
    $code .= <<END;
static v8::Handle<v8::Value> ${attrName}AttrGetter${forMainWorldSuffix}(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
END
    if ($svgNativeType) {
        my $svgWrappedNativeType = GetSVGWrappedTypeNeedingTearOff($interfaceName);
        if ($svgWrappedNativeType =~ /List/) {
            $code .= <<END;
    $svgNativeType* imp = ${v8InterfaceName}::toNative(info.Holder());
END
        } else {
            $code .= <<END;
    $svgNativeType* wrapper = ${v8InterfaceName}::toNative(info.Holder());
    $svgWrappedNativeType& impInstance = wrapper->propertyReference();
END
            if ($getterStringUsesImp) {
                $code .= <<END;
    $svgWrappedNativeType* imp = &impInstance;
END
            }
        }
    } elsif ($attrExt->{"OnProto"} || $attrExt->{"Unforgeable"}) {
        if ($interfaceName eq "DOMWindow") {
            $code .= <<END;
    v8::Handle<v8::Object> holder = info.Holder();
END
        } else {
            # perform lookup first
            $code .= <<END;
    v8::Handle<v8::Object> holder = info.This()->FindInstanceInPrototypeChain(${v8InterfaceName}::GetTemplate(info.GetIsolate(), worldType(info.GetIsolate())));
    if (holder.IsEmpty())
        return v8Undefined();
END
        }
        $code .= <<END;
    ${interfaceName}* imp = ${v8InterfaceName}::toNative(holder);
END
    } else {
        my $reflect = $attribute->signature->extendedAttributes->{"Reflect"};
        my $url = $attribute->signature->extendedAttributes->{"URL"};
        if ($getterStringUsesImp && $reflect && !$url && InheritsInterface($interface, "Node") && $attrType eq "DOMString") {
            # Generate super-compact call for regular attribute getter:
            my ($functionName, @arguments) = GetterExpression(\%implIncludes, $interfaceName, $attribute);
            $code .= "    Element* imp = V8Element::toNative(info.Holder());\n";
            $code .= "    return v8String(imp->${functionName}(" . join(", ", @arguments) . "), info.GetIsolate(), ReturnUnsafeHandle);\n";
            $code .= "}\n\n";
            $code .= "#endif // ${conditionalString}\n\n" if $conditionalString;
            AddToImplContentInternals($code);
            return;
            # Skip the rest of the function!
        }
        if ($attribute->signature->type eq "SerializedScriptValue" && $attrExt->{"CachedAttribute"}) {
            $code .= <<END;
    v8::Handle<v8::String> propertyName = v8::String::NewSymbol("${attrName}");
    v8::Handle<v8::Value> value = info.Holder()->GetHiddenValue(propertyName);
    if (!value.IsEmpty())
        return value;
END
        }
        if (!$attribute->isStatic) {
            $code .= <<END;
    ${interfaceName}* imp = ${v8InterfaceName}::toNative(info.Holder());
END
        }
    }

    # Generate security checks if necessary
    if ($attribute->signature->extendedAttributes->{"CheckSecurityForNode"}) {
        $code .= "    if (!BindingSecurity::shouldAllowAccessToNode(imp->" . $attribute->signature->name . "()))\n        return v8::Handle<v8::Value>(v8Null(info.GetIsolate()));\n\n";
    }

    my $useExceptions = 1 if $attribute->signature->extendedAttributes->{"GetterRaisesException"};
    my $isNullable = $attribute->signature->isNullable;
    if ($useExceptions) {
        AddToImplIncludes("core/dom/ExceptionCode.h");
        $code .= "    ExceptionCode ec = 0;\n";
    }

    if ($isNullable) {
        $code .= "    bool isNull = false;\n";
    }

    my $returnType = $attribute->signature->type;
    my $getterString;

    if ($getterStringUsesImp) {
        my ($functionName, @arguments) = GetterExpression(\%implIncludes, $interfaceName, $attribute);
        push(@arguments, "isNull") if $isNullable;
        push(@arguments, "ec") if $useExceptions;
        if ($attribute->signature->extendedAttributes->{"ImplementedBy"}) {
            my $implementedBy = $attribute->signature->extendedAttributes->{"ImplementedBy"};
            AddInterfaceToImplIncludes($implementedBy);
            unshift(@arguments, "imp") if !$attribute->isStatic;
            $functionName = "${implementedBy}::${functionName}";
        } elsif ($attribute->isStatic) {
            $functionName = "${interfaceName}::${functionName}";
        } else {
            $functionName = "imp->${functionName}";
        }
        my ($arg, $subCode) = GenerateCallWith($attribute->signature->extendedAttributes->{"CallWith"}, "    ", 0);
        $code .= $subCode;
        unshift(@arguments, @$arg);
        $getterString = "${functionName}(" . join(", ", @arguments) . ")";
    } else {
        $getterString = "impInstance";
    }

    my $expression;
    if ($attribute->signature->type eq "EventListener" && $interface->name eq "DOMWindow") {
        $code .= "    if (!imp->document())\n";
        $code .= "        return v8Undefined();\n";
    }

    if ($useExceptions || $isNullable) {
        if ($nativeType =~ /^V8StringResource/) {
            $code .= "    " . ConvertToV8StringResource($attribute->signature, $nativeType, "v", $getterString) . ";\n";
        } else {
            $code .= "    $nativeType v = $getterString;\n";
        }

        if ($isNullable) {
            $code .= "    if (isNull)\n";
            $code .= "        return v8Null(info.GetIsolate());\n";
        }

        if ($useExceptions) {
            $code .= "    if (UNLIKELY(ec))\n";
            $code .= "        return setDOMException(ec, info.GetIsolate());\n";

            if (ExtendedAttributeContains($attribute->signature->extendedAttributes->{"CallWith"}, "ScriptState")) {
                $code .= "    if (state.hadException())\n";
                $code .= "        return throwError(state.exception(), info.GetIsolate());\n";
            }
        }

        $expression = "v";
        $expression .= ".release()" if (IsRefPtrType($returnType));
    } else {
        # Can inline the function call into the return statement to avoid overhead of using a Ref<> temporary
        $expression = $getterString;
        # Fix amigious conversion problem, by casting to the base type first ($getterString returns a type that inherits from SVGAnimatedEnumeration, not the base class directly).
        $expression = "static_pointer_cast<SVGAnimatedEnumeration>($expression)" if $returnType eq "SVGAnimatedEnumeration";
    }

    if (ShouldKeepAttributeAlive($interface, $attribute, $returnType)) {
        my $arrayType = GetArrayType($returnType);
        if ($arrayType) {
            if (!SkipIncludeHeader($arrayType)) {
                AddToImplIncludes("V8$arrayType.h");
                AddToImplIncludes("$arrayType.h");
            }
            $code .= "    return v8Array(${getterString}, info.GetIsolate());\n";
            $code .= "}\n\n";
            AddToImplContentInternals($code);
            return;
        }

        AddIncludesForType($returnType);
        # Check for a wrapper in the wrapper cache. If there is one, we know that a hidden reference has already
        # been created. If we don't find a wrapper, we create both a wrapper and a hidden reference.
        $code .= "    RefPtr<$returnType> result = ${getterString};\n";
        if ($forMainWorldSuffix) {
          $code .= "    v8::Handle<v8::Value> wrapper = result.get() ? v8::Handle<v8::Value>(DOMDataStore::getWrapper${forMainWorldSuffix}(result.get())) : v8Undefined();\n";
        } else {
          $code .= "    v8::Handle<v8::Value> wrapper = result.get() ? v8::Handle<v8::Value>(DOMDataStore::getWrapper(result.get(), info.GetIsolate())) : v8Undefined();\n";
        }
        $code .= "    if (wrapper.IsEmpty()) {\n";
        $code .= "        wrapper = toV8(result.get(), info.Holder(), info.GetIsolate());\n"; # FIXME: Could use wrap here since the wrapper is empty.
        $code .= "        if (!wrapper.IsEmpty())\n";
        $code .= "            V8HiddenPropertyName::setNamedHiddenReference(info.Holder(), \"${attrName}\", wrapper);\n";
        $code .= "    }\n";
        $code .= "    return wrapper;\n";
        $code .= "}\n\n";
        $code .= "#endif // ${conditionalString}\n\n" if $conditionalString;
        AddToImplContentInternals($code);
        return;
    }

    if ((IsSVGAnimatedType($interfaceName) or $interfaceName eq "SVGViewSpec") and IsSVGTypeNeedingTearOff($attrType)) {
        AddToImplIncludes("V8$attrType.h");
        my $svgNativeType = GetSVGTypeNeedingTearOff($attrType);
        # Convert from abstract SVGProperty to real type, so the right toJS() method can be invoked.
        $code .= "    return toV8Fast$forMainWorldSuffix(static_cast<$svgNativeType*>($expression), info, imp);\n";
    } elsif (IsSVGTypeNeedingTearOff($attrType) and not $interfaceName =~ /List$/) {
        AddToImplIncludes("V8$attrType.h");
        AddToImplIncludes("core/svg/properties/SVGPropertyTearOff.h");
        my $tearOffType = GetSVGTypeNeedingTearOff($attrType);
        my $wrappedValue;
        if (IsSVGTypeWithWritablePropertiesNeedingTearOff($attrType) and not defined $attribute->signature->extendedAttributes->{"Immutable"}) {
            my $getter = $expression;
            $getter =~ s/imp->//;
            $getter =~ s/\(\)//;

            my $updateMethod = "&${interfaceName}::update" . FirstLetterToUpperCase($getter);

            my $selfIsTearOffType = IsSVGTypeNeedingTearOff($interfaceName);
            if ($selfIsTearOffType) {
                AddToImplIncludes("core/svg/properties/SVGStaticPropertyWithParentTearOff.h");
                $tearOffType =~ s/SVGPropertyTearOff</SVGStaticPropertyWithParentTearOff<$interfaceName, /;

                if ($expression =~ /matrix/ and $interfaceName eq "SVGTransform") {
                    # SVGTransform offers a matrix() method for internal usage that returns an AffineTransform
                    # and a svgMatrix() method returning a SVGMatrix, used for the bindings.
                    $expression =~ s/matrix/svgMatrix/;
                }

                $wrappedValue = "WTF::getPtr(${tearOffType}::create(wrapper, $expression, $updateMethod))";
            } else {
                AddToImplIncludes("core/svg/properties/SVGStaticPropertyTearOff.h");
                $tearOffType =~ s/SVGPropertyTearOff</SVGStaticPropertyTearOff<$interfaceName, /;

                $wrappedValue = "WTF::getPtr(${tearOffType}::create(imp, $expression, $updateMethod))";
            }
        } elsif ($tearOffType =~ /SVGStaticListPropertyTearOff/) {
                $wrappedValue = "WTF::getPtr(${tearOffType}::create(imp, $expression))";
        } elsif ($tearOffType =~ /SVG(Point|PathSeg)List/) {
                $wrappedValue = "WTF::getPtr($expression)";
        } else {
                $wrappedValue = "WTF::getPtr(${tearOffType}::create($expression))";
        }
        $code .= "    return toV8Fast$forMainWorldSuffix($wrappedValue, info, imp);\n";
    } elsif ($attribute->signature->type eq "MessagePortArray") {
        AddToImplIncludes("MessagePort.h");
        AddToImplIncludes("V8MessagePort.h");
        my $getterFunc = ToMethodName($attribute->signature->name);
        $code .= <<END;
    MessagePortArray* ports = imp->${getterFunc}();
    if (!ports)
        return v8::Array::New(0);
    MessagePortArray portsCopy(*ports);
    v8::Local<v8::Array> portArray = v8::Array::New(portsCopy.size());
    for (size_t i = 0; i < portsCopy.size(); ++i)
        portArray->Set(v8Integer(i, info.GetIsolate()), toV8Fast$forMainWorldSuffix(portsCopy[i].get(), info, imp));
    return portArray;
END
    } elsif ($attribute->signature->type eq "SerializedScriptValue" && $attrExt->{"CachedAttribute"}) {
        my $getterFunc = ToMethodName($attribute->signature->name);
        $code .= <<END;
    RefPtr<SerializedScriptValue> serialized = imp->${getterFunc}();
    value = serialized ? serialized->deserialize() : v8::Handle<v8::Value>(v8Null(info.GetIsolate()));
    info.Holder()->SetHiddenValue(propertyName, value);
    return value;
END
    } else {
        $code .= "    return " . NativeToJSValue($attribute->signature, $expression, "info.Holder()", "info.GetIsolate()", "info", "imp", "ReturnUnsafeHandle", $forMainWorldSuffix).";\n";
    }

    $code .= "}\n\n";  # end of getter
    $code .= "#endif // ${conditionalString}\n\n" if $conditionalString;
    AddToImplContentInternals($code);
}

sub ShouldKeepAttributeAlive
{
    my ($interface, $attribute, $returnType) = @_;
    my $attrName = $attribute->signature->name;

    return 1 if $attribute->signature->extendedAttributes->{"KeepAttributeAliveForGC"};

    # Basically, for readonly or replaceable attributes, we have to guarantee
    # that JS wrappers don't get garbage-collected prematually when their
    # lifetime is strongly tied to their owner.
    return 0 if !IsWrapperType($returnType);
    return 0 if !IsReadonly($attribute) && !$attribute->signature->extendedAttributes->{"Replaceable"};

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
    return 0 if $returnType eq "SerializedScriptValue";
    return 0 if $returnType eq "DOMWindow";
    return 0 if $returnType eq "MessagePortArray";
    return 0 if $returnType =~ /SVG/;
    return 0 if $returnType =~ /HTML/;

    return 1;
}

sub GenerateReplaceableAttrSetterCallback
{
    my $interface = shift;
    my $interfaceName = $interface->name;

    my $code = "";
    $code .= "static void ${interfaceName}ReplaceableAttrSetterCallback(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)\n";
    $code .= "{\n";
    $code .= GenerateFeatureObservation($interface->extendedAttributes->{"MeasureAs"});
    $code .= GenerateDeprecationNotification($interface->extendedAttributes->{"DeprecateAs"});
    if (HasActivityLogging("", $interface->extendedAttributes, "Setter")) {
         die "IDL error: ActivityLog attribute cannot exist on a ReplacableAttrSetterCallback";
    }
    $code .= "    return ${interfaceName}V8Internal::${interfaceName}ReplaceableAttrSetter(name, value, info);\n";
    $code .= "}\n\n";
    AddToImplContentInternals($code);
}

sub GenerateReplaceableAttrSetter
{
    my $interface = shift;
    my $interfaceName = $interface->name;

    my $code = "";
    $code .= <<END;
static void ${interfaceName}ReplaceableAttrSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
END
    if ($interface->extendedAttributes->{"CheckSecurity"}) {
        AddToImplIncludes("core/page/Frame.h");
        $code .= <<END;
    ${interfaceName}* imp = V8${interfaceName}::toNative(info.Holder());
    if (!BindingSecurity::shouldAllowAccessToFrame(imp->frame()))
        return;
END
    }

    $code .= <<END;
    info.This()->ForceSet(name, value);
}

END
    AddToImplContentInternals($code);
}

sub GenerateCustomElementInvocationScopeIfNeeded
{
    my $code = "";
    my $ext = shift;

    if ($ext->{"DeliverCustomElementCallbacks"}) {
        if ($ext->{"Reflect"}) {
            die "IDL error: [Reflect] and [DeliverCustomElementCallbacks] cannot coexist yet";
        }

        AddToImplIncludes("core/dom/CustomElementRegistry.h");
        $code .= <<END;
    CustomElementRegistry::CallbackDeliveryScope deliveryScope;
END
    }
    return $code;
}

sub GenerateNormalAttrSetterCallback
{
    my $attribute = shift;
    my $interface = shift;
    my $forMainWorldSuffix = shift;

    my $interfaceName = $interface->name;
    my $v8InterfaceName = "V8$interfaceName";
    my $attrExt = $attribute->signature->extendedAttributes;
    my $attrName = $attribute->signature->name;

    my $conditionalString = GenerateConditionalString($attribute->signature);
    my $code = "";
    $code .= "#if ${conditionalString}\n\n" if $conditionalString;

    $code .= "static void ${attrName}AttrSetterCallback${forMainWorldSuffix}(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)\n";
    $code .= "{\n";
    $code .= GenerateFeatureObservation($attrExt->{"MeasureAs"});
    $code .= GenerateDeprecationNotification($attrExt->{"DeprecateAs"});
    if (HasActivityLogging($forMainWorldSuffix, $attrExt, "Setter")) {
        $code .= GenerateActivityLogging("Setter", $interface, "${attrName}");
    }
    if (HasCustomSetter($attrExt)) {
        $code .= "    ${v8InterfaceName}::${attrName}AttrSetterCustom(name, value, info);\n";
    } else {
        $code .= "    ${interfaceName}V8Internal::${attrName}AttrSetter${forMainWorldSuffix}(name, value, info);\n";
    }
    $code .= "}\n\n";
    $code .= "#endif // ${conditionalString}\n\n" if $conditionalString;
    AddToImplContentInternals($code);
}

sub GenerateNormalAttrSetter
{
    my $attribute = shift;
    my $interface = shift;
    my $forMainWorldSuffix = shift;

    my $interfaceName = $interface->name;
    my $v8InterfaceName = "V8$interfaceName";
    my $attrName = $attribute->signature->name;
    my $attrExt = $attribute->signature->extendedAttributes;
    my $attrType = $attribute->signature->type;

    if (HasCustomSetter($attrExt)) {
        return;
    }

    my $conditionalString = GenerateConditionalString($attribute->signature);
    my $code = "";
    $code .= "#if ${conditionalString}\n\n" if $conditionalString;
    $code .= "static void ${attrName}AttrSetter${forMainWorldSuffix}(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)\n";
    $code .= "{\n";

    # If the "StrictTypeChecking" extended attribute is present, and the attribute's type is an
    # interface type, then if the incoming value does not implement that interface, a TypeError is
    # thrown rather than silently passing NULL to the C++ code.
    # Per the Web IDL and ECMAScript specifications, incoming values can always be converted to both
    # strings and numbers, so do not throw TypeError if the attribute is of these types.
    if ($attribute->signature->extendedAttributes->{"StrictTypeChecking"}) {
        my $argType = $attribute->signature->type;
        if (IsWrapperType($argType)) {
            $code .= "    if (!isUndefinedOrNull(value) && !V8${argType}::HasInstance(value, info.GetIsolate(), worldType(info.GetIsolate()))) {\n";
            $code .= "        throwTypeError(0, info.GetIsolate());\n";
            $code .= "        return;\n";
            $code .= "    }\n";
        }
    }

    my $svgNativeType = GetSVGTypeNeedingTearOff($interfaceName);
    if ($svgNativeType) {
        my $svgWrappedNativeType = GetSVGWrappedTypeNeedingTearOff($interfaceName);
        if ($svgWrappedNativeType =~ /List$/) {
            $code .= <<END;
    $svgNativeType* imp = ${v8InterfaceName}::toNative(info.Holder());
END
        } else {
            AddToImplIncludes("core/dom/ExceptionCode.h");
            $code .= "    $svgNativeType* wrapper = ${v8InterfaceName}::toNative(info.Holder());\n";
            $code .= "    if (wrapper->isReadOnly()) {\n";
            $code .= "        setDOMException(NO_MODIFICATION_ALLOWED_ERR, info.GetIsolate());\n";
            $code .= "        return;\n";
            $code .= "    }\n";
            $code .= "    $svgWrappedNativeType& impInstance = wrapper->propertyReference();\n";
            $code .= "    $svgWrappedNativeType* imp = &impInstance;\n";
        }
    } elsif ($attrExt->{"OnProto"}) {
        $code .= <<END;
    ${interfaceName}* imp = ${v8InterfaceName}::toNative(info.Holder());
END
    } else {
        my $reflect = $attribute->signature->extendedAttributes->{"Reflect"};
        if ($reflect && InheritsInterface($interface, "Node") && $attrType eq "DOMString") {
            # Generate super-compact call for regular attribute setter:
            my $contentAttributeName = $reflect eq "VALUE_IS_MISSING" ? lc $attrName : $reflect;
            my $namespace = NamespaceForAttributeName($interfaceName, $contentAttributeName);
            AddToImplIncludes("${namespace}.h");
            $code .= "    Element* imp = V8Element::toNative(info.Holder());\n";
            $code .= "    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<WithNullCheck>, stringResource, value);\n";
            $code .= "    imp->setAttribute(${namespace}::${contentAttributeName}Attr, stringResource);\n";
            $code .= "}\n\n";
            $code .= "#endif // ${conditionalString}\n\n" if $conditionalString;
            AddToImplContentInternals($code);
            return;
            # Skip the rest of the function!
        }

        if (!$attribute->isStatic) {
            $code .= <<END;
    ${interfaceName}* imp = ${v8InterfaceName}::toNative(info.Holder());
END
        }
    }

    my $nativeType = GetNativeTypeFromSignature($attribute->signature, 0);
    if ($attribute->signature->type eq "EventListener") {
        if ($interface->name eq "DOMWindow") {
            $code .= "    if (!imp->document())\n";
            $code .= "        return;\n";
        }
    } else {
        my $value = JSValueToNative($attribute->signature, "value", "info.GetIsolate()");
        my $arrayType = GetArrayType($nativeType);

        if ($nativeType =~ /^V8StringResource/) {
            $code .= "    " . ConvertToV8StringResource($attribute->signature, $nativeType, "v", $value, "VOID") . "\n";
        } elsif ($arrayType) {
            $code .= "    Vector<$arrayType> v = $value;\n";
        } elsif ($attribute->signature->extendedAttributes->{"EnforceRange"}) {
            $code .= "    V8TRYCATCH_WITH_TYPECHECK_VOID($nativeType, v, $value, info.GetIsolate());\n";
        } else {
            $code .= "    V8TRYCATCH_VOID($nativeType, v, $value);\n";
        }
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
    String string = v;
    if (!($enumValidationExpression))
        return;
END
    }

    my $expression = "v";
    my $returnType = $attribute->signature->type;
    if (IsRefPtrType($returnType) && !GetArrayType($returnType)) {
        $expression = "WTF::getPtr(" . $expression . ")";
    }

    $code .= GenerateCustomElementInvocationScopeIfNeeded($attribute->signature->extendedAttributes);

    my $useExceptions = 1 if $attribute->signature->extendedAttributes->{"SetterRaisesException"};

    if ($useExceptions) {
        AddToImplIncludes("core/dom/ExceptionCode.h");
        $code .= "    ExceptionCode ec = 0;\n";
    }

    if ($interfaceName eq "SVGNumber") {
        $code .= "    *imp = $expression;\n";
    } else {
        if ($attribute->signature->type eq "EventListener") {
            my $implSetterFunctionName = FirstLetterToUpperCase($attrName);
            AddToImplIncludes("bindings/v8/V8AbstractEventListener.h");
            if (!InheritsInterface($interface, "Node")) {
                $code .= "    transferHiddenDependency(info.Holder(), imp->$attrName(), value, ${v8InterfaceName}::eventListenerCacheIndex, info.GetIsolate());\n";
            }
            AddToImplIncludes("bindings/v8/V8EventListenerList.h");
            if ($interfaceName eq "WorkerContext" and $attribute->signature->name eq "onerror") {
                AddToImplIncludes("bindings/v8/V8WorkerContextErrorHandler.h");
                $code .= "    imp->set$implSetterFunctionName(V8EventListenerList::findOrCreateWrapper<V8WorkerContextErrorHandler>(value, true)";
            } elsif ($interfaceName eq "DOMWindow" and $attribute->signature->name eq "onerror") {
                AddToImplIncludes("bindings/v8/V8WindowErrorHandler.h");
                $code .= "    imp->set$implSetterFunctionName(V8EventListenerList::findOrCreateWrapper<V8WindowErrorHandler>(value, true)";
            } else {
                $code .= "    imp->set$implSetterFunctionName(V8EventListenerList::getEventListener(value, true, ListenerFindOrCreate)";
            }
            $code .= ", ec" if $useExceptions;
            $code .= ");\n";
        } else {
            my ($functionName, @arguments) = SetterExpression(\%implIncludes, $interfaceName, $attribute);
            push(@arguments, $expression);
            push(@arguments, "ec") if $useExceptions;
            if ($attribute->signature->extendedAttributes->{"ImplementedBy"}) {
                my $implementedBy = $attribute->signature->extendedAttributes->{"ImplementedBy"};
                AddInterfaceToImplIncludes($implementedBy);
                unshift(@arguments, "imp") if !$attribute->isStatic;
                $functionName = "${implementedBy}::${functionName}";
            } elsif ($attribute->isStatic) {
                $functionName = "${interfaceName}::${functionName}";
            } else {
                $functionName = "imp->${functionName}";
            }
            my ($arg, $subCode) = GenerateCallWith($attribute->signature->extendedAttributes->{"CallWith"}, "    ", 1);
            $code .= $subCode;
            unshift(@arguments, @$arg);
            $code .= "    ${functionName}(" . join(", ", @arguments) . ");\n";
        }
    }

    if ($useExceptions) {
        $code .= "    if (UNLIKELY(ec))\n";
        $code .= "        setDOMException(ec, info.GetIsolate());\n";
    }

    if (ExtendedAttributeContains($attribute->signature->extendedAttributes->{"CallWith"}, "ScriptState")) {
        $code .= "    if (state.hadException())\n";
        $code .= "        throwError(state.exception(), info.GetIsolate());\n";
    }

    if ($svgNativeType) {
        if ($useExceptions) {
            $code .= "    if (!ec)\n";
            $code .= "        wrapper->commitChange();\n";
        } else {
            $code .= "    wrapper->commitChange();\n";
        }
    }

    if ($attribute->signature->type eq "SerializedScriptValue" && $attribute->signature->extendedAttributes->{"CachedAttribute"}) {
        $code .= <<END;
    info.Holder()->DeleteHiddenValue(v8::String::NewSymbol("${attrName}")); // Invalidate the cached value.
END
    }

    $code .= "    return;\n";
    $code .= "}\n\n";  # end of setter
    $code .= "#endif // ${conditionalString}\n\n" if $conditionalString;
    AddToImplContentInternals($code);
}

sub GenerateParametersCheckExpression
{
    my $numParameters = shift;
    my $function = shift;

    my @andExpression = ();
    push(@andExpression, "args.Length() == $numParameters");
    my $parameterIndex = 0;
    foreach my $parameter (@{$function->parameters}) {
        last if $parameterIndex >= $numParameters;
        my $value = "args[$parameterIndex]";
        my $type = $parameter->type;

        # Only DOMString or wrapper types are checked.
        # For DOMString with StrictTypeChecking only Null, Undefined and Object
        # are accepted for compatibility. Otherwise, no restrictions are made to
        # match the non-overloaded behavior.
        # FIXME: Implement WebIDL overload resolution algorithm.
        if ($type eq "DOMString") {
            if ($parameter->extendedAttributes->{"StrictTypeChecking"}) {
                push(@andExpression, "(${value}->IsNull() || ${value}->IsUndefined() || ${value}->IsString() || ${value}->IsObject())");
            }
        } elsif (IsCallbackInterface($parameter->type)) {
            # For Callbacks only checks if the value is null or object.
            push(@andExpression, "(${value}->IsNull() || ${value}->IsFunction())");
        } elsif (GetArrayType($type) || GetSequenceType($type)) {
            if ($parameter->isNullable) {
                push(@andExpression, "(${value}->IsNull() || ${value}->IsArray())");
            } else {
                push(@andExpression, "(${value}->IsArray())");
            }
        } elsif (IsWrapperType($type)) {
            if ($parameter->isNullable) {
                push(@andExpression, "(${value}->IsNull() || V8${type}::HasInstance($value, args.GetIsolate(), worldType(args.GetIsolate())))");
            } else {
                push(@andExpression, "(V8${type}::HasInstance($value, args.GetIsolate(), worldType(args.GetIsolate())))");
            }
        }

        $parameterIndex++;
    }
    my $res = join(" && ", @andExpression);
    $res = "($res)" if @andExpression > 1;
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
    my $interfaceName = $interface->name;

    # Generate code for choosing the correct overload to call. Overloads are
    # chosen based on the total number of arguments passed and the type of
    # values passed in non-primitive argument slots. When more than a single
    # overload is applicable, precedence is given according to the order of
    # declaration in the IDL.

    my $name = $function->signature->name;

    my $conditionalString = GenerateConditionalString($function->signature);
    my $leastNumMandatoryParams = 255;
    my $code = "";
    $code .= "#if ${conditionalString}\n\n" if $conditionalString;
    $code .= <<END;
static v8::Handle<v8::Value> ${name}Method${forMainWorldSuffix}(const v8::Arguments& args)
{
END
    $code .= GenerateFeatureObservation($function->signature->extendedAttributes->{"MeasureAs"});
    $code .= GenerateDeprecationNotification($function->signature->extendedAttributes->{"DeprecateAs"});

    foreach my $overload (@{$function->{overloads}}) {
        my ($numMandatoryParams, $parametersCheck) = GenerateFunctionParametersCheck($overload);
        $leastNumMandatoryParams = $numMandatoryParams if ($numMandatoryParams < $leastNumMandatoryParams);
        $code .= "    if ($parametersCheck)\n";
        my $overloadedIndexString = $overload->{overloadIndex};
        $code .= "        return ${name}${overloadedIndexString}Method${forMainWorldSuffix}(args);\n";
    }
    if ($leastNumMandatoryParams >= 1) {
        $code .= "    if (args.Length() < $leastNumMandatoryParams)\n";
        $code .= "        return throwNotEnoughArgumentsError(args.GetIsolate());\n";
    }
    $code .= <<END;
    return throwTypeError(0, args.GetIsolate());
END
    $code .= "}\n\n";
    $code .= "#endif // ${conditionalString}\n\n" if $conditionalString;
    AddToImplContentInternals($code);
}

sub GenerateFunctionCallback
{
    my $function = shift;
    my $interface = shift;
    my $forMainWorldSuffix = shift;

    my $interfaceName = $interface->name;
    my $v8InterfaceName = "V8$interfaceName";
    my $name = $function->signature->name;

    my $conditionalString = GenerateConditionalString($function->signature);
    my $code = "";
    $code .= "#if ${conditionalString}\n\n" if $conditionalString;
    $code .= <<END;
static v8::Handle<v8::Value> ${name}MethodCallback${forMainWorldSuffix}(const v8::Arguments& args)
{
END
    $code .= GenerateFeatureObservation($function->signature->extendedAttributes->{"MeasureAs"});
    $code .= GenerateDeprecationNotification($function->signature->extendedAttributes->{"DeprecateAs"});
    if (HasActivityLogging($forMainWorldSuffix, $function->signature->extendedAttributes, "Access")) {
        $code .= GenerateActivityLogging("Method", $interface, "${name}");
    }
    if (HasCustomMethod($function->signature->extendedAttributes)) {
        $code .= "    return ${v8InterfaceName}::${name}MethodCustom(args);\n";
    } else {
        $code .= "    return ${interfaceName}V8Internal::${name}Method${forMainWorldSuffix}(args);\n";
    }
    $code .= "}\n\n";
    $code .= "#endif // ${conditionalString}\n\n" if $conditionalString;
    AddToImplContentInternals($code);
}

sub GenerateFunction
{
    my $function = shift;
    my $interface = shift;
    my $forMainWorldSuffix = shift;

    my $interfaceName = $interface->name;
    my $v8InterfaceName = "V8$interfaceName";
    my $name = $function->signature->name;
    my $funcExt = $function->signature->extendedAttributes;

    if (HasCustomMethod($funcExt)) {
        return;
    }

    if (@{$function->{overloads}} > 1) {
        # Append a number to an overloaded method's name to make it unique:
        $name = $name . $function->{overloadIndex};
    }

    my $conditionalString = GenerateConditionalString($function->signature);
    my $code = "";
    $code .= "#if ${conditionalString}\n\n" if $conditionalString;
    $code .= "static v8::Handle<v8::Value> ${name}Method${forMainWorldSuffix}(const v8::Arguments& args)\n";
    $code .= "{\n";

    if ($name eq "addEventListener" || $name eq "removeEventListener") {
        my $lookupType = ($name eq "addEventListener") ? "OrCreate" : "Only";
        my $passRefPtrHandling = ($name eq "addEventListener") ? "" : ".get()";
        my $hiddenDependencyAction = ($name eq "addEventListener") ? "create" : "remove";

        AddToImplIncludes("bindings/v8/V8EventListenerList.h");
        $code .= <<END;
    RefPtr<EventListener> listener = V8EventListenerList::getEventListener(args[1], false, ListenerFind${lookupType});
    if (listener) {
        V8TRYCATCH_FOR_V8STRINGRESOURCE(V8StringResource<WithNullCheck>, stringResource, args[0]);
        V8${interfaceName}::toNative(args.Holder())->${name}(stringResource, listener${passRefPtrHandling}, args[2]->BooleanValue());
END
        if (!InheritsInterface($interface, "Node")) {
            $code .= <<END;
        ${hiddenDependencyAction}HiddenDependency(args.Holder(), args[1], V8${interfaceName}::eventListenerCacheIndex, args.GetIsolate());
END
        }
        $code .= <<END;
    }
    return v8Undefined();
}

END
        $code .= "#endif // ${conditionalString}\n\n" if $conditionalString;
        AddToImplContentInternals($code);
        return;
    }

    $code .= GenerateArgumentsCountCheck($function, $interface);

    if ($name eq "set" and $interface->extendedAttributes->{"TypedArray"}) {
        AddToImplIncludes("bindings/v8/custom/V8ArrayBufferViewCustom.h");
        $code .= <<END;
    return setWebGLArrayHelper<$interfaceName, ${v8InterfaceName}>(args);
}

END
        AddToImplContentInternals($code);
        return;
    }

    my ($svgPropertyType, $svgListPropertyType, $svgNativeType) = GetSVGPropertyTypes($interfaceName);

    if ($svgNativeType) {
        my $nativeClassName = GetNativeType($interfaceName);
        if ($interfaceName =~ /List$/) {
            $code .= "    $nativeClassName imp = ${v8InterfaceName}::toNative(args.Holder());\n";
        } else {
            AddToImplIncludes("core/dom/ExceptionCode.h");
            $code .= "    $nativeClassName wrapper = ${v8InterfaceName}::toNative(args.Holder());\n";
            $code .= "    if (wrapper->isReadOnly())\n";
            $code .= "        return setDOMException(NO_MODIFICATION_ALLOWED_ERR, args.GetIsolate());\n";
            my $svgWrappedNativeType = GetSVGWrappedTypeNeedingTearOff($interfaceName);
            $code .= "    $svgWrappedNativeType& impInstance = wrapper->propertyReference();\n";
            $code .= "    $svgWrappedNativeType* imp = &impInstance;\n";
        }
    } elsif (!$function->isStatic) {
        $code .= <<END;
    ${interfaceName}* imp = ${v8InterfaceName}::toNative(args.Holder());
END
    }

    $code .= GenerateCustomElementInvocationScopeIfNeeded($funcExt);

    # Check domain security if needed
    if ($interface->extendedAttributes->{"CheckSecurity"} && !$function->signature->extendedAttributes->{"DoNotCheckSecurity"}) {
        # We have not find real use cases yet.
        AddToImplIncludes("core/page/Frame.h");
        $code .= <<END;
    if (!BindingSecurity::shouldAllowAccessToFrame(imp->frame()))
        return v8Undefined();
END
    }

    my $raisesExceptions = $function->signature->extendedAttributes->{"RaisesException"};
    if (!$raisesExceptions) {
        foreach my $parameter (@{$function->parameters}) {
            if ((!IsCallbackInterface($parameter->type) and TypeCanFailConversion($parameter)) or $parameter->extendedAttributes->{"IsIndex"}) {
                $raisesExceptions = 1;
            }
        }
    }

    if ($raisesExceptions) {
        AddToImplIncludes("core/dom/ExceptionCode.h");
        $code .= "    ExceptionCode ec = 0;\n";
        $code .= "    {\n";
        # The brace here is needed to prevent the ensuing 'goto fail's from jumping past constructors
        # of objects (like Strings) declared later, causing compile errors. The block scope ends
        # right before the label 'fail:'.
    }

    if ($function->signature->extendedAttributes->{"CheckSecurityForNode"}) {
        $code .= "    if (!BindingSecurity::shouldAllowAccessToNode(imp->" . $function->signature->name . "(ec)))\n";
        $code .= "        return v8::Handle<v8::Value>(v8Null(args.GetIsolate()));\n";
END
    }

    my ($parameterCheckString, $paramIndex, %replacements) = GenerateParametersCheck($function, $interfaceName, $forMainWorldSuffix);
    $code .= $parameterCheckString;

    # Build the function call string.
    $code .= GenerateFunctionCallString($function, $paramIndex, "    ", $interfaceName, $forMainWorldSuffix, %replacements);

    if ($raisesExceptions) {
        $code .= "    }\n";
        $code .= "    fail:\n";
        $code .= "    return setDOMException(ec, args.GetIsolate());\n";
    }

    $code .= "}\n\n";
    $code .= "#endif // ${conditionalString}\n\n" if $conditionalString;
    AddToImplContentInternals($code);
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
    }
    if (ExtendedAttributeContains($callWith, "ScriptExecutionContext")) {
        $code .= $indent . "ScriptExecutionContext* scriptContext = getScriptExecutionContext();\n";
        push(@callWithArgs, "scriptContext");
    }
    if ($function and ExtendedAttributeContains($callWith, "ScriptArguments")) {
        $code .= $indent . "RefPtr<ScriptArguments> scriptArguments(createScriptArguments(args, " . @{$function->parameters} . "));\n";
        push(@callWithArgs, "scriptArguments.release()");
        AddToImplIncludes("bindings/v8/ScriptCallStackFactory.h");
        AddToImplIncludes("core/inspector/ScriptArguments.h");
    }
    return ([@callWithArgs], $code);
}

sub GenerateArgumentsCountCheck
{
    my $function = shift;
    my $interface = shift;

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

    my $argumentsCountCheckString = "";
    if ($numMandatoryParams >= 1) {
        $argumentsCountCheckString .= "    if (args.Length() < $numMandatoryParams)\n";
        $argumentsCountCheckString .= "        return throwNotEnoughArgumentsError(args.GetIsolate());\n";
    }
    return $argumentsCountCheckString;
}

sub GetIndexOf
{
    my $paramName = shift;
    my @paramList = @_;
    my $index = 0;
    foreach my $param (@paramList) {
        if ($paramName eq $param) {
            return $index;
        }
        $index++;
    }
    return -1;
}

sub GenerateParametersCheck
{
    my $function = shift;
    my $interfaceName = shift;
    my $forMainWorldSuffix = shift;

    my $parameterCheckString = "";
    my $paramIndex = 0;
    my %replacements = ();

    foreach my $parameter (@{$function->parameters}) {
        my $nativeType = GetNativeTypeFromSignature($parameter, $paramIndex);

        # Optional arguments without [Default=...] should generate an early call with fewer arguments.
        # Optional arguments with [Optional=...] should not generate the early call.
        # Optional Dictionary arguments always considered to have default of empty dictionary.
        if ($parameter->isOptional && !$parameter->extendedAttributes->{"Default"} && $nativeType ne "Dictionary" && !IsCallbackInterface($parameter->type)) {
            $parameterCheckString .= "    if (args.Length() <= $paramIndex) {\n";
            my $functionCall = GenerateFunctionCallString($function, $paramIndex, "    " x 2, $interfaceName, $forMainWorldSuffix, %replacements);
            $parameterCheckString .= $functionCall;
            $parameterCheckString .= "    }\n";
        }

        my $parameterDefaultPolicy = "DefaultIsUndefined";
        my $default = defined $parameter->extendedAttributes->{"Default"} ? $parameter->extendedAttributes->{"Default"} : "";
        if ($parameter->isOptional and $default eq "NullString") {
            $parameterDefaultPolicy = "DefaultIsNullString";
        }

        my $parameterName = $parameter->name;
        AddToImplIncludes("core/dom/ExceptionCode.h");
        if (IsCallbackInterface($parameter->type)) {
            my $v8InterfaceName = "V8" . $parameter->type;
            AddToImplIncludes("$v8InterfaceName.h");
            if ($parameter->isOptional) {
                $parameterCheckString .= "    RefPtr<" . $parameter->type . "> $parameterName;\n";
                $parameterCheckString .= "    if (args.Length() > $paramIndex && !args[$paramIndex]->IsNull() && !args[$paramIndex]->IsUndefined()) {\n";
                $parameterCheckString .= "        if (!args[$paramIndex]->IsFunction())\n";
                $parameterCheckString .= "            return throwTypeError(0, args.GetIsolate());\n";
                $parameterCheckString .= "        $parameterName = ${v8InterfaceName}::create(args[$paramIndex], getScriptExecutionContext());\n";
                $parameterCheckString .= "    }\n";
            } else {
                $parameterCheckString .= "    if (args.Length() <= $paramIndex || !args[$paramIndex]->IsFunction())\n";
                $parameterCheckString .= "        return throwTypeError(0, args.GetIsolate());\n";
                $parameterCheckString .= "    RefPtr<" . $parameter->type . "> $parameterName = ${v8InterfaceName}::create(args[$paramIndex], getScriptExecutionContext());\n";
            }
        } elsif ($parameter->extendedAttributes->{"Clamp"}) {
                my $nativeValue = "${parameterName}NativeValue";
                my $paramType = $parameter->type;
                $parameterCheckString .= "    $paramType $parameterName = 0;\n";
                $parameterCheckString .= "    V8TRYCATCH(double, $nativeValue, args[$paramIndex]->NumberValue());\n";
                $parameterCheckString .= "    if (!std::isnan($nativeValue))\n";
                $parameterCheckString .= "        $parameterName = clampTo<$paramType>($nativeValue);\n";
        } elsif ($parameter->type eq "SerializedScriptValue") {
            AddToImplIncludes("bindings/v8/SerializedScriptValue.h");
            $parameterCheckString .= "    bool ${parameterName}DidThrow = false;\n";
            $parameterCheckString .= "    $nativeType $parameterName = SerializedScriptValue::create(args[$paramIndex], 0, 0, ${parameterName}DidThrow, args.GetIsolate());\n";
            $parameterCheckString .= "    if (${parameterName}DidThrow)\n";
            $parameterCheckString .= "        return v8Undefined();\n";
        } elsif (TypeCanFailConversion($parameter)) {
            $parameterCheckString .= "    $nativeType $parameterName = " .
                 JSValueToNative($parameter, "args[$paramIndex]", "args.GetIsolate()") . ";\n";
            $parameterCheckString .= "    if (UNLIKELY(!$parameterName)) {\n";
            $parameterCheckString .= "        ec = TYPE_MISMATCH_ERR;\n";
            $parameterCheckString .= "        goto fail;\n";
            $parameterCheckString .= "    }\n";
        } elsif ($parameter->isVariadic) {
            my $nativeElementType = GetNativeType($parameter->type);
            if ($nativeElementType =~ />$/) {
                $nativeElementType .= " ";
            }

            my $argType = $parameter->type;
            if (IsWrapperType($argType)) {
                $parameterCheckString .= "    Vector<$nativeElementType> $parameterName;\n";
                $parameterCheckString .= "    for (int i = $paramIndex; i < args.Length(); ++i) {\n";
                $parameterCheckString .= "        if (!V8${argType}::HasInstance(args[i], args.GetIsolate(), worldType(args.GetIsolate())))\n";
                $parameterCheckString .= "            return throwTypeError(0, args.GetIsolate());\n";
                $parameterCheckString .= "        $parameterName.append(V8${argType}::toNative(v8::Handle<v8::Object>::Cast(args[i])));\n";
                $parameterCheckString .= "    }\n";
            } else {
                $parameterCheckString .= "    V8TRYCATCH(Vector<$nativeElementType>, $parameterName, toNativeArguments<$nativeElementType>(args, $paramIndex));\n";
            }
        } elsif ($nativeType =~ /^V8StringResource/) {
            my $default = defined $parameter->extendedAttributes->{"Default"} ? $parameter->extendedAttributes->{"Default"} : "";
            my $value = JSValueToNative($parameter, $parameter->isOptional && $default eq "NullString" ? "argumentOrNull(args, $paramIndex)" : "args[$paramIndex]", "args.GetIsolate()");
            $parameterCheckString .= "    " . ConvertToV8StringResource($parameter, $nativeType, $parameterName, $value) . "\n";
            if (IsEnumType($parameter->type)) {
                my @enumValues = ValidEnumValues($parameter->type);
                my @validEqualities = ();
                foreach my $enumValue (@enumValues) {
                    push(@validEqualities, "string == \"$enumValue\"");
                }
                my $enumValidationExpression = join(" || ", @validEqualities);
                $parameterCheckString .=  "    String string = $parameterName;\n";
                $parameterCheckString .=  "    if (!($enumValidationExpression))\n";
                $parameterCheckString .=  "        return throwTypeError(0, args.GetIsolate());\n";
            }
        } else {
            # If the "StrictTypeChecking" extended attribute is present, and the argument's type is an
            # interface type, then if the incoming value does not implement that interface, a TypeError
            # is thrown rather than silently passing NULL to the C++ code.
            # Per the Web IDL and ECMAScript specifications, incoming values can always be converted
            # to both strings and numbers, so do not throw TypeError if the argument is of these
            # types.
            if ($function->signature->extendedAttributes->{"StrictTypeChecking"}) {
                my $argValue = "args[$paramIndex]";
                my $argType = $parameter->type;
                if (IsWrapperType($argType)) {
                    $parameterCheckString .= "    if (args.Length() > $paramIndex && !isUndefinedOrNull($argValue) && !V8${argType}::HasInstance($argValue, args.GetIsolate(), worldType(args.GetIsolate())))\n";
                    $parameterCheckString .= "        return throwTypeError(0, args.GetIsolate());\n";
                }
            }
            my $default = defined $parameter->extendedAttributes->{"Default"} ? $parameter->extendedAttributes->{"Default"} : "";
            my $value = JSValueToNative($parameter, $parameter->isOptional && $default eq "NullString" ? "argumentOrNull(args, $paramIndex)" : "args[$paramIndex]", "args.GetIsolate()");
            if ($parameter->extendedAttributes->{"EnforceRange"}) {
                $parameterCheckString .= "    V8TRYCATCH_WITH_TYPECHECK($nativeType, $parameterName, $value, args.GetIsolate());\n";
            } else {
                $parameterCheckString .= "    V8TRYCATCH($nativeType, $parameterName, $value);\n";
            }
            if ($nativeType eq 'Dictionary') {
               $parameterCheckString .= "    if (!$parameterName.isUndefinedOrNull() && !$parameterName.isObject())\n";
               $parameterCheckString .= "        return throwTypeError(\"Not an object.\", args.GetIsolate());\n";
            }
        }

        if ($parameter->extendedAttributes->{"IsIndex"}) {
            $parameterCheckString .= "    if (UNLIKELY($parameterName < 0)) {\n";
            $parameterCheckString .= "        ec = INDEX_SIZE_ERR;\n";
            $parameterCheckString .= "        goto fail;\n";
            $parameterCheckString .= "    }\n";
        }

        $paramIndex++;
    }
    return ($parameterCheckString, $paramIndex, %replacements);
}

sub GenerateOverloadedConstructorCallback
{
    my $interface = shift;
    my $interfaceName = $interface->name;

    my $code = "";
    $code .= <<END;
static v8::Handle<v8::Value> constructor(const v8::Arguments& args)
{
END
    my $leastNumMandatoryParams = 255;
    foreach my $constructor (@{$interface->constructors}) {
        my $name = "constructor" . $constructor->{overloadedIndex};
        my ($numMandatoryParams, $parametersCheck) = GenerateFunctionParametersCheck($constructor);
        $leastNumMandatoryParams = $numMandatoryParams if ($numMandatoryParams < $leastNumMandatoryParams);
        $code .= "    if ($parametersCheck)\n";
        $code .= "        return ${interfaceName}V8Internal::${name}(args);\n";
    }
    if ($leastNumMandatoryParams >= 1) {
        $code .= "    if (args.Length() < $leastNumMandatoryParams)\n";
        $code .= "        return throwNotEnoughArgumentsError(args.GetIsolate());\n";
    }
    $code .= <<END;
    return throwTypeError(0, args.GetIsolate());
END
    $code .= "}\n\n";
    AddToImplContentInternals($code);
}

sub GenerateSingleConstructorCallback
{
    my $interface = shift;
    my $function = shift;

    my $interfaceName = $interface->name;
    my $overloadedIndexString = "";
    if ($function->{overloadedIndex} > 0) {
        $overloadedIndexString .= $function->{overloadedIndex};
    }

    my $raisesExceptions = $function->signature->extendedAttributes->{"RaisesException"};
    if ($interface->extendedAttributes->{"RaisesException"}) {
        $raisesExceptions = 1;
    }
    if (!$raisesExceptions) {
        foreach my $parameter (@{$function->parameters}) {
            if ((!IsCallbackInterface($parameter->type) and TypeCanFailConversion($parameter)) or $parameter->extendedAttributes->{"IsIndex"}) {
                $raisesExceptions = 1;
            }
        }
    }

    my @beforeArgumentList;
    my @afterArgumentList;
    my $code = "";
    $code .= <<END;
static v8::Handle<v8::Value> constructor${overloadedIndexString}(const v8::Arguments& args)
{
END

    if ($function->{overloadedIndex} == 0) {
        $code .= GenerateArgumentsCountCheck($function, $interface);
    }

    if ($raisesExceptions) {
        AddToImplIncludes("core/dom/ExceptionCode.h");
        $code .= "\n";
        $code .= "    ExceptionCode ec = 0;\n";
    }

    # FIXME: Currently [Constructor(...)] does not yet support optional arguments without [Default=...]
    my ($parameterCheckString, $paramIndex, %replacements) = GenerateParametersCheck($function, $interfaceName, "");
    $code .= $parameterCheckString;

    if ($interface->extendedAttributes->{"CallWith"} && $interface->extendedAttributes->{"CallWith"} eq "ScriptExecutionContext") {
        push(@beforeArgumentList, "context");
        $code .= <<END;

    ScriptExecutionContext* context = getScriptExecutionContext();
END
    }

    if ($interface->extendedAttributes->{"RaisesException"}) {
        push(@afterArgumentList, "ec");
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
    $code .= "\n";
    $code .= "    RefPtr<${interfaceName}> impl = ${interfaceName}::create(${argumentString});\n";
    $code .= "    v8::Handle<v8::Object> wrapper = args.Holder();\n";

    if ($interface->extendedAttributes->{"RaisesException"}) {
        $code .= "    if (ec)\n";
        $code .= "        goto fail;\n";
    }

    $code .= <<END;

    V8DOMWrapper::associateObjectWithWrapper(impl.release(), &V8${interfaceName}::info, wrapper, args.GetIsolate(), WrapperConfiguration::Dependent);
    return wrapper;
END

    if ($raisesExceptions) {
        $code .= "    fail:\n";
        $code .= "    return setDOMException(ec, args.GetIsolate());\n";
    }

    $code .= "}\n";
    $code .= "\n";
    AddToImplContentInternals($code);
}

# The Web IDL specification states that Interface objects for interfaces MUST have a property named
# "length" that returns the length of the shortest argument list of the entries in the effective
# overload set for constructors. In other words, use the lowest number of mandatory arguments among
# all constructors.
sub GetInterfaceLength
{
    my $interface = shift;

    my $leastConstructorLength = 0;
    if (IsConstructorTemplate($interface, "Event") || IsConstructorTemplate($interface, "TypedArray")) {
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

    my $interfaceName = $interface->name;
    my $code = "";
    $code .= "v8::Handle<v8::Value> V8${interfaceName}::constructorCallback(const v8::Arguments& args)\n";
    $code .= "{\n";
    $code .= GenerateFeatureObservation($interface->extendedAttributes->{"MeasureAs"});
    $code .= GenerateDeprecationNotification($interface->extendedAttributes->{"DeprecateAs"});
    $code .= GenerateConstructorHeader();
    if (HasCustomConstructor($interface)) {
        $code .= "    return V8${interfaceName}::constructorCustom(args);\n";
    } else {
        $code .= "    return ${interfaceName}V8Internal::constructor(args);\n";
    }
    $code .= "}\n\n";
    AddToImplContent($code);
}

sub GenerateConstructor
{
    my $interface = shift;
    my $interfaceName = $interface->name;

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

    AddToImplIncludes("bindings/v8/Dictionary.h");
    AddToImplContentInternals(<<END);
static v8::Handle<v8::Value> constructor(const v8::Arguments& args)
{
    if (args.Length() < 1)
        return throwNotEnoughArgumentsError(args.GetIsolate());

    V8TRYCATCH_FOR_V8STRINGRESOURCE(V8StringResource<>, type, args[0]);
    ${interfaceName}Init eventInit;
    if (args.Length() >= 2) {
        V8TRYCATCH(Dictionary, options, Dictionary(args[1], args.GetIsolate()));
        if (!fill${interfaceName}Init(eventInit, options))
            return v8Undefined();
    }

    RefPtr<${interfaceName}> event = ${interfaceName}::create(type, eventInit);

    v8::Handle<v8::Object> wrapper = args.Holder();
    V8DOMWrapper::associateObjectWithWrapper(event.release(), &V8${interfaceName}::info, wrapper, args.GetIsolate(), WrapperConfiguration::Dependent);
    return wrapper;
}
END

    my $code = "";
    $code .= <<END;
bool fill${interfaceName}Init(${interfaceName}Init& eventInit, const Dictionary& options)
{
END

    foreach my $interfaceBase (@{$interface->parents}) {
        $code .= <<END;
    if (!fill${interfaceBase}Init(eventInit, options))
        return false;

END
    }

    for (my $index = 0; $index < @{$interface->attributes}; $index++) {
        my $attribute = @{$interface->attributes}[$index];
        if ($attribute->signature->extendedAttributes->{"InitializedByEventConstructor"}) {
            my $attributeName = $attribute->signature->name;
            $code .= "    options.get(\"$attributeName\", eventInit.$attributeName);\n";
        }
    }

    $code .= <<END;
    return true;
}

END
    AddToImplContent($code);
}

sub GenerateTypedArrayConstructor
{
    my $interface = shift;
    my $interfaceName = $interface->name;
    my $viewType = GetTypeNameOfExternalTypedArray($interface);
    my $type = $interface->extendedAttributes->{"TypedArray"};
    AddToImplIncludes("bindings/v8/custom/V8ArrayBufferViewCustom.h");

    AddToImplContentInternals(<<END);
static v8::Handle<v8::Value> constructor(const v8::Arguments& args)
{
    return constructWebGLArray<$interfaceName, V8${interfaceName}, $type>(args, &V8${interfaceName}::info, $viewType);
}

END
}

sub GenerateNamedConstructor
{
    my $function = shift;
    my $interface = shift;

    my $interfaceName = $interface->name;
    my $v8InterfaceName = "V8$interfaceName";
    my $raisesExceptions = $function->signature->extendedAttributes->{"RaisesException"};
    if ($interface->extendedAttributes->{"RaisesException"}) {
        $raisesExceptions = 1;
    }
    if (!$raisesExceptions) {
        foreach my $parameter (@{$function->parameters}) {
            if ((!IsCallbackInterface($parameter->type) and TypeCanFailConversion($parameter)) or $parameter->extendedAttributes->{"IsIndex"}) {
                $raisesExceptions = 1;
            }
        }
    }

    my $maybeObserveFeature = GenerateFeatureObservation($function->signature->extendedAttributes->{"MeasureAs"});
    my $maybeDeprecateFeature = GenerateDeprecationNotification($function->signature->extendedAttributes->{"DeprecateAs"});

    my @beforeArgumentList;
    my @afterArgumentList;

    my $toActiveDOMObject = "0";
    if (InheritsExtendedAttribute($interface, "ActiveDOMObject")) {
        $toActiveDOMObject = "${v8InterfaceName}::toActiveDOMObject";
    }

    my $toEventTarget = "0";
    if (InheritsExtendedAttribute($interface, "EventTarget")) {
        $toEventTarget = "${v8InterfaceName}::toEventTarget";
    }

    AddToImplIncludes("core/page/Frame.h");
    AddToImplContent(<<END);
WrapperTypeInfo ${v8InterfaceName}Constructor::info = { ${v8InterfaceName}Constructor::GetTemplate, ${v8InterfaceName}::derefObject, $toActiveDOMObject, $toEventTarget, 0, ${v8InterfaceName}::installPerContextPrototypeProperties, 0, WrapperTypeObjectPrototype };

END

    my $code = "";
    $code .= <<END;
static v8::Handle<v8::Value> ${v8InterfaceName}ConstructorCallback(const v8::Arguments& args)
{
END
    $code .= $maybeObserveFeature if $maybeObserveFeature;
    $code .= $maybeDeprecateFeature if $maybeDeprecateFeature;
    $code .= GenerateConstructorHeader();
    AddToImplIncludes("V8Document.h");
    $code .= <<END;
    Document* document = currentDocument();

    // Make sure the document is added to the DOM Node map. Otherwise, the ${interfaceName} instance
    // may end up being the only node in the map and get garbage-collected prematurely.
    toV8(document, args.Holder(), args.GetIsolate());

END

    $code .= GenerateArgumentsCountCheck($function, $interface);

    if ($raisesExceptions) {
        AddToImplIncludes("core/dom/ExceptionCode.h");
        $code .= "\n";
        $code .= "    ExceptionCode ec = 0;\n";
    }

    my ($parameterCheckString, $paramIndex, %replacements) = GenerateParametersCheck($function, $interfaceName);
    $code .= $parameterCheckString;

    push(@beforeArgumentList, "document");

    if ($interface->extendedAttributes->{"RaisesException"}) {
        push(@afterArgumentList, "ec");
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
    $code .= "\n";
    $code .= "    RefPtr<${interfaceName}> impl = ${interfaceName}::createForJSConstructor(${argumentString});\n";
    $code .= "    v8::Handle<v8::Object> wrapper = args.Holder();\n";

    if ($interface->extendedAttributes->{"RaisesException"}) {
        $code .= "    if (ec)\n";
        $code .= "        goto fail;\n";
    }

    $code .= <<END;

    V8DOMWrapper::associateObjectWithWrapper(impl.release(), &${v8InterfaceName}Constructor::info, wrapper, args.GetIsolate(), WrapperConfiguration::Dependent);
    return wrapper;
END

    if ($raisesExceptions) {
        $code .= "    fail:\n";
        $code .= "    return setDOMException(ec, args.GetIsolate());\n";
    }

    $code .= "}\n";
    AddToImplContent($code);

    $code = <<END;

v8::Persistent<v8::FunctionTemplate> ${v8InterfaceName}Constructor::GetTemplate(v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    static v8::Persistent<v8::FunctionTemplate> cachedTemplate;
    if (!cachedTemplate.IsEmpty())
        return cachedTemplate;

    v8::HandleScope scope;
    v8::Local<v8::FunctionTemplate> result = v8::FunctionTemplate::New(${v8InterfaceName}ConstructorCallback);

    v8::Local<v8::ObjectTemplate> instance = result->InstanceTemplate();
    instance->SetInternalFieldCount(${v8InterfaceName}::internalFieldCount);
    result->SetClassName(v8::String::NewSymbol("${interfaceName}"));
    result->Inherit(${v8InterfaceName}::GetTemplate(isolate, currentWorldType));

    cachedTemplate = v8::Persistent<v8::FunctionTemplate>::New(isolate, result);
    return cachedTemplate;
}

END
    AddToImplContent($code);
}

sub GenerateConstructorHeader
{
    my $content = <<END;
    if (!args.IsConstructCall())
        return throwTypeError("DOM object constructor cannot be called as a function.", args.GetIsolate());

    if (ConstructorMode::current() == ConstructorMode::WrapExistingObject)
        return args.Holder();

END
    return $content;
}

sub GenerateBatchedAttributeData
{
    my $interface = shift;
    my $attributes = shift;
    my $code = "";
    my $interfaceName = $interface->name;

    foreach my $attribute (@$attributes) {
        my $conditionalString = GenerateConditionalString($attribute->signature);
        my $subCode = "";
        $subCode .= "#if ${conditionalString}\n" if $conditionalString;
        $subCode .= GenerateSingleBatchedAttribute($interfaceName, $attribute, ",", "");
        $subCode .= "#endif // ${conditionalString}\n" if $conditionalString;
        $code .= $subCode;
    }
    return $code;
}

sub GenerateSingleBatchedAttribute
{
    my $interfaceName = shift;
    my $attribute = shift;
    my $delimiter = shift;
    my $indent = shift;
    my $code = "";
    my $attrName = $attribute->signature->name;
    my $attrExt = $attribute->signature->extendedAttributes;

    my $accessControl = "v8::DEFAULT";
    if ($attrExt->{"DoNotCheckSecurityOnGetter"}) {
        $accessControl = "v8::ALL_CAN_READ";
    } elsif ($attrExt->{"DoNotCheckSecurityOnSetter"}) {
        $accessControl = "v8::ALL_CAN_WRITE";
    } elsif ($attrExt->{"DoNotCheckSecurity"}) {
        $accessControl = "v8::ALL_CAN_READ";
        if (!IsReadonly($attribute)) {
            $accessControl .= " | v8::ALL_CAN_WRITE";
        }
    }
    if ($attrExt->{"Unforgeable"}) {
        $accessControl .= " | v8::PROHIBITS_OVERWRITING";
    }
    $accessControl = "static_cast<v8::AccessControl>(" . $accessControl . ")";

    my $customAccessor = HasCustomGetter($attrExt) || HasCustomSetter($attrExt) || "";
    if ($customAccessor eq "VALUE_IS_MISSING") {
        # use the naming convension, interface + (capitalize) attr name
        $customAccessor = $interfaceName . "::" . $attrName;
    }

    my $getter;
    my $setter;
    my $getterForMainWorld;
    my $setterForMainWorld;
    my $propAttr = "v8::None";

    my $isConstructor = ($attribute->signature->type =~ /Constructor$/);

    # Check attributes.
    # As per Web IDL specification, constructor properties on the ECMAScript global object should be
    # configurable and should not be enumerable.
    if ($attrExt->{"NotEnumerable"} || $isConstructor) {
        $propAttr .= " | v8::DontEnum";
    }
    if ($attrExt->{"Unforgeable"} && !$isConstructor) {
        $propAttr .= " | v8::DontDelete";
    }

    my $on_proto = "0 /* on instance */";
    my $data = "0 /* no data */";

    # Constructor
    if ($isConstructor) {
        my $constructorType = $attribute->signature->type;
        $constructorType =~ s/Constructor$//;
        # $constructorType ~= /Constructor$/ indicates that it is NamedConstructor.
        # We do not generate the header file for NamedConstructor of class XXXX,
        # since we generate the NamedConstructor declaration into the header file of class XXXX.
        if ($constructorType !~ /Constructor$/ || $attribute->signature->extendedAttributes->{"CustomConstructor"}) {
            AddToImplIncludes("V8${constructorType}.h", $attribute->signature->extendedAttributes->{"Conditional"});
        }
        $data = "&V8${constructorType}::info";
        $getter = "${interfaceName}V8Internal::${interfaceName}ConstructorGetter";
        $setter = "${interfaceName}V8Internal::${interfaceName}ReplaceableAttrSetterCallback";
        $getterForMainWorld = "0";
        $setterForMainWorld = "0";
    } else {
        # Default Getter and Setter
        $getter = "${interfaceName}V8Internal::${attrName}AttrGetterCallback";
        $setter = "${interfaceName}V8Internal::${attrName}AttrSetterCallback";
        $getterForMainWorld = "${getter}ForMainWorld";
        $setterForMainWorld = "${setter}ForMainWorld";

        if (!HasCustomSetter($attrExt) && $attrExt->{"Replaceable"}) {
            $setter = "${interfaceName}V8Internal::${interfaceName}ReplaceableAttrSetterCallback";
            $setterForMainWorld = "0";
        }
    }

    # Read only attributes
    if (IsReadonly($attribute)) {
        $setter = "0";
        $setterForMainWorld = "0";
    }

    # An accessor can be installed on the proto
    if ($attrExt->{"OnProto"}) {
        $on_proto = "1 /* on proto */";
    }

    if (!$attrExt->{"PerWorldBindings"}) {
      $getterForMainWorld = "0";
      $setterForMainWorld = "0";
    }

    my $commentInfo = "Attribute '$attrName' (Type: '" . $attribute->type .
                      "' ExtAttr: '" . join(' ', keys(%{$attrExt})) . "')";

    $code .= $indent . "    \/\/ $commentInfo\n";
    $code .= $indent . "    {\"$attrName\", $getter, $setter, $getterForMainWorld, $setterForMainWorld, $data, $accessControl, static_cast<v8::PropertyAttribute>($propAttr), $on_proto}" . $delimiter . "\n";
    return $code;
}

sub IsStandardFunction
{
    my $interface = shift;
    my $function = shift;

    my $interfaceName = $interface->name;
    my $attrExt = $function->signature->extendedAttributes;
    return 0 if $attrExt->{"Unforgeable"};
    return 0 if $function->isStatic;
    return 0 if $attrExt->{"EnabledAtRuntime"};
    return 0 if $attrExt->{"EnabledPerContext"};
    return 0 if RequiresCustomSignature($function);
    return 0 if $attrExt->{"DoNotCheckSignature"};
    return 0 if ($attrExt->{"DoNotCheckSecurity"} && ($interface->extendedAttributes->{"CheckSecurity"} || $interfaceName eq "DOMWindow"));
    return 0 if $attrExt->{"NotEnumerable"};
    return 0 if $attrExt->{"ReadOnly"};
    return 1;
}

sub GenerateNonStandardFunction
{
    my $interface = shift;
    my $function = shift;
    my $code = "";

    my $interfaceName = $interface->name;
    my $attrExt = $function->signature->extendedAttributes;
    my $name = $function->signature->name;

    my $property_attributes = "v8::DontDelete";
    if ($attrExt->{"NotEnumerable"}) {
        $property_attributes .= " | v8::DontEnum";
    }
    if ($attrExt->{"ReadOnly"}) {
        $property_attributes .= " | v8::ReadOnly";
    }

    my $commentInfo = "Function '$name' (ExtAttr: '" . join(' ', keys(%{$attrExt})) . "')";

    my $template = "proto";
    if ($attrExt->{"Unforgeable"}) {
        $template = "instance";
    }
    if ($function->isStatic) {
        $template = "desc";
    }

    my $conditional = "";
    if ($attrExt->{"EnabledAtRuntime"}) {
        # Only call Set()/SetAccessor() if this method should be enabled
        my $enable_function = GetRuntimeEnableFunctionName($function->signature);
        $conditional = "if (${enable_function}())\n        ";
    }
    if ($attrExt->{"EnabledPerContext"}) {
        # Only call Set()/SetAccessor() if this method should be enabled
        my $enable_function = GetContextEnableFunction($function->signature);
        $conditional = "if (${enable_function}(impl->document()))\n        ";
    }

    if ($interface->extendedAttributes->{"CheckSecurity"} && $attrExt->{"DoNotCheckSecurity"}) {
        # Functions that are marked DoNotCheckSecurity are always readable but if they are changed
        # and then accessed on a different domain we do not return the underlying value but instead
        # return a new copy of the original function. This is achieved by storing the changed value
        # as hidden property.
        $code .= <<END;

    // $commentInfo
    ${conditional}$template->SetAccessor(v8::String::NewSymbol("$name"), ${interfaceName}V8Internal::${name}AttrGetterCallback, ${interfaceName}V8Internal::${interfaceName}DomainSafeFunctionSetter, v8Undefined(), v8::ALL_CAN_READ, static_cast<v8::PropertyAttribute>($property_attributes));
END
        return $code;
    }

    my $signature = "defaultSignature";
    if ($attrExt->{"DoNotCheckSignature"} || $function->isStatic) {
       $signature = "v8::Local<v8::Signature>()";
    }

    if (RequiresCustomSignature($function)) {
        $signature = "${name}Signature";
        $code .= "\n    // Custom Signature '$name'\n" . CreateCustomSignature($function);
    }

    if ($property_attributes eq "v8::DontDelete") {
        $property_attributes = "";
    } else {
        $property_attributes = ", static_cast<v8::PropertyAttribute>($property_attributes)";
    }

    if ($template eq "proto" && $conditional eq "" && $signature eq "defaultSignature" && $property_attributes eq "") {
        die "This shouldn't happen: Intraface '$interfaceName' $commentInfo\n";
    }

    my $functionLength = GetFunctionLength($function);

    my $conditionalString = GenerateConditionalString($function->signature);
    $code .= "#if ${conditionalString}\n" if $conditionalString;
    if ($function->signature->extendedAttributes->{"PerWorldBindings"}) {
        $code .= "    if (currentWorldType == MainWorld) {\n";
        $code .= "        ${conditional}$template->Set(v8::String::NewSymbol(\"$name\"), v8::FunctionTemplate::New(${interfaceName}V8Internal::${name}MethodCallbackForMainWorld, v8Undefined(), ${signature}, $functionLength)$property_attributes);\n";
        $code .= "    } else {\n";
        $code .= "        ${conditional}$template->Set(v8::String::NewSymbol(\"$name\"), v8::FunctionTemplate::New(${interfaceName}V8Internal::${name}MethodCallback, v8Undefined(), ${signature}, $functionLength)$property_attributes);\n";
        $code .= "    }\n";
    } else {
        $code .= "    ${conditional}$template->Set(v8::String::NewSymbol(\"$name\"), v8::FunctionTemplate::New(${interfaceName}V8Internal::${name}MethodCallback, v8Undefined(), ${signature}, $functionLength)$property_attributes);\n";
    }
    $code .= "#endif // ${conditionalString}\n" if $conditionalString;
    return $code;
}

sub GenerateImplementationIndexedProperty
{
    my $interface = shift;
    my $interfaceName = $interface->name;
    my $v8InterfaceName = "V8$interfaceName";

    my $indexedGetterfunction = GetIndexedGetterFunction($interface);
    my $hasCustomIndexedSetter = $interface->extendedAttributes->{"CustomIndexedSetter"};
    my $hasCustomIndexedGetter = $interface->extendedAttributes->{"CustomIndexedGetter"};

    my $hasEnumerator = $indexedGetterfunction || $hasCustomIndexedGetter;
    # FIXME: Remove the special cases. Interfaces that have indexedPropertyGetter should have indexedPropertyEnumerator.
    $hasEnumerator = 0 if $interfaceName eq "WebKitCSSKeyframesRule";
    $hasEnumerator = 0 if $interfaceName eq "HTMLAppletElement";
    $hasEnumerator = 0 if $interfaceName eq "HTMLEmbedElement";
    $hasEnumerator = 0 if $interfaceName eq "HTMLObjectElement";
    $hasEnumerator = 0 if $interfaceName eq "DOMWindow";
    $hasEnumerator = 0 if $interfaceName eq "Storage";

    if (!$indexedGetterfunction && !$hasCustomIndexedGetter) {
        return "";
    }

    AddToImplIncludes("bindings/v8/V8Collection.h");

    my $hasDeleter = $interface->extendedAttributes->{"CustomDeleteProperty"};
    my $setOn = "Instance";

    # V8 has access-check callback API (see ObjectTemplate::SetAccessCheckCallbacks) and it's used on DOMWindow
    # instead of deleters or enumerators. In addition, the getter should be set on prototype template, to
    # get implementation straight out of the DOMWindow prototype regardless of what prototype is actually set
    # on the object.
    if ($interfaceName eq "DOMWindow") {
        $setOn = "Prototype";
    }

    my $code = "";
    $code .= "    desc->${setOn}Template()->SetIndexedPropertyHandler(${v8InterfaceName}::indexedPropertyGetter";
    $code .= $hasCustomIndexedSetter ? ", ${v8InterfaceName}::indexedPropertySetter" : ", 0";
    $code .= ", 0"; # IndexedPropertyQuery -- not being used at the moment.
    $code .= $hasDeleter ? ", ${v8InterfaceName}::indexedPropertyDeleter" : ", 0";
    $code .= ", nodeCollectionIndexedPropertyEnumerator<${interfaceName}>" if $hasEnumerator;
    $code .= ");\n";

    if ($indexedGetterfunction && !$hasCustomIndexedGetter) {
        my $returnType = $indexedGetterfunction->signature->type;
        my $methodName = $indexedGetterfunction->signature->name;
        AddToImplIncludes("bindings/v8/V8Collection.h");
        my $jsValue = "";
        my $nativeType = GetNativeType($returnType);
        my $isNull = "";

        if (IsRefPtrType($returnType)) {
            AddToImplIncludes("V8$returnType.h");
            $isNull = "!element";
            $jsValue = NativeToJSValue($indexedGetterfunction->signature, "element.release()", "info.Holder()", "info.GetIsolate()", "info", "collection", "", "");
        } else {
            $isNull = "element.isNull()";
            $jsValue = NativeToJSValue($indexedGetterfunction->signature, "element", "info.Holder()", "info.GetIsolate()");
        }

        AddToImplContent(<<END);
v8::Handle<v8::Value> ${v8InterfaceName}::indexedPropertyGetter(uint32_t index, const v8::AccessorInfo& info)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(info.Holder()));
    ${interfaceName}* collection = toNative(info.Holder());
    $nativeType element = collection->$methodName(index);
    if ($isNull)
        return v8Undefined();
    return $jsValue;
}
END
    }
    return $code;
}

sub GenerateImplementationNamedPropertyGetter
{
    my $interface = shift;

    my $subCode = "";
    my $interfaceName = $interface->name;
    my $v8InterfaceName = "V8$interfaceName";

    my $namedGetterFunction = GetNamedGetterFunction($interface);
    my $hasCustomNamedGetter = $interface->extendedAttributes->{"CustomNamedGetter"};

    if ($namedGetterFunction && !$hasCustomNamedGetter) {
        my $returnType = $namedGetterFunction->signature->type;
        my $methodName = $namedGetterFunction->signature->name;
        AddToImplIncludes("bindings/v8/V8Collection.h");
        AddToImplIncludes("V8$returnType.h");
        $subCode .= <<END;
    desc->InstanceTemplate()->SetNamedPropertyHandler(${v8InterfaceName}::namedPropertyGetter, 0, 0, 0, 0);
END

        my $code .= <<END;
v8::Handle<v8::Value> ${v8InterfaceName}::namedPropertyGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    if (!info.Holder()->GetRealNamedPropertyInPrototypeChain(name).IsEmpty())
        return v8Undefined();
    if (info.Holder()->HasRealNamedCallbackProperty(name))
        return v8Undefined();

    v8::Local<v8::Object> object = info.Holder();
    ASSERT(V8DOMWrapper::maybeDOMWrapper(object));
    ASSERT(toWrapperTypeInfo(object) != &V8Node::info);
    $interfaceName* collection = toNative(object);

    AtomicString propertyName = toWebCoreAtomicStringWithNullCheck(name);
    RefPtr<$returnType> element = collection->$methodName(propertyName);

    if (!element)
        return v8Undefined();

    return toV8Fast(element.release(), info, collection);
}

END
        AddToImplContent($code);
    }

    if ($hasCustomNamedGetter) {
        my $hasCustomNamedSetter = $interface->extendedAttributes->{"CustomNamedSetter"};
        my $hasDeleter = $interface->extendedAttributes->{"CustomDeleteProperty"};
        my $hasEnumerator = $interface->extendedAttributes->{"CustomEnumerateProperty"};
        my $setOn = "Instance";

        # V8 has access-check callback API (see ObjectTemplate::SetAccessCheckCallbacks) and it's used on DOMWindow
        # instead of deleters or enumerators. In addition, the getter should be set on prototype template, to
        # get implementation straight out of the DOMWindow prototype regardless of what prototype is actually set
        # on the object.
        if ($interfaceName eq "DOMWindow") {
            $setOn = "Prototype";
        }

        $subCode .= "    desc->${setOn}Template()->SetNamedPropertyHandler(${v8InterfaceName}::namedPropertyGetter, ";
        $subCode .= $hasCustomNamedSetter ? "${v8InterfaceName}::namedPropertySetter, " : "0, ";
        # If there is a custom enumerator, there MUST be custom query to properly communicate property attributes.
        $subCode .= $hasEnumerator ? "${v8InterfaceName}::namedPropertyQuery, " : "0, ";
        $subCode .= $hasDeleter ? "${v8InterfaceName}::namedPropertyDeleter, " : "0, ";
        $subCode .= $hasEnumerator ? "${v8InterfaceName}::namedPropertyEnumerator" : "0";
        $subCode .= ");\n";
    }

    return $subCode;
}

sub GenerateImplementationCustomCall
{
    my $interface = shift;
    my $code = "";

    my $interfaceName = $interface->name;

    if ($interface->extendedAttributes->{"CustomCall"}) {
        $code .= "    desc->InstanceTemplate()->SetCallAsFunctionHandler(V8${interfaceName}::callAsFunctionCallback);\n";
    }
    return $code;
}

sub GenerateImplementationMasqueradesAsUndefined
{
    my $interface = shift;
    my $code = "";

    if ($interface->extendedAttributes->{"MasqueradesAsUndefined"})
    {
        $code .= "    desc->InstanceTemplate()->MarkAsUndetectable();\n";
    }
    return $code;
}

sub GenerateImplementation
{
    my $object = shift;
    my $interface = shift;
    my $interfaceName = $interface->name;
    my $visibleInterfaceName = GetVisibleInterfaceName($interface);
    my $v8InterfaceName = "V8$interfaceName";
    my $nativeType = GetNativeTypeForConversions($interface);
    my $vtableNameGnu = GetGnuVTableNameForInterface($interface);
    my $vtableRefGnu = GetGnuVTableRefForInterface($interface);
    my $vtableRefWin = GetWinVTableRefForInterface($interface);

    # - Add default header template
    push(@implContentHeader, GenerateImplementationContentHeader($interface));

    AddToImplIncludes("bindings/v8/V8Binding.h");
    AddToImplIncludes("bindings/v8/V8DOMWrapper.h");
    AddToImplIncludes("core/dom/ContextFeatures.h");
    AddToImplIncludes("RuntimeEnabledFeatures.h");

    AddIncludesForType($interfaceName);

    my $toActiveDOMObject = InheritsExtendedAttribute($interface, "ActiveDOMObject") ? "${v8InterfaceName}::toActiveDOMObject" : "0";
    my $toEventTarget = InheritsExtendedAttribute($interface, "EventTarget") ? "${v8InterfaceName}::toEventTarget" : "0";
    my $rootForGC = NeedsCustomOpaqueRootForGC($interface) ? "${v8InterfaceName}::opaqueRootForGC" : "0";

    # Find the super descriptor.
    my $parentClass = "";
    my $parentClassTemplate = "";
    foreach (@{$interface->parents}) {
        my $parent = $_;
        AddToImplIncludes("V8${parent}.h");
        $parentClass = "V8" . $parent;
        $parentClassTemplate = $parentClass . "::GetTemplate(isolate, currentWorldType)";
        last;
    }

    AddToImplContentInternals(<<END) if $vtableNameGnu;
#if ENABLE(BINDING_INTEGRITY)
#if defined(OS_WIN)
#pragma warning(disable: 4483)
extern "C" { extern void (*const ${vtableRefWin}[])(); }
#else
extern "C" { extern void* ${vtableNameGnu}[]; }
#endif
#endif // ENABLE(BINDING_INTEGRITY)

END

    AddToImplContentInternals("namespace WebCore {\n\n");

    AddToImplContentInternals(<<END) if $vtableNameGnu;
#if ENABLE(BINDING_INTEGRITY)
// This checks if a DOM object that is about to be wrapped is valid.
// Specifically, it checks that a vtable of the DOM object is equal to
// a vtable of an expected class.
// Due to a dangling pointer, the DOM object you are wrapping might be
// already freed or realloced. If freed, the check will fail because
// a free list pointer should be stored at the head of the DOM object.
// If realloced, the check will fail because the vtable of the DOM object
// differs from the expected vtable (unless the same class of DOM object
// is realloced on the slot).
inline void checkTypeOrDieTrying(${nativeType}* object)
{
    void* actualVTablePointer = *(reinterpret_cast<void**>(object));
#if defined(OS_WIN)
    void* expectedVTablePointer = reinterpret_cast<void*>(${vtableRefWin});
#else
    void* expectedVTablePointer = ${vtableRefGnu};
#endif
    if (actualVTablePointer != expectedVTablePointer)
        CRASH();
}
#endif // ENABLE(BINDING_INTEGRITY)

END

    my $parentClassInfo = $parentClass ? "&${parentClass}::info" : "0";
    my $WrapperTypePrototype = $interface->isException ? "WrapperTypeErrorPrototype" : "WrapperTypeObjectPrototype";

    if (!IsSVGTypeNeedingTearOff($interfaceName)) {
        push(@implContentInternals, <<END);
#if defined(OS_WIN)
// In ScriptWrappable, the use of extern function prototypes inside templated static methods has an issue on windows.
// These prototypes do not pick up the surrounding namespace, so drop out of WebCore as a workaround.
} // namespace WebCore
using WebCore::ScriptWrappable;
using WebCore::${v8InterfaceName};
END
       push(@implContentInternals, <<END) if (GetNamespaceForInterface($interface) eq "WebCore");
using WebCore::${interfaceName};
END
      push(@implContentInternals, <<END);
#endif
void initializeScriptWrappableForInterface(${interfaceName}* object)
{
    if (ScriptWrappable::wrapperCanBeStoredInObject(object))
        ScriptWrappable::setTypeInfoInObject(object, &${v8InterfaceName}::info);
}
#if defined(OS_WIN)
namespace WebCore {
#endif
END
    }

    my $code = "WrapperTypeInfo ${v8InterfaceName}::info = { ${v8InterfaceName}::GetTemplate, ${v8InterfaceName}::derefObject, $toActiveDOMObject, $toEventTarget, ";
    $code .= "$rootForGC, ${v8InterfaceName}::installPerContextPrototypeProperties, $parentClassInfo, $WrapperTypePrototype };\n\n";
    AddToImplContentInternals($code);

    AddToImplContentInternals("namespace ${interfaceName}V8Internal {\n\n");
    AddToImplContentInternals("template <typename T> void V8_USE(T) { }\n\n");

    my $hasConstructors = 0;
    my $hasReplaceable = 0;

    # Generate property accessors for attributes.
    for (my $index = 0; $index < @{$interface->attributes}; $index++) {
        my $attribute = @{$interface->attributes}[$index];
        my $attrType = $attribute->signature->type;
        my $attrExt = $attribute->signature->extendedAttributes;

        # Generate special code for the constructor attributes.
        if ($attrType =~ /Constructor$/) {
            if (!HasCustomGetter($attrExt)) {
                $hasConstructors = 1;
            }
            next;
        }

        if ($attrType eq "EventListener" && $interfaceName eq "DOMWindow") {
            $attrExt->{"OnProto"} = 1;
        }

        if ($attrType eq "SerializedScriptValue") {
            AddToImplIncludes("bindings/v8/SerializedScriptValue.h");
        }

        GenerateNormalAttrGetter($attribute, $interface, "");
        GenerateNormalAttrGetterCallback($attribute, $interface, "");
        if ($attrExt->{"PerWorldBindings"}) {
            GenerateNormalAttrGetter($attribute, $interface, "ForMainWorld");
            GenerateNormalAttrGetterCallback($attribute, $interface, "ForMainWorld");
        }
        if (!HasCustomSetter($attrExt) && $attrExt->{"Replaceable"}) {
            $hasReplaceable = 1;
        } elsif (!IsReadonly($attribute)) {
            GenerateNormalAttrSetter($attribute, $interface, "");
            GenerateNormalAttrSetterCallback($attribute, $interface, "");
            if ($attrExt->{"PerWorldBindings"}) {
              GenerateNormalAttrSetter($attribute, $interface, "ForMainWorld");
              GenerateNormalAttrSetterCallback($attribute, $interface, "ForMainWorld");
            }
        }
    }

    if ($hasConstructors) {
        GenerateConstructorGetter($interface);
    }

    if ($hasConstructors || $hasReplaceable) {
        GenerateReplaceableAttrSetter($interface);
        GenerateReplaceableAttrSetterCallback($interface);
    }

    if (NeedsCustomOpaqueRootForGC($interface)) {
        GenerateOpaqueRootForGC($interface);
    }

    if ($interface->extendedAttributes->{"CheckSecurity"} && $interface->name ne "DOMWindow") {
        GenerateSecurityCheckFunctions($interface);
    }

    if ($interface->extendedAttributes->{"TypedArray"}) {
        my $viewType = GetTypeNameOfExternalTypedArray($interface);
        AddToImplContent(<<END);
v8::Handle<v8::Object> wrap($interfaceName* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    v8::Handle<v8::Object> wrapper = ${v8InterfaceName}::createWrapper(impl, creationContext, isolate);
    if (!wrapper.IsEmpty())
        wrapper->SetIndexedPropertiesToExternalArrayData(impl->baseAddress(), $viewType, impl->length());
    return wrapper;
}

END
    }

    my $indexer;
    my @enabledPerContextFunctions;
    my @normalFunctions;
    my $needsDomainSafeFunctionSetter = 0;
    # Generate methods for functions.
    foreach my $function (@{$interface->functions}) {
        GenerateFunction($function, $interface, "");
        if ($function->signature->extendedAttributes->{"PerWorldBindings"}) {
            GenerateFunction($function, $interface, "ForMainWorld");
        }
        if ($function->{overloadIndex} == @{$function->{overloads}}) {
            if ($function->{overloadIndex} > 1) {
                GenerateOverloadedFunction($function, $interface, "");
                if ($function->signature->extendedAttributes->{"PerWorldBindings"}) {
                    GenerateOverloadedFunction($function, $interface, "ForMainWorld");
                }
            }
            GenerateFunctionCallback($function, $interface, "");
            if ($function->signature->extendedAttributes->{"PerWorldBindings"}) {
                GenerateFunctionCallback($function, $interface, "ForMainWorld");
            }
        }

        if ($function->signature->name eq "item") {
            $indexer = $function->signature;
        }

        # If the function does not need domain security check, we need to
        # generate an access getter that returns different function objects
        # for different calling context.
        if ($interface->extendedAttributes->{"CheckSecurity"} && $function->signature->extendedAttributes->{"DoNotCheckSecurity"}) {
            if (!HasCustomMethod($function->signature->extendedAttributes) || $function->{overloadIndex} == 1) {
                GenerateDomainSafeFunctionGetter($function, $interfaceName);
                $needsDomainSafeFunctionSetter = 1;
            }
        }

        # Separate out functions that are enabled per context so we can process them specially.
        if ($function->signature->extendedAttributes->{"EnabledPerContext"}) {
            push(@enabledPerContextFunctions, $function);
        } else {
            push(@normalFunctions, $function);
        }
    }

    if ($needsDomainSafeFunctionSetter) {
        GenerateDomainSafeFunctionSetter($interfaceName);
    }

    # Attributes
    my $attributes = $interface->attributes;

    # For the DOMWindow interface we partition the attributes into the
    # ones that disallows shadowing and the rest.
    my @disallowsShadowing;
    # Also separate out attributes that are enabled at runtime so we can process them specially.
    my @enabledAtRuntimeAttributes;
    my @enabledPerContextAttributes;
    my @normalAttributes;
    foreach my $attribute (@$attributes) {

        if ($interfaceName eq "DOMWindow" && $attribute->signature->extendedAttributes->{"Unforgeable"}) {
            push(@disallowsShadowing, $attribute);
        } elsif ($attribute->signature->extendedAttributes->{"EnabledAtRuntime"} || $attribute->signature->extendedAttributes->{"EnabledPerContext"}) {
            if ($attribute->signature->extendedAttributes->{"EnabledPerContext"}) {
                push(@enabledPerContextAttributes, $attribute);
            }
            if ($attribute->signature->extendedAttributes->{"EnabledAtRuntime"}) {
                push(@enabledAtRuntimeAttributes, $attribute);
            }
        } else {
            push(@normalAttributes, $attribute);
        }
    }
    $attributes = \@normalAttributes;
    # Put the attributes that disallow shadowing on the shadow object.
    if (@disallowsShadowing) {
        my $code = "";
        $code .= "static const V8DOMConfiguration::BatchedAttribute shadowAttrs[] = {\n";
        $code .= GenerateBatchedAttributeData($interface, \@disallowsShadowing);
        $code .= "};\n\n";
        AddToImplContent($code);
    }

    my $has_attributes = 0;
    if (@$attributes) {
        $has_attributes = 1;
        my $code = "";
        $code .= "static const V8DOMConfiguration::BatchedAttribute ${v8InterfaceName}Attrs[] = {\n";
        $code .= GenerateBatchedAttributeData($interface, $attributes);
        $code .= "};\n\n";
        AddToImplContent($code);
    }

    # Setup table of standard callback functions
    my $num_callbacks = 0;
    my $has_callbacks = 0;
    $code = "";
    foreach my $function (@normalFunctions) {
        # Only one table entry is needed for overloaded methods:
        next if $function->{overloadIndex} > 1;
        # Don't put any nonstandard functions into this table:
        next if !IsStandardFunction($interface, $function);
        if (!$has_callbacks) {
            $has_callbacks = 1;
            $code .= "static const V8DOMConfiguration::BatchedMethod ${v8InterfaceName}Methods[] = {\n";
        }
        my $name = $function->signature->name;
        my $methodForMainWorld = "0";
        if ($function->signature->extendedAttributes->{"PerWorldBindings"}) {
            $methodForMainWorld = "${interfaceName}V8Internal::${name}MethodCallbackForMainWorld";
        }
        my $functionLength = GetFunctionLength($function);
        my $conditionalString = GenerateConditionalString($function->signature);
        $code .= "#if ${conditionalString}\n" if $conditionalString;
        $code .= <<END;
    {"$name", ${interfaceName}V8Internal::${name}MethodCallback, ${methodForMainWorld}, ${functionLength}},
END
        $code .= "#endif\n" if $conditionalString;
        $num_callbacks++;
    }
    $code .= "};\n\n"  if $has_callbacks;
    AddToImplContent($code);

    # Setup constants
    my $has_constants = 0;
    my @constantsEnabledAtRuntime;
    $code = "";
    if (@{$interface->constants}) {
        $has_constants = 1;
        $code .= "static const V8DOMConfiguration::BatchedConstant ${v8InterfaceName}Consts[] = {\n";
    }
    foreach my $constant (@{$interface->constants}) {
        my $name = $constant->name;
        my $value = $constant->value;
        my $attrExt = $constant->extendedAttributes;
        my $implementedBy = $attrExt->{"ImplementedBy"};
        if ($implementedBy) {
            AddInterfaceToImplIncludes($implementedBy);
        }
        if ($attrExt->{"EnabledAtRuntime"}) {
            push(@constantsEnabledAtRuntime, $constant);
        } else {
            my $conditionalString = GenerateConditionalString($constant);
            $code .= "#if ${conditionalString}\n" if $conditionalString;
            # If the value we're dealing with is a hex number, preprocess it into a signed integer
            # here, rather than running static_cast<signed int> in the generated code.
            if (substr($value, 0, 2) eq "0x") {
              $value = unpack('i', pack('I', hex($value)));
            }
            $code .= <<END;
    {"${name}", $value},
END
            $code .= "#endif\n" if $conditionalString;
        }
    }
    if ($has_constants) {
        $code .= "};\n\n";
        $code .= join "", GenerateCompileTimeCheckForEnumsIfNeeded($interface);
        AddToImplContent($code);
    }

    if (!HasCustomConstructor($interface)) {
        if ($interface->extendedAttributes->{"NamedConstructor"}) {
            GenerateNamedConstructor(@{$interface->constructors}[0], $interface);
        } elsif ($interface->extendedAttributes->{"Constructor"}) {
            GenerateConstructor($interface);
        } elsif (IsConstructorTemplate($interface, "Event")) {
            GenerateEventConstructor($interface);
        } elsif (IsConstructorTemplate($interface, "TypedArray")) {
            GenerateTypedArrayConstructor($interface);
        }
    }
    if (IsConstructable($interface)) {
        GenerateConstructorCallback($interface);
    }

    AddToImplContentInternals("} // namespace ${interfaceName}V8Internal\n\n");

    my $access_check = "";
    if ($interface->extendedAttributes->{"CheckSecurity"} && $interfaceName ne "DOMWindow") {
        $access_check = "instance->SetAccessCheckCallbacks(${interfaceName}V8Internal::namedSecurityCheck, ${interfaceName}V8Internal::indexedSecurityCheck, v8::External::New(&${v8InterfaceName}::info));";
    }

    # For the DOMWindow interface, generate the shadow object template
    # configuration method.
    if ($interfaceName eq "DOMWindow") {
        AddToImplContent(<<END);
static v8::Persistent<v8::ObjectTemplate> ConfigureShadowObjectTemplate(v8::Persistent<v8::ObjectTemplate> templ, v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    V8DOMConfiguration::batchConfigureAttributes(templ, v8::Handle<v8::ObjectTemplate>(), shadowAttrs, WTF_ARRAY_LENGTH(shadowAttrs), isolate, currentWorldType);

    // Install a security handler with V8.
    templ->SetAccessCheckCallbacks(V8DOMWindow::namedSecurityCheckCustom, V8DOMWindow::indexedSecurityCheckCustom, v8::External::New(&V8DOMWindow::info));
    templ->SetInternalFieldCount(V8DOMWindow::internalFieldCount);
    return templ;
}
END
    }

    if (!$parentClassTemplate) {
        $parentClassTemplate = "v8::Persistent<v8::FunctionTemplate>()";
    }

    # Generate the template configuration method
    $code =  <<END;
static v8::Persistent<v8::FunctionTemplate> Configure${v8InterfaceName}Template(v8::Persistent<v8::FunctionTemplate> desc, v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    desc->ReadOnlyPrototype();

    v8::Local<v8::Signature> defaultSignature;
END
    if ($interface->extendedAttributes->{"EnabledAtRuntime"}) {
        my $enable_function = GetRuntimeEnableFunctionName($interface);
        $code .= <<END;
    if (!${enable_function}())
        defaultSignature = V8DOMConfiguration::configureTemplate(desc, \"\", $parentClassTemplate, ${v8InterfaceName}::internalFieldCount, 0, 0, 0, 0, isolate, currentWorldType);
    else
END
    }
    $code .=  <<END;
    defaultSignature = V8DOMConfiguration::configureTemplate(desc, \"${visibleInterfaceName}\", $parentClassTemplate, ${v8InterfaceName}::internalFieldCount,
END
    # Set up our attributes if we have them
    if ($has_attributes) {
        $code .= <<END;
        ${v8InterfaceName}Attrs, WTF_ARRAY_LENGTH(${v8InterfaceName}Attrs),
END
    } else {
        $code .= <<END;
        0, 0,
END
    }

    if ($has_callbacks) {
        $code .= <<END;
        ${v8InterfaceName}Methods, WTF_ARRAY_LENGTH(${v8InterfaceName}Methods), isolate, currentWorldType);
END
    } else {
        $code .= <<END;
        0, 0, isolate, currentWorldType);
END
    }

    AddToImplIncludes("wtf/UnusedParam.h");
    $code .= <<END;
    UNUSED_PARAM(defaultSignature); // In some cases, it will not be used.
END

    if (IsConstructable($interface)) {
        $code .= "    desc->SetCallHandler(${v8InterfaceName}::constructorCallback);\n";
        my $interfaceLength = GetInterfaceLength($interface);
        $code .= "    desc->SetLength(${interfaceLength});\n";
    }

    if ($access_check or @enabledAtRuntimeAttributes or @normalFunctions or $has_constants) {
        $code .=  <<END;
    v8::Local<v8::ObjectTemplate> instance = desc->InstanceTemplate();
    v8::Local<v8::ObjectTemplate> proto = desc->PrototypeTemplate();
    UNUSED_PARAM(instance); // In some cases, it will not be used.
    UNUSED_PARAM(proto); // In some cases, it will not be used.
END
    }

    if ($access_check) {
        $code .=  "    $access_check\n";
    }

    # Setup the enable-at-runtime attrs if we have them
    foreach my $runtime_attr (@enabledAtRuntimeAttributes) {
        next if grep { $_ eq $runtime_attr } @enabledPerContextAttributes;
        my $enable_function = GetRuntimeEnableFunctionName($runtime_attr->signature);
        my $conditionalString = GenerateConditionalString($runtime_attr->signature);
        $code .= "\n#if ${conditionalString}\n" if $conditionalString;
        $code .= "    if (${enable_function}()) {\n";
        $code .= "        static const V8DOMConfiguration::BatchedAttribute attrData =\\\n";
        $code .= GenerateSingleBatchedAttribute($interfaceName, $runtime_attr, ";", "    ");
        $code .= <<END;
        V8DOMConfiguration::configureAttribute(instance, proto, attrData, isolate, currentWorldType);
    }
END
        $code .= "\n#endif // ${conditionalString}\n" if $conditionalString;
    }

    # Setup the enable-at-runtime constants if we have them
    foreach my $runtime_const (@constantsEnabledAtRuntime) {
        my $enable_function = GetRuntimeEnableFunctionName($runtime_const);
        my $conditionalString = GenerateConditionalString($runtime_const);
        my $name = $runtime_const->name;
        my $value = $runtime_const->value;
        $code .= "\n#if ${conditionalString}\n" if $conditionalString;
        $code .= "    if (${enable_function}()) {\n";
        $code .= <<END;
        static const V8DOMConfiguration::BatchedConstant constData = {"${name}", static_cast<signed int>(${value})};
        V8DOMConfiguration::batchConfigureConstants(desc, proto, &constData, 1, isolate);
END
        $code .= "    }\n";
        $code .= "\n#endif // ${conditionalString}\n" if $conditionalString;
    }

    $code .= GenerateImplementationIndexedProperty($interface);
    $code .= GenerateImplementationNamedPropertyGetter($interface);
    $code .= GenerateImplementationCustomCall($interface);
    $code .= GenerateImplementationMasqueradesAsUndefined($interface);

    # Define our functions with Set() or SetAccessor()
    my $total_functions = 0;
    foreach my $function (@normalFunctions) {
        # Only one accessor is needed for overloaded methods:
        next if $function->{overloadIndex} > 1;

        $total_functions++;
        next if IsStandardFunction($interface, $function);
        $code .= GenerateNonStandardFunction($interface, $function);
        $num_callbacks++;
    }

    die "Wrong number of callbacks generated for $interfaceName ($num_callbacks, should be $total_functions)" if $num_callbacks != $total_functions;

    if ($has_constants) {
        $code .= <<END;
    V8DOMConfiguration::batchConfigureConstants(desc, proto, ${v8InterfaceName}Consts, WTF_ARRAY_LENGTH(${v8InterfaceName}Consts), isolate);
END
    }

    # Special cases
    if ($interfaceName eq "DOMWindow") {
        $code .= <<END;

    proto->SetInternalFieldCount(V8DOMWindow::internalFieldCount);
    desc->SetHiddenPrototype(true);
    instance->SetInternalFieldCount(V8DOMWindow::internalFieldCount);
    // Set access check callbacks, but turned off initially.
    // When a context is detached from a frame, turn on the access check.
    // Turning on checks also invalidates inline caches of the object.
    instance->SetAccessCheckCallbacks(V8DOMWindow::namedSecurityCheckCustom, V8DOMWindow::indexedSecurityCheckCustom, v8::External::New(&V8DOMWindow::info), false);
END
    }
    if ($interfaceName eq "HTMLDocument" or $interfaceName eq "DedicatedWorkerContext" or $interfaceName eq "SharedWorkerContext") {
        $code .= <<END;
    desc->SetHiddenPrototype(true);
END
    }
    if ($interfaceName eq "Location") {
        $code .= <<END;

    // For security reasons, these functions are on the instance instead
    // of on the prototype object to ensure that they cannot be overwritten.
    instance->SetAccessor(v8::String::NewSymbol("reload"), V8Location::reloadAttrGetterCustom, 0, v8Undefined(), v8::ALL_CAN_READ, static_cast<v8::PropertyAttribute>(v8::DontDelete | v8::ReadOnly));
    instance->SetAccessor(v8::String::NewSymbol("replace"), V8Location::replaceAttrGetterCustom, 0, v8Undefined(), v8::ALL_CAN_READ, static_cast<v8::PropertyAttribute>(v8::DontDelete | v8::ReadOnly));
    instance->SetAccessor(v8::String::NewSymbol("assign"), V8Location::assignAttrGetterCustom, 0, v8Undefined(), v8::ALL_CAN_READ, static_cast<v8::PropertyAttribute>(v8::DontDelete | v8::ReadOnly));
END
    }

    $code .= <<END;

    // Custom toString template
    desc->Set(v8::String::NewSymbol("toString"), V8PerIsolateData::current()->toStringTemplate());
    return desc;
}

END
    AddToImplContent($code);

    AddToImplContent(<<END);
v8::Persistent<v8::FunctionTemplate> ${v8InterfaceName}::GetTemplate(v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    V8PerIsolateData::TemplateMap::iterator result = data->templateMap(currentWorldType).find(&info);
    if (result != data->templateMap(currentWorldType).end())
        return result->value;

    v8::HandleScope handleScope;
    v8::Persistent<v8::FunctionTemplate> templ =
        Configure${v8InterfaceName}Template(data->rawTemplate(&info, currentWorldType), isolate, currentWorldType);
    data->templateMap(currentWorldType).add(&info, templ);
    return templ;
}

END
    AddToImplContent(<<END);
bool ${v8InterfaceName}::HasInstance(v8::Handle<v8::Value> value, v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    return V8PerIsolateData::from(isolate)->hasInstance(&info, value, currentWorldType);
}

END
    AddToImplContent(<<END);
bool ${v8InterfaceName}::HasInstanceInAnyWorld(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    return V8PerIsolateData::from(isolate)->hasInstance(&info, value, MainWorld)
        || V8PerIsolateData::from(isolate)->hasInstance(&info, value, IsolatedWorld)
        || V8PerIsolateData::from(isolate)->hasInstance(&info, value, WorkerWorld);
}

END

    if (@enabledPerContextAttributes) {
        my $code = "";
        $code .= <<END;
void ${v8InterfaceName}::installPerContextProperties(v8::Handle<v8::Object> instance, ${nativeType}* impl, v8::Isolate* isolate)
{
    v8::Local<v8::Object> proto = v8::Local<v8::Object>::Cast(instance->GetPrototype());
END

        # Setup the enable-by-settings attrs if we have them
        foreach my $runtimeAttr (@enabledPerContextAttributes) {
            my $enableFunction = GetContextEnableFunction($runtimeAttr->signature);
            my $conditionalString = GenerateConditionalString($runtimeAttr->signature);
            $code .= "\n#if ${conditionalString}\n" if $conditionalString;
            if (grep { $_ eq $runtimeAttr } @enabledAtRuntimeAttributes) {
                my $runtimeEnableFunction = GetRuntimeEnableFunctionName($runtimeAttr->signature);
                $code .= "    if (${enableFunction}(impl->document()) && ${runtimeEnableFunction}()) {\n";
            } else {
                $code .= "    if (${enableFunction}(impl->document())) {\n";
            }

            $code .= "        static const V8DOMConfiguration::BatchedAttribute attrData =\\\n";
            $code .= GenerateSingleBatchedAttribute($interfaceName, $runtimeAttr, ";", "    ");
            $code .= <<END;
        V8DOMConfiguration::configureAttribute(instance, proto, attrData, isolate);
END
            $code .= "    }\n";
            $code .= "#endif // ${conditionalString}\n" if $conditionalString;
        }
        $code .= <<END;
}

END
        AddToImplContent($code);
    }

    if (@enabledPerContextFunctions) {
        my $code = "";
        $code .= <<END;
void ${v8InterfaceName}::installPerContextPrototypeProperties(v8::Handle<v8::Object> proto, v8::Isolate* isolate)
{
    UNUSED_PARAM(proto);
END
        # Setup the enable-by-settings functions if we have them
        $code .=  <<END;
    v8::Local<v8::Signature> defaultSignature = v8::Signature::New(GetTemplate(isolate, worldType(isolate)));
    UNUSED_PARAM(defaultSignature); // In some cases, it will not be used.

    ScriptExecutionContext* context = toScriptExecutionContext(proto->CreationContext());
END

        foreach my $runtimeFunc (@enabledPerContextFunctions) {
            my $enableFunction = GetContextEnableFunction($runtimeFunc->signature);
            my $functionLength = GetFunctionLength($runtimeFunc);
            my $conditionalString = GenerateConditionalString($runtimeFunc->signature);
            $code .= "\n#if ${conditionalString}\n" if $conditionalString;
            $code .= "    if (context && context->isDocument() && ${enableFunction}(toDocument(context))) {\n";
            my $name = $runtimeFunc->signature->name;
            $code .= <<END;
        proto->Set(v8::String::NewSymbol("${name}"), v8::FunctionTemplate::New(${interfaceName}V8Internal::${name}MethodCallback, v8Undefined(), defaultSignature, $functionLength)->GetFunction());
END
            $code .= "    }\n";
            $code .= "#endif // ${conditionalString}\n" if $conditionalString;
        }

        $code .= <<END;
}

END
        AddToImplContent($code);
    }

    if (InheritsExtendedAttribute($interface, "ActiveDOMObject")) {
        # MessagePort is handled like an active dom object even though it doesn't inherit
        # from ActiveDOMObject, so don't try to cast it to ActiveDOMObject.
        my $returnValue = $interfaceName eq "MessagePort" ? "0" : "toNative(object)";
        AddToImplContent(<<END);
ActiveDOMObject* ${v8InterfaceName}::toActiveDOMObject(v8::Handle<v8::Object> object)
{
    return $returnValue;
}

END
    }

    if (InheritsExtendedAttribute($interface, "EventTarget")) {
        AddToImplContent(<<END);
EventTarget* ${v8InterfaceName}::toEventTarget(v8::Handle<v8::Object> object)
{
    return toNative(object);
}

END
    }

    if ($interfaceName eq "DOMWindow") {
        AddToImplContent(<<END);
v8::Persistent<v8::ObjectTemplate> V8DOMWindow::GetShadowObjectTemplate(v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    if (currentWorldType == MainWorld) {
        static v8::Persistent<v8::ObjectTemplate> V8DOMWindowShadowObjectCacheForMainWorld;
        if (V8DOMWindowShadowObjectCacheForMainWorld.IsEmpty()) {
            V8DOMWindowShadowObjectCacheForMainWorld = v8::Persistent<v8::ObjectTemplate>::New(isolate, v8::ObjectTemplate::New());
            ConfigureShadowObjectTemplate(V8DOMWindowShadowObjectCacheForMainWorld, isolate, currentWorldType);
        }
        return V8DOMWindowShadowObjectCacheForMainWorld;
    } else {
        static v8::Persistent<v8::ObjectTemplate> V8DOMWindowShadowObjectCacheForNonMainWorld;
        if (V8DOMWindowShadowObjectCacheForNonMainWorld.IsEmpty()) {
            V8DOMWindowShadowObjectCacheForNonMainWorld = v8::Persistent<v8::ObjectTemplate>::New(isolate, v8::ObjectTemplate::New());
            ConfigureShadowObjectTemplate(V8DOMWindowShadowObjectCacheForNonMainWorld, isolate, currentWorldType);
        }
        return V8DOMWindowShadowObjectCacheForNonMainWorld;
    }
}

END
    }

    GenerateToV8Converters($interface, $v8InterfaceName, $nativeType);

    AddToImplContent(<<END);
void ${v8InterfaceName}::derefObject(void* object)
{
    static_cast<${nativeType}*>(object)->deref();
}

END
    AddToImplContent(<<END);
} // namespace WebCore
END

    my $conditionalString = GenerateConditionalString($interface);
    AddToImplContent("\n#endif // ${conditionalString}\n") if $conditionalString;
}

sub GenerateHeaderContentHeader
{
    my $interface = shift;
    my $v8InterfaceName = "V8" . $interface->name;
    my $conditionalString = GenerateConditionalString($interface);

    my @headerContentHeader = split("\r", $headerTemplate);

    push(@headerContentHeader, "\n#ifndef ${v8InterfaceName}" . "_h\n");
    push(@headerContentHeader, "#define ${v8InterfaceName}" . "_h\n\n");
    push(@headerContentHeader, "#if ${conditionalString}\n") if $conditionalString;
    return join "", @headerContentHeader;
}

sub GenerateImplementationContentHeader
{
    my $interface = shift;
    my $v8InterfaceName = "V8" . $interface->name;
    my $conditionalString = GenerateConditionalString($interface);

    my @implContentHeader = split("\r", $headerTemplate);

    push(@implContentHeader, "\n#include \"config.h\"\n");
    push(@implContentHeader, "#if ${conditionalString}\n") if $conditionalString;
    push(@implContentHeader, "#include \"${v8InterfaceName}.h\"\n\n");
    return @implContentHeader;
}

sub GenerateCallbackHeader
{
    my $object = shift;
    my $interface = shift;

    my $interfaceName = $interface->name;
    my $v8InterfaceName = "V8$interfaceName";

    $header{root}->addFooter("\n");

    my @unsortedIncludes = ();
    push(@unsortedIncludes, "#include \"bindings/v8/ActiveDOMCallback.h\"");
    push(@unsortedIncludes, "#include \"bindings/v8/DOMWrapperWorld.h\"");
    push(@unsortedIncludes, "#include \"bindings/v8/ScopedPersistent.h\"");
    my $interfaceHeader = HeaderFileForInterface($interfaceName);
    push(@unsortedIncludes, "#include \"$interfaceHeader\"");
    push(@unsortedIncludes, "#include <v8.h>");
    push(@unsortedIncludes, "#include <wtf/Forward.h>");
    $header{includes}->add(join("\n", sort @unsortedIncludes));
    unshift(@{$header{nameSpaceWebCore}->{header}}, "\n");
    $header{nameSpaceWebCore}->addHeader("class ScriptExecutionContext;\n\n");
    $header{class}->addHeader("class $v8InterfaceName : public $interfaceName, public ActiveDOMCallback {");
    $header{class}->addFooter("};\n");

    $header{classPublic}->add(<<END);
    static PassRefPtr<${v8InterfaceName}> create(v8::Handle<v8::Value> value, ScriptExecutionContext* context)
    {
        ASSERT(value->IsObject());
        ASSERT(context);
        return adoptRef(new ${v8InterfaceName}(v8::Handle<v8::Object>::Cast(value), context));
    }

    virtual ~${v8InterfaceName}();

END

    # Functions
    my $numFunctions = @{$interface->functions};
    if ($numFunctions > 0) {
        $header{classPublic}->add("    // Functions\n");
        foreach my $function (@{$interface->functions}) {
            my $code = "";
            my @params = @{$function->parameters};
            if (!$function->signature->extendedAttributes->{"Custom"} &&
                !(GetNativeType($function->signature->type) eq "bool")) {
                    $code .= "    COMPILE_ASSERT(false)";
            }

            $code .= "    virtual " . GetNativeTypeForCallbacks($function->signature->type) . " " . $function->signature->name . "(";

            my @args = ();
            foreach my $param (@params) {
                push(@args, GetNativeTypeForCallbacks($param->type) . " " . $param->name);
            }
            $code .= join(", ", @args);
            $code .= ");\n";
            $header{classPublic}->add($code);
        }
    }

    $header{classPublic}->add(<<END);

    virtual ScriptExecutionContext* scriptExecutionContext() const { return ContextDestructionObserver::scriptExecutionContext(); }

END
    $header{classPrivate}->add(<<END);
    ${v8InterfaceName}(v8::Handle<v8::Object>, ScriptExecutionContext*);

    ScopedPersistent<v8::Object> m_callback;
    RefPtr<DOMWrapperWorld> m_world;
END
}

sub GenerateCallbackImplementation
{
    my $object = shift;
    my $interface = shift;
    my $interfaceName = $interface->name;
    my $v8InterfaceName = "V8$interfaceName";

    # - Add default header template
    push(@implContentHeader, GenerateImplementationContentHeader($interface));

    AddToImplIncludes("core/dom/ScriptExecutionContext.h");
    AddToImplIncludes("bindings/v8/V8Binding.h");
    AddToImplIncludes("bindings/v8/V8Callback.h");

    AddToImplContent("#include <wtf/Assertions.h>\n\n");
    AddToImplContent("namespace WebCore {\n\n");

    AddToImplContent(<<END);
${v8InterfaceName}::${v8InterfaceName}(v8::Handle<v8::Object> callback, ScriptExecutionContext* context)
    : ActiveDOMCallback(context)
    , m_callback(callback)
    , m_world(DOMWrapperWorld::current())
{
}

END

    AddToImplContent(<<END);
${v8InterfaceName}::~${v8InterfaceName}()
{
}

END

    # Functions
    my $numFunctions = @{$interface->functions};
    if ($numFunctions > 0) {
        AddToImplContent("// Functions\n");
        foreach my $function (@{$interface->functions}) {
            my $code = "";
            my @params = @{$function->parameters};
            if ($function->signature->extendedAttributes->{"Custom"} ||
                !(GetNativeTypeForCallbacks($function->signature->type) eq "bool")) {
                next;
            }

            AddIncludesForType($function->signature->type);
            $code .= "\n" . GetNativeTypeForCallbacks($function->signature->type) . " ${v8InterfaceName}::" . $function->signature->name . "(";

            my @args = ();
            my @argsCheck = ();
            foreach my $param (@params) {
                my $paramName = $param->name;
                AddIncludesForType($param->type);
                push(@args, GetNativeTypeForCallbacks($param->type) . " " . $paramName);
            }
            $code .= join(", ", @args);

            $code .= ")\n";
            $code .= "{\n";
            $code .= join "", @argsCheck if @argsCheck;
            $code .= "    if (!canInvokeCallback())\n";
            $code .= "        return true;\n\n";
            $code .= "    v8::HandleScope handleScope;\n\n";
            $code .= "    v8::Handle<v8::Context> v8Context = toV8Context(scriptExecutionContext(), m_world.get());\n";
            $code .= "    if (v8Context.IsEmpty())\n";
            $code .= "        return true;\n\n";
            $code .= "    v8::Context::Scope scope(v8Context);\n\n";

            @args = ();
            foreach my $param (@params) {
                my $paramName = $param->name;
                $code .= "    v8::Handle<v8::Value> ${paramName}Handle = " . NativeToJSValue($param, $paramName, "v8::Handle<v8::Object>()", "v8Context->GetIsolate()", "") . ";\n";
                $code .= "    if (${paramName}Handle.IsEmpty()) {\n";
                $code .= "        if (!isScriptControllerTerminating())\n";
                $code .= "            CRASH();\n";
                $code .= "        return true;\n";
                $code .= "    }\n";
                push(@args, "        ${paramName}Handle");
            }

            if (scalar(@args) > 0) {
                $code .= "\n    v8::Handle<v8::Value> argv[] = {\n";
                $code .= join(",\n", @args);
                $code .= "\n    };\n\n";
            } else {
                $code .= "\n    v8::Handle<v8::Value> *argv = 0;\n\n";
            }
            $code .= "    bool callbackReturnValue = false;\n";
            $code .= "    return !invokeCallback(m_callback.get(), " . scalar(@params) . ", argv, callbackReturnValue, scriptExecutionContext());\n";
            $code .= "}\n";
            AddToImplContent($code);
        }
   }

    AddToImplContent("\n} // namespace WebCore\n\n");

    my $conditionalString = GenerateConditionalString($interface);
    AddToImplContent("#endif // ${conditionalString}\n") if $conditionalString;
}

sub BaseInterfaceName
{
    my $interface = shift;

    while (@{$interface->parents}) {
        $interface = ParseInterface(@{$interface->parents}[0]);
    }

    return $interface->name;
}

sub GenerateToV8Converters
{
    my $interface = shift;
    my $v8InterfaceName = shift;
    my $nativeType = shift;
    my $interfaceName = $interface->name;

    if ($interface->extendedAttributes->{"DoNotGenerateWrap"} || $interface->extendedAttributes->{"DoNotGenerateToV8"}) {
        return;
    }

    AddToImplIncludes("bindings/v8/ScriptController.h");
    AddToImplIncludes("core/page/Frame.h");

    my $createWrapperArgumentType = GetPassRefPtrType($nativeType);
    my $baseType = BaseInterfaceName($interface);

    my $code = "";
    $code .= <<END;

v8::Handle<v8::Object> ${v8InterfaceName}::createWrapper(${createWrapperArgumentType} impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl.get());
    ASSERT(DOMDataStore::getWrapper(impl.get(), isolate).IsEmpty());
END

    my $vtableNameGnu = GetGnuVTableNameForInterface($interface);
    $code .= <<END if $vtableNameGnu;

#if ENABLE(BINDING_INTEGRITY)
    checkTypeOrDieTrying(impl.get());
#endif
END

    $code .= <<END if ($baseType ne $interfaceName);
    ASSERT(static_cast<void*>(static_cast<${baseType}*>(impl.get())) == static_cast<void*>(impl.get()));
END

    if (InheritsInterface($interface, "Document")) {
        $code .= <<END;
    if (Frame* frame = impl->frame()) {
        if (frame->script()->initializeMainWorld()) {
            // initializeMainWorld may have created a wrapper for the object, retry from the start.
            v8::Handle<v8::Object> wrapper = DOMDataStore::getWrapper(impl.get(), isolate);
            if (!wrapper.IsEmpty())
                return wrapper;
        }
    }
END
    }

    $code .= <<END;

    v8::Handle<v8::Object> wrapper = V8DOMWrapper::createWrapper(creationContext, &info, impl.get(), isolate);
    if (UNLIKELY(wrapper.IsEmpty()))
        return wrapper;

    installPerContextProperties(wrapper, impl.get(), isolate);
    V8DOMWrapper::associateObjectWithWrapper(impl, &info, wrapper, isolate, hasDependentLifetime ? WrapperConfiguration::Dependent : WrapperConfiguration::Independent);
    return wrapper;
}
END
    AddToImplContent($code);
}

sub GenerateSecurityCheckFunctions
{
    my $interface = shift;
    my $interfaceName = $interface->name;
    my $v8InterfaceName = "V8$interfaceName";

    AddToImplContentInternals(<<END);
bool indexedSecurityCheck(v8::Local<v8::Object> host, uint32_t index, v8::AccessType type, v8::Local<v8::Value>)
{
    $interfaceName* imp =  ${v8InterfaceName}::toNative(host);
    return BindingSecurity::shouldAllowAccessToFrame(imp->frame(), DoNotReportSecurityError);
}

END
    AddToImplContentInternals(<<END);
bool namedSecurityCheck(v8::Local<v8::Object> host, v8::Local<v8::Value> key, v8::AccessType type, v8::Local<v8::Value>)
{
    $interfaceName* imp =  ${v8InterfaceName}::toNative(host);
    return BindingSecurity::shouldAllowAccessToFrame(imp->frame(), DoNotReportSecurityError);
}

END
}

sub GetNativeTypeForConversions
{
    my $interface = shift;
    my $interfaceName = $interface->name;
    $interfaceName = GetSVGTypeNeedingTearOff($interfaceName) if IsSVGTypeNeedingTearOff($interfaceName);
    return $interfaceName;
}

# See http://refspecs.linux-foundation.org/cxxabi-1.83.html.
sub GetGnuVTableRefForInterface
{
    my $interface = shift;
    my $vtableName = GetGnuVTableNameForInterface($interface);
    if (!$vtableName) {
        return "0";
    }
    my $typename = GetNativeTypeForConversions($interface);
    my $offset = GetGnuVTableOffsetForType($typename);
    return "&" . $vtableName . "[" . $offset . "]";
}

sub GetGnuVTableNameForInterface
{
    my $interface = shift;
    my $typename = GetNativeTypeForConversions($interface);
    my $templatePosition = index($typename, "<");
    return "" if $templatePosition != -1;
    return "" if GetImplementationLacksVTableForInterface($interface);
    return "" if GetV8SkipVTableValidationForInterface($interface);
    return "_ZTV" . GetGnuMangledNameForInterface($interface);
}

sub GetGnuMangledNameForInterface
{
    my $interface = shift;
    my $typename = GetNativeTypeForConversions($interface);
    my $templatePosition = index($typename, "<");
    if ($templatePosition != -1) {
        return "";
    }
    my $mangledType = length($typename) . $typename;
    my $namespace = GetNamespaceForInterface($interface);
    my $mangledNamespace =  "N" . length($namespace) . $namespace;
    return $mangledNamespace . $mangledType . "E";
}

sub GetGnuVTableOffsetForType
{
    my $typename = shift;
    if ($typename eq "SVGAElement"
        || $typename eq "SVGCircleElement"
        || $typename eq "SVGClipPathElement"
        || $typename eq "SVGDefsElement"
        || $typename eq "SVGEllipseElement"
        || $typename eq "SVGForeignObjectElement"
        || $typename eq "SVGGElement"
        || $typename eq "SVGImageElement"
        || $typename eq "SVGLineElement"
        || $typename eq "SVGPathElement"
        || $typename eq "SVGPolyElement"
        || $typename eq "SVGPolygonElement"
        || $typename eq "SVGPolylineElement"
        || $typename eq "SVGRectElement"
        || $typename eq "SVGSVGElement"
        || $typename eq "SVGStyledLocatableElement"
        || $typename eq "SVGStyledTransformableElement"
        || $typename eq "SVGSwitchElement"
        || $typename eq "SVGTextElement"
        || $typename eq "SVGTransformable"
        || $typename eq "SVGUseElement") {
        return "3";
    }
    return "2";
}

# See http://en.wikipedia.org/wiki/Microsoft_Visual_C%2B%2B_Name_Mangling.
sub GetWinVTableRefForInterface
{
    my $interface = shift;
    my $vtableName = GetWinVTableNameForInterface($interface);
    return 0 if !$vtableName;
    return "__identifier(\"" . $vtableName . "\")";
}

sub GetWinVTableNameForInterface
{
    my $interface = shift;
    my $typename = GetNativeTypeForConversions($interface);
    my $templatePosition = index($typename, "<");
    return "" if $templatePosition != -1;
    return "" if GetImplementationLacksVTableForInterface($interface);
    return "" if GetV8SkipVTableValidationForInterface($interface);
    return "??_7" . GetWinMangledNameForInterface($interface) . "6B@";
}

sub GetWinMangledNameForInterface
{
    my $interface = shift;
    my $typename = GetNativeTypeForConversions($interface);
    my $namespace = GetNamespaceForInterface($interface);
    return $typename . "@" . $namespace . "@@";
}

sub GetNamespaceForInterface
{
    my $interface = shift;
    return $interface->extendedAttributes->{"ImplementationNamespace"} || "WebCore";
}

sub GetImplementationLacksVTableForInterface
{
    my $interface = shift;
    return $interface->extendedAttributes->{"ImplementationLacksVTable"};
}

sub GetV8SkipVTableValidationForInterface
{
    my $interface = shift;
    return $interface->extendedAttributes->{"SkipVTableValidation"};
}

sub GenerateFunctionCallString
{
    my $function = shift;
    my $numberOfParameters = shift;
    my $indent = shift;
    my $interfaceName = shift;
    my $forMainWorldSuffix = shift;
    my %replacements = @_;

    my $name = $function->signature->name;
    my $returnType = $function->signature->type;
    my $nativeReturnType = GetNativeType($returnType, 0);
    my $code = "";

    my $isSVGTearOffType = (IsSVGTypeNeedingTearOff($returnType) and not $interfaceName =~ /List$/);
    $nativeReturnType = GetSVGWrappedTypeNeedingTearOff($returnType) if $isSVGTearOffType;

    if ($function->signature->extendedAttributes->{"ImplementedAs"}) {
        $name = $function->signature->extendedAttributes->{"ImplementedAs"};
    }

    my $index = 0;

    my @arguments;
    my $functionName;
    my $implementedBy = $function->signature->extendedAttributes->{"ImplementedBy"};
    if ($implementedBy) {
        AddInterfaceToImplIncludes($implementedBy);
        unshift(@arguments, "imp") if !$function->isStatic;
        $functionName = "${implementedBy}::${name}";
    } elsif ($function->isStatic) {
        $functionName = "${interfaceName}::${name}";
    } else {
        $functionName = "imp->${name}";
    }

    my $callWith = $function->signature->extendedAttributes->{"CallWith"};
    my ($callWithArgs, $subCode) = GenerateCallWith($callWith, $indent, 0, $function);
    $code .= $subCode;
    unshift(@arguments, @$callWithArgs);
    $index += @$callWithArgs;
    $numberOfParameters += @$callWithArgs;

    foreach my $parameter (@{$function->parameters}) {
        if ($index eq $numberOfParameters) {
            last;
        }
        my $paramName = $parameter->name;
        my $paramType = $parameter->type;

        if ($replacements{$paramName}) {
            push @arguments, $replacements{$paramName};
        } elsif ($parameter->type eq "NodeFilter" || $parameter->type eq "XPathNSResolver") {
            push @arguments, "$paramName.get()";
        } elsif (IsSVGTypeNeedingTearOff($parameter->type) and not $interfaceName =~ /List$/) {
            push @arguments, "$paramName->propertyReference()";
            $code .= $indent . "if (!$paramName)\n";
            $code .= $indent . "    return setDOMException(WebCore::TYPE_MISMATCH_ERR, args.GetIsolate());\n";
        } elsif ($parameter->type eq "SVGMatrix" and $interfaceName eq "SVGTransformList") {
            push @arguments, "$paramName.get()";
        } else {
            push @arguments, $paramName;
        }
        $index++;
    }

    if ($function->signature->extendedAttributes->{"RaisesException"}) {
        push @arguments, "ec";
    }

    my $functionString = "$functionName(" . join(", ", @arguments) . ")";

    my $return = "result";
    my $returnIsRef = IsRefPtrType($returnType);

    if ($returnType eq "void") {
        $code .= $indent . "$functionString;\n";
    } elsif (ExtendedAttributeContains($callWith, "ScriptState") or $function->signature->extendedAttributes->{"RaisesException"}) {
        $code .= $indent . $nativeReturnType . " result = $functionString;\n";
    } else {
        # Can inline the function call into the return statement to avoid overhead of using a Ref<> temporary
        $return = $functionString;
        $returnIsRef = 0;

        if ($interfaceName eq "SVGTransformList" and IsRefPtrType($returnType)) {
            $return = "WTF::getPtr(" . $return . ")";
        }
    }

    if ($function->signature->extendedAttributes->{"RaisesException"}) {
        $code .= $indent . "if (UNLIKELY(ec))\n";
        $code .= $indent . "    goto fail;\n";
    }

    if (ExtendedAttributeContains($callWith, "ScriptState")) {
        $code .= $indent . "if (state.hadException()) {\n";
        $code .= $indent . "    v8::Local<v8::Value> exception = state.exception();\n";
        $code .= $indent . "    state.clearException();\n";
        $code .= $indent . "    return throwError(exception, args.GetIsolate());\n";
        $code .= $indent . "}\n";
    }

    if ($isSVGTearOffType) {
        AddToImplIncludes("V8$returnType.h");
        AddToImplIncludes("core/svg/properties/SVGPropertyTearOff.h");
        my $svgNativeType = GetSVGTypeNeedingTearOff($returnType);
        # FIXME: Update for all ScriptWrappables.
        if (IsDOMNodeType($interfaceName)) {
            $code .= $indent . "return toV8Fast${forMainWorldSuffix}(WTF::getPtr(${svgNativeType}::create($return)), args, imp);\n";
        } else {
            $code .= $indent . "return toV8${forMainWorldSuffix}(WTF::getPtr(${svgNativeType}::create($return)), args.Holder(), args.GetIsolate());\n";
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
        $nativeValue = NativeToJSValue($function->signature, $return, "args.Holder()", "args.GetIsolate()", "args", "imp", "ReturnUnsafeHandle", $forMainWorldSuffix);
    } else {
        $nativeValue = NativeToJSValue($function->signature, $return, "args.Holder()", "args.GetIsolate()", 0, 0, "ReturnUnsafeHandle", $forMainWorldSuffix);
    }

    $code .= $indent . "return " . $nativeValue . ";\n";

    return $code;
}

sub GetNativeTypeFromSignature
{
    my $signature = shift;
    my $parameterIndex = shift;

    my $type = $signature->type;

    if ($type eq "unsigned long" and $signature->extendedAttributes->{"IsIndex"}) {
        # Special-case index arguments because we need to check that they aren't < 0.
        return "int";
    }

    $type = GetNativeType($type, $parameterIndex >= 0 ? 1 : 0);

    if ($parameterIndex >= 0 && $type eq "V8StringResource") {
        # FIXME: This implements [TreatNullAs=NullString] and [TreatUndefinedAs=NullString],
        # but the Web IDL spec requires [TreatNullAs=EmptyString] and [TreatUndefinedAs=EmptyString].
        my $mode = "";
        if (($signature->extendedAttributes->{"TreatNullAs"} and $signature->extendedAttributes->{"TreatNullAs"} eq "NullString") and ($signature->extendedAttributes->{"TreatUndefinedAs"} and $signature->extendedAttributes->{"TreatUndefinedAs"} eq "NullString")) {
            $mode = "WithUndefinedOrNullCheck";
        } elsif (($signature->extendedAttributes->{"TreatNullAs"} and $signature->extendedAttributes->{"TreatNullAs"} eq "NullString") or $signature->extendedAttributes->{"Reflect"}) {
            $mode = "WithNullCheck";
        }
        # FIXME: Add the case for 'elsif ($signature->extendedAttributes->{"TreatUndefinedAs"} and $signature->extendedAttributes->{"TreatUndefinedAs"} eq "NullString"))'.
        $type .= "<$mode>";
    }

    return $type;
}

sub GetNativeType
{
    my $type = shift;
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
    return "int" if $type eq "long" or $type eq "int" or $type eq "short" or $type eq "unsigned short";
    return "unsigned" if $type eq "unsigned long" or $type eq "unsigned int";
    return "long long" if $type eq "long long";
    return "unsigned long long" if $type eq "unsigned long long";
    return "bool" if $type eq "boolean";

    return "V8StringResource" if ($type eq "DOMString" or IsEnumType($type)) and $isParameter;
    return "String" if $type eq "DOMString" or IsEnumType($type);

    return "Range::CompareHow" if $type eq "CompareHow";
    return "DOMTimeStamp" if $type eq "DOMTimeStamp";
    return "double" if $type eq "Date";
    return "ScriptValue" if $type eq "any";
    return "Dictionary" if $type eq "Dictionary";

    return "RefPtr<DOMStringList>" if $type eq "DOMStringList";
    return "RefPtr<MediaQueryListListener>" if $type eq "MediaQueryListListener";
    return "RefPtr<NodeFilter>" if $type eq "NodeFilter";
    return "RefPtr<SerializedScriptValue>" if $type eq "SerializedScriptValue";
    return "RefPtr<XPathNSResolver>" if $type eq "XPathNSResolver";
    return "RefPtr<${type}>" if IsRefPtrType($type) and not $isParameter;

    my $arrayType = GetArrayType($type);
    my $sequenceType = GetSequenceType($type);
    my $arrayOrSequenceType = $arrayType || $sequenceType;

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

    # Callbacks use raw pointers, so pass isParameter = 1
    return GetNativeType($type, 1);
}

sub TypeCanFailConversion
{
    my $signature = shift;
    my $type = $signature->type;

    AddToImplIncludes("core/dom/ExceptionCode.h") if $type eq "Attr";
    return 1 if $type eq "Attr";
    return 0;
}

sub JSValueToNative
{
    my $signature = shift;
    my $value = shift;
    my $getIsolate = shift;

    my $type = $signature->type;
    my $intConversion = $signature->extendedAttributes->{"EnforceRange"} ? "EnforceRange" : "NormalConversion";

    return "$value" if $type eq "JSObject";
    return "$value->BooleanValue()" if $type eq "boolean";
    return "static_cast<$type>($value->NumberValue())" if $type eq "float" or $type eq "double";

    if ($intConversion ne "NormalConversion") {
        return "toInt32($value, $intConversion, ok)" if $type eq "long" or $type eq "short";
        return "toUInt32($value, $intConversion, ok)" if $type eq "unsigned long" or $type eq "unsigned short";
        return "toInt64($value, $intConversion, ok)" if $type eq "long long";
        return "toUInt64($value, $intConversion, ok)" if $type eq "unsigned long long";
    } else {
        return "toInt32($value)" if $type eq "long" or $type eq "short";
        return "toUInt32($value)" if $type eq "unsigned long" or $type eq "unsigned short";
        return "toInt64($value)" if $type eq "long long";
        return "toUInt64($value)" if $type eq "unsigned long long";
    }
    return "static_cast<Range::CompareHow>($value->Int32Value())" if $type eq "CompareHow";
    return "toWebCoreDate($value)" if $type eq "Date";
    return "toDOMStringList($value, $getIsolate)" if $type eq "DOMStringList";

    if ($type eq "DOMString") {
        return $value;
    }

    if (IsEnumType($type)) {
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

    if ($type eq "any") {
        AddToImplIncludes("bindings/v8/ScriptValue.h");
        return "ScriptValue($value)";
    }

    if ($type eq "NodeFilter") {
        return "toNodeFilter($value)";
    }

    if ($type eq "MediaQueryListListener") {
        AddToImplIncludes("core/css/MediaQueryListListener.h");
        return "MediaQueryListListener::create(" . $value . ")";
    }

    if ($type eq "EventTarget") {
        return "V8DOMWrapper::isDOMWrapper($value) ? toWrapperTypeInfo(v8::Handle<v8::Object>::Cast($value))->toEventTarget(v8::Handle<v8::Object>::Cast($value)) : 0";
    }

    if ($type eq "XPathNSResolver") {
        return "toXPathNSResolver($value, $getIsolate)";
    }

    my $arrayType = GetArrayType($type);
    my $sequenceType = GetSequenceType($type);
    my $arrayOrSequenceType = $arrayType || $sequenceType;

    if ($arrayOrSequenceType) {
        if (IsRefPtrType($arrayOrSequenceType)) {
            AddToImplIncludes("V8${arrayOrSequenceType}.h");
            return "(toRefPtrNativeArray<${arrayOrSequenceType}, V8${arrayOrSequenceType}>($value, $getIsolate))";
        }
        return "toNativeArray<" . GetNativeType($arrayOrSequenceType) . ">($value)";
    }

    AddIncludesForType($type);

    AddToImplIncludes("V8${type}.h");
    return "V8${type}::HasInstance($value, $getIsolate, worldType($getIsolate)) ? V8${type}::toNative(v8::Handle<v8::Object>::Cast($value)) : 0";
}

sub GetV8HeaderName
{
    my $type = shift;
    return "V8Event.h" if $type eq "DOMTimeStamp";
    return "core/dom/EventListener.h" if $type eq "EventListener";
    return "bindings/v8/SerializedScriptValue.h" if $type eq "SerializedScriptValue";
    return "bindings/v8/ScriptValue.h" if $type eq "any";
    return "V8${type}.h";
}

sub CreateCustomSignature
{
    my $function = shift;
    my $count = @{$function->parameters};
    my $name = $function->signature->name;
    my $code = "    const int ${name}Argc = ${count};\n" .
      "    v8::Handle<v8::FunctionTemplate> ${name}Argv[${name}Argc] = { ";
    my $first = 1;
    foreach my $parameter (@{$function->parameters}) {
        if ($first) { $first = 0; }
        else { $code .= ", "; }
        if (IsWrapperType($parameter->type)) {
            if ($parameter->type eq "XPathNSResolver") {
                # Special case for XPathNSResolver.  All other browsers accepts a callable,
                # so, even though it's against IDL, accept objects here.
                $code .= "v8::Handle<v8::FunctionTemplate>()";
            } else {
                my $type = $parameter->type;

                my $arrayType = GetArrayType($type);
                my $sequenceType = GetSequenceType($type);
                my $arrayOrSequenceType = $arrayType || $sequenceType;

                if ($arrayOrSequenceType) {
                    if ($arrayType eq "DOMString") {
                        AddToImplIncludes("V8DOMStringList.h");
                        AddToImplIncludes("core/dom/DOMStringList.h");

                    } elsif (IsRefPtrType($arrayOrSequenceType)) {
                        AddToImplIncludes(GetV8HeaderName($arrayOrSequenceType));
                        AddToImplIncludes("${arrayOrSequenceType}.h");
                    } else {
                        $code .= "v8::Handle<v8::FunctionTemplate>()";
                        next;
                    }
                } else {
                    AddToImplIncludes(GetV8HeaderName($type));
                }
                $code .= "V8PerIsolateData::from(isolate)->rawTemplate(&V8${type}::info, currentWorldType)";
            }
        } else {
            $code .= "v8::Handle<v8::FunctionTemplate>()";
        }
    }
    $code .= " };\n";
    $code .= "    v8::Handle<v8::Signature> ${name}Signature = v8::Signature::New(desc, ${name}Argc, ${name}Argv);\n";
    return $code;
}


sub RequiresCustomSignature
{
    my $function = shift;
    # No signature needed for Custom function
    if (HasCustomMethod($function->signature->extendedAttributes)) {
        return 0;
    }
    # No signature needed for overloaded function
    if (@{$function->{overloads}} > 1) {
        return 0;
    }
    if ($function->isStatic) {
        return 0;
    }
    # Type checking is performed in the generated code
    if ($function->signature->extendedAttributes->{"StrictTypeChecking"}) {
      return 0;
    }
    foreach my $parameter (@{$function->parameters}) {
        if (($parameter->isOptional && !$parameter->extendedAttributes->{"Default"}) || IsCallbackInterface($parameter->type)) {
            return 0;
        }
    }

    foreach my $parameter (@{$function->parameters}) {
        if (IsWrapperType($parameter->type)) {
            return 1;
        }
    }
    return 0;
}


my %non_wrapper_types = (
    'CompareHow' => 1,
    'DOMObject' => 1,
    'DOMString' => 1,
    'DOMTimeStamp' => 1,
    'Date' => 1,
    'Dictionary' => 1,
    'EventListener' => 1,
    # FIXME: When EventTarget is an interface and not a mixin, fix this so that
    # EventTarget is treated as a wrapper type.
    'EventTarget' => 1,
    'JSObject' => 1,
    'MediaQueryListListener' => 1,
    'NodeFilter' => 1,
    'SerializedScriptValue' => 1,
    'any' => 1,
    'boolean' => 1,
    'double' => 1,
    'float' => 1,
    'int' => 1,
    'long long' => 1,
    'long' => 1,
    'short' => 1,
    'unsigned int' => 1,
    'unsigned long long' => 1,
    'unsigned long' => 1,
    'unsigned short' => 1
);


sub IsWrapperType
{
    my $type = shift;
    # FIXME: Should this return false for Sequence and Array types?
    return 0 if IsEnumType($type);
    return !($non_wrapper_types{$type});
}

sub IsCallbackInterface
{
    my $type = shift;
    return 0 unless IsWrapperType($type);
    # FIXME: Those checks for Sequence and Array types should probably
    # be moved to IsWrapperType().
    return 0 if GetArrayType($type);
    return 0 if GetSequenceType($type);

    my $idlFile = IDLFileForInterface($type)
        or die("Could NOT find IDL file for interface \"$type\"!\n");

    open FILE, "<", $idlFile;
    my @lines = <FILE>;
    close FILE;

    my $fileContents = join('', @lines);
    return ($fileContents =~ /callback\s+interface\s+(\w+)/gs);
}

sub GetTypeNameOfExternalTypedArray
{
    my $interface = shift;
    my $interfaceName = $interface->name;
    my $viewType = $interface->extendedAttributes->{"TypedArray"};
    return "v8::kExternalByteArray" if $viewType eq "signed char" and $interfaceName eq "Int8Array";
    return "v8::kExternalPixelArray" if $viewType eq "unsigned char" and $interfaceName eq "Uint8ClampedArray";
    return "v8::kExternalUnsignedByteArray" if $viewType eq "unsigned char" and $interfaceName eq "Uint8Array";
    return "v8::kExternalShortArray" if $viewType eq "short" and $interfaceName eq "Int16Array";
    return "v8::kExternalUnsignedShortArray" if $viewType eq "unsigned short" and $interfaceName eq "Uint16Array";
    return "v8::kExternalIntArray" if $viewType eq "int" and $interfaceName eq "Int32Array";
    return "v8::kExternalUnsignedIntArray" if $viewType eq "unsigned int" and $interfaceName eq "Uint32Array";
    return "v8::kExternalFloatArray" if $viewType eq "float" and $interfaceName eq "Float32Array";
    return "v8::kExternalDoubleArray" if $viewType eq "double" and $interfaceName eq "Float64Array";

    die "TypedArray of unknown type is found";
}

sub IsDOMNodeType
{
    my $type = shift;

    return 1 if $type eq 'Attr';
    return 1 if $type eq 'CDATASection';
    return 1 if $type eq 'CharacterData';
    return 1 if $type eq 'Comment';
    return 1 if $type eq 'Document';
    return 1 if $type eq 'DocumentFragment';
    return 1 if $type eq 'DocumentType';
    return 1 if $type eq 'Element';
    return 1 if $type eq 'Entity';
    return 1 if $type eq 'EntityReference';
    return 1 if $type eq 'HTMLDocument';
    return 1 if $type eq 'Node';
    return 1 if $type eq 'Notation';
    return 1 if $type eq 'ProcessingInstruction';
    return 1 if $type eq 'ShadowRoot';
    return 1 if $type eq 'SVGDocument';
    return 1 if $type eq 'Text';

    return 1 if $type =~ /^HTML.*Element$/;
    return 1 if $type =~ /^SVG.*Element$/;

    return 1 if $type eq 'TestNode';

    return 0;
}


sub NativeToJSValue
{
    my $signature = shift;
    my $value = shift;
    my $getCreationContext = shift;
    my $getIsolate = shift;
    die "An Isolate is mandatory for native value => JS value conversion." unless $getIsolate;
    my $getHolderContainer = shift;
    my $getHolderContainerArg = $getHolderContainer ? ", $getHolderContainer" : "";
    my $getScriptWrappable = shift;
    my $getScriptWrappableArg = $getScriptWrappable ? ", $getScriptWrappable" : "";
    my $returnHandleType = shift;
    my $returnHandleTypeArg = $returnHandleType ? ", $returnHandleType" : "";
    my $forMainWorldSuffix = shift;

    my $type = $signature->type;

    return "v8Boolean($value, $getIsolate)" if $type eq "boolean";
    return "v8Undefined()" if $type eq "void";     # equivalent to v8Undefined()

    # HTML5 says that unsigned reflected attributes should be in the range
    # [0, 2^31). When a value isn't in this range, a default value (or 0)
    # should be returned instead.
    if ($signature->extendedAttributes->{"Reflect"} and ($type eq "unsigned long" or $type eq "unsigned short")) {
        $value =~ s/getUnsignedIntegralAttribute/getIntegralAttribute/g;
        return "v8UnsignedInteger(std::max(0, " . $value . "), $getIsolate)";
    }

    # For all the types where we use 'int' as the representation type,
    # we use v8Integer() which has a fast small integer conversion check.
    my $nativeType = GetNativeType($type);
    return "v8Integer($value, $getIsolate)" if $nativeType eq "int";
    return "v8UnsignedInteger($value, $getIsolate)" if $nativeType eq "unsigned";

    return "v8DateOrNull($value, $getIsolate)" if $type eq "Date";
    # long long and unsigned long long are not representable in ECMAScript.
    return "v8::Number::New(static_cast<double>($value))" if $type eq "long long" or $type eq "unsigned long long" or $type eq "DOMTimeStamp";
    return "v8::Number::New($value)" if IsPrimitiveType($type);
    return "$value.v8Value()" if $nativeType eq "ScriptValue";

    if ($type eq "DOMString" or IsEnumType($type)) {
        my $conv = $signature->extendedAttributes->{"TreatReturnedNullStringAs"};
        if (defined $conv) {
            return "v8StringOrNull($value, $getIsolate$returnHandleTypeArg)" if $conv eq "Null";
            return "v8StringOrUndefined($value, $getIsolate$returnHandleTypeArg)" if $conv eq "Undefined";

            die "Unknown value for TreatReturnedNullStringAs extended attribute";
        }
        return "v8String($value, $getIsolate$returnHandleTypeArg)";
    }

    my $arrayType = GetArrayType($type);
    my $sequenceType = GetSequenceType($type);
    my $arrayOrSequenceType = $arrayType || $sequenceType;

    if ($arrayOrSequenceType) {
        if ($arrayType eq "DOMString") {
            AddToImplIncludes("V8DOMStringList.h");
            AddToImplIncludes("core/dom/DOMStringList.h");

        } elsif (IsRefPtrType($arrayOrSequenceType)) {
            AddToImplIncludes(GetV8HeaderName($arrayOrSequenceType));
            AddToImplIncludes("${arrayOrSequenceType}.h");
        }
        return "v8Array($value, $getIsolate)";
    }

    AddIncludesForType($type);

    if (IsDOMNodeType($type) || $type eq "EventTarget") {
      if ($getScriptWrappable) {
          return "toV8Fast${forMainWorldSuffix}($value$getHolderContainerArg$getScriptWrappableArg)";
      }
      return "toV8($value, $getCreationContext, $getIsolate)";
    }

    if ($type eq "EventListener") {
        AddToImplIncludes("bindings/v8/V8AbstractEventListener.h");
        return "${value} ? v8::Handle<v8::Value>(static_cast<V8AbstractEventListener*>(${value})->getListenerObject(imp->scriptExecutionContext())) : v8::Handle<v8::Value>(v8Null($getIsolate))";
    }

    if ($type eq "SerializedScriptValue") {
        AddToImplIncludes("$type.h");
        return "$value ? $value->deserialize() : v8::Handle<v8::Value>(v8Null($getIsolate))";
    }

    AddToImplIncludes("wtf/RefCounted.h");
    AddToImplIncludes("wtf/RefPtr.h");
    AddToImplIncludes("wtf/GetPtr.h");

    if ($getScriptWrappable) {
          return "toV8Fast$forMainWorldSuffix($value$getHolderContainerArg$getScriptWrappableArg)";
    }
    return "toV8($value, $getCreationContext, $getIsolate)";
}

sub WriteData
{
    my $object = shift;
    my $interface = shift;
    my $outputDirectory = shift;
    my $outputHeadersDirectory = shift;

    my $name = $interface->name;
    my $headerFileName = "$outputHeadersDirectory/V8$name.h";
    my $implFileName = "$outputDirectory/V8$name.cpp";

    # Update a .cpp file if the contents are changed.
    my $contents = join "", @implContentHeader;

    my @includes = ();
    my %implIncludeConditions = ();
    foreach my $include (keys %implIncludes) {
        my $condition = $implIncludes{$include};
        my $checkType = $include;
        $checkType =~ s/\.h//;
        next if IsSVGAnimatedType($checkType);

        if ($include =~ /wtf/) {
            $include = "\<$include\>";
        } else {
            $include = "\"$include\"";
        }

        if ($condition eq 1) {
            push @includes, $include;
        } else {
            push @{$implIncludeConditions{$condition}}, $include;
        }
    }
    foreach my $include (sort @includes) {
        $contents .= "#include $include\n";
    }
    foreach my $condition (sort keys %implIncludeConditions) {
        $contents .= "\n#if " . GenerateConditionalStringFromAttributeValue($condition) . "\n";
        foreach my $include (sort @{$implIncludeConditions{$condition}}) {
            $contents .= "#include $include\n";
        }
        $contents .= "#endif\n";
    }

    $contents .= "\n";
    $contents .= join "", @implContentInternals, @implContent;
    UpdateFile($implFileName, $contents);

    %implIncludes = ();
    @implContentHeader = ();
    @implContentInternals = ();
    @implContent = ();

    # Update a .h file if the contents are changed.
    $contents = join "", @headerContent;
    UpdateFile($headerFileName, $header{root}->toString());

    @headerContent = ();
}

sub ConvertToV8StringResource
{
    my $signature = shift;
    my $nativeType = shift;
    my $variableName = shift;
    my $value = shift;
    my $suffix = shift;

    die "Wrong native type passed: $nativeType" unless $nativeType =~ /^V8StringResource/;
    if ($signature->type eq "DOMString" or IsEnumType($signature->type)) {
        my $macro = "V8TRYCATCH_FOR_V8STRINGRESOURCE";
        $macro .= "_$suffix" if $suffix;
        return "$macro($nativeType, $variableName, $value);"
    } else {
        return "$nativeType $variableName($value, true);";
    }
}

# Returns the RuntimeEnabledFeatures function name that is hooked up to check if a method/attribute is enabled.
sub GetRuntimeEnableFunctionName
{
    my $signature = shift;

    # If a parameter is given (e.g. "EnabledAtRuntime=FeatureName") return the RuntimeEnabledFeatures::{FeatureName}Enabled() method.
    return "RuntimeEnabledFeatures::" . ToMethodName($signature->extendedAttributes->{"EnabledAtRuntime"}) . "Enabled" if ($signature->extendedAttributes->{"EnabledAtRuntime"} && $signature->extendedAttributes->{"EnabledAtRuntime"} ne "VALUE_IS_MISSING");

    # Otherwise return a function named RuntimeEnabledFeatures::{methodName}Enabled().
    return "RuntimeEnabledFeatures::" . ToMethodName($signature->name) . "Enabled";
}

sub GetContextEnableFunction
{
    my $signature = shift;

    # If a parameter is given (e.g. "EnabledPerContext=FeatureName") return the {FeatureName}Allowed() method.
    if ($signature->extendedAttributes->{"EnabledPerContext"} && $signature->extendedAttributes->{"EnabledPerContext"} ne "VALUE_IS_MISSING") {
        return "ContextFeatures::" . ToMethodName($signature->extendedAttributes->{"EnabledPerContext"}) . "Enabled";
    }

    # Or it fallbacks to the attribute name if the parameter value is missing.
    return "ContextFeatures::" . ToMethodName($signature->name) . "Enabled";
}

sub GetPassRefPtrType
{
    my $v8InterfaceName = shift;

    my $angleBracketSpace = $v8InterfaceName =~ />$/ ? " " : "";
    return "PassRefPtr<${v8InterfaceName}${angleBracketSpace}>";
}

sub UpdateFile
{
    my $fileName = shift;
    my $contents = shift;

    open FH, "> $fileName" or die "Couldn't open $fileName: $!\n";
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

        for (@{$currentInterface->parents}) {
            my $interfaceName = $_;
            my $parentInterface = ParseInterface($interfaceName);

            if ($beforeRecursion) {
                &$beforeRecursion($parentInterface) eq 'prune' and next;
            }
            &$recurse($parentInterface);
            &$afterRecursion($parentInterface) if $afterRecursion;
        }
    };

    &$recurse($interface);
}

sub AddMethodsConstantsAndAttributesFromParentInterfaces
{
    # Add to $interface all of its inherited interface members, except for those
    # inherited through $interface's first listed parent.  If an array reference
    # is passed in as $parents, the names of all ancestor interfaces visited
    # will be appended to the array.  If $collectDirectParents is true, then
    # even the names of $interface's first listed parent and its ancestors will
    # be appended to $parents.

    my $interface = shift;
    my $parents = shift;
    my $collectDirectParents = shift;

    my $first = 1;

    ForAllParents($interface, sub {
        my $currentInterface = shift;

        if ($first) {
            # Ignore first parent class, already handled by the generation itself.
            $first = 0;

            if ($collectDirectParents) {
                # Just collect the names of the direct ancestor interfaces,
                # if necessary.
                push(@$parents, $currentInterface->name);
                ForAllParents($currentInterface, sub {
                    my $currentInterface = shift;
                    push(@$parents, $currentInterface->name);
                }, undef);
            }

            # Prune the recursion here.
            return 'prune';
        }

        # Collect the name of this additional parent.
        push(@$parents, $currentInterface->name) if $parents;

        # Add this parent's members to $interface.
        push(@{$interface->constants}, @{$currentInterface->constants});
        push(@{$interface->functions}, @{$currentInterface->functions});
        push(@{$interface->attributes}, @{$currentInterface->attributes});
    });
}

sub FindSuperMethod
{
    my ($interface, $functionName) = @_;
    my $indexer;
    ForAllParents($interface, undef, sub {
        my $currentInterface = shift;
        foreach my $function (@{$currentInterface->functions}) {
            if ($function->signature->name eq $functionName) {
                $indexer = $function->signature;
                return 'prune';
            }
        }
    });
    return $indexer;
}

sub SkipIncludeHeader
{
    my $type = shift;

    return 1 if $primitiveTypeHash{$type};
    return 1 if $type eq "String";

    # Special case: SVGPoint.h / SVGNumber.h do not exist.
    return 1 if $type eq "SVGPoint" or $type eq "SVGNumber";
    return 0;
}

sub IsConstructorTemplate
{
    my $interface = shift;
    my $template = shift;

    return $interface->extendedAttributes->{"ConstructorTemplate"} && $interface->extendedAttributes->{"ConstructorTemplate"} eq $template;
}

sub IsPrimitiveType
{
    my $type = shift;

    return 1 if $primitiveTypeHash{$type};
    return 0;
}

sub IsEnumType
{
    my $type = shift;

    return 1 if exists $enumTypeHash{$type};
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

    return 1 if exists $svgTypeNeedingTearOff{$type};
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
    return 1 if (($type eq "ArrayBuffer") or ($type eq "ArrayBufferView"));
    return 1 if (($type eq "Uint8Array") or ($type eq "Uint8ClampedArray") or ($type eq "Uint16Array") or ($type eq "Uint32Array"));
    return 1 if (($type eq "Int8Array") or ($type eq "Int16Array") or ($type eq "Int32Array"));
    return 1 if (($type eq "Float32Array") or ($type eq "Float64Array"));
    return 0;
}

sub IsRefPtrType
{
    my $type = shift;

    return 0 if IsPrimitiveType($type);
    return 0 if GetArrayType($type);
    return 0 if GetSequenceType($type);
    return 0 if $type eq "DOMString";
    return 0 if IsEnumType($type);

    return 1;
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
    }

    $svgTypeNeedingTearOff =~ s/>//;
    return $svgTypeNeedingTearOff;
}

sub IsSVGAnimatedType
{
    my $type = shift;

    return 1 if $svgAnimatedTypeHash{$type};
    return 0;
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

sub AssertNotSequenceType
{
    my $type = shift;
    die "Sequences must not be used as the type of an attribute, constant or exception field." if GetSequenceType($type);
}

sub FirstLetterToUpperCase
{
    my $param = shift;
    my $ret = ucfirst($param);
    # xmlEncoding becomes XMLEncoding, but xmlllang becomes Xmllang.
    $ret =~ s/Xml/XML/ if $ret =~ /^Xml[^a-z]/;
    $ret =~ s/Css/CSS/ if $ret =~ /^Css[^T]/;  # css -> setCSS, except setCssText.
    $ret =~ s/Ime/IME/ if $ret =~ /^Ime/;  # ime -> setIME
    return $ret;
}

# URL becomes url, but SetURL becomes setURL.
sub ToMethodName
{
    my $param = shift;
    my $ret = lcfirst($param);
    $ret =~ s/hTML/html/ if $ret =~ /^hTML/;
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

# Return the C++ namespace that a given attribute name string is defined in.
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
        my $name = $function->signature->name;
        $nameToFunctionsMap{$name} = [] if !exists $nameToFunctionsMap{$name};
        push(@{$nameToFunctionsMap{$name}}, $function);
        $function->{overloads} = $nameToFunctionsMap{$name};
        $function->{overloadIndex} = @{$nameToFunctionsMap{$name}};
    }
}

sub AttributeNameForGetterAndSetter
{
    my $attribute = shift;

    my $attributeName = $attribute->signature->name;
    if ($attribute->signature->extendedAttributes->{"ImplementedAs"}) {
        $attributeName = $attribute->signature->extendedAttributes->{"ImplementedAs"};
    }
    my $attributeType = $attribute->signature->type;

    # Avoid clash with C++ keyword.
    $attributeName = "_operator" if $attributeName eq "operator";

    # SVGAElement defines a non-virtual "String& target() const" method which clashes with "virtual String target() const" in Element.
    # To solve this issue the SVGAElement method was renamed to "svgTarget", take care of that when calling this method.
    $attributeName = "svgTarget" if $attributeName eq "target" and $attributeType eq "SVGAnimatedString";

    # SVG animated types need to use a special attribute name.
    # The rest of the special casing for SVG animated types is handled in the language-specific code generators.
    $attributeName .= "Animated" if IsSVGAnimatedType($attributeType);

    return $attributeName;
}

sub ContentAttributeName
{
    my ($implIncludes, $interfaceName, $attribute) = @_;

    my $contentAttributeName = $attribute->signature->extendedAttributes->{"Reflect"};
    return undef if !$contentAttributeName;

    $contentAttributeName = lc AttributeNameForGetterAndSetter($attribute) if $contentAttributeName eq "VALUE_IS_MISSING";

    my $namespace = NamespaceForAttributeName($interfaceName, $contentAttributeName);

    AddToImplIncludes("${namespace}.h");
    return "WebCore::${namespace}::${contentAttributeName}Attr";
}

sub CanUseFastAttribute
{
    my $attribute = shift;
    my $attributeType = $attribute->signature->type;
    # HTMLNames::styleAttr cannot be used with fast{Get,Has}Attribute but we do not [Reflect] the
    # style attribute.

    return !IsSVGAnimatedType($attributeType);
}

sub GetterExpression
{
    my ($implIncludes, $interfaceName, $attribute) = @_;

    my $contentAttributeName = ContentAttributeName($implIncludes, $interfaceName, $attribute);

    if (!$contentAttributeName) {
        return (ToMethodName(AttributeNameForGetterAndSetter($attribute)));
    }

    my $functionName;
    if ($attribute->signature->extendedAttributes->{"URL"}) {
        $functionName = "getURLAttribute";
    } elsif ($attribute->signature->type eq "boolean") {
        my $namespace = NamespaceForAttributeName($interfaceName, $contentAttributeName);
        if (CanUseFastAttribute($attribute)) {
            $functionName = "fastHasAttribute";
        } else {
            $functionName = "hasAttribute";
        }
    } elsif ($attribute->signature->type eq "long") {
        $functionName = "getIntegralAttribute";
    } elsif ($attribute->signature->type eq "unsigned long") {
        $functionName = "getUnsignedIntegralAttribute";
    } else {
        if ($contentAttributeName eq "WebCore::HTMLNames::idAttr") {
            $functionName = "getIdAttribute";
            $contentAttributeName = "";
        } elsif ($contentAttributeName eq "WebCore::HTMLNames::nameAttr") {
            $functionName = "getNameAttribute";
            $contentAttributeName = "";
        } elsif ($contentAttributeName eq "WebCore::HTMLNames::classAttr") {
            $functionName = "getClassAttribute";
            $contentAttributeName = "";
        } elsif (CanUseFastAttribute($attribute)) {
            $functionName = "fastGetAttribute";
        } else {
            $functionName = "getAttribute";
        }
    }

    return ($functionName, $contentAttributeName);
}

sub SetterExpression
{
    my ($implIncludes, $interfaceName, $attribute) = @_;

    my $contentAttributeName = ContentAttributeName($implIncludes, $interfaceName, $attribute);

    if (!$contentAttributeName) {
        return ("set" . FirstLetterToUpperCase(AttributeNameForGetterAndSetter($attribute)));
    }

    my $functionName;
    if ($attribute->signature->type eq "boolean") {
        $functionName = "setBooleanAttribute";
    } elsif ($attribute->signature->type eq "long") {
        $functionName = "setIntegralAttribute";
    } elsif ($attribute->signature->type eq "unsigned long") {
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
    my $interfaceName = $interface->name;
    my @checks = ();
    # If necessary, check that all constants are available as enums with the same value.
    if (!$interface->extendedAttributes->{"DoNotCheckConstants"} && @{$interface->constants}) {
        push(@checks, "\n");
        foreach my $constant (@{$interface->constants}) {
            my $reflect = $constant->extendedAttributes->{"Reflect"};
            my $name = $reflect ? $reflect : $constant->name;
            my $value = $constant->value;
            my $conditionalString = GenerateConditionalString($constant);
            push(@checks, "#if ${conditionalString}\n") if $conditionalString;

            if ($constant->extendedAttributes->{"ImplementedBy"}) {
                push(@checks, "COMPILE_ASSERT($value == " . $constant->extendedAttributes->{"ImplementedBy"} . "::$name, ${interfaceName}Enum${name}IsWrongUseDoNotCheckConstants);\n");
            } else {
                push(@checks, "COMPILE_ASSERT($value == ${interfaceName}::$name, ${interfaceName}Enum${name}IsWrongUseDoNotCheckConstants);\n");
            }

            push(@checks, "#endif\n") if $conditionalString;
        }
        push(@checks, "\n");
    }
    return @checks;
}

sub ExtendedAttributeContains
{
    my $callWith = shift;
    return 0 unless $callWith;
    my $keyword = shift;

    my @callWithKeywords = split /\s*\|\s*/, $callWith;
    return grep { $_ eq $keyword } @callWithKeywords;
}

# FIXME: This is backwards. We currently name the interface and the IDL files with the implementation name. We
# should use the real interface name in the IDL files and then use ImplementedAs to map this to the implementation name.
sub GetVisibleInterfaceName
{
    my $interface = shift;
    my $interfaceName = $interface->extendedAttributes->{"InterfaceName"};
    return $interfaceName ? $interfaceName : $interface->name;
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

1;
