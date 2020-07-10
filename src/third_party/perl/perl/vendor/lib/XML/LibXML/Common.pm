#-------------------------------------------------------------------------#
# $Id: Common.pm,v 1.5 2003/02/27 18:32:59 phish108 Exp $
#
#
# This is free software, you may use it and distribute it under the same terms as
# Perl itself.
#
# Copyright 2001-2003 AxKit.com Ltd., 2002-2006 Christian Glahn, 2006-2009 Petr Pajas
#
#
#-------------------------------------------------------------------------#
package XML::LibXML::Common;


#-------------------------------------------------------------------------#
# global blur                                                             #
#-------------------------------------------------------------------------#
use strict;
use warnings;

require Exporter;
require DynaLoader;
use vars qw( @ISA $VERSION @EXPORT @EXPORT_OK %EXPORT_TAGS);

@ISA = qw(Exporter);

$VERSION = "2.0200"; # VERSION TEMPLATE: DO NOT CHANGE

use XML::LibXML qw(:libxml);

#-------------------------------------------------------------------------#
# export information                                                      #
#-------------------------------------------------------------------------#
%EXPORT_TAGS = (
                all => [qw(
                           ELEMENT_NODE
                           ATTRIBUTE_NODE
                           TEXT_NODE
                           CDATA_SECTION_NODE
                           ENTITY_REFERENCE_NODE
                           ENTITY_NODE
                           PI_NODE
                           PROCESSING_INSTRUCTION_NODE
                           COMMENT_NODE
                           DOCUMENT_NODE
                           DOCUMENT_TYPE_NODE
                           DOCUMENT_FRAG_NODE
                           DOCUMENT_FRAGMENT_NODE
                           NOTATION_NODE
                           HTML_DOCUMENT_NODE
                           DTD_NODE
                           ELEMENT_DECLARATION
                           ATTRIBUTE_DECLARATION
                           ENTITY_DECLARATION
                           NAMESPACE_DECLARATION
                           XINCLUDE_END
                           XINCLUDE_START
                           encodeToUTF8
                           decodeFromUTF8
                          )],
                w3c => [qw(
                           ELEMENT_NODE
                           ATTRIBUTE_NODE
                           TEXT_NODE
                           CDATA_SECTION_NODE
                           ENTITY_REFERENCE_NODE
                           ENTITY_NODE
                           PI_NODE
                           PROCESSING_INSTRUCTION_NODE
                           COMMENT_NODE
                           DOCUMENT_NODE
                           DOCUMENT_TYPE_NODE
                           DOCUMENT_FRAG_NODE
                           DOCUMENT_FRAGMENT_NODE
                           NOTATION_NODE
                           HTML_DOCUMENT_NODE
                           DTD_NODE
                           ELEMENT_DECLARATION
                           ATTRIBUTE_DECLARATION
                           ENTITY_DECLARATION
                           NAMESPACE_DECLARATION
                           XINCLUDE_END
                           XINCLUDE_START
                          )],
                libxml => [qw(
                           XML_ELEMENT_NODE
                           XML_ATTRIBUTE_NODE
                           XML_TEXT_NODE
                           XML_CDATA_SECTION_NODE
                           XML_ENTITY_REF_NODE
                           XML_ENTITY_NODE
                           XML_PI_NODE
                           XML_COMMENT_NODE
                           XML_DOCUMENT_NODE
                           XML_DOCUMENT_TYPE_NODE
                           XML_DOCUMENT_FRAG_NODE
                           XML_NOTATION_NODE
                           XML_HTML_DOCUMENT_NODE
                           XML_DTD_NODE
                           XML_ELEMENT_DECL
                           XML_ATTRIBUTE_DECL
                           XML_ENTITY_DECL
                           XML_NAMESPACE_DECL
                           XML_XINCLUDE_END
                           XML_XINCLUDE_START
                          )],
                gdome => [qw(
                           GDOME_ELEMENT_NODE
                           GDOME_ATTRIBUTE_NODE
                           GDOME_TEXT_NODE
                           GDOME_CDATA_SECTION_NODE
                           GDOME_ENTITY_REF_NODE
                           GDOME_ENTITY_NODE
                           GDOME_PI_NODE
                           GDOME_COMMENT_NODE
                           GDOME_DOCUMENT_NODE
                           GDOME_DOCUMENT_TYPE_NODE
                           GDOME_DOCUMENT_FRAG_NODE
                           GDOME_NOTATION_NODE
                           GDOME_HTML_DOCUMENT_NODE
                           GDOME_DTD_NODE
                           GDOME_ELEMENT_DECL
                           GDOME_ATTRIBUTE_DECL
                           GDOME_ENTITY_DECL
                           GDOME_NAMESPACE_DECL
                           GDOME_XINCLUDE_END
                           GDOME_XINCLUDE_START
                          )],
                encoding => [qw(
                                encodeToUTF8
                                decodeFromUTF8
                               )],
               );

@EXPORT_OK = (
              @{$EXPORT_TAGS{encoding}},
              @{$EXPORT_TAGS{w3c}},
              @{$EXPORT_TAGS{libxml}},
              @{$EXPORT_TAGS{gdome}},
             );

@EXPORT = (
           @{$EXPORT_TAGS{encoding}},
           @{$EXPORT_TAGS{w3c}},
          );

#-------------------------------------------------------------------------#
# W3 conform node types                                                   #
#-------------------------------------------------------------------------#
use constant ELEMENT_NODE                => 1;
use constant ATTRIBUTE_NODE              => 2;
use constant TEXT_NODE                   => 3;
use constant CDATA_SECTION_NODE          => 4;
use constant ENTITY_REFERENCE_NODE       => 5;
use constant ENTITY_NODE                 => 6;
use constant PROCESSING_INSTRUCTION_NODE => 7;
use constant COMMENT_NODE                => 8;
use constant DOCUMENT_NODE               => 9;
use constant DOCUMENT_TYPE_NODE          => 10;
use constant DOCUMENT_FRAGMENT_NODE      => 11;
use constant NOTATION_NODE               => 12;
use constant HTML_DOCUMENT_NODE          => 13;
use constant DTD_NODE                    => 14;
use constant ELEMENT_DECLARATION         => 15;
use constant ATTRIBUTE_DECLARATION       => 16;
use constant ENTITY_DECLARATION          => 17;
use constant NAMESPACE_DECLARATION       => 18;

#-------------------------------------------------------------------------#
# some extras for the W3 spec
#-------------------------------------------------------------------------#
use constant PI_NODE                     => 7;
use constant DOCUMENT_FRAG_NODE          => 11;
use constant XINCLUDE_END                => 19;
use constant XINCLUDE_START              => 20;

#-------------------------------------------------------------------------#
# libgdome compat names                                                   #
#-------------------------------------------------------------------------#
use constant GDOME_ELEMENT_NODE          => 1;
use constant GDOME_ATTRIBUTE_NODE        => 2;
use constant GDOME_TEXT_NODE             => 3;
use constant GDOME_CDATA_SECTION_NODE    => 4;
use constant GDOME_ENTITY_REF_NODE       => 5;
use constant GDOME_ENTITY_NODE           => 6;
use constant GDOME_PI_NODE               => 7;
use constant GDOME_COMMENT_NODE          => 8;
use constant GDOME_DOCUMENT_NODE         => 9;
use constant GDOME_DOCUMENT_TYPE_NODE    => 10;
use constant GDOME_DOCUMENT_FRAG_NODE    => 11;
use constant GDOME_NOTATION_NODE         => 12;
use constant GDOME_HTML_DOCUMENT_NODE    => 13;
use constant GDOME_DTD_NODE              => 14;
use constant GDOME_ELEMENT_DECL          => 15;
use constant GDOME_ATTRIBUTE_DECL        => 16;
use constant GDOME_ENTITY_DECL           => 17;
use constant GDOME_NAMESPACE_DECL        => 18;
use constant GDOME_XINCLUDE_START        => 19;
use constant GDOME_XINCLUDE_END          => 20;

1;
#-------------------------------------------------------------------------#
__END__

