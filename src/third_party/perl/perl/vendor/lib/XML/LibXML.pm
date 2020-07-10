# $Id$
#
#
# This is free software, you may use it and distribute it under the same terms as
# Perl itself.
#
# Copyright 2001-2003 AxKit.com Ltd., 2002-2006 Christian Glahn, 2006-2009 Petr Pajas
#
#

package XML::LibXML;

use strict;
use warnings;

use vars qw($VERSION $ABI_VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS
            $skipDTD $skipXMLDeclaration $setTagCompression
            $MatchCB $ReadCB $OpenCB $CloseCB %PARSER_FLAGS
	    $XML_LIBXML_PARSE_DEFAULTS
            );
use Carp;

use constant XML_XMLNS_NS => 'http://www.w3.org/2000/xmlns/';
use constant XML_XML_NS => 'http://www.w3.org/XML/1998/namespace';

use XML::LibXML::Error;
use XML::LibXML::NodeList;
use XML::LibXML::XPathContext;
use IO::Handle; # for FH reads called as methods

BEGIN {
$VERSION = "2.0200"; # VERSION TEMPLATE: DO NOT CHANGE
$ABI_VERSION = 2;
require Exporter;
require DynaLoader;
@ISA = qw(DynaLoader Exporter);

use vars qw($__PROXY_NODE_REGISTRY $__threads_shared $__PROXY_NODE_REGISTRY_MUTEX $__loaded);

sub VERSION {
  my $class = shift;
  my ($caller) = caller;
  my $req_abi = $ABI_VERSION;
  if (UNIVERSAL::can($caller,'REQUIRE_XML_LIBXML_ABI_VERSION')) {
    $req_abi = $caller->REQUIRE_XML_LIBXML_ABI_VERSION();
  } elsif ($caller eq 'XML::LibXSLT') {
    # XML::LibXSLT without REQUIRE_XML_LIBXML_ABI_VERSION is an old and incompatible version
    $req_abi = 1;
  }
  unless ($req_abi == $ABI_VERSION) {
    my $ver = @_ ? ' '.$_[0] : '';
    die ("This version of $caller requires XML::LibXML$ver (ABI $req_abi), which is incompatible with currently installed XML::LibXML $VERSION (ABI $ABI_VERSION). Please upgrade $caller, XML::LibXML, or both!");
  }
  return $class->UNIVERSAL::VERSION(@_)
}

#-------------------------------------------------------------------------#
# export information                                                      #
#-------------------------------------------------------------------------#
%EXPORT_TAGS = (
                all => [qw(
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
                           encodeToUTF8
                           decodeFromUTF8
		           XML_XMLNS_NS
		           XML_XML_NS
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
                encoding => [qw(
                                encodeToUTF8
                                decodeFromUTF8
                               )],
		ns => [qw(
		           XML_XMLNS_NS
		           XML_XML_NS
		 )],
               );

@EXPORT_OK = (
              @{$EXPORT_TAGS{all}},
             );

@EXPORT = (
           @{$EXPORT_TAGS{all}},
          );

#-------------------------------------------------------------------------#
# initialization of the global variables                                  #
#-------------------------------------------------------------------------#
$skipDTD            = 0;
$skipXMLDeclaration = 0;
$setTagCompression  = 0;

$MatchCB = undef;
$ReadCB  = undef;
$OpenCB  = undef;
$CloseCB = undef;

# if ($threads::threads) {
#   our $__THREADS_TID = 0;
#   eval q{
#     use threads::shared;
#     our $__PROXY_NODE_REGISTRY_MUTEX :shared = 0;
#   };
#   die $@ if $@;
# }
#-------------------------------------------------------------------------#
# bootstrapping                                                           #
#-------------------------------------------------------------------------#
bootstrap XML::LibXML $VERSION;
undef &AUTOLOAD;

*encodeToUTF8 = \&XML::LibXML::Common::encodeToUTF8;
*decodeFromUTF8 = \&XML::LibXML::Common::decodeFromUTF8;

} # BEGIN


#-------------------------------------------------------------------------#
# libxml2 node names (see also XML::LibXML::Common                        #
#-------------------------------------------------------------------------#
use constant XML_ELEMENT_NODE            => 1;
use constant XML_ATTRIBUTE_NODE          => 2;
use constant XML_TEXT_NODE               => 3;
use constant XML_CDATA_SECTION_NODE      => 4;
use constant XML_ENTITY_REF_NODE         => 5;
use constant XML_ENTITY_NODE             => 6;
use constant XML_PI_NODE                 => 7;
use constant XML_COMMENT_NODE            => 8;
use constant XML_DOCUMENT_NODE           => 9;
use constant XML_DOCUMENT_TYPE_NODE      => 10;
use constant XML_DOCUMENT_FRAG_NODE      => 11;
use constant XML_NOTATION_NODE           => 12;
use constant XML_HTML_DOCUMENT_NODE      => 13;
use constant XML_DTD_NODE                => 14;
use constant XML_ELEMENT_DECL            => 15;
use constant XML_ATTRIBUTE_DECL          => 16;
use constant XML_ENTITY_DECL             => 17;
use constant XML_NAMESPACE_DECL          => 18;
use constant XML_XINCLUDE_START          => 19;
use constant XML_XINCLUDE_END            => 20;


sub import {
  my $package=shift;
  if (grep /^:threads_shared$/, @_) {
    require threads;
    if (!defined($__threads_shared)) {
      if (INIT_THREAD_SUPPORT()) {
	eval q{
          use threads::shared;
          share($__PROXY_NODE_REGISTRY_MUTEX);
        };
	if ($@) { # something went wrong
	  DISABLE_THREAD_SUPPORT(); # leave the library in a usable state
	  die $@; # and die
	}
	$__PROXY_NODE_REGISTRY = XML::LibXML::HashTable->new();
	$__threads_shared=1;
      } else {
	croak("XML::LibXML or Perl compiled without ithread support!");
      }
    } elsif (!$__threads_shared) {
      croak("XML::LibXML already loaded without thread support. Too late to enable thread support!");
    }
  } elsif (defined $XML::LibXML::__loaded) {
    $__threads_shared=0 if not defined $__threads_shared;
  }
  __PACKAGE__->export_to_level(1,$package,grep !/^:threads(_shared)?$/,@_);
}

sub threads_shared_enabled {
  return $__threads_shared ? 1 : 0;
}

# if ($threads::threads) {
#   our $__PROXY_NODE_REGISTRY = XML::LibXML::HashTable->new();
# }

#-------------------------------------------------------------------------#
# test exact version (up to patch-level)                                  #
#-------------------------------------------------------------------------#
{
  my ($runtime_version) = LIBXML_RUNTIME_VERSION() =~ /^(\d+)/;
  if ( $runtime_version < LIBXML_VERSION ) {
    warn "Warning: XML::LibXML compiled against libxml2 ".LIBXML_VERSION.
      ", but runtime libxml2 is older $runtime_version\n";
  }
}


#-------------------------------------------------------------------------#
# parser flags                                                            #
#-------------------------------------------------------------------------#

# Copied directly from http://xmlsoft.org/html/libxml-parser.html#xmlParserOption
use constant {
  XML_PARSE_RECOVER	  => 1,	       # recover on errors
  XML_PARSE_NOENT	  => 2,	       # substitute entities
  XML_PARSE_DTDLOAD	  => 4,	       # load the external subset
  XML_PARSE_DTDATTR	  => 8,	       # default DTD attributes
  XML_PARSE_DTDVALID	  => 16,       # validate with the DTD
  XML_PARSE_NOERROR	  => 32,       # suppress error reports
  XML_PARSE_NOWARNING	  => 64,       # suppress warning reports
  XML_PARSE_PEDANTIC	  => 128,      # pedantic error reporting
  XML_PARSE_NOBLANKS	  => 256,      # remove blank nodes
  XML_PARSE_SAX1	  => 512,      # use the SAX1 interface internally
  XML_PARSE_XINCLUDE	  => 1024,     # Implement XInclude substitution
  XML_PARSE_NONET	  => 2048,     # Forbid network access
  XML_PARSE_NODICT	  => 4096,     # Do not reuse the context dictionary
  XML_PARSE_NSCLEAN	  => 8192,     # remove redundant namespaces declarations
  XML_PARSE_NOCDATA	  => 16384,    # merge CDATA as text nodes
  XML_PARSE_NOXINCNODE	  => 32768,    # do not generate XINCLUDE START/END nodes
  XML_PARSE_COMPACT	  => 65536,    # compact small text nodes; no modification of the tree allowed afterwards
                                       # (will possibly crash if you try to modify the tree)
  XML_PARSE_OLD10	  => 131072,   # parse using XML-1.0 before update 5
  XML_PARSE_NOBASEFIX	  => 262144,   # do not fixup XINCLUDE xml#base uris
  XML_PARSE_HUGE	  => 524288,   # relax any hardcoded limit from the parser
  XML_PARSE_OLDSAX	  => 1048576,  # parse using SAX2 interface from before 2.7.0
  HTML_PARSE_RECOVER => (1<<0),       # suppress error reports
  HTML_PARSE_NOERROR  => (1<<5),       # suppress error reports
};

$XML_LIBXML_PARSE_DEFAULTS = ( XML_PARSE_NODICT | XML_PARSE_DTDLOAD | XML_PARSE_NOENT );

# this hash is made global so that applications can add names for new
# libxml2 parser flags as temporary workaround

%PARSER_FLAGS = (
  recover		 => XML_PARSE_RECOVER,
  expand_entities	 => XML_PARSE_NOENT,
  load_ext_dtd	         => XML_PARSE_DTDLOAD,
  complete_attributes	 => XML_PARSE_DTDATTR,
  validation		 => XML_PARSE_DTDVALID,
  suppress_errors	 => XML_PARSE_NOERROR,
  suppress_warnings	 => XML_PARSE_NOWARNING,
  pedantic_parser	 => XML_PARSE_PEDANTIC,
  no_blanks		 => XML_PARSE_NOBLANKS,
  expand_xinclude	 => XML_PARSE_XINCLUDE,
  xinclude		 => XML_PARSE_XINCLUDE,
  no_network		 => XML_PARSE_NONET,
  clean_namespaces	 => XML_PARSE_NSCLEAN,
  no_cdata		 => XML_PARSE_NOCDATA,
  no_xinclude_nodes	 => XML_PARSE_NOXINCNODE,
  old10		         => XML_PARSE_OLD10,
  no_base_fix		 => XML_PARSE_NOBASEFIX,
  huge		         => XML_PARSE_HUGE,
  oldsax		 => XML_PARSE_OLDSAX,
);

my %OUR_FLAGS = (
  recover => 'XML_LIBXML_RECOVER',
  line_numbers => 'XML_LIBXML_LINENUMBERS',
  URI => 'XML_LIBXML_BASE_URI',
  base_uri => 'XML_LIBXML_BASE_URI',
  gdome => 'XML_LIBXML_GDOME',
  ext_ent_handler => 'ext_ent_handler',
);

sub _parser_options {
  my ($self, $opts) = @_;

  # currently dictionaries break XML::LibXML memory management

  my $flags;

  if (ref($self)) {
    $flags = ($self->{XML_LIBXML_PARSER_OPTIONS}||0);
  } else {
    $flags = $XML_LIBXML_PARSE_DEFAULTS;		# safety precaution
  }

  my ($key, $value);
  while (($key,$value) = each %$opts) {
    my $f = $PARSER_FLAGS{ $key };
    if (defined $f) {
      if ($value) {
	$flags |= $f
      } else {
	$flags &= ~$f;
      }
    } elsif ($key eq 'set_parser_flags') { # this can be used to pass flags XML::LibXML does not yet know about
      $flags |= $value;
    } elsif ($key eq 'unset_parser_flags') {
      $flags &= ~$value;
    }

  }
  return $flags;
}

my %compatibility_flags = (
  XML_LIBXML_VALIDATION => 'validation',
  XML_LIBXML_EXPAND_ENTITIES => 'expand_entities',
  XML_LIBXML_PEDANTIC => 'pedantic_parser',
  XML_LIBXML_NONET => 'no_network',
  XML_LIBXML_EXT_DTD => 'load_ext_dtd',
  XML_LIBXML_COMPLETE_ATTR => 'complete_attributes',
  XML_LIBXML_EXPAND_XINCLUDE => 'expand_xinclude',
  XML_LIBXML_NSCLEAN => 'clean_namespaces',
  XML_LIBXML_KEEP_BLANKS => 'keep_blanks',
  XML_LIBXML_LINENUMBERS => 'line_numbers',
);

#-------------------------------------------------------------------------#
# parser constructor                                                      #
#-------------------------------------------------------------------------#


sub new {
    my $class = shift;
    my $self = bless {
    }, $class;
    if (@_) {
      my %opts = ();
      if (ref($_[0]) eq 'HASH') {
	%opts = %{$_[0]};
      } else {
	# old interface
	my %args = @_;
	%opts=(
	  map {
	    (($compatibility_flags{ $_ }||$_) => $args{ $_ })
	  } keys %args
	);
      }
      # parser flags
      $opts{no_blanks} = !$opts{keep_blanks} if exists($opts{keep_blanks}) and !exists($opts{no_blanks});

      for (keys %OUR_FLAGS) {
	$self->{$OUR_FLAGS{$_}} = delete $opts{$_};
      }
      $class->load_catalog(delete($opts{catalog})) if $opts{catalog};

      $self->{XML_LIBXML_PARSER_OPTIONS} = XML::LibXML->_parser_options(\%opts);

      # store remaining unknown options directly in $self
      for (keys %opts) {
	$self->{$_}=$opts{$_} unless exists $PARSER_FLAGS{$_};
      }
    } else {
      $self->{XML_LIBXML_PARSER_OPTIONS} = $XML_LIBXML_PARSE_DEFAULTS;
    }
    if ( defined $self->{Handler} ) {
      $self->set_handler( $self->{Handler} );
    }

    $self->{_State_} = 0;
    return $self;
}

sub _clone {
  my ($self)=@_;
  my $new = ref($self)->new({
      recover => $self->{XML_LIBXML_RECOVER},
      line_numbers => $self->{XML_LIBXML_LINENUMBERS},
      base_uri => $self->{XML_LIBXML_BASE_URI},
      gdome => $self->{XML_LIBXML_GDOME},
    });
  # The parser options may contain some options that were zeroed from the
  # defaults so set_parser_flags won't work here. We need to assign them
  # explicitly.
  $new->{XML_LIBXML_PARSER_OPTIONS} = $self->{XML_LIBXML_PARSER_OPTIONS};
  $new->input_callbacks($self->input_callbacks());
  return $new;
}

#-------------------------------------------------------------------------#
# Threads support methods                                                 #
#-------------------------------------------------------------------------#

# threads doc says CLONE's API may change in future, which would break
# an XS method prototype
sub CLONE {
  if ($XML::LibXML::__threads_shared) {
    XML::LibXML::_CLONE( $_[0] );
  }
}

sub CLONE_SKIP {
  return $XML::LibXML::__threads_shared ? 0 : 1;
}

sub __proxy_registry {
  my ($class)=caller;
  die "This version of $class uses API of XML::LibXML 1.66 which is not compatible with XML::LibXML $VERSION. Please upgrade $class!\n";
}

#-------------------------------------------------------------------------#
# DOM Level 2 document constructor                                        #
#-------------------------------------------------------------------------#

sub createDocument {
   my $self = shift;
   if (!@_ or $_[0] =~ m/^\d\.\d$/) {
     # for backward compatibility
     return XML::LibXML::Document->new(@_);
   }
   else {
     # DOM API: createDocument(namespaceURI, qualifiedName, doctype?)
     my $doc = XML::LibXML::Document-> new;
     my $el = $doc->createElementNS(shift, shift);
     $doc->setDocumentElement($el);
     $doc->setExternalSubset(shift) if @_;
     return $doc;
   }
}

#-------------------------------------------------------------------------#
# callback functions                                                      #
#-------------------------------------------------------------------------#

sub externalEntityLoader(&)
{
    return _externalEntityLoader($_[0]);
}

sub input_callbacks {
    my $self     = shift;
    my $icbclass = shift;

    if ( defined $icbclass ) {
        $self->{XML_LIBXML_CALLBACK_STACK} = $icbclass;
    }
    return $self->{XML_LIBXML_CALLBACK_STACK};
}

sub match_callback {
    my $self = shift;
    if ( ref $self ) {
        if ( scalar @_ ) {
            $self->{XML_LIBXML_MATCH_CB} = shift;
            $self->{XML_LIBXML_CALLBACK_STACK} = undef;
        }
        return $self->{XML_LIBXML_MATCH_CB};
    }
    else {
        $MatchCB = shift if scalar @_;
        return $MatchCB;
    }
}

sub read_callback {
    my $self = shift;
    if ( ref $self ) {
        if ( scalar @_ ) {
            $self->{XML_LIBXML_READ_CB} = shift;
            $self->{XML_LIBXML_CALLBACK_STACK} = undef;
        }
        return $self->{XML_LIBXML_READ_CB};
    }
    else {
        $ReadCB = shift if scalar @_;
        return $ReadCB;
    }
}

sub close_callback {
    my $self = shift;
    if ( ref $self ) {
        if ( scalar @_ ) {
            $self->{XML_LIBXML_CLOSE_CB} = shift;
            $self->{XML_LIBXML_CALLBACK_STACK} = undef;
        }
        return $self->{XML_LIBXML_CLOSE_CB};
    }
    else {
        $CloseCB = shift if scalar @_;
        return $CloseCB;
    }
}

sub open_callback {
    my $self = shift;
    if ( ref $self ) {
        if ( scalar @_ ) {
            $self->{XML_LIBXML_OPEN_CB} = shift;
            $self->{XML_LIBXML_CALLBACK_STACK} = undef;
        }
        return $self->{XML_LIBXML_OPEN_CB};
    }
    else {
        $OpenCB = shift if scalar @_;
        return $OpenCB;
    }
}

sub callbacks {
    my $self = shift;
    if ( ref $self ) {
        if (@_) {
            my ($match, $open, $read, $close) = @_;
            @{$self}{qw(XML_LIBXML_MATCH_CB XML_LIBXML_OPEN_CB XML_LIBXML_READ_CB XML_LIBXML_CLOSE_CB)} = ($match, $open, $read, $close);
            $self->{XML_LIBXML_CALLBACK_STACK} = undef;
        }
        else {
            return @{$self}{qw(XML_LIBXML_MATCH_CB XML_LIBXML_OPEN_CB XML_LIBXML_READ_CB XML_LIBXML_CLOSE_CB)};
        }
    }
    else {
        if (@_) {
           ( $MatchCB, $OpenCB, $ReadCB, $CloseCB ) = @_;
        }
        else {
            return ( $MatchCB, $OpenCB, $ReadCB, $CloseCB );
        }
    }
}

#-------------------------------------------------------------------------#
# internal member variable manipulation                                   #
#-------------------------------------------------------------------------#
sub __parser_option {
  my ($self, $opt) = @_;
  if (@_>2) {
    if ($_[2]) {
      $self->{XML_LIBXML_PARSER_OPTIONS} |= $opt;
      return 1;
    } else {
      $self->{XML_LIBXML_PARSER_OPTIONS} &= ~$opt;
      return 0;
    }
  } else {
    return ($self->{XML_LIBXML_PARSER_OPTIONS} & $opt) ? 1 : 0;
  }
}

sub option_exists {
    my ($self,$name)=@_;
    return ($PARSER_FLAGS{$name} || $OUR_FLAGS{$name}) ? 1 : 0;
}
sub get_option {
    my ($self,$name)=@_;
    my $flag = $OUR_FLAGS{$name};
    return $self->{$flag} if $flag;
    $flag = $PARSER_FLAGS{$name};
    return $self->__parser_option($flag) if $flag;
    warn "XML::LibXML::get_option: unknown parser option $name\n";
    return undef;
}
sub set_option {
    my ($self,$name,$value)=@_;
    my $flag = $OUR_FLAGS{$name};
    return ($self->{$flag}=$value) if $flag;
    $flag = $PARSER_FLAGS{$name};
    return $self->__parser_option($flag,$value) if $flag;
    warn "XML::LibXML::get_option: unknown parser option $name\n";
    return undef;
}
sub set_options {
  my $self=shift;
  my $opts;
  if (@_==1 and ref($_[0]) eq 'HASH') {
    $opts = $_[0];
  } elsif (@_ % 2 == 0) {
    $opts={@_};
  } else {
    croak("Odd number of elements passed to set_options");
  }
  $self->set_option($_=>$opts->{$_}) foreach keys %$opts;
  return;
}

sub validation {
    my $self = shift;
    return $self->__parser_option(XML_PARSE_DTDVALID,@_);
}

sub recover {
    my $self = shift;
    if (scalar @_) {
      $self->{XML_LIBXML_RECOVER} = $_[0];
      $self->__parser_option(XML_PARSE_RECOVER,@_);
    }
    return $self->{XML_LIBXML_RECOVER};
}

sub recover_silently {
    my $self = shift;
    my $arg = shift;
    if ( defined($arg) )
    {
        $self->recover(($arg == 1) ? 2 : $arg);
    }
    return (($self->recover()||0) == 2) ? 1 : 0;
}

sub expand_entities {
    my $self = shift;
    if (scalar(@_) and $_[0]) {
      return $self->__parser_option(XML_PARSE_NOENT | XML_PARSE_DTDLOAD,1);
    }
    return $self->__parser_option(XML_PARSE_NOENT,@_);
}

sub keep_blanks {
    my $self = shift;
    my @args; # we have to negate the argument and return negated value, since
              # the actual flag is no_blanks
    if (scalar @_) {
      @args=($_[0] ? 0 : 1);
    }
    return $self->__parser_option(XML_PARSE_NOBLANKS,@args) ? 0 : 1;
}

sub pedantic_parser {
    my $self = shift;
    return $self->__parser_option(XML_PARSE_PEDANTIC,@_);
}

sub line_numbers {
    my $self = shift;
    $self->{XML_LIBXML_LINENUMBERS} = shift if scalar @_;
    return $self->{XML_LIBXML_LINENUMBERS};
}

sub no_network {
    my $self = shift;
    return $self->__parser_option(XML_PARSE_NONET,@_);
}

sub load_ext_dtd {
    my $self = shift;
    return $self->__parser_option(XML_PARSE_DTDLOAD,@_);
}

sub complete_attributes {
    my $self = shift;
    return $self->__parser_option(XML_PARSE_DTDATTR,@_);
}

sub expand_xinclude  {
    my $self = shift;
    return $self->__parser_option(XML_PARSE_XINCLUDE,@_);
}

sub base_uri {
    my $self = shift;
    $self->{XML_LIBXML_BASE_URI} = shift if scalar @_;
    return $self->{XML_LIBXML_BASE_URI};
}

sub gdome_dom {
    my $self = shift;
    $self->{XML_LIBXML_GDOME} = shift if scalar @_;
    return $self->{XML_LIBXML_GDOME};
}

sub clean_namespaces {
    my $self = shift;
    return $self->__parser_option(XML_PARSE_NSCLEAN,@_);
}

#-------------------------------------------------------------------------#
# set the optional SAX(2) handler                                         #
#-------------------------------------------------------------------------#
sub set_handler {
    my $self = shift;
    if ( defined $_[0] ) {
        $self->{HANDLER} = $_[0];

        $self->{SAX_ELSTACK} = [];
        $self->{SAX} = {State => 0};
    }
    else {
        # undef SAX handling
        $self->{SAX_ELSTACK} = [];
        delete $self->{HANDLER};
        delete $self->{SAX};
    }
}

#-------------------------------------------------------------------------#
# helper functions                                                        #
#-------------------------------------------------------------------------#
sub _auto_expand {
    my ( $self, $result, $uri ) = @_;

    $result->setBaseURI( $uri ) if defined $uri;

    if ( $self->expand_xinclude ) {
        $self->{_State_} = 1;
        eval { $self->processXIncludes($result); };
        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
            $self->_cleanup_callbacks();
            $result = undef;
            croak $err;
        }
    }
    return $result;
}

sub _init_callbacks {
    my $self = shift;
    my $icb = $self->{XML_LIBXML_CALLBACK_STACK};
    unless ( defined $icb ) {
        $self->{XML_LIBXML_CALLBACK_STACK} = XML::LibXML::InputCallback->new();
        $icb = $self->{XML_LIBXML_CALLBACK_STACK};
    }

    $icb->init_callbacks($self);
}

sub _cleanup_callbacks {
    my $self = shift;
    $self->{XML_LIBXML_CALLBACK_STACK}->cleanup_callbacks();
}

sub __read {
    read($_[0], $_[1], $_[2]);
}

sub __write {
    if ( ref( $_[0] ) ) {
        $_[0]->write( $_[1], $_[2] );
    }
    else {
        $_[0]->write( $_[1] );
    }
}

sub load_xml {
  my $class_or_self = shift;
  my %args = map { ref($_) eq 'HASH' ? (%$_) : $_ } @_;

  my $URI = delete($args{URI});
  $URI = "$URI"  if defined $URI; # stringify in case it is an URI object
  my $parser;
  if (ref($class_or_self)) {
    $parser = $class_or_self->_clone();
    $parser->{XML_LIBXML_PARSER_OPTIONS} = $parser->_parser_options(\%args);
  } else {
    $parser = $class_or_self->new(\%args);
  }
  my $dom;
  if ( defined $args{location} ) {
    $dom = $parser->parse_file( "$args{location}" );
  }
  elsif ( defined $args{string} ) {
    $dom = $parser->parse_string( $args{string}, $URI );
  }
  elsif ( defined $args{IO} ) {
    $dom = $parser->parse_fh( $args{IO}, $URI );
  }
  else {
    croak("XML::LibXML->load: specify location, string, or IO");
  }
  return $dom;
}

sub load_html {
  my ($class_or_self) = shift;
  my %args = map { ref($_) eq 'HASH' ? (%$_) : $_ } @_;
  my $URI = delete($args{URI});
  $URI = "$URI"  if defined $URI; # stringify in case it is an URI object
  my $parser;
  if (ref($class_or_self)) {
    $parser = $class_or_self->_clone();
  } else {
    $parser = $class_or_self->new();
  }
  my $dom;
  if ( defined $args{location} ) {
    $dom = $parser->parse_html_file( "$args{location}", \%args );
  }
  elsif ( defined $args{string} ) {
    $dom = $parser->parse_html_string( $args{string}, \%args );
  }
  elsif ( defined $args{IO} ) {
    $dom = $parser->parse_html_fh( $args{IO}, \%args );
  }
  else {
    croak("XML::LibXML->load: specify location, string, or IO");
  }
  return $dom;
}

#-------------------------------------------------------------------------#
# parsing functions                                                       #
#-------------------------------------------------------------------------#
# all parsing functions handle normal as SAX parsing at the same time.
# note that SAX parsing is handled incomplete! use XML::LibXML::SAX for
# complete parsing sequences
#-------------------------------------------------------------------------#
sub parse_string {
    my $self = shift;
    croak("parse_string is not a class method! Create a parser object with XML::LibXML->new first!") unless ref $self;
    croak("parse already in progress") if $self->{_State_};

    unless ( defined $_[0] and length $_[0] ) {
        croak("Empty String");
    }

    $self->{_State_} = 1;
    my $result;

    $self->_init_callbacks();

    if ( defined $self->{SAX} ) {
        my $string = shift;
        $self->{SAX_ELSTACK} = [];
        eval { $result = $self->_parse_sax_string($string); };
        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
            chomp $err unless ref $err;
            $self->_cleanup_callbacks();
            croak $err;
        }
    }
    else {
        eval { $result = $self->_parse_string( @_ ); };

        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
            chomp $err unless ref $err;
            $self->_cleanup_callbacks();
            croak $err;
        }

        $result = $self->_auto_expand( $result, $self->{XML_LIBXML_BASE_URI} );
    }
    $self->_cleanup_callbacks();

    return $result;
}

sub parse_fh {
    my $self = shift;
    croak("parse_fh is not a class method! Create a parser object with XML::LibXML->new first!") unless ref $self;
    croak("parse already in progress") if $self->{_State_};
    $self->{_State_} = 1;
    my $result;

    $self->_init_callbacks();

    if ( defined $self->{SAX} ) {
        $self->{SAX_ELSTACK} = [];
        eval { $self->_parse_sax_fh( @_ );  };
        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
	    chomp $err unless ref $err;
            $self->_cleanup_callbacks();
            croak $err;
        }
    }
    else {
        eval { $result = $self->_parse_fh( @_ ); };
        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
	    chomp $err unless ref $err;
            $self->_cleanup_callbacks();
            croak $err;
        }

        $result = $self->_auto_expand( $result, $self->{XML_LIBXML_BASE_URI} );
    }

    $self->_cleanup_callbacks();

    return $result;
}

sub parse_file {
    my $self = shift;
    croak("parse_file is not a class method! Create a parser object with XML::LibXML->new first!") unless ref $self;
    croak("parse already in progress") if $self->{_State_};

    $self->{_State_} = 1;
    my $result;

    $self->_init_callbacks();

    if ( defined $self->{SAX} ) {
        $self->{SAX_ELSTACK} = [];
        eval { $self->_parse_sax_file( @_ );  };
        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
	    chomp $err unless ref $err;
            $self->_cleanup_callbacks();
            croak $err;
        }
    }
    else {
        eval { $result = $self->_parse_file(@_); };
        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
	    chomp $err unless ref $err;
            $self->_cleanup_callbacks();
            croak $err;
        }

        $result = $self->_auto_expand( $result );
    }
    $self->_cleanup_callbacks();

    return $result;
}

sub parse_xml_chunk {
    my $self = shift;
    # max 2 parameter:
    # 1: the chunk
    # 2: the encoding of the string
    croak("parse_xml_chunk is not a class method! Create a parser object with XML::LibXML->new first!") unless ref $self;
    croak("parse already in progress") if $self->{_State_};    my $result;

    unless ( defined $_[0] and length $_[0] ) {
        croak("Empty String");
    }

    $self->{_State_} = 1;

    $self->_init_callbacks();

    if ( defined $self->{SAX} ) {
        eval {
            $self->_parse_sax_xml_chunk( @_ );

            # this is required for XML::GenericChunk.
            # in normal case is_filter is not defined, an thus the parsing
            # will be terminated. in case of a SAX filter the parsing is not
            # finished at that state. therefore we must not reset the parsing
            unless ( $self->{IS_FILTER} ) {
	      $result = $self->{HANDLER}->end_document();
	    }
        };
    }
    else {
        eval { $result = $self->_parse_xml_chunk( @_ ); };
    }

    $self->_cleanup_callbacks();

    my $err = $@;
    $self->{_State_} = 0;
    if ($err) {
        chomp $err unless ref $err;
        croak $err;
    }

    return $result;
}

sub parse_balanced_chunk {
    my $self = shift;
    $self->_init_callbacks();
    my $rv;
    eval {
        $rv = $self->parse_xml_chunk( @_ );
    };
    my $err = $@;
    $self->_cleanup_callbacks();
    if ( $err ) {
        chomp $err unless ref $err;
        croak $err;
    }
    return $rv
}

# java style
sub processXIncludes {
    my $self = shift;
    my $doc = shift;
    my $opts = shift;
    my $options = $self->_parser_options($opts);
    if ( $self->{_State_} != 1 ) {
        $self->_init_callbacks();
    }
    my $rv;
    eval {
        $rv = $self->_processXIncludes($doc || " ", $options);
    };
    my $err = $@;
    if ( $self->{_State_} != 1 ) {
        $self->_cleanup_callbacks();
    }

    if ( $err ) {
        chomp $err unless ref $err;
        croak $err;
    }
    return $rv;
}

# perl style
sub process_xincludes {
    my $self = shift;
    my $doc = shift;
    my $opts = shift;
    my $options = $self->_parser_options($opts);

    my $rv;
    $self->_init_callbacks();
    eval {
        $rv = $self->_processXIncludes($doc || " ", $options);
    };
    my $err = $@;
    $self->_cleanup_callbacks();
    if ( $err ) {
        chomp $err unless ref $err;
        croak $@;
    }
    return $rv;
}

#-------------------------------------------------------------------------#
# HTML parsing functions                                                  #
#-------------------------------------------------------------------------#

sub _html_options {
  my ($self,$opts)=@_;
  $opts = {} unless ref $opts;
  #  return (undef,undef) unless ref $opts;
  my $flags = 0;
  {
    my $recover = exists $opts->{recover} ? $opts->{recover} : $self->recover;

    if ($recover)
    {
      $flags |= HTML_PARSE_RECOVER;
      if ($recover == 2)
      {
        $flags |= HTML_PARSE_NOERROR;
      }
    }
  }

  $flags |=     4 if $opts->{no_defdtd}; # default is ON: injects DTD as needed
  $flags |=    32 if exists $opts->{suppress_errors} ? $opts->{suppress_errors} : $self->get_option('suppress_errors');
  # This is to fix https://rt.cpan.org/Ticket/Display.html?id=58024 :
  # <quote>
  # In XML::LibXML, warnings are not suppressed when specifying the recover
  # or recover_silently flags as per the following excerpt from the manpage:
  # </quote>
  if ($self->recover_silently)
  {
      $flags |= 32;
  }
  $flags |=    64 if $opts->{suppress_warnings};
  $flags |=   128 if exists $opts->{pedantic_parser} ? $opts->{pedantic_parser} : $self->pedantic_parser;
  $flags |=   256 if exists $opts->{no_blanks} ? $opts->{no_blanks} : !$self->keep_blanks;
  $flags |=  2048 if exists $opts->{no_network} ? $opts->{no_network} : !$self->no_network;
  $flags |= 16384 if $opts->{no_cdata};
  $flags |= 65536 if $opts->{compact}; # compact small text nodes; no modification
                                         # of the tree allowed afterwards
                                         # (WILL possibly CRASH IF YOU try to MODIFY THE TREE)
  $flags |= 524288 if $opts->{huge}; # relax any hardcoded limit from the parser
  $flags |= 1048576 if $opts->{oldsax}; # parse using SAX2 interface from before 2.7.0

  return ($opts->{URI},$opts->{encoding},$flags);
}

sub parse_html_string {
    my ($self,$str,$opts) = @_;
    croak("parse_html_string is not a class method! Create a parser object with XML::LibXML->new first!") unless ref $self;
    croak("parse already in progress") if $self->{_State_};

    unless ( defined $str and length $str ) {
        croak("Empty String");
    }
    $self->{_State_} = 1;
    my $result;

    $self->_init_callbacks();
    eval {
      $result = $self->_parse_html_string( $str,
					   $self->_html_options($opts)
					  );
    };
    my $err = $@;
    $self->{_State_} = 0;
    if ($err) {
      chomp $err unless ref $err;
      $self->_cleanup_callbacks();
      croak $err;
    }

    $self->_cleanup_callbacks();

    return $result;
}

sub parse_html_file {
    my ($self,$file,$opts) = @_;
    croak("parse_html_file is not a class method! Create a parser object with XML::LibXML->new first!") unless ref $self;
    croak("parse already in progress") if $self->{_State_};
    $self->{_State_} = 1;
    my $result;

    $self->_init_callbacks();
    eval { $result = $self->_parse_html_file($file,
					     $self->_html_options($opts)
					    ); };
    my $err = $@;
    $self->{_State_} = 0;
    if ($err) {
      chomp $err unless ref $err;
      $self->_cleanup_callbacks();
      croak $err;
    }

    $self->_cleanup_callbacks();

    return $result;
}

sub parse_html_fh {
    my ($self,$fh,$opts) = @_;
    croak("parse_html_fh is not a class method! Create a parser object with XML::LibXML->new first!") unless ref $self;
    croak("parse already in progress") if $self->{_State_};
    $self->{_State_} = 1;

    my $result;
    $self->_init_callbacks();
    eval { $result = $self->_parse_html_fh( $fh,
					    $self->_html_options($opts)
					   ); };
    my $err = $@;
    $self->{_State_} = 0;
    if ($err) {
      chomp $err unless ref $err;
      $self->_cleanup_callbacks();
      croak $err;
    }
    $self->_cleanup_callbacks();

    return $result;
}

#-------------------------------------------------------------------------#
# push parser interface                                                   #
#-------------------------------------------------------------------------#
sub init_push {
    my $self = shift;

    if ( defined $self->{CONTEXT} ) {
        delete $self->{CONTEXT};
    }

    if ( defined $self->{SAX} ) {
        $self->{CONTEXT} = $self->_start_push(1);
    }
    else {
        $self->{CONTEXT} = $self->_start_push(0);
    }
}

sub push {
    my $self = shift;

    $self->_init_callbacks();

    if ( not defined $self->{CONTEXT} ) {
        $self->init_push();
    }

    eval {
        foreach ( @_ ) {
            $self->_push( $self->{CONTEXT}, $_ );
        }
    };
    my $err = $@;
    $self->_cleanup_callbacks();
    if ( $err ) {
        chomp $err unless ref $err;
        croak $err;
    }
}

# this function should be promoted!
# the reason is because libxml2 uses xmlParseChunk() for this purpose!
sub parse_chunk {
    my $self = shift;
    my $chunk = shift;
    my $terminate = shift;

    if ( not defined $self->{CONTEXT} ) {
        $self->init_push();
    }

    if ( defined $chunk and length $chunk ) {
        $self->_push( $self->{CONTEXT}, $chunk );
    }

    if ( $terminate ) {
        return $self->finish_push();
    }
}


sub finish_push {
    my $self = shift;
    my $restore = shift || 0;
    return undef unless defined $self->{CONTEXT};

    my $retval;

    if ( defined $self->{SAX} ) {
        eval {
            $self->_end_sax_push( $self->{CONTEXT} );
            $retval = $self->{HANDLER}->end_document( {} );
        };
    }
    else {
        eval { $retval = $self->_end_push( $self->{CONTEXT}, $restore ); };
    }
    my $err = $@;
    delete $self->{CONTEXT};
    if ( $err ) {
        chomp $err unless ref $err;
        croak( $err );
    }
    return $retval;
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Node Interface                                             #
#-------------------------------------------------------------------------#
package XML::LibXML::Node;

use Carp qw(croak);

use overload
    '""'   => sub { $_[0]->toString() },
    'bool' => sub { 1 },
    '0+'   => sub { Scalar::Util::refaddr($_[0]) },
    fallback => 1,
    ;


sub CLONE_SKIP {
  return $XML::LibXML::__threads_shared ? 0 : 1;
}

sub isSupported {
    my $self    = shift;
    my $feature = shift;
    return $self->can($feature) ? 1 : 0;
}

sub getChildNodes { my $self = shift; return $self->childNodes(); }

sub childNodes {
    my $self = shift;
    my @children = $self->_childNodes(0);
    return wantarray ? @children : XML::LibXML::NodeList->new_from_ref(\@children , 1);
}

sub nonBlankChildNodes {
    my $self = shift;
    my @children = $self->_childNodes(1);
    return wantarray ? @children : XML::LibXML::NodeList->new_from_ref(\@children , 1);
}

sub attributes {
    my $self = shift;
    my @attr = $self->_attributes();
    return wantarray ? @attr : XML::LibXML::NamedNodeMap->new( @attr );
}


sub findnodes {
    my ($node, $xpath) = @_;
    my @nodes = $node->_findnodes($xpath);
    if (wantarray) {
        return @nodes;
    }
    else {
        return XML::LibXML::NodeList->new_from_ref(\@nodes, 1);
    }
}

sub exists {
    my ($node, $xpath) = @_;
    my (undef, $value) = $node->_find($xpath,1);
    return $value;
}

sub findvalue {
    my ($node, $xpath) = @_;
    my $res;
    $res = $node->find($xpath);
    return $res->to_literal->value;
}

sub findbool {
    my ($node, $xpath) = @_;
    my ($type, @params) = $node->_find($xpath,1);
    if ($type) {
        return $type->new(@params);
    }
    return undef;
}

sub find {
    my ($node, $xpath) = @_;
    my ($type, @params) = $node->_find($xpath,0);
    if ($type) {
        return $type->new(@params);
    }
    return undef;
}

sub setOwnerDocument {
    my ( $self, $doc ) = @_;
    $doc->adoptNode( $self );
}

sub toStringC14N {
    my ($self, $comments, $xpath, $xpc) = @_;
    return $self->_toStringC14N( $comments || 0,
				 (defined $xpath ? $xpath : undef),
				 0,
				 undef,
				 (defined $xpc ? $xpc : undef)
				);
}

{
my $C14N_version_1_dot_1_val = 2;

sub toStringC14N_v1_1 {
    my ($self, $comments, $xpath, $xpc) = @_;

    return $self->_toStringC14N(
        $comments || 0,
        (defined $xpath ? $xpath : undef),
        $C14N_version_1_dot_1_val,
        undef,
        (defined $xpc ? $xpc : undef)
    );
}

}

sub toStringEC14N {
    my ($self, $comments, $xpath, $xpc, $inc_prefix_list) = @_;
    unless (UNIVERSAL::isa($xpc,'XML::LibXML::XPathContext')) {
        if ($inc_prefix_list) {
            croak("toStringEC14N: 3rd argument is not an XML::LibXML::XPathContext");
        } else {
            $inc_prefix_list=$xpc;
            $xpc=undef;
        }
    }
    if (defined($inc_prefix_list) and !UNIVERSAL::isa($inc_prefix_list,'ARRAY')) {
        croak("toStringEC14N: inclusive_prefix_list must be undefined or ARRAY");
    }
    return $self->_toStringC14N( $comments || 0,
        (defined $xpath ? $xpath : undef),
        1,
        (defined $inc_prefix_list ? $inc_prefix_list : undef),
        (defined $xpc ? $xpc : undef)
    );
}

*serialize_c14n = \&toStringC14N;
*serialize_exc_c14n = \&toStringEC14N;

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Document Interface                                         #
#-------------------------------------------------------------------------#
package XML::LibXML::Document;

use vars qw(@ISA);
@ISA = ('XML::LibXML::Node');

sub actualEncoding {
  my $doc = shift;
  my $enc = $doc->encoding;
  return (defined $enc and length $enc) ? $enc : 'UTF-8';
}

sub setDocumentElement {
    my $doc = shift;
    my $element = shift;

    my $oldelem = $doc->documentElement;
    if ( defined $oldelem ) {
        $doc->removeChild($oldelem);
    }

    $doc->_setDocumentElement($element);
}

sub toString {
    my $self = shift;
    my $flag = shift;

    my $retval = "";

    if ( defined $XML::LibXML::skipXMLDeclaration
         and $XML::LibXML::skipXMLDeclaration == 1 ) {
        foreach ( $self->childNodes ){
            next if $_->nodeType == XML::LibXML::XML_DTD_NODE()
                    and $XML::LibXML::skipDTD;
            $retval .= $_->toString;
        }
    }
    else {
        $flag ||= 0 unless defined $flag;
        $retval =  $self->_toString($flag);
    }

    return $retval;
}

sub serialize {
    my $self = shift;
    return $self->toString( @_ );
}

#-------------------------------------------------------------------------#
# bad style xinclude processing                                           #
#-------------------------------------------------------------------------#
sub process_xinclude {
    my $self = shift;
    my $opts = shift;
    XML::LibXML->new->processXIncludes( $self, $opts );
}

sub insertProcessingInstruction {
    my $self   = shift;
    my $target = shift;
    my $data   = shift;

    my $pi     = $self->createPI( $target, $data );
    my $root   = $self->documentElement;

    if ( defined $root ) {
        # this is actually not correct, but i guess it's what the user
        # intends
        $self->insertBefore( $pi, $root );
    }
    else {
        # if no documentElement was found we just append the PI
        $self->appendChild( $pi );
    }
}

sub insertPI {
    my $self = shift;
    $self->insertProcessingInstruction( @_ );
}

#-------------------------------------------------------------------------#
# DOM L3 Document functions.
# added after robins implicit feature request
#-------------------------------------------------------------------------#
*getElementsByTagName = \&XML::LibXML::Element::getElementsByTagName;
*getElementsByTagNameNS = \&XML::LibXML::Element::getElementsByTagNameNS;
*getElementsByLocalName = \&XML::LibXML::Element::getElementsByLocalName;

1;

#-------------------------------------------------------------------------#
# XML::LibXML::DocumentFragment Interface                                 #
#-------------------------------------------------------------------------#
package XML::LibXML::DocumentFragment;

use vars qw(@ISA);
@ISA = ('XML::LibXML::Node');

sub toString {
    my $self = shift;
    my $retval = "";
    if ( $self->hasChildNodes() ) {
        foreach my $n ( $self->childNodes() ) {
            $retval .= $n->toString(@_);
        }
    }
    return $retval;
}

*serialize = \&toString;

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Element Interface                                          #
#-------------------------------------------------------------------------#
package XML::LibXML::Element;

use vars qw(@ISA);
@ISA = ('XML::LibXML::Node');
use XML::LibXML qw(:ns :libxml);
use XML::LibXML::AttributeHash;
use Carp;

use Scalar::Util qw(blessed);

use overload
    '%{}'  => 'getAttributeHash',
    'eq' => '_isSameNodeLax', '==' => '_isSameNodeLax',
    'ne' => '_isNotSameNodeLax', '!=' => '_isNotSameNodeLax',
    fallback => 1,
    ;

sub _isNotSameNodeLax {
    my ($self, $other) = @_;

    return ((not $self->_isSameNodeLax($other)) ? 1 : '');
}

sub _isSameNodeLax {
    my ($self, $other) = @_;

    if (blessed($other) and $other->isa('XML::LibXML::Element'))
    {
        return ($self->isSameNode($other) ? 1 : '');
    }
    else
    {
        return '';
    }
}

{
    my %tiecache;

    sub __destroy_tiecache
    {
        delete $tiecache{ 0+$_[0] };
    }

    sub getAttributeHash
    {
        my $self = shift;
        if (!exists $tiecache{ 0+$self }) {
            tie my %attr, 'XML::LibXML::AttributeHash', $self, weaken => 1;
            $tiecache{ 0+$self } = \%attr;
        }
        return $tiecache{ 0+$self };
    }
    sub DESTROY
    {
        my ($self) = @_;
        $self->__destroy_tiecache;
        $self->SUPER::DESTROY;
    }
}

sub setNamespace {
    my $self = shift;
    my $n = $self->localname;
    if ( $self->_setNamespace(@_) ){
        if ( scalar @_ < 3 || $_[2] == 1 ){
            $self->setNodeName( $n );
        }
        return 1;
    }
    return 0;
}

sub getAttribute {
    my $self = shift;
    my $name = $_[0];
    if ( $name =~ /^xmlns(?::|$)/ ) {
        # user wants to get a namespace ...
        (my $prefix = $name )=~s/^xmlns:?//;
	$self->_getNamespaceDeclURI($prefix);
    }
    else {
        $self->_getAttribute(@_);
    }
}

sub setAttribute {
    my ( $self, $name, $value ) = @_;
    if ( $name =~ /^xmlns(?::|$)/ ) {
      # user wants to set the special attribute for declaring XML namespace ...

      # this is fine but not exactly DOM conformant behavior, btw (according to DOM we should
      # probably declare an attribute which looks like XML namespace declaration
      # but isn't)
      (my $nsprefix = $name )=~s/^xmlns:?//;
      my $nn = $self->nodeName;
      if ( $nn =~ /^\Q${nsprefix}\E:/ ) {
	# the element has the same prefix
	$self->setNamespaceDeclURI($nsprefix,$value) ||
	  $self->setNamespace($value,$nsprefix,1);
        ##
        ## We set the namespace here.
        ## This is helpful, as in:
        ##
        ## |  $e = XML::LibXML::Element->new('foo:bar');
        ## |  $e->setAttribute('xmlns:foo','http://yoyodine')
        ##
      }
      else {
	# just modify the namespace
	$self->setNamespaceDeclURI($nsprefix, $value) ||
	  $self->setNamespace($value,$nsprefix,0);
      }
    }
    else {
        $self->_setAttribute($name, $value);
    }
}

sub getAttributeNS {
    my $self = shift;
    my ($nsURI, $name) = @_;
    croak("invalid attribute name") if !defined($name) or $name eq q{};
    if ( defined($nsURI) and $nsURI eq XML_XMLNS_NS ) {
	$self->_getNamespaceDeclURI($name eq 'xmlns' ? undef : $name);
    }
    else {
        $self->_getAttributeNS(@_);
    }
}

sub setAttributeNS {
  my ($self, $nsURI, $qname, $value)=@_;
  unless (defined $qname and length $qname) {
    croak("bad name");
  }
  if (defined($nsURI) and $nsURI eq XML_XMLNS_NS) {
    if ($qname !~ /^xmlns(?::|$)/) {
      croak("NAMESPACE ERROR: Namespace declarations must have the prefix 'xmlns'");
    }
    $self->setAttribute($qname,$value); # see implementation above
    return;
  }
  if ($qname=~/:/ and not (defined($nsURI) and length($nsURI))) {
    croak("NAMESPACE ERROR: Attribute without a prefix cannot be in a namespace");
  }
  if ($qname=~/^xmlns(?:$|:)/) {
    croak("NAMESPACE ERROR: 'xmlns' prefix and qualified-name are reserved for the namespace ".XML_XMLNS_NS);
  }
  if ($qname=~/^xml:/ and not (defined $nsURI and $nsURI eq XML_XML_NS)) {
    croak("NAMESPACE ERROR: 'xml' prefix is reserved for the namespace ".XML_XML_NS);
  }
  $self->_setAttributeNS( defined $nsURI ? $nsURI : undef, $qname, $value );
}

sub getElementsByTagName {
    my ( $node , $name ) = @_;
    my $xpath = $name eq '*' ? "descendant::*" : "descendant::*[name()='$name']";
    my @nodes = $node->_findnodes($xpath);
    return wantarray ? @nodes : XML::LibXML::NodeList->new_from_ref(\@nodes, 1);
}

sub  getElementsByTagNameNS {
    my ( $node, $nsURI, $name ) = @_;
    my $xpath;
    if ( $name eq '*' ) {
      if ( $nsURI eq '*' ) {
	$xpath = "descendant::*";
      } else {
	$xpath = "descendant::*[namespace-uri()='$nsURI']";
      }
    } elsif ( $nsURI eq '*' ) {
      $xpath = "descendant::*[local-name()='$name']";
    } else {
      $xpath = "descendant::*[local-name()='$name' and namespace-uri()='$nsURI']";
    }
    my @nodes = $node->_findnodes($xpath);
    return wantarray ? @nodes : XML::LibXML::NodeList->new_from_ref(\@nodes, 1);
}

sub getElementsByLocalName {
    my ( $node,$name ) = @_;
    my $xpath;
    if ($name eq '*') {
      $xpath = "descendant::*";
    } else {
      $xpath = "descendant::*[local-name()='$name']";
    }
    my @nodes = $node->_findnodes($xpath);
    return wantarray ? @nodes : XML::LibXML::NodeList->new_from_ref(\@nodes, 1);
}

sub getChildrenByTagName {
    my ( $node, $name ) = @_;
    my @nodes;
    if ($name eq '*') {
      @nodes = grep { $_->nodeType == XML_ELEMENT_NODE() }
	$node->childNodes();
    } else {
      @nodes = grep { $_->nodeName eq $name } $node->childNodes();
    }
    return wantarray ? @nodes : XML::LibXML::NodeList->new_from_ref(\@nodes, 1);
}

sub getChildrenByLocalName {
    my ( $node, $name ) = @_;
    # my @nodes;
    # if ($name eq '*') {
    #   @nodes = grep { $_->nodeType == XML_ELEMENT_NODE() }
    # 	$node->childNodes();
    # } else {
    #   @nodes = grep { $_->nodeType == XML_ELEMENT_NODE() and
    # 		      $_->localName eq $name } $node->childNodes();
    # }
    # return wantarray ? @nodes : XML::LibXML::NodeList->new_from_ref(\@nodes, 1);
    my @nodes = $node->_getChildrenByTagNameNS('*',$name);
    return wantarray ? @nodes : XML::LibXML::NodeList->new_from_ref(\@nodes, 1);
}

sub getChildrenByTagNameNS {
    my ( $node, $nsURI, $name ) = @_;
    my @nodes = $node->_getChildrenByTagNameNS($nsURI,$name);
    return wantarray ? @nodes : XML::LibXML::NodeList->new_from_ref(\@nodes, 1);
}

sub appendWellBalancedChunk {
    my ( $self, $chunk ) = @_;

    my $local_parser = XML::LibXML->new();
    my $frag = $local_parser->parse_xml_chunk( $chunk );

    $self->appendChild( $frag );
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Text Interface                                             #
#-------------------------------------------------------------------------#
package XML::LibXML::Text;

use vars qw(@ISA);
@ISA = ('XML::LibXML::Node');

sub attributes { return; }

sub deleteDataString {
    my ($node, $string, $all) = @_;

    return $node->replaceDataString($string, '', $all);
}

sub replaceDataString {
    my ( $node, $left_proto, $right,$all ) = @_;

    # Assure we exchange the strings and not expressions!
    my $left = quotemeta($left_proto);

    my $datastr = $node->nodeValue();
    if ( $all ) {
        $datastr =~ s/$left/$right/g;
    }
    else{
        $datastr =~ s/$left/$right/;
    }
    $node->setData( $datastr );
}

sub replaceDataRegEx {
    my ( $node, $leftre, $rightre, $flags ) = @_;
    return unless defined $leftre;
    $rightre ||= "";

    my $datastr = $node->nodeValue();
    my $restr   = "s/" . $leftre . "/" . $rightre . "/";
    $restr .= $flags if defined $flags;

    eval '$datastr =~ '. $restr;

    $node->setData( $datastr );
}

1;

package XML::LibXML::Comment;

use vars qw(@ISA);
@ISA = ('XML::LibXML::Text');

1;

package XML::LibXML::CDATASection;

use vars qw(@ISA);
@ISA     = ('XML::LibXML::Text');

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Attribute Interface                                        #
#-------------------------------------------------------------------------#
package XML::LibXML::Attr;
use vars qw( @ISA ) ;
@ISA = ('XML::LibXML::Node') ;

sub setNamespace {
    my ($self,$href,$prefix) = @_;
    my $n = $self->localname;
    if ( $self->_setNamespace($href,$prefix) ) {
        $self->setNodeName($n);
        return 1;
    }

    return 0;
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Dtd Interface                                              #
#-------------------------------------------------------------------------#
# this is still under construction
#
package XML::LibXML::Dtd;
use vars qw( @ISA );
@ISA = ('XML::LibXML::Node');

# at least DESTROY and CLONE_SKIP must be inherited

1;

#-------------------------------------------------------------------------#
# XML::LibXML::PI Interface                                               #
#-------------------------------------------------------------------------#
package XML::LibXML::PI;
use vars qw( @ISA );
@ISA = ('XML::LibXML::Node');

sub setData {
    my $pi = shift;

    my $string = "";
    if ( scalar @_ == 1 ) {
        $string = shift;
    }
    else {
        my %h = @_;
        $string = join " ", map {$_.'="'.$h{$_}.'"'} keys %h;
    }

    # the spec says any char but "?>" [17]
    $pi->_setData( $string ) unless  $string =~ /\?>/;
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Namespace Interface                                        #
#-------------------------------------------------------------------------#
package XML::LibXML::Namespace;

sub CLONE_SKIP { 1 }

# In fact, this is not a node!
sub prefix { return "xmlns"; }
sub getPrefix { return "xmlns"; }
sub getNamespaceURI { return "http://www.w3.org/2000/xmlns/" };

sub getNamespaces { return (); }

sub nodeName {
  my $self = shift;
  my $nsP  = $self->localname;
  return ( defined($nsP) && length($nsP) ) ? "xmlns:$nsP" : "xmlns";
}
sub name    { goto &nodeName }
sub getName { goto &nodeName }

sub isEqualNode {
    my ( $self, $ref ) = @_;
    if ( ref($ref) eq "XML::LibXML::Namespace" ) {
        return $self->_isEqual($ref);
    }
    return 0;
}

sub isSameNode {
    my ( $self, $ref ) = @_;
    if ( $$self == $$ref ){
        return 1;
    }
    return 0;
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::NamedNodeMap Interface                                     #
#-------------------------------------------------------------------------#
package XML::LibXML::NamedNodeMap;

use XML::LibXML qw(:libxml);

sub CLONE_SKIP {
  return $XML::LibXML::__threads_shared ? 0 : 1;
}

sub new {
    my $class = shift;
    my $self = bless { Nodes => [@_] }, $class;
    $self->{NodeMap} = { map { $_->nodeName => $_ } @_ };
    return $self;
}

sub length     { return scalar( @{$_[0]->{Nodes}} ); }
sub nodes      { return $_[0]->{Nodes}; }
sub item       { $_[0]->{Nodes}->[$_[1]]; }

sub getNamedItem {
    my $self = shift;
    my $name = shift;

    return $self->{NodeMap}->{$name};
}

sub setNamedItem {
    my $self = shift;
    my $node = shift;

    my $retval;
    if ( defined $node ) {
        if ( scalar @{$self->{Nodes}} ) {
            my $name = $node->nodeName();
            if ( $node->nodeType() == XML_NAMESPACE_DECL ) {
                return;
            }
            if ( defined $self->{NodeMap}->{$name} ) {
                if ( $node->isSameNode( $self->{NodeMap}->{$name} ) ) {
                    return;
                }
                $retval = $self->{NodeMap}->{$name}->replaceNode( $node );
            }
            else {
                $self->{Nodes}->[0]->addSibling($node);
            }

            $self->{NodeMap}->{$name} = $node;
            push @{$self->{Nodes}}, $node;
        }
        else {
            # not done yet
            # can this be properly be done???
            warn "not done yet\n";
        }
    }
    return $retval;
}

sub removeNamedItem {
    my $self = shift;
    my $name = shift;
    my $retval;
    if ( $name =~ /^xmlns/ ) {
        warn "not done yet\n";
    }
    elsif ( exists $self->{NodeMap}->{$name} ) {
        $retval = $self->{NodeMap}->{$name};
        $retval->unbindNode;
        delete $self->{NodeMap}->{$name};
        $self->{Nodes} = [grep {not($retval->isSameNode($_))} @{$self->{Nodes}}];
    }

    return $retval;
}

sub getNamedItemNS {
    my $self = shift;
    my $nsURI = shift;
    my $name = shift;
    return undef;
}

sub setNamedItemNS {
    my $self = shift;
    my $nsURI = shift;
    my $node = shift;
    return undef;
}

sub removeNamedItemNS {
    my $self = shift;
    my $nsURI = shift;
    my $name = shift;
    return undef;
}

1;

package XML::LibXML::_SAXParser;

# this is pseudo class!!! and it will be removed as soon all functions
# moved to XS level

use XML::SAX::Exception;

sub CLONE_SKIP {
  return $XML::LibXML::__threads_shared ? 0 : 1;
}

# these functions will use SAX exceptions as soon i know how things really work
sub warning {
    my ( $parser, $message, $line, $col ) = @_;
    my $error = XML::SAX::Exception::Parse->new( LineNumber   => $line,
                                                 ColumnNumber => $col,
                                                 Message      => $message, );
    $parser->{HANDLER}->warning( $error );
}

sub error {
    my ( $parser, $message, $line, $col ) = @_;

    my $error = XML::SAX::Exception::Parse->new( LineNumber   => $line,
                                                 ColumnNumber => $col,
                                                 Message      => $message, );
    $parser->{HANDLER}->error( $error );
}

sub fatal_error {
    my ( $parser, $message, $line, $col ) = @_;
    my $error = XML::SAX::Exception::Parse->new( LineNumber   => $line,
                                                 ColumnNumber => $col,
                                                 Message      => $message, );
    $parser->{HANDLER}->fatal_error( $error );
}

1;

package XML::LibXML::RelaxNG;

sub CLONE_SKIP { 1 }

sub new {
    my $class = shift;
    my %args = @_;

    my $self = undef;
    if ( defined $args{location} ) {
        $self = $class->parse_location( $args{location} );
    }
    elsif ( defined $args{string} ) {
        $self = $class->parse_buffer( $args{string} );
    }
    elsif ( defined $args{DOM} ) {
        $self = $class->parse_document( $args{DOM} );
    }

    return $self;
}

1;

package XML::LibXML::Schema;

sub CLONE_SKIP { 1 }

sub new {
    my $class = shift;
    my %args = @_;

    my $self = undef;
    if ( defined $args{location} ) {
        $self = $class->parse_location( $args{location} );
    }
    elsif ( defined $args{string} ) {
        $self = $class->parse_buffer( $args{string} );
    }

    return $self;
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Pattern Interface                                          #
#-------------------------------------------------------------------------#

package XML::LibXML::Pattern;

sub CLONE_SKIP { 1 }

sub new {
  my $class = shift;
  my ($pattern,$ns_map)=@_;
  my $self = undef;

  unless (UNIVERSAL::can($class,'_compilePattern')) {
    croak("Cannot create XML::LibXML::Pattern - ".
	  "your libxml2 is compiled without pattern support!");
  }

  if (ref($ns_map) eq 'HASH') {
    # translate prefix=>URL hash to a (URL,prefix) list
    $self = $class->_compilePattern($pattern,0,[reverse %$ns_map]);
  } else {
    $self = $class->_compilePattern($pattern,0);
  }
  return $self;
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::RegExp Interface                                          #
#-------------------------------------------------------------------------#

package XML::LibXML::RegExp;

sub CLONE_SKIP { 1 }

sub new {
  my $class = shift;
  my ($regexp)=@_;
  unless (UNIVERSAL::can($class,'_compile')) {
    croak("Cannot create XML::LibXML::RegExp - ".
	  "your libxml2 is compiled without regexp support!");
  }
  return $class->_compile($regexp);
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::XPathExpression Interface                                  #
#-------------------------------------------------------------------------#

package XML::LibXML::XPathExpression;

sub CLONE_SKIP { 1 }

1;


#-------------------------------------------------------------------------#
# XML::LibXML::InputCallback Interface                                    #
#-------------------------------------------------------------------------#
package XML::LibXML::InputCallback;

use vars qw($_CUR_CB @_GLOBAL_CALLBACKS @_CB_STACK $_CB_NESTED_DEPTH @_CB_NESTED_STACK);

BEGIN {
  $_CUR_CB = undef;
  @_GLOBAL_CALLBACKS = ();
  @_CB_STACK = ();
  $_CB_NESTED_DEPTH = 0;
  @_CB_NESTED_STACK = ();
}

sub CLONE_SKIP {
  return $XML::LibXML::__threads_shared ? 0 : 1;
}

#-------------------------------------------------------------------------#
# global callbacks                                                        #
#-------------------------------------------------------------------------#
sub _callback_match {
    my $uri = shift;
    my $retval = 0;

    # loop through the callbacks, and find the first matching one.
    # The callbacks are stored in execution order (reverse stack order).
    # Any new global callbacks are shifted to the callback stack.
    foreach my $cb ( @_GLOBAL_CALLBACKS ) {

        # callbacks have to return 1, 0 or undef, while 0 and undef
        # are handled the same way.
        # in fact, if callbacks return other values, the global match
        # assumes silently that the callback failed.

        $retval = $cb->[0]->($uri);

        if ( defined $retval and $retval == 1 ) {
            # make the other callbacks use this callback
            $_CUR_CB = $cb;
            unshift @_CB_STACK, $cb;
            last;
        }
    }

    return $retval;
}

sub _callback_open {
    my $uri = shift;
    my $retval = undef;

    # the open callback has to return a defined value.
    # if one works on files this can be a file handle. But
    # depending on the needs of the callback it also can be a
    # database handle or a integer labeling a certain dataset.

    if ( defined $_CUR_CB ) {
        $retval = $_CUR_CB->[1]->( $uri );

        # reset the callbacks, if one callback cannot open an uri
        if ( not defined $retval or $retval == 0 ) {
            shift @_CB_STACK;
            $_CUR_CB = $_CB_STACK[0];
        }
    }

    return $retval;
}

sub _callback_read {
    my $fh = shift;
    my $buflen = shift;

    my $retval = undef;

    if ( defined $_CUR_CB ) {
        $retval = $_CUR_CB->[2]->( $fh, $buflen );
    }

    return $retval;
}

sub _callback_close {
    my $fh = shift;
    my $retval = 0;

    if ( defined $_CUR_CB ) {
        $retval = $_CUR_CB->[3]->( $fh );
        shift @_CB_STACK;
        $_CUR_CB = $_CB_STACK[0];
    }

    return $retval;
}

#-------------------------------------------------------------------------#
# member functions and methods                                            #
#-------------------------------------------------------------------------#

sub new {
    my $CLASS = shift;
    return bless {'_CALLBACKS' => []}, $CLASS;
}

# add a callback set to the callback stack
# synopsis: $icb->register_callbacks( [$match_cb, $open_cb, $read_cb, $close_cb] );
sub register_callbacks {
    my $self = shift;
    my $cbset = shift;

    # test if callback set is complete
    if ( ref $cbset eq "ARRAY" and scalar( @$cbset ) == 4 ) {
        unshift @{$self->{_CALLBACKS}}, $cbset;
    }
}

# remove a callback set to the callback stack
# if a callback set is passed, this function will check for the match function
sub unregister_callbacks {
    my $self = shift;
    my $cbset = shift;
    if ( ref $cbset eq "ARRAY" and scalar( @$cbset ) == 4 ) {
        $self->{_CALLBACKS} = [grep { $_->[0] != $cbset->[0] } @{$self->{_CALLBACKS}}];
    }
    else {
        shift @{$self->{_CALLBACKS}};
    }
}

# make libxml2 use the callbacks
sub init_callbacks {
    my $self = shift;
    my $parser = shift;

    #initialize the libxml2 callbacks unless this is a nested callback
    $self->lib_init_callbacks() unless($_CB_NESTED_DEPTH);

    #store the callbacks for any outer executing parser instance
    $_CB_NESTED_DEPTH++;
    push @_CB_NESTED_STACK, [
      $_CUR_CB,
      [@_CB_STACK],
      [@_GLOBAL_CALLBACKS],
    ];

    #initialize the callback variables for the current parser
    $_CUR_CB           = undef;
    @_CB_STACK         = ();
    @_GLOBAL_CALLBACKS = @{ $self->{_CALLBACKS} };

    #attach parser specific callbacks
    if($parser) {
        my $mcb = $parser->match_callback();
        my $ocb = $parser->open_callback();
        my $rcb = $parser->read_callback();
        my $ccb = $parser->close_callback();
        if ( defined $mcb and defined $ocb and defined $rcb and defined $ccb ) {
            unshift @_GLOBAL_CALLBACKS, [$mcb, $ocb, $rcb, $ccb];
        }
    }

    #attach global callbacks
    if ( defined $XML::LibXML::match_cb and
         defined $XML::LibXML::open_cb  and
         defined $XML::LibXML::read_cb  and
         defined $XML::LibXML::close_cb ) {
        push @_GLOBAL_CALLBACKS, [$XML::LibXML::match_cb,
                                  $XML::LibXML::open_cb,
                                  $XML::LibXML::read_cb,
                                  $XML::LibXML::close_cb];
    }
}

# reset libxml2's callbacks
sub cleanup_callbacks {
    my $self = shift;

    #restore the callbacks for the outer parser instance
    $_CB_NESTED_DEPTH--;
    my $saved          = pop @_CB_NESTED_STACK;
    $_CUR_CB           = $saved->[0];
    @_CB_STACK         = (@{$saved->[1]});
    @_GLOBAL_CALLBACKS = (@{$saved->[2]});

    #clean up the libxml2 callbacks unless there are still outer parsing instances
    $self->lib_cleanup_callbacks() unless($_CB_NESTED_DEPTH);
}

$XML::LibXML::__loaded=1;

1;

__END__
