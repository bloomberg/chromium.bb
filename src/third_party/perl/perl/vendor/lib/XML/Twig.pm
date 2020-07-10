use strict;
use warnings; # > perl 5.5

# This is created in the caller's space
# I realize (now!) that it's not clean, but it's been there for 10+ years...
BEGIN
{ sub ::PCDATA { '#PCDATA' }  ## no critic (Subroutines::ProhibitNestedSubs);
  sub ::CDATA  { '#CDATA'  }  ## no critic (Subroutines::ProhibitNestedSubs);
}

use UNIVERSAL();

## if a sub returns a scalar, it better not bloody disappear in list context
## no critic (Subroutines::ProhibitExplicitReturnUndef);

my $perl_version;
my $parser_version;

######################################################################
package XML::Twig;
######################################################################

require 5.004;

use utf8; # > perl 5.5

use vars qw($VERSION @ISA %valid_option);

use Carp;
use File::Spec;
use File::Basename;

use Config; # to get perl's path name in case we need to know if perlio is available

*isa= *UNIVERSAL::isa;

# flag, set to true if the weaken sub is available
use vars qw( $weakrefs);

# flag set to true if the version of expat seems to be 1.95.2, which has annoying bugs
# wrt doctype handling. This is global for performance reasons. 
my $expat_1_95_2=0;

# a slight non-xml mod: # is allowed as a first character
my $REG_TAG_FIRST_LETTER;
#$REG_TAG_FIRST_LETTER= q{(?:[^\W\d]|[:#_])};  # < perl 5.6 - does not work for leading non-ascii letters
$REG_TAG_FIRST_LETTER= q{(?:[[:alpha:]:#_])}; # >= perl 5.6

my $REG_TAG_LETTER= q{(?:[\w_.-]*)};

# a simple name (no colon)
my $REG_NAME_TOKEN= qq{(?:$REG_TAG_FIRST_LETTER$REG_TAG_LETTER*)};

# a tag name, possibly including namespace
my $REG_NAME= qq{(?:(?:$REG_NAME_TOKEN:)?$REG_NAME_TOKEN)};

# tag name (leading # allowed)
# first line is for perl 5.005, second line for modern perl, that accept character classes
my $REG_TAG_NAME=$REG_NAME;

# name or wildcard (* or '') (leading # allowed)
my $REG_NAME_W = qq{(?:$REG_NAME|[*])}; 

# class and ids are deliberately permissive
my $REG_NTOKEN_FIRST_LETTER;
#$REG_NTOKEN_FIRST_LETTER= q{(?:[^\W\d]|[:_])};  # < perl 5.6 - does not work for leading non-ascii letters
$REG_NTOKEN_FIRST_LETTER= q{(?:[[:alpha:]:_])}; # >= perl 5.6

my $REG_NTOKEN_LETTER= q{(?:[\w_:.-]*)};

my $REG_NTOKEN= qq{(?:$REG_NTOKEN_FIRST_LETTER$REG_NTOKEN_LETTER*)};
my $REG_CLASS = $REG_NTOKEN;
my $REG_ID    = $REG_NTOKEN;

# allow <tag> #<tag> (private elt) * <tag>.<class> *.<class> <tag>#<id> *#<id>
my $REG_TAG_PART= qq{(?:$REG_NAME_W(?:[.]$REG_CLASS|[#]$REG_ID)?|[.]$REG_CLASS)};

my $REG_REGEXP     = q{(?:/(?:[^\\/]|\\.)*/[eimsox]*)};               # regexp
my $REG_MATCH      = q{[!=]~};                                        # match (or not)
my $REG_STRING     = q{(?:"(?:[^\\"]|\\.)*"|'(?:[^\\']|\\.)*')};      # string (simple or double quoted)
my $REG_NUMBER     = q{(?:\d+(?:\.\d*)?|\.\d+)};                      # number
my $REG_VALUE      = qq{(?:$REG_STRING|$REG_NUMBER)};                 # value
my $REG_OP         = q{==|!=|>|<|>=|<=|eq|ne|lt|gt|le|ge|=};          # op
my $REG_FUNCTION   = q{(?:string|text)\(\s*\)};
my $REG_STRING_ARG = qq{(?:string|text)\\(\\s*$REG_NAME_W\\s*\\)};
my $REG_COMP       = q{(?:>=|<=|!=|<|>|=)};

my $REG_TAG_IN_PREDICATE= $REG_NAME_W . q{(?=\s*(?i:and\b|or\b|\]|$))};

# keys in the context stack, chosen not to interfere with att names, even private (#-prefixed) ones
my $ST_TAG = '##tag';
my $ST_ELT = '##elt';
my $ST_NS  = '##ns' ;

# used in the handler trigger code
my $REG_NAKED_PREDICATE= qq{((?:"[^"]*"|'[^']*'|$REG_STRING_ARG|$REG_FUNCTION|\@$REG_NAME_W|$REG_MATCH\\s*$REG_REGEXP|[\\s\\d><=!()+.-]|(?i:and)|(?i:or)|$REG_TAG_IN_PREDICATE)*)};
my $REG_PREDICATE= qq{\\[$REG_NAKED_PREDICATE\\]};

# not all axis, only supported ones (in get_xpath)
my @supported_axis= ( 'ancestor', 'ancestor-or-self', 'child', 'descendant', 'descendant-or-self', 
                      'following', 'following-sibling', 'parent', 'preceding', 'preceding-sibling', 'self'
                    );
my $REG_AXIS       = "(?:" . join( '|', @supported_axis) .")";

# only used in the "xpath"engine (for get_xpath/findnodes) for now
my $REG_PREDICATE_ALT  = qr{\[(?:(?:string\(\s*\)|\@$REG_TAG_NAME)\s*$REG_MATCH\s*$REG_REGEXP\s*|[^\]]*)\]};

# used to convert XPath tests on strings to the perl equivalent 
my %PERL_ALPHA_TEST= ( '=' => ' eq ', '!=' => ' ne ', '>' => ' gt ', '>=' => ' ge ', '<' => ' lt ', '<=' => ' le ');

my( $FB_HTMLCREF, $FB_XMLCREF);

my $NO_WARNINGS= $perl_version >= 5.006 ? 'no warnings' : 'local $^W=0';

# default namespaces, both ways
my %DEFAULT_NS= ( xml   => "http://www.w3.org/XML/1998/namespace",
                  xmlns => "http://www.w3.org/2000/xmlns/",
                );
my %DEFAULT_URI2NS= map { $DEFAULT_NS{$_} => $_ } keys %DEFAULT_NS;

# constants
my( $PCDATA, $CDATA, $PI, $COMMENT, $ENT, $ELT, $NOTATION, $TEXT, $ASIS, $EMPTY, $BUFSIZE);

# used when an HTML doc only has a PUBLIC declaration, to generate the SYSTEM one
# this should really be done by HTML::TreeBuilder, but as of HTML::TreeBuilder 4.2 it isn't
# the various declarations are taken from http://en.wikipedia.org/wiki/Document_Type_Declaration
my %HTML_DECL= ( "-//W3C//DTD HTML 4.0 Transitional//EN"  => "http://www.w3.org/TR/REC-html40/loose.dtd",
                 "-//W3C//DTD HTML 4.01//EN"              => "http://www.w3.org/TR/html4/strict.dtd",
                 "-//W3C//DTD HTML 4.01 Transitional//EN" => "http://www.w3.org/TR/html4/loose.dtd",
                 "-//W3C//DTD HTML 4.01 Frameset//EN"     => "http://www.w3.org/TR/html4/frameset.dtd",
                 "-//W3C//DTD XHTML 1.0 Strict//EN"       => "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd",
                 "-//W3C//DTD XHTML 1.0 Transitional//EN" => "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd",
                 "-//W3C//DTD XHTML 1.0 Frameset//EN"     => "http://www.w3.org/TR/xhtml1/DTD/xhtml1-frameset.dtd",
                 "-//W3C//DTD XHTML 1.1//EN"              => "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd",
                 "-//W3C//DTD XHTML Basic 1.0//EN"        => "http://www.w3.org/TR/xhtml-basic/xhtml-basic10.dtd",
                 "-//W3C//DTD XHTML Basic 1.1//EN"        => "http://www.w3.org/TR/xhtml-basic/xhtml-basic11.dtd",
                 "-//WAPFORUM//DTD XHTML Mobile 1.0//EN"  => "http://www.wapforum.org/DTD/xhtml-mobile10.dtd",
                 "-//WAPFORUM//DTD XHTML Mobile 1.1//EN"  => "http://www.openmobilealliance.org/tech/DTD/xhtml-mobile11.dtd",
                 "-//WAPFORUM//DTD XHTML Mobile 1.2//EN"  => "http://www.openmobilealliance.org/tech/DTD/xhtml-mobile12.dtd",
                 "-//W3C//DTD XHTML+RDFa 1.0//EN"         => "http://www.w3.org/MarkUp/DTD/xhtml-rdfa-1.dtd",
               );

my $DEFAULT_HTML_TYPE= "-//W3C//DTD HTML 4.0 Transitional//EN";

my $SEP= qr/\s*(?:$|\|)/;

BEGIN
{ 
$VERSION = '3.52';

use XML::Parser;
my $needVersion = '2.23';
($parser_version= $XML::Parser::VERSION)=~ s{_\d+}{}; # remove _<n> from version so numeric tests do not warn
croak "need at least XML::Parser version $needVersion" unless $parser_version >= $needVersion;

($perl_version= $])=~ s{_\d+}{};

if( $perl_version >= 5.008) 
  { eval "use Encode qw( :all)"; ## no critic ProhibitStringyEval
    $FB_XMLCREF  = 0x0400; # Encode::FB_XMLCREF;
    $FB_HTMLCREF = 0x0200; # Encode::FB_HTMLCREF;
  }

# test whether we can use weak references
# set local empty signal handler to trap error messages
{ local $SIG{__DIE__};
  if( eval( 'require Scalar::Util') && defined( \&Scalar::Util::weaken)) 
    { import Scalar::Util( 'weaken'); $weakrefs= 1; }
  elsif( eval( 'require WeakRef')) 
    { import WeakRef; $weakrefs= 1;                 }
  else  
    { $weakrefs= 0;                                 } 
}

import XML::Twig::Elt;
import XML::Twig::Entity;
import XML::Twig::Entity_list;

# used to store the gi's
# should be set for each twig really, at least when there are several
# the init ensures that special gi's are always the same

# constants: element types
$PCDATA    = '#PCDATA';
$CDATA     = '#CDATA';
$PI        = '#PI';
$COMMENT   = '#COMMENT';
$ENT       = '#ENT';
$NOTATION  = '#NOTATION';

# element classes
$ELT     = '#ELT';
$TEXT    = '#TEXT';

# element properties
$ASIS    = '#ASIS';
$EMPTY   = '#EMPTY';

# used in parseurl to set the buffer size to the same size as in XML::Parser::Expat
$BUFSIZE = 32768;


# gi => index
%XML::Twig::gi2index=( '', 0, $PCDATA => 1, $CDATA => 2, $PI => 3, $COMMENT => 4, $ENT => 5); 
# list of gi's
@XML::Twig::index2gi=( '', $PCDATA, $CDATA, $PI, $COMMENT, $ENT);

# gi's under this value are special 
$XML::Twig::SPECIAL_GI= @XML::Twig::index2gi;

%XML::Twig::base_ent= ( '>' => '&gt;', '<' => '&lt;', '&' => '&amp;', "'" => '&apos;', '"' => '&quot;',);
foreach my $c ( "\n", "\r", "\t") { $XML::Twig::base_ent{$c}= sprintf( "&#x%02x;", ord( $c)); }

# now set some aliases
*find_nodes           = *get_xpath;               # same as XML::XPath
*findnodes            = *get_xpath;               # same as XML::LibXML
*getElementsByTagName = *descendants;
*descendants_or_self  = *descendants;             # valid in XML::Twig, not in XML::Twig::Elt
*find_by_tag_name     = *descendants;
*getElementById       = *elt_id;
*getEltById           = *elt_id;
*toString             = *sprint;
*create_accessors     = *att_accessors;

}

@ISA = qw(XML::Parser);

# fake gi's used in twig_handlers and start_tag_handlers
my $ALL    = '_all_';     # the associated function is always called
my $DEFAULT= '_default_'; # the function is called if no other handler has been

# some defaults
my $COMMENTS_DEFAULT= 'keep';
my $PI_DEFAULT      = 'keep';


# handlers used in regular mode
my %twig_handlers=( Start      => \&_twig_start, 
                    End        => \&_twig_end, 
                    Char       => \&_twig_char, 
                    Entity     => \&_twig_entity, 
                    Notation   => \&_twig_notation, 
                    XMLDecl    => \&_twig_xmldecl, 
                    Doctype    => \&_twig_doctype, 
                    Element    => \&_twig_element, 
                    Attlist    => \&_twig_attlist, 
                    CdataStart => \&_twig_cdatastart, 
                    CdataEnd   => \&_twig_cdataend, 
                    Proc       => \&_twig_pi,
                    Comment    => \&_twig_comment,
                    Default    => \&_twig_default,
                    ExternEnt  => \&_twig_extern_ent,
      );

# handlers used when twig_roots is used and we are outside of the roots
my %twig_handlers_roots=
  ( Start      => \&_twig_start_check_roots, 
    End        => \&_twig_end_check_roots, 
    Doctype    => \&_twig_doctype, 
    Char       => undef, Entity     => undef, XMLDecl    => \&_twig_xmldecl, 
    Element    => undef, Attlist    => undef, CdataStart => undef, 
    CdataEnd   => undef, Proc       => undef, Comment    => undef, 
    Proc       => \&_twig_pi_check_roots,
    Default    =>  sub {}, # hack needed for XML::Parser 2.27
    ExternEnt  => \&_twig_extern_ent,
  );

# handlers used when twig_roots and print_outside_roots are used and we are
# outside of the roots
my %twig_handlers_roots_print_2_30=
  ( Start      => \&_twig_start_check_roots, 
    End        => \&_twig_end_check_roots, 
    Char       => \&_twig_print, 
    Entity     => \&_twig_print_entity, 
    ExternEnt  => \&_twig_print_entity,
    DoctypeFin => \&_twig_doctype_fin_print,
    XMLDecl    => sub { _twig_xmldecl( @_); _twig_print( @_); },
    Doctype   =>  \&_twig_print_doctype, # because recognized_string is broken here
    # Element    => \&_twig_print, Attlist    => \&_twig_print, 
    CdataStart => \&_twig_print, CdataEnd   => \&_twig_print, 
    Proc       => \&_twig_pi_check_roots, Comment    => \&_twig_print, 
    Default    => \&_twig_print_check_doctype,
    ExternEnt  => \&_twig_extern_ent,
  );

# handlers used when twig_roots, print_outside_roots and keep_encoding are used
# and we are outside of the roots
my %twig_handlers_roots_print_original_2_30=
  ( Start      => \&_twig_start_check_roots, 
    End        => \&_twig_end_check_roots, 
    Char       => \&_twig_print_original, 
    # I have no idea why I should not be using this handler!
    Entity     => \&_twig_print_entity, 
    ExternEnt  => \&_twig_print_entity,
    DoctypeFin => \&_twig_doctype_fin_print,
    XMLDecl    => sub { _twig_xmldecl( @_); _twig_print_original( @_) }, 
    Doctype    => \&_twig_print_original_doctype,  # because original_string is broken here
    Element    => \&_twig_print_original, Attlist   => \&_twig_print_original,
    CdataStart => \&_twig_print_original, CdataEnd  => \&_twig_print_original,
    Proc       => \&_twig_pi_check_roots, Comment   => \&_twig_print_original,
    Default    => \&_twig_print_original_check_doctype, 
  );

# handlers used when twig_roots and print_outside_roots are used and we are
# outside of the roots
my %twig_handlers_roots_print_2_27=
  ( Start      => \&_twig_start_check_roots, 
    End        => \&_twig_end_check_roots, 
    Char       => \&_twig_print, 
    # if the Entity handler is set then it prints the entity declaration
    # before the entire internal subset (including the declaration!) is output
    Entity     => sub {},
    XMLDecl    => \&_twig_print, Doctype    => \&_twig_print, 
    CdataStart => \&_twig_print, CdataEnd   => \&_twig_print, 
    Proc       => \&_twig_pi_check_roots, Comment    => \&_twig_print, 
    Default    => \&_twig_print, 
    ExternEnt  => \&_twig_extern_ent,
  );

# handlers used when twig_roots, print_outside_roots and keep_encoding are used
# and we are outside of the roots
my %twig_handlers_roots_print_original_2_27=
  ( Start      => \&_twig_start_check_roots, 
    End        => \&_twig_end_check_roots, 
    Char       => \&_twig_print_original, 
    # for some reason original_string is wrong here 
    # this can be a problem if the doctype includes non ascii characters
    XMLDecl    => \&_twig_print, Doctype    => \&_twig_print,
    # if the Entity handler is set then it prints the entity declaration
    # before the entire internal subset (including the declaration!) is output
    Entity     => sub {}, 
    #Element    => undef, Attlist   => undef,
    CdataStart => \&_twig_print_original, CdataEnd  => \&_twig_print_original,
    Proc       => \&_twig_pi_check_roots, Comment   => \&_twig_print_original,
    Default    => \&_twig_print, #  _twig_print_original does not work
    ExternEnt  => \&_twig_extern_ent,
  );


my %twig_handlers_roots_print= $parser_version > 2.27 
                               ? %twig_handlers_roots_print_2_30 
                               : %twig_handlers_roots_print_2_27; 
my %twig_handlers_roots_print_original= $parser_version > 2.27 
                               ? %twig_handlers_roots_print_original_2_30 
                               : %twig_handlers_roots_print_original_2_27; 


# handlers used when the finish_print method has been called
my %twig_handlers_finish_print=
  ( Start      => \&_twig_print, 
    End        => \&_twig_print, Char       => \&_twig_print, 
    Entity     => \&_twig_print, XMLDecl    => \&_twig_print, 
    Doctype    => \&_twig_print, Element    => \&_twig_print, 
    Attlist    => \&_twig_print, CdataStart => \&_twig_print, 
    CdataEnd   => \&_twig_print, Proc       => \&_twig_print, 
    Comment    => \&_twig_print, Default    => \&_twig_print, 
    ExternEnt  => \&_twig_extern_ent,
  );

# handlers used when the finish_print method has been called and the keep_encoding
# option is used
my %twig_handlers_finish_print_original=
  ( Start      => \&_twig_print_original, End      => \&_twig_print_end_original, 
    Char       => \&_twig_print_original, Entity   => \&_twig_print_original, 
    XMLDecl    => \&_twig_print_original, Doctype  => \&_twig_print_original, 
    Element    => \&_twig_print_original, Attlist  => \&_twig_print_original, 
    CdataStart => \&_twig_print_original, CdataEnd => \&_twig_print_original, 
    Proc       => \&_twig_print_original, Comment  => \&_twig_print_original, 
    Default    => \&_twig_print_original, 
  );

# handlers used within ignored elements
my %twig_handlers_ignore=
  ( Start      => \&_twig_ignore_start, 
    End        => \&_twig_ignore_end, 
    Char       => undef, Entity     => undef, XMLDecl    => undef, 
    Doctype    => undef, Element    => undef, Attlist    => undef, 
    CdataStart => undef, CdataEnd   => undef, Proc       => undef, 
    Comment    => undef, Default    => undef,
    ExternEnt  => undef,
  );


# those handlers are only used if the entities are NOT to be expanded
my %twig_noexpand_handlers= ( ExternEnt => undef, Default => \&_twig_default );

my @saved_default_handler;

my $ID= 'id';  # default value, set by the Id argument
my $css_sel=0; # set through the css_sel option to allow .class selectors in triggers 

# all allowed options
%valid_option=
    ( # XML::Twig options
      TwigHandlers          => 1, Id                    => 1,
      TwigRoots             => 1, TwigPrintOutsideRoots => 1,
      StartTagHandlers      => 1, EndTagHandlers        => 1,
      ForceEndTagHandlersUsage => 1,
      DoNotChainHandlers    => 1,
      IgnoreElts            => 1,
      Index                 => 1, 
      AttAccessors          => 1,
      EltAccessors          => 1,
      FieldAccessors        => 1,
      CharHandler           => 1, 
      TopDownHandlers       => 1,
      KeepEncoding          => 1, DoNotEscapeAmpInAtts  => 1,
      ParseStartTag         => 1, KeepAttsOrder         => 1,
      LoadDTD               => 1, DTDHandler            => 1, DTDBase => 1, NoXxe => 1,
      DoNotOutputDTD        => 1, NoProlog              => 1,
      ExpandExternalEnts    => 1,
      DiscardSpaces         => 1, KeepSpaces            => 1, DiscardAllSpaces => 1,
      DiscardSpacesIn       => 1, KeepSpacesIn          => 1, 
      PrettyPrint           => 1, EmptyTags             => 1, 
      EscapeGt              => 1,
      Quote                 => 1,
      Comments              => 1, Pi                    => 1, 
      OutputFilter          => 1, InputFilter           => 1,
      OutputTextFilter      => 1, 
      OutputEncoding        => 1, 
      RemoveCdata           => 1,
      EltClass              => 1,
      MapXmlns              => 1, KeepOriginalPrefix    => 1,
      SkipMissingEnts       => 1,
      # XML::Parser options
      ErrorContext          => 1, ProtocolEncoding      => 1,
      Namespaces            => 1, NoExpand              => 1,
      Stream_Delimiter      => 1, ParseParamEnt         => 1,
      NoLWP                 => 1, Non_Expat_Options     => 1,
      Xmlns                 => 1, CssSel                => 1,
      UseTidy               => 1, TidyOptions           => 1,
      OutputHtmlDoctype     => 1,
    );

my $active_twig; # last active twig,for XML::Twig::s

# predefined input and output filters
use vars qw( %filter);
%filter= ( html       => \&html_encode,
           safe       => \&safe_encode,
           safe_hex   => \&safe_encode_hex,
         );


# trigger types (used to sort them)
my ($LEVEL_TRIGGER, $REGEXP_TRIGGER, $XPATH_TRIGGER)=(1..3);

sub new
  { my ($class, %args) = @_;
    my $handlers;

    # change all nice_perlish_names into nicePerlishNames
    %args= _normalize_args( %args);

    # check options
    unless( $args{MoreOptions})
      { foreach my $arg (keys %args)
        { carp "invalid option $arg" unless $valid_option{$arg}; }
      }
     
    # a twig is really an XML::Parser
    # my $self= XML::Parser->new(%args);
    my $self;
    $self= XML::Parser->new(%args);   

    bless $self, $class;

    $self->{_twig_context_stack}= [];

    # allow tag.class selectors in handler triggers
    $css_sel= $args{CssSel} || 0; 


    if( exists $args{TwigHandlers})
      { $handlers= $args{TwigHandlers};
        $self->setTwigHandlers( $handlers);
        delete $args{TwigHandlers};
      }

    # take care of twig-specific arguments
    if( exists $args{StartTagHandlers})
      { $self->setStartTagHandlers( $args{StartTagHandlers});
        delete $args{StartTagHandlers};
      }

    if( exists $args{DoNotChainHandlers})
      { $self->{twig_do_not_chain_handlers}=  $args{DoNotChainHandlers}; }

    if( exists $args{IgnoreElts})
      { # change array to hash so you can write ignore_elts => [ qw(foo bar baz)]
        if( isa( $args{IgnoreElts}, 'ARRAY')) { $args{IgnoreElts}= { map { $_ => 1 } @{$args{IgnoreElts}} }; }
        $self->setIgnoreEltsHandlers( $args{IgnoreElts});
        delete $args{IgnoreElts};
      }

    if( exists $args{Index})
      { my $index= $args{Index};
        # we really want a hash name => path, we turn an array into a hash if necessary
        if( ref( $index) eq 'ARRAY')
          { my %index= map { $_ => $_ } @$index;
            $index= \%index;
          }
        while( my( $name, $exp)= each %$index)
          { $self->setTwigHandler( $exp, sub { push @{$_[0]->{_twig_index}->{$name}}, $_; 1; }); }
      }

    $self->{twig_elt_class}= $args{EltClass} || 'XML::Twig::Elt';
    if( defined( $args{EltClass}) && $args{EltClass} ne 'XML::Twig::Elt') { $self->{twig_alt_elt_class}=1; }
    if( exists( $args{EltClass})) { delete $args{EltClass}; }

    if( exists( $args{MapXmlns}))
      { $self->{twig_map_xmlns}=  $args{MapXmlns};
        $self->{Namespaces}=1;
        delete $args{MapXmlns};
      }

    if( exists( $args{KeepOriginalPrefix}))
      { $self->{twig_keep_original_prefix}= $args{KeepOriginalPrefix};
        delete $args{KeepOriginalPrefix};
      }

    $self->{twig_dtd_handler}= $args{DTDHandler};
    delete $args{DTDHandler};

    if( $args{ExpandExternalEnts})
      { $self->set_expand_external_entities( 1);
        $self->{twig_expand_external_ents}= $args{ExpandExternalEnts}; 
        $self->{twig_read_external_dtd}= 1; # implied by ExpandExternalEnts
        if( $args{ExpandExternalEnts} == -1) 
          { $self->{twig_extern_ent_nofail}= 1;
            $self->setHandlers( ExternEnt => \&_twig_extern_ent_nofail);
          }
        delete $args{LoadDTD};
        delete $args{ExpandExternalEnts};
      }
    else
      { $self->set_expand_external_entities( 0); }

    if( !$args{NoLWP} && ! _use( 'URI') && ! _use( 'URI::File') && ! _use( 'LWP'))
      { $self->{twig_ext_ent_handler}= \&XML::Parser::initial_ext_ent_handler }
    elsif( $args{NoXxe})
      { $self->{twig_ext_ent_handler}= 
          sub { my($xp, $base, $path) = @_; $xp->{ErrorMessage}.= "cannot use entities in document when the no_xxe option is on"; return undef; }; 
      }
    else
      { $self->{twig_ext_ent_handler}= \&XML::Parser::file_ext_ent_handler }

    if( $args{DoNotEscapeAmpInAtts})
      { $self->set_do_not_escape_amp_in_atts( 1); 
        $self->{twig_do_not_escape_amp_in_atts}=1;
      }
    else
      { $self->set_do_not_escape_amp_in_atts( 0); 
        $self->{twig_do_not_escape_amp_in_atts}=0;
      }

    # deal with TwigRoots argument, a hash of elements for which
    # subtrees will be built (and associated handlers)
     
    if( $args{TwigRoots})
      { $self->setTwigRoots( $args{TwigRoots});
        delete $args{TwigRoots}; 
      }
    
    if( $args{EndTagHandlers})
      { unless ($self->{twig_roots} || $args{ForceEndTagHandlersUsage})
          { croak "you should not use EndTagHandlers without TwigRoots\n",
                  "if you want to use it anyway, normally because you have ",
                  "a start_tag_handlers that calls 'ignore' and you want to ",
                  "call an ent_tag_handlers at the end of the element, then ",
                  "pass 'force_end_tag_handlers_usage => 1' as an argument ",
                  "to new";
          }
                  
        $self->setEndTagHandlers( $args{EndTagHandlers});
        delete $args{EndTagHandlers};
      }
      
    if( $args{TwigPrintOutsideRoots})
      { croak "cannot use twig_print_outside_roots without twig_roots"
          unless( $self->{twig_roots});
        # if the arg is a filehandle then store it
        if( _is_fh( $args{TwigPrintOutsideRoots}) )
          { $self->{twig_output_fh}= $args{TwigPrintOutsideRoots}; }
        $self->{twig_default_print}= $args{TwigPrintOutsideRoots};
      }

    # space policy
    if( $args{KeepSpaces})
      { croak "cannot use both keep_spaces and discard_spaces"        if( $args{DiscardSpaces});
        croak "cannot use both keep_spaces and discard_all_spaces"    if( $args{DiscardAllSpaces});
        croak "cannot use both keep_spaces and keep_spaces_in"        if( $args{KeepSpacesIn});
        $self->{twig_keep_spaces}=1;
        delete $args{KeepSpaces}; 
      }
    if( $args{DiscardSpaces})
      { 
        croak "cannot use both discard_spaces and keep_spaces_in"     if( $args{KeepSpacesIn});
        croak "cannot use both discard_spaces and discard_all_spaces" if( $args{DiscardAllSpaces});
        croak "cannot use both discard_spaces and discard_spaces_in"  if( $args{DiscardSpacesIn});
        $self->{twig_discard_spaces}=1; 
        delete $args{DiscardSpaces}; 
      }
    if( $args{KeepSpacesIn})
      { croak "cannot use both keep_spaces_in and discard_spaces_in"  if( $args{DiscardSpacesIn});
        croak "cannot use both keep_spaces_in and discard_all_spaces" if( $args{DiscardAllSpaces});
        $self->{twig_discard_spaces}=1; 
        $self->{twig_keep_spaces_in}={}; 
        my @tags= @{$args{KeepSpacesIn}}; 
        foreach my $tag (@tags) { $self->{twig_keep_spaces_in}->{$tag}=1; } 
        delete $args{KeepSpacesIn}; 
      }

    if( $args{DiscardAllSpaces})
      { 
        croak "cannot use both discard_all_spaces and discard_spaces_in" if( $args{DiscardSpacesIn});
        $self->{twig_discard_all_spaces}=1; 
        delete $args{DiscardAllSpaces}; 
      }

    if( $args{DiscardSpacesIn})
      { $self->{twig_keep_spaces}=1; 
        $self->{twig_discard_spaces_in}={}; 
        my @tags= @{$args{DiscardSpacesIn}};
        foreach my $tag (@tags) { $self->{twig_discard_spaces_in}->{$tag}=1; } 
        delete $args{DiscardSpacesIn}; 
      }
    # discard spaces by default 
    $self->{twig_discard_spaces}= 1 unless(  $self->{twig_keep_spaces});

    $args{Comments}||= $COMMENTS_DEFAULT;
    if( $args{Comments} eq 'drop')       { $self->{twig_keep_comments}= 0;    }
    elsif( $args{Comments} eq 'keep')    { $self->{twig_keep_comments}= 1;    }
    elsif( $args{Comments} eq 'process') { $self->{twig_process_comments}= 1; }
    else { croak "wrong value for comments argument: '$args{Comments}' (should be 'drop', 'keep' or 'process')"; }
    delete $args{Comments};

    $args{Pi}||= $PI_DEFAULT;
    if( $args{Pi} eq 'drop')       { $self->{twig_keep_pi}= 0;    }
    elsif( $args{Pi} eq 'keep')    { $self->{twig_keep_pi}= 1;    }
    elsif( $args{Pi} eq 'process') { $self->{twig_process_pi}= 1; }
    else { croak "wrong value for pi argument: '$args{Pi}' (should be 'drop', 'keep' or 'process')"; }
    delete $args{Pi};

    if( $args{KeepEncoding})
      { 
        # set it in XML::Twig::Elt so print functions know what to do
        $self->set_keep_encoding( 1); 
        $self->{parse_start_tag}= $args{ParseStartTag} || \&_parse_start_tag; 
        delete $args{ParseStartTag} if defined( $args{ParseStartTag}) ;
        delete $args{KeepEncoding};
      }
    else
      { $self->set_keep_encoding( 0);  
        if( $args{ParseStartTag}) 
          { $self->{parse_start_tag}= $args{ParseStartTag}; }
        else
          { delete $self->{parse_start_tag}; }
        delete $args{ParseStartTag};
      }

    if( $args{OutputFilter})
      { $self->set_output_filter( $args{OutputFilter}); 
        delete $args{OutputFilter};
      }
    else
      { $self->set_output_filter( 0); }

    if( $args{RemoveCdata})
      { $self->set_remove_cdata( $args{RemoveCdata}); 
        delete $args{RemoveCdata}; 
      }
    else
      { $self->set_remove_cdata( 0); }

    if( $args{OutputTextFilter})
      { $self->set_output_text_filter( $args{OutputTextFilter}); 
        delete $args{OutputTextFilter};
      }
    else
      { $self->set_output_text_filter( 0); }

    if( $args{KeepAttsOrder})
      { $self->{keep_atts_order}= $args{KeepAttsOrder};
        if( _use( 'Tie::IxHash'))
          { $self->set_keep_atts_order(  $self->{keep_atts_order}); }
        else 
          { croak "Tie::IxHash not available, option keep_atts_order not allowed"; }
      }
    else
      { $self->set_keep_atts_order( 0); }


    if( $args{PrettyPrint})    { $self->set_pretty_print( $args{PrettyPrint}); }
    if( $args{EscapeGt})       { $self->escape_gt( $args{EscapeGt});           }
    if( $args{EmptyTags})      { $self->set_empty_tag_style( $args{EmptyTags}) }

    if( exists $args{Id})      { $ID= $args{Id};                     delete $args{ID};             }
    if( $args{NoProlog})       { $self->{no_prolog}= 1;              delete $args{NoProlog};       }
    if( $args{DoNotOutputDTD}) { $self->{no_dtd_output}= 1;          delete $args{DoNotOutputDTD}; }
    if( $args{LoadDTD})        { $self->{twig_read_external_dtd}= 1; delete $args{LoadDTD};        }
    if( $args{CharHandler})    { $self->setCharHandler( $args{CharHandler}); delete $args{CharHandler}; }

    if( $args{InputFilter})    { $self->set_input_filter(  $args{InputFilter}); delete  $args{InputFilter}; }
    if( $args{NoExpand})       { $self->setHandlers( %twig_noexpand_handlers); $self->{twig_no_expand}=1; }
    if( my $output_encoding= $args{OutputEncoding}) { $self->set_output_encoding( $output_encoding); delete $args{OutputFilter}; }

    if( my $tdh= $args{TopDownHandlers}) { $self->{twig_tdh}=1; delete $args{TopDownHandlers}; }

    if( my $acc_a= $args{AttAccessors})   { $self->att_accessors( @$acc_a);  }
    if( my $acc_e= $args{EltAccessors})   { $self->elt_accessors( isa( $acc_e, 'ARRAY') ? @$acc_e : $acc_e);   }
    if( my $acc_f= $args{FieldAccessors}) { $self->field_accessors( isa( $acc_f, 'ARRAY') ? @$acc_f : $acc_f); }

    if( $args{UseTidy}) { $self->{use_tidy}= 1; }
    $self->{tidy_options}= $args{TidyOptions} || {};

    if( $args{OutputHtmlDoctype}) { $self->{html_doctype}= 1; }

    $self->set_quote( $args{Quote} || 'double');

    # set handlers
    if( $self->{twig_roots})
      { if( $self->{twig_default_print})
          { if( $self->{twig_keep_encoding})
              { $self->setHandlers( %twig_handlers_roots_print_original); }
            else
              { $self->setHandlers( %twig_handlers_roots_print);  }
          }
        else
          { $self->setHandlers( %twig_handlers_roots); }
      }
    else
      { $self->setHandlers( %twig_handlers); }

    # XML::Parser::Expat does not like these handler to be set. So in order to 
    # use the various sets of handlers on XML::Parser or XML::Parser::Expat
    # objects when needed, these ones have to be set only once, here, at 
    # XML::Parser level
    $self->setHandlers( Init => \&_twig_init, Final => \&_twig_final);

    $self->{twig_entity_list}= XML::Twig::Entity_list->new; 
    $self->{twig_notation_list}= XML::Twig::Notation_list->new; 

    $self->{twig_id}= $ID; 
    $self->{twig_stored_spaces}='';

    $self->{twig_autoflush}= 1; # auto flush by default

    $self->{twig}= $self;
    if( $weakrefs) { weaken( $self->{twig}); }

    return $self;
  }

sub parse
  {
    my $t= shift;
    # if called as a class method, calls nparse, which creates the twig then parses it
    if( !ref( $t) || !isa( $t, 'XML::Twig')) { return $t->nparse( @_); }

    # requires 5.006 at least (or the ${^UNICODE} causes a problem)                                       # > perl 5.5
    # trap underlying bug in IO::Handle (see RT #17500)                                                   # > perl 5.5
    # croak if perl 5.8+, -CD (or PERL_UNICODE set to D) and parsing a pipe                               # > perl 5.5
    if( $perl_version>=5.008 && ${^UNICODE} && (${^UNICODE} & 24) && isa( $_[0], 'GLOB') && -p $_[0] )               # > perl 5.5
      { croak   "cannot parse the output of a pipe when perl is set to use the UTF8 perlIO layer\n"       # > perl 5.5
              . "set the environment variable PERL_UNICODE or use the -C option (see perldoc perlrun)\n"  # > perl 5.5
              . "not to include 'D'";                                                                     # > perl 5.5
      }                                                                                                   # > perl 5.5
    $t= eval { $t->SUPER::parse( @_); }; 
    
    if(    !$t 
        && $@=~m{(syntax error at line 1, column 0, byte 0|not well-formed \(invalid token\) at line 1, column 1, byte 1)} 
        && -f $_[0]
        && ( ! ref( $_[0]) || ref( $_[0])) ne 'GLOB' # -f works on a filehandle, so this make sure $_[0] is a real file
      )
      { croak "you seem to have used the parse method on a filename ($_[0]), you probably want parsefile instead"; }
    return _checked_parse_result( $t, $@);
  }

sub parsefile
  { my $t= shift;
    if( -f $_[0] && ! -s $_[0]) { return _checked_parse_result( undef, "empty file '$_[0]'"); }
    $t= eval { $t->SUPER::parsefile( @_); };
    return _checked_parse_result( $t, $@);
  }

sub _checked_parse_result
  { my( $t, $returned)= @_;
    if( !$t)
      { if( isa( $returned, 'XML::Twig') && $returned->{twig_finish_now})
          { $t= $returned;
            delete $t->{twig_finish_now};
            return $t->_twig_final;
          }
        else
          { _croak( $returned, 0); }
      }
    
    $active_twig= $t;
    return $t;
  }

sub active_twig { return $active_twig; }

sub finish_now
  { my $t= shift;
    $t->{twig_finish_now}=1;
    # XML::Parser 2.43 changed xpcroak in a way that caused test failures for XML::Twig
    # the change was reverted in 2.44, but this is here to ensure that tests pass with 2.43
    if( $XML::Parser::VERSION == 2.43)
      { no warnings;
        $t->parser->{twig_error}= $t;
        *XML::Parser::Expat::xpcroak= sub { die $_[0]->{twig_error}; };
        die $t;
      }
    else
      { die $t; }
  }


sub parsefile_inplace      { shift->_parse_inplace( parsefile      => @_); }
sub parsefile_html_inplace { shift->_parse_inplace( parsefile_html => @_); }

sub _parse_inplace
  { my( $t, $method, $file, $suffix)= @_;
    _use( 'File::Temp') || croak "need File::Temp to use inplace methods\n";
    _use( 'File::Basename');


    my $tmpdir= dirname( $file);
    my( $tmpfh, $tmpfile)= File::Temp::tempfile( DIR => $tmpdir);
    my $original_fh= select $tmpfh;

    # we can only use binmode :utf8 if perl was compiled with useperlio
    # might be a problem if keep_encoding used but the file is already in utf8
    if( $perl_version > 5.006 && !$t->{twig_keep_encoding} && _use_perlio()) {  binmode( $tmpfh, ":utf8" ); }

    $t->$method( $file);

    select $original_fh;
    close $tmpfh;
    my $mode= (stat( $file))[2] & oct(7777);
    chmod $mode, $tmpfile or croak "cannot change temp file mode to $mode: $!";

    if( $suffix) 
      { my $backup;
        if( $suffix=~ m{\*}) { ($backup = $suffix) =~ s/\*/$file/g; }
        else                 { $backup= $file . $suffix; }
          
        rename( $file, $backup) or croak "cannot backup initial file ($file) to $backup: $!"; 
      }
    rename( $tmpfile, $file) or croak "cannot rename temp file ($tmpfile) to initial file ($file): $!";

    return $t;
  }
    
 
sub parseurl
  { my $t= shift;
    $t->_parseurl( 0, @_);
  }

sub safe_parseurl
  { my $t= shift;
    $t->_parseurl( 1, @_);
  }

sub safe_parsefile_html
  { my $t= shift;
    eval { $t->parsefile_html( @_); };
    return $@ ? $t->_reset_twig_after_error : $t;
  }

sub safe_parseurl_html
  { my $t= shift;
    _use( 'LWP::Simple') or croak "missing LWP::Simple"; 
    eval { $t->parse_html( LWP::Simple::get( shift()), @_); } ;
    return $@ ? $t->_reset_twig_after_error : $t;
  }

sub parseurl_html
  { my $t= shift;
    _use( 'LWP::Simple') or croak "missing LWP::Simple"; 
    $t->parse_html( LWP::Simple::get( shift()), @_); 
  }


# uses eval to catch the parser's death
sub safe_parse_html
  { my $t= shift;
    eval { $t->parse_html( @_); } ;
    return $@ ? $t->_reset_twig_after_error : $t;
  }

sub parsefile_html
  { my $t= shift;
    my $file= shift;
    my $indent= $t->{ErrorContext} ? 1 : 0;
    $t->set_empty_tag_style( 'html');
    my $html2xml=  $t->{use_tidy} ? \&_tidy_html : \&_html2xml;
    my $options= $t->{use_tidy} ? $t->{tidy_options} || {} :  { indent => $indent, html_doctype => $t->{html_doctype} };
    $t->parse( $html2xml->( _slurp( $file), $options), @_);
    return $t;
  }

sub parse_html
  { my $t= shift;
    my $options= ref $_[0] && ref $_[0] eq 'HASH' ? shift() : {};
    my $use_tidy= exists $options->{use_tidy} ? $options->{use_tidy} : $t->{use_tidy};
    my $content= shift;
    my $indent= $t->{ErrorContext} ? 1 : 0;
    $t->set_empty_tag_style( 'html');
    my $html2xml=  $use_tidy ? \&_tidy_html : \&_html2xml;
    my $conv_options= $use_tidy ? $t->{tidy_options} || {} :  { indent => $indent, html_doctype => $t->{html_doctype} };
    $t->parse( $html2xml->( isa( $content, 'GLOB') ? _slurp_fh( $content) : $content, $conv_options), @_);
    return $t;
  }

sub xparse
  { my $t= shift;
    my $to_parse= $_[0];
    if( isa( $to_parse, 'GLOB'))             { $t->parse( @_);                 }
    elsif( $to_parse=~ m{^\s*<})             { $to_parse=~ m{<html}i ? $t->_parse_as_xml_or_html( @_)
                                                                     : $t->parse( @_);                 
                                             }
    elsif( $to_parse=~ m{^\w+://.*\.html?$}) { _use( 'LWP::Simple') or croak "missing LWP::Simple"; 
                                               $t->_parse_as_xml_or_html( LWP::Simple::get( shift()), @_);
                                             }
    elsif( $to_parse=~ m{^\w+://})           { _use( 'LWP::Simple') or croak "missing LWP::Simple";
                                               my $doc= LWP::Simple::get( shift);
                                               if( ! defined $doc) { $doc=''; }
                                               my $xml_parse_ok= $t->safe_parse( $doc, @_);
                                               if( $xml_parse_ok)
                                                 { return $xml_parse_ok; }
                                               else
                                                 { my $diag= $@;
                                                   if( $doc=~ m{<html}i)
                                                     { $t->parse_html( $doc, @_); }
                                                    else
                                                      { croak $diag; }
                                                 }
                                             }
    elsif( $to_parse=~ m{\.html?$})          { my $content= _slurp( shift);
                                               $t->_parse_as_xml_or_html( $content, @_); 
                                             }
    else                                     { $t->parsefile( @_);             }
  }

sub _parse_as_xml_or_html
  { my $t= shift; 
    if( _is_well_formed_xml( $_[0]))
      { $t->parse( @_) }
    else
      { my $html2xml=  $t->{use_tidy} ? \&_tidy_html : \&_html2xml;
        my $options= $t->{use_tidy} ? $t->{tidy_options} || {} :  { indent => 0, html_doctype => $t->{html_doctype} };
        my $html= $html2xml->( $_[0], $options, @_);
        if( _is_well_formed_xml( $html))
          { $t->parse( $html); }
        else
          { croak $@; } # can't really test this because HTML::Parser or HTML::Tidy may change how they deal with bas HTML between versions
      }
  }  
    
{ my $parser;
  sub _is_well_formed_xml
    { $parser ||= XML::Parser->new;
      eval { $parser->parse( $_[0]); };
      return $@ ? 0 : 1;
    }
}

sub nparse
  { my $class= shift;
    my $to_parse= pop;
    $class->new( @_)->xparse( $to_parse);
  }

sub nparse_pp   { shift()->nparse( pretty_print => 'indented', @_); }
sub nparse_e    { shift()->nparse( error_context => 1,         @_); }
sub nparse_ppe  { shift()->nparse( pretty_print => 'indented', error_context => 1, @_); }


sub _html2xml
  { my( $html, $options)= @_;
    _use( 'HTML::TreeBuilder', '3.13') or croak "cannot parse HTML: missing HTML::TreeBuilder v >= 3.13\n"; 
    my $tree= HTML::TreeBuilder->new;
    $tree->ignore_ignorable_whitespace( 0); 
    $tree->ignore_unknown( 0); 
    $tree->no_space_compacting( 1);
    $tree->store_comments( 1);
    $tree->store_pis(1); 
    $tree->parse( $html);
    $tree->eof;

    my $xml='';
    if( $options->{html_doctype} && exists $tree->{_decl} )
      { my $decl= $tree->{_decl}->as_XML;

        # first try to fix declarations that are missing the SYSTEM part 
        $decl =~ s{^\s*<!DOCTYPE \s+ ((?i)html) \s+ PUBLIC \s+ "([^"]*)" \s* >}
                  { my $system= $HTML_DECL{$2} || $HTML_DECL{$DEFAULT_HTML_TYPE};
                    qq{<!DOCTYPE $1 PUBLIC "$2" "$system">}
                   
                  }xe;

        # then check that the declaration looks OK (so it parses), if not remove it,
        # better to parse without the declaration than to die stupidly
        if(    $decl =~ m{<!DOCTYPE \s+ (?i:HTML) (\s+ PUBLIC \s+ "[^"]*" \s+ (SYSTEM \s+)? "[^"]*")? \s*>}x # PUBLIC then SYSTEM
            || $decl =~ m{<!DOCTYPE \s+ (?i:HTML) \s+ SYSTEM \s+ "[^"]*" \s*>}x                             # just SYSTEM
          )
          { $xml= $decl; }
      } 

    $xml.= _as_XML( $tree);


    _fix_xml( $tree, \$xml);

    if( $options->{indent}) { _indent_xhtml( \$xml); }
    $tree->delete;
    $xml=~ s{\s+$}{}s; # trim end
    return $xml;
  }

sub _tidy_html
  { my( $html, $options)= @_;
   _use( 'HTML::Tidy') or croak "cannot cleanup HTML using HTML::Tidy (required by the use_tidy option): $@\n"; ;
    my $TIDY_DEFAULTS= { output_xhtml => 1, # duh!
                         tidy_mark => 0,    # do not add the "generated by tidy" comment
                         numeric_entities => 1,
                         char_encoding =>  'utf8',
                         bare => 1,
                         clean => 1,
                         doctype => 'transitional',
                         fix_backslash => 1,
                         merge_divs => 0,
                         merge_spans => 0,
                         sort_attributes => 'alpha',
                         indent => 0,
                         wrap => 0,
                         break_before_br => 0,
                       };
    $options ||= {};
    my $tidy_options= { %$TIDY_DEFAULTS, %$options};
    my $tidy = HTML::Tidy->new( $tidy_options);
    $tidy->ignore( type => 1, type => 2 ); # 1 is TIDY_WARNING, 2 is TIDY_ERROR, not clean
    my $xml= $tidy->clean( $html );
    return $xml;
  }


{ my %xml_parser_encoding;
  sub _fix_xml
    { my( $tree, $xml)= @_; # $xml is a ref to the xml string

      my $max_tries=5;
      my $add_decl;

      while( ! _check_xml( $xml) && $max_tries--)
        { 
          # a couple of fixes for weird HTML::TreeBuilder errors
          if( $@=~ m{^\s*xml (or text )?declaration not at start of (external )?entity}i)
            { $$xml=~ s{<\?xml.*?\?>}{}g; 
              #warn " fixed xml declaration in the wrong place\n";
            }
          elsif( $@=~ m{undefined entity})
            { $$xml=~ s{&(amp;)?Amp;}{&amp;}g if $HTML::TreeBuilder::VERSION < 4.00;
              if( _use( 'HTML::Entities::Numbered')) { $$xml=name2hex_xml( $$xml); }
              $$xml=~ s{&(\w+);}{ my $ent= $1; if( $ent !~ m{^(amp|lt|gt|apos|quote)$}) { "&amp;$ent;" } }eg;
            }
          elsif( $@=~ m{&Amp; used in html})
            # if $Amp; is used instead of &amp; then HTML::TreeBuilder's as_xml is tripped (old version)
            { $$xml=~ s{&(amp;)?Amp;}{&amp;}g if $HTML::TreeBuilder::VERSION < 4.00; 
            } 
          elsif( $@=~ m{^\s*not well-formed \(invalid token\)})
            { if( $HTML::TreeBuilder::VERSION < 4.00)
                { $$xml=~ s{&(amp;)?Amp;}{&amp;}g; 
                  $$xml=~  s{(<[^>]* )(\d+=)"}{$1a$2"}g; # <table 1> comes out as <table 1="1">, "fix the attribute
                }
              my $q= '<img "="&#34;" '; # extracted so vim doesn't get confused
              if( _use( 'HTML::Entities::Numbered')) { $$xml=name2hex_xml( $$xml); }
              if( $$xml=~ m{$q}) 
                { $$xml=~ s{$q}{<img }g; # happens with <img src="foo.png"" ...
                } 
              else
                { my $encoding= _encoding_from_meta( $tree);
                  unless( keys %xml_parser_encoding) { %xml_parser_encoding= _xml_parser_encodings(); }

                  if( ! $add_decl)
                    { if( $xml_parser_encoding{$encoding})
                        { $add_decl=1; }
                      elsif( $encoding eq 'euc-jp' && $xml_parser_encoding{'x-euc-jp-jisx0221'})
                        { $encoding="x-euc-jp-jisx0221"; $add_decl=1;}
                      elsif( $encoding eq 'shift-jis' && $xml_parser_encoding{'x-sjis-jisx0221'})
                        { $encoding="x-sjis-jisx0221";   $add_decl=1;}

                      if( $add_decl) 
                        { $$xml=~ s{^(<\?xml.*?\?>)?}{<?xml version="1.0" encoding="$encoding"?>}s;
                          #warn "  added decl (encoding $encoding)\n";
                        }
                      else
                        { $$xml=~ s{^(<\?xml.*?\?>)?}{}s;
                          #warn "  converting to utf8 from $encoding\n";
                          $$xml= _to_utf8( $encoding, $$xml);
                        }
                    }
                  else
                    { $$xml=~ s{^(<\?xml.*?\?>)?}{}s;
                      #warn "  converting to utf8 from $encoding\n";
                      $$xml= _to_utf8( $encoding, $$xml);
                    }
                }
            }
        }

      # some versions of HTML::TreeBuilder escape CDATA sections
      $$xml=~ s{(&lt;!\[CDATA\[.*?\]\]&gt;)}{_unescape_cdata( $1)}eg;
    
  }

  sub _xml_parser_encodings
    { my @encodings=( 'iso-8859-1'); # this one is included by default, there is no map for it in @INC
      foreach my $inc (@INC)
        { push @encodings, map { basename( $_, '.enc') } glob( File::Spec->catdir( $inc => XML => Parser => Encodings => '*.enc')); }
      return map { $_ => 1 } @encodings;
    }
}


sub _unescape_cdata
  { my( $cdata)= @_;
    $cdata=~s{&lt;}{<}g;
    $cdata=~s{&gt;}{>}g;
    $cdata=~s{&amp;}{&}g;
    return $cdata;
  }

sub _as_XML {

    # fork of HTML::Element::as_XML, which is a little too buggy and inconsistent between versions for my liking
    my ($elt) = @_;
    my $xml= '';
    my $empty_element_map = $elt->_empty_element_map;

    my ( $tag, $node, $start );    # per-iteration scratch
    $elt->traverse(
        sub {
            ( $node, $start ) = @_;
            if ( ref $node ) 
              { # it's an element
                $tag = $node->{'_tag'};
                if ($start)
                  { # on the way in
                    foreach my $att ( grep { ! m{^(_|/$)} } keys %$node ) 
                       { # fix attribute names instead of dying
                         my $new_att= $att;
                         if( $att=~ m{^\d}) { $new_att= "a$att"; }
                         $new_att=~ s{[^\w\d:_-]}{}g;
                         $new_att ||= 'a'; 
                         if( $new_att ne $att) { $node->{$new_att}= delete $node->{$att}; }
                       }

                    if ( $empty_element_map->{$tag} && (!@{ $node->{'_content'} || []}) )
                      { $xml.= $node->starttag_XML( undef, 1 ); }
                    else 
                      { $xml.= $node->starttag_XML(undef); }
                  }
                else
                 { # on the way out
                   unless ( $empty_element_map->{$tag} and !@{ $node->{'_content'} || [] } )
                    { $xml.= $node->endtag_XML();
                    }     # otherwise it will have been an <... /> tag.
                  }
              }
            elsif( $node=~ /<!\[CDATA\[/)  # the content includes CDATA
              {  foreach my $chunk (split /(<!\[CDATA\[.*?\]\]>)/s, $node) # chunks are CDATA sections or normal text
                  { $xml.= $chunk =~ m{<!\[CDATA\[} ? $chunk : _xml_escape( $chunk); }
              }
            else   # it's just text
              { $xml .= _xml_escape($node); }
            1;            # keep traversing
        }
    );
  return $xml;
}

sub _xml_escape 
  { my( $html)= @_;
    $html =~ s{&(?!                     # An ampersand that isn't followed by...
                  (  \#[0-9]+;       |  #   A hash mark, digits and semicolon, or
                    \#x[0-9a-fA-F]+; |  #   A hash mark, "x", hex digits and semicolon, or
                    [\w]+;              #   A valid unicode entity name and semicolon
                  )
                )
              }
              {&amp;}gx if 0;    # Needs to be escaped to amp

    $html=~ s{&}{&amp;}g;

    # in old versions of HTML::TreeBuilder &amp; can come out as &Amp;
    if( $HTML::TreeBuilder::VERSION && $HTML::TreeBuilder::VERSION <= 3.23) { $html=~ s{&Amp;}{&amp;}g; }

    # simple character escapes
    $html =~ s/</&lt;/g;
    $html =~ s/>/&gt;/g;
    $html =~ s/"/&quot;/g;
    $html =~ s/'/&apos;/g;

    return $html;
  }




sub _check_xml
  { my( $xml)= @_; # $xml is a ref to the xml string
    my $ok= eval { XML::Parser->new->parse( $$xml); };
    #if( $ok) { warn "  parse OK\n"; }
    return $ok;
  }

sub _encoding_from_meta
  { my( $tree)= @_; 
    my $enc="iso-8859-1";
    my @meta= $tree->find( 'meta');
    foreach my $meta (@meta)
      { if(    $meta->{'http-equiv'} && ($meta->{'http-equiv'} =~ m{^\s*content-type\s*}i)
            && $meta->{content}      && ($meta->{content}      =~ m{^\s*text/html\s*;\s*charset\s*=\s*(\S*)\s*}i)
          )
          { $enc= lc $1;
            #warn "  encoding from meta tag is '$enc'\n";
            last;
          }
      }
    return $enc;
  }

{ sub _to_utf8 
    { my( $encoding, $string)= @_;
      local $SIG{__DIE__};
      if( _use(  'Encode')) 
        { Encode::from_to( $string, $encoding => 'utf8', 0x0400); } # 0x0400 is Encode::FB_XMLCREF
      elsif( _use( 'Text::Iconv'))
        { my $converter =  eval { Text::Iconv->new( $encoding => "utf8") };
          if( $converter) {  $string= $converter->convert( $string); }
        }
      elsif( _use( 'Unicode::Map8') && _use( 'Unicode::String'))
        { my $map= Unicode::Map8->new( $encoding); 
          $string= $map->tou( $string)->utf8;
        }
      $string=~ s{[\x00-\x08\x0B\x0C\x0E-\x1F]}{}g; # get rid of control chars, portable in 5.6
    return $string;
  }
}


sub _indent_xhtml
  { my( $xhtml)= @_; # $xhtml is a ref
    my %block_tag= map { $_ => 1 } qw( html 
                                         head 
                                           meta title link script base
                                         body 
                                           h1 h2 h3 h4 h5 h6 
                                           p br address  blockquote pre 
                                           ol ul li  dd dl dt 
                                           table tr td th tbody tfoot thead  col colgroup caption 
                                           div frame frameset hr
                                     ); 

    my $level=0;
    $$xhtml=~ s{( (?:<!(?:--.*?-->|[CDATA[.*?]]>)) # ignore comments and CDATA sections
                  | <(\w+)((?:\s+\w+\s*=\s*(?:"[^"]*"|'[^']*'))*\s*/>) # empty tag
                  | <(\w+)                         # start tag
                  |</(\w+)                         # end tag 
                )
               }
               { if(    $2 && $block_tag{$2})  { my $indent= "  " x $level;
                                                 "\n$indent<$2$3"; 
                                               }
                 elsif( $4 && $block_tag{$4})  { my $indent= "  " x $level; 
                                                 $level++ unless( $4=~ m{/>});
                                                 my $nl= $4 eq 'html' ? '' : "\n";
                                                 "$nl$indent<$4"; 
                                               }
                 elsif( $5  && $block_tag{$5}) { $level--; "</$5"; }
                 else                          { $1; }
               }xesg;
  }


sub add_stylesheet
  { my( $t, $type, $href)= @_;
    my %text_type= map { $_ => 1 } qw( xsl css);
    my $ss= $t->{twig_elt_class}->new( $PI);
    if( $text_type{$type}) 
      { $ss->_set_pi( 'xml-stylesheet', qq{type="text/$type" href="$href"}); }
    else
      { croak "unsupported style sheet type '$type'"; }
      
    $t->_add_cpi_outside_of_root( leading_cpi => $ss);
    return $t;
  }

{ my %used;       # module => 1 if require ok, 0 otherwise
  my %disallowed; # for testing, refuses to _use modules in this hash

  sub _disallow_use ## no critic (Subroutines::ProhibitNestedSubs);
    { my( @modules)= @_;
      $disallowed{$_}= 1 foreach (@modules);
    }

  sub _allow_use  ## no critic (Subroutines::ProhibitNestedSubs);
    { my( @modules)= @_;
      $disallowed{$_}= 0 foreach (@modules);
    }

  sub _use  ## no critic (Subroutines::ProhibitNestedSubs);
    { my( $module, $version)= @_;
      $version ||= 0;
      if( $disallowed{$module})   { return 0; }
      if( $used{$module})         { return 1; }
      if( eval "require $module") { import $module; $used{$module}= 1;  # no critic ProhibitStringyEval
                                    if( $version)
                                      { 
                                        ## no critic (TestingAndDebugging::ProhibitNoStrict);
                                        no strict 'refs';
                                        if( ${"${module}::VERSION"} >= $version ) { return 1; }
                                        else                                      { return 0; }
                                      }
                                    else
                                      { return 1; }
                                  }
      else                        {                          $used{$module}= 0; return 0; }
    }
}

# used to solve the [n] predicates while avoiding getting the entire list
# needs a prototype to accept passing bare blocks
sub _first_n(&$@)       ## no critic (Subroutines::ProhibitSubroutinePrototypes);
  { my $coderef= shift;
    my $n= shift;         
    my $i=0;
    if( $n > 0)
      { foreach (@_)         { if( &$coderef) { $i++; return $_ if( $i == $n); } } }
    elsif( $n < 0)
      { foreach (reverse @_) { if( &$coderef) { $i--; return $_ if( $i == $n); } } }
    else
      { croak "illegal position number 0"; }
    return undef;
  }

sub _slurp_uri
  { my( $uri, $base)= @_;
    if( $uri=~ m{^\w+://}) { _use( 'LWP::Simple'); return LWP::Simple::get( $uri); }
    else                   { return _slurp( _based_filename( $uri, $base));        }
  }

sub _based_filename
  { my( $filename, $base)= @_;
    # cf. XML/Parser.pm's file_ext_ent_handler
    if (defined($base) and not ($filename =~ m{^(?:[\\/]|\w+:)})) 
          { my $newpath = $base;
            $newpath =~ s{[^\\/:]*$}{$filename};
            $filename = $newpath;
          }
    return $filename;
  }

sub _slurp
  { my( $filename)= @_;
    my $to_slurp;
    open( $to_slurp, "<$filename") or croak "cannot open '$filename': $!"; 
    local $/= undef;
    my $content= <$to_slurp>;
    close $to_slurp;
    return $content;
  }
  
sub _slurp_fh
  { my( $fh)= @_;
    local $/= undef;
    my $content= <$fh>;
    return $content;
  }    
 
# I should really add extra options to allow better configuration of the 
# LWP::UserAgent object
# this method forks (except on VMS!)
#   - the child gets the data and copies it to the pipe,
#   - the parent reads the stream and sends it to XML::Parser
# the data is cut it chunks the size of the XML::Parser::Expat buffer
# the method returns the twig and the status
sub _parseurl
  { my( $t, $safe, $url, $agent)= @_;
    _use( 'LWP') || croak "LWP not available, needed to use parseurl methods";
    if( $^O ne 'VMS')
      { pipe( README, WRITEME) or croak  "cannot create connected pipes: $!";
        if( my $pid= fork)
          { # parent code: parse the incoming file
            close WRITEME; # no need to write
            my $result= $safe ? $t->safe_parse( \*README) : $t->parse( \*README);
            close README;
            return $@ ? 0 : $t;
          }
        else
         { # child
            close README; # no need to read
            local $|=1;
            $agent    ||= LWP::UserAgent->new;
            my $request  = HTTP::Request->new( GET => $url);
            # _pass_url_content is called with chunks of data the same size as
            # the XML::Parser buffer 
            my $response = $agent->request( $request, 
                             sub { _pass_url_content( \*WRITEME, @_); }, $BUFSIZE);
            $response->is_success or croak "$url ", $response->message;
            close WRITEME;
            CORE::exit(); # CORE is there for mod_perl (which redefines exit)
          }
      } 
    else 
      { # VMS branch (hard to test!)
        local $|=1;
        $agent    ||= LWP::UserAgent->new;
        my $request  = HTTP::Request->new( GET => $url);
        my $response = $agent->request( $request);
        $response->is_success or croak "$url ", $response->message;
        my $result= $safe ? $t->safe_parse($response->content) : $t->parse($response->content);
        return $@ ? 0 : $t;
     }

  }

# get the (hopefully!) XML data from the URL and 
sub _pass_url_content
  { my( $fh, $data, $response, $protocol)= @_;
    print {$fh} $data;
  }

sub add_options
  { my %args= map { $_, 1 } @_;
    %args= _normalize_args( %args);
    foreach (keys %args) { $valid_option{$_}++; } 
  }

sub _pretty_print_styles { return XML::Twig::Elt::_pretty_print_styles(); }

sub _twig_store_internal_dtd
 { 
   # warn " in _twig_store_internal_dtd...\n"; # DEBUG handler
    my( $p, $string)= @_;
    my $t= $p->{twig};
    if( $t->{twig_keep_encoding}) { $string= $p->original_string(); }
    $t->{twig_doctype}->{internal} .= $string;
    return;
  }

sub _twig_stop_storing_internal_dtd
   { # warn " in _twig_stop_storing_internal_dtd...\n"; # DEBUG handler
    my $p= shift;
    if( @saved_default_handler && defined $saved_default_handler[1])
      { $p->setHandlers( @saved_default_handler); }
    else
      { 
        $p->setHandlers( Default => undef);
      }
    $p->{twig}->{twig_doctype}->{internal}=~ s{^\s*\[}{};
    $p->{twig}->{twig_doctype}->{internal}=~ s{\]\s*$}{};
    return;
  }

sub _twig_doctype_fin_print
  { # warn " in _twig_doctype_fin_print...\n"; # DEBUG handler
    my( $p)= shift;
    if( $p->{twig}->{twig_doctype}->{has_internal} && !$expat_1_95_2) { print ' ]>'; }
    return;
  }
    

sub _normalize_args
  { my %normalized_args;
    while( my $key= shift )
      { $key= join '', map { ucfirst } split /_/, $key;
        #$key= "Twig".$key unless( substr( $key, 0, 4) eq 'Twig');
        $normalized_args{$key}= shift ;
      }
    return %normalized_args;
  }    

sub _is_fh { return unless $_[0]; return $_[0] if( isa( $_[0], 'GLOB') || isa( $_[0], 'IO::Scalar')); }

sub _set_handler
  { my( $handlers, $whole_path, $handler)= @_;

    my $H_SPECIAL = qr{($ALL|$DEFAULT|$COMMENT|$TEXT)};
    my $H_PI      = qr{(\?|$PI)\s*(([^\s]*)\s*)};
    my $H_LEVEL   = qr{level \s* \( \s* ([0-9]+) \s* \)}x;
    my $H_REGEXP  = qr{\(\?([\^xism]*)(-[\^xism]*)?:(.*)\)}x;
    my $H_XPATH   = qr{(/?/?$REG_TAG_PART? \s* ($REG_PREDICATE\s*)?)+}x;

    my $prev_handler;

    my $cpath= $whole_path;
    #warn "\$cpath: '$cpath\n";
    while( $cpath && $cpath=~ s{^\s*($H_SPECIAL|$H_PI|$H_LEVEL|$H_REGEXP|$H_XPATH)\s*($|\|)}{})
      { my $path= $1;
        #warn "\$cpath: '$cpath' - $path: '$path'\n";
        $prev_handler ||= $handlers->{handlers}->{string}->{$path} || undef; # $prev_handler gets the first found handler

           _set_special_handler         ( $handlers, $path, $handler, $prev_handler)
        || _set_pi_handler              ( $handlers, $path, $handler, $prev_handler)
        || _set_level_handler           ( $handlers, $path, $handler, $prev_handler)
        || _set_regexp_handler          ( $handlers, $path, $handler, $prev_handler)
        || _set_xpath_handler           ( $handlers, $path, $handler, $prev_handler)
        || croak "unrecognized expression in handler: '$whole_path'";
    
        # this both takes care of the simple (gi) handlers and store
        # the handler code reference for other handlers
        $handlers->{handlers}->{string}->{$path}= $handler;
      }

    if( $cpath) { croak "unrecognized expression in handler: '$whole_path'"; }

    return $prev_handler;
  }


sub _set_special_handler
  { my( $handlers, $path, $handler, $prev_handler)= @_;
    if( $path =~ m{^\s*($ALL|$DEFAULT|$COMMENT|$TEXT)\s*$}io )
      { $handlers->{handlers}->{$1}= $handler; 
        return 1;
      }
    else 
      { return 0; }
  }

sub _set_xpath_handler
  { my( $handlers, $path, $handler, $prev_handler)= @_;
    if( my $handler_data= _parse_xpath_handler( $path, $handler))
      { _add_handler( $handlers, $handler_data, $path, $prev_handler);
        return 1;
      }
    else 
      { return 0; }
  }

sub _add_handler
  { my( $handlers, $handler_data, $path, $prev_handler)= @_;

    my $tag= $handler_data->{tag};
    my @handlers= $handlers->{xpath_handler}->{$tag} ? @{$handlers->{xpath_handler}->{$tag}} : ();

    if( $prev_handler) { @handlers= grep { $_->{path} ne $path } @handlers; }

    push @handlers, $handler_data if( $handler_data->{handler});
    
    if( @handlers > 1)
      { @handlers= sort {    (($b->{score}->{type}        || 0)  <=>  ($a->{score}->{type}        || 0))
                          || (($b->{score}->{anchored}    || 0)  <=>  ($a->{score}->{anchored}    || 0))
                          || (($b->{score}->{steps}       || 0)  <=>  ($a->{score}->{steps}       || 0))
                          || (($b->{score}->{predicates}  || 0)  <=>  ($a->{score}->{predicates}  || 0))
                          || (($b->{score}->{tests}       || 0)  <=>  ($a->{score}->{tests}       || 0))
                          || ($a->{path} cmp $b->{path})
                        } @handlers;
      }

    $handlers->{xpath_handler}->{$tag}= \@handlers;
  }

sub _set_pi_handler
  { my( $handlers, $path, $handler, $prev_handler)= @_;
    # PI conditions ( '?target' => \&handler or '?' => \&handler
    #             or '#PItarget' => \&handler or '#PI' => \&handler)
    if( $path=~ /^\s*(?:\?|$PI)\s*(?:([^\s]*)\s*)$/)
      { my $target= $1 || '';
        # update the path_handlers count, knowing that
        # either the previous or the new handler can be undef
        $handlers->{pi_handlers}->{$1}= $handler;
        return 1;
      }
    else 
      { return 0; 
      }
  }

sub _set_level_handler
  { my( $handlers, $path, $handler, $prev_handler)= @_;
    if( $path =~ m{^ \s* level \s* \( \s* ([0-9]+) \s* \) \s* $}ox )
      { my $level= $1;
        my $sub= sub { my( $stack)= @_; return( ($stack->[-1]->{$ST_TAG} !~ m{^#}) && (scalar @$stack == $level + 1) ) }; 
        my $handler_data=  { tag=> '*', score => { type => $LEVEL_TRIGGER}, trigger => $sub, 
                             path => $path, handler => $handler, test_on_text => 0
                           };
        _add_handler( $handlers, $handler_data, $path, $prev_handler);
        return 1;
      }
    else 
      { return 0; }
  }

sub _set_regexp_handler
  { my( $handlers, $path, $handler, $prev_handler)= @_; 
    # if the expression was a regexp it is now a string (it was stringified when it became a hash key)
    if( $path=~ m{^\(\?([\^xism]*)(?:-[\^xism]*)?:(.*)\)$}) 
      { my $regexp= qr/(?$1:$2)/; # convert it back into a regexp
        my $sub= sub { my( $stack)= @_; return( $stack->[-1]->{$ST_TAG} =~ $regexp ) }; 
        my $handler_data=  { tag=> '*', score => { type => $REGEXP_TRIGGER} , trigger => $sub, 
                             path => $path, handler => $handler, test_on_text => 0 
                           };
        _add_handler( $handlers, $handler_data, $path, $prev_handler);
        return 1;
      }
    else 
      { return 0; }
  }

my $DEBUG_HANDLER= 0; # 0 or 1 (output the handler checking code) or 2 (super verbose)
my $handler_string;   # store the handler itself
sub _set_debug_handler    { $DEBUG_HANDLER= shift; }
sub _warn_debug_handler   { if( $DEBUG_HANDLER < 3) { warn @_; } else { $handler_string .= join( '', @_); } }
sub _return_debug_handler { my $string=  $handler_string; $handler_string=''; return $string; }

sub _parse_xpath_handler
  { my( $xpath, $handler)= @_;
    my $xpath_original= $xpath;


    if( $DEBUG_HANDLER >=1) { _warn_debug_handler( "\n\nparsing path '$xpath'\n"); }

    my $path_to_check= $xpath;
    $path_to_check=~ s{/?/?$REG_TAG_PART?\s*(?:$REG_PREDICATE\s*)?}{}g;
    if( $DEBUG_HANDLER && $path_to_check=~ /\S/) { _warn_debug_handler( "left: $path_to_check\n"); }
    return if( $path_to_check=~ /\S/);

    (my $xpath_to_display= $xpath)=~ s{(["{}'\[\]\@\$])}{\\$1}g;

    my @xpath_steps;
    my $last_token_is_sep;

    while( $xpath=~ s{^\s*
                       ( (//?)                                      # separator
                        | (?:$REG_TAG_PART\s*(?:$REG_PREDICATE\s*)?) # tag name and optional predicate
                        | (?:$REG_PREDICATE)                        # just a predicate
                       )
                     }
                     {}x
         )
      { # check that we have alternating separators and steps
        if( $2) # found a separator
          { if(  $last_token_is_sep) { return 0; }                                 # 2 separators in a row
            $last_token_is_sep= 1;
          }
        else
          { if( defined( $last_token_is_sep) && !$last_token_is_sep) { return 0; } # 2 steps in a row
            $last_token_is_sep= 0;
          }

        push @xpath_steps, $1;
      }
    if( $last_token_is_sep) { return 0; } # expression cannot end with a separator 

    my $i=-1;

    my $perlfunc= _join_n( $NO_WARNINGS . ';',
                           q|my( $stack)= @_;                    |,
                           q|my @current_elts= (scalar @$stack); |,
                           q|my @new_current_elts;               |,
                           q|my $elt;                            |,
                           ($DEBUG_HANDLER >= 1) && (qq#warn q{checking path '$xpath_to_display'\n};#),
                         );


    my $last_tag='';
    my $anchored= $xpath_original=~ m{^\s*/(?!/)} ? 1 : 0; 
    my $score={ type => $XPATH_TRIGGER, anchored => $anchored };
    my $flag= { test_on_text => 0 };
    my $sep='/';  # '/' or '//'
    while( my $xpath_step= pop @xpath_steps)
      { my( $tag, $predicate)= $xpath_step =~ m{^($REG_TAG_PART)?(?:\[(.*)\])?\s*$}; 
        $score->{steps}++;
        $tag||='*';

        my $warn_empty_stack= $DEBUG_HANDLER >= 2 ? qq{warn "return with empty stack\\n";} : '';

        if( $predicate)
          { if( $DEBUG_HANDLER >= 2)  { _warn_debug_handler( "predicate is: '$predicate'\n"); }
            # changes $predicate (from an XPath expression to a Perl one)
            if( $predicate=~ m{^\s*$REG_NUMBER\s*$}) { croak "position selector [$predicate] not supported on twig_handlers"; }
            _parse_predicate_in_handler( $predicate, $flag, $score);
            if( $DEBUG_HANDLER >= 2) { _warn_debug_handler( "predicate becomes: '$predicate'\n"); }
          }

       my $tag_cond=  _tag_cond( $tag);
       my $cond= join( " && ", grep { $_ } $tag_cond, $predicate) || 1;

       if( $css_sel && $tag=~ m{\.}) { $tag=~s{\.[^.]*$}{}; $tag ||='*'; }
       $tag=~ s{(.)#.+$}{$1};

       $last_tag ||= $tag;

       if( $sep eq '/')
         { 
           $perlfunc .= sprintf( _join_n(  q#foreach my $current_elt (@current_elts)              #,
                                           q#  { next if( !$current_elt);                         #,
                                           q#    $current_elt--;                                  #,
                                           q#    $elt= $stack->[$current_elt];                    #,
                                           q#    if( %s) { push @new_current_elts, $current_elt;} #,
                                           q#  }                                                  #,
                                        ),
                                 $cond
                               );
         }
       elsif( $sep eq '//')
         { 
           $perlfunc .= sprintf( _join_n(  q#foreach my $current_elt (@current_elts)                #,
                                           q#  { next if( !$current_elt);                           #,
                                           q#    $current_elt--;                                    #,
                                           q#    my $candidate= $current_elt;                       #,
                                           q#    while( $candidate >=0)                             #,
                                           q#      { $elt= $stack->[$candidate];                    #,
                                           q#        if( %s) { push @new_current_elts, $candidate;} #,
                                           q#        $candidate--;                                  #,
                                           q#      }                                                #,
                                           q#  }                                                    #,
                                        ),
                                 $cond
                               );
         }
       my $warn= $DEBUG_HANDLER >= 2 ? _join_n( qq#warn qq%fail at cond '$cond'%;#) : '';
       $perlfunc .= sprintf( _join_n( q#unless( @new_current_elts) { %s return 0; } #,
                                      q#@current_elts= @new_current_elts;           #,
                                      q#@new_current_elts=();                       #,
                                    ),
                             $warn
                           );

        $sep= pop @xpath_steps;
     }

    if( $anchored) # there should be a better way, but this works
      {  
       my $warn= $DEBUG_HANDLER >= 2 ? _join_n( qq#warn qq{fail, stack not empty};#) : '';
       $perlfunc .= sprintf( _join_n( q#if( ! grep { $_ == 0 } @current_elts) { %s return 0;}#), $warn);
      }

    $perlfunc.= qq{warn "handler for '$xpath_to_display' triggered\\n";\n} if( $DEBUG_HANDLER >=2);
    $perlfunc.= qq{return q{$xpath_original};\n};
    _warn_debug_handler( "\nperlfunc:\n$perlfunc\n") if( $DEBUG_HANDLER>=1);
    my $s= eval "sub { $perlfunc }";
      if( $@) 
        { croak "wrong handler condition '$xpath' ($@);" }

      _warn_debug_handler( "last tag: '$last_tag', test_on_text: '$flag->{test_on_text}'\n") if( $DEBUG_HANDLER >=1);
      _warn_debug_handler( "score: ", join( ' ', map { "$_: $score->{$_}" } sort keys %$score), "\n") if( $DEBUG_HANDLER >=1);
      return { tag=> $last_tag, score => $score, trigger => $s, path => $xpath_original, handler => $handler, test_on_text => $flag->{test_on_text} };
    }

sub _join_n { return join( "\n", @_, ''); }

# the "tag" part can be <tag>, <tag>.<class> or <tag>#<id> (where tag can be *, or start with # for hidden tags) 
sub _tag_cond
  { my( $full_tag)= @_;

    my( $tag, $class, $id);
    if( $full_tag=~ m{^(.+)#(.+)$})
      { ($tag, $id)= ($1, $2); } # <tag>#<id>
    else
      { ( $tag, $class)= $css_sel ? $full_tag=~ m{^(.*?)(?:\.([^.]*))?$} : ($full_tag, undef); }

    my $tag_cond   = $tag && $tag ne '*' ? qq#(\$elt->{'$ST_TAG'} eq "$tag")# : '';
    my $id_cond    = defined $id         ? qq#(\$elt->{id} eq "$id")#  : '';
    my $class_cond = defined $class      ? qq#(\$elt->{class}=~ m{(^| )$class( |\$)})# : '';

    my $full_cond= join( ' && ', grep { $_ } ( $tag_cond, $class_cond, $id_cond));
    
    return $full_cond;
  }

# input: the predicate ($_[0]) which will be changed in place
#        flags, a hashref with various flags (like test_on_text)
#        the score 
sub _parse_predicate_in_handler
  { my( $flag, $score)= @_[1..2];
    $_[0]=~ s{(   ($REG_STRING)                            # strings
                 |\@($REG_TAG_NAME)(\s* $REG_MATCH \s* $REG_REGEXP) # @att and regexp
                 |\@($REG_TAG_NAME)(?=\s*(?:[><=!]))       # @att followed by a comparison operator
                 |\@($REG_TAG_NAME)                        # @att (not followed by a comparison operator)
                 |=~|!~                                    # matching operators
                 |([><]=?|=|!=)(?=\s*[\d+-])               # test before a number
                 |([><]=?|=|!=)                            # test, other cases
                 |($REG_FUNCTION)                          # no arg functions
                 # this bit is a mess, but it is the only solution with this half-baked parser
                 |(string\(\s*$REG_NAME\s*\)\s*$REG_MATCH\s*$REG_REGEXP)  # string( child)=~ /regexp/
                 |(string\(\s*$REG_NAME\s*\)\s*$REG_COMP\s*$REG_STRING)   # string( child) = "value" (or other test)
                 |(string\(\s*$REG_NAME\s*\)\s*$REG_COMP\s*$REG_NUMBER)   # string( child) = nb (or other test)
                 |(and|or)
                # |($REG_NAME(?=\s*(and|or|$)))            # nested tag name (needs to be after all other unquoted strings)
                 |($REG_TAG_IN_PREDICATE)                  # nested tag name (needs to be after all other unquoted strings)
                 
              )}
             { my( $token, $str, $att_re_name, $att_re_regexp, $att, $bare_att, $num_test, $alpha_test, $func, $str_regexp, $str_test_alpha, $str_test_num, $and_or, $tag) 
               = ( $1,     $2,   $3,           $4,             $5,   $6,        $7,        $8,          $9,    $10,         $11,             $12,           $13,     $14); 
    
               $score->{predicates}++;
              
               # store tests on text (they are not always allowed)
               if( $func || $str_regexp || $str_test_num || $str_test_alpha ) { $flag->{test_on_text}= 1;   }

               if( defined $str)      { $token }
               elsif( $tag)           { qq{(\$elt->{'$ST_ELT'} && \$elt->{'$ST_ELT'}->has_child( '$tag'))} }
               elsif( $att)           { $att=~ m{^#} ? qq{ (\$elt->{'$ST_ELT'} && \$elt->{'$ST_ELT'}->{att}->{'$att'})}
                                                     : qq{\$elt->{'$att'}}
                                      }
               elsif( $att_re_name)   { $att_re_name=~ m{^#} ? qq{ (\$elt->{'$ST_ELT'} && \$elt->{'$ST_ELT'}->{att}->{'$att_re_name'}$att_re_regexp)}
                                                     : qq{\$elt->{'$att_re_name'}$att_re_regexp}
                                      }
                                        # for some reason Devel::Cover flags the following lines as not tested. They are though.
               elsif( $bare_att)      { $bare_att=~ m{^#} ? qq{(\$elt->{'$ST_ELT'} && defined(\$elt->{'$ST_ELT'}->{att}->{'$bare_att'}))}
                                                          : qq{defined( \$elt->{'$bare_att'})}
                                      }
               elsif( $num_test && ($num_test eq '=') ) { "==" } # others tests are unchanged
               elsif( $alpha_test)    { $PERL_ALPHA_TEST{$alpha_test} }
               elsif( $func && $func=~ m{^string})
                                      { "\$elt->{'$ST_ELT'}->text"; }
               elsif( $str_regexp     && $str_regexp     =~ m{string\(\s*($REG_TAG_NAME)\s*\)\s*($REG_MATCH)\s*($REG_REGEXP)})
                                      { "defined( _first_n {  \$_->text $2 $3 } 1, \$elt->{'$ST_ELT'}->_children( '$1'))"; }
               elsif( $str_test_alpha && $str_test_alpha =~ m{string\(\s*($REG_TAG_NAME)\s*\)\s*($REG_COMP)\s*($REG_STRING)})
                                      { my( $tag, $op, $str)= ($1, $2, $3);
                                        $str=~ s{(?<=.)'(?=.)}{\\'}g; # escape a quote within the string 
                                        $str=~ s{^"}{'};
                                        $str=~ s{"$}{'};
                                        "defined( _first_n { \$_->text $PERL_ALPHA_TEST{$op} $str } 1, \$elt->{'$ST_ELT'}->children( '$tag'))"; }
               elsif( $str_test_num   && $str_test_num   =~ m{string\(\s*($REG_TAG_NAME)\s*\)\s*($REG_COMP)\s*($REG_NUMBER)})
                                      { my $test= ($2 eq '=') ? '==' : $2;
                                        "defined( _first_n { \$_->text $test $3 } 1, \$elt->{'$ST_ELT'}->children( '$1'))"; 
                                      }
               elsif( $and_or)        { $score->{tests}++; $and_or eq 'and' ? '&&' : '||' ; }
               else                   { $token; }
             }gexs;
  }
    

sub setCharHandler
  { my( $t, $handler)= @_;
    $t->{twig_char_handler}= $handler;
  }


sub _reset_handlers
  { my $handlers= shift;
    delete $handlers->{handlers};
    delete $handlers->{path_handlers};
    delete $handlers->{subpath_handlers};
    $handlers->{attcond_handlers_exp}=[] if( $handlers->{attcond_handlers});
    delete $handlers->{attcond_handlers};
  }
  
sub _set_handlers
  { my $handlers= shift || return;
    my $set_handlers= {};
    foreach my $path (keys %{$handlers})
      { _set_handler( $set_handlers, $path, $handlers->{$path}); }
    
    return $set_handlers;
  }
    

sub setTwigHandler
  { my( $t, $path, $handler)= @_;
    $t->{twig_handlers} ||={};
    return _set_handler( $t->{twig_handlers}, $path, $handler);
  }

sub setTwigHandlers
  { my( $t, $handlers)= @_;
    my $previous_handlers= $t->{twig_handlers} || undef;
    _reset_handlers( $t->{twig_handlers});
    $t->{twig_handlers}= _set_handlers( $handlers);
    return $previous_handlers;
  }

sub setStartTagHandler
  { my( $t, $path, $handler)= @_;
    $t->{twig_starttag_handlers}||={};
    return _set_handler( $t->{twig_starttag_handlers}, $path, $handler);
  }

sub setStartTagHandlers
  { my( $t, $handlers)= @_;
    my $previous_handlers= $t->{twig_starttag_handlers} || undef;
    _reset_handlers( $t->{twig_starttag_handlers});
    $t->{twig_starttag_handlers}= _set_handlers( $handlers);
    return $previous_handlers;
   }

sub setIgnoreEltsHandler
  { my( $t, $path, $action)= @_;
    $t->{twig_ignore_elts_handlers}||={};
    return _set_handler( $t->{twig_ignore_elts_handlers}, $path, $action );
  }

sub setIgnoreEltsHandlers
  { my( $t, $handlers)= @_;
    my $previous_handlers= $t->{twig_ignore_elts_handlers};
    _reset_handlers( $t->{twig_ignore_elts_handlers});
    $t->{twig_ignore_elts_handlers}= _set_handlers( $handlers);
    return $previous_handlers;
   }

sub setEndTagHandler
  { my( $t, $path, $handler)= @_;
    $t->{twig_endtag_handlers}||={};
    return _set_handler( $t->{twig_endtag_handlers}, $path,$handler);
  }

sub setEndTagHandlers
  { my( $t, $handlers)= @_;
    my $previous_handlers= $t->{twig_endtag_handlers};
    _reset_handlers( $t->{twig_endtag_handlers});
    $t->{twig_endtag_handlers}= _set_handlers( $handlers);
    return $previous_handlers;
   }

# a little more complex: set the twig_handlers only if a code ref is given
sub setTwigRoots
  { my( $t, $handlers)= @_;
    my $previous_roots= $t->{twig_roots};
    _reset_handlers($t->{twig_roots});
    $t->{twig_roots}= _set_handlers( $handlers);

    _check_illegal_twig_roots_handlers( $t->{twig_roots});
    
    foreach my $path (keys %{$handlers})
      { $t->{twig_handlers}||= {};
        _set_handler( $t->{twig_handlers}, $path, $handlers->{$path})
          if( ref($handlers->{$path}) && isa( $handlers->{$path}, 'CODE')); 
      }
    return $previous_roots;
  }

sub _check_illegal_twig_roots_handlers
  { my( $handlers)= @_;
    foreach my $tag_handlers (values %{$handlers->{xpath_handler}})
      { foreach my $handler_data (@$tag_handlers)
          { if( my $type= $handler_data->{test_on_text})
              { croak "string() condition not supported on twig_roots option"; }
          }
      }
    return;
  }
    

# just store the reference to the expat object in the twig
sub _twig_init
   { # warn " in _twig_init...\n"; # DEBUG handler
    
    my $p= shift;
    my $t=$p->{twig};

    if( $t->{twig_parsing} ) { croak "cannot reuse a twig that is already parsing"; }
    $t->{twig_parsing}=1;

    $t->{twig_parser}= $p; 
    if( $weakrefs) { weaken( $t->{twig_parser}); }

    # in case they had been created by a previous parse
    delete $t->{twig_dtd};
    delete $t->{twig_doctype};
    delete $t->{twig_xmldecl};
    delete $t->{twig_root};

    # if needed set the output filehandle
    $t->_set_fh_to_twig_output_fh();
    return;
  }

# uses eval to catch the parser's death
sub safe_parse
  { my $t= shift;
    eval { $t->parse( @_); } ;
    return $@ ? $t->_reset_twig_after_error : $t;
  }

sub safe_parsefile
  { my $t= shift;
    eval { $t->parsefile( @_); } ;
    return $@ ? $t->_reset_twig_after_error : $t;
  }

# restore a twig in a proper state so it can be reused for a new parse
sub _reset_twig
  { my $t= shift;
    $t->{twig_parsing}= 0;
    delete $t->{twig_current};
    delete $t->{extra_data};
    delete $t->{twig_dtd};
    delete $t->{twig_in_pcdata};
    delete $t->{twig_in_cdata};
    delete $t->{twig_stored_space};
    delete $t->{twig_entity_list};
    $t->root->delete if( $t->root);
    delete $t->{twig_root};
    return $t;
  }

sub _reset_twig_after_error
  { my $t= shift;
    $t->_reset_twig;
    return undef;
  }


sub _add_or_discard_stored_spaces
  { my $t= shift;
   
    $t->{twig_right_after_root}=0; #XX

    my $current= $t->{twig_current} or return; # ugly hack, with ignore on, twig_current can disappear 
    return unless length $t->{twig_stored_spaces};
    my $current_gi= $XML::Twig::index2gi[$current->{'gi'}];

    if( ! $t->{twig_discard_all_spaces}) 
      { if( ! defined( $t->{twig_space_policy}->{$current_gi}))
          { $t->{twig_space_policy}->{$current_gi}= _space_policy( $t, $current_gi); }
        if(    $t->{twig_space_policy}->{$current_gi} || ($t->{twig_stored_spaces}!~ m{\n}) || $t->{twig_preserve_space})
          { _insert_pcdata( $t, $t->{twig_stored_spaces} ); }
      }

    $t->{twig_stored_spaces}='';

    return;
  }

# the default twig handlers, which build the tree
sub _twig_start
   { # warn " in _twig_start...\n"; # DEBUG handler
    
    #foreach my $s (@_) { next if ref $s; warn "$s: ", is_utf8( $s) ? "has flag" : "FLAG NOT SET"; } # YYY

    my ($p, $gi, @att)= @_;
    my $t=$p->{twig};

    # empty the stored pcdata (space stored in case they are really part of 
    # a pcdata element) or stored it if the space policy dictates so
    # create a pcdata element with the spaces if need be
    _add_or_discard_stored_spaces( $t);
    my $parent= $t->{twig_current};

    # if we were parsing PCDATA then we exit the pcdata
    if( $t->{twig_in_pcdata})
      { $t->{twig_in_pcdata}= 0;
        delete $parent->{'twig_current'};
        $parent= $parent->{parent};
      }

    # if we choose to keep the encoding then we need to parse the tag
    if( my $func = $t->{parse_start_tag})
      { ($gi, @att)= &$func($p->original_string); }
    elsif( $t->{twig_entities_in_attribute})
      { 
       ($gi,@att)= _parse_start_tag( $p->recognized_string); 
         $t->{twig_entities_in_attribute}=0;
      }

    # if we are using an external DTD, we need to fill the default attributes
    if( $t->{twig_read_external_dtd}) { _fill_default_atts( $t, $gi, \@att); }
    
    # filter the input data if need be  
    if( my $filter= $t->{twig_input_filter})
      { $gi= $filter->( $gi);
        foreach my $att (@att) { $att= $filter->($att); } 
      }

    my $ns_decl;
    if( $t->{twig_map_xmlns}) 
      { $ns_decl= _replace_ns( $t, \$gi, \@att); }

    my $elt= $t->{twig_elt_class}->new( $gi);
    $elt->set_atts( @att);

    # now we can store the tag and atts
    my $context= { $ST_TAG => $gi, $ST_ELT => $elt, @att};
    $context->{$ST_NS}= $ns_decl if $ns_decl; 
    if( $weakrefs) { weaken( $context->{$ST_ELT}); }
    push @{$t->{_twig_context_stack}}, $context;

    delete $parent->{'twig_current'} if( $parent);
    $t->{twig_current}= $elt;
    $elt->{'twig_current'}=1;

    if( $parent)
      { my $prev_sibling= $parent->{last_child};
        if( $prev_sibling) 
          { $prev_sibling->{next_sibling}=  $elt; 
            $elt->{prev_sibling}=$prev_sibling; if( $XML::Twig::weakrefs) { weaken( $elt->{prev_sibling});} ;
          }

        $elt->{parent}=$parent; if( $XML::Twig::weakrefs) { weaken( $elt->{parent});} ;
        unless( $parent->{first_child}) { $parent->{first_child}=  $elt; } 
         delete $parent->{empty}; $parent->{last_child}=$elt; if( $XML::Twig::weakrefs) { weaken( $parent->{last_child});} ;
      }
    else 
      { # processing root
        $t->set_root( $elt);
        # call dtd handler if need be
        $t->{twig_dtd_handler}->($t, $t->{twig_dtd})
          if( defined $t->{twig_dtd_handler});
      
        # set this so we can catch external entities
        # (the handler was modified during DTD processing)
        if( $t->{twig_default_print})
          { $p->setHandlers( Default => \&_twig_print); }
        elsif( $t->{twig_roots})
          { $p->setHandlers( Default => sub { return }); }
        else
          { $p->setHandlers( Default => \&_twig_default); }
      }
  
    $elt->{empty}=  $p->recognized_string=~ m{/\s*>$}s ? 1 : 0;

    $elt->{extra_data}= $t->{extra_data} if( $t->{extra_data});
    $t->{extra_data}='';

    # if the element is ID-ed then store that info
    my $id= $elt->{'att'}->{$ID};
    if( defined $id)
      { $t->{twig_id_list}->{$id}= $elt; 
        if( $weakrefs) { weaken( $t->{twig_id_list}->{$id}); }
      }

    # call user handler if need be
    if( $t->{twig_starttag_handlers})
      { # call all appropriate handlers
        my @handlers= _handler( $t, $t->{twig_starttag_handlers}, $gi);
    
        local $_= $elt;
    
        foreach my $handler ( @handlers)
          { $handler->($t, $elt) || last; }
        # call _all_ handler if needed
        if( my $all= $t->{twig_starttag_handlers}->{handlers}->{$ALL})
          { $all->($t, $elt); }
      }

    # check if the tag is in the list of tags to be ignored
    if( $t->{twig_ignore_elts_handlers})
      { my @handlers= _handler( $t, $t->{twig_ignore_elts_handlers}, $gi);
        # only the first handler counts, it contains the action (discard/print/string)
        if( @handlers) { my $action= shift @handlers; $t->ignore( $elt, $action); }
      }

    if( $elt->{'att'}->{'xml:space'} && (  $elt->{'att'}->{'xml:space'} eq 'preserve')) { $t->{twig_preserve_space}++; }
    

    return;
  }

sub _replace_ns
  { my( $t, $gi, $atts)= @_;
    my $decls;
    foreach my $new_prefix ( $t->parser->new_ns_prefixes)
      { my $uri= $t->parser->expand_ns_prefix( $new_prefix);
        # replace the prefix if it is mapped
        $decls->{$new_prefix}= $uri;
        if( !$t->{twig_keep_original_prefix} && (my $mapped_prefix= $t->{twig_map_xmlns}->{$uri}))
          { $new_prefix= $mapped_prefix; }
        # now put the namespace declaration back in the element
        if( $new_prefix eq '#default')
          { push @$atts, "xmlns" =>  $uri; } 
        else
          { push @$atts, "xmlns:$new_prefix" =>  $uri; } 
      }

    if( $t->{twig_keep_original_prefix})
      { # things become more complex: we need to find the original prefix
        # and store both prefixes
        my $ns_info= $t->_ns_info( $$gi);
        my $map_att;
        if( $ns_info->{mapped_prefix})
          { $$gi= "$ns_info->{mapped_prefix}:$$gi";
            $map_att->{$ns_info->{mapped_prefix}}= $ns_info->{prefix};
          }
        my $att_name=1;
        foreach( @$atts) 
          { if( $att_name) 
              { 
                my $ns_info= $t->_ns_info( $_);
                if( $ns_info->{mapped_prefix})
                  { $_= "$ns_info->{mapped_prefix}:$_";
                    $map_att->{$ns_info->{mapped_prefix}}= $ns_info->{prefix};
                  }
                $att_name=0; 
              }
            else           
              {  $att_name=1; }
          }
        push @$atts, '#original_gi', $map_att if( $map_att);
      }
    else
      { $$gi= $t->_replace_prefix( $$gi); 
        my $att_name=1;
        foreach( @$atts) 
          { if( $att_name) { $_= $t->_replace_prefix( $_); $att_name=0; }
            else           {  $att_name=1; }
          }
      }
    return $decls;
  }


# extract prefix, local_name, uri, mapped_prefix from a name
# will only work if called from a start or end tag handler
sub _ns_info
  { my( $t, $name)= @_;
    my $ns_info={};
    my $p= $t->parser;
    $ns_info->{uri}= $p->namespace( $name); 
    return $ns_info unless( $ns_info->{uri});

    $ns_info->{prefix}= _a_proper_ns_prefix( $p, $ns_info->{uri});
    $ns_info->{mapped_prefix}= $t->{twig_map_xmlns}->{$ns_info->{uri}} || $ns_info->{prefix};

    return $ns_info;
  }
    
sub _a_proper_ns_prefix
  { my( $p, $uri)= @_;
    foreach my $prefix ($p->current_ns_prefixes)
      { if( $p->expand_ns_prefix( $prefix) eq $uri)
          { return $prefix; }
      }
    return;
  }

# returns the uri bound to a prefix in the original document
# only works in a handler
# can be used to deal with xsi:type attributes
sub original_uri
  { my( $t, $prefix)= @_;
    my $ST_NS  = '##ns' ;
    foreach my $ns (map { $_->{$ST_NS} if  $_->{$ST_NS} } reverse @{$t->{_twig_context_stack}})
      { return $ns->{$prefix} || next; }
    return;
  }


sub _fill_default_atts
  { my( $t, $gi, $atts)= @_;
    my $dtd= $t->{twig_dtd};
    my $attlist= $dtd->{att}->{$gi};
    my %value= @$atts;
    foreach my $att (keys %$attlist)
      { if(   !exists( $value{$att}) 
            && exists( $attlist->{$att}->{default})
            && ( $attlist->{$att}->{default} ne '#IMPLIED')
          )
          { # the quotes are included in the default, so we need to remove them
            my $default_value= substr( $attlist->{$att}->{default}, 1, -1);
            push @$atts, $att, $default_value;
          }
      }
    return;
  }


# the default function to parse a start tag (in keep_encoding mode)
# can be overridden with the parse_start_tag method
# only works for 1-byte character sets
sub _parse_start_tag
  { my $string= shift;
    my( $gi, @atts);

    # get the gi (between < and the first space, / or > character)
    #if( $string=~ s{^<\s*([^\s>/]*)[\s>/]*}{}s)
    if( $string=~ s{^<\s*($REG_TAG_NAME)\s*[\s>/]}{}s)
      { $gi= $1; }
    else
      { croak "error parsing tag '$string'"; }
    while( $string=~ s{^([^\s=]*)\s*=\s*(["'])(.*?)\2\s*}{}s)
      { push @atts, $1, $3; }
    return $gi, @atts;
  }

sub set_root
  { my( $t, $elt)= @_;
    $t->{twig_root}= $elt;
    if( $elt)
      { $elt->{twig}= $t;
        if( $weakrefs) { weaken(  $elt->{twig}); }
      }
    return $t;
  }

sub _twig_end
   { # warn " in _twig_end...\n"; # DEBUG handler
    my ($p, $gi)  = @_;

    my $t=$p->{twig};

    if( $t->{twig_in_pcdata} && (my $text_handler= $t->{TwigHandlers}->{$TEXT}) )
      { local $_= $t->{twig_current}; $text_handler->( $t, $_) if $_;
      }

    if( $t->{twig_map_xmlns}) { $gi= $t->_replace_prefix( $gi); }
  
    _add_or_discard_stored_spaces( $t);
 
    # the new twig_current is the parent
    my $elt= $t->{twig_current};
    delete $elt->{'twig_current'};

    # if we were parsing PCDATA then we exit the pcdata too
    if( $t->{twig_in_pcdata})
      { 
        $t->{twig_in_pcdata}= 0;
        $elt= $elt->{parent} if($elt->{parent});
        delete $elt->{'twig_current'};
      }

    # parent is the new current element
    my $parent= $elt->{parent};
    $t->{twig_current}= $parent;

    if( $parent)
      { $parent->{'twig_current'}=1;
        # twig_to_be_normalized
        if( $parent->{twig_to_be_normalized}) { $parent->normalize; $parent->{twig_to_be_normalized}=0; }
      }

    if( $t->{extra_data}) 
      { $elt->_set_extra_data_before_end_tag( $t->{extra_data});  
        $t->{extra_data}='';
      }

    if( $t->{twig_handlers})
      { # look for handlers
        my @handlers= _handler( $t, $t->{twig_handlers}, $gi);
        
        if( $t->{twig_tdh})
          { if( @handlers) { push @{$t->{twig_handlers_to_trigger}}, [ $elt, \@handlers ]; }
            if( my $all= $t->{twig_handlers}->{handlers}->{$ALL}) 
              { push @{$t->{twig_handlers_to_trigger}}, [ $elt, [$all] ]; }
          }
        else
          {
            local $_= $elt; # so we can use $_ in the handlers
    
            foreach my $handler ( @handlers)
              { $handler->($t, $elt) || last; }
            # call _all_ handler if needed
            my $all= $t->{twig_handlers}->{handlers}->{$ALL};
            if( $all)
              { $all->($t, $elt); }
            if( @handlers || $all) { $t->{twig_right_after_root}=0; }
          }
      }

    # if twig_roots is set for the element then set appropriate handler
    if(  $t->{twig_root_depth} and ($p->depth == $t->{twig_root_depth}) )
      { if( $t->{twig_default_print})
          { # select the proper fh (and store the currently selected one)
            $t->_set_fh_to_twig_output_fh(); 
            if( !$p->depth==1) { $t->{twig_right_after_root}=1; } #XX
            if( $t->{twig_keep_encoding})
              { $p->setHandlers( %twig_handlers_roots_print_original); }
            else
              { $p->setHandlers( %twig_handlers_roots_print); }
          }
        else
          { $p->setHandlers( %twig_handlers_roots); }
      }

    if( $elt->{'att'}->{'xml:space'} && (  $elt->{'att'}->{'xml:space'} eq 'preserve')) { $t->{twig_preserve_space}--; }

    pop @{$t->{_twig_context_stack}};
    return;
  }

sub _trigger_tdh
  { my( $t)= @_;

    if( @{$t->{twig_handlers_to_trigger}})
      { my @handlers_to_trigger_now= sort { $a->[0]->cmp( $b->[0]) } @{$t->{twig_handlers_to_trigger}};
        foreach my $elt_handlers (@handlers_to_trigger_now)
          { my( $handled_elt, $handlers_to_trigger)= @$elt_handlers;
            foreach my $handler ( @$handlers_to_trigger) 
              { local $_= $handled_elt; $handler->($t, $handled_elt) || last; }
          }
      }
    return;
  }

# return the list of handler that can be activated for an element 
# (either of CODE ref's or 1's for twig_roots)

sub _handler
  { my( $t, $handlers, $gi)= @_;

    my @found_handlers=();
    my $found_handler;

    foreach my $handler ( map { @$_ } grep { $_ } $handlers->{xpath_handler}->{$gi}, $handlers->{xpath_handler}->{'*'})
      {  my $trigger= $handler->{trigger};
         if( my $found_path= $trigger->( $t->{_twig_context_stack}))
          { my $found_handler= $handler->{handler};
            push @found_handlers, $found_handler; 
          }
      }

    # if no handler found call default handler if defined
    if( !@found_handlers && defined $handlers->{handlers}->{$DEFAULT})
      { push @found_handlers, $handlers->{handlers}->{$DEFAULT}; }

    if( @found_handlers and $t->{twig_do_not_chain_handlers}) 
      { @found_handlers= ($found_handlers[0]); }

    return @found_handlers; # empty if no handler found

  }


sub _replace_prefix
  { my( $t, $name)= @_;
    my $p= $t->parser;
    my $uri= $p->namespace( $name);
    # try to get the namespace from default if none is found (for attributes)
    # this should probably be an option
    if( !$uri and( $name!~/^xml/)) { $uri= $p->expand_ns_prefix( '#default'); }
    if( $uri)
      { if (my $mapped_prefix= $t->{twig_map_xmlns}->{$uri} || $DEFAULT_URI2NS{$uri})
          { return "$mapped_prefix:$name"; }
        else
          { my $prefix= _a_proper_ns_prefix( $p, $uri);
            if( $prefix eq '#default') { $prefix=''; }
            return $prefix ? "$prefix:$name" : $name; 
          }
      }
    else
      { return $name; }
  }


sub _twig_char
   { # warn " in _twig_char...\n"; # DEBUG handler
    
    my ($p, $string)= @_;
    my $t=$p->{twig}; 

    if( $t->{twig_keep_encoding})
      { if( !$t->{twig_in_cdata})
          { $string= $p->original_string(); }
        else
          { 
            use bytes; # > perl 5.5
            if( length( $string) < 1024)
              { $string= $p->original_string(); }
            else
              { #warn "dodgy case";
                # TODO original_string does not hold the entire string, but $string is wrong
                # I believe due to a bug in XML::Parser
                # for now, we use the original string, even if it means that it's been converted to utf8
              }
          }
      }

    if( $t->{twig_input_filter}) { $string= $t->{twig_input_filter}->( $string); }
    if( $t->{twig_char_handler}) { $string= $t->{twig_char_handler}->( $string); }

    my $elt= $t->{twig_current};

    if(    $t->{twig_in_cdata})
      { # text is the continuation of a previously created cdata
        $elt->{cdata}.=  $t->{twig_stored_spaces} . $string;
      } 
    elsif( $t->{twig_in_pcdata})
      { # text is the continuation of a previously created pcdata
        if( $t->{extra_data})
          { $elt->_push_extra_data_in_pcdata( $t->{extra_data}, length( $elt->{pcdata}));
            $t->{extra_data}='';
          }
        $elt->{pcdata}.=  $string; 
      } 
    else
      { 
        # text is just space, which might be discarded later
        if( $string=~/\A\s*\Z/s)
          { 
            if( $t->{extra_data})
              { # we got extra data (comment, pi), lets add the spaces to it
                $t->{extra_data} .= $string; 
              }
            else
              { # no extra data, just store the spaces
                $t->{twig_stored_spaces}.= $string;
              }
          } 
        else
          { my $new_elt= _insert_pcdata( $t, $t->{twig_stored_spaces}.$string);
            delete $elt->{'twig_current'};
            $new_elt->{'twig_current'}=1;
            $t->{twig_current}= $new_elt;
            $t->{twig_in_pcdata}=1;
            if( $t->{extra_data})
              { $new_elt->_push_extra_data_in_pcdata( $t->{extra_data}, 0);
                $t->{extra_data}='';
              }
          }
      }
    return; 
  }

sub _twig_cdatastart
   { # warn " in _twig_cdatastart...\n"; # DEBUG handler
    
    my $p= shift;
    my $t=$p->{twig};

    $t->{twig_in_cdata}=1;
    my $cdata=  $t->{twig_elt_class}->new( $CDATA);
    my $twig_current= $t->{twig_current};

    if( $t->{twig_in_pcdata})
      { # create the node as a sibling of the PCDATA
        $cdata->{prev_sibling}=$twig_current; if( $XML::Twig::weakrefs) { weaken( $cdata->{prev_sibling});} ;
        $twig_current->{next_sibling}=  $cdata;
        my $parent= $twig_current->{parent};
        $cdata->{parent}=$parent; if( $XML::Twig::weakrefs) { weaken( $cdata->{parent});} ;
         delete $parent->{empty}; $parent->{last_child}=$cdata; if( $XML::Twig::weakrefs) { weaken( $parent->{last_child});} ;
        $t->{twig_in_pcdata}=0;
      }
    else
      { # we have to create a PCDATA element if we need to store spaces
        if( $t->_space_policy($XML::Twig::index2gi[$twig_current->{'gi'}]) && $t->{twig_stored_spaces})
          { _insert_pcdata( $t, $t->{twig_stored_spaces}); }
        $t->{twig_stored_spaces}='';
      
        # create the node as a child of the current element      
        $cdata->{parent}=$twig_current; if( $XML::Twig::weakrefs) { weaken( $cdata->{parent});} ;
        if( my $prev_sibling= $twig_current->{last_child})
          { $cdata->{prev_sibling}=$prev_sibling; if( $XML::Twig::weakrefs) { weaken( $cdata->{prev_sibling});} ;
            $prev_sibling->{next_sibling}=  $cdata;
          }
        else
          { $twig_current->{first_child}=  $cdata; }
         delete $twig_current->{empty}; $twig_current->{last_child}=$cdata; if( $XML::Twig::weakrefs) { weaken( $twig_current->{last_child});} ;
      
      }

    delete $twig_current->{'twig_current'};
    $t->{twig_current}= $cdata;
    $cdata->{'twig_current'}=1;
    if( $t->{extra_data}) { $cdata->set_extra_data( $t->{extra_data}); $t->{extra_data}='' };
    return;
  }

sub _twig_cdataend
   { # warn " in _twig_cdataend...\n"; # DEBUG handler
    
    my $p= shift;
    my $t=$p->{twig};

    $t->{twig_in_cdata}=0;

    my $elt= $t->{twig_current};
    delete $elt->{'twig_current'};
    my $cdata= $elt->{cdata};
    $elt->{cdata}=  $cdata;

    push @{$t->{_twig_context_stack}}, { $ST_TAG => $CDATA };

    if( $t->{twig_handlers})
      { # look for handlers
        my @handlers= _handler( $t, $t->{twig_handlers}, $CDATA);
        local $_= $elt; # so we can use $_ in the handlers
        foreach my $handler ( @handlers) { $handler->($t, $elt) || last; }
      }

    pop @{$t->{_twig_context_stack}};

    $elt= $elt->{parent};
    $t->{twig_current}= $elt;
    $elt->{'twig_current'}=1;

    $t->{twig_long_cdata}=0;
    return;
  }

sub _pi_elt_handlers
  { my( $t, $pi)= @_;
    my $pi_handlers= $t->{twig_handlers}->{pi_handlers} || return;
    foreach my $handler ( $pi_handlers->{$pi->{target}}, $pi_handlers->{''})
      { if( $handler) { local $_= $pi; $handler->( $t, $pi) || last; } }
  }

sub _pi_text_handler
  { my( $t, $target, $data)= @_;
    if( my $handler= $t->{twig_handlers}->{pi_handlers}->{$target})
      { return $handler->( $t, $target, $data); }
    if( my $handler= $t->{twig_handlers}->{pi_handlers}->{''})
      { return $handler->( $t, $target, $data); }
    return defined( $data) && $data ne ''  ? "<?$target $data?>" : "<?$target?>" ;
  }

sub _comment_elt_handler
  { my( $t, $comment)= @_; 
    if( my $handler= $t->{twig_handlers}->{handlers}->{$COMMENT})
      { local $_= $comment; $handler->($t, $comment); }
  }

sub _comment_text_handler
  { my( $t, $comment)= @_; 
    if( my $handler= $t->{twig_handlers}->{handlers}->{$COMMENT})
      { $comment= $handler->($t, $comment); 
        if( !defined $comment || $comment eq '') { return ''; }
      }
    return "<!--$comment-->";
  }



sub _twig_comment
   { # warn " in _twig_comment...\n"; # DEBUG handler
    
    my( $p, $comment_text)= @_;
    my $t=$p->{twig};

    if( $t->{twig_keep_encoding}) { $comment_text= substr( $p->original_string(), 4, -3); }
    
    $t->_twig_pi_comment( $p, $COMMENT, $t->{twig_keep_comments}, $t->{twig_process_comments},
                          '_set_comment', '_comment_elt_handler', '_comment_text_handler', $comment_text
                        );
    return;
  }

sub _twig_pi
   { # warn " in _twig_pi...\n"; # DEBUG handler
    
    my( $p, $target, $data)= @_;
    my $t=$p->{twig};

    if( $t->{twig_keep_encoding}) 
      { my $pi_text= substr( $p->original_string(), 2, -2); 
        ($target, $data)= split( /\s+/, $pi_text, 2);
      }

    $t->_twig_pi_comment( $p, $PI, $t->{twig_keep_pi}, $t->{twig_process_pi},
                          '_set_pi', '_pi_elt_handlers', '_pi_text_handler', $target, $data
                        );
    return;
  }

sub _twig_pi_comment
  { my( $t, $p, $type, $keep, $process, $set, $elt_handler, $text_handler, @parser_args)= @_;

    if( $t->{twig_input_filter})
          { foreach my $arg (@parser_args) { $arg= $t->{twig_input_filter}->( $arg); } }
          
    # if pi/comments are to be kept then we piggyback them to the current element
    if( $keep)
      { # first add spaces
        if( $t->{twig_stored_spaces})
              { $t->{extra_data}.= $t->{twig_stored_spaces};
                $t->{twig_stored_spaces}= '';
              }

        my $extra_data= $t->$text_handler( @parser_args);
        $t->{extra_data}.= $extra_data;

      }
    elsif( $process)
      {
        my $twig_current= $t->{twig_current}; # defined unless we are outside of the root

        my $elt= $t->{twig_elt_class}->new( $type);
        $elt->$set( @parser_args);
        if( $t->{extra_data}) 
          { $elt->set_extra_data( $t->{extra_data});
            $t->{extra_data}='';
          }

        unless( $t->root) 
          { $t->_add_cpi_outside_of_root( leading_cpi => $elt);
          }
        elsif( $t->{twig_in_pcdata})
          { # create the node as a sibling of the PCDATA
            $elt->paste_after( $twig_current);
            $t->{twig_in_pcdata}=0;
          }
        elsif( $twig_current)
          { # we have to create a PCDATA element if we need to store spaces
            if( $t->_space_policy($XML::Twig::index2gi[$twig_current->{'gi'}]) && $t->{twig_stored_spaces})
              { _insert_pcdata( $t, $t->{twig_stored_spaces}); }
            $t->{twig_stored_spaces}='';
            # create the node as a child of the current element
            $elt->paste_last_child( $twig_current);
          }
        else
          { $t->_add_cpi_outside_of_root( trailing_cpi => $elt); }

        if( $twig_current)
          { delete $twig_current->{'twig_current'};
            my $parent= $elt->{parent};
            $t->{twig_current}= $parent;
            $parent->{'twig_current'}=1;
          }

        $t->$elt_handler( $elt);
      }

  }
    

# add a comment or pi before the first element
sub _add_cpi_outside_of_root
  { my($t, $type, $elt)= @_; # $type is 'leading_cpi' or 'trailing_cpi'
    $t->{$type} ||= $t->{twig_elt_class}->new( '#CPI');
    # create the node as a child of the current element
    $elt->paste_last_child( $t->{$type});
    return $t;
  }
  
sub _twig_final
   { # warn " in _twig_final...\n"; # DEBUG handler
    
    my $p= shift;
    my $t= $p->isa( 'XML::Twig') ? $p : $p->{twig};

    # store trailing data
    if( $t->{extra_data}) { $t->{trailing_cpi_text} = $t->{extra_data}; $t->{extra_data}=''; }
    $t->{trailing_spaces}= $t->{twig_stored_spaces} || ''; 
    my $s=  $t->{twig_stored_spaces}; $s=~s{\n}{\\n}g;
    if( $t->{twig_stored_spaces}) { my $s=  $t->{twig_stored_spaces}; }

    # restore the selected filehandle if needed
    $t->_set_fh_to_selected_fh();

    $t->_trigger_tdh if( $t->{twig_tdh});

    select $t->{twig_original_selected_fh} if($t->{twig_original_selected_fh}); # probably dodgy

    if( exists $t->{twig_autoflush_data})
      { my @args;
        push @args,  $t->{twig_autoflush_data}->{fh}      if( $t->{twig_autoflush_data}->{fh});
        push @args,  @{$t->{twig_autoflush_data}->{args}} if( $t->{twig_autoflush_data}->{args});
        $t->flush( @args);
        delete $t->{twig_autoflush_data};
        $t->root->delete if $t->root;
      }

    # tries to clean-up (probably not very well at the moment)
    #undef $p->{twig};
    undef $t->{twig_parser};
    delete $t->{twig_parsing};
    @{$t}{ qw( twig_parser twig_parsing _twig_context_stack twig_current) }=();

    return $t;
  }

sub _insert_pcdata
  { my( $t, $string)= @_;
    # create a new PCDATA element
    my $parent= $t->{twig_current};    # always defined
    my $elt;
    if( exists $t->{twig_alt_elt_class})
      { $elt=  $t->{twig_elt_class}->new( $PCDATA);
        $elt->{pcdata}=  $string;
      }
    else
      { $elt= bless( { gi => $XML::Twig::gi2index{$PCDATA}, pcdata => $string }, 'XML::Twig::Elt'); }

    my $prev_sibling= $parent->{last_child};
    if( $prev_sibling) 
      { $prev_sibling->{next_sibling}=  $elt; 
        $elt->{prev_sibling}=$prev_sibling; if( $XML::Twig::weakrefs) { weaken( $elt->{prev_sibling});} ;
      }
    else
      { $parent->{first_child}=  $elt; }

    $elt->{parent}=$parent; if( $XML::Twig::weakrefs) { weaken( $elt->{parent});} ;
     delete $parent->{empty}; $parent->{last_child}=$elt; if( $XML::Twig::weakrefs) { weaken( $parent->{last_child});} ;
    $t->{twig_stored_spaces}='';
    return $elt;
  }

sub _space_policy
  { my( $t, $gi)= @_;
    my $policy;
    $policy=0 if( $t->{twig_discard_spaces});
    $policy=1 if( $t->{twig_keep_spaces});
    $policy=1 if( $t->{twig_keep_spaces_in}
               && $t->{twig_keep_spaces_in}->{$gi});
    $policy=0 if( $t->{twig_discard_spaces_in} 
               && $t->{twig_discard_spaces_in}->{$gi});
    return $policy;
  }


sub _twig_entity
   { # warn " in _twig_entity...\n"; # DEBUG handler
    my( $p, $name, $val, $sysid, $pubid, $ndata, $param)= @_;
    my $t=$p->{twig};

    #{ no warnings; my $base= $p->base; warn "_twig_entity called: expand: '$t->{twig_expand_external_ents}', base: '$base', name: '$name', val: '$val', sysid: '$sysid', pubid: '$pubid', ndata: '$ndata', param: '$param'\n";}

    my $missing_entity=0;

    if( $sysid) 
      { if($ndata)
          { if( ! -f _based_filename( $sysid, $p->base)) { $missing_entity= 1; }
          }
        else
          { if( $t->{twig_expand_external_ents})
              { $val= eval { _slurp_uri( $sysid, $p->base) };
                if( ! defined $val) 
                  { if( $t->{twig_extern_ent_nofail}) 
                      { $missing_entity= 1; }
                    else
                      { _croak( "cannot load SYSTEM entity '$name' from '$sysid': $@", 3); }
                  }
              }
          }
      }

    my $ent=XML::Twig::Entity->new( $name, $val, $sysid, $pubid, $ndata, $param);
    if( $missing_entity) { $t->{twig_missing_system_entities}->{$name}= $ent; }

    my $entity_list= $t->entity_list;
    if( $entity_list) { $entity_list->add( $ent); }

    if( $parser_version > 2.27) 
      { # this is really ugly, but with some versions of XML::Parser the value 
        # of the entity is not properly returned by the default handler
        my $ent_decl= $ent->text;
        if( $t->{twig_keep_encoding})
          { if( defined $ent->{val} && ($ent_decl !~ /["']/))
              { my $val=  $ent->{val};
                $ent_decl .= $val =~ /"/ ? qq{'$val' } : qq{"$val" }; 
              }
            # for my solaris box (perl 5.6.1, XML::Parser 2.31, expat?)
            $t->{twig_doctype}->{internal}=~ s{<!ENTITY\s+$name\s+$}{substr( $ent_decl, 0, -1)}e;
          }
        $t->{twig_doctype}->{internal} .= $ent_decl 
          unless( $t->{twig_doctype}->{internal}=~ m{<!ENTITY\s+$name\s+});
      }

    return;
  }

sub _twig_notation
   { my( $p, $name, $base, $sysid, $pubid ) = @_;
     my $t = $p->{twig};

     my $notation = XML::Twig::Notation->new( $name, $base, $sysid, $pubid );
     my $notation_list = $t->notation_list();
     if( $notation_list ) { $notation_list->add( $notation ); }

     # internal should get the recognized_string, but XML::Parser does not provide it
     # so we need to re-create it ( $notation->text) and stick it there.
     $t->{twig_doctype}->{internal} .= $notation->text;

     return;     
   }


sub _twig_extern_ent
   { # warn " in _twig_extern_ent...I (", $_[0]->original_string, ")\n"; # DEBUG handler
    my( $p, $base, $sysid, $pubid)= @_;
    my $t= $p->{twig};
    if( $t->{twig_no_expand}) 
      { my $ent_name= $t->{twig_keep_encoding} ? $p->original_string : $p->recognized_string;
        _twig_insert_ent( $t, $ent_name);
        return '';
      }
    my $ent_content= eval { $t->{twig_ext_ent_handler}->( $p, $base, $sysid) };
    if( ! defined $ent_content)
      { 
        my $ent_name = $p->recognized_string;
        my $file     =  _based_filename( $sysid, $base);
        my $error_message= "cannot expand $ent_name - cannot load '$file'";
        if( $t->{twig_extern_ent_nofail}) { return "<!-- $error_message -->"; }
        else                              { _croak( $error_message);   }
      }
    return $ent_content; 
  }

# I use this so I can change the $Carp::CarpLevel (which determines how many call frames to skip when reporting an error)
sub _croak
  { my( $message, $level)= @_;
    $Carp::CarpLevel= $level || 0;
    croak $message;
  }

sub _twig_xmldecl
   { # warn " in _twig_xmldecl...\n"; # DEBUG handler
    
    my $p= shift;
    my $t=$p->{twig};
    $t->{twig_xmldecl}||={};                 # could have been set by set_output_encoding
    $t->{twig_xmldecl}->{version}= shift;
    $t->{twig_xmldecl}->{encoding}= shift; 
    $t->{twig_xmldecl}->{standalone}= shift;
    return;
  }

sub _twig_doctype
   { # warn " in _twig_doctype...\n"; # DEBUG handler
    my( $p, $name, $sysid, $pub, $internal)= @_;
    my $t=$p->{twig};
    $t->{twig_doctype}||= {};                   # create 
    $t->{twig_doctype}->{name}= $name;          # always there
    $t->{twig_doctype}->{sysid}= $sysid;        #  
    $t->{twig_doctype}->{pub}= $pub;            #  

    # now let's try to cope with XML::Parser 2.28 and above
    if( $parser_version > 2.27)
      { @saved_default_handler= $p->setHandlers( Default     => \&_twig_store_internal_dtd,
                                                 Entity      => \&_twig_entity,
                                               );
      $p->setHandlers( DoctypeFin  => \&_twig_stop_storing_internal_dtd);
      $t->{twig_doctype}->{internal}='';
      }
    else            
      # for XML::Parser before 2.28
      { $internal||='';
        $internal=~ s{^\s*\[}{}; 
        $internal=~ s{]\s*$}{}; 
        $t->{twig_doctype}->{internal}=$internal; 
      }

    # now check if we want to get the DTD info
    if( $t->{twig_read_external_dtd} && $sysid)
      { # let's build a fake document with an internal DTD
        if( $t->{DTDBase}) 
          { _use( 'File::Spec'); 
            $sysid=File::Spec->catfile($t->{DTDBase}, $sysid);
          }
        my $dtd= _slurp_uri( $sysid);
        # if the DTD includes an XML declaration, it needs to be moved before the DOCTYPE bit
        if( $dtd=~ s{^(\s*<\?xml(\s+\w+\s*=\s*("[^"]*"|'[^']*'))*\s*\?>)}{})
          { $dtd= "$1<!DOCTYPE $name [$dtd]><$name/>"; }
        else
          { $dtd= "<!DOCTYPE $name [$dtd]><$name/>"; }
       
        $t->save_global_state();            # save the globals (they will be reset by the following new)  
        my $t_dtd= XML::Twig->new( load_DTD => 1, ParseParamEnt => 1, error_context => $t->{ErrorContext} || 0);          # create a temp twig
        $t_dtd->parse( $dtd);               # parse it
        $t->{twig_dtd}= $t_dtd->{twig_dtd}; # grab the dtd info
        #$t->{twig_dtd_is_external}=1;
        $t->entity_list->_add_list( $t_dtd->entity_list)     if( $t_dtd->entity_list);   # grab the entity info
        $t->notation_list->_add_list( $t_dtd->notation_list) if( $t_dtd->notation_list); # grab the notation info
        $t->restore_global_state();
      }
    return;
  }

sub _twig_element
   { # warn " in _twig_element...\n"; # DEBUG handler
    
    my( $p, $name, $model)= @_;
    my $t=$p->{twig};
    $t->{twig_dtd}||= {};                      # may create the dtd 
    $t->{twig_dtd}->{model}||= {};             # may create the model hash 
    $t->{twig_dtd}->{elt_list}||= [];          # ordered list of elements 
    push @{$t->{twig_dtd}->{elt_list}}, $name; # store the elt
    $t->{twig_dtd}->{model}->{$name}= $model;  # store the model
    if( ($parser_version > 2.27) && ($t->{twig_doctype}->{internal}=~ m{(^|>)\s*$}) ) 
      { my $text= $XML::Twig::Elt::keep_encoding ? $p->original_string : $p->recognized_string; 
        unless( $text)
          { # this version of XML::Parser does not return the text in the *_string method
            # we need to rebuild it
            $text= "<!ELEMENT $name $model>";
          }
        $t->{twig_doctype}->{internal} .= $text;
      }
    return;
  }

sub _twig_attlist
   { # warn " in _twig_attlist...\n"; # DEBUG handler
    
    my( $p, $gi, $att, $type, $default, $fixed)= @_;
    #warn "in attlist: gi: '$gi', att: '$att', type: '$type', default: '$default', fixed: '$fixed'\n";
    my $t=$p->{twig};
    $t->{twig_dtd}||= {};                      # create dtd if need be 
    $t->{twig_dtd}->{$gi}||= {};               # create elt if need be 
    #$t->{twig_dtd}->{$gi}->{att}||= {};        # create att if need be 
    if( ($parser_version > 2.27) && ($t->{twig_doctype}->{internal}=~ m{(^|>)\s*$}) ) 
      { my $text= $XML::Twig::Elt::keep_encoding ? $p->original_string : $p->recognized_string; 
        unless( $text)
          { # this version of XML::Parser does not return the text in the *_string method
            # we need to rebuild it
            my $att_decl="$att $type";
            $att_decl .= " #FIXED"   if( $fixed);
            $att_decl .= " $default" if( defined $default);
            # 2 cases: there is already an attlist on that element or not
            if( $t->{twig_dtd}->{att}->{$gi})
              { # there is already an attlist, add to it
                $t->{twig_doctype}->{internal}=~ s{(<!ATTLIST\s*$gi )(.*?)\n?>}
                                                  { "$1$2\n" . ' ' x length( $1) . "$att_decl\n>"}es;
              }
            else
              { # create the attlist
                 $t->{twig_doctype}->{internal}.= "<!ATTLIST $gi $att_decl>"
              }
          }
      }
    $t->{twig_dtd}->{att}->{$gi}->{$att}= {} ;
    $t->{twig_dtd}->{att}->{$gi}->{$att}->{type}= $type; 
    $t->{twig_dtd}->{att}->{$gi}->{$att}->{default}= $default if( defined $default);
    $t->{twig_dtd}->{att}->{$gi}->{$att}->{fixed}= $fixed; 
    return;
  }

sub _twig_default
   { # warn " in _twig_default...\n"; # DEBUG handler
    
    my( $p, $string)= @_;
    
    my $t= $p->{twig};
   
    # we need to process the data in 2 cases: entity, or spaces after the closing tag

    # after the closing tag (no twig_current and root has been created)
    if(  ! $t->{twig_current} && $t->{twig_root} && $string=~ m{^\s+$}m) { $t->{twig_stored_spaces} .= $string; }

    # process only if we have an entity
    if( $string=~ m{^&([^;]*);$})
      { # the entity has to be pure pcdata, or we have a problem
        if( ($p->original_string=~ m{^<}) && ($p->original_string=~ m{>$}) ) 
          { # string is a tag, entity is in an attribute
            $t->{twig_entities_in_attribute}=1 if( $t->{twig_do_not_escape_amp_in_atts});
          }
        else
          { my $ent;
            if( $t->{twig_keep_encoding}) 
              { _twig_char( $p, $string); 
                $ent= substr( $string, 1, -1);
              }
            else
              { $ent= _twig_insert_ent( $t, $string); 
              }

            return $ent;
          }
      }
  }
    
sub _twig_insert_ent
  { 
    my( $t, $string)=@_;

    my $twig_current= $t->{twig_current};

    my $ent=  $t->{twig_elt_class}->new( $ENT);
    $ent->{ent}=  $string;

    _add_or_discard_stored_spaces( $t);
    
    if( $t->{twig_in_pcdata})
      { # create the node as a sibling of the #PCDATA

        $ent->{prev_sibling}=$twig_current; if( $XML::Twig::weakrefs) { weaken( $ent->{prev_sibling});} ;
        $twig_current->{next_sibling}=  $ent;
        my $parent= $twig_current->{parent};
        $ent->{parent}=$parent; if( $XML::Twig::weakrefs) { weaken( $ent->{parent});} ;
         delete $parent->{empty}; $parent->{last_child}=$ent; if( $XML::Twig::weakrefs) { weaken( $parent->{last_child});} ;
        # the twig_current is now the parent
        delete $twig_current->{'twig_current'};
        $t->{twig_current}= $parent;
        # we left pcdata
        $t->{twig_in_pcdata}=0;
      }
    else
      { # create the node as a child of the current element
        $ent->{parent}=$twig_current; if( $XML::Twig::weakrefs) { weaken( $ent->{parent});} ;
        if( my $prev_sibling= $twig_current->{last_child})
          { $ent->{prev_sibling}=$prev_sibling; if( $XML::Twig::weakrefs) { weaken( $ent->{prev_sibling});} ;
            $prev_sibling->{next_sibling}=  $ent;
          }
        else
          { if( $twig_current) { $twig_current->{first_child}=  $ent; } }
        if( $twig_current) {  delete $twig_current->{empty}; $twig_current->{last_child}=$ent; if( $XML::Twig::weakrefs) { weaken( $twig_current->{last_child});} ; }
      }

    # meant to trigger entity handler, does not seem to be activated at this time
    #if( my $handler= $t->{twig_handlers}->{gi}->{$ENT})
    #  { local $_= $ent; $handler->( $t, $ent); }

    return $ent;
  }

sub parser
  { return $_[0]->{twig_parser}; }

# returns the declaration text (or a default one)
sub xmldecl
  { my $t= shift;
    return '' unless( $t->{twig_xmldecl} || $t->{output_encoding});
    my $decl_string;
    my $decl= $t->{twig_xmldecl};
    if( $decl)
      { my $version= $decl->{version};
        $decl_string= q{<?xml};
        $decl_string .= qq{ version="$version"};

        # encoding can either have been set (in $decl->{output_encoding})
        # or come from the document (in $decl->{encoding})
        if( $t->{output_encoding})
          { my $encoding= $t->{output_encoding};
            $decl_string .= qq{ encoding="$encoding"};
          }
        elsif( $decl->{encoding})
          { my $encoding= $decl->{encoding};
            $decl_string .= qq{ encoding="$encoding"};
          }
    
        if( defined( $decl->{standalone}))
          { $decl_string .= q{ standalone="};  
            $decl_string .= $decl->{standalone} ? "yes" : "no";  
            $decl_string .= q{"}; 
          }
      
        $decl_string .= "?>\n";
      }
    else
      { my $encoding= $t->{output_encoding};
        $decl_string= qq{<?xml version="1.0" encoding="$encoding"?>};
      }
      
    my $output_filter= XML::Twig::Elt::output_filter();
    return $output_filter ? $output_filter->( $decl_string) : $decl_string;
  }

sub set_doctype
  { my( $t, $name, $system, $public, $internal)= @_;
    $t->{twig_doctype}= {} unless defined $t->{twig_doctype};
    my $doctype= $t->{twig_doctype};
    $doctype->{name}     = $name     if( defined $name);
    $doctype->{sysid}    = $system   if( defined $system);
    $doctype->{pub}      = $public   if( defined $public);
    $doctype->{internal} = $internal if( defined $internal);
  }

sub doctype_name
  { my $t= shift;
    my $doctype= $t->{twig_doctype} or return '';
    return $doctype->{name} || '';
  }

sub system_id
  { my $t= shift;
    my $doctype= $t->{twig_doctype} or return '';
    return $doctype->{sysid} || '';
  }

sub public_id
  { my $t= shift;
    my $doctype= $t->{twig_doctype} or return '';
    return $doctype->{pub} || '';
  }

sub internal_subset
  { my $t= shift;
    my $doctype= $t->{twig_doctype} or return '';
    return $doctype->{internal} || '';
  }

# return the dtd object
sub dtd
  { my $t= shift;
    return $t->{twig_dtd};
  }

# return an element model, or the list of element models
sub model
  { my $t= shift;
    my $elt= shift;
    return $t->dtd->{model}->{$elt} if( $elt);
    return (sort keys %{$t->dtd->{model}});
  }

        
# return the entity_list object 
sub entity_list
  { my $t= shift;
    return $t->{twig_entity_list};
  }

# return the list of entity names 
sub entity_names
  { my $t= shift;
    return $t->entity_list->entity_names;
  }

# return the entity object 
sub entity
  { my $t= shift;
    my $entity_name= shift;
    return $t->entity_list->ent( $entity_name);
  }

# return the notation_list object 
sub notation_list
  { my $t= shift;
    return $t->{twig_notation_list};
  }

# return the list of notation names 
sub notation_names
  { my $t= shift;
    return $t->notation_list->notation_names;
  }

# return the notation object 
sub notation
  { my $t= shift;
    my $notation_name= shift;
    return $t->notation_list->notation( $notation_name);
  }




sub print_prolog
  { my $t= shift;
    my $fh=  isa( $_[0], 'GLOB') || isa( $_[0], 'IO::Scalar')  ? shift : $t->{twig_output_fh} || select() || \*STDOUT;
    ## no critic (TestingAndDebugging::ProhibitNoStrict);
    no strict 'refs';
    print {$fh} $t->prolog( @_);
  }

sub prolog
  { my $t= shift;
    if( $t->{no_prolog}){ return ''; }

    return   $t->{no_prolog}             ? '' 
           : defined $t->{no_dtd_output} ? $t->xmldecl
           :                               $t->xmldecl . $t->doctype( @_);
  }

sub doctype
  { my $t= shift; 
    my %args= _normalize_args( @_);
    my $update_dtd = $args{UpdateDTD} || '';
    my $doctype_text='';
    
    my $doctype= $t->{twig_doctype};

    if( $doctype)
      { $doctype_text .= qq{<!DOCTYPE $doctype->{name}} if( $doctype->{name});
        $doctype_text .= qq{ PUBLIC "$doctype->{pub}"}  if( $doctype->{pub});
        $doctype_text .= qq{ SYSTEM}                    if( $doctype->{sysid} && !$doctype->{pub});
        $doctype_text .= qq{ "$doctype->{sysid}"}       if( $doctype->{sysid});
      }

    if( $update_dtd)
      { if( $doctype)  
          { my $internal=$doctype->{internal}; 
            # awful hack, but at least it works a little better that what was there before
            if( $internal)
              { # remove entity and notation declarations (they will be re-generated from the updated entity list)
                $internal=~ s{<! \s* ENTITY \s+ $REG_TAG_NAME \s+ ( ("[^"]*"|'[^']*') \s* | SYSTEM [^>]*) >\s*}{}xg;
                $internal=~ s{<! \s* NOTATION .*? >\s*}{}sxg;
                $internal=~ s{^\n}{};
              }
            $internal .= $t->entity_list->text   ||'' if( $t->entity_list);
            $internal .= $t->notation_list->text ||'' if( $t->notation_list);
            if( $internal) { $doctype_text .= "[\n$internal]>\n"; }
          }
        elsif( !$t->{'twig_dtd'} && ( keys %{$t->entity_list} || keys %{$t->notation_list} ) ) 
          { $doctype_text .= "<!DOCTYPE " . $t->root->gi . " [\n" . $t->entity_list->text . $t->notation_list->text . "\n]>";} 
        else
          { $doctype_text= $t->{twig_dtd};
            $doctype_text .= $t->dtd_text;
          }            
      }
    elsif( $doctype)
      { if( my $internal= $doctype->{internal}) 
          { # add opening and closing brackets if not already there
            # plus some spaces and newlines for a nice formating
            # I test it here because I can't remember which version of
            # XML::Parser need it or not, nor guess which one will in the
            # future, so this about the best I can do
            $internal=~ s{^\s*(\[\s*)?}{ [\n};
            $internal=~ s{\s*(\]\s*(>\s*)?)?\s*$}{\n]>\n};

            # XML::Parser does not include the NOTATION declarations in the DTD
            # at least in the current version. So put them back
            #if( $t->notation_list && $internal !~ m{<!\s*NOTATION})
            #  { $internal=~ s{(\n]>\n)$}{ "\n" . $t->notation_list->text . $1}es; }

            $doctype_text .=  $internal;
          }
      }
      
    if( $doctype_text)
      {
        # terrible hack, as I can't figure out in which case the darn prolog
        # should get an extra > (depends on XML::Parser and expat versions)
        $doctype_text=~ s/(>\s*)*$/>\n/; # if($doctype_text);

        my $output_filter= XML::Twig::Elt::output_filter();
        return $output_filter ? $output_filter->( $doctype_text) : $doctype_text;
      }
    else
      { return $doctype_text; }
  }

sub _leading_cpi
  { my $t= shift;
    my $leading_cpi= $t->{leading_cpi} || return '';
    return $leading_cpi->sprint( 1);
  }

sub _trailing_cpi
  { my $t= shift;
    my $trailing_cpi= $t->{trailing_cpi} || return '';
    return $trailing_cpi->sprint( 1);
  }

sub _trailing_cpi_text
  { my $t= shift;
    return $t->{trailing_cpi_text} || '';
  }

sub print_to_file
  { my( $t, $filename)= (shift, shift);
    my $out_fh;
#    open( $out_fh, ">$filename") or _croak( "cannot create file $filename: $!");     # < perl 5.8
    my $mode= $t->{twig_keep_encoding} && ! _use_perlio() ? '>' : '>:utf8';                             # >= perl 5.8
    open( $out_fh, $mode, $filename) or _croak( "cannot create file $filename: $!"); # >= perl 5.8
    $t->print( $out_fh, @_);
    close $out_fh;
    return $t;
  }

# probably only works on *nix (at least the chmod bit)
# first print to a temporary file, then rename that file to the desired file name, then change permissions
# to the original file permissions (or to the current umask)
sub safe_print_to_file
  { my( $t, $filename)= (shift, shift);
    my $perm= -f $filename ? (stat $filename)[2] & 07777 : ~umask() ;
    XML::Twig::_use( 'File::Temp') || croak "need File::Temp to use safe_print_to_file\n";
    my $tmpdir= dirname( $filename);    
    my( $fh, $tmpfilename) = File::Temp::tempfile( DIR => $tmpdir);
    $t->print_to_file( $tmpfilename, @_);
    rename( $tmpfilename, $filename) or unlink $tmpfilename && _croak( "cannot move temporary file to $filename: $!"); 
    chmod $perm, $filename;
    return $t;
  }
    

sub print
  { my $t= shift;
    my $fh=  isa( $_[0], 'GLOB') || isa( $_[0], 'IO::Scalar')  ? shift : undef;
    my %args= _normalize_args( @_);

    my $old_select    = defined $fh                  ? select $fh                                 : undef;
    my $old_pretty    = defined ($args{PrettyPrint}) ? $t->set_pretty_print( $args{PrettyPrint})  : undef;
    my $old_empty_tag = defined ($args{EmptyTags})   ? $t->set_empty_tag_style( $args{EmptyTags}) : undef;

    #if( !$t->{encoding} || lc( $t->{encoding}) eq 'utf-8') { my $out= $fh || \*STDOUT; binmode $out, ':utf8'; }

    if( $perl_version > 5.006 && ! $t->{twig_keep_encoding} && _use_perlio() ) { binmode( $fh || \*STDOUT, ":utf8" ); }

     print  $t->prolog( %args) . $t->_leading_cpi( %args);
     $t->{twig_root}->print;
     print $t->_trailing_cpi        # trailing comments and pi's (elements, in 'process' mode)
         . $t->_trailing_cpi_text   # trailing comments and pi's (in 'keep' mode)
         . ( ($t->{twig_keep_spaces}||'') && ($t->{trailing_spaces} || ''))
         ;

    
    $t->set_pretty_print( $old_pretty)       if( defined $old_pretty); 
    $t->set_empty_tag_style( $old_empty_tag) if( defined $old_empty_tag); 
    if( $fh) { select $old_select; }

    return $t;
  }


sub flush
  { my $t= shift;

    $t->_trigger_tdh if $t->{twig_tdh};

    return if( $t->{twig_completely_flushed});
  
    my $fh=  isa( $_[0], 'GLOB') || isa( $_[0], 'IO::Scalar') ? shift : undef;
    my $old_select= defined $fh ? select $fh : undef;
    my $up_to= ref $_[0] ? shift : undef;
    my %args= _normalize_args( @_);

    my $old_pretty;
    if( defined $args{PrettyPrint})
      { $old_pretty= $t->set_pretty_print( $args{PrettyPrint}); 
        delete $args{PrettyPrint};
      }

     my $old_empty_tag_style;
     if( $args{EmptyTags})
      { $old_empty_tag_style= $t->set_empty_tag_style( $args{EmptyTags}); 
        delete $args{EmptyTags};
      }


    # the "real" last element processed, as _twig_end has closed it
    my $last_elt;
    my $flush_trailing_data=0;
    if( $up_to)
      { $last_elt= $up_to; }
    elsif( $t->{twig_current})
      { $last_elt= $t->{twig_current}->{last_child}; }
    else
      { $last_elt= $t->{twig_root};
        $flush_trailing_data=1;
        $t->{twig_completely_flushed}=1;
      }

    # flush the DTD unless it has ready flushed (ie root has been flushed)
    my $elt= $t->{twig_root};
    unless( $elt->{'flushed'})
      { # store flush info so we can auto-flush later
        if( $t->{twig_autoflush})
          { $t->{twig_autoflush_data}={};
            $t->{twig_autoflush_data}->{fh}   = $fh  if( $fh);
            $t->{twig_autoflush_data}->{args} = \@_  if( @_);
          }
        $t->print_prolog( %args); 
        print $t->_leading_cpi;
      }

    while( $elt)
      { my $next_elt; 
        if( $last_elt && $last_elt->in( $elt))
          { 
            unless( $elt->{'flushed'}) 
              { # just output the front tag
                print $elt->start_tag();
                $elt->{'flushed'}=1;
              }
            $next_elt= $elt->{first_child};
          }
        else
          { # an element before the last one or the last one,
            $next_elt= $elt->{next_sibling};  
            $elt->_flush();
            $elt->delete; 
            last if( $last_elt && ($elt == $last_elt));
          }
        $elt= $next_elt;
      }

    if( $flush_trailing_data)
      { print $t->_trailing_cpi        # trailing comments and pi's (elements, in 'process' mode)
            , $t->_trailing_cpi_text   # trailing comments and pi's (in 'keep' mode)
      }

    select $old_select if( defined $old_select);
    $t->set_pretty_print( $old_pretty) if( defined $old_pretty); 
    $t->set_empty_tag_style( $old_empty_tag_style) if( defined $old_empty_tag_style); 

    if( my $ids= $t->{twig_id_list}) 
      { while( my ($id, $elt)= each %$ids) 
          { if( ! defined $elt) 
             { delete $t->{twig_id_list}->{$id} } 
          }
      }

    return $t;
  }


# flushes up to an element
# this method just reorders the arguments and calls flush
sub flush_up_to
  { my $t= shift;
    my $up_to= shift;
    if( isa( $_[0], 'GLOB') || isa( $_[0], 'IO::Scalar'))
      { my $fh=  shift;
        $t->flush( $fh, $up_to, @_);
      }
    else
      { $t->flush( $up_to, @_); }

    return $t;
  }

    
# same as print except the entire document text is returned as a string
sub sprint
  { my $t= shift;
    my %args= _normalize_args( @_);

    my $old_pretty;
    if( defined $args{PrettyPrint})
      { $old_pretty= $t->set_pretty_print( $args{PrettyPrint}); 
        delete $args{PrettyPrint};
      }

     my $old_empty_tag_style;
     if( defined $args{EmptyTags})
      { $old_empty_tag_style= $t->set_empty_tag_style( $args{EmptyTags}); 
        delete $args{EmptyTags};
      }
      
    my $string=   $t->prolog( %args)       # xml declaration and doctype
                . $t->_leading_cpi( %args) # leading comments and pi's in 'process' mode
                . ( ($t->{twig_root} && $t->{twig_root}->sprint) || '')
                . $t->_trailing_cpi        # trailing comments and pi's (elements, in 'process' mode)
                . $t->_trailing_cpi_text   # trailing comments and pi's (in 'keep' mode)
                ;
    if( $t->{twig_keep_spaces} && $t->{trailing_spaces}) { $string .= $t->{trailing_spaces}; }

    $t->set_pretty_print( $old_pretty) if( defined $old_pretty); 
    $t->set_empty_tag_style( $old_empty_tag_style) if( defined $old_empty_tag_style); 

    return $string;
  }
    

# this method discards useless elements in a tree
# it does the same thing as a flush except it does not print it
# the second argument is an element, the last purged element
# (this argument is usually set through the purge_up_to method)
sub purge
  { my $t= shift;
    my $up_to= shift;

    $t->_trigger_tdh if $t->{twig_tdh};

    # the "real" last element processed, as _twig_end has closed it
    my $last_elt;
    if( $up_to)
      { $last_elt= $up_to; }
    elsif( $t->{twig_current})
      { $last_elt= $t->{twig_current}->{last_child}; }
    else
      { $last_elt= $t->{twig_root}; }
    
    my $elt= $t->{twig_root};

    while( $elt)
      { my $next_elt; 
        if( $last_elt && $last_elt->in( $elt))
          { $elt->{'flushed'}=1;
            $next_elt= $elt->{first_child};
          }
        else
          { # an element before the last one or the last one,
            $next_elt= $elt->{next_sibling};  
            $elt->delete; 
            last if( $last_elt && ($elt == $last_elt) );
          }
        $elt= $next_elt;
      }

    if( my $ids= $t->{twig_id_list}) 
      { while( my ($id, $elt)= each %$ids) { if( ! defined $elt) { delete $t->{twig_id_list}->{$id} } } }

    return $t;
  }
    
# flushes up to an element. This method just calls purge
sub purge_up_to
  { my $t= shift;
    return $t->purge( @_);
  }

sub root
  { return $_[0]->{twig_root}; }

sub normalize
  { return $_[0]->root->normalize; }


# create accessor methods on attribute names
{ my %accessor; # memorize accessor names so re-creating them won't trigger an error
sub att_accessors
  { 
    my $twig_or_class= shift;
    my $elt_class= ref $twig_or_class ? $twig_or_class->{twig_elt_class}
                                      : 'XML::Twig::Elt'
                                      ;
    ## no critic (TestingAndDebugging::ProhibitNoStrict);
    no strict 'refs';
    foreach my $att (@_)
      { _croak( "attempt to redefine existing method $att using att_accessors")
          if( $elt_class->can( $att) && !$accessor{$att});

        if( !$accessor{$att})
          { *{"$elt_class\::$att"}=
                sub
                    :lvalue                                  # > perl 5.5
                  { my $elt= shift;
                    if( @_) { $elt->{att}->{$att}= $_[0]; }
                    $elt->{att}->{$att};
                  };
            $accessor{$att}=1;
          }
      }
    return $twig_or_class;
  }
}

{ my %accessor; # memorize accessor names so re-creating them won't trigger an error
sub elt_accessors
  { 
    my $twig_or_class= shift;
    my $elt_class= ref $twig_or_class ? $twig_or_class->{twig_elt_class}
                                      : 'XML::Twig::Elt'
                                      ;

    # if arg is a hash ref, it's exp => name, otherwise it's a list of tags
    my %exp_to_alias= ref( $_[0]) && isa( $_[0], 'HASH') ? %{$_[0]}
                                                         : map { $_ => $_ } @_;
    ## no critic (TestingAndDebugging::ProhibitNoStrict);
    no strict 'refs';
    while( my( $alias, $exp)= each %exp_to_alias )
      { if( $elt_class->can( $alias) && !$accessor{$alias})
          { _croak( "attempt to redefine existing method $alias using elt_accessors"); }

        if( !$accessor{$alias})
          { *{"$elt_class\::$alias"}= 
                sub
                  { my $elt= shift;
                    return wantarray ? $elt->children( $exp) : $elt->first_child( $exp);
                  };
            $accessor{$alias}=1;
          }                                            
      }
    return $twig_or_class;
  }
}

{ my %accessor; # memorize accessor names so re-creating them won't trigger an error
sub field_accessors
  { 
    my $twig_or_class= shift;
    my $elt_class= ref $twig_or_class ? $twig_or_class->{twig_elt_class}
                                      : 'XML::Twig::Elt'
                                      ;
    my %exp_to_alias= ref( $_[0]) && isa( $_[0], 'HASH') ? %{$_[0]}
                                                         : map { $_ => $_ } @_;

    ## no critic (TestingAndDebugging::ProhibitNoStrict);
    no strict 'refs';
    while( my( $alias, $exp)= each %exp_to_alias )
      { if( $elt_class->can( $alias) && !$accessor{$alias})
          { _croak( "attempt to redefine existing method $exp using field_accessors"); }
        if( !$accessor{$alias})                                
          { *{"$elt_class\::$alias"}=                          
                sub                                          
                  { my $elt= shift;                          
                    $elt->field( $exp)                       
                  };                                         
            $accessor{$alias}=1;                               
          }                                                  
      }
    return $twig_or_class;
  }
}

sub first_elt
  { my( $t, $cond)= @_;
    my $root= $t->root || return undef;
    return $root if( $root->passes( $cond));
    return $root->next_elt( $cond); 
  }

sub last_elt
  { my( $t, $cond)= @_;
    my $root= $t->root || return undef;
    return $root->last_descendant( $cond); 
  }

sub next_n_elt
  { my( $t, $offset, $cond)= @_;
    $offset -- if( $t->root->matches( $cond) );
    return $t->root->next_n_elt( $offset, $cond);
  }

sub get_xpath
  { my $twig= shift;
    if( isa( $_[0], 'ARRAY'))
      { my $elt_array= shift;
        return _unique_elts( map { $_->get_xpath( @_) } @$elt_array);
      }
    else
      { return $twig->root->get_xpath( @_); }
  }

# get a list of elts and return a sorted list of unique elts
sub _unique_elts
  { my @sorted= sort { $a ->cmp( $b) } @_;
    my @unique;
    while( my $current= shift @sorted)
      { push @unique, $current unless( @unique && ($unique[-1] == $current)); }
    return @unique;
  }

sub findvalue
  { my $twig= shift;
    if( isa( $_[0], 'ARRAY'))
      { my $elt_array= shift;
        return join( '', map { $_->findvalue( @_) } @$elt_array);
      }
    else
      { return $twig->root->findvalue( @_); }
  }

sub findvalues
  { my $twig= shift;
    if( isa( $_[0], 'ARRAY'))
      { my $elt_array= shift;
        return map { $_->findvalues( @_) } @$elt_array;
      }
    else
      { return $twig->root->findvalues( @_); }
  }

sub set_id_seed
  { my $t= shift;
    XML::Twig::Elt->set_id_seed( @_);
    return $t;
  }

# return an array ref to an index, or undef
sub index
  { my( $twig, $name, $index)= @_;
    return defined( $index) ? $twig->{_twig_index}->{$name}->[$index] : $twig->{_twig_index}->{$name};
  }

# return a list with just the root
# if a condition is given then return an empty list unless the root matches
sub children
  { my( $t, $cond)= @_;
    my $root= $t->root;
    unless( $cond && !($root->passes( $cond)) )
      { return ($root); }
    else
      { return (); }
  }
  
sub _children { return ($_[0]->root); }

# weird, but here for completude
# used to solve (non-sensical) /doc[1] XPath queries
sub child
  { my $t= shift;
    my $nb= shift;
    return ($t->children( @_))[$nb];
  }

sub descendants
  { my( $t, $cond)= @_;
    my $root= $t->root;
    if( $root->passes( $cond) )
      { return ($root, $root->descendants( $cond)); }
    else
      { return ( $root->descendants( $cond)); }
  }

sub simplify  { my $t= shift; $t->root->simplify( @_);  }
sub subs_text { my $t= shift; $t->root->subs_text( @_); }
sub trim      { my $t= shift; $t->root->trim( @_);      }


sub set_keep_encoding
  { my( $t, $keep)= @_;
    $t->{twig_keep_encoding}= $keep;
    $t->{NoExpand}= $keep;
    return XML::Twig::Elt::set_keep_encoding( $keep);
   }

sub set_expand_external_entities
  { return XML::Twig::Elt::set_expand_external_entities( @_); }

sub escape_gt
  { my $t= shift; $t->{twig_escape_gt}= 1; return XML::Twig::Elt::escape_gt( @_); }

sub do_not_escape_gt
  { my $t= shift; $t->{twig_escape_gt}= 0; return XML::Twig::Elt::do_not_escape_gt( @_); }

sub elt_id
  { return $_[0]->{twig_id_list}->{$_[1]}; }

# change it in ALL twigs at the moment
sub change_gi 
  { my( $twig, $old_gi, $new_gi)= @_;
    my $index;
    return unless($index= $XML::Twig::gi2index{$old_gi});
    $XML::Twig::index2gi[$index]= $new_gi;
    delete $XML::Twig::gi2index{$old_gi};
    $XML::Twig::gi2index{$new_gi}= $index;
    return $twig;
  }


# builds the DTD from the stored (possibly updated) data
sub dtd_text
  { my $t= shift;
    my $dtd= $t->{twig_dtd};
    my $doctype= $t->{twig_doctype} or return '';
    my $string= "<!DOCTYPE ".$doctype->{name};

    $string .= " [\n";

    foreach my $gi (@{$dtd->{elt_list}})
      { $string.= "<!ELEMENT $gi ".$dtd->{model}->{$gi}.">\n" ;
        if( $dtd->{att}->{$gi})
          { my $attlist= $dtd->{att}->{$gi};
            $string.= "<!ATTLIST $gi\n";
            foreach my $att ( sort keys %{$attlist})
              { 
                if( $attlist->{$att}->{fixed})
                  { $string.= "   $att $attlist->{$att}->{type} #FIXED $attlist->{$att}->{default}"; }
                else
                  { $string.= "   $att $attlist->{$att}->{type} $attlist->{$att}->{default}"; }
                $string.= "\n";
              }
            $string.= ">\n";
          }
      }
    $string.= $t->entity_list->text if( $t->entity_list);
    $string.= "\n]>\n";
    return $string;
  }
        
# prints the DTD from the stored (possibly updated) data
sub dtd_print
  { my $t= shift;
    my $fh=  isa( $_[0], 'GLOB') || isa( $_[0], 'IO::Scalar')  ? shift : undef;
    if( $fh) { print $fh $t->dtd_text; }
    else     { print $t->dtd_text;     }
    return $t;
  }

# build the subs that call directly expat
BEGIN
  { my @expat_methods= qw( depth in_element within_element context
                           current_line current_column current_byte
                           recognized_string original_string 
                           xpcroak xpcarp 
                           base current_element element_index 
                           xml_escape
                           position_in_context);
    foreach my $method (@expat_methods)
      { 
        ## no critic (TestingAndDebugging::ProhibitNoStrict);
        no strict 'refs';
        *{$method}= sub { my $t= shift;
                          _croak( "calling $method after parsing is finished") unless( $t->{twig_parsing}); 
                          return $t->{twig_parser}->$method(@_); 
                        };
      }
  }

sub path
  { my( $t, $gi)= @_;
    if( $t->{twig_map_xmlns})
      { return "/" . join( "/", map { $t->_replace_prefix( $_)} ($t->{twig_parser}->context, $gi)); }
    else
      { return "/" . join( "/", ($t->{twig_parser}->context, $gi)); }
  }

sub finish
  { my $t= shift;
    return $t->{twig_parser}->finish;
  }

# just finish the parse by printing the rest of the document
sub finish_print
  { my( $t, $fh)= @_;
    my $old_fh;
    unless( defined $fh)
      { $t->_set_fh_to_twig_output_fh(); }
    elsif( defined $fh)
      { $old_fh= select $fh; 
        $t->{twig_original_selected_fh}= $old_fh if( $old_fh); 
      }
    
    my $p=$t->{twig_parser};
    if( $t->{twig_keep_encoding})
      { $p->setHandlers( %twig_handlers_finish_print); }
    else
      { $p->setHandlers( %twig_handlers_finish_print_original); }
    return $t;
  }

sub set_remove_cdata { return XML::Twig::Elt::set_remove_cdata( @_); }

sub output_filter          { return XML::Twig::Elt::output_filter( @_);          }
sub set_output_filter      { return XML::Twig::Elt::set_output_filter( @_);      }

sub output_text_filter     { return XML::Twig::Elt::output_text_filter( @_);     }
sub set_output_text_filter { return XML::Twig::Elt::set_output_text_filter( @_); }

sub set_input_filter
  { my( $t, $input_filter)= @_;
    my $old_filter= $t->{twig_input_filter};
      if( !$input_filter || isa( $input_filter, 'CODE') )
        { $t->{twig_input_filter}= $input_filter; }
      elsif( $input_filter eq 'latin1')
        {  $t->{twig_input_filter}= latin1(); }
      elsif( $filter{$input_filter})
        {  $t->{twig_input_filter}= $filter{$input_filter}; }
      else
        { _croak( "invalid input filter: $input_filter"); }
      
      return $old_filter;
    }

sub set_empty_tag_style
  { return XML::Twig::Elt::set_empty_tag_style( @_); }

sub set_pretty_print
  { return XML::Twig::Elt::set_pretty_print( @_); }

sub set_quote
  { return XML::Twig::Elt::set_quote( @_); }

sub set_indent
  { return XML::Twig::Elt::set_indent( @_); }

sub set_keep_atts_order
  { shift; return XML::Twig::Elt::set_keep_atts_order( @_); }

sub keep_atts_order
  { return XML::Twig::Elt::keep_atts_order( @_); }

sub set_do_not_escape_amp_in_atts
  { return XML::Twig::Elt::set_do_not_escape_amp_in_atts( @_); }

# save and restore package globals (the ones in XML::Twig::Elt)
# should probably return the XML::Twig object itself, but instead
# returns the state (as a hashref) for backward compatibility
sub save_global_state
  { my $t= shift;
    return $t->{twig_saved_state}= XML::Twig::Elt::global_state();
  }

sub restore_global_state
  { my $t= shift;
    XML::Twig::Elt::set_global_state( $t->{twig_saved_state});
  }

sub global_state
  { return XML::Twig::Elt::global_state(); }

sub set_global_state
  {  return XML::Twig::Elt::set_global_state( $_[1]); }

sub dispose
  { my $t= shift;
    $t->DESTROY;
    return;
  }
  
sub DESTROY
  { my $t= shift; 
    if( $t->{twig_root} && isa(  $t->{twig_root}, 'XML::Twig::Elt')) 
      { $t->{twig_root}->delete } 

    # added to break circular references
    undef $t->{twig};
    undef $t->{twig_root}->{twig} if( $t->{twig_root});
    undef $t->{twig_parser};
    
    undef %$t;# prevents memory leaks (especially when using mod_perl)
    undef $t;
  }        

# return true if perl was compiled using perlio
# if perl is not available return true, these days perlio should be used
sub _use_perlio
  { my $perl= _this_perl();
    return $perl ? grep /useperlio=define/, `$perl -V` : 1;
  }

# returns the parth to the perl executable (if available)
sub _this_perl
  { # straight from perlvar
    my $secure_perl_path= $Config{perlpath};
    if ($^O ne 'VMS') 
      { $secure_perl_path .= $Config{_exe} unless $secure_perl_path =~ m/$Config{_exe}$/i; }
    if( ! -f $secure_perl_path) { $secure_perl_path= ''; } # when perl is not available (PDK)
    return $secure_perl_path;
  }

#
#  non standard handlers
#

# kludge: expat 1.95.2 calls both Default AND Doctype handlers
# so if the default handler finds '<!DOCTYPE' then it must 
# unset itself (_twig_print_doctype will reset it)
sub _twig_print_check_doctype
   { # warn " in _twig_print_check_doctype...\n"; # DEBUG handler
    
    my $p= shift;
    my $string= $p->recognized_string();
    if( $string eq '<!DOCTYPE') 
      { 
        $p->setHandlers( Default => undef); 
        $p->setHandlers( Entity => undef); 
        $expat_1_95_2=1; 
      }
    else                        
      { print $string; }

    return;
  }


sub _twig_print
   { # warn " in _twig_print...\n"; # DEBUG handler
    my $p= shift;
    if( $expat_1_95_2 && ($p->recognized_string eq '[') && !$p->{twig}->{expat_1_95_2_seen_bracket})
      { # otherwise the opening square bracket of the doctype gets printed twice 
        $p->{twig}->{expat_1_95_2_seen_bracket}=1;
      }
    else
      { if( $p->{twig}->{twig_right_after_root})
          { my $s= $p->recognized_string(); print $s if $s=~ m{\S}; }
        else
          { print $p->recognized_string(); }
      }
    return;
  }
# recognized_string does not seem to work for entities, go figure!
# so this handler is used to print them anyway
sub _twig_print_entity
   { # warn " in _twig_print_entity...\n"; # DEBUG handler
    my $p= shift; 
    XML::Twig::Entity->new( @_)->print;
  }

# kludge: expat 1.95.2 calls both Default AND Doctype handlers
# so if the default handler finds '<!DOCTYPE' then it must 
# unset itself (_twig_print_doctype will reset it)
sub _twig_print_original_check_doctype
   { # warn " in _twig_print_original_check_doctype...\n"; # DEBUG handler
    
    my $p= shift;
    my $string= $p->original_string();
    if( $string eq '<!DOCTYPE') 
      { $p->setHandlers( Default => undef); 
        $p->setHandlers( Entity => undef); 
        $expat_1_95_2=1; 
      }
    else                        
      { print $string; }

    return;    
  }

sub _twig_print_original
   { # warn " in _twig_print_original...\n"; # DEBUG handler
    my $p= shift; 
    print $p->original_string();
    return;    
  }


sub _twig_print_original_doctype
   { # warn " in _twig_print_original_doctype...\n"; # DEBUG handler
    
    my(  $p, $name, $sysid, $pubid, $internal)= @_;
    if( $name)
      { # with recent versions of XML::Parser original_string does not work,
        # hence we need to rebuild the doctype declaration
        my $doctype='';
        $doctype .= qq{<!DOCTYPE $name}    if( $name);
        $doctype .=  qq{ PUBLIC  "$pubid"}  if( $pubid);
        $doctype .=  qq{ SYSTEM}            if( $sysid && !$pubid);
        $doctype .=  qq{ "$sysid"}          if( $sysid); 
        $doctype .=  ' [' if( $internal && !$expat_1_95_2) ;
        $doctype .=  qq{>} unless( $internal || $expat_1_95_2);
        $p->{twig}->{twig_doctype}->{has_internal}=$internal;
        print $doctype;
      }
    $p->setHandlers( Default => \&_twig_print_original);
    return;    
  }

sub _twig_print_doctype
   { # warn " in _twig_print_doctype...\n"; # DEBUG handler
    my(  $p, $name, $sysid, $pubid, $internal)= @_;
    if( $name)
      { # with recent versions of XML::Parser original_string does not work,
        # hence we need to rebuild the doctype declaration
        my $doctype='';
        $doctype .= qq{<!DOCTYPE $name}    if( $name);
        $doctype .=  qq{ PUBLIC  "$pubid"}  if( $pubid);
        $doctype .=  qq{ SYSTEM}            if( $sysid && !$pubid);
        $doctype .=  qq{ "$sysid"}          if( $sysid); 
        $doctype .=  ' [' if( $internal) ;
        $doctype .=  qq{>} unless( $internal || $expat_1_95_2);
        $p->{twig}->{twig_doctype}->{has_internal}=$internal;
        print $doctype;
      }
    $p->setHandlers( Default => \&_twig_print);
    return;    
  }


sub _twig_print_original_default
   { # warn " in _twig_print_original_default...\n"; # DEBUG handler
    my $p= shift;
    print $p->original_string();
    return;    
  }

# account for the case where the element is empty
sub _twig_print_end_original
   { # warn " in _twig_print_end_original...\n"; # DEBUG handler
    my $p= shift;
    print $p->original_string();
    return;    
  }

sub _twig_start_check_roots
   { # warn " in _twig_start_check_roots...\n"; # DEBUG handler
    my $p= shift;
    my $gi= shift;
    
    my $t= $p->{twig};
    
    my $fh= $t->{twig_output_fh} || select() || \*STDOUT;

    my $ns_decl;
    unless( $p->depth == 0)
      { if( $t->{twig_map_xmlns}) { $ns_decl= _replace_ns( $t, \$gi, \@_); }
      }

    my $context= { $ST_TAG => $gi, @_};
    $context->{$ST_NS}= $ns_decl if $ns_decl;
    push @{$t->{_twig_context_stack}}, $context;
    my %att= @_;

    if( _handler( $t, $t->{twig_roots}, $gi))
      { $p->setHandlers( %twig_handlers); # restore regular handlers
        $t->{twig_root_depth}= $p->depth; 
        pop @{$t->{_twig_context_stack}}; # will be pushed back in _twig_start
        _twig_start( $p, $gi, @_);
        return;
      }

    # $tag will always be true if it needs to be printed (the tag string is never empty)
    my $tag= $t->{twig_default_print} ? $t->{twig_keep_encoding} ? $p->original_string
                                                                 : $p->recognized_string
                                      : '';

    if( $p->depth == 0)
      { 
        ## no critic (TestingAndDebugging::ProhibitNoStrict);
        no strict 'refs';
        print {$fh} $tag if( $tag);
        pop @{$t->{_twig_context_stack}}; # will be pushed back in _twig_start
        _twig_start( $p, $gi, @_);
        $t->root->{'flushed'}=1; # or the root start tag gets output the first time we flush
      }
    elsif( $t->{twig_starttag_handlers})
      { # look for start tag handlers

        my @handlers= _handler( $t, $t->{twig_starttag_handlers}, $gi);
        my $last_handler_res;
        foreach my $handler ( @handlers)
          { $last_handler_res= $handler->($t, $gi, %att);
            last unless $last_handler_res;
          }
        ## no critic (TestingAndDebugging::ProhibitNoStrict);
        no strict 'refs';
        print {$fh} $tag if( $tag && (!@handlers || $last_handler_res));   
      }
    else
      { 
        ## no critic (TestingAndDebugging::ProhibitNoStrict);
        no strict 'refs';
        print {$fh} $tag if( $tag); 
      }  
    return;    
  }

sub _twig_end_check_roots
   { # warn " in _twig_end_check_roots...\n"; # DEBUG handler
    
    my( $p, $gi, %att)= @_;
    my $t= $p->{twig};
    # $tag can be empty (<elt/>), hence the undef and the tests for defined
    my $tag= $t->{twig_default_print} ? $t->{twig_keep_encoding} ? $p->original_string
                                                                 : $p->recognized_string
                                      : undef;
    my $fh= $t->{twig_output_fh} || select() || \*STDOUT;
    
    if( $t->{twig_endtag_handlers})
      { # look for end tag handlers
        my @handlers= _handler( $t, $t->{twig_endtag_handlers}, $gi);
        my $last_handler_res=1;
        foreach my $handler ( @handlers)
          { $last_handler_res= $handler->($t, $gi) || last; }
        #if( ! $last_handler_res) 
        #  { pop @{$t->{_twig_context_stack}}; warn "tested";
        #    return;
        #  }
      }
    {
      ## no critic (TestingAndDebugging::ProhibitNoStrict);
      no strict 'refs';
      print {$fh} $tag if( defined $tag);
    }
    if( $p->depth == 0)
      { 
        _twig_end( $p, $gi);  
        $t->root->{end_tag_flushed}=1;
      }

    pop @{$t->{_twig_context_stack}};
    return;    
  }

sub _twig_pi_check_roots
   { # warn " in _twig_pi_check_roots...\n"; # DEBUG handler
    my( $p, $target, $data)= @_;
    my $t= $p->{twig};
    my $pi= $t->{twig_default_print} ? $t->{twig_keep_encoding} ? $p->original_string
                                                                : $p->recognized_string
                                    : undef;
    my $fh= $t->{twig_output_fh} || select() || \*STDOUT;
    
    if( my $handler=    $t->{twig_handlers}->{pi_handlers}->{$target}
                     || $t->{twig_handlers}->{pi_handlers}->{''}
      )
      { # if handler is called on pi, then it needs to be processed as a regular node
        my @flags= qw( twig_process_pi twig_keep_pi);
        my @save= @{$t}{@flags}; # save pi related flags
        @{$t}{@flags}= (1, 0);   # override them, pi needs to be processed
        _twig_pi( @_);           # call handler on the pi
        @{$t}{@flags}= @save;;   # restore flag
      }
    else
      { 
        ## no critic (TestingAndDebugging::ProhibitNoStrict);
        no strict 'refs';
        print  {$fh} $pi if( defined( $pi));
      }
    return;    
  }


sub _output_ignored
  { my( $t, $p)= @_;
    my $action= $t->{twig_ignore_action};

    my $get_string= $t->{twig_keep_encoding} ? 'original_string' : 'recognized_string';

    if( $action eq 'print' ) { print $p->$get_string; }
    else
      { my $string_ref;
        if( $action eq 'string') 
          { $string_ref= \$t->{twig_buffered_string}; }
        elsif( ref( $action) && ref( $action) eq 'SCALAR')
          { $string_ref= $action; }
        else
          { _croak( "wrong ignore action: $action"); }

        $$string_ref .= $p->$get_string;
      }
  }
     
        

sub _twig_ignore_start
   { # warn " in _twig_ignore_start...\n"; # DEBUG handler
    
    my( $p, $gi)= @_;
    my $t= $p->{twig};
    $t->{twig_ignore_level}++;
    my $action= $t->{twig_ignore_action}; 

    $t->_output_ignored( $p) unless $action eq 'discard';
    return;    
  }

sub _twig_ignore_end
   { # warn " in _twig_ignore_end...\n"; # DEBUG handler
    
    my( $p, $gi)= @_;
    my $t= $p->{twig};

    my $action= $t->{twig_ignore_action};
    $t->_output_ignored( $p) unless $action eq 'discard';

    $t->{twig_ignore_level}--;

    if( ! $t->{twig_ignore_level})
      { 
        $t->{twig_current}   = $t->{twig_ignore_elt};
        $t->{twig_current}->{'twig_current'}=1;

        $t->{twig_ignore_elt}->cut;  # there could possibly be a memory leak here (delete would avoid it,
                                     # but could also delete elements that should not be deleted)

        # restore the saved stack to the current level
        splice( @{$t->{_twig_context_stack}}, $p->depth+ 1 );
        #warn "stack: ", _dump_stack( $t->{_twig_context_stack}), "\n";

        $p->setHandlers( @{$t->{twig_saved_handlers}});
        # test for handlers
        if( $t->{twig_endtag_handlers})
          { # look for end tag handlers
            my @handlers= _handler( $t, $t->{twig_endtag_handlers}, $gi);
            my $last_handler_res=1;
            foreach my $handler ( @handlers)
              { $last_handler_res= $handler->($t, $gi) || last; }
          }
        pop @{$t->{_twig_context_stack}};
      };
    return;    
  }

#sub _dump_stack { my( $stack)= @_; return join( ":", map { $_->{$ST_TAG} } @$stack); }
    
sub ignore
  { my( $t, $elt, $action)= @_;
    my $current= $t->{twig_current};

    if( ! ($elt && ref( $elt) && isa( $elt, 'XML::Twig::Elt'))) { $elt= $current; }

    #warn "ignore:  current = ", $current->tag, ", elt = ", $elt->tag, ")\n";

    # we need the ($elt == $current->{last_child}) test because the current element is set to the
    # parent _before_ handlers are called (and I can't figure out how to fix this)
    unless( ($elt == $current) || ($current->{last_child} && ($elt == $current->{last_child})) || $current->in( $elt)) 
      { _croak( "element to be ignored must be ancestor of current element"); }

    $t->{twig_ignore_level}= $current == $elt ? 1 : $t->_level_in_stack( $current) - $t->_level_in_stack($elt) + 1;
    #warn "twig_ignore_level:  $t->{twig_ignore_level} (current: ", $current->tag, ", elt: ", $elt->tag, ")\n";
    $t->{twig_ignore_elt}  = $elt;     # save it, so we can delete it later

    $action ||= 'discard'; 
    if( !($action eq 'print' || $action eq 'string' || ( ref( $action) && ref( $action) eq 'SCALAR')))
      { $action= 'discard'; }
   
    $t->{twig_ignore_action}= $action;

    my $p= $t->{twig_parser};
    my @saved_handlers= $p->setHandlers( %twig_handlers_ignore); # set handlers
   
    my $get_string= $t->{twig_keep_encoding} ? 'original_string' : 'recognized_string';

    my $default_handler;

    if( $action ne 'discard')
      { if( $action eq 'print')
          { $p->setHandlers( Default => sub { print $_[0]->$get_string; }); }
        else
          { my $string_ref;
            if( $action eq 'string') 
              { if( ! exists $t->{twig_buffered_string}) { $t->{twig_buffered_string}=''; }
                $string_ref= \$t->{twig_buffered_string}; 
              }
            elsif( ref( $action) && ref( $action) eq 'SCALAR')
              { $string_ref= $action; }
    
            $p->setHandlers( Default =>  sub { $$string_ref .= $_[0]->$get_string; });
          }
        $t->_output_ignored( $p, $action);
      }


    $t->{twig_saved_handlers}= \@saved_handlers;        # save current handlers
  }

sub _level_in_stack
  { my( $t, $elt)= @_;
    my $level=1;
    foreach my $elt_in_stack ( @{$t->{_twig_context_stack}} )
      { if( $elt_in_stack->{$ST_ELT} && ($elt == $elt_in_stack->{$ST_ELT})) { return $level }
        $level++;
      }
  }



# select $t->{twig_output_fh} and store the current selected fh 
sub _set_fh_to_twig_output_fh
  { my $t= shift;
    my $output_fh= $t->{twig_output_fh};
    if( $output_fh && !$t->{twig_output_fh_selected})
      { # there is an output fh
        $t->{twig_selected_fh}= select(); # store the currently selected fh
        $t->{twig_output_fh_selected}=1;
        select $output_fh;                # select the output fh for the twig
      }
  }

# select the fh that was stored in $t->{twig_selected_fh} 
# (before $t->{twig_output_fh} was selected)
sub _set_fh_to_selected_fh
  { my $t= shift;
    return unless( $t->{twig_output_fh});
    my $selected_fh= $t->{twig_selected_fh};
    $t->{twig_output_fh_selected}=0;
    select $selected_fh;
    return;
  }
  

sub encoding
  { return $_[0]->{twig_xmldecl}->{encoding} if( $_[0]->{twig_xmldecl}); }

sub set_encoding
  { my( $t, $encoding)= @_;
    $t->{twig_xmldecl} ||={};
    $t->set_xml_version( "1.0") unless( $t->xml_version);
    $t->{twig_xmldecl}->{encoding}= $encoding;
    return $t;
  }

sub output_encoding
  { return $_[0]->{output_encoding}; }
  
sub set_output_encoding
  { my( $t, $encoding)= @_;
    my $output_filter= $t->output_filter || '';

    if( ($encoding && $encoding !~ m{^utf-?8$}i) || $t->{twig_keep_encoding} || $output_filter)
      { $t->set_output_filter( _encoding_filter( $encoding || '')); }

    $t->{output_encoding}= $encoding;
    return $t;
  }

sub xml_version
  { return $_[0]->{twig_xmldecl}->{version} if( $_[0]->{twig_xmldecl}); }

sub set_xml_version
  { my( $t, $version)= @_;
    $t->{twig_xmldecl} ||={};
    $t->{twig_xmldecl}->{version}= $version;
    return $t;
  }

sub standalone
  { return $_[0]->{twig_xmldecl}->{standalone} if( $_[0]->{twig_xmldecl}); }

sub set_standalone
  { my( $t, $standalone)= @_;
    $t->{twig_xmldecl} ||={};
    $t->set_xml_version( "1.0") unless( $t->xml_version);
    $t->{twig_xmldecl}->{standalone}= $standalone;
    return $t;
  }


# SAX methods

sub toSAX1
  { _croak( "cannot use toSAX1 while parsing (use flush_toSAX1)") if (defined $_[0]->{twig_parser});
    shift(@_)->_toSAX(@_, \&XML::Twig::Elt::_start_tag_data_SAX1,
                          \&XML::Twig::Elt::_end_tag_data_SAX1
             ); 
  }

sub toSAX2
  { _croak( "cannot use toSAX2 while parsing (use flush_toSAX2)") if (defined $_[0]->{twig_parser});
    shift(@_)->_toSAX(@_, \&XML::Twig::Elt::_start_tag_data_SAX2,
                          \&XML::Twig::Elt::_end_tag_data_SAX2
             ); 
  }


sub _toSAX
  { my( $t, $handler, $start_tag_data, $end_tag_data) = @_;

    if( my $start_document =  $handler->can( 'start_document'))
      { $start_document->( $handler); }
    
    $t->_prolog_toSAX( $handler);
    
    if( $t->root) { $t->root->_toSAX( $handler, $start_tag_data, $end_tag_data) ; }
    if( my $end_document =  $handler->can( 'end_document'))
      { $end_document->( $handler); }
  }


sub flush_toSAX1
  { shift(@_)->_flush_toSAX(@_, \&XML::Twig::Elt::_start_tag_data_SAX1,
                               \&XML::Twig::Elt::_end_tag_data_SAX1
             ); 
  }

sub flush_toSAX2
  { shift(@_)->_flush_toSAX(@_, \&XML::Twig::Elt::_start_tag_data_SAX2,
                               \&XML::Twig::Elt::_end_tag_data_SAX2
             ); 
  }

sub _flush_toSAX
  { my( $t, $handler, $start_tag_data, $end_tag_data)= @_;

    # the "real" last element processed, as _twig_end has closed it
    my $last_elt;
    if( $t->{twig_current})
      { $last_elt= $t->{twig_current}->{last_child}; }
    else
      { $last_elt= $t->{twig_root}; }

    my $elt= $t->{twig_root};
    unless( $elt->{'flushed'})
      { # init unless already done (ie root has been flushed)
        if( my $start_document =  $handler->can( 'start_document'))
          { $start_document->( $handler); }
        # flush the DTD
        $t->_prolog_toSAX( $handler) 
      }

    while( $elt)
      { my $next_elt; 
        if( $last_elt && $last_elt->in( $elt))
          { 
            unless( $elt->{'flushed'}) 
              { # just output the front tag
                if( my $start_element = $handler->can( 'start_element'))
                 { if( my $tag_data= $start_tag_data->( $elt))
                     { $start_element->( $handler, $tag_data); }
                 }
                $elt->{'flushed'}=1;
              }
            $next_elt= $elt->{first_child};
          }
        else
          { # an element before the last one or the last one,
            $next_elt= $elt->{next_sibling};  
            $elt->_toSAX( $handler, $start_tag_data, $end_tag_data);
            $elt->delete; 
            last if( $last_elt && ($elt == $last_elt));
          }
        $elt= $next_elt;
      }
    if( !$t->{twig_parsing}) 
      { if( my $end_document =  $handler->can( 'end_document'))
          { $end_document->( $handler); }
      }
  }


sub _prolog_toSAX
  { my( $t, $handler)= @_;
    $t->_xmldecl_toSAX( $handler);
    $t->_DTD_toSAX( $handler);
  }

sub _xmldecl_toSAX
  { my( $t, $handler)= @_;
    my $decl= $t->{twig_xmldecl};
    my $data= { Version    => $decl->{version},
                Encoding   => $decl->{encoding},
                Standalone => $decl->{standalone},
          };
    if( my $xml_decl= $handler->can( 'xml_decl'))
      { $xml_decl->( $handler, $data); }
  }
                
sub _DTD_toSAX
  { my( $t, $handler)= @_;
    my $doctype= $t->{twig_doctype};
    return unless( $doctype);
    my $data= { Name     => $doctype->{name},
                PublicId => $doctype->{pub},
                SystemId => $doctype->{sysid},
              };

    if( my $start_dtd= $handler->can( 'start_dtd'))
      { $start_dtd->( $handler, $data); }

    # I should call code to export the internal subset here 
    
    if( my $end_dtd= $handler->can( 'end_dtd'))
      { $end_dtd->( $handler); }
  }

# input/output filters

sub latin1 
  { local $SIG{__DIE__};
    if( _use(  'Encode'))
      { return encode_convert( 'ISO-8859-15'); }
    elsif( _use( 'Text::Iconv'))
      { return iconv_convert( 'ISO-8859-15'); }
    elsif( _use( 'Unicode::Map8') && _use( 'Unicode::String'))
      { return unicode_convert( 'ISO-8859-15'); }
    else
      { return \&regexp2latin1; }
  }

sub _encoding_filter
  { 
      { local $SIG{__DIE__};
        my $encoding= $_[1] || $_[0];
        if( _use( 'Encode'))
          { my $sub= encode_convert( $encoding);
            return $sub;
          }
        elsif( _use( 'Text::Iconv'))
          { return iconv_convert( $encoding); }
        elsif( _use( 'Unicode::Map8') && _use( 'Unicode::String'))
          { return unicode_convert( $encoding); }
        }
    _croak( "Encode, Text::Iconv or Unicode::Map8 and Unicode::String need to be installed in order to use encoding options");
  }

# shamelessly lifted from XML::TyePYX (works only with XML::Parse 2.27)
sub regexp2latin1
  { my $text=shift;
    $text=~s{([\xc0-\xc3])(.)}{ my $hi = ord($1);
                                my $lo = ord($2);
                                chr((($hi & 0x03) <<6) | ($lo & 0x3F))
                              }ge;
    return $text;
  }


sub html_encode
  { _use( 'HTML::Entities') or croak "cannot use html_encode: missing HTML::Entities";
    return HTML::Entities::encode_entities($_[0] );
  }

sub safe_encode
  {   my $str= shift;
      if( $perl_version < 5.008)
        { # the no utf8 makes the regexp work in 5.6
          no utf8; # = perl 5.6
          $str =~ s{([\xC0-\xDF].|[\xE0-\xEF]..|[\xF0-\xFF]...)}
                   {_XmlUtf8Decode($1)}egs; 
        }
      else
        { $str= encode( ascii => $str, $FB_HTMLCREF); }
      return $str;
  }

sub safe_encode_hex
  {   my $str= shift;
      if( $perl_version < 5.008)
        { # the no utf8 makes the regexp work in 5.6
          no utf8; # = perl 5.6
          $str =~ s{([\xC0-\xDF].|[\xE0-\xEF]..|[\xF0-\xFF]...)}
                   {_XmlUtf8Decode($1, 1)}egs; 
        }
      else
        { $str= encode( ascii => $str, $FB_XMLCREF); }
      return $str;
  }

# this one shamelessly lifted from XML::DOM
# does NOT work on 5.8.0
sub _XmlUtf8Decode
  { my ($str, $hex) = @_;
    my $len = length ($str);
    my $n;

    if ($len == 2)
      { my @n = unpack "C2", $str;
        $n = (($n[0] & 0x3f) << 6) + ($n[1] & 0x3f);
      }
    elsif ($len == 3)
      { my @n = unpack "C3", $str;
        $n = (($n[0] & 0x1f) << 12) + (($n[1] & 0x3f) << 6) + ($n[2] & 0x3f);
      }
    elsif ($len == 4)
      { my @n = unpack "C4", $str;
        $n = (($n[0] & 0x0f) << 18) + (($n[1] & 0x3f) << 12) 
           + (($n[2] & 0x3f) << 6) + ($n[3] & 0x3f);
      }
    elsif ($len == 1)    # just to be complete...
      { $n = ord ($str); }
    else
      { croak "bad value [$str] for _XmlUtf8Decode"; }

    my $char= $hex ? sprintf ("&#x%x;", $n) : "&#$n;";
    return $char;
  }


sub unicode_convert
  { my $enc= $_[1] ? $_[1] : $_[0]; # so the method can be called on the twig or directly
    _use( 'Unicode::Map8') or croak "Unicode::Map8 not available, needed for encoding filter: $!";
    _use( 'Unicode::String') or croak "Unicode::String not available, needed for encoding filter: $!";
    import Unicode::String qw(utf8);
    my $sub= eval qq{ { $NO_WARNINGS;
                        my \$cnv;
                        BEGIN {  \$cnv= Unicode::Map8->new(\$enc) 
                                     or croak "Can't create converter to \$enc";
                              }
                        sub { return  \$cnv->to8 (utf8(\$_[0])->ucs2); } 
                      } 
                    };
    unless( $sub) { croak $@; }
    return $sub;
  }

sub iconv_convert
  { my $enc= $_[1] ? $_[1] : $_[0]; # so the method can be called on the twig or directly
    _use( 'Text::Iconv') or croak "Text::Iconv not available, needed for encoding filter: $!";
    my $sub= eval qq{ { $NO_WARNINGS;
                        my \$cnv;
                        BEGIN { \$cnv = Text::Iconv->new( 'utf8', \$enc) 
                                     or croak "Can't create iconv converter to \$enc";
                              }
                        sub { return  \$cnv->convert( \$_[0]); } 
                      }       
                    };
    unless( $sub)
      { if( $@=~ m{^Unsupported conversion: Invalid argument})
          { croak "Unsupported encoding: $enc"; }
        else
          { croak $@; }
      }

    return $sub;
  }

sub encode_convert
  { my $enc= $_[1] ? $_[1] : $_[0]; # so the method can be called on the twig or directly
    my $sub=  eval qq{sub { $NO_WARNINGS; return encode( "$enc", \$_[0]); } };
    croak "can't create Encode-based filter: $@" unless( $sub);
    return $sub;
  }


# XML::XPath compatibility
sub getRootNode        { return $_[0]; }
sub getParentNode      { return undef; }
sub getChildNodes      { my @children= ($_[0]->root); return wantarray ? @children : \@children; }

sub _weakrefs     { return $weakrefs;       }
sub _set_weakrefs { $weakrefs=shift() || 0; XML::Twig::Elt::set_destroy()if ! $weakrefs; } # for testing purposes

sub _dump
  { my $t= shift;
    my $dump='';

    $dump="document\n"; # should dump twig level data here
    if( $t->root) { $dump .= $t->root->_dump( @_); }

    return $dump;
    
  }


1;

######################################################################
package XML::Twig::Entity_list;
######################################################################

*isa= *UNIVERSAL::isa;

sub new
  { my $class = shift;
    my $self={ entities => {}, updated => 0};

    bless $self, $class;
    return $self;

  }

sub add_new_ent
  { my $ent_list= shift;
    my $ent= XML::Twig::Entity->new( @_);
    $ent_list->add( $ent);
    return $ent_list;
  }

sub _add_list
  { my( $ent_list, $to_add)= @_;
    my $ents_to_add= $to_add->{entities};
    return $ent_list unless( $ents_to_add && %$ents_to_add);
    @{$ent_list->{entities}}{keys %$ents_to_add}= values %$ents_to_add;
    $ent_list->{updated}=1;
    return $ent_list;
  }

sub add
  { my( $ent_list, $ent)= @_;
    $ent_list->{entities}->{$ent->{name}}= $ent;
    $ent_list->{updated}=1;
    return $ent_list;
  }

sub ent
  { my( $ent_list, $ent_name)= @_;
    return $ent_list->{entities}->{$ent_name};
  }

# can be called with an entity or with an entity name
sub delete
  { my $ent_list= shift;
    if( isa( ref $_[0], 'XML::Twig::Entity'))
      { # the second arg is an entity
        my $ent= shift;
        delete $ent_list->{entities}->{$ent->{name}};
      }
    else
      { # the second arg was not entity, must be a string then
        my $name= shift;
        delete $ent_list->{entities}->{$name};
      }
    $ent_list->{updated}=1;
    return $ent_list;
  }

sub print
  { my ($ent_list, $fh)= @_;
    my $old_select= defined $fh ? select $fh : undef;

    foreach my $ent_name ( sort keys %{$ent_list->{entities}})
      { my $ent= $ent_list->{entities}->{$ent_name};
        # we have to test what the entity is or un-defined entities can creep in
        if( isa( $ent, 'XML::Twig::Entity')) { $ent->print(); }
      }
    select $old_select if( defined $old_select);
    return $ent_list;
  }

sub text
  { my ($ent_list)= @_;
    return join "\n", map { $ent_list->{entities}->{$_}->text} sort keys %{$ent_list->{entities}};
  }

# return the list of entity names 
sub entity_names
  { my $ent_list= shift;
    return (sort keys %{$ent_list->{entities}}) ;
  }


sub list
  { my ($ent_list)= @_;
    return map { $ent_list->{entities}->{$_} } sort keys %{$ent_list->{entities}};
  }

1;

######################################################################
package XML::Twig::Entity;
######################################################################

#*isa= *UNIVERSAL::isa;

sub new
  { my( $class, $name, $val, $sysid, $pubid, $ndata, $param)= @_;
    $class= ref( $class) || $class;

    my $self={};
    
    $self->{name}  = $name;
    $self->{val}   = $val   if( defined $val  );
    $self->{sysid} = $sysid if( defined $sysid);
    $self->{pubid} = $pubid if( defined $pubid);
    $self->{ndata} = $ndata if( defined $ndata);
    $self->{param} = $param if( defined $param);

    bless $self, $class;
    return $self;
  }


sub name  { return $_[0]->{name}; }
sub val   { return $_[0]->{val}; }
sub sysid { return defined( $_[0]->{sysid}) ? $_[0]->{sysid} : ''; }
sub pubid { return defined( $_[0]->{pubid}) ? $_[0]->{pubid} : ''; }
sub ndata { return defined( $_[0]->{ndata}) ? $_[0]->{ndata} : ''; }
sub param { return defined( $_[0]->{param}) ? $_[0]->{param} : ''; }


sub print
  { my ($ent, $fh)= @_;
    my $text= $ent->text;
    if( $fh) { print $fh $text . "\n"; }
    else     { print $text . "\n"; }
  }

sub sprint
  { my ($ent)= @_;
    return $ent->text;
  }

sub text
  { my ($ent)= @_;
    #warn "text called: '", $ent->_dump, "'\n";
    return '' if( !$ent->{name});
    my @tokens;
    push @tokens, '<!ENTITY';
   
    push @tokens, '%' if( $ent->{param});
    push @tokens, $ent->{name};

    if( defined $ent->{val} && !defined( $ent->{sysid}) && !defined($ent->{pubid}) )
      { push @tokens, _quoted_val( $ent->{val});
      }
    elsif( defined $ent->{sysid})
      { push @tokens, 'PUBLIC', _quoted_val( $ent->{pubid}) if( $ent->{pubid});
        push @tokens, 'SYSTEM' unless( $ent->{pubid});
        push @tokens, _quoted_val( $ent->{sysid}); 
        push @tokens, 'NDATA', $ent->{ndata} if( $ent->{ndata});
      }
    return join( ' ', @tokens) . '>';
  }

sub _quoted_val
  { my $q= $_[0]=~ m{"} ? q{'} : q{"};
    return qq{$q$_[0]$q};
  }

sub _dump
  { my( $ent)= @_; return join( " - ", map { "$_ => '$ent->{$_}'" } grep { defined $ent->{$_} } sort keys %$ent); }
                
1;

######################################################################
package XML::Twig::Notation_list;
######################################################################

*isa= *UNIVERSAL::isa;

sub new
  { my $class = shift;
    my $self={ notations => {}, updated => 0};

    bless $self, $class;
    return $self;

  }

sub add_new_notation
  { my $notation_list= shift;
    my $notation= XML::Twig::Notation->new( @_);
    $notation_list->add( $notation);
    return $notation_list;
  }

sub _add_list
  { my( $notation_list, $to_add)= @_;
    my $notations_to_add= $to_add->{notations};
    return $notation_list unless( $notations_to_add && %$notations_to_add);
    @{$notation_list->{notations}}{keys %$notations_to_add}= values %$notations_to_add;
    $notation_list->{updated}=1;
    return $notation_list;
  }

sub add
  { my( $notation_list, $notation)= @_;
    $notation_list->{notations}->{$notation->{name}}= $notation;
    $notation_list->{updated}=1;
    return $notation_list;
  }

sub notation
  { my( $notation_list, $notation_name)= @_;
    return $notation_list->{notations}->{$notation_name};
  }

# can be called with an notation or with an notation name
sub delete
  { my $notation_list= shift;
    if( isa( ref $_[0], 'XML::Twig::Notation'))
      { # the second arg is an notation
        my $notation= shift;
        delete $notation_list->{notations}->{$notation->{name}};
      }
    else
      { # the second arg was not notation, must be a string then
        my $name= shift;
        delete $notation_list->{notations}->{$name};
      }
    $notation_list->{updated}=1;
    return $notation_list;
  }

sub print
  { my ($notation_list, $fh)= @_;
    my $old_select= defined $fh ? select $fh : undef;

    foreach my $notation_name ( sort keys %{$notation_list->{notations}})
      { my $notation= $notation_list->{notations}->{$notation_name};
        # we have to test what the notation is or un-defined notations can creep in
        if( isa( $notation, 'XML::Twig::Notation')) { $notation->print(); }
      }
    select $old_select if( defined $old_select);
    return $notation_list;
  }

sub text
  { my ($notation_list)= @_;
    return join "\n", map { $notation_list->{notations}->{$_}->text} sort keys %{$notation_list->{notations}};
  }

# return the list of notation names 
sub notation_names
  { my $notation_list= shift;
    return (sort keys %{$notation_list->{notations}}) ;
  }


sub list
  { my ($notation_list)= @_;
    return map { $notation_list->{notations}->{$_} } sort keys %{$notation_list->{notations}};
  }

1;

######################################################################
package XML::Twig::Notation;
######################################################################

#*isa= *UNIVERSAL::isa;

BEGIN 
  { *sprint= *text;
  }

sub new
  { my( $class, $name, $base, $sysid, $pubid)= @_;
    $class= ref( $class) || $class;

    my $self={};
    
    $self->{name}  = $name;
    $self->{base}  = $base  if( defined $base  );
    $self->{sysid} = $sysid if( defined $sysid);
    $self->{pubid} = $pubid if( defined $pubid);

    bless $self, $class;
    return $self;
  }


sub name  { return $_[0]->{name};  }
sub base  { return $_[0]->{base};  }
sub sysid { return $_[0]->{sysid}; }
sub pubid { return $_[0]->{pubid}; }


sub print
  { my ($notation, $fh)= @_;
    my $text= $notation->text;
    if( $fh) { print $fh $text . "\n"; }
    else     { print $text . "\n"; }
  }

sub text
  { my ($notation)= @_;
    return '' if( !$notation->{name});
    my @tokens;
    push @tokens, '<!NOTATION';
    push @tokens, $notation->{name};
    push @tokens, ( 'PUBLIC', _quoted_val( $notation->{pubid} ) ) if $notation->{pubid};
    push @tokens, ( 'SYSTEM')                                     if ! $notation->{pubid} && $notation->{sysid};
    push @tokens, (_quoted_val( $notation->{sysid}) )             if $notation->{sysid};
    
    return join( ' ', @tokens) . '>';
  }

sub _quoted_val
  { my $q= $_[0]=~ m{"} ? q{'} : q{"};
    return qq{$q$_[0]$q};
  }

sub _dump
  { my( $notation)= @_; return join( " - ", map { "$_ => '$notation->{$_}'" } grep { defined $notation->{$_} } sort keys %$notation); }
                
1;

######################################################################
package XML::Twig::Elt;
######################################################################

use Carp;
*isa= *UNIVERSAL::isa;

my $CDATA_START    = "<![CDATA[";
my $CDATA_END      = "]]>";
my $PI_START       = "<?";
my $PI_END         = "?>";
my $COMMENT_START  = "<!--";
my $COMMENT_END    = "-->";

my $XMLNS_URI      = 'http://www.w3.org/2000/xmlns/';


BEGIN
  { # set some aliases for methods
    *tag           = *gi; 
    *name          = *gi; 
    *set_tag       = *set_gi; 
    *set_name      = *set_gi; 
    *find_nodes    = *get_xpath; # as in XML::DOM
    *findnodes     = *get_xpath; # as in XML::LibXML
    *field         = *first_child_text;
    *trimmed_field = *first_child_trimmed_text;
    *is_field      = *contains_only_text;
    *is            = *passes;
    *matches       = *passes;
    *has_child     = *first_child;
    *has_children  = *first_child;
    *all_children_pass = *all_children_are;
    *all_children_match= *all_children_are;
    *getElementsByTagName= *descendants;
    *find_by_tag_name= *descendants_or_self;
    *unwrap          = *erase;
    *inner_xml       = *xml_string;
    *outer_xml       = *sprint;
    *add_class       = *add_to_class;
  
    *first_child_is  = *first_child_matches;
    *last_child_is   = *last_child_matches;
    *next_sibling_is = *next_sibling_matches;
    *prev_sibling_is = *prev_sibling_matches;
    *next_elt_is     = *next_elt_matches;
    *prev_elt_is     = *prev_elt_matches;
    *parent_is       = *parent_matches;
    *child_is        = *child_matches;
    *inherited_att   = *inherit_att;

    *sort_children_by_value= *sort_children_on_value;

    *has_atts= *att_nb;

    # imports from XML::Twig
    *_is_fh= *XML::Twig::_is_fh;

    # XML::XPath compatibility
    *string_value       = *text;
    *toString           = *sprint;
    *getName            = *gi;
    *getRootNode        = *twig;  
    *getNextSibling     = *_next_sibling;
    *getPreviousSibling = *_prev_sibling;
    *isElementNode      = *is_elt;
    *isTextNode         = *is_text;
    *isPI               = *is_pi;
    *isPINode           = *is_pi;
    *isProcessingInstructionNode= *is_pi;
    *isComment          = *is_comment;
    *isCommentNode      = *is_comment;
    *getTarget          = *target;
    *getFirstChild      = *_first_child;
    *getLastChild      = *_last_child;

    # try using weak references
    # test whether we can use weak references
    { local $SIG{__DIE__};
      if( eval 'require Scalar::Util' && defined( &Scalar::Util::weaken) )
        { import Scalar::Util qw(weaken); }
      elsif( eval 'require WeakRef')
        { import WeakRef; }
    }
}

 
# can be called as XML::Twig::Elt->new( [[$gi, $atts, [@content]])
# - gi is an optional gi given to the element
# - $atts is a hashref to attributes for the element
# - @content is an optional list of text and elements that will
#   be inserted under the element 
sub new 
  { my $class= shift;
    $class= ref $class || $class;
    my $elt  = {};
    bless ($elt, $class);

    return $elt unless @_;

    if( @_ == 1 && $_[0]=~ m{^\s*<}) { return $class->parse( @_); }

    # if a gi is passed then use it
    my $gi= shift;
    $elt->{gi}=$XML::Twig::gi2index{$gi} or $elt->set_gi( $gi);


    my $atts= ref $_[0] eq 'HASH' ? shift : undef;

    if( $atts && defined $atts->{$CDATA})
      { delete $atts->{$CDATA};

        my $cdata= $class->new( $CDATA => @_);
        return $class->new( $gi, $atts, $cdata);
      }

    if( $gi eq $PCDATA)
      { if( grep { ref $_ } @_) { croak "element $PCDATA can only be created from text"; }
        $elt->{pcdata}=  join '', @_; 
      }
    elsif( $gi eq $ENT)
      { $elt->{ent}=  shift; }
    elsif( $gi eq $CDATA)
      { if( grep { ref $_ } @_) { croak "element $CDATA can only be created from text"; }
        $elt->{cdata}=  join '', @_; 
      }
    elsif( $gi eq $COMMENT)
      { if( grep { ref $_ } @_) { croak "element $COMMENT can only be created from text"; }
        $elt->{comment}=  join '', @_; 
      }
    elsif( $gi eq $PI)
      { if( grep { ref $_ } @_) { croak "element $PI can only be created from text"; }
        $elt->_set_pi( shift, join '', @_);
      }
    else
      { # the rest of the arguments are the content of the element
        if( @_)
          { $elt->set_content( @_); }
        else
          { $elt->{empty}=  1;    }
      }

    if( $atts)
      { # the attribute hash can be used to pass the asis status 
        if( defined $atts->{$ASIS})  { $elt->set_asis(  $atts->{$ASIS} ); delete $atts->{$ASIS};  }
        if( defined $atts->{$EMPTY}) { $elt->{empty}=  $atts->{$EMPTY}; delete $atts->{$EMPTY}; }
        if( keys %$atts) { $elt->set_atts( $atts); }
        $elt->_set_id( $atts->{$ID}) if( $atts->{$ID});
      }

    return $elt;
  }

# optimized version of $elt->new( PCDATA, $text);
sub _new_pcdata
  { my $class= $_[0];
    $class= ref $class || $class;
    my $elt  = {};
    bless $elt, $class;
    $elt->{gi}=$XML::Twig::gi2index{$PCDATA} or $elt->set_gi( $PCDATA);
    $elt->{pcdata}=  $_[1];
    return $elt;
  }
    
# this function creates an XM:::Twig::Elt from a string
# it is quite clumsy at the moment, as it just creates a
# new twig then returns its root
# there might also be memory leaks there
# additional arguments are passed to new XML::Twig
sub parse
  { my $class= shift;
    if( ref( $class)) { $class= ref( $class); }
    my $string= shift;
    my %args= @_;
    my $t= XML::Twig->new(%args);
    $t->parse( $string);
    my $elt= $t->root;
    # clean-up the node 
    delete $elt->{twig};         # get rid of the twig data
    delete $elt->{twig_current}; # better get rid of this too
    if( $t->{twig_id_list}) { $elt->{twig_id_list}= $t->{twig_id_list}; }
    $elt->cut; 
    undef $t->{twig_root};
    return $elt;
  }
   
sub set_inner_xml
  { my( $elt, $xml, @args)= @_;
    my $new_elt= $elt->parse( "<dummy>$xml</dummy>", @args);
    $elt->cut_children;
    $new_elt->paste_first_child( $elt);
    $new_elt->erase;
    return $elt;
  }
 
sub set_outer_xml
  { my( $elt, $xml, @args)= @_;
    my $new_elt= $elt->parse( "<dummy>$xml</dummy>", @args);
    $elt->cut_children;
    $new_elt->replace( $elt);
    $new_elt->erase;
    return $new_elt;
  }
  
 
sub set_inner_html
  { my( $elt, $html)= @_;
    my $t= XML::Twig->new->parse_html( "<html>$html</html>");
    my $new_elt= $t->root;
    if( $elt->tag eq 'head')
      { $new_elt->first_child( 'head')->unwrap;
        $new_elt->first_child( 'body')->cut;
      }
    elsif( $elt->tag ne 'html')
      { $new_elt->first_child( 'head')->cut;
        $new_elt->first_child( 'body')->unwrap;
      }
    $new_elt->cut;
    $elt->cut_children;
    $new_elt->paste_first_child( $elt);
    $new_elt->erase;
    return $elt;
  }

sub set_gi 
  { my ($elt, $gi)= @_;
    unless( defined $XML::Twig::gi2index{$gi})
      { # new gi, create entries in %gi2index and @index2gi
        push  @XML::Twig::index2gi, $gi;
        $XML::Twig::gi2index{$gi}= $#XML::Twig::index2gi;
      }
    $elt->{gi}= $XML::Twig::gi2index{$gi};
    return $elt; 
  }

sub gi  { return $XML::Twig::index2gi[$_[0]->{gi}]; }

sub local_name 
  { my $elt= shift;
    return _local_name( $XML::Twig::index2gi[$elt->{'gi'}]);
  }

sub ns_prefix
  { my $elt= shift;
    return _ns_prefix( $XML::Twig::index2gi[$elt->{'gi'}]);
  }

# namespace prefix for any qname (can be used for elements or attributes)
sub _ns_prefix
  { my $qname= shift;
    if( $qname=~ m{^([^:]*):})
      { return $1; }
    else
      { return( ''); } # should it be '' ?
  }

# local name for any qname (can be used for elements or attributes)
sub _local_name
  { my $qname= shift;
    (my $local= $qname)=~ s{^[^:]*:}{};
    return $local;
  }

#sub get_namespace
sub namespace ## no critic (Subroutines::ProhibitNestedSubs);
  { my $elt= shift;
    my $prefix= defined $_[0] ? shift() : $elt->ns_prefix;
    my $ns_att= $prefix ? "xmlns:$prefix" : "xmlns";
    my $expanded= $DEFAULT_NS{$prefix} || $elt->_inherit_att_through_cut( $ns_att) || '';
    return $expanded;
  }

sub declare_missing_ns ## no critic (Subroutines::ProhibitNestedSubs);
  { my $root= shift;
    my %missing_prefix;
    my $map= $root->_current_ns_prefix_map;

    foreach my $prefix (keys %$map)
      { my $prefix_att= $prefix eq '#default' ? 'xmlns' : "xmlns:$prefix";
        if( ! $root->{'att'}->{$prefix_att}) 
          { $root->set_att( $prefix_att => $map->{$prefix}); }
      }
    return $root;
  }

sub _current_ns_prefix_map
  { my( $elt)= shift;
    my $map;
    while( $elt)
      { foreach my $att ($elt->att_names)
          { my $prefix= $att eq 'xmlns'        ? '#default'
                      : $att=~ m{^xmlns:(.*)$} ? $1
                      : next
                      ;
            if( ! exists $map->{$prefix}) { $map->{$prefix}= $elt->{'att'}->{$att}; }
          }
        $elt= $elt->{parent} || ($elt->{former} && $elt->{former}->{parent});
      }
    return $map;
  }
 
sub set_ns_decl
  { my( $elt, $uri, $prefix)= @_;
    my $ns_att=  $prefix ? "xmlns:$prefix" : 'xmlns';
    $elt->set_att( $ns_att => $uri);
    return $elt;
  }

sub set_ns_as_default
  { my( $root, $uri)= @_;
    my @ns_decl_to_remove;
    foreach my $elt ($root->descendants_or_self)
      { if( $elt->_ns_prefix && $elt->namespace eq $uri) 
          { $elt->set_tag( $elt->local_name); }
        # store any namespace declaration for that uri
        foreach my $ns_decl (grep { $_=~ m{xmlns(:|$)} && $elt->{'att'}->{$_} eq $uri } $elt->att_names)
          { push @ns_decl_to_remove, [$elt, $ns_decl]; }
      }
    $root->set_ns_decl( $uri);
    # now remove the ns declarations (if done earlier then descendants of an element with the ns declaration
    # are not considered being in the namespace
    foreach my $ns_decl_to_remove ( @ns_decl_to_remove)
      { my( $elt, $ns_decl)= @$ns_decl_to_remove;
        $elt->del_att( $ns_decl);
      }
    
    return $root;
  }
     


# return #ELT for an element and #PCDATA... for others
sub get_type
  { my $gi_nb= $_[0]->{gi}; # the number, not the string
    return $ELT if( $gi_nb >= $XML::Twig::SPECIAL_GI);
    return $_[0]->gi;
  }

# return the gi if it's a "real" element, 0 otherwise
sub is_elt
  { if(  $_[0]->{gi} >=  $XML::Twig::SPECIAL_GI)
     { return $_[0]->gi; }
    else
      { return 0; }
  }


sub is_pcdata
  { my $elt= shift;
    return (exists $elt->{'pcdata'});
  }

sub is_cdata
  { my $elt= shift;
    return (exists $elt->{'cdata'});
  }

sub is_pi
  { my $elt= shift;
    return (exists $elt->{'target'});
  }

sub is_comment
  { my $elt= shift;
    return (exists $elt->{'comment'});
  }

sub is_ent
  { my $elt= shift;
    return (exists $elt->{ent} || $elt->{ent_name});
  } 


sub is_text
  { my $elt= shift;
    return (exists( $elt->{'pcdata'}) || (exists $elt->{'cdata'}));
  }

sub is_empty
  { return $_[0]->{empty} || 0; }

sub set_empty
  { $_[0]->{empty}= defined( $_[1]) ? $_[1] : 1; return $_[0]; }

sub set_not_empty
  { delete $_[0]->{empty} if( $_[0]->{'empty'}); return $_[0]; }


sub set_asis
  { my $elt=shift;

    foreach my $descendant ($elt, $elt->_descendants )
      { $descendant->{asis}= 1;
        if( (exists $descendant->{'cdata'}))
          { $descendant->{gi}=$XML::Twig::gi2index{$PCDATA} or $descendant->set_gi( $PCDATA);
            $descendant->{pcdata}=  $descendant->{cdata};
          }

      }
    return $elt;
  }

sub set_not_asis
  { my $elt=shift;
    foreach my $descendant ($elt, $elt->descendants)
      { delete $descendant->{asis} if $descendant->{asis};}
    return $elt;
  }

sub is_asis
  { return $_[0]->{asis}; }

sub closed 
  { my $elt= shift;
    my $t= $elt->twig || return;
    my $curr_elt= $t->{twig_current};
    return 1 unless( $curr_elt);
    return $curr_elt->in( $elt);
  }

sub set_pcdata 
  { my( $elt, $pcdata)= @_;
  
    if( $elt->{extra_data_in_pcdata})
      { _try_moving_extra_data( $elt, $pcdata);
      }
    $elt->{pcdata}= $pcdata;
    return $elt; 
  }

sub _extra_data_in_pcdata      { return $_[0]->{extra_data_in_pcdata}; }
sub _set_extra_data_in_pcdata  { $_[0]->{extra_data_in_pcdata}= $_[1]; return $_[0]; }
sub _del_extra_data_in_pcdata  { delete $_[0]->{extra_data_in_pcdata}; return $_[0]; }
sub _unshift_extra_data_in_pcdata 
    { my $e= shift;
      $e->{extra_data_in_pcdata}||=[];
      unshift @{$e->{extra_data_in_pcdata}}, { text => shift(), offset => shift() };
    }
sub _push_extra_data_in_pcdata    
  { my $e= shift; 
    $e->{extra_data_in_pcdata}||=[]; 
    push @{$e->{extra_data_in_pcdata}}, { text => shift(), offset => shift() }; 
  }

sub _extra_data_before_end_tag     { return $_[0]->{extra_data_before_end_tag} || ''; }
sub _set_extra_data_before_end_tag { $_[0]->{extra_data_before_end_tag}= $_[1]; return $_[0]}
sub _del_extra_data_before_end_tag { delete $_[0]->{extra_data_before_end_tag}; return $_[0]}
sub _prefix_extra_data_before_end_tag 
  { my( $elt, $data)= @_;
    if($elt->{extra_data_before_end_tag})
      { $elt->{extra_data_before_end_tag}= $data . $elt->{extra_data_before_end_tag}; }
    else  
      { $elt->{extra_data_before_end_tag}= $data; }
    return $elt;
  }

# internal, in cases where we know there is no extra_data (inlined anyway!)
sub _set_pcdata { $_[0]->{pcdata}= $_[1]; }

# try to figure out if we can keep the extra_data around
sub _try_moving_extra_data
  { my( $elt, $modified)=@_;
    my $initial= $elt->{pcdata};
    my $cpis= $elt->{extra_data_in_pcdata};

    if( (my $offset= index( $modified, $initial)) != -1) 
      { # text has been added
        foreach (@$cpis) { $_->{offset}+= $offset; }
      }
    elsif( ($offset= index( $initial, $modified)) != -1)
      { # text has been cut
        my $len= length( $modified);
        foreach my $cpi (@$cpis) { $cpi->{offset} -= $offset; }
        $elt->_set_extra_data_in_pcdata( [ grep { $_->{offset} >= 0 && $_->{offset} < $len } @$cpis ]);
      } 
    else
      {    _match_extra_data_words( $elt, $initial, $modified)
        || _match_extra_data_chars( $elt, $initial, $modified)
        || $elt->_del_extra_data_in_pcdata;
      }
  }

sub _match_extra_data_words
  { my( $elt, $initial, $modified)= @_;
    my @initial= split /\b/, $initial; 
    my @modified= split /\b/, $modified;
       
    return _match_extra_data( $elt, length( $initial), \@initial, \@modified);
  }
  
sub _match_extra_data_chars
  { my( $elt, $initial, $modified)= @_;
    my @initial= split //, $initial; 
    my @modified= split //, $modified;
       
    return _match_extra_data( $elt, length( $initial), \@initial, \@modified);
  }

sub _match_extra_data
  { my( $elt, $length, $initial, $modified)= @_;
        
    my $cpis= $elt->{extra_data_in_pcdata};

    if( @$initial <= @$modified)
      { 
        my( $ok, $positions, $offsets)= _pos_offset( $initial, $modified);
        if( $ok) 
          { my $offset=0;
            my $pos= shift @$positions;
            foreach my $cpi (@$cpis)
              { while( $cpi->{offset} >= $pos)
                  { $offset= shift @$offsets; 
                    $pos= shift @$positions || $length +1;
                  }
                $cpi->{offset} += $offset;
              }
            return 1;
          }
      }
    else
      { my( $ok, $positions, $offsets)= _pos_offset( $modified, $initial);
        if( $ok)
          { #print STDERR "pos:    ", join( ':', @$positions), "\n",
            #             "offset: ", join( ':', @$offsets), "\n";
            my $offset=0;
            my $pos= shift @$positions;
            my $prev_pos= 0;
            
            foreach my $cpi (@$cpis)
              { while( $cpi->{offset} >= $pos)
                  { $offset= shift @$offsets;
                    $prev_pos= $pos;
                    $pos= shift @$positions || $length +1;
                  }
                $cpi->{offset} -= $offset;
                if( $cpi->{offset} < $prev_pos) { delete $cpi->{text}; }
              }
            $elt->_set_extra_data_in_pcdata( [ grep { exists $_->{text} } @$cpis ]);
            return 1;
          }
      }
    return 0;
  }

          
sub _pos_offset
  { my( $short, $long)= @_;
    my( @pos, @offset);
    my( $s_length, $l_length)=(0,0);
    while (@$short)
      { my $s_word= shift @$short;
        my $l_word= shift @$long;
        if( $s_word ne $l_word)
          { while( @$long && $s_word ne $l_word)
              { $l_length += length( $l_word);
                $l_word= shift @$long;
              }
            if( !@$long && $s_word ne $l_word) { return 0; }
            push @pos, $s_length;
            push @offset, $l_length - $s_length;
          }
        my $length= length( $s_word);
        $s_length += $length;
        $l_length += $length;
      }
    return( 1, \@pos, \@offset);
  }

sub append_pcdata
  { $_[0]->{'pcdata'}.= $_[1];
    return $_[0]; 
  }

sub pcdata        { return $_[0]->{pcdata}; }


sub append_extra_data 
  {  $_[0]->{extra_data}.= $_[1];
     return $_[0]; 
  }
  
sub set_extra_data 
  { $_[0]->{extra_data}= $_[1];
    return $_[0]; 
  }
sub extra_data { return $_[0]->{extra_data} || ''; }

sub set_target 
  { my( $elt, $target)= @_;
    $elt->{target}= $target;
    return $elt; 
  }
sub target { return $_[0]->{target}; }

sub set_data 
  { $_[0]->{'data'}= $_[1]; 
    return $_[0];
  }
sub data { return $_[0]->{data}; }

sub set_pi
  { my $elt= shift;
    unless( $elt->{gi} == $XML::Twig::gi2index{$PI})
      { $elt->cut_children;
        $elt->{gi}=$XML::Twig::gi2index{$PI} or $elt->set_gi( $PI);
      }
    return $elt->_set_pi( @_);
  }

sub _set_pi
  { $_[0]->set_target( $_[1]);
    $_[0]->{data}=  $_[2];
    return $_[0]; 
  }

sub pi_string { my $string= $PI_START . $_[0]->{target};
                my $data= $_[0]->{data};
                if( defined( $data) && $data ne '') { $string .= " $data"; }
                $string .= $PI_END ;
                return $string;
              }

sub set_comment
  { my $elt= shift;
    unless( $elt->{gi} == $XML::Twig::gi2index{$COMMENT})
      { $elt->cut_children;
        $elt->{gi}=$XML::Twig::gi2index{$COMMENT} or $elt->set_gi( $COMMENT);
      }
    $elt->{comment}=  $_[0];
    return $elt;
  }

sub _set_comment   { $_[0]->{comment}= $_[1]; return $_[0]; }
sub comment        { return $_[0]->{comment}; }
sub comment_string { return $COMMENT_START . _comment_escaped_string( $_[0]->{comment}) . $COMMENT_END; }
# comments cannot start or end with 
sub _comment_escaped_string 
  { my( $c)= @_;
    $c=~ s{^-}{ -};  
    $c=~ s{-$}{- };
    $c=~ s{--}{- -}g;
    return $c;
  }

sub set_ent  { $_[0]->{ent}= $_[1]; return $_[0]; }
sub ent      { return $_[0]->{ent}; }
sub ent_name { return substr( $_[0]->{ent}, 1, -1);}

sub set_cdata 
  { my $elt= shift;
    unless( $elt->{gi} == $XML::Twig::gi2index{$CDATA})
      { $elt->cut_children;
        $elt->insert_new_elt( first_child => $CDATA, @_);
        return $elt;
      }
    $elt->{cdata}=  $_[0];
    return $_[0];
  }
  
sub _set_cdata 
  { $_[0]->{cdata}= $_[1]; 
    return $_[0];
  }

sub append_cdata
  { $_[0]->{cdata}.= $_[1]; 
    return $_[0];
  }
sub cdata { return $_[0]->{cdata}; }


sub contains_only_text
  { my $elt= shift;
    return 0 unless $elt->is_elt;
    foreach my $child ($elt->_children)
      { return 0 if $child->is_elt; }
    return $elt;
  } 
  
sub contains_only
  { my( $elt, $exp)= @_;
    my @children= do { my $elt= $elt; my @children=(); my $child= $elt->{first_child}; while( $child) { push @children, $child; $child= $child->{next_sibling}; } @children; };
    foreach my $child (@children)
      { return 0 unless $child->is( $exp); }
    return @children || 1;
  } 

sub contains_a_single
  { my( $elt, $exp)= @_;
    my $child= $elt->{first_child} or return 0;
    return 0 unless $child->passes( $exp);
    return 0 if( $child->{next_sibling});
    return $child;
  } 


sub root 
  { my $elt= shift;
    while( $elt->{parent}) { $elt= $elt->{parent}; }
    return $elt;
  }

sub _root_through_cut
  { my $elt= shift;
    while( $elt->{parent} || ($elt->{former} && $elt->{former}->{parent})) { $elt= $elt->{parent} || ($elt->{former} && $elt->{former}->{parent}); }
    return $elt;
  }

sub twig 
  { my $elt= shift;
    my $root= $elt->root;
    return $root->{twig};
  }

sub _twig_through_cut
  { my $elt= shift;
    my $root= $elt->_root_through_cut;
    return $root->{twig};
  }


# used for navigation
# returns undef or the element, depending on whether $elt passes $cond
# $cond can be
# - empty: the element passes the condition
# - ELT ('#ELT'): the element passes the condition if it is a "real" element
# - TEXT ('#TEXT'): the element passes if it is a CDATA or PCDATA element
# - a string with an XPath condition (only a subset of XPath is actually
#   supported).
# - a regexp: the element passes if its gi matches the regexp
# - a code ref: the element passes if the code, applied on the element,
#   returns true

my %cond_cache; # expression => coderef

sub reset_cond_cache { %cond_cache=(); }

{ 
   sub _install_cond
    { my $cond= shift;
      my $test;
      my $init=''; 

      my $original_cond= $cond;

      my $not= ($cond=~ s{^\s*!}{}) ? '!' : '';

      if( ref $cond eq 'CODE') { return $cond; }
    
      if( ref $cond eq 'Regexp')
        { $test = qq{(\$_[0]->gi=~ /$cond/)}; }
      else
        { my @tests;
          while( $cond)
            { 
              # the condition is a string
              if( $cond=~ s{$ELT$SEP}{})     
                { push @tests, qq{\$_[0]->is_elt}; }
              elsif( $cond=~ s{$TEXT$SEP}{}) 
                { push @tests, qq{\$_[0]->is_text}; }
              elsif( $cond=~ s{^\s*($REG_TAG_PART)$SEP}{})                  
                { push @tests, _gi_test( $1); } 
              elsif( $cond=~ s{^\s*($REG_REGEXP)$SEP}{})
                { # /regexp/
                  push @tests, qq{ \$_[0]->gi=~ $1 }; 
                }
              elsif( $cond=~ s{^\s*($REG_TAG_PART)?\s*  # $1
                               \[\s*(-?)\s*(\d+)\s*\]  #   [$2]
                               $SEP}{}xo
                   )
                { my( $gi, $neg, $index)= ($1, $2, $3);
                  my $siblings= $neg ? q{$_[0]->_next_siblings} : q{$_[0]->_prev_siblings};
                  if( $gi && ($gi ne '*')) 
                    #{ $test= qq{((\$_[0]->gi eq "$gi") && (scalar( grep { \$_->gi eq "$gi" } $siblings) + 1 == $index))}; }
                    { push @tests, _and( _gi_test( $gi), qq{ (scalar( grep { \$_->gi eq "$gi" } $siblings) + 1 == $index)}); }
                  else
                    { push @tests, qq{(scalar( $siblings) + 1 == $index)}; }
                }
              elsif( $cond=~ s{^\s*($REG_TAG_PART?)\s*($REG_PREDICATE)$SEP}{})
                { my( $gi, $predicate)= ( $1, $2);
                  push @tests, _and( _gi_test( $gi), _parse_predicate_in_step( $predicate));
                }
              elsif( $cond=~ s{^\s*($REG_NAKED_PREDICATE)$SEP}{})
                { push @tests,   _parse_predicate_in_step( $1); }
              else
                { croak "wrong navigation condition '$original_cond' ($@)"; }
            }
           $test= @tests > 1 ? '(' . join( '||', map { "($_)" } @tests) . ')' : $tests[0];
        }

      #warn "init: '$init' - test: '$test'\n";

      my $sub= qq{sub { $NO_WARNINGS; $init; return $not($test) ? \$_[0] : undef; } };
      my $s= eval $sub; 
      #warn "cond: $cond\n$sub\n";
      if( $@) 
        { croak "wrong navigation condition '$original_cond' ($@);" }
      return $s;
    }

  sub _gi_test
    { my( $full_gi)= @_;

      # optimize if the gi exists, including the case where the gi includes a dot
      my $index= $XML::Twig::gi2index{$full_gi};
      if( $index) { return qq{\$_[0]->{gi} == $index}; }

      my( $gi, $class, $id)= $full_gi=~ m{^(.*?)(?:[.]([^.]*)|[#](.*))?$};

      my $gi_test='';
      if( $gi && $gi ne '*' )
        { # 2 options, depending on whether the gi exists in gi2index
          # start optimization
          my $index= $XML::Twig::gi2index{$gi};
          if( $index)
            { # the gi exists, use its index as a faster shortcut
              $gi_test = qq{\$_[0]->{gi} == $index};
            }
          else
          # end optimization
            { # it does not exist (but might be created later), compare the strings
              $gi_test = qq{ \$_[0]->gi eq "$gi"}; 
            }
        }
      else
        { $gi_test= 1; }

      my $class_test='';
      #warn "class: '$class'";
      if( $class)
        { $class_test = qq{ defined( \$_[0]->{att}->{class}) && \$_[0]->{att}->{class}=~ m{\\b$class\\b} }; }

      my $id_test='';
      #warn "id: '$id'";
      if( $id)
        { $id_test = qq{ defined( \$_[0]->{att}->{$ID}) && \$_[0]->{att}->{$ID} eq '$id' }; }


      #warn "gi_test: '$gi_test' - class_test: '$class_test' returning ",  _and( $gi_test, $class_test);
      return _and( $gi_test, $class_test, $id_test);
  }


  # input: the original predicate
  sub _parse_predicate_in_step
    { my $cond= shift; 
      my %PERL_ALPHA_TEST= ( '=' => ' eq ', '!=' => ' ne ', '>' => ' gt ', '>=' => ' ge ', '<' => ' lt ', '<=' => ' le ');

      $cond=~ s{^\s*\[\s*}{};
      $cond=~ s{\s*\]\s*$}{};
      $cond=~ s{(   ($REG_STRING|$REG_REGEXP)                # strings or regexps
                   |\@($REG_TAG_NAME)(?=\s*(?:[><=!]|!~|=~)) # @att (followed by a comparison operator)
                   |\@($REG_TAG_NAME)                        # @att (not followed by a comparison operator)
                   |=~|!~                                    # matching operators
                   |([><]=?|=|!=)(?=\s*[\d+-])               # test before a number
                   |([><]=?|=|!=)                            # test, other cases
                   |($REG_FUNCTION)                          # no arg functions
                   # this bit is a mess, but it is the only solution with this half-baked parser
                   |((?:string|text)\(\s*$REG_TAG_NAME\s*\)\s*$REG_MATCH\s*$REG_REGEXP) # string( child) =~ /regexp/
                   |((?:string|text)\(\s*$REG_TAG_NAME\s*\)\s*!?=\s*$REG_VALUE)         # string( child) = "value" (or !=)
                   |((?:string|text)\(\s*$REG_TAG_NAME\s*\)\s*[<>]=?\s*$REG_VALUE)      # string( child) > "value"
                   |(and|or)
                )}
               { my( $token, $string, $att, $bare_att, $num_test, $alpha_test, $func, $string_regexp, $string_eq, $string_test, $and_or)
                 = ( $1,     $2,      $3,   $4,        $5,        $6,          $7,    $8,             $9,         $10,          $11);
      
                 if( defined $string)   { $token }
                 elsif( $att)           { "( \$_[0]->{att} && exists( \$_[0]->{att}->{'$att'}) && \$_[0]->{att}->{'$att'})"; }
                 elsif( $bare_att)      { "(\$_[0]->{att} && defined( \$_[0]->{att}->{'$bare_att'}))"; }
                 elsif( $num_test && ($num_test eq '=') ) { "==" } # others tests are unchanged
                 elsif( $alpha_test)    { $PERL_ALPHA_TEST{$alpha_test} }
                 elsif( $func && $func=~ m{^(?:string|text)})
                                        { "\$_[0]->text"; }
                 elsif( $string_regexp && $string_regexp =~ m{(?:string|text)\(\s*($REG_TAG_NAME)\s*\)\s*($REG_MATCH)\s*($REG_REGEXP)})
                                        { "(XML::Twig::_first_n { (\$_->gi eq '$1') && (\$_->text $2 $3) } 1, \$_[0]->_children)"; }
                 elsif( $string_eq     && $string_eq     =~ m{(?:string|text)\(\s*($REG_TAG_NAME)\s*\)\s*(!?=)\s*($REG_VALUE)})
                                        {"(XML::Twig::_first_n { (\$_->gi eq '$1') && (\$_->text $PERL_ALPHA_TEST{$2} $3) } 1, \$_[0]->_children)"; }
                 elsif( $string_test   && $string_test   =~ m{(?:string|text)\(\s*($REG_TAG_NAME)\s*\)\s*([<>]=?)\s*($REG_VALUE)})
                                        { "(XML::Twig::_first_n { (\$_->gi eq '$1') && (\$_->text $2 $3) } 1, \$_[0]->_children)"; }
                 elsif( $and_or)        { $and_or eq 'and' ? '&&' : '||' ; }
                 else                   { $token; }
               }gexs;
      return "($cond)";
    }
  

  sub _op
    { my $op= shift;
      if(    $op eq '=')  { $op= 'eq'; }
      elsif( $op eq '!=') { $op= 'ne'; }
      return $op;
    }

  sub passes
    { my( $elt, $cond)= @_;
      return $elt unless $cond;
      my $sub= ($cond_cache{$cond} ||= _install_cond( $cond));
      return $sub->( $elt);
    }
}

sub set_parent 
  { $_[0]->{parent}= $_[1];
    if( $XML::Twig::weakrefs) { weaken( $_[0]->{parent}); }
  }

sub parent
  { my $elt= shift;
    my $cond= shift || return $elt->{parent};
    do { $elt= $elt->{parent} || return; } until ( $elt->passes( $cond));
    return $elt;
  }

sub set_first_child 
  { $_[0]->{'first_child'}= $_[1]; 
  }

sub first_child
  { my $elt= shift;
    my $cond= shift || return $elt->{first_child};
    my $child= $elt->{first_child};
    my $test_cond= ($cond_cache{$cond} ||= _install_cond( $cond));
    while( $child && !$test_cond->( $child)) 
       { $child= $child->{next_sibling}; }
    return $child;
  }
  
sub _first_child   { return $_[0]->{first_child};  }
sub _last_child    { return $_[0]->{last_child};   }
sub _next_sibling  { return $_[0]->{next_sibling}; }
sub _prev_sibling  { return $_[0]->{prev_sibling}; }
sub _parent        { return $_[0]->{parent};       }
sub _next_siblings { my $elt= shift; my @siblings; while( $elt= $elt->{next_sibling}) { push @siblings, $elt; } return @siblings; }
sub _prev_siblings { my $elt= shift; my @siblings; while( $elt= $elt->{prev_sibling}) { push @siblings, $elt; } return @siblings; }

# sets a field
# arguments $record, $cond, @content
sub set_field
  { my $record = shift;
    my $cond = shift;
    my $child= $record->first_child( $cond);
    if( $child)
      { $child->set_content( @_); }
    else
      { if( $cond=~ m{^\s*($REG_TAG_NAME)})
          { my $gi= $1;
            $child= $record->insert_new_elt( last_child => $gi, @_); 
          }
        else
          { croak "can't create a field name from $cond"; }
      } 
    return $child;
  }

sub set_last_child 
  { $_[0]->{'last_child'}= $_[1];
    delete $_->[0]->{empty};
    if( $XML::Twig::weakrefs) { weaken( $_[0]->{'last_child'}); }
  }

sub last_child
  { my $elt= shift;
    my $cond= shift || return $elt->{last_child};
    my $test_cond= ($cond_cache{$cond} ||= _install_cond( $cond));
    my $child= $elt->{last_child};
    while( $child && !$test_cond->( $child) )
      { $child= $child->{prev_sibling}; }
    return $child
  }


sub set_prev_sibling 
  { $_[0]->{'prev_sibling'}= $_[1]; 
    if( $XML::Twig::weakrefs) { weaken( $_[0]->{'prev_sibling'}); } 
  }

sub prev_sibling
  { my $elt= shift;
    my $cond= shift || return $elt->{prev_sibling};
    my $test_cond= ($cond_cache{$cond} ||= _install_cond( $cond));
    my $sibling= $elt->{prev_sibling};
    while( $sibling && !$test_cond->( $sibling) )
          { $sibling= $sibling->{prev_sibling}; }
    return $sibling;
  }

sub set_next_sibling { $_[0]->{'next_sibling'}= $_[1]; }

sub next_sibling
  { my $elt= shift;
    my $cond= shift || return $elt->{next_sibling};
    my $test_cond= ($cond_cache{$cond} ||= _install_cond( $cond));
    my $sibling= $elt->{next_sibling};
    while( $sibling && !$test_cond->( $sibling) )
          { $sibling= $sibling->{next_sibling}; }
    return $sibling;
  }

# methods dealing with the class attribute, convenient if you work with xhtml
sub class   {   $_[0]->{att}->{class}; }
# lvalue version of class. separate from class to avoid problem like RT#
sub lclass     
          :lvalue    # > perl 5.5
  { $_[0]->{att}->{class}; }

sub set_class { my( $elt, $class)= @_; $elt->set_att( class => $class); }

# adds a class to an element
sub add_to_class
  { my( $elt, $new_class)= @_;
    return $elt unless $new_class;
    my $class= $elt->class;
    my %class= $class ? map { $_ => 1 } split /\s+/, $class : ();
    $class{$new_class}= 1;
    $elt->set_class( join( ' ', sort keys %class));
  }

sub remove_class
  { my( $elt, $class_to_remove)= @_;
    return $elt unless $class_to_remove;
    my $class= $elt->class;
    my %class= $class ? map { $_ => 1 } split /\s+/, $class : ();
    delete $class{$class_to_remove};
    $elt->set_class( join( ' ', sort keys %class));
  }

sub att_to_class      { my( $elt, $att)= @_; $elt->set_class( $elt->{'att'}->{$att}); }
sub add_att_to_class  { my( $elt, $att)= @_; $elt->add_to_class( $elt->{'att'}->{$att}); }
sub move_att_to_class { my( $elt, $att)= @_; $elt->add_to_class( $elt->{'att'}->{$att});
                        $elt->del_att( $att); 
                      }
sub tag_to_class      { my( $elt)= @_; $elt->set_class( $elt->tag);    }
sub add_tag_to_class  { my( $elt)= @_; $elt->add_to_class( $elt->tag); }
sub set_tag_class     { my( $elt, $new_tag)= @_; $elt->add_tag_to_class; $elt->set_tag( $new_tag); }

sub tag_to_span       
  { my( $elt)= @_; 
    $elt->set_class( $elt->tag) unless( $elt->tag eq 'span' && $elt->class); # set class to span unless it would mean replacing it with span
    $elt->set_tag( 'span'); 
  }

sub tag_to_div    
  { my( $elt)= @_; 
    $elt->set_class( $elt->tag) unless( $elt->tag eq 'div' && $elt->class); # set class to div unless it would mean replacing it with div
    $elt->set_tag( 'div');
  }

sub in_class          
  { my( $elt, $class)= @_;
    my $elt_class= $elt->class;
    return unless( defined $elt_class);
    return $elt->class=~ m{(?:^|\s)\Q$class\E(?:\s|$)} ? $elt : 0;
  }


# get or set all attributes
# argument can be a hash or a hashref
sub set_atts 
  { my $elt= shift;
    my %atts;
    tie %atts, 'Tie::IxHash' if( keep_atts_order());
    %atts= @_ == 1 ? %{$_[0]} : @_;
    $elt->{att}= \%atts;
    if( exists $atts{$ID}) { $elt->_set_id( $atts{$ID}); }
    return $elt;
  }

sub atts      { return $_[0]->{att};                }
sub att_names { return (sort keys %{$_[0]->{att}}); }
sub del_atts  { $_[0]->{att}={}; return $_[0];      }

# get or set a single attribute (set works for several atts)
sub set_att 
  { my $elt= shift;

    if( $_[0] && ref( $_[0]) && !$_[1]) 
      { croak "improper call to set_att, usage is \$elt->set_att( att1 => 'val1', att2 => 'val2',...)"; }
    
    unless( $elt->{att})
      { $elt->{att}={};
        tie %{$elt->{att}}, 'Tie::IxHash' if( keep_atts_order());
      }

    while(@_) 
      { my( $att, $val)= (shift, shift);
        $elt->{att}->{$att}= $val;
        if( $att eq $ID) { $elt->_set_id( $val); } 
      }
    return $elt;
  }
 
sub att {  $_[0]->{att}->{$_[1]}; }
# lvalue version of att. separate from class to avoid problem like RT#
sub latt 
          :lvalue    # > perl 5.5
  { $_[0]->{att}->{$_[1]}; }

sub del_att 
  { my $elt= shift;
    while( @_) { delete $elt->{'att'}->{shift()}; }
    return $elt;
  }

sub att_exists { return exists  $_[0]->{att}->{$_[1]}; }

# delete an attribute from all descendants of an element
sub strip_att
  { my( $elt, $att)= @_;
    $_->del_att( $att) foreach ($elt->descendants_or_self( qq{*[\@$att]}));
    return $elt;
  }

sub change_att_name
  { my( $elt, $old_name, $new_name)= @_;
    my $value= $elt->{'att'}->{$old_name};
    return $elt unless( defined $value);
    $elt->del_att( $old_name)
        ->set_att( $new_name => $value);
    return $elt;
  }

sub lc_attnames
  { my $elt= shift;
    foreach my $att ($elt->att_names)
      { if( $att ne lc $att) { $elt->change_att_name( $att, lc $att); } }
    return $elt;
  }

sub set_twig_current { $_[0]->{twig_current}=1; }
sub del_twig_current { delete $_[0]->{twig_current}; }


# get or set the id attribute
sub set_id 
  { my( $elt, $id)= @_;
    $elt->del_id() if( exists $elt->{att}->{$ID});
    $elt->set_att($ID, $id); 
    $elt->_set_id( $id);
    return $elt;
  }

# only set id, does not update the attribute value
sub _set_id
  { my( $elt, $id)= @_;
    my $t= $elt->twig || $elt;
    $t->{twig_id_list}->{$id}= $elt;
    if( $XML::Twig::weakrefs) { weaken(  $t->{twig_id_list}->{$id}); }
    return $elt;
  }

sub id { return $_[0]->{att}->{$ID}; }

# methods used to add ids to elements that don't have one
BEGIN 
{ my $id_nb   = "0001";
  my $id_seed = "twig_id_";

  sub set_id_seed ## no critic (Subroutines::ProhibitNestedSubs);
    { $id_seed= $_[1]; $id_nb=1; }

  sub add_id ## no critic (Subroutines::ProhibitNestedSubs);
    { my $elt= shift; 
      if( defined $elt->{'att'}->{$ID})
        { return $elt->{'att'}->{$ID}; }
      else
        { my $id= $_[0] && ref( $_[0]) && isa( $_[0], 'CODE') ? $_[0]->( $elt) : $id_seed . $id_nb++; 
          $elt->set_id( $id);
          return $id;
        }
    }
}



# delete the id attribute and remove the element from the id list
sub del_id 
  { my $elt= shift; 
    if( ! exists $elt->{att}->{$ID}) { return $elt }; 
    my $id= $elt->{att}->{$ID};

    delete $elt->{att}->{$ID}; 

    my $t= shift || $elt->twig;
    unless( $t) { return $elt; }
    if( exists $t->{twig_id_list}->{$id}) { delete $t->{twig_id_list}->{$id}; }

    return $elt;
  }

# return the list of children
sub children
  { my $elt= shift;
    my @children;
    my $child= $elt->first_child( @_);
    while( $child) 
      { push @children, $child;
        $child= $child->next_sibling( @_);
      } 
    return @children;
  }

sub _children
  { my $elt= shift;
    my @children=();
    my $child= $elt->{first_child};
    while( $child) 
      { push @children, $child;
        $child= $child->{next_sibling};
      } 
    return @children;
  }

sub children_copy
  { my $elt= shift;
    my @children;
    my $child= $elt->first_child( @_);
    while( $child) 
      { push @children, $child->copy;
        $child= $child->next_sibling( @_);
      } 
    return @children;
  }


sub children_count
  { my $elt= shift;
    my $cond= shift;
    my $count=0;
    my $child= $elt->{first_child};
    while( $child)
      { $count++ if( $child->passes( $cond)); 
        $child= $child->{next_sibling};
      }
    return $count;
  }

sub children_text
  { my $elt= shift;
    return wantarray() ? map { $_->text} $elt->children( @_)
                       : join( '', map { $_->text} $elt->children( @_) )
                       ;
  }

sub children_trimmed_text
  { my $elt= shift;
    return wantarray() ? map { $_->trimmed_text} $elt->children( @_)
                       : join( '', map { $_->trimmed_text} $elt->children( @_) )
                       ;
  }

sub all_children_are
  { my( $parent, $cond)= @_;
    foreach my $child ($parent->_children)
      { return 0 unless( $child->passes( $cond)); }
    return $parent;
  }


sub ancestors
  { my( $elt, $cond)= @_;
    my @ancestors;
    while( $elt->{parent})
      { $elt= $elt->{parent};
        push @ancestors, $elt if( $elt->passes( $cond));
      }
    return @ancestors;
  }

sub ancestors_or_self
  { my( $elt, $cond)= @_;
    my @ancestors;
    while( $elt)
      { push @ancestors, $elt if( $elt->passes( $cond));
        $elt= $elt->{parent};
      }
    return @ancestors;
  }


sub _ancestors
  { my( $elt, $include_self)= @_;
    my @ancestors= $include_self ? ($elt) : ();
    while( $elt= $elt->{parent}) { push @ancestors, $elt; }
    return @ancestors;
  }


sub inherit_att
  { my $elt= shift;
    my $att= shift;
    my %tags= map { ($_, 1) } @_;

    do 
      { if(   (defined $elt->{'att'}->{$att})
           && ( !%tags || $tags{$XML::Twig::index2gi[$elt->{'gi'}]})
          )
          { return $elt->{'att'}->{$att}; }
      } while( $elt= $elt->{parent});
    return undef;
  }

sub _inherit_att_through_cut
  { my $elt= shift;
    my $att= shift;
    my %tags= map { ($_, 1) } @_;

    do 
      { if(   (defined $elt->{'att'}->{$att})
           && ( !%tags || $tags{$XML::Twig::index2gi[$elt->{'gi'}]})
          )
          { return $elt->{'att'}->{$att}; }
      } while( $elt= $elt->{parent} || ($elt->{former} && $elt->{former}->{parent}));
    return undef;
  }


sub current_ns_prefixes
  { my $elt= shift;
    my %prefix;
    $prefix{''}=1 if( $elt->namespace( ''));
    while( $elt)
      { my @ns= grep { !m{^xml} } map { m{^([^:]+):} } ($XML::Twig::index2gi[$elt->{'gi'}], $elt->att_names);
        $prefix{$_}=1 foreach (@ns);
        $elt= $elt->{parent};
      }

    return (sort keys %prefix);
  }

# kinda counter-intuitive actually:
# the next element is found by looking for the next open tag after from the
# current one, which is the first child, if it exists, or the next sibling
# or the first next sibling of an ancestor
# optional arguments are: 
#   - $subtree_root: a reference to an element, when the next element is not 
#                    within $subtree_root anymore then next_elt returns undef
#   - $cond: a condition, next_elt returns the next element matching the condition
                 
sub next_elt
  { my $elt= shift;
    my $subtree_root= 0;
    $subtree_root= shift if( ref( $_[0]) && isa( $_[0], 'XML::Twig::Elt'));
    my $cond= shift;
    my $next_elt;

    my $ind;                                                              # optimization
    my $test_cond;
    if( $cond)                                                            # optimization
      { unless( defined( $ind= $XML::Twig::gi2index{$cond}) )             # optimization
          { $test_cond= ($cond_cache{$cond} ||= _install_cond( $cond)); } # optimization
      }                                                                   # optimization
    
    do
      { if( $next_elt= $elt->{first_child})
          { # simplest case: the elt has a child
          }
         elsif( $next_elt= $elt->{next_sibling}) 
          { # no child but a next sibling (just check we stay within the subtree)
          
            # case where elt is subtree_root, is empty and has a sibling
            return undef if( $subtree_root && ($elt == $subtree_root));
            
          }
        else
          { # case where the element has no child and no next sibling:
            # get the first next sibling of an ancestor, checking subtree_root 
          
            # case where elt is subtree_root, is empty and has no sibling
            return undef if( $subtree_root && ($elt == $subtree_root));
             
            $next_elt= $elt->{parent} || return undef;

            until( $next_elt->{next_sibling})
              { return undef if( $subtree_root && ($subtree_root == $next_elt));
                $next_elt= $next_elt->{parent} || return undef;
              }
            return undef if( $subtree_root && ($subtree_root == $next_elt)); 
            $next_elt= $next_elt->{next_sibling};   
          }  
      $elt= $next_elt;                   # just in case we need to loop
    } until(    ! defined $elt 
             || ! defined $cond 
         || (defined $ind       && ($elt->{gi} eq $ind))   # optimization
         || (defined $test_cond && ($test_cond->( $elt)))
               );
    
      return $elt;
      }

# return the next_elt within the element
# just call next_elt with the element as first and second argument
sub first_descendant { return $_[0]->next_elt( @_); }

# get the last descendant, # then return the element found or call prev_elt with the condition
sub last_descendant
  { my( $elt, $cond)= @_;
    my $last_descendant= $elt->_last_descendant;
    if( !$cond || $last_descendant->matches( $cond))
      { return $last_descendant; }
    else
      { return $last_descendant->prev_elt( $elt, $cond); }
  }

# no argument allowed here, just go down the last_child recursively
sub _last_descendant
  { my $elt= shift;
    while( my $child= $elt->{last_child}) { $elt= $child; }
    return $elt;
  }

# counter-intuitive too:
# the previous element is found by looking
# for the first open tag backwards from the current one
# it's the last descendant of the previous sibling 
# if it exists, otherwise it's simply the parent
sub prev_elt
  { my $elt= shift;
    my $subtree_root= 0;
    if( defined $_[0] and (ref( $_[0]) && isa( $_[0], 'XML::Twig::Elt')))
      { $subtree_root= shift ;
        return undef if( $elt == $subtree_root);
      }
    my $cond= shift;
    # get prev elt
    my $prev_elt;
    do
      { return undef if( $elt == $subtree_root);
        if( $prev_elt= $elt->{prev_sibling})
          { while( $prev_elt->{last_child})
              { $prev_elt= $prev_elt->{last_child}; }
          }
        else
          { $prev_elt= $elt->{parent} || return undef; }
        $elt= $prev_elt;     # in case we need to loop 
      } until( $elt->passes( $cond));

    return $elt;
  }

sub _following_elt
  { my( $elt)= @_;
    while( $elt && !$elt->{next_sibling})
      { $elt= $elt->{parent}; }
    return $elt ? $elt->{next_sibling} : undef;
  }

sub following_elt
  { my( $elt, $cond)= @_;
    $elt= $elt->_following_elt || return undef;
    return $elt if( !$cond || $elt->matches( $cond));
    return $elt->next_elt( $cond);
  }

sub following_elts
  { my( $elt, $cond)= @_;
    if( !$cond) { undef $cond; }
    my $following= $elt->following_elt( $cond);
    if( $following)
      { my @followings= $following;
        while( $following= $following->next_elt( $cond))
          { push @followings, $following; }
        return( @followings);
      }
    else
      { return (); }
  }

sub _preceding_elt
  { my( $elt)= @_;
    while( $elt && !$elt->{prev_sibling})
      { $elt= $elt->{parent}; }
    return $elt ? $elt->{prev_sibling}->_last_descendant : undef;
  }

sub preceding_elt
  { my( $elt, $cond)= @_;
    $elt= $elt->_preceding_elt || return undef;
    return $elt if( !$cond || $elt->matches( $cond));
    return $elt->prev_elt( $cond);
  }

sub preceding_elts
  { my( $elt, $cond)= @_;
    if( !$cond) { undef $cond; }
    my $preceding= $elt->preceding_elt( $cond);
    if( $preceding)
      { my @precedings= $preceding;
        while( $preceding= $preceding->prev_elt( $cond))
          { push @precedings, $preceding; }
        return( @precedings);
      }
    else
      { return (); }
  }

# used in get_xpath
sub _self
  { my( $elt, $cond)= @_;
    return $cond ? $elt->matches( $cond) : $elt;
  }

sub next_n_elt
  { my $elt= shift;
    my $offset= shift || return undef;
    foreach (1..$offset)
      { $elt= $elt->next_elt( @_) || return undef; }
    return $elt;
  }

# checks whether $elt is included in $ancestor, returns 1 in that case
sub in
  { my ($elt, $ancestor)= @_;
    if( ref( $ancestor) && isa( $ancestor, 'XML::Twig::Elt'))
      { # element
        while( $elt= $elt->{parent}) { return $elt if( $elt ==  $ancestor); } 
      }
    else
      { # condition
        while( $elt= $elt->{parent}) { return $elt if( $elt->matches( $ancestor)); } 
      }
    return 0;           
  }

sub first_child_text  
  { my $elt= shift;
    my $dest=$elt->first_child(@_) or return '';
    return $dest->text;
  }

sub fields  
  { my $elt= shift;
    return map { $elt->field( $_) } @_;
  }

sub first_child_trimmed_text  
  { my $elt= shift;
    my $dest=$elt->first_child(@_) or return '';
    return $dest->trimmed_text;
  }
  
sub first_child_matches
  { my $elt= shift;
    my $dest= $elt->{first_child} or return undef;
    return $dest->passes( @_);
  }
  
sub last_child_text  
  { my $elt= shift;
    my $dest=$elt->last_child(@_) or return '';
    return $dest->text;
  }
  
sub last_child_trimmed_text  
  { my $elt= shift;
    my $dest=$elt->last_child(@_) or return '';
    return $dest->trimmed_text;
  }
  
sub last_child_matches
  { my $elt= shift;
    my $dest= $elt->{last_child} or return undef;
    return $dest->passes( @_);
  }
  
sub child_text
  { my $elt= shift;
    my $dest=$elt->child(@_) or return '';
    return $dest->text;
  }
  
sub child_trimmed_text
  { my $elt= shift;
    my $dest=$elt->child(@_) or return '';
    return $dest->trimmed_text;
  }
  
sub child_matches
  { my $elt= shift;
    my $nb= shift;
    my $dest= $elt->child( $nb) or return undef;
    return $dest->passes( @_);
  }

sub prev_sibling_text  
  { my $elt= shift;
    my $dest= $elt->_prev_sibling(@_) or return '';
    return $dest->text;
  }
  
sub prev_sibling_trimmed_text  
  { my $elt= shift;
    my $dest= $elt->_prev_sibling(@_) or return '';
    return $dest->trimmed_text;
  }
  
sub prev_sibling_matches
  { my $elt= shift;
    my $dest= $elt->{prev_sibling} or return undef;
    return $dest->passes( @_);
  }
  
sub next_sibling_text  
  { my $elt= shift;
    my $dest= $elt->next_sibling(@_) or return '';
    return $dest->text;
  }
  
sub next_sibling_trimmed_text  
  { my $elt= shift;
    my $dest= $elt->next_sibling(@_) or return '';
    return $dest->trimmed_text;
  }
  
sub next_sibling_matches
  { my $elt= shift;
    my $dest= $elt->{next_sibling} or return undef;
    return $dest->passes( @_);
  }
  
sub prev_elt_text  
  { my $elt= shift;
    my $dest= $elt->prev_elt(@_) or return '';
    return $dest->text;
  }
  
sub prev_elt_trimmed_text  
  { my $elt= shift;
    my $dest= $elt->prev_elt(@_) or return '';
    return $dest->trimmed_text;
  }
  
sub prev_elt_matches
  { my $elt= shift;
    my $dest= $elt->prev_elt or return undef;
    return $dest->passes( @_);
  }
  
sub next_elt_text  
  { my $elt= shift;
    my $dest= $elt->next_elt(@_) or return '';
    return $dest->text;
  }
  
sub next_elt_trimmed_text  
  { my $elt= shift;
    my $dest= $elt->next_elt(@_) or return '';
    return $dest->trimmed_text;
  }
  
sub next_elt_matches
  { my $elt= shift;
    my $dest= $elt->next_elt or return undef;
    return $dest->passes( @_);
  }
  
sub parent_text  
  { my $elt= shift;
    my $dest= $elt->parent(@_) or return '';
    return $dest->text;
  }
  
sub parent_trimmed_text  
  { my $elt= shift;
    my $dest= $elt->parent(@_) or return '';
    return $dest->trimmed_text;
  }
  
sub parent_matches
  { my $elt= shift;
    my $dest= $elt->{parent} or return undef;
    return $dest->passes( @_);
  }
 
sub is_first_child
  { my $elt= shift;
    my $parent= $elt->{parent} or return 0;
    my $first_child= $parent->first_child( @_) or return 0;
    return ($first_child == $elt) ? $elt : 0;
  }
 
sub is_last_child
  { my $elt= shift;
    my $parent= $elt->{parent} or return 0;
    my $last_child= $parent->last_child( @_) or return 0;
    return ($last_child == $elt) ? $elt : 0;
  }

# returns the depth level of the element
# if 2 parameter are used then counts the 2cd element name in the
# ancestors list
sub level
  { my( $elt, $cond)= @_;
    my $level=0;
    my $name=shift || '';
    while( $elt= $elt->{parent}) { $level++ if( !$cond || $elt->matches( $cond)); }
    return $level;           
  }

# checks whether $elt has an ancestor that satisfies $cond, returns the ancestor
sub in_context
  { my ($elt, $cond, $level)= @_;
    $level= -1 unless( $level) ;  # $level-- will never hit 0

    while( $level)
      { $elt= $elt->{parent} or return 0;
        if( $elt->matches( $cond)) { return $elt; }
        $level--;
      }
    return 0;
  }

sub _descendants
  { my( $subtree_root, $include_self)= @_;
    my @descendants= $include_self ? ($subtree_root) : ();

    my $elt= $subtree_root; 
    my $next_elt;   
 
    MAIN: while( 1)  
      { if( $next_elt= $elt->{first_child})
          { # simplest case: the elt has a child
          }
        elsif( $next_elt= $elt->{next_sibling}) 
          { # no child but a next sibling (just check we stay within the subtree)
          
            # case where elt is subtree_root, is empty and has a sibling
            last MAIN if( $elt == $subtree_root);
          }
        else
          { # case where the element has no child and no next sibling:
            # get the first next sibling of an ancestor, checking subtree_root 
                
            # case where elt is subtree_root, is empty and has no sibling
            last MAIN if( $elt == $subtree_root);
               
            # backtrack until we find a parent with a next sibling
            $next_elt= $elt->{parent} || last;
            until( $next_elt->{next_sibling})
              { last MAIN if( $subtree_root == $next_elt);
                $next_elt= $next_elt->{parent} || last MAIN;
              }
            last MAIN if( $subtree_root == $next_elt); 
            $next_elt= $next_elt->{next_sibling};   
          }  
        $elt= $next_elt || last MAIN;
        push @descendants, $elt;
      }
    return @descendants;
  }


sub descendants
  { my( $subtree_root, $cond)= @_;
    my @descendants=(); 
    my $elt= $subtree_root;
    
    # this branch is pure optimization for speed: if $cond is a gi replace it
    # by the index of the gi and loop here 
    # start optimization
    my $ind;
    if( !$cond || ( defined ( $ind= $XML::Twig::gi2index{$cond})) )
      {
        my $next_elt;

        while( 1)  
          { if( $next_elt= $elt->{first_child})
                { # simplest case: the elt has a child
                }
             elsif( $next_elt= $elt->{next_sibling}) 
              { # no child but a next sibling (just check we stay within the subtree)
           
                # case where elt is subtree_root, is empty and has a sibling
                last if( $subtree_root && ($elt == $subtree_root));
              }
            else
              { # case where the element has no child and no next sibling:
                # get the first next sibling of an ancestor, checking subtree_root 
                
                # case where elt is subtree_root, is empty and has no sibling
                last if( $subtree_root && ($elt == $subtree_root));
               
                # backtrack until we find a parent with a next sibling
                $next_elt= $elt->{parent} || last undef;
                until( $next_elt->{next_sibling})
                  { last if( $subtree_root && ($subtree_root == $next_elt));
                    $next_elt= $next_elt->{parent} || last;
                  }
                last if( $subtree_root && ($subtree_root == $next_elt)); 
                $next_elt= $next_elt->{next_sibling};   
              }  
            $elt= $next_elt || last;
            push @descendants, $elt if( !$cond || ($elt->{gi} eq $ind));
          }
      }
    else
    # end optimization
      { # branch for a complex condition: use the regular (slow but simple) way
        while( $elt= $elt->next_elt( $subtree_root, $cond))
          { push @descendants, $elt; }
      }
    return @descendants;
  }

 
sub descendants_or_self
  { my( $elt, $cond)= @_;
    my @descendants= $elt->passes( $cond) ? ($elt) : (); 
    push @descendants, $elt->descendants( $cond);
    return @descendants;
  }
  
sub sibling
  { my $elt= shift;
    my $nb= shift;
    if( $nb > 0)
      { foreach( 1..$nb)
          { $elt= $elt->next_sibling( @_) or return undef; }
      }
    elsif( $nb < 0)
      { foreach( 1..(-$nb))
          { $elt= $elt->prev_sibling( @_) or return undef; }
      }
    else # $nb == 0
      { return $elt->passes( $_[0]); }
    return $elt;
  }

sub sibling_text
  { my $elt= sibling( @_);
    return $elt ? $elt->text : undef;
  }


sub child
  { my $elt= shift;
    my $nb= shift;
    if( $nb >= 0)
      { $elt= $elt->first_child( @_) or return undef;
        foreach( 1..$nb)
          { $elt= $elt->next_sibling( @_) or return undef; }
      }
    else
      { $elt= $elt->last_child( @_) or return undef;
        foreach( 2..(-$nb))
          { $elt= $elt->prev_sibling( @_) or return undef; }
      }
    return $elt;
  }

sub prev_siblings
  { my $elt= shift;
    my @siblings=();
    while( $elt= $elt->prev_sibling( @_))
      { unshift @siblings, $elt; }
    return @siblings;
  }

sub siblings
  { my $elt= shift;
    return grep { $_ ne $elt } $elt->{parent}->children( @_);
  }

sub pos
  { my $elt= shift;
    return 0 if ($_[0] && !$elt->matches( @_));
    my $pos=1;
    $pos++ while( $elt= $elt->prev_sibling( @_));
    return $pos;
  }


sub next_siblings
  { my $elt= shift;
    my @siblings=();
    while( $elt= $elt->next_sibling( @_))
      { push @siblings, $elt; }
    return @siblings;
  }


# used by get_xpath: parses the xpath expression and generates a sub that performs the
# search
{ my %axis2method;
  BEGIN { %axis2method= ( child               => 'children',
                          descendant          => 'descendants',
                         'descendant-or-self' => 'descendants_or_self',
                          parent              => 'parent_is',
                          ancestor            => 'ancestors',
                         'ancestor-or-self'   => 'ancestors_or_self',
                         'following-sibling'  => 'next_siblings',
                         'preceding-sibling'  => 'prev_siblings',
                          following           => 'following_elts',
                          preceding           => 'preceding_elts',
                          self                => '_self',
                        );
        }

  sub _install_xpath
    { my( $xpath_exp, $type)= @_;
      my $original_exp= $xpath_exp;
      my $sub= 'my $elt= shift; my @results;';
      
      # grab the root if expression starts with a /
      if( $xpath_exp=~ s{^/}{})
        { $sub .= '@results= ($elt->twig) || croak "cannot use an XPath query starting with a / on a node not attached to a whole twig";'; }
      elsif( $xpath_exp=~ s{^\./}{})
        { $sub .= '@results= ($elt);'; }
      else
        { $sub .= '@results= ($elt);'; }
  
 
     #warn "xpath_exp= '$xpath_exp'\n";

      while( $xpath_exp &&
             $xpath_exp=~s{^\s*(/?)                            
                            # the xxx=~/regexp/ is a pain as it includes /  
                            (\s*(?:(?:($REG_AXIS)::)?(\*|$REG_TAG_PART|\.\.|\.)\s*)?($REG_PREDICATE_ALT*)
                            )
                            (/|$)}{}xo)
  
        { my( $wildcard, $sub_exp, $axis, $gi, $predicates)= ($1, $2, $3, $4, $5);
           if( $axis && ! $gi)
                { _croak_and_doublecheck_xpath( $original_exp, "error in xpath expression $original_exp"); }

          # grab a parent
          if( $sub_exp eq '..')
            { _croak_and_doublecheck_xpath( $original_exp, "error in xpath expression $original_exp") if( $wildcard);
              $sub .= '@results= map { $_->{parent}} @results;';
            }
          # test the element itself
          elsif( $sub_exp=~ m{^\.(.*)$}s)
            { $sub .= "\@results= grep { \$_->matches( q{$1}) } \@results;" }
          # grab children
          else       
            {
              if( !$axis)             
                { $axis= $wildcard ? 'descendant' : 'child'; }
              if( !$gi or $gi eq '*') { $gi=''; }
              my $function;
  
              # "special" predicates, that return just one element
              if( $predicates && ($predicates =~ m{^\s*\[\s*((-\s*)?\d+)\s*\]\s*$}))
                { # [<nb>]
                  my $offset= $1;
                  $offset-- if( $offset > 0);
                  $function=  $axis eq 'descendant' ? "next_n_elt( $offset, '$gi')" 
                           :  $axis eq 'child'      ? "child( $offset, '$gi')"
                           :                          _croak_and_doublecheck_xpath( $original_exp, "error [$1] not supported along axis '$axis'")
                           ;
                  $sub .= "\@results= grep { \$_ } map { \$_->$function } \@results;"
                }
              elsif( $predicates && ($predicates =~ m{^\s*\[\s*last\s*\(\s*\)\s*\]\s*$}) )
                { # last()
                  _croak_and_doublecheck_xpath( $original_exp, "error in xpath expression $original_exp, usage of // and last() not supported") if( $wildcard);
                   $sub .= "\@results= map { \$_->last_child( '$gi') } \@results;";
                }
              else
                { # follow the axis
                  #warn "axis: '$axis' - method: '$axis2method{$axis}' - gi: '$gi'\n";

                  my $follow_axis= " \$_->$axis2method{$axis}( '$gi')";
                  my $step= $follow_axis;
                  
                  # now filter using the predicate
                  while( $predicates=~ s{^\s*($REG_PREDICATE_ALT)\s*}{}o)
                    { my $pred= $1;
                      $pred=~ s{^\s*\[\s*}{};
                      $pred=~ s{\s*\]\s*$}{};
                      my $test="";
                      my $pos;
                      if( $pred=~ m{^(-?\s*\d+)$})
                        { my $pos= $1;
                          if( $step=~ m{^\s*grep(.*) (\$_->\w+\(\s*'[^']*'\s*\))})
                            { $step= "XML::Twig::_first_n $1 $pos, $2"; }
                          else
                            { if( $pos > 0) { $pos--; }
                              $step= "($step)[$pos]"; 
                            }
                          #warn "number predicate '$pos' - generated step '$step'\n";
                        }
                      else
                        { my $syntax_error=0;
                          do
                            { if( $pred =~ s{^string\(\s*\)\s*=\s*($REG_STRING)\s*}{}o)  # string()="string" pred
                                { $test .= "\$_->text eq $1"; }
                              elsif( $pred =~ s{^string\(\s*\)\s*!=\s*($REG_STRING)\s*}{}o)  # string()!="string" pred
                                { $test .= "\$_->text ne $1"; }
                              if( $pred =~ s{^string\(\s*\)\s*=\s*($REG_NUMBER)\s*}{}o)  # string()=<number> pred
                                { $test .= "\$_->text eq $1"; }
                              elsif( $pred =~ s{^string\(\s*\)\s*!=\s*($REG_NUMBER)\s*}{}o)  # string()!=<number> pred
                                { $test .= "\$_->text ne $1"; }
                              elsif( $pred =~ s{^string\(\s*\)\s*(>|<|>=|<=)\s*($REG_NUMBER)\s*}{}o)  # string()!=<number> pred
                                { $test .= "\$_->text $1 $2"; }

                             elsif( $pred =~ s{^string\(\s*\)\s*($REG_MATCH)\s*($REG_REGEXP)\s*}{}o)  # string()=~/regex/ pred
                                { my( $match, $regexp)= ($1, $2);
                                  $test .= "\$_->text $match $regexp"; 
                                }
                              elsif( $pred =~ s{^string\(\s*\)\s*}{}o)  # string() pred
                                { $test .= "\$_->text"; }
                             elsif( $pred=~ s{^@($REG_TAG_NAME)\s*($REG_OP)\s*($REG_STRING|$REG_NUMBER)}{}o)  # @att="val" pred
                                { my( $att, $oper, $val)= ($1, _op( $2), $3);
                                  $test .= qq{((defined \$_->{'att'}->{"$att"})  && (\$_->{'att'}->{"$att"} $oper $val))};
                                }
                             elsif( $pred =~ s{^@($REG_TAG_NAME)\s*($REG_MATCH)\s*($REG_REGEXP)\s*}{}o)  # @att=~/regex/ pred XXX
                                { my( $att, $match, $regexp)= ($1, $2, $3);
                                  $test .= qq{((defined \$_->{'att'}->{"$att"})  && (\$_->{'att'}->{"$att"} $match $regexp))};; 
                                }
                             elsif( $pred=~ s{^@($REG_TAG_NAME)\s*}{}o)                      # @att pred
                                { $test .= qq{(defined \$_->{'att'}->{"$1"})}; }
                             elsif( $pred=~ s{^\s*(?:not|!)\s*@($REG_TAG_NAME)\s*}{}o)       # not @att pred
                                { $test .= qq{((\$_->is_elt) && (not defined \$_->{'att'}->{"$1"}))}; }
                              elsif( $pred=~ s{^\s*([()])}{})                            # ( or ) (just add to the test)
                                { $test .= qq{$1};           }
                              elsif( $pred=~ s{^\s*(and|or)\s*}{})
                                { $test .= lc " $1 "; }
                              else
                                { $syntax_error=1; }
                             
                             } while( !$syntax_error && $pred);
                           _croak_and_doublecheck_xpath( $original_exp, "error in xpath expression $original_exp at $pred") if( $pred);
                           $step= " grep { $test } $step ";
                        }
                    }
                  #warn "step: '$step'";
                  $sub .= "\@results= grep defined, map { $step } \@results;"; 
                }
            }
        }
  
      if( $xpath_exp)
        { _croak_and_doublecheck_xpath( $original_exp, "error in xpath expression $original_exp around $xpath_exp"); }
        
      $sub .= q{return XML::Twig::_unique_elts( @results); };
      #warn "generated: '$sub'\n";
      my $s= eval "sub { $NO_WARNINGS; $sub }";
      if( $@) 
        { _croak_and_doublecheck_xpath( $original_exp, "error in xpath expression $original_exp ($@);") }
      return( $s); 
    }
}

sub _croak_and_doublecheck_xpath
  { my $xpath_expression= shift;
    my $mess= join( "\n", @_);
    if( $XML::Twig::XPath::VERSION || 0) 
      { my $check_twig= XML::Twig::XPath->new;
        if( eval { $check_twig->{twig_xp}->_parse( $xpath_expression) })
          { $mess .= "\nthe expression is a valid XPath statement, and you are using XML::Twig::XPath, but"
                   . "\nyou are using either 'find_nodes' or 'get_xpath' where the method you likely wanted"
                   . "\nto use is 'findnodes', which is the only one that uses the full XPath engine\n";
          }
      }
    croak $mess;
  }
    
    
           
{ # extremely elaborate caching mechanism
  my %xpath; # xpath_expression => subroutine_code;  
  sub get_xpath
    { my( $elt, $xpath_exp, $offset)= @_;
      my $sub= ($xpath{$xpath_exp} ||= _install_xpath( $xpath_exp));
      return $sub->( $elt) unless( defined $offset); 
      my @res= $sub->( $elt);
      return $res[$offset];
    }
}


sub findvalues
  { my $elt= shift;
    return map { $_->text } $elt->get_xpath( @_);
  }

sub findvalue
  { my $elt= shift;
    return join '', map { $_->text } $elt->get_xpath( @_);
  }


# XML::XPath compatibility
sub getElementById     { return $_[0]->twig->elt_id( $_[1]); }
sub getChildNodes      { my @children= do { my $elt= $_[0]; my @children=(); my $child= $elt->{first_child}; while( $child) { push @children, $child; $child= $child->{next_sibling}; } @children; }; return wantarray ? @children : \@children; }

sub _flushed     { return $_[0]->{flushed}; }
sub _set_flushed { $_[0]->{flushed}=1;      }
sub _del_flushed { delete $_[0]->{flushed}; }

sub cut
  { my $elt= shift;
    my( $parent, $prev_sibling, $next_sibling);
    $parent=  $elt->{parent}; 
    if( ! $parent && $elt->is_elt)
      { # are we cutting the root?
        my $t= $elt->{twig};
        if( $t && ! $t->{twig_parsing})
          { delete $t->{twig_root};
            delete $elt->{twig};
            return $elt;
          }  # cutt`ing the root
        else
          { return;  }  # cutting an orphan, returning $elt would break backward compatibility
      }

    # save the old links, that'll make it easier for some loops
    foreach my $link ( qw(parent prev_sibling next_sibling) )
      { $elt->{former}->{$link}= $elt->{$link};
         if( $XML::Twig::weakrefs) { weaken( $elt->{former}->{$link}); }
      }

    # if we cut the current element then its parent becomes the current elt
    if( $elt->{twig_current})
      { my $twig_current= $elt->{parent};
        $elt->twig->{twig_current}= $twig_current;
        $twig_current->{'twig_current'}=1;
        delete $elt->{'twig_current'};
      }

    if( $parent->{first_child} && $parent->{first_child} == $elt)
      { $parent->{first_child}=  $elt->{next_sibling};
        # cutting can make the parent empty
        if( ! $parent->{first_child}) { $parent->{empty}=  1; }
      }

    if( $parent->{last_child} && $parent->{last_child} == $elt)
      {  delete $parent->{empty}; $parent->{last_child}=$elt->{prev_sibling}; if( $XML::Twig::weakrefs) { weaken( $parent->{last_child});} ;
      }

    if( $prev_sibling= $elt->{prev_sibling})
      { $prev_sibling->{next_sibling}=  $elt->{next_sibling}; }
    if( $next_sibling= $elt->{next_sibling})
      { $next_sibling->{prev_sibling}=$elt->{prev_sibling}; if( $XML::Twig::weakrefs) { weaken( $next_sibling->{prev_sibling});} ; }


    $elt->{parent}=undef; if( $XML::Twig::weakrefs) { weaken( $elt->{parent});} ;
    $elt->{prev_sibling}=undef; if( $XML::Twig::weakrefs) { weaken( $elt->{prev_sibling});} ;
    $elt->{next_sibling}=  undef;

    # merge 2 (now) consecutive text nodes if they are of the same type 
    # (type can be PCDATA or CDATA)
    if( $prev_sibling && $next_sibling && $prev_sibling->is_text && ( $XML::Twig::index2gi[$prev_sibling->{'gi'}] eq $XML::Twig::index2gi[$next_sibling->{'gi'}]))
      { $prev_sibling->merge_text( $next_sibling); }

    return $elt;
  }


sub former_next_sibling { return $_[0]->{former}->{next_sibling}; }
sub former_prev_sibling { return $_[0]->{former}->{prev_sibling}; }
sub former_parent       { return $_[0]->{former}->{parent};       }

sub cut_children
  { my( $elt, $exp)= @_;
    my @children= $elt->children( $exp);
    foreach (@children) { $_->cut; }
    if( ! $elt->has_children) { $elt->{empty}=  1; }
    return @children;
  }

sub cut_descendants
  { my( $elt, $exp)= @_;
    my @descendants= $elt->descendants( $exp);
    foreach ($elt->descendants( $exp)) { $_->cut; }
    if( ! $elt->has_children) { $elt->{empty}=  1; }
    return @descendants;
  }


sub erase
  { my $elt= shift;
    #you cannot erase the current element
    if( $elt->{twig_current})
      { croak "trying to erase an element before it has been completely parsed"; }
    if( my $parent= $elt->{parent})
      { # normal case
        $elt->_move_extra_data_after_erase;
        my @children= do { my $elt= $elt; my @children=(); my $child= $elt->{first_child}; while( $child) { push @children, $child; $child= $child->{next_sibling}; } @children; };
        if( @children)
          { 
            # elt has children, move them up

            # the first child may need to be merged with a previous text
            my $first_child= shift @children;
            $first_child->move( before => $elt);
            my $prev= $first_child->{prev_sibling};
            if( $prev && $prev->is_text && ($XML::Twig::index2gi[$first_child->{'gi'}] eq $XML::Twig::index2gi[$prev->{'gi'}]) )
              { $prev->merge_text( $first_child); }

            # move the rest of the children
            foreach my $child (@children) 
              { $child->move( before => $elt); }

            # now the elt had no child, delete it
            $elt->delete;

            # now see if we need to merge the last child with the next element
            my $last_child= $children[-1] || $first_child; # if no last child, then it's also the first child
            my $next= $last_child->{next_sibling};
            if( $next && $next->is_text && ($XML::Twig::index2gi[$last_child->{'gi'}] eq $XML::Twig::index2gi[$next->{'gi'}]) )
              { $last_child->merge_text( $next); }

            # if parsing and have now a PCDATA text, mark so we can normalize later on if need be
            if( $parent->{twig_current} && $last_child->is_text) {  $parent->{twig_to_be_normalized}=1; }
          }
       else
         { # no children, just cut the elt
           $elt->delete;
         }
      }
    else     
      { # trying to erase the root (of a twig or of a cut/new element)
        my @children= do { my $elt= $elt; my @children=(); my $child= $elt->{first_child}; while( $child) { push @children, $child; $child= $child->{next_sibling}; } @children; };
        unless( @children == 1)
          { croak "can only erase an element with no parent if it has a single child"; }
        $elt->_move_extra_data_after_erase;
        my $child= shift @children;
        $child->{parent}=undef; if( $XML::Twig::weakrefs) { weaken( $child->{parent});} ;
        my $twig= $elt->twig;
        $twig->set_root( $child);
      }

    return $elt;

  }

sub _move_extra_data_after_erase
  { my( $elt)= @_;
    # extra_data
    if( my $extra_data= $elt->{extra_data}) 
      { my $target= $elt->{first_child} || $elt->{next_sibling};
        if( $target)
          {
            if( $target->is( $ELT))
              { $target->set_extra_data( $extra_data . ($target->extra_data || '')); }
            elsif( $target->is( $TEXT))
              { $target->_unshift_extra_data_in_pcdata( $extra_data, 0); }  # TO CHECK
          }
        else
          { my $parent= $elt->{parent}; # always exists or the erase cannot be performed
            $parent->_prefix_extra_data_before_end_tag( $extra_data); 
          }
      }
      
     # extra_data_before_end_tag
    if( my $extra_data= $elt->{extra_data_before_end_tag}) 
      { if( my $target= $elt->{next_sibling})
          { if( $target->is( $ELT))
              { $target->set_extra_data( $extra_data . ($target->extra_data || '')); }
            elsif( $target->is( $TEXT))
              { 
                $target->_unshift_extra_data_in_pcdata( $extra_data, 0);
             }
          }
        elsif( my $parent= $elt->{parent})
          { $parent->_prefix_extra_data_before_end_tag( $extra_data); }
       }

    return $elt;

  }
BEGIN
  { my %method= ( before      => \&paste_before,
                  after       => \&paste_after,
                  first_child => \&paste_first_child,
                  last_child  => \&paste_last_child,
                  within      => \&paste_within,
        );
    
    # paste elt somewhere around ref
    # pos can be first_child (default), last_child, before, after or within
    sub paste ## no critic (Subroutines::ProhibitNestedSubs);
      { my $elt= shift;
        if( $elt->{parent}) 
          { croak "cannot paste an element that belongs to a tree"; }
        my $pos;
        my $ref;
        if( ref $_[0]) 
          { $pos= 'first_child'; 
            croak "wrong argument order in paste, should be $_[1] first" if($_[1]); 
          }
        else
          { $pos= shift; }

        if( my $method= $method{$pos})
          {
            unless( ref( $_[0]) && isa( $_[0], 'XML::Twig::Elt'))
              { if( ! defined( $_[0]))
                  { croak "missing target in paste"; }
                elsif( ! ref( $_[0]))
                  { croak "wrong target type in paste (not a reference), should be XML::Twig::Elt or a subclass"; }
                else
                  { my $ref= ref $_[0];
                    croak "wrong target type in paste: '$ref', should be XML::Twig::Elt or a subclass";
                  }
              }
            $ref= $_[0];
            # check here so error message lists the caller file/line
            if( !$ref->{parent} && ($pos=~ m{^(before|after)$}) && !(exists $elt->{'target'}) && !(exists $elt->{'comment'})) 
              { croak "cannot paste $1 root"; }
            $elt->$method( @_); 
          }
        else
          { croak "tried to paste in wrong position '$pos', allowed positions " . 
              " are 'first_child', 'last_child', 'before', 'after' and "    .
              "'within'";
          }
        if( (my $ids= $elt->{twig_id_list}) && (my $t= $ref->twig) )
          { $t->{twig_id_list}||={};
            foreach my $id (keys %$ids)
              { $t->{twig_id_list}->{$id}= $ids->{$id}; 
                if( $XML::Twig::weakrefs) { weaken( $t->{twig_id_list}->{$id}); }
              }
          }
        return $elt;
      }
  

    sub paste_before
      { my( $elt, $ref)= @_;
        my( $parent, $prev_sibling, $next_sibling );
        
        # trying to paste before an orphan (root or detached wlt)
        unless( $ref->{parent}) 
          { if( my $t= $ref->twig)
              { if( (exists $elt->{'comment'}) || (exists $elt->{'target'})) # we can still do this
                  { $t->_add_cpi_outside_of_root( leading_cpi => $elt); return; }
                else
                  { croak "cannot paste before root"; }
              }
            else
              { croak "cannot paste before an orphan element"; }
          }
        $parent= $ref->{parent};
        $prev_sibling= $ref->{prev_sibling};
        $next_sibling= $ref;

        $elt->{parent}=$parent; if( $XML::Twig::weakrefs) { weaken( $elt->{parent});} ;
        if( $parent->{first_child} == $ref) { $parent->{first_child}=  $elt; }

        if( $prev_sibling) { $prev_sibling->{next_sibling}=  $elt; }
        $elt->{prev_sibling}=$prev_sibling; if( $XML::Twig::weakrefs) { weaken( $elt->{prev_sibling});} ;

        $next_sibling->{prev_sibling}=$elt; if( $XML::Twig::weakrefs) { weaken( $next_sibling->{prev_sibling});} ;
        $elt->{next_sibling}=  $ref;
        return $elt;
      }
     
     sub paste_after
      { my( $elt, $ref)= @_;
        my( $parent, $prev_sibling, $next_sibling );

        # trying to paste after an orphan (root or detached wlt)
        unless( $ref->{parent}) 
            { if( my $t= $ref->twig)
                { if( (exists $elt->{'comment'}) || (exists $elt->{'target'})) # we can still do this
                    { $t->_add_cpi_outside_of_root( trailing_cpi => $elt); return; }
                  else
                    { croak "cannot paste after root"; }
                }
              else
                { croak "cannot paste after an orphan element"; }
            }
        $parent= $ref->{parent};
        $prev_sibling= $ref;
        $next_sibling= $ref->{next_sibling};

        $elt->{parent}=$parent; if( $XML::Twig::weakrefs) { weaken( $elt->{parent});} ;
        if( $parent->{last_child}== $ref) {  delete $parent->{empty}; $parent->{last_child}=$elt; if( $XML::Twig::weakrefs) { weaken( $parent->{last_child});} ; }

        $prev_sibling->{next_sibling}=  $elt;
        $elt->{prev_sibling}=$prev_sibling; if( $XML::Twig::weakrefs) { weaken( $elt->{prev_sibling});} ;

        if( $next_sibling) { $next_sibling->{prev_sibling}=$elt; if( $XML::Twig::weakrefs) { weaken( $next_sibling->{prev_sibling});} ; }
        $elt->{next_sibling}=  $next_sibling;
        return $elt;

      }

    sub paste_first_child
      { my( $elt, $ref)= @_;
        my( $parent, $prev_sibling, $next_sibling );
        $parent= $ref;
        $next_sibling= $ref->{first_child};

        $elt->{parent}=$parent; if( $XML::Twig::weakrefs) { weaken( $elt->{parent});} ;
        $parent->{first_child}=  $elt;
        unless( $parent->{last_child}) {  delete $parent->{empty}; $parent->{last_child}=$elt; if( $XML::Twig::weakrefs) { weaken( $parent->{last_child});} ; }

        $elt->{prev_sibling}=undef; if( $XML::Twig::weakrefs) { weaken( $elt->{prev_sibling});} ;

        if( $next_sibling) { $next_sibling->{prev_sibling}=$elt; if( $XML::Twig::weakrefs) { weaken( $next_sibling->{prev_sibling});} ; }
        $elt->{next_sibling}=  $next_sibling;
        return $elt;
      }
      
    sub paste_last_child
      { my( $elt, $ref)= @_;
        my( $parent, $prev_sibling, $next_sibling );
        $parent= $ref;
        $prev_sibling= $ref->{last_child};

        $elt->{parent}=$parent; if( $XML::Twig::weakrefs) { weaken( $elt->{parent});} ;
         delete $parent->{empty}; $parent->{last_child}=$elt; if( $XML::Twig::weakrefs) { weaken( $parent->{last_child});} ;
        unless( $parent->{first_child}) { $parent->{first_child}=  $elt; }

        $elt->{prev_sibling}=$prev_sibling; if( $XML::Twig::weakrefs) { weaken( $elt->{prev_sibling});} ;
        if( $prev_sibling) { $prev_sibling->{next_sibling}=  $elt; }

        $elt->{next_sibling}=  undef;
        return $elt;
      }

    sub paste_within
      { my( $elt, $ref, $offset)= @_;
        my $text= $ref->is_text ? $ref : $ref->next_elt( $TEXT, $ref);
        my $new= $text->split_at( $offset);
        $elt->paste_before( $new);
        return $elt;
      }
  }

# load an element into a structure similar to XML::Simple's
sub simplify
  { my $elt= shift;

    # normalize option names
    my %options= @_;
    %options= map { my ($key, $val)= ($_, $options{$_});
                       $key=~ s{(\w)([A-Z])}{$1_\L$2}g;
                       $key => $val
                     } keys %options;

    # check options
    my @allowed_options= qw( keyattr forcearray noattr content_key
                             var var_regexp variables var_attr 
                             group_tags forcecontent
                             normalise_space normalize_space
                   );
    my %allowed_options= map { $_ => 1 } @allowed_options;
    foreach my $option (keys %options)
      { carp "invalid option $option\n" unless( $allowed_options{$option}); }

    $options{normalise_space} ||= $options{normalize_space} || 0;
    
    $options{content_key} ||= 'content';
    if( $options{content_key}=~ m{^-})
      { # need to remove the - and to activate extra folding
        $options{content_key}=~ s{^-}{};
        $options{extra_folding}= 1;
      }
    else
      { $options{extra_folding}= 0; }
   
    $options{forcearray} ||=0; 
    if( isa( $options{forcearray}, 'ARRAY'))
      { my %forcearray_tags= map { $_ => 1 } @{$options{forcearray}};
        $options{forcearray_tags}= \%forcearray_tags;
        $options{forcearray}= 0;
      }

    $options{keyattr}     ||= ['name', 'key', 'id'];
    if( ref $options{keyattr} eq 'ARRAY')
      { foreach my $keyattr (@{$options{keyattr}})
          { my( $prefix, $att)= ($keyattr=~ m{^([+-])?(.*)});
            $prefix ||= '';
            $options{key_for_all}->{$att}= 1;
            $options{remove_key_for_all}->{$att}=1 unless( $prefix eq '+');
            $options{prefix_key_for_all}->{$att}=1 if( $prefix eq '-');
          }
      }
    elsif( ref $options{keyattr} eq 'HASH')
      { while( my( $elt, $keyattr)= each %{$options{keyattr}})
         { my( $prefix, $att)= ($keyattr=~ m{^([+-])?(.*)});
           $prefix ||='';
           $options{key_for_elt}->{$elt}= $att;
           $options{remove_key_for_elt}->{"$elt#$att"}=1 unless( $prefix);
           $options{prefix_key_for_elt}->{"$elt#$att"}=1 if( $prefix eq '-');
         }
      }
       

    $options{var}||= $options{var_attr}; # for compat with XML::Simple
    if( $options{var}) { $options{var_values}= {}; }
    else               { $options{var}='';         }

    if( $options{variables}) 
      { $options{var}||= 1;
        $options{var_values}= $options{variables};
      }

    if( $options{var_regexp} and !$options{var})
      { warn "var option not used, var_regexp option ignored\n"; }
    $options{var_regexp} ||= '\$\{?(\w+)\}?';
      
    $elt->_simplify( \%options);
 
 }

sub _simplify
  { my( $elt, $options)= @_;

    my $data;

    my $gi= $XML::Twig::index2gi[$elt->{'gi'}];
    my @children= do { my $elt= $elt; my @children=(); my $child= $elt->{first_child}; while( $child) { push @children, $child; $child= $child->{next_sibling}; } @children; };
    my %atts= $options->{noattr} || !$elt->{att} ? () : %{$elt->{att}};
    my $nb_atts= keys %atts;
    my $nb_children= $elt->children_count + $nb_atts;

    my %nb_children;
    foreach (@children)   { $nb_children{$_->tag}++; }
    foreach (keys %atts)  { $nb_children{$_}++;      }

    my $arrays; # tag => array where elements are stored


    # store children
    foreach my $child (@children)
      { if( $child->is_text)
          { # generate with a content key
            my $text= $elt->_text_with_vars( $options);
            if( $options->{normalise_space} >= 2) { $text= _normalize_space( $text); }
            if(    $options->{force_content}
                || $nb_atts 
                || (scalar @children > 1)
              )
              { $data->{$options->{content_key}}= $text; }
            else
              { $data= $text; }
          }
        else
          { # element with sub-elements
            my $child_gi= $XML::Twig::index2gi[$child->{'gi'}];

            my $child_data= $child->_simplify( $options);

            # first see if we need to simplify further the child data
            # simplify because of grouped tags
            if( my $grouped_tag= $options->{group_tags}->{$child_gi})
              { # check that the child data is a hash with a single field
                unless(    (ref( $child_data) eq 'HASH')
                        && (keys %$child_data == 1)
                        && defined ( my $grouped_child_data= $child_data->{$grouped_tag})
                      )
                  { croak "error in grouped tag $child_gi"; }
                else
                  { $child_data=  $grouped_child_data; }
              }
            # simplify because of extra folding
            if( $options->{extra_folding})
              { if(    (ref( $child_data) eq 'HASH')
                    && (keys %$child_data == 1)
                    && defined( my $content= $child_data->{$options->{content_key}})
                  )
                  { $child_data= $content; }
              }

            if( my $keyatt= $child->_key_attr( $options))
              { # simplify element with key
                my $key= $child->{'att'}->{$keyatt};
                if( $options->{normalise_space} >= 1) { $key= _normalize_space( $key); }
                $data->{$child_gi}->{$key}= $child_data;
              }
            elsif(      $options->{forcearray}
                   ||   $options->{forcearray_tags}->{$child_gi}
                   || ( $nb_children{$child_gi} > 1)
                 )
              { # simplify element to store in an array
                if( defined $child_data && $child_data ne "" )
                  { $data->{$child_gi} ||= [];
                    push @{$data->{$child_gi}}, $child_data;
                  }
                else
                  { $data->{$child_gi}= [{}]; }
              }
            else
              { # simplify element to store as a hash field
                $data->{$child_gi}=$child_data;
                $data->{$child_gi}= defined $child_data && $child_data ne "" ? $child_data : {};
              }
          }
    }

    # store atts
    # TODO: deal with att that already have an element by that name
    foreach my $att (keys %atts)
      { # do not store if the att is a key that needs to be removed
        if(    $options->{remove_key_for_all}->{$att}
            || $options->{remove_key_for_elt}->{"$gi#$att"}
          )
          { next; }

        my $att_text= $options->{var} ?  _replace_vars_in_text( $atts{$att}, $options) : $atts{$att} ;
        if( $options->{normalise_space} >= 2) { $att_text= _normalize_space( $att_text); }
        
        if(    $options->{prefix_key_for_all}->{$att}
            || $options->{prefix_key_for_elt}->{"$gi#$att"}
          )
          { # prefix the att
            $data->{"-$att"}= $att_text;
          } 
        else
          { # normal case
            $data->{$att}= $att_text; 
          }
      }
    
    return $data;
  }

sub _key_attr
  { my( $elt, $options)=@_;
    return if( $options->{noattr});
    if( $options->{key_for_all})
      { foreach my $att ($elt->att_names)
          { if( $options->{key_for_all}->{$att})
              { return $att; }
          }
      }
    elsif( $options->{key_for_elt})
      { if( my $key_for_elt= $options->{key_for_elt}->{$XML::Twig::index2gi[$elt->{'gi'}]} )
          { return $key_for_elt if( defined( $elt->{'att'}->{$key_for_elt})); }
      }
    return;
  }

sub _text_with_vars
  { my( $elt, $options)= @_;
    my $text;
    if( $options->{var}) 
      { $text= _replace_vars_in_text( $elt->text, $options); 
        $elt->_store_var( $options);
      }
     else
      { $text= $elt->text; }
    return $text;
  }


sub _normalize_space
  { my $text= shift;
    $text=~ s{\s+}{ }sg;
    $text=~ s{^\s}{};
    $text=~ s{\s$}{};
    return $text;
  }


sub att_nb
  { return 0 unless( my $atts= $_[0]->{att});
    return scalar keys %$atts;
  }
    
sub has_no_atts
  { return 1 unless( my $atts= $_[0]->{att});
    return scalar keys %$atts ? 0 : 1;
  }
    
sub _replace_vars_in_text
  { my( $text, $options)= @_;

    $text=~ s{($options->{var_regexp})}
             { if( defined( my $value= $options->{var_values}->{$2}))
                 { $value }
               else
                 { warn "unknown variable $2\n";
                   $1
                 }
             }gex;
    return $text;
  }

sub _store_var
  { my( $elt, $options)= @_;
    if( defined (my $var_name= $elt->{'att'}->{$options->{var}}))
       { $options->{var_values}->{$var_name}= $elt->text; 
       }
  }


# split a text element at a given offset
sub split_at
  { my( $elt, $offset)= @_;
    my $text_elt= $elt->is_text ? $elt : $elt->first_child( $TEXT) || return '';
    my $string= $text_elt->text; 
    my $left_string= substr( $string, 0, $offset);
    my $right_string= substr( $string, $offset);
    $text_elt->{pcdata}= (delete $text_elt->{empty} || 1) &&  $left_string;
    my $new_elt= $elt->new( $XML::Twig::index2gi[$elt->{'gi'}], $right_string);
    $new_elt->paste( after => $elt);
    return $new_elt;
  }

    
# split an element or its text descendants into several, in place
# all elements (new and untouched) are returned
sub split    
  { my $elt= shift;
    my @text_chunks;
    my @result;
    if( $elt->is_text) { @text_chunks= ($elt); }
    else               { @text_chunks= $elt->descendants( $TEXT); }
    foreach my $text_chunk (@text_chunks)
      { push @result, $text_chunk->_split( 1, @_); }
    return @result;
  }

# split an element or its text descendants into several, in place
# created elements (those which match the regexp) are returned
sub mark
  { my $elt= shift;
    my @text_chunks;
    my @result;
    if( $elt->is_text) { @text_chunks= ($elt); }
    else               { @text_chunks= $elt->descendants( $TEXT); }
    foreach my $text_chunk (@text_chunks)
      { push @result, $text_chunk->_split( 0, @_); }
    return @result;
  }

# split a single text element
# return_all defines what is returned: if it is true 
# only returns the elements created by matches in the split regexp
# otherwise all elements (new and untouched) are returned


{ 
 
  sub _split
    { my $elt= shift;
      my $return_all= shift;
      my $regexp= shift;
      my @tags;

      while( @_)
        { my $tag= shift();
          if( ref $_[0]) 
            { push @tags, { tag => $tag, atts => shift }; }
          else
            { push @tags, { tag => $tag }; }
        }

      unless( @tags) { @tags= { tag => $elt->{parent}->gi }; }
          
      my @result;                                 # the returned list of elements
      my $text= $elt->text;
      my $gi= $XML::Twig::index2gi[$elt->{'gi'}];
  
      # 2 uses: if split matches then the first substring reuses $elt
      #         once a split has occurred then the last match needs to be put in
      #         a new element      
      my $previous_match= 0;

      while( my( $pre_match, @matches)= $text=~ /^(.*?)$regexp(.*)$/gcs)
        { $text= pop @matches;
          if( $previous_match)
            { # match, not the first one, create a new text ($gi) element
              _utf8_ify( $pre_match) if( $] < 5.010);
              $elt= $elt->insert_new_elt( after => $gi, $pre_match);
              push @result, $elt if( $return_all);
            }
          else
            { # first match in $elt, re-use $elt for the first sub-string
              _utf8_ify( $pre_match) if( $] < 5.010);
              $elt->set_text( $pre_match);
              $previous_match++;                # store the fact that there was a match
              push @result, $elt if( $return_all);
            }

          # now deal with matches captured in the regexp
          if( @matches)
            { # match, with capture
              my $i=0;
              foreach my $match (@matches)
                { # create new element, text is the match
                  _utf8_ify( $match) if( $] < 5.010);
                  my $tag  = _repl_match( $tags[$i]->{tag}, @matches) || '#PCDATA';
                  my $atts = \%{$tags[$i]->{atts}} || {};
                  my %atts= map { _repl_match( $_, @matches) => _repl_match( $atts->{$_}, @matches) } keys %$atts;
                  $elt= $elt->insert_new_elt( after => $tag, \%atts, $match);
                  push @result, $elt;
                  $i= ($i + 1) % @tags;
                }
            }
          else
            { # match, no captures
              my $tag  = $tags[0]->{tag};
              my $atts = \%{$tags[0]->{atts}} || {};
              $elt=  $elt->insert_new_elt( after => $tag, $atts);
              push @result, $elt;
            }
        }
      if( $previous_match && $text)
        { # there was at least 1 match, and there is text left after the match
          $elt= $elt->insert_new_elt( after => $gi, $text);
        }

      push @result, $elt if( $return_all);

      return @result; # return all elements
   }

sub _repl_match
  { my( $val, @matches)= @_;
    $val=~ s{\$(\d+)}{$matches[$1-1]}g;
    return $val;
  }

  # evil hack needed as sometimes 
  my $encode_is_loaded=0;   # so we only load Encode once
  sub _utf8_ify
    { 
      if( $perl_version >= 5.008 and $perl_version < 5.010 and !_keep_encoding()) 
        { unless( $encode_is_loaded) { require Encode; import Encode; $encode_is_loaded++; }
          Encode::_utf8_on( $_[0]); # the flag should be set but is not
        }
    }


}

{ my %replace_sub; # cache for complex expressions (expression => sub)

  sub subs_text
    { my( $elt, $regexp, $replace)= @_;
  
      my $replacement_string;
      my $is_string= _is_string( $replace);

      my @parents;

      foreach my $text_elt ($elt->descendants_or_self( $TEXT))
        { 
          if( $is_string)
            { my $text= $text_elt->text;
              $text=~ s{$regexp}{ _replace_var( $replace, $1, $2, $3, $4, $5, $6, $7, $8, $9)}egx;
              $text_elt->set_text( $text);
           }
          else
            {  
              no utf8; # = perl 5.6
              my $replace_sub= ( $replace_sub{$replace} ||= _install_replace_sub( $replace)); 
              my $text= $text_elt->text;
              my $pos=0;  # used to skip text that was previously matched
              my $found_hit;
              while( my( $pre_match_string, $match_string, @var)= ($text=~ m{(.*?)($regexp)}sg))
                { $found_hit=1;
                  my $match_start  = length( $pre_match_string);
                  my $match        = $match_start ? $text_elt->split_at( $match_start + $pos) : $text_elt;
                  my $match_length = length( $match_string);
                  my $post_match   = $match->split_at( $match_length); 
                  $replace_sub->( $match, @var);

                  # go to next 
                  $text_elt= $post_match;
                  $text= $post_match->text;

                  if( $found_hit) { push @parents, $text_elt->{parent} unless $parents[-1] && $parents[-1]== $text_elt->{parent}; }

                }
            }
        }

      foreach my $parent (@parents) { $parent->normalize; }

      return $elt;
    }


  sub _is_string
    { return ($_[0]=~ m{&e[ln]t}) ? 0: 1 }

  sub _replace_var
    { my( $string, @var)= @_;
      unshift @var, undef;
      $string=~ s{\$(\d)}{$var[$1]}g;
      return $string;
    }

  sub _install_replace_sub
    { my $replace_exp= shift;
      my @item= split m{(&e[ln]t\s*\([^)]*\))}, $replace_exp;
      my $sub= q{ my( $match, @var)= @_; my $new; my $last_inserted=$match;};
      my( $gi, $exp);
      foreach my $item (@item)
        { next if ! length $item;
          if(    $item=~ m{^&elt\s*\(([^)]*)\)})
            { $exp= $1; }
          elsif( $item=~ m{^&ent\s*\(\s*([^\s)]*)\s*\)})
            { $exp= " '#ENT' => $1"; }
          else
            { $exp= qq{ '#PCDATA' => "$item"}; }
          $exp=~ s{\$(\d)}{my $i= $1-1; "\$var[$i]"}eg; # replace references to matches
          $sub.= qq{ \$new= \$match->new( $exp); };
          $sub .= q{ $new->paste( after => $last_inserted); $last_inserted=$new;};
        }
      $sub .= q{ $match->delete; };
      #$sub=~ s/;/;\n/g; warn "subs: $sub"; 
      my $coderef= eval "sub { $NO_WARNINGS; $sub }";
      if( $@) { croak( "invalid replacement expression $replace_exp: ",$@); }
      return $coderef;
    }

  }


sub merge_text
  { my( $e1, $e2)= @_;
    croak "invalid merge: can only merge 2 elements" 
        unless( isa( $e2, 'XML::Twig::Elt'));
    croak "invalid merge: can only merge 2 text elements" 
        unless( $e1->is_text && $e2->is_text && ($e1->gi eq $e2->gi));

    my $t1_length= length( $e1->text);

    $e1->set_text( $e1->text . $e2->text);

    if( my $extra_data_in_pcdata= $e2->_extra_data_in_pcdata)
      { foreach my $data (@$extra_data_in_pcdata) { $e1->_push_extra_data_in_pcdata( $data->{text}, $data->{offset} + $t1_length); } }

    $e2->delete;

    return $e1;
  }

sub merge
  { my( $e1, $e2)= @_;
    my @e2_children= $e2->_children;
    if(     $e1->_last_child && $e1->_last_child->is_pcdata
        &&  @e2_children && $e2_children[0]->is_pcdata
      )
      { my $t1_length= length( $e1->_last_child->{pcdata});
        my $child1= $e1->_last_child; 
        my $child2= shift @e2_children; 
        $child1->{pcdata} .= $child2->{pcdata}; 

        my $extra_data= $e1->_extra_data_before_end_tag . $e2->extra_data;

        if( $extra_data) 
          { $e1->_del_extra_data_before_end_tag;
            $child1->_push_extra_data_in_pcdata( $extra_data, $t1_length); 
          }

        if( my $extra_data_in_pcdata= $child2->_extra_data_in_pcdata)
          { foreach my $data (@$extra_data_in_pcdata) { $child1->_push_extra_data_in_pcdata( $data->{text}, $data->{offset} + $t1_length); } }

        if( my $extra_data_before_end_tag= $e2->_extra_data_before_end_tag) 
          { $e1->_set_extra_data_before_end_tag( $extra_data_before_end_tag); }
      }

    foreach my $e (@e2_children) { $e->move( last_child => $e1); } 

    $e2->delete;
    return $e1;
  }


# recursively copy an element and returns the copy (can be huge and long)
sub copy
  { my $elt= shift;
    my $copy= $elt->new( $XML::Twig::index2gi[$elt->{'gi'}]);

    if( $elt->extra_data) { $copy->set_extra_data( $elt->extra_data); }
    if( $elt->{extra_data_before_end_tag}) { $copy->_set_extra_data_before_end_tag( $elt->{extra_data_before_end_tag}); }

    if( $elt->is_asis)   { $copy->set_asis; }

    if( (exists $elt->{'pcdata'})) 
      { $copy->{pcdata}= (delete $copy->{empty} || 1) &&  $elt->{pcdata}; 
        if( $elt->{extra_data_in_pcdata}) { $copy->_set_extra_data_in_pcdata( $elt->{extra_data_in_pcdata}); }
      }
    elsif( (exists $elt->{'cdata'}))
      { $copy->{cdata}=  $elt->{cdata}; 
        if( $elt->{extra_data_in_pcdata}) { $copy->_set_extra_data_in_pcdata( $elt->{extra_data_in_pcdata}); }
      }
    elsif( (exists $elt->{'target'}))
      { $copy->_set_pi( $elt->{target}, $elt->{data}); }
    elsif( (exists $elt->{'comment'}))
      { $copy->{comment}=  $elt->{comment}; }
    elsif( (exists $elt->{'ent'}))
      { $copy->{ent}=  $elt->{ent}; }
    else
      { my @children= do { my $elt= $elt; my @children=(); my $child= $elt->{first_child}; while( $child) { push @children, $child; $child= $child->{next_sibling}; } @children; };
        if( my $atts= $elt->{att})
          { my %atts;
            tie %atts, 'Tie::IxHash' if (keep_atts_order());
            %atts= %{$atts}; # we want to do a real copy of the attributes
            $copy->set_atts( \%atts);
          }
        foreach my $child (@children)
          { my $child_copy= $child->copy;
            $child_copy->paste( 'last_child', $copy);
          }
      }
    # save links to the original location, which can be convenient and is used for namespace resolution
    foreach my $link ( qw(parent prev_sibling next_sibling) )
      { $copy->{former}->{$link}= $elt->{$link};
        if( $XML::Twig::weakrefs) { weaken( $copy->{former}->{$link}); }
      }

    $copy->{empty}=  $elt->{'empty'};

    return $copy;
  }


sub delete
  { my $elt= shift;
    $elt->cut;
    $elt->DESTROY unless $XML::Twig::weakrefs;
    return undef;
  }

sub __destroy
  { my $elt= shift; 
    return if( $XML::Twig::weakrefs); 
    my $t= shift || $elt->twig; # optional argument, passed in recursive calls
  
    foreach( @{[$elt->_children]}) { $_->DESTROY( $t); }
  
    # the id reference needs to be destroyed
    # lots of tests to avoid warnings during the cleanup phase
    $elt->del_id( $t) if( $ID && $t && defined( $elt->{att}) && exists( $elt->{att}->{$ID}));
    if( $elt->{former}) { foreach (keys %{$elt->{former}}) { delete $elt->{former}->{$_}; } delete $elt->{former}; } 
    foreach (qw( keys %$elt)) { delete $elt->{$_}; }
    undef $elt;
  }

BEGIN
{ sub set_destroy { if( $XML::Twig::weakrefs) { undef *DESTROY } else { *DESTROY= *__destroy; } }
  set_destroy();
}

# ignores the element
sub ignore
  { my $elt= shift;
    my $t= $elt->twig;
    $t->ignore( $elt, @_);
  }

BEGIN {
  my $pretty                    = 0;
  my $quote                     = '"';
  my $INDENT                    = '  ';
  my $empty_tag_style           = 0;
  my $remove_cdata              = 0;
  my $keep_encoding             = 0;
  my $expand_external_entities  = 0;
  my $keep_atts_order           = 0;
  my $do_not_escape_amp_in_atts = 0;
  my $WRAP                      = '80';
  my $REPLACED_ENTS             = qq{&<};

  my ($NSGMLS, $NICE, $INDENTED, $INDENTEDCT, $INDENTEDC, $WRAPPED, $RECORD1, $RECORD2, $INDENTEDA)= (1..9);
  my %KEEP_TEXT_TAG_ON_ONE_LINE= map { $_ => 1 } ( $INDENTED, $INDENTEDCT, $INDENTEDC, $INDENTEDA, $WRAPPED);
  my %WRAPPED =  map { $_ => 1 } ( $WRAPPED, $INDENTEDA, $INDENTEDC);

  my %pretty_print_style=
    ( none       => 0,          # no added \n
      nsgmls     => $NSGMLS,    # nsgmls-style, \n in tags
      # below this line styles are UNSAFE (the generated XML can be well-formed but invalid)
      nice       => $NICE,      # \n after open/close tags except when the 
                                # element starts with text
      indented   => $INDENTED,  # nice plus idented
      indented_close_tag   => $INDENTEDCT,  # nice plus idented
      indented_c => $INDENTEDC, # slightly more compact than indented (closing
                                # tags are on the same line)
      wrapped    => $WRAPPED,   # text is wrapped at column 
      record_c   => $RECORD1,   # for record-like data (compact)
      record     => $RECORD2,   # for record-like data  (not so compact)
      indented_a => $INDENTEDA, # nice, indented, and with attributes on separate
                                # lines as the nsgmls style, as well as wrapped
                                # lines - to make the xml friendly to line-oriented tools
      cvs        => $INDENTEDA, # alias for indented_a
    );

  my ($HTML, $EXPAND)= (1..2);
  my %empty_tag_style=
    ( normal => 0,        # <tag/>
      html   => $HTML,    # <tag />
      xhtml  => $HTML,    # <tag />
      expand => $EXPAND,  # <tag></tag>
    );

  my %quote_style=
    ( double  => '"',    
      single  => "'", 
      # smart  => "smart", 
    );

  my $xml_space_preserve; # set when an element includes xml:space="preserve"

  my $output_filter;      # filters the entire output (including < and >)
  my $output_text_filter; # filters only the text part (tag names, attributes, pcdata)

  my $replaced_ents= $REPLACED_ENTS;


  # returns those pesky "global" variables so you can switch between twigs 
  sub global_state ## no critic (Subroutines::ProhibitNestedSubs);
    { return
       { pretty                    => $pretty,
         quote                     => $quote,
         indent                    => $INDENT,
         empty_tag_style           => $empty_tag_style,
         remove_cdata              => $remove_cdata,
         keep_encoding             => $keep_encoding,
         expand_external_entities  => $expand_external_entities,
         output_filter             => $output_filter,
         output_text_filter        => $output_text_filter,
         keep_atts_order           => $keep_atts_order,
         do_not_escape_amp_in_atts => $do_not_escape_amp_in_atts,
         wrap                      => $WRAP,
         replaced_ents             => $replaced_ents,
        };
    }

  # restores the global variables
  sub set_global_state
    { my $state= shift;
      $pretty                    = $state->{pretty};
      $quote                     = $state->{quote};
      $INDENT                    = $state->{indent};
      $empty_tag_style           = $state->{empty_tag_style};
      $remove_cdata              = $state->{remove_cdata};
      $keep_encoding             = $state->{keep_encoding};
      $expand_external_entities  = $state->{expand_external_entities};
      $output_filter             = $state->{output_filter};
      $output_text_filter        = $state->{output_text_filter};
      $keep_atts_order           = $state->{keep_atts_order};
      $do_not_escape_amp_in_atts = $state->{do_not_escape_amp_in_atts};
      $WRAP                      = $state->{wrap};
      $replaced_ents             = $state->{replaced_ents},
    }

  # sets global state to defaults
  sub init_global_state
    { set_global_state(
       { pretty                    => 0,
         quote                     => '"',
         indent                    => $INDENT,
         empty_tag_style           => 0,
         remove_cdata              => 0,
         keep_encoding             => 0,
         expand_external_entities  => 0,
         output_filter             => undef,
         output_text_filter        => undef,
         keep_atts_order           => undef,
         do_not_escape_amp_in_atts => 0,
         wrap                      => $WRAP,
         replaced_ents             => $REPLACED_ENTS,
        });
    }


  # set the pretty_print style (in $pretty) and returns the old one
  # can be called from outside the package with 2 arguments (elt, style)
  # or from inside with only one argument (style)
  # the style can be either a string (one of the keys of %pretty_print_style
  # or a number (presumably an old value saved)
  sub set_pretty_print
    { my $style= lc( defined $_[1] ? $_[1] : $_[0]); # so we cover both cases 
      my $old_pretty= $pretty;
      if( $style=~ /^\d+$/)
        { croak "invalid pretty print style $style" unless( $style < keys %pretty_print_style);
          $pretty= $style;
        }
      else
        { croak "invalid pretty print style '$style'" unless( exists $pretty_print_style{$style});
          $pretty= $pretty_print_style{$style};
        }
      if( $WRAPPED{$pretty} )
        { XML::Twig::_use( 'Text::Wrap') or croak( "Text::Wrap not available, cannot use style $style"); }
      return $old_pretty;
    }
 
  sub _pretty_print { return $pretty; } 
  
  # set the empty tag style (in $empty_tag_style) and returns the old one
  # can be called from outside the package with 2 arguments (elt, style)
  # or from inside with only one argument (style)
  # the style can be either a string (one of the keys of %empty_tag_style
  # or a number (presumably an old value saved)
  sub set_empty_tag_style
    { my $style= lc( defined $_[1] ? $_[1] : $_[0]); # so we cover both cases 
      my $old_style= $empty_tag_style;
      if( $style=~ /^\d+$/)
        { croak "invalid empty tag style $style"
        unless( $style < keys %empty_tag_style);
        $empty_tag_style= $style;
        }
      else
        { croak "invalid empty tag style '$style'"
            unless( exists $empty_tag_style{$style});
          $empty_tag_style= $empty_tag_style{$style};
        }
      return $old_style;
    }

  sub _pretty_print_styles
    { return (sort { $pretty_print_style{$a} <=> $pretty_print_style{$b} || $a cmp $b } keys %pretty_print_style); }
      
  sub set_quote
    { my $style= $_[1] || $_[0];
      my $old_quote= $quote;
      croak "invalid quote '$style'" unless( exists $quote_style{$style});
      $quote= $quote_style{$style};
      return $old_quote;
    }
    
  sub set_remove_cdata
    { my $new_value= defined $_[1] ? $_[1] : $_[0];
      my $old_value= $remove_cdata;
      $remove_cdata= $new_value;
      return $old_value;
    }
      
      
  sub set_indent
    { my $new_value= defined $_[1] ? $_[1] : $_[0];
      my $old_value= $INDENT;
      $INDENT= $new_value;
      return $old_value;
    }

  sub set_wrap
    { my $new_value= defined $_[1] ? $_[1] : $_[0];
      my $old_value= $WRAP;
      $WRAP= $new_value;
      return $old_value;
    }
       
       
  sub set_keep_encoding
    { my $new_value= defined $_[1] ? $_[1] : $_[0];
      my $old_value= $keep_encoding;
      $keep_encoding= $new_value;
      return $old_value;
   }

  sub set_replaced_ents
    { my $new_value= defined $_[1] ? $_[1] : $_[0];
      my $old_value= $replaced_ents;
      $replaced_ents= $new_value;
      return $old_value;
   }

  sub do_not_escape_gt
    { my $old_value= $replaced_ents;
      $replaced_ents= q{&<}; # & needs to be first
      return $old_value;
    }

  sub escape_gt
    { my $old_value= $replaced_ents;
      $replaced_ents= qq{&<>}; # & needs to be first
      return $old_value;
    }

  sub _keep_encoding { return $keep_encoding; } # so I can use elsewhere in the module

  sub set_do_not_escape_amp_in_atts
    { my $new_value= defined $_[1] ? $_[1] : $_[0];
      my $old_value= $do_not_escape_amp_in_atts;
      $do_not_escape_amp_in_atts= $new_value;
      return $old_value;
   }

  sub output_filter      { return $output_filter; }
  sub output_text_filter { return $output_text_filter; }

  sub set_output_filter
    { my $new_value= defined $_[1] ? $_[1] : $_[0]; # can be called in object/non-object mode
      # if called in object mode with no argument, the filter is undefined
      if( isa( $new_value, 'XML::Twig::Elt') || isa( $new_value, 'XML::Twig')) { undef $new_value; }
      my $old_value= $output_filter;
      if( !$new_value || isa( $new_value, 'CODE') )
        { $output_filter= $new_value; }
      elsif( $new_value eq 'latin1')
        { $output_filter= XML::Twig::latin1();
        }
      elsif( $XML::Twig::filter{$new_value})
        {  $output_filter= $XML::Twig::filter{$new_value}; }
      else
        { croak "invalid output filter '$new_value'"; }
      
      return $old_value;
    }
       
  sub set_output_text_filter
    { my $new_value= defined $_[1] ? $_[1] : $_[0]; # can be called in object/non-object mode
      # if called in object mode with no argument, the filter is undefined
      if( isa( $new_value, 'XML::Twig::Elt') || isa( $new_value, 'XML::Twig')) { undef $new_value; }
      my $old_value= $output_text_filter;
      if( !$new_value || isa( $new_value, 'CODE') )
        { $output_text_filter= $new_value; }
      elsif( $new_value eq 'latin1')
        { $output_text_filter= XML::Twig::latin1();
        }
      elsif( $XML::Twig::filter{$new_value})
        {  $output_text_filter= $XML::Twig::filter{$new_value}; }
      else
        { croak "invalid output text filter '$new_value'"; }
      
      return $old_value;
    }
       
  sub set_expand_external_entities
    { my $new_value= defined $_[1] ? $_[1] : $_[0];
      my $old_value= $expand_external_entities;
      $expand_external_entities= $new_value;
      return $old_value;
    }
       
  sub set_keep_atts_order
    { my $new_value= defined $_[1] ? $_[1] : $_[0];
      my $old_value= $keep_atts_order;
      $keep_atts_order= $new_value;
      return $old_value;
    
   }

  sub keep_atts_order { return $keep_atts_order; } # so I can use elsewhere in the module

  my %html_empty_elt;
  BEGIN { %html_empty_elt= map { $_ => 1} qw( base meta link hr br param img area input col); }

  sub start_tag
    { my( $elt, $option)= @_;
 
 
      return if( $elt->{gi} < $XML::Twig::SPECIAL_GI);

      my $extra_data= $elt->{extra_data} || '';

      my $gi= $XML::Twig::index2gi[$elt->{'gi'}];
      my $att= $elt->{att}; # should be $elt->{att}, optimized into a pure hash look-up

      my $ns_map= $att ? $att->{'#original_gi'} : '';
      if( $ns_map) { $gi= _restore_original_prefix( $ns_map, $gi); }
      $gi=~ s{^#default:}{}; # remove default prefix
 
      if( $output_text_filter) { $gi= $output_text_filter->( $gi); }
  
      # get the attribute and their values
      my $att_sep = $pretty==$NSGMLS    ? "\n"
                  : $pretty==$INDENTEDA ? "\n" . $INDENT x ($elt->level+1) . '  '
                  :                       ' '
                  ;

      my $replace_in_att_value= $replaced_ents . "$quote\t\r\n";
      if( $option->{escape_gt} && $replaced_ents !~ m{>}) { $replace_in_att_value.= '>'; }

      my $tag;
      my @att_names= grep { !( $_=~ m{^#(?!default:)} ) } $keep_atts_order ?  keys %{$att} : sort keys %{$att};
      if( @att_names)
        { my $atts= join $att_sep, map  { my $output_att_name= $ns_map ? _restore_original_prefix( $ns_map, $_) : $_;
                                          if( $output_text_filter)
                                            { $output_att_name=  $output_text_filter->( $output_att_name); }
                                          $output_att_name . '=' . $quote . _att_xml_string( $att->{$_}, $replace_in_att_value) . $quote

                                        } 
                                        @att_names
                                   ;
           if( $pretty==$INDENTEDA && @att_names == 1) { $att_sep= ' '; }
           $tag= "<$gi$att_sep$atts";
        }
      else
        { $tag= "<$gi"; }
  
      $tag .= "\n" if($pretty==$NSGMLS);


      # force empty if suitable HTML tag, otherwise use the value from the input tree
      if( ($empty_tag_style eq $HTML) && !$elt->{first_child} && !$elt->{extra_data_before_end_tag} && $html_empty_elt{$gi})
        { $elt->{empty}= 1; }
      my $empty= defined $elt->{empty} ? $elt->{empty} 
               : $elt->{first_child}    ? 0
               :                         1;

      $tag .= (!$elt->{empty} || $elt->{extra_data_before_end_tag})  ? '>'            # element has content
            : (($empty_tag_style eq $HTML) && $html_empty_elt{$gi}) ? ' />'          # html empty element 
                                                                                     # cvs-friendly format
            : ( $pretty == $INDENTEDA && @att_names > 1)            ? "\n" .  $INDENT x $elt->level . "/>"  
            : ( $pretty == $INDENTEDA && @att_names == 1)           ? " />"  
            : $empty_tag_style                                      ? "></" . $XML::Twig::index2gi[$elt->{'gi'}] . ">" # $empty_tag_style is $HTML or $EXPAND
            :                                                         '/>'
            ;

      if( ( (substr( $XML::Twig::index2gi[$elt->{'gi'}], 0, 1) eq '#') && (substr( $XML::Twig::index2gi[$elt->{'gi'}], 0, 9) ne '#default:') )) { $tag= ''; }

#warn "TRACE: ", $tag,": ", Encode::is_utf8( $tag) ? "has flag" : "FLAG NOT SET";

      unless( $pretty) { return defined( $extra_data) ? $extra_data . $tag : $tag;  }

      my $prefix='';
      my $return='';   # '' or \n is to be printed before the tag
      my $indent=0;    # number of indents before the tag

      if( $pretty==$RECORD1)
        { my $level= $elt->level;
          $return= "\n" if( $level < 2);
          $indent= 1 if( $level == 1);
        }

     elsif( $pretty==$RECORD2)
        { $return= "\n";
          $indent= $elt->level;
        }

      elsif( $pretty==$NICE)
        { my $parent= $elt->{parent};
          unless( !$parent || $parent->{contains_text}) 
            { $return= "\n"; }
          $elt->{contains_text}= 1 if( ($parent && $parent->{contains_text})
                                     || $elt->contains_text);
        }

      elsif( $KEEP_TEXT_TAG_ON_ONE_LINE{$pretty})
        { my $parent= $elt->{parent};
          unless( !$parent || $parent->{contains_text}) 
            { $return= "\n"; 
              $indent= $elt->level; 
            }
          $elt->{contains_text}= 1 if( ($parent && $parent->{contains_text})
                                     || $elt->contains_text);
        }

      if( $return || $indent)
        { # check for elements in which spaces should be kept
          my $t= $elt->twig;
          return $extra_data . $tag if( $xml_space_preserve);
          if( $t && $t->{twig_keep_spaces_in})
            { foreach my $ancestor ($elt->ancestors)
                { return $extra_data . $tag if( $t->{twig_keep_spaces_in}->{$XML::Twig::index2gi[$ancestor->{'gi'}]}) }
            }
        
          $prefix= $INDENT x $indent;
          if( $extra_data)
            { $extra_data=~ s{\s+$}{};
              $extra_data=~ s{^\s+}{};
              $extra_data= $prefix .  $extra_data . $return;
            }
        }


      return $return . $extra_data . $prefix . $tag;
    }
  
  sub end_tag
    { my $elt= shift;
      return  '' if(    ($elt->{gi}<$XML::Twig::SPECIAL_GI) 
                     || ($elt->{'empty'} && !$elt->{extra_data_before_end_tag})
                   );
      my $tag= "<";
      my $gi= $XML::Twig::index2gi[$elt->{'gi'}];

      if( my $map= $elt->{'att'}->{'#original_gi'}) { $gi= _restore_original_prefix( $map, $gi); }
      $gi=~ s{^#default:}{}; # remove default prefix

      if( $output_text_filter) { $gi= $output_text_filter->( $XML::Twig::index2gi[$elt->{'gi'}]); } 
      $tag .=  "/$gi>";

      $tag = ($elt->{extra_data_before_end_tag} || '') . $tag;

      if( ( (substr( $XML::Twig::index2gi[$elt->{'gi'}], 0, 1) eq '#') && (substr( $XML::Twig::index2gi[$elt->{'gi'}], 0, 9) ne '#default:') )) { $tag= ''; }

      return $tag unless $pretty;

      my $prefix='';
      my $return=0;    # 1 if a \n is to be printed before the tag
      my $indent=0;    # number of indents before the tag

      if( $pretty==$RECORD1)
        { $return= 1 if( $elt->level == 0);
        }

     elsif( $pretty==$RECORD2)
        { unless( $elt->contains_text)
            { $return= 1 ;
              $indent= $elt->level;
            }
        }

      elsif( $pretty==$NICE)
        { my $parent= $elt->{parent};
          if( (    ($parent && !$parent->{contains_text}) || !$parent )
            && ( !$elt->{contains_text}  
             && ($elt->{has_flushed_child} || $elt->{first_child})           
           )
         )
            { $return= 1; }
        }

      elsif( $KEEP_TEXT_TAG_ON_ONE_LINE{$pretty})
        { my $parent= $elt->{parent};
          if( (    ($parent && !$parent->{contains_text}) || !$parent )
            && ( !$elt->{contains_text}  
             && ($elt->{has_flushed_child} || $elt->{first_child})           
           )
         )
            { $return= 1; 
              $indent= $elt->level; 
            }
        }

      if( $return || $indent)
        { # check for elements in which spaces should be kept
          my $t= $elt->twig;
          return $tag if( $xml_space_preserve);
          if( $t && $t->{twig_keep_spaces_in})
            { foreach my $ancestor ($elt, $elt->ancestors)
                { return $tag if( $t->{twig_keep_spaces_in}->{$XML::Twig::index2gi[$ancestor->{'gi'}]}) }
            }
      
          if( $return) { $prefix= ($pretty== $INDENTEDCT) ? "\n$INDENT" : "\n"; }
          $prefix.= $INDENT x $indent;
    }

      # add a \n at the end of the document (after the root element)
      $tag .= "\n" unless( $elt->{parent});
  
      return $prefix . $tag;
    }

  sub _restore_original_prefix
    { my( $map, $name)= @_;
      my $prefix= _ns_prefix( $name);
      if( my $original_prefix= $map->{$prefix})
        { if( $original_prefix eq '#default')
            { $name=~ s{^$prefix:}{}; }
          else
            { $name=~ s{^$prefix(?=:)}{$original_prefix}; }
        }
      return $name;
    }

  # buffer used to hold the text to print/sprint, to avoid passing it back and forth between methods
  my @sprint;

  # $elt is an element to print
  # $fh is an optional filehandle to print to
  # $pretty is an optional value, if true a \n is printed after the < of the
  # opening tag
  sub print
    { my $elt= shift;

      my $fh= isa( $_[0], 'GLOB') || isa( $_[0], 'IO::Scalar') ? shift : undef;
      my $old_select= defined $fh ? select $fh : undef;
      print $elt->sprint( @_);
      select $old_select if( defined $old_select);
    }


# those next 2 methods need to be refactored, they are copies of the same methods in XML::Twig 
sub print_to_file
  { my( $elt, $filename)= (shift, shift);
    my $out_fh;
#    open( $out_fh, ">$filename") or _croak( "cannot create file $filename: $!");     # < perl 5.8
    my $mode= $keep_encoding ? '>' : '>:utf8';                                       # >= perl 5.8
    open( $out_fh, $mode, $filename) or _croak( "cannot create file $filename: $!"); # >= perl 5.8
    $elt->print( $out_fh, @_);
    close $out_fh;
    return $elt;
  }

# probably only works on *nix (at least the chmod bit)
# first print to a temporary file, then rename that file to the desired file name, then change permissions
# to the original file permissions (or to the current umask)
sub safe_print_to_file
  { my( $elt, $filename)= (shift, shift);
    my $perm= -f $filename ? (stat $filename)[2] & 07777 : ~umask() ;
    XML::Twig::_use( 'File::Temp') || croak "need File::Temp to use safe_print_to_file\n";
    XML::Twig::_use( 'File::Basename') || croak "need File::Basename to use safe_print_to_file\n";
    my $tmpdir= File::Basename::dirname( $filename);    
    my( $fh, $tmpfilename) = File::Temp::tempfile( DIR => $tmpdir);
    $elt->print_to_file( $tmpfilename, @_);
    rename( $tmpfilename, $filename) or unlink $tmpfilename && _croak( "cannot move temporary file to $filename: $!"); 
    chmod $perm, $filename;
    return $elt;
  }

  
  # same as print but does not output the start tag if the element
  # is marked as flushed
  sub flush 
    { my $elt= shift; 
      my $up_to= $_[0] && isa( $_[0], 'XML::Twig::Elt') ? shift : $elt;
      $elt->twig->flush_up_to( $up_to, @_); 
    }
  sub purge
    { my $elt= shift; 
      my $up_to= $_[0] && isa( $_[0], 'XML::Twig::Elt') ? shift : $elt;
      $elt->twig->purge_up_to( $up_to, @_); 
    }
  
  sub _flush
    { my $elt= shift;
  
      my $pretty;
      my $fh=  isa( $_[0], 'GLOB') || isa( $_[0], 'IO::Scalar') ? shift : undef;
      my $old_select= defined $fh ? select $fh : undef;
      my $old_pretty= defined ($pretty= shift) ? set_pretty_print( $pretty) : undef;

      $xml_space_preserve= 1 if( ($elt->inherit_att( 'xml:space') || '') eq 'preserve');

      $elt->__flush();

      $xml_space_preserve= 0;

      select $old_select if( defined $old_select);
      set_pretty_print( $old_pretty) if( defined $old_pretty);
    }

  sub __flush
    { my $elt= shift;
  
      if( $elt->{gi} >= $XML::Twig::SPECIAL_GI)
        { my $preserve= ($elt->{'att'}->{'xml:space'} || '') eq 'preserve';
          $xml_space_preserve++ if $preserve;
          unless( $elt->{'flushed'})
            { print $elt->start_tag();
            }
      
          # flush the children
          my @children= do { my $elt= $elt; my @children=(); my $child= $elt->{first_child}; while( $child) { push @children, $child; $child= $child->{next_sibling}; } @children; };
          foreach my $child (@children)
            { $child->_flush( $pretty); 
              $child->{'flushed'}=1;
            }
          if( ! $elt->{end_tag_flushed}) 
            { print $elt->end_tag; 
              $elt->{end_tag_flushed}=1;
              $elt->{'flushed'}=1;
            }
          $xml_space_preserve-- if $preserve;
          # used for pretty printing
          if( my $parent= $elt->{parent}) { $parent->{has_flushed_child}= 1; }
        }
      else # text or special element
        { my $text;
          if( (exists $elt->{'pcdata'}))     { $text= $elt->pcdata_xml_string; 
                                     if( my $parent= $elt->{parent}) 
                                       { $parent->{contains_text}= 1; }
                                   }
          elsif( (exists $elt->{'cdata'}))   { $text= $elt->cdata_string;        
                                     if( my $parent= $elt->{parent}) 
                                       { $parent->{contains_text}= 1; }
                                   }
          elsif( (exists $elt->{'target'}))      { $text= $elt->pi_string;          }
          elsif( (exists $elt->{'comment'})) { $text= $elt->comment_string;     }
          elsif( (exists $elt->{'ent'}))     { $text= $elt->ent_string;         }

          print $output_filter ? $output_filter->( $text) : $text;
        }
    }
  

  sub xml_text
    { my( $elt, @options)= @_;

      if( @options && grep { lc( $_) eq 'no_recurse' } @options) { return $elt->xml_text_only; }

      my $string='';

      if( ($elt->{gi} >= $XML::Twig::SPECIAL_GI) )
        { # sprint the children
          my $child= $elt->{first_child} || '';
          while( $child)
            { $string.= $child->xml_text;
            } continue { $child= $child->{next_sibling}; }
        }
      elsif( (exists $elt->{'pcdata'}))  { $string .= $output_filter ?  $output_filter->($elt->pcdata_xml_string) 
                                                           : $elt->pcdata_xml_string; 
                               }
      elsif( (exists $elt->{'cdata'}))   { $string .= $output_filter ?  $output_filter->($elt->cdata_string)  
                                                           : $elt->cdata_string;      
                               }
      elsif( (exists $elt->{'ent'}))     { $string .= $elt->ent_string; }

      return $string;
    }

  sub xml_text_only
    { return join '', map { $_->xml_text if( $_->is_text || (exists $_->{'ent'})) } $_[0]->_children; }

  # same as print but except... it does not print but rather returns the string
  # if the second parameter is set then only the content is returned, not the
  # start and end tags of the element (but the tags of the included elements are
  # returned)

  sub sprint
    { my $elt= shift;
      my( $old_pretty, $old_empty_tag_style);

      if( $_[0])
        { if( isa( $_[0], 'HASH'))
            { # "proper way, using a hashref for options
              my %args= XML::Twig::_normalize_args( %{shift()}); 
              if( defined $args{PrettyPrint}) { $old_pretty          = set_pretty_print( $args{PrettyPrint});  }
              if( defined $args{EmptyTags})   { $old_empty_tag_style = set_empty_tag_style( $args{EmptyTags}); }
            }
          else
            { # "old" way, just using the option name
              my @other_opt;
              foreach my $opt (@_) 
                { if( exists $pretty_print_style{$opt}) { $old_pretty = set_pretty_print( $opt);             }
                  elsif( exists $empty_tag_style{$opt}) { $old_empty_tag_style = set_empty_tag_style( $opt); }
                  else                                  { push @other_opt, $opt;                             }
                }
               @_= @other_opt;
            }
        }

      $xml_space_preserve= 1 if( ($elt->inherit_att( 'xml:space') || '') eq 'preserve');

      @sprint=();
      $elt->_sprint( @_);
      my $sprint= join( '', @sprint);
      if( $output_filter) { $sprint= $output_filter->( $sprint); }

      if( ( ($pretty== $WRAPPED) || ($pretty==$INDENTEDC)) && !$xml_space_preserve)
        { $sprint= _wrap_text( $sprint); }
      $xml_space_preserve= 0;


      if( defined $old_pretty)          { set_pretty_print( $old_pretty);             } 
      if( defined $old_empty_tag_style) { set_empty_tag_style( $old_empty_tag_style); }

      return $sprint;
    }
  
  sub _wrap_text
    { my( $string)= @_;
      my $wrapped;
      foreach my $line (split /\n/, $string)
        { my( $initial_indent)= $line=~ m{^(\s*)};
          my $wrapped_line= Text::Wrap::wrap(  '',  $initial_indent . $INDENT, $line) . "\n";
          
          # fix glitch with Text::wrap when the first line is long and does not include spaces
          # the first line ends up being too short by 2 chars, but we'll have to live with it!
          $wrapped_line=~ s{^ +\n  }{}s; # this prefix needs to be removed
      
          $wrapped .= $wrapped_line;
        }
     
      return $wrapped;
    }
      
  
  sub _sprint
    { my $elt= shift;
      my $no_tag= shift || 0;
      # in case there's some comments or PI's piggybacking

      if( $elt->{gi} >= $XML::Twig::SPECIAL_GI)
        {
          my $preserve= ($elt->{'att'}->{'xml:space'} || '') eq 'preserve';
          $xml_space_preserve++ if $preserve;

          push @sprint, $elt->start_tag unless( $no_tag);
      
          # sprint the children
          my $child= $elt->{first_child};
          while( $child)
            { $child->_sprint;
              $child= $child->{next_sibling};
            }
          push @sprint, $elt->end_tag unless( $no_tag);
          $xml_space_preserve-- if $preserve;
        }
      else
        { push @sprint, $elt->{extra_data} if( $elt->{extra_data}) ;
          if(    (exists $elt->{'pcdata'}))  { push @sprint, $elt->pcdata_xml_string; }
          elsif( (exists $elt->{'cdata'}))   { push @sprint, $elt->cdata_string;      }
          elsif( (exists $elt->{'target'}))      { if( ($pretty >= $INDENTED) && !$elt->{parent}->{contains_text}) { push @sprint, "\n" . $INDENT x $elt->level; }
                                     push @sprint, $elt->pi_string;
                                   }
          elsif( (exists $elt->{'comment'})) { if( ($pretty >= $INDENTED) && !$elt->{parent}->{contains_text}) { push @sprint, "\n" . $INDENT x $elt->level; }
                                     push @sprint, $elt->comment_string;    
                                   }
          elsif( (exists $elt->{'ent'}))     { push @sprint, $elt->ent_string;        }
        }

      return;
    }

  # just a shortcut to $elt->sprint( 1)
  sub xml_string
    { my $elt= shift;
      isa( $_[0], 'HASH') ?  $elt->sprint( shift(), 1) : $elt->sprint( 1);
    }

  sub pcdata_xml_string 
    { my $elt= shift;
      if( defined( my $string= $elt->{pcdata}) )
        { 
          if( ! $elt->{extra_data_in_pcdata})
            { 
              $string=~ s/([$replaced_ents])/$XML::Twig::base_ent{$1}/g unless( !$replaced_ents || $keep_encoding || $elt->{asis});  
              $string=~ s{\Q]]>}{]]&gt;}g;
            }
          else
            { _gen_mark( $string); # used by _(un)?protect_extra_data
              foreach my $data (reverse @{$elt->{extra_data_in_pcdata}})
                { my $substr= substr( $string, $data->{offset});
                  if( $keep_encoding || $elt->{asis})
                    { substr( $string, $data->{offset}, 0, $data->{text}); }
                  else
                    { substr( $string, $data->{offset}, 0, _protect_extra_data( $data->{text})); }
                }
              unless( $keep_encoding || $elt->{asis})
                { 
                  $string=~ s{([$replaced_ents])}{$XML::Twig::base_ent{$1}}g ;
                  $string=~ s{\Q]]>}{]]&gt;}g;
                  _unprotect_extra_data( $string);
                }
            }
          return $output_text_filter ? $output_text_filter->( $string) : $string;
        }
      else
        { return ''; }
    }

  { my $mark;
    my( %char2ent, %ent2char);
    BEGIN
      { %char2ent= ( '<' => 'lt', '&' => 'amp', '>' => 'gt');
        %ent2char= map { $char2ent{$_} => $_ } keys %char2ent;
      }

    # generate a unique mark (a string) not found in the string, 
    # used to mark < and & in the extra data
    sub _gen_mark
      { $mark="AAAA";
        $mark++ while( index( $_[0], $mark) > -1);
        return $mark;
      }
      
    sub _protect_extra_data
      { my( $extra_data)= @_;
        $extra_data=~ s{([<&>])}{:$mark:$char2ent{$1}:}g;
        return $extra_data;
      }

    sub _unprotect_extra_data
      { $_[0]=~ s{:$mark:(\w+):}{$ent2char{$1}}g; }

  } 
  
  sub cdata_string
    { my $cdata= $_[0]->{cdata};
      unless( defined $cdata) { return ''; }
      if( $remove_cdata)
        { $cdata=~ s/([$replaced_ents])/$XML::Twig::base_ent{$1}/g; }
      else
        { $cdata= $CDATA_START . $cdata . $CDATA_END; }
      return $cdata;
   }

  sub att_xml_string 
    { my $elt= shift;
      my $att= shift;

      my $replace= $replaced_ents . "$quote\n\r\t";
      if($_[0] && $_[0]->{escape_gt} && ($replace!~ m{>}) ) { $replace .='>'; }

      if( defined (my $string= $elt->{att}->{$att}))
        { return _att_xml_string( $string, $replace); }
      else
        { return ''; }
    }
    
  # escaped xml string for an attribute value
  sub _att_xml_string 
    { my( $string, $escape)= @_;
      if( !defined( $string)) { return ''; }
      if( $keep_encoding)
        { $string=~ s{$quote}{$XML::Twig::base_ent{$quote}}g; 
        }
      else
        { 
          if( $do_not_escape_amp_in_atts)
            { $escape=~ s{^.}{}; # seems like the most backward compatible way to remove & from the list
              $string=~ s{([$escape])}{$XML::Twig::base_ent{$1}}g; 
              $string=~ s{&(?!(\w+|#\d+|[xX][0-9a-fA-F]+);)}{&amp;}g; # dodgy: escape & that do not start an entity
            }
          else
            { $string=~ s{([$escape])}{$XML::Twig::base_ent{$1}}g; 
              $string=~ s{\Q]]>}{]]&gt;}g;
            }
        }

      return $output_text_filter ? $output_text_filter->( $string) : $string;
    }

  sub ent_string 
    { my $ent= shift;
      my $ent_text= $ent->{ent};
      my( $t, $el, $ent_string);
      if(    $expand_external_entities
          && ($t= $ent->twig) 
          && ($el= $t->entity_list)
          && ($ent_string= $el->{entities}->{$ent->ent_name}->{val})
        )
        { return $ent_string; }
       else
         { return $ent_text;  }
    }

  # returns just the text, no tags, for an element
  sub text
    { my( $elt, @options)= @_;

      if( @options && grep { lc( $_) eq 'no_recurse' } @options) { return $elt->text_only; }
      my $sep = (@options && grep { lc( $_) eq 'sep' } @options) ? ' ' : '';
 
      my $string;
  
      if( (exists $elt->{'pcdata'}))     { return  $elt->{pcdata}    . $sep;  }
      elsif( (exists $elt->{'cdata'}))   { return  $elt->{cdata}     . $sep;  }
      elsif( (exists $elt->{'target'}))      { return  $elt->pi_string . $sep;  }
      elsif( (exists $elt->{'comment'})) { return  $elt->{comment}   . $sep;  }
      elsif( (exists $elt->{'ent'}))     { return  $elt->{ent}       . $sep ; }

  
      my $child= $elt->{first_child} ||'';
      while( $child)
        {
          my $child_text= $child->text( @options);
          $string.= defined( $child_text) ? $sep . $child_text : '';
        } continue { $child= $child->{next_sibling}; }

      unless( defined $string) { $string=''; }
 
      return $output_text_filter ? $output_text_filter->( $string) : $string;
    }

  sub text_only
    { return join '', map { $_->text if( $_->is_text || (exists $_->{'ent'})) } $_[0]->_children; }

  sub trimmed_text
    { my $elt= shift;
      my $text= $elt->text( @_);
      $text=~ s{\s+}{ }sg;
      $text=~ s{^\s*}{};
      $text=~ s{\s*$}{};
      return $text;
    }

  sub trim
    { my( $elt)= @_;
      my $pcdata= $elt->first_descendant( $TEXT);
      (my $pcdata_text= $pcdata->text)=~ s{^\s+}{}s;
      $pcdata->set_text( $pcdata_text);
      $pcdata= $elt->last_descendant( $TEXT);
      ($pcdata_text= $pcdata->text)=~ s{\s+$}{};
      $pcdata->set_text( $pcdata_text);
      foreach my $pcdata ($elt->descendants( $TEXT))
        { ($pcdata_text= $pcdata->text)=~ s{\s+}{ }g;
          $pcdata->set_text( $pcdata_text);
        }
      return $elt;
    }
  

  # remove cdata sections (turns them into regular pcdata) in an element 
  sub remove_cdata 
    { my $elt= shift;
      foreach my $cdata ($elt->descendants_or_self( $CDATA))
        { if( $keep_encoding)
            { my $data= $cdata->{cdata};
              $data=~ s{([&<"'])}{$XML::Twig::base_ent{$1}}g;
              $cdata->{pcdata}= (delete $cdata->{empty} || 1) &&  $data;
            }
          else
            { $cdata->{pcdata}= (delete $cdata->{empty} || 1) &&  $cdata->{cdata}; }
          $cdata->{gi}=$XML::Twig::gi2index{$PCDATA} or $cdata->set_gi( $PCDATA);
          undef $cdata->{cdata};
        }
    }

sub _is_private      { return _is_private_name( $_[0]->gi); }
sub _is_private_name { return $_[0]=~ m{^#(?!default:)};                }


} # end of block containing package globals ($pretty_print, $quotes, keep_encoding...)

# merges consecutive #PCDATAs in am element
sub normalize
  { my( $elt)= @_;
    my @descendants= $elt->descendants( $PCDATA);
    while( my $desc= shift @descendants)
      { if( ! length $desc->{pcdata}) { $desc->delete; next; }
        while( @descendants && $desc->{next_sibling} && $desc->{next_sibling}== $descendants[0])
          { my $to_merge= shift @descendants;
            $desc->merge_text( $to_merge);
          }
      }
    return $elt;
  }

# SAX export methods
sub toSAX1
  { _toSAX(@_, \&_start_tag_data_SAX1, \&_end_tag_data_SAX1); }

sub toSAX2
  { _toSAX(@_, \&_start_tag_data_SAX2, \&_end_tag_data_SAX2); }

sub _toSAX
  { my( $elt, $handler, $start_tag_data, $end_tag_data)= @_;
    if( $elt->{gi} >= $XML::Twig::SPECIAL_GI)
      { my $data= $start_tag_data->( $elt);
        _start_prefix_mapping( $elt, $handler, $data);
        if( $data && (my $start_element = $handler->can( 'start_element')))
          { unless( $elt->{'flushed'}) { $start_element->( $handler, $data); } }
      
        foreach my $child ($elt->_children)
          { $child->_toSAX( $handler, $start_tag_data, $end_tag_data); }

        if( (my $data= $end_tag_data->( $elt)) && (my $end_element = $handler->can( 'end_element')) )
          { $end_element->( $handler, $data); }
        _end_prefix_mapping( $elt, $handler);
      }
    else # text or special element
      { if( (exists $elt->{'pcdata'}) && (my $characters= $handler->can( 'characters')))
          { $characters->( $handler, { Data => $elt->{pcdata} });  }
        elsif( (exists $elt->{'cdata'}))  
          { if( my $start_cdata= $handler->can( 'start_cdata'))
              { $start_cdata->( $handler); }
            if( my $characters= $handler->can( 'characters'))
              { $characters->( $handler, {Data => $elt->{cdata} });  }
            if( my $end_cdata= $handler->can( 'end_cdata'))
              { $end_cdata->( $handler); }
          }
        elsif( ((exists $elt->{'target'}))  && (my $pi= $handler->can( 'processing_instruction')))
          { $pi->( $handler, { Target =>$elt->{target}, Data => $elt->{data} });  }
        elsif( ((exists $elt->{'comment'}))  && (my $comment= $handler->can( 'comment')))
          { $comment->( $handler, { Data => $elt->{comment} });  }
        elsif( ((exists $elt->{'ent'})))
          { 
            if( my $se=   $handler->can( 'skipped_entity'))
              { $se->( $handler, { Name => $elt->ent_name });  }
            elsif( my $characters= $handler->can( 'characters'))
              { if( defined $elt->ent_string)
                  { $characters->( $handler, {Data => $elt->ent_string});  }
                else
                  { $characters->( $handler, {Data => $elt->ent_name});  }
              }
          }
      
      }
  }
  
sub _start_tag_data_SAX1
  { my( $elt)= @_;
    my $name= $XML::Twig::index2gi[$elt->{'gi'}];
    return if( ( (substr( $XML::Twig::index2gi[$elt->{'gi'}], 0, 1) eq '#') && (substr( $XML::Twig::index2gi[$elt->{'gi'}], 0, 9) ne '#default:') ));
    my $attributes={};
    my $atts= $elt->{att};
    while( my( $att, $value)= each %$atts)
      { $attributes->{$att}= $value unless( ( $att=~ m{^#(?!default:)} )); }
    my $data= { Name => $name, Attributes => $attributes};
    return $data;
  }

sub _end_tag_data_SAX1
  { my( $elt)= @_;
    return if( ( (substr( $XML::Twig::index2gi[$elt->{'gi'}], 0, 1) eq '#') && (substr( $XML::Twig::index2gi[$elt->{'gi'}], 0, 9) ne '#default:') ));
    return  { Name => $XML::Twig::index2gi[$elt->{'gi'}] };
  } 
  
sub _start_tag_data_SAX2
  { my( $elt)= @_;
    my $data={};
    
    my $name= $XML::Twig::index2gi[$elt->{'gi'}];
    return if( ( (substr( $XML::Twig::index2gi[$elt->{'gi'}], 0, 1) eq '#') && (substr( $XML::Twig::index2gi[$elt->{'gi'}], 0, 9) ne '#default:') ));
    $data->{Name}         = $name;
    $data->{Prefix}       = $elt->ns_prefix; 
    $data->{LocalName}    = $elt->local_name;
    $data->{NamespaceURI} = $elt->namespace;

    # save a copy of the data so we can re-use it for the end tag
    my %sax2_data= %$data;
    $elt->{twig_elt_SAX2_data}= \%sax2_data;
   
    # add the attributes
    $data->{Attributes}= $elt->_atts_to_SAX2;

    return $data;
  }

sub _atts_to_SAX2
  { my $elt= shift;
    my $SAX2_atts= {};
    foreach my $att (keys %{$elt->{att}})
      { 
        next if( ( $att=~ m{^#(?!default:)} ));
        my $SAX2_att={};
        $SAX2_att->{Name}         = $att;
        $SAX2_att->{Prefix}       = _ns_prefix( $att); 
        $SAX2_att->{LocalName}    = _local_name( $att);
        $SAX2_att->{NamespaceURI} = $elt->namespace( $SAX2_att->{Prefix});
        $SAX2_att->{Value}        = $elt->{'att'}->{$att};
        my $SAX2_att_name= "{$SAX2_att->{NamespaceURI}}$SAX2_att->{LocalName}";

        $SAX2_atts->{$SAX2_att_name}= $SAX2_att;
      }
    return $SAX2_atts;
  }

sub _start_prefix_mapping
  { my( $elt, $handler, $data)= @_;
    if( my $start_prefix_mapping= $handler->can( 'start_prefix_mapping')
        and my @new_prefix_mappings= grep { /^\{[^}]*\}xmlns/ || /^\{$XMLNS_URI\}/ } keys %{$data->{Attributes}}
      )
      { foreach my $prefix (@new_prefix_mappings)
          { my $prefix_string= $data->{Attributes}->{$prefix}->{LocalName};
            if( $prefix_string eq 'xmlns') { $prefix_string=''; }
            my $prefix_data=
              {  Prefix       => $prefix_string,
                 NamespaceURI => $data->{Attributes}->{$prefix}->{Value}
              };
            $start_prefix_mapping->( $handler, $prefix_data);
            $elt->{twig_end_prefix_mapping} ||= [];
            push @{$elt->{twig_end_prefix_mapping}}, $prefix_string;
          }
      }
  }

sub _end_prefix_mapping
  { my( $elt, $handler)= @_;
    if( my $end_prefix_mapping= $handler->can( 'end_prefix_mapping'))
      { foreach my $prefix (@{$elt->{twig_end_prefix_mapping}})
          { $end_prefix_mapping->( $handler, { Prefix => $prefix} ); }
      }
  }
             
sub _end_tag_data_SAX2
  { my( $elt)= @_;
    return if( ( (substr( $XML::Twig::index2gi[$elt->{'gi'}], 0, 1) eq '#') && (substr( $XML::Twig::index2gi[$elt->{'gi'}], 0, 9) ne '#default:') ));
    return $elt->{twig_elt_SAX2_data};
  } 

sub contains_text
  { my $elt= shift;
    my $child= $elt->{first_child};
    while ($child)
      { return 1 if( $child->is_text || (exists $child->{'ent'})); 
        $child= $child->{next_sibling};
      }
    return 0;
  }

# creates a single pcdata element containing the text as child of the element
# options: 
#   - force_pcdata: when set to a true value forces the text to be in a #PCDATA
#                   even if the original element was a #CDATA
sub set_text
  { my( $elt, $string, %option)= @_;

    if( $XML::Twig::index2gi[$elt->{'gi'}] eq $PCDATA) 
      { return $elt->{pcdata}= (delete $elt->{empty} || 1) &&  $string; }
    elsif( $XML::Twig::index2gi[$elt->{'gi'}] eq $CDATA)  
      { if( $option{force_pcdata})
          { $elt->{gi}=$XML::Twig::gi2index{$PCDATA} or $elt->set_gi( $PCDATA);
            $elt->{cdata}= '';
            return $elt->{pcdata}= (delete $elt->{empty} || 1) &&  $string;
          }
        else
          { $elt->{cdata}=  $string; 
            return $string;
          }
      }
    elsif( $elt->contains_a_single( $PCDATA) )
      { # optimized so we have a slight chance of not losing embedded comments and pi's
        $elt->{first_child}->set_pcdata( $string);
        return $elt;
      }

    foreach my $child (@{[$elt->_children]})
      { $child->delete; }

    my $pcdata= $elt->_new_pcdata( $string);
    $pcdata->paste( $elt);

    delete $elt->{empty};

    return $elt;
  }

# set the content of an element from a list of strings and elements
sub set_content
  { my $elt= shift;

    return $elt unless defined $_[0];

    # attributes can be given as a hash (passed by ref)
    if( ref $_[0] eq 'HASH')
      { my $atts= shift;
        $elt->del_atts; # usually useless but better safe than sorry
        $elt->set_atts( $atts);
        return $elt unless defined $_[0];
      }

    # check next argument for #EMPTY
    if( !(ref $_[0]) && ($_[0] eq $EMPTY) ) 
      { $elt->{empty}= 1; return $elt; }

    # case where we really want to do a set_text, the element is '#PCDATA'
    # or contains a single PCDATA and we only want to add text in it
    if( ($XML::Twig::index2gi[$elt->{'gi'}] eq $PCDATA || $elt->contains_a_single( $PCDATA)) 
        && (@_ == 1) && !( ref $_[0]))
      { $elt->set_text( $_[0]);
        return $elt;
      }
    elsif( ($XML::Twig::index2gi[$elt->{'gi'}] eq $CDATA) && (@_ == 1) && !( ref $_[0]))
      { $elt->{cdata}=  $_[0];
        return $elt;
      }

    # delete the children
    foreach my $child (@{[$elt->_children]})
      { $child->delete; }

    if( @_) { delete $elt->{empty}; }

    foreach my $child (@_)
      { if( ref( $child) && isa( $child, 'XML::Twig::Elt'))
          { # argument is an element
            $child->paste( 'last_child', $elt);
          }
        else
          { # argument is a string
            if( (my $pcdata= $elt->{last_child}) && $elt->{last_child}->is_pcdata)
              { # previous child is also pcdata: just concatenate
                $pcdata->{pcdata}= (delete $pcdata->{empty} || 1) &&  $pcdata->{pcdata} . $child 
              }
            else
              { # previous child is not a string: create a new pcdata element
                $pcdata= $elt->_new_pcdata( $child);
                $pcdata->paste( 'last_child', $elt);  
              }
          }
      }


    return $elt;
  }

# inserts an element (whose gi is given) as child of the element
# all children of the element are now children of the new element
# returns the new element
sub insert
  { my ($elt, @args)= @_;
    # first cut the children
    my @children= do { my $elt= $elt; my @children=(); my $child= $elt->{first_child}; while( $child) { push @children, $child; $child= $child->{next_sibling}; } @children; };
    foreach my $child (@children)
      { $child->cut; }
    # insert elements
    while( my $gi= shift @args)
      { my $new_elt= $elt->new( $gi);
        # add attributes if needed
        if( defined( $args[0]) && ( isa( $args[0], 'HASH')) )
          { $new_elt->set_atts( shift @args); }
        # paste the element
        $new_elt->paste( $elt);
        delete $elt->{empty};
        $elt= $new_elt;
      }
    # paste back the children
    foreach my $child (@children)
      { $child->paste( 'last_child', $elt); }
    return $elt;
  }

# insert a new element 
# $elt->insert_new_element( $opt_position, $gi, $opt_atts_hash, @opt_content); 
# the element is created with the same syntax as new
# position is the same as in paste, first_child by default
sub insert_new_elt
  { my $elt= shift;
    my $position= $_[0];
    if(     ($position eq 'before') || ($position eq 'after')
         || ($position eq 'first_child') || ($position eq 'last_child'))
      { shift; }
    else
      { $position= 'first_child'; }

    my $new_elt= $elt->new( @_);
    $new_elt->paste( $position, $elt);

    #if( defined $new_elt->{'att'}->{$ID}) { $new_elt->set_id( $new_elt->{'att'}->{$ID}); }
    
    return $new_elt;
  }

# wraps an element in elements which gi's are given as arguments
# $elt->wrap_in( 'td', 'tr', 'table') wraps the element as a single
# cell in a table for example
# returns the new element
sub wrap_in
  { my $elt= shift;
    while( my $gi = shift @_)
      { my $new_elt = $elt->new( $gi);
        if( $elt->{twig_current})
          { my $t= $elt->twig;
            $t->{twig_current}= $new_elt;
            delete $elt->{'twig_current'};
            $new_elt->{'twig_current'}=1;
          }

        if( my $parent= $elt->{parent})
          { $new_elt->{parent}=$parent; if( $XML::Twig::weakrefs) { weaken( $new_elt->{parent});} ; 
            if( $parent->{first_child} == $elt) { $parent->{first_child}=  $new_elt; }
             if( $parent->{last_child} == $elt) {  delete $parent->{empty}; $parent->{last_child}=$new_elt; if( $XML::Twig::weakrefs) { weaken( $parent->{last_child});} ;  }
          }
        else
          { # wrapping the root
            my $twig= $elt->twig;
            if( $twig && $twig->root && ($twig->root eq $elt) )
              { $twig->set_root( $new_elt); 
              }
          }

        if( my $prev_sibling= $elt->{prev_sibling})
          { $new_elt->{prev_sibling}=$prev_sibling; if( $XML::Twig::weakrefs) { weaken( $new_elt->{prev_sibling});} ;
            $prev_sibling->{next_sibling}=  $new_elt;
          }

        if( my $next_sibling= $elt->{next_sibling})
          { $new_elt->{next_sibling}=  $next_sibling;
            $next_sibling->{prev_sibling}=$new_elt; if( $XML::Twig::weakrefs) { weaken( $next_sibling->{prev_sibling});} ;
          }
        $new_elt->{first_child}=  $elt;
         delete $new_elt->{empty}; $new_elt->{last_child}=$elt; if( $XML::Twig::weakrefs) { weaken( $new_elt->{last_child});} ;

        $elt->{parent}=$new_elt; if( $XML::Twig::weakrefs) { weaken( $elt->{parent});} ;
        $elt->{prev_sibling}=undef; if( $XML::Twig::weakrefs) { weaken( $elt->{prev_sibling});} ;
        $elt->{next_sibling}=  undef;

        # add the attributes if the next argument is a hash ref
        if( defined( $_[0]) && (isa( $_[0], 'HASH')) )
          { $new_elt->set_atts( shift @_); }

        $elt= $new_elt;
      }
      
    return $elt;
  }

sub replace
  { my( $elt, $ref)= @_;

    if( $elt->{parent}) { $elt->cut; }

    if( my $parent= $ref->{parent})
      { $elt->{parent}=$parent; if( $XML::Twig::weakrefs) { weaken( $elt->{parent});} ;
        if( $parent->{first_child} == $ref) { $parent->{first_child}=  $elt; }
        if( $parent->{last_child} == $ref)  {  delete $parent->{empty}; $parent->{last_child}=$elt; if( $XML::Twig::weakrefs) { weaken( $parent->{last_child});}  ; }
      }
    elsif( $ref->twig && $ref == $ref->twig->root)
      { $ref->twig->set_root( $elt); }

    if( my $prev_sibling= $ref->{prev_sibling})
      { $elt->{prev_sibling}=$prev_sibling; if( $XML::Twig::weakrefs) { weaken( $elt->{prev_sibling});} ;
        $prev_sibling->{next_sibling}=  $elt;
      }
    if( my $next_sibling= $ref->{next_sibling})
      { $elt->{next_sibling}=  $next_sibling;
        $next_sibling->{prev_sibling}=$elt; if( $XML::Twig::weakrefs) { weaken( $next_sibling->{prev_sibling});} ;
      }
   
    $ref->{parent}=undef; if( $XML::Twig::weakrefs) { weaken( $ref->{parent});} ;
    $ref->{prev_sibling}=undef; if( $XML::Twig::weakrefs) { weaken( $ref->{prev_sibling});} ;
    $ref->{next_sibling}=  undef;
    return $ref;
  }

sub replace_with
  { my $ref= shift;
    my $elt= shift;
    $elt->replace( $ref);
    foreach my $new_elt (reverse @_)
      { $new_elt->paste( after => $elt); }
    return $elt;
  }


# move an element, same syntax as paste, except the element is first cut
sub move
  { my $elt= shift;
    $elt->cut;
    $elt->paste( @_);
    return $elt;
  }


# adds a prefix to an element, creating a pcdata child if needed
sub prefix
  { my ($elt, $prefix, $option)= @_;
    my $asis= ($option && ($option eq 'asis')) ? 1 : 0;
    if( (exists $elt->{'pcdata'}) 
        && (($asis && $elt->{asis}) || (!$asis && ! $elt->{asis}))
      )
      { $elt->{pcdata}= (delete $elt->{empty} || 1) &&  $prefix . $elt->{pcdata}; }
    elsif( $elt->{first_child} && $elt->{first_child}->is_pcdata
        && (   ($asis && $elt->{first_child}->{asis}) 
            || (!$asis && ! $elt->{first_child}->{asis}))
         )
      { 
        $elt->{first_child}->set_pcdata( $prefix . $elt->{first_child}->pcdata); 
      }
    else
      { my $new_elt= $elt->_new_pcdata( $prefix);
        my $pos= (exists $elt->{'pcdata'}) ? 'before' : 'first_child';
        $new_elt->paste( $pos => $elt);
        if( $asis) { $new_elt->set_asis; }
      }
    return $elt;
  }

# adds a suffix to an element, creating a pcdata child if needed
sub suffix
  { my ($elt, $suffix, $option)= @_;
    my $asis= ($option && ($option eq 'asis')) ? 1 : 0;
    if( (exists $elt->{'pcdata'})
        && (($asis && $elt->{asis}) || (!$asis && ! $elt->{asis}))
      )
      { $elt->{pcdata}= (delete $elt->{empty} || 1) &&  $elt->{pcdata} . $suffix; }
    elsif( $elt->{last_child} && $elt->{last_child}->is_pcdata
        && (   ($asis && $elt->{last_child}->{asis}) 
            || (!$asis && ! $elt->{last_child}->{asis}))
         )
      { $elt->{last_child}->set_pcdata( $elt->{last_child}->pcdata . $suffix); }
    else
      { my $new_elt= $elt->_new_pcdata( $suffix);
        my $pos= (exists $elt->{'pcdata'}) ? 'after' : 'last_child';
        $new_elt->paste( $pos => $elt);
        if( $asis) { $new_elt->set_asis; }
      }
    return $elt;
  }

# create a path to an element ('/root/.../gi)
sub path
  { my $elt= shift;
    my @context= ( $elt, $elt->ancestors);
    return "/" . join( "/", reverse map {$_->gi} @context);
  }

sub xpath
  { my $elt= shift;
    my $xpath;
    foreach my $ancestor (reverse $elt->ancestors_or_self)
      { my $gi= $XML::Twig::index2gi[$ancestor->{'gi'}];
        $xpath.= "/$gi";
        my $index= $ancestor->prev_siblings( $gi) + 1;
        unless( ($index == 1) && !$ancestor->next_sibling( $gi))
          { $xpath.= "[$index]"; }
      }
    return $xpath;
  }

# methods used mainly by wrap_children

# return a string with the 
# for an element <foo><elt att="val">...</elt><elt2/><elt>...</elt></foo>
# returns '<elt att="val"><elt2><elt>'
sub _stringify_struct
  { my( $elt, %opt)= @_;
    my $string='';
    my $pretty_print= set_pretty_print( 'none');
    foreach my $child ($elt->_children)
      { $child->add_id; $string .= $child->start_tag( { escape_gt => 1 }) ||''; }
    set_pretty_print( $pretty_print);
    return $string;
  }

# wrap a series of elements in a new one
sub _wrap_range
  { my $elt= shift;
    my $gi= shift;
    my $atts= isa( $_[0], 'HASH') ? shift : undef;
    my $range= shift; # the string with the tags to wrap

    my $t= $elt->twig;

    # get the tags to wrap
    my @to_wrap;
    while( $range=~ m{<\w+\s+[^>]*id=("[^"]*"|'[^']*')[^>]*>}g)
      { push @to_wrap, $t->elt_id( substr( $1, 1, -1)); }

    return '' unless @to_wrap;
    
    my $to_wrap= shift @to_wrap;
    my %atts= %$atts;
    my $new_elt= $to_wrap->wrap_in( $gi, \%atts);
    $_->move( last_child => $new_elt) foreach (@to_wrap);

    return '';
  }
    
# wrap children matching a regexp in a new element
sub wrap_children
  { my( $elt, $regexp, $gi, $atts)= @_;

    $atts ||={};

    my $elt_as_string= $elt->_stringify_struct; # stringify the elt structure
    $regexp=~ s{(<[^>]*>)}{_match_expr( $1)}eg; # in the regexp, replace gi's by the proper regexp 
    $elt_as_string=~ s{($regexp)}{$elt->_wrap_range( $gi, $atts, $1)}eg; # then do the actual replace
  
    return $elt; 
  }

sub _match_expr
  { my $tag= shift;
    my( $gi, %atts)= XML::Twig::_parse_start_tag( $tag);
    return _match_tag( $gi, %atts);
  }


sub _match_tag
  { my( $elt, %atts)= @_;
    my $string= "<$elt\\b";
    foreach my $key (sort keys %atts)
      { my $val= qq{\Q$atts{$key}\E};
        $string.= qq{[^>]*$key=(?:"$val"|'$val')};
      }
    $string.=  qq{[^>]*>};
    return "(?:$string)";
  }

sub field_to_att
  { my( $elt, $cond, $att)= @_;
    $att ||= $cond;
    my $child= $elt->first_child( $cond) or return undef;
    $elt->set_att( $att => $child->text);
    $child->cut;
    return $elt;
  }

sub att_to_field
  { my( $elt, $att, $tag)= @_;
    $tag ||= $att;
    my $child= $elt->insert_new_elt( first_child => $tag, $elt->{'att'}->{$att});
    $elt->del_att( $att);
    return $elt;
  }

# sort children methods

sub sort_children_on_field
  { my $elt   = shift;
    my $field = shift;
    my $get_key= sub { return $_[0]->field( $field) };
    return $elt->sort_children( $get_key, @_); 
  }

sub sort_children_on_att
  { my $elt = shift;
    my $att = shift;
    my $get_key= sub { return $_[0]->{'att'}->{$att} };
    return $elt->sort_children( $get_key, @_); 
  }

sub sort_children_on_value
  { my $elt   = shift;
    #my $get_key= eval qq{ sub { $NO_WARNINGS; return \$_[0]->text } };
    my $get_key= \&text;
    return $elt->sort_children( $get_key, @_); 
  }

sub sort_children
  { my( $elt, $get_key, %opt)=@_;
    $opt{order} ||= 'normal';
    $opt{type}  ||= 'alpha';
    my( $par_a, $par_b)= ($opt{order} eq 'reverse') ? qw( b a) : qw ( a b) ;
    my $op= ($opt{type} eq 'numeric') ? '<=>' : 'cmp' ;
    my @children= $elt->cut_children;
    if( $opt{type} eq 'numeric')
      {  @children= map  { $_->[1] }
                    sort { $a->[0] <=> $b->[0] }
                    map  { [ $get_key->( $_), $_] } @children;
      }
    elsif( $opt{type} eq 'alpha')
      {  @children= map  { $_->[1] }
                    sort { $a->[0] cmp $b->[0] }
                    map  { [ $get_key->( $_), $_] } @children;
      }
    else
      { croak "wrong sort type '$opt{type}', should be either 'alpha' or 'numeric'"; }

    @children= reverse @children if( $opt{order} eq 'reverse');
    $elt->set_content( @children);
  }


# comparison methods

sub before
  { my( $a, $b)=@_;
    if( $a->cmp( $b) == -1) { return 1; } else { return 0; }
  }

sub after
  { my( $a, $b)=@_;
    if( $a->cmp( $b) == 1) { return 1; } else { return 0; }
  }

sub lt
  { my( $a, $b)=@_;
    return 1 if( $a->cmp( $b) == -1);
    return 0;
  }

sub le
  { my( $a, $b)=@_;
    return 1 unless( $a->cmp( $b) == 1);
    return 0;
  }

sub gt
  { my( $a, $b)=@_;
    return 1 if( $a->cmp( $b) == 1);
    return 0;
  }

sub ge
  { my( $a, $b)=@_;
    return 1 unless( $a->cmp( $b) == -1);
    return 0;
  }


sub cmp
  { my( $a, $b)=@_;

    # easy cases
    return  0 if( $a == $b);    
    return  1 if( $a->in($b)); # a in b => a starts after b 
    return -1 if( $b->in($a)); # b in a => a starts before b

    # ancestors does not include the element itself
    my @a_pile= ($a, $a->ancestors); 
    my @b_pile= ($b, $b->ancestors);

    # the 2 elements are not in the same twig
    return undef unless( $a_pile[-1] == $b_pile[-1]);

    # find the first non common ancestors (they are siblings)
    my $a_anc= pop @a_pile;
    my $b_anc= pop @b_pile;

    while( $a_anc == $b_anc) 
      { $a_anc= pop @a_pile;
        $b_anc= pop @b_pile;
      }

    # from there move left and right and figure out the order
    my( $a_prev, $a_next, $b_prev, $b_next)= ($a_anc, $a_anc, $b_anc, $b_anc);
    while()
      { $a_prev= $a_prev->{prev_sibling} || return( -1);
        return 1 if( $a_prev == $b_next);
        $a_next= $a_next->{next_sibling} || return( 1);
        return -1 if( $a_next == $b_prev);
        $b_prev= $b_prev->{prev_sibling} || return( 1);
        return -1 if( $b_prev == $a_next);
        $b_next= $b_next->{next_sibling} || return( -1);
        return 1 if( $b_next == $a_prev);
      }
  }
    
sub _dump
  { my( $elt, $option)= @_; 
  
    my $atts       = defined $option->{atts}       ? $option->{atts}       :  1;
    my $extra      = defined $option->{extra}      ? $option->{extra}      :  0;
    my $short_text = defined $option->{short_text} ? $option->{short_text} : 40;

    my $sp= '| ';
    my $indent= $sp x $elt->level;
    my $indent_sp= '  ' x $elt->level;
    
    my $dump='';
    if( $elt->is_elt)
      { 
        $dump .= $indent  . '|-' . $XML::Twig::index2gi[$elt->{'gi'}];
        
        if( $atts && (my @atts= $elt->att_names) )
          { $dump .= ' ' . join( ' ', map { qq{$_="} . $elt->{'att'}->{$_} . qq{"} } @atts); }

        $dump .= "\n";
        if( $extra) { $dump .= $elt->_dump_extra_data( $indent, $indent_sp, $short_text); }
        $dump .= join( "", map { $_->_dump( $option) } do { my $elt= $elt; my @children=(); my $child= $elt->{first_child}; while( $child) { push @children, $child; $child= $child->{next_sibling}; } @children; });
      }
    else
      { 
        if( (exists $elt->{'pcdata'}))
          { $dump .= "$indent|-PCDATA:  '"  . _short_text( $elt->{pcdata}, $short_text) . "'\n" }
        elsif( (exists $elt->{'ent'}))
          { $dump .= "$indent|-ENTITY:  '" . _short_text( $elt->{ent}, $short_text) . "'\n" }
        elsif( (exists $elt->{'cdata'}))
          { $dump .= "$indent|-CDATA:   '" . _short_text( $elt->{cdata}, $short_text) . "'\n" }
        elsif( (exists $elt->{'comment'}))
          { $dump .= "$indent|-COMMENT: '" . _short_text( $elt->comment_string, $short_text) . "'\n" }
        elsif( (exists $elt->{'target'}))
          { $dump .= "$indent|-PI:      '"      . $elt->{target} . "' - '" . _short_text( $elt->{data}, $short_text) . "'\n" }
        if( $extra) { $dump .= $elt->_dump_extra_data( $indent, $indent_sp, $short_text); }
      }
    return $dump;
  }

sub _dump_extra_data
  { my( $elt, $indent, $indent_sp, $short_text)= @_;
    my $dump='';
    if( $elt->extra_data)
      { my $extra_data = $indent . "|-- (cpi before) '" . _short_text( $elt->extra_data, $short_text) . "'";
        $extra_data=~ s{\n}{$indent_sp}g;
        $dump .= $extra_data . "\n";
      }
    if( $elt->{extra_data_in_pcdata})
      { foreach my $data ( @{$elt->{extra_data_in_pcdata}})
          { my $extra_data = $indent . "|-- (cpi offset $data->{offset}) '" . _short_text( $data->{text}, $short_text) . "'";
            $extra_data=~ s{\n}{$indent_sp}g;
            $dump .= $extra_data . "\n";
          }
      } 
    if( $elt->{extra_data_before_end_tag})
      { my $extra_data = $indent . "|-- (cpi end) '" . _short_text( $elt->{extra_data_before_end_tag}, $short_text) . "'";
        $extra_data=~ s{\n}{$indent_sp}g;
        $dump .= $extra_data . "\n";
      } 
    return $dump;
  }
 

sub _short_text
  { my( $string, $length)= @_;
    if( !$length || (length( $string) < $length) ) { return $string; }
    my $l1= (length( $string) -5) /2;
    my $l2= length( $string) - ($l1 + 5);
    return substr( $string, 0, $l1) . ' ... ' . substr( $string, -$l2);
  }


sub _and { return _join_defined( ' && ',  @_); }
sub _join_defined { return join( shift(), grep { $_ } @_); }

1;
__END__

=head1 NAME

XML::Twig - A perl module for processing huge XML documents in tree mode.

=head1 SYNOPSIS

Note that this documentation is intended as a reference to the module.

Complete docs, including a tutorial, examples, an easier to use HTML version,
a quick reference card and a FAQ are available at L<http://www.xmltwig.org/xmltwig>

Small documents (loaded in memory as a tree):

  my $twig=XML::Twig->new();    # create the twig
  $twig->parsefile( 'doc.xml'); # build it
  my_process( $twig);           # use twig methods to process it 
  $twig->print;                 # output the twig

Huge documents (processed in combined stream/tree mode):

  # at most one div will be loaded in memory
  my $twig=XML::Twig->new(   
    twig_handlers => 
      { title   => sub { $_->set_tag( 'h2') }, # change title tags to h2 
                                               # $_ is the current element
        para    => sub { $_->set_tag( 'p')  }, # change para to p
        hidden  => sub { $_->delete;       },  # remove hidden elements
        list    => \&my_list_process,          # process list elements
        div     => sub { $_[0]->flush;     },  # output and free memory
      },
    pretty_print => 'indented',                # output will be nicely formatted
    empty_tags   => 'html',                    # outputs <empty_tag />
                         );
  $twig->parsefile( 'my_big.xml');

  sub my_list_process
    { my( $twig, $list)= @_;
      # ...
    }

See L<XML::Twig 101|/XML::Twig 101> for other ways to use the module, as a 
filter for example.

=encoding utf8   

=head1 DESCRIPTION

This module provides a way to process XML documents. It is build on top
of C<XML::Parser>.

The module offers a tree interface to the document, while allowing you
to output the parts of it that have been completely processed.

It allows minimal resource (CPU and memory) usage by building the tree
only for the parts of the documents that need actual processing, through the 
use of the C<L<twig_roots> > and 
C<L<twig_print_outside_roots> > options. The 
C<L<finish> > and C<L<finish_print> > methods also help 
to increase performances.

XML::Twig tries to make simple things easy so it tries its best to takes care 
of a lot of the (usually) annoying (but sometimes necessary) features that 
come with XML and XML::Parser.

=head1 TOOLS

XML::Twig comes with a few command-line utilities:

=head2 xml_pp - xml pretty-printer

XML pretty printer using XML::Twig

=head2 xml_grep - grep XML files looking for specific elements

C<xml_grep> does a grep on XML files. Instead of using regular expressions 
it uses XPath expressions (in fact the subset of XPath supported by 
XML::Twig). 

=head2 xml_split - cut a big XML file into smaller chunks

C<xml_split> takes a (presumably big) XML file and split it in several smaller
files, based on various criteria (level in the tree, size or an XPath 
expression)

=head2 xml_merge - merge back XML files split with xml_split

C<xml_merge> takes several xml files that have been split using C<xml_split>
and recreates a single file.

=head2 xml_spellcheck - spellcheck XML files

C<xml_spellcheck> lets you spell check the content of an XML file. It extracts
the text (the content of elements and optionally of attributes), call a spell
checker on it and then recreates the XML document.


=head1 XML::Twig 101

XML::Twig can be used either on "small" XML documents (that fit in memory)
or on huge ones, by processing parts of the document and outputting or
discarding them once they are processed.


=head2 Loading an XML document and processing it

  my $t= XML::Twig->new();
  $t->parse( '<d><title>title</title><para>p 1</para><para>p 2</para></d>');
  my $root= $t->root;
  $root->set_tag( 'html');              # change doc to html
  $title= $root->first_child( 'title'); # get the title
  $title->set_tag( 'h1');               # turn it into h1
  my @para= $root->children( 'para');   # get the para children
  foreach my $para (@para)
    { $para->set_tag( 'p'); }           # turn them into p
  $t->print;                            # output the document

Other useful methods include:

L<att>: C<< $elt->{'att'}->{'foo'} >> return the C<foo> attribute for an 
element,

L<set_att> : C<< $elt->set_att( foo => "bar") >> sets the C<foo> 
attribute to the C<bar> value,

L<next_sibling>: C<< $elt->{next_sibling} >> return the next sibling
in the document (in the example C<< $title->{next_sibling} >> is the first
C<para>, you can also (and actually should) use 
C<< $elt->next_sibling( 'para') >> to get it 

The document can also be transformed through the use of the L<cut>, 
L<copy>, L<paste> and L<move> methods: 
C<< $title->cut; $title->paste( after => $p); >> for example

And much, much more, see L<XML::Twig::Elt|/XML::Twig::Elt>.

=head2 Processing an XML document chunk by chunk

One of the strengths of XML::Twig is that it let you work with files that do 
not fit in memory (BTW storing an XML document in memory as a tree is quite
memory-expensive, the expansion factor being often around 10).

To do this you can define handlers, that will be called once a specific 
element has been completely parsed. In these handlers you can access the
element and process it as you see fit, using the navigation and the
cut-n-paste methods, plus lots of convenient ones like C<L<prefix> >.
Once the element is completely processed you can then C<L<flush> > it, 
which will output it and free the memory. You can also C<L<purge> > it 
if you don't need to output it (if you are just extracting some data from 
the document for example). The handler will be called again once the next 
relevant element has been parsed.

  my $t= XML::Twig->new( twig_handlers => 
                          { section => \&section,
                            para   => sub { $_->set_tag( 'p'); }
                          },
                       );
  $t->parsefile( 'doc.xml');

  # the handler is called once a section is completely parsed, ie when 
  # the end tag for section is found, it receives the twig itself and
  # the element (including all its sub-elements) as arguments
  sub section 
    { my( $t, $section)= @_;      # arguments for all twig_handlers
      $section->set_tag( 'div');  # change the tag name
      # let's use the attribute nb as a prefix to the title
      my $title= $section->first_child( 'title'); # find the title
      my $nb= $title->{'att'}->{'nb'}; # get the attribute
      $title->prefix( "$nb - ");  # easy isn't it?
      $section->flush;            # outputs the section and frees memory
    }


There is of course more to it: you can trigger handlers on more elaborate 
conditions than just the name of the element, C<section/title> for example.

  my $t= XML::Twig->new( twig_handlers => 
                           { 'section/title' => sub { $_->print } }
                       )
                  ->parsefile( 'doc.xml');

Here C<< sub { $_->print } >> simply prints the current element (C<$_> is aliased
to the element in the handler).

You can also trigger a handler on a test on an attribute:

  my $t= XML::Twig->new( twig_handlers => 
                      { 'section[@level="1"]' => sub { $_->print } }
                       );
                  ->parsefile( 'doc.xml');

You can also use C<L<start_tag_handlers> > to process an 
element as soon as the start tag is found. Besides C<L<prefix> > you
can also use C<L<suffix> >, 

=head2 Processing just parts of an XML document

The twig_roots mode builds only the required sub-trees from the document
Anything outside of the twig roots will just be ignored:

  my $t= XML::Twig->new( 
       # the twig will include just the root and selected titles 
           twig_roots   => { 'section/title' => \&print_n_purge,
                             'annex/title'   => \&print_n_purge
           }
                      );
  $t->parsefile( 'doc.xml');

  sub print_n_purge 
    { my( $t, $elt)= @_;
      print $elt->text;    # print the text (including sub-element texts)
      $t->purge;           # frees the memory
    }

You can use that mode when you want to process parts of a documents but are
not interested in the rest and you don't want to pay the price, either in
time or memory, to build the tree for the it.


=head2 Building an XML filter

You can combine the C<twig_roots> and the C<twig_print_outside_roots> options to 
build filters, which let you modify selected elements and will output the rest 
of the document as is.

This would convert prices in $ to prices in Euro in a document:

  my $t= XML::Twig->new( 
           twig_roots   => { 'price' => \&convert, },   # process prices 
           twig_print_outside_roots => 1,               # print the rest
                      );
  $t->parsefile( 'doc.xml');

  sub convert 
    { my( $t, $price)= @_;
      my $currency=  $price->{'att'}->{'currency'};          # get the currency
      if( $currency eq 'USD')
        { $usd_price= $price->text;                     # get the price
          # %rate is just a conversion table 
          my $euro_price= $usd_price * $rate{usd2euro};
          $price->set_text( $euro_price);               # set the new price
          $price->set_att( currency => 'EUR');          # don't forget this!
        }
      $price->print;                                    # output the price
    }

=head2 XML::Twig and various versions of Perl, XML::Parser and expat:

XML::Twig is a lot more sensitive to variations in versions of perl, 
XML::Parser and expat than to the OS, so this should cover some
reasonable configurations.

The "recommended configuration" is perl 5.8.3+ (for good Unicode
support), XML::Parser 2.31+ and expat 1.95.5+

See L<http://testers.cpan.org/search?request=dist&dist=XML-Twig> for the
CPAN testers reports on XML::Twig, which list all tested configurations.

An Atom feed of the CPAN Testers results is available at
L<http://xmltwig.org/rss/twig_testers.rss>

Finally: 

=over 4

=item XML::Twig does B<NOT> work with expat 1.95.4

=item  XML::Twig only works with XML::Parser 2.27 in perl 5.6.*  

Note that I can't compile XML::Parser 2.27 anymore, so I can't guarantee 
that it still works

=item XML::Parser 2.28 does not really work

=back

When in doubt, upgrade expat, XML::Parser and Scalar::Util

Finally, for some optional features, XML::Twig depends on some additional
modules. The complete list, which depends somewhat on the version of Perl
that you are running, is given by running C<t/zz_dump_config.t>

=head1 Simplifying XML processing

=over 4

=item Whitespaces

Whitespaces that look non-significant are discarded, this behaviour can be 
controlled using the C<L<keep_spaces> >, 
C<L<keep_spaces_in> > and 
C<L<discard_spaces_in> > options.

=item Encoding

You can specify that you want the output in the same encoding as the input
(provided you have valid XML, which means you have to specify the encoding
either in the document or when you create the Twig object) using the 
C<L<keep_encoding> > option

You can also use C<L<output_encoding>> to convert the internal UTF-8 format
to the required encoding.

=item Comments and Processing Instructions (PI)

Comments and PI's can be hidden from the processing, but still appear in the
output (they are carried by the "real" element closer to them)

=item Pretty Printing

XML::Twig can output the document pretty printed so it is easier to read for
us humans.

=item Surviving an untimely death

XML parsers are supposed to react violently when fed improper XML. 
XML::Parser just dies.

XML::Twig provides the C<L<safe_parse> > and the 
C<L<safe_parsefile> > methods which wrap the parse in an eval
and return either the parsed twig or 0 in case of failure.

=item Private attributes

Attributes with a name starting with # (illegal in XML) will not be
output, so you can safely use them to store temporary values during
processing. Note that you can store anything in a private attribute, 
not just text, it's just a regular Perl variable, so a reference to
an object or a huge data structure is perfectly fine.

=back

=head1 CLASSES

XML::Twig uses a very limited number of classes. The ones you are most likely to use
are C<L<XML::Twig>> of course, which represents a complete XML document, including the 
document itself (the root of the document itself is C<L<root>>), its handlers, its
input or output filters... The other main class is C<L<XML::Twig::Elt>>, which models 
an XML element. Element here has a very wide definition: it can be a regular element, or
but also text, with an element C<L<tag>> of C<#PCDATA> (or C<#CDATA>), an entity (tag is
C<#ENT>), a Processing Instruction (C<#PI>), a comment (C<#COMMENT>). 

Those are the 2 commonly used classes.

You might want to look the C<L<elt_class>> option if you want to subclass C<XML::Twig::Elt>.

Attributes are just attached to their parent element, they are not objects per se. (Please
use the provided methods C<L<att>> and C<L<set_att>> to access them, if you access them
as a hash, then your code becomes implementation dependent and might break in the future).

Other classes that are seldom used are C<L<XML::Twig::Entity_list>> and C<L<XML::Twig::Entity>>.

If you use C<L<XML::Twig::XPath>> instead of C<XML::Twig>, elements are then created as
C<L<XML::Twig::XPath::Elt>>


=head1 METHODS

=head2 XML::Twig 

A twig is a subclass of XML::Parser, so all XML::Parser methods can be
called on a twig object, including parse and parsefile.
C<setHandlers> on the other hand cannot be used, see C<L<BUGS> >


=over 4

=item new 

This is a class method, the constructor for XML::Twig. Options are passed
as keyword value pairs. Recognized options are the same as XML::Parser,
plus some (in fact a lot!) XML::Twig specifics.

New Options:

=over 4

=item twig_handlers

This argument consists of a hash C<{ expression => \&handler}> where 
expression is a an I<XPath-like expression> (+ some others). 

XPath expressions are limited to using the child and descendant axis
(indeed you can't specify an axis), and predicates cannot be nested.
You can use the C<string>, or C<< string(<tag>) >> function (except 
in C<twig_roots> triggers).

Additionally you can use regexps (/ delimited) to match attribute
and string values.

Examples:

  foo
  foo/bar
  foo//bar
  /foo/bar
  /foo//bar
  /foo/bar[@att1 = "val1" and @att2 = "val2"]/baz[@a >= 1]
  foo[string()=~ /^duh!+/]
  /foo[string(bar)=~ /\d+/]/baz[@att != 3]

#CDATA can be used to call a handler for a CDATA section.
#COMMENT can be used to call a handler for comments

Some additional (non-XPath) expressions are also provided for convenience: 

=over 4

=item processing instructions

C<'?'> or C<'#PI'> triggers the handler for any processing instruction,
and C<< '?<target>' >> or C<< '#PI <target>' >> triggers a handler for processing
instruction with the given target( ex: C<'#PI xml-stylesheet'>).

=item level(<level>)

Triggers the handler on any element at that level in the tree (root is level 1)

=item _all_

Triggers the handler for B<all> elements in the tree

=item _default_

Triggers the handler for each element that does NOT have any other handler.

=back

Expressions are evaluated against the input document. 
Which means that even if you have changed the tag of an element (changing the
tag of a parent element from a handler for example) the change will not impact
the expression evaluation. There is an exception to this: "private" attributes
(which name start with a '#', and can only be created during the parsing, as
they are not valid XML) are checked against the current twig. 

Handlers are triggered in fixed order, sorted by their type (xpath expressions
first, then regexps, then level), then by whether they specify a full path 
(starting at the root element) or
not, then by number of steps in the expression, then number of
predicates, then number of tests in predicates. Handlers where the last
step does not specify a step (C<foo/bar/*>) are triggered after other XPath 
handlers. Finally C<_all_> handlers are triggered last. 

B<Important>: once a handler has been triggered if it returns 0 then no other
handler is called, except a C<_all_> handler which will be called anyway.

If a handler returns a true value and other handlers apply, then the next
applicable handler will be called. Repeat, rinse, lather..; The exception
to that rule is when the C<L<do_not_chain_handlers>>
option is set, in which case only the first handler will be called.

Note that it might be a good idea to explicitly return a short true value
(like 1) from handlers: this ensures that other applicable handlers are 
called even if the last statement for the handler happens to evaluate to
false. This might also speedup the code by avoiding the result of the last 
statement of the code to be copied and passed to the code managing handlers.
It can really pay to have 1 instead of a long string returned.

When the closing tag for an element is parsed the corresponding handler is
called, with 2 arguments: the twig and the C<L<Element> >. The twig includes 
the document tree that has been built so far, the element is the complete 
sub-tree for the element. B<The fact that the handler is called only when the 
closing tag for the element is found means that handlers for inner elements
are called before handlers for outer elements>.

C<$_> is also set to the element, so it is easy to write inline handlers like

  para => sub { $_->set_tag( 'p'); }

Text is stored in elements whose tag name is #PCDATA (due to mixed content, 
text and sub-element in an element there is no way to store the text as just 
an attribute of the enclosing element, this is similar to the DOM model).

B<Warning>: if you have used purge or flush on the twig the element might not
be complete, some of its children might have been entirely flushed or purged,
and the start tag might even have been printed (by C<flush>) already, so changing
its tag might not give the expected result.


=item twig_roots

This argument let's you build the tree only for those elements you are
interested in. 

  Example: my $t= XML::Twig->new( twig_roots => { title => 1, subtitle => 1});
           $t->parsefile( file);
           my $t= XML::Twig->new( twig_roots => { 'section/title' => 1});
           $t->parsefile( file);


return a twig containing a document including only C<title> and C<subtitle> 
elements, as children of the root element.

You can use I<generic_attribute_condition>, I<attribute_condition>,
I<full_path>, I<partial_path>, I<tag>, I<tag_regexp>, I<_default_> and 
I<_all_> to trigger the building of the twig. 
I<string_condition> and I<regexp_condition> cannot be used as the content 
of the element, and the string, have not yet been parsed when the condition
is checked.

B<WARNING>: path are checked for the document. Even if the C<twig_roots> option
is used they will be checked against the full document tree, not the virtual
tree created by XML::Twig


B<WARNING>: twig_roots elements should NOT be nested, that would hopelessly
confuse XML::Twig ;--(

Note: you can set handlers (twig_handlers) using twig_roots
  Example: my $t= XML::Twig->new( twig_roots => 
                                   { title    => sub { $_[1]->print;}, 
                                     subtitle => \&process_subtitle 
                                   }
                               );
           $t->parsefile( file);


=item twig_print_outside_roots

To be used in conjunction with the C<twig_roots> argument. When set to a true 
value this will print the document outside of the C<twig_roots> elements.

 Example: my $t= XML::Twig->new( twig_roots => { title => \&number_title },
                                twig_print_outside_roots => 1,
                               );
           $t->parsefile( file);
           { my $nb;
           sub number_title
             { my( $twig, $title);
               $nb++;
               $title->prefix( "$nb ");
               $title->print;
             }
           }


This example prints the document outside of the title element, calls 
C<number_title> for each C<title> element, prints it, and then resumes printing
the document. The twig is built only for the C<title> elements. 

If the value is a reference to a file handle then the document outside the
C<twig_roots> elements will be output to this file handle:

  open( my $out, '>', 'out_file.xml') or die "cannot open out file.xml out_file:$!";
  my $t= XML::Twig->new( twig_roots => { title => \&number_title },
                         # default output to $out
                         twig_print_outside_roots => $out, 
                       );

         { my $nb;
           sub number_title
             { my( $twig, $title);
               $nb++;
               $title->prefix( "$nb ");
               $title->print( $out);    # you have to print to \*OUT here
             }
           }


=item start_tag_handlers

A hash C<{ expression => \&handler}>. Sets element handlers that are called when
the element is open (at the end of the XML::Parser C<Start> handler). The handlers
are called with 2 params: the twig and the element. The element is empty at 
that point, its attributes are created though. 

You can use I<generic_attribute_condition>, I<attribute_condition>,
I<full_path>, I<partial_path>, I<tag>, I<tag_regexp>, I<_default_>  and I<_all_> 
to trigger the handler. 

I<string_condition> and I<regexp_condition> cannot be used as the content of 
the element, and the string, have not yet been parsed when the condition is 
checked.

The main uses for those handlers are to change the tag name (you might have to 
do it as soon as you find the open tag if you plan to C<flush> the twig at some
point in the element, and to create temporary attributes that will be used
when processing sub-element with C<twig_hanlders>. 

B<Note>: C<start_tag> handlers can be called outside of C<twig_roots> if this 
argument is used. Since the element object is not built, in this case handlers
are called with the following arguments: C<$t> (the twig), C<$tag> (the tag of 
the element) and C<%att> (a hash of the attributes of the element).  

If the C<twig_print_outside_roots> argument is also used, if the last handler
called returns  a C<true> value, then the start tag will be output as it
appeared in the original document, if the handler returns a C<false> value
then the start tag will B<not> be printed (so you can print a modified string 
yourself for example).

Note that you can use the L<ignore> method in C<start_tag_handlers> 
(and only there). 

=item end_tag_handlers

A hash C<{ expression => \&handler}>. Sets element handlers that are called when
the element is closed (at the end of the XML::Parser C<End> handler). The handlers
are called with 2 params: the twig and the tag of the element. 

I<twig_handlers> are called when an element is completely parsed, so why have 
this redundant option? There is only one use for C<end_tag_handlers>: when using
the C<twig_roots> option, to trigger a handler for an element B<outside> the roots.
It is for example very useful to number titles in a document using nested 
sections: 

  my @no= (0);
  my $no;
  my $t= XML::Twig->new( 
          start_tag_handlers => 
           { section => sub { $no[$#no]++; $no= join '.', @no; push @no, 0; } },
          twig_roots         => 
           { title   => sub { $_->prefix( $no); $_->print; } },
          end_tag_handlers   => { section => sub { pop @no;  } },
          twig_print_outside_roots => 1
                      );
   $t->parsefile( $file);

Using the C<end_tag_handlers> argument without C<twig_roots> will result in an
error.

=item do_not_chain_handlers

If this option is set to a true value, then only one handler will be called for
each element, even if several satisfy the condition

Note that the C<_all_> handler will still be called regardless

=item ignore_elts

This option lets you ignore elements when building the twig. This is useful 
in cases where you cannot use C<twig_roots> to ignore elements, for example if
the element to ignore is a sibling of elements you are interested in.

Example:

  my $twig= XML::Twig->new( ignore_elts => { elt => 'discard' });
  $twig->parsefile( 'doc.xml');

This will build the complete twig for the document, except that all C<elt> 
elements (and their children) will be left out.

The keys in the hash are triggers, limited to the same subset as 
C<L<start_tag_handlers>>. The values can be C<discard>, to discard
the element, C<print>, to output the element as-is, C<string> to 
store the text of the ignored element(s), including markup, in a field of
the twig: C<< $t->{twig_buffered_string} >> or a reference to a scalar, in
which case the text of the ignored element(s), including markup, will be
stored in the scalar. Any other value will be treated as C<discard>.


=item char_handler

A reference to a subroutine that will be called every time C<PCDATA> is found.

The subroutine receives the string as argument, and returns the modified string:

  # WE WANT ALL STRINGS IN UPPER CASE
  sub my_char_handler
    { my( $text)= @_;
      $text= uc( $text);
      return $text;
    }

=item elt_class

The name of a class used to store elements. this class should inherit from
C<XML::Twig::Elt> (and by default it is C<XML::Twig::Elt>). This option is used
to subclass the element class and extend it with new methods.

This option is needed because during the parsing of the XML, elements are created
by C<XML::Twig>, without any control from the user code.

=item keep_atts_order

Setting this option to a true value causes the attribute hash to be tied to
a C<Tie::IxHash> object.
This means that C<Tie::IxHash> needs to be installed for this option to be 
available. It also means that the hash keeps its order, so you will get 
the attributes in order. This allows outputting the attributes in the same 
order as they were in the original document.

=item keep_encoding

This is a (slightly?) evil option: if the XML document is not UTF-8 encoded and
you want to keep it that way, then setting keep_encoding will use theC<Expat> 
original_string method for character, thus keeping the original encoding, as 
well as the original entities in the strings.

See the C<t/test6.t> test file to see what results you can expect from the 
various encoding options.

B<WARNING>: if the original encoding is multi-byte then attribute parsing will
be EXTREMELY unsafe under any Perl before 5.6, as it uses regular expressions
which do not deal properly with multi-byte characters. You can specify an 
alternate function to parse the start tags with the C<parse_start_tag> option 
(see below)

B<WARNING>: this option is NOT used when parsing with XML::Parser non-blocking
parser (C<parse_start>, C<parse_more>, C<parse_done> methods) which you probably
should not use with XML::Twig anyway as they are totally untested!

=item output_encoding

This option generates an output_filter using C<Encode>,  C<Text::Iconv> or 
C<Unicode::Map8> and C<Unicode::Strings>, and sets the encoding in the XML
declaration. This is the easiest way to deal with encodings, if you need 
more sophisticated features, look at C<output_filter> below


=item output_filter

This option is used to convert the character encoding of the output document.
It is passed either a string corresponding to a predefined filter or
a subroutine reference. The filter will be called every time a document or 
element is processed by the "print" functions (C<print>, C<sprint>, C<flush>). 

Pre-defined filters: 

=over 4 

=item latin1 

uses either C<Encode>, C<Text::Iconv> or C<Unicode::Map8> and C<Unicode::String>
or a regexp (which works only with XML::Parser 2.27), in this order, to convert 
all characters to ISO-8859-15 (usually latin1 is synonym to ISO-8859-1, but
in practice it seems that ISO-8859-15, which includes the euro sign, is more 
useful and probably what most people want).

=item html

does the same conversion as C<latin1>, plus encodes entities using
C<HTML::Entities> (oddly enough you will need to have HTML::Entities installed 
for it to be available). This should only be used if the tags and attribute 
names themselves are in US-ASCII, or they will be converted and the output will
not be valid XML any more

=item safe

converts the output to ASCII (US) only  plus I<character entities> (C<&#nnn;>) 
this should be used only if the tags and attribute names themselves are in 
US-ASCII, or they will be converted and the output will not be valid XML any 
more

=item safe_hex

same as C<safe> except that the character entities are in hex (C<&#xnnn;>)

=item encode_convert ($encoding)

Return a subref that can be used to convert utf8 strings to C<$encoding>).
Uses C<Encode>.

   my $conv = XML::Twig::encode_convert( 'latin1');
   my $t = XML::Twig->new(output_filter => $conv);

=item iconv_convert ($encoding)

this function is used to create a filter subroutine that will be used to 
convert the characters to the target encoding using C<Text::Iconv> (which needs
to be installed, look at the documentation for the module and for the
C<iconv> library to find out which encodings are available on your system, 
C<iconv -l> should give you a list of available encodings)

   my $conv = XML::Twig::iconv_convert( 'latin1');
   my $t = XML::Twig->new(output_filter => $conv);

=item unicode_convert ($encoding)

this function is used to create a filter subroutine that will be used to 
convert the characters to the target encoding using  C<Unicode::Strings> 
and C<Unicode::Map8> (which need to be installed, look at the documentation 
for the modules to find out which encodings are available on your system)

   my $conv = XML::Twig::unicode_convert( 'latin1');
   my $t = XML::Twig->new(output_filter => $conv);

=back

The C<text> and C<att> methods do not use the filter, so their 
result are always in unicode.

Those predeclared filters are based on subroutines that can be used
by themselves (as C<XML::Twig::foo>). 

=over 4

=item html_encode ($string)

Use C<HTML::Entities> to encode a utf8 string

=item safe_encode ($string)

Use either a regexp (perl < 5.8) or C<Encode> to encode non-ascii characters
in the string in C<< &#<nnnn>; >> format

=item safe_encode_hex ($string)

Use either a regexp (perl < 5.8) or C<Encode> to encode non-ascii characters
in the string in C<< &#x<nnnn>; >> format

=item regexp2latin1 ($string)

Use a regexp to encode a utf8 string into latin 1 (ISO-8859-1). Does not
work with Perl 5.8.0!

=back

=item output_text_filter

same as output_filter, except it doesn't apply to the brackets and quotes 
around attribute values. This is useful for all filters that could change
the tagging, basically anything that does not just change the encoding of
the output. C<html>, C<safe> and C<safe_hex> are better used with this option.

=item input_filter

This option is similar to C<output_filter> except the filter is applied to 
the characters before they are stored in the twig, at parsing time.

=item remove_cdata

Setting this option to a true value will force the twig to output CDATA 
sections as regular (escaped) PCDATA

=item parse_start_tag

If you use the C<keep_encoding> option then this option can be used to replace
the default parsing function. You should provide a coderef (a reference to a 
subroutine) as the argument, this subroutine takes the original tag (given
by XML::Parser::Expat C<original_string()> method) and returns a tag and the
attributes in a hash (or in a list attribute_name/attribute value).

=item no_xxe

prevents external entities to be parsed. 

This is a security feature, in case the input XML cannot be trusted. With this
option set to a true value defining external entities in the document will cause
the parse to fail.  

This prevents an entity like C<< <!ENTITY xxe PUBLIC "bar" "/etc/passwd"> >> to
make the password fiel available in the document.


=item expand_external_ents

When this option is used external entities (that are defined) are expanded
when the document is output using "print" functions such as C<L<print> >,
C<L<sprint> >, C<L<flush> > and C<L<xml_string> >. 
Note that in the twig the entity will be stored as an element with a 
tag 'C<#ENT>', the entity will not be expanded there, so you might want to 
process the entities before outputting it.

If an external entity is not available, then the parse will fail.

A special case is when the value of this option is -1. In that case a missing
entity will not cause the parser to die, but its C<name>, C<sysid> and C<pubid>
will be stored in the twig as C<< $twig->{twig_missing_system_entities} >>
(a reference to an array of hashes { name => <name>, sysid => <sysid>,
pubid => <pubid> }). Yes, this is a bit of a hack, but it's useful in some
cases.  

=item load_DTD

If this argument is set to a true value, C<parse> or C<parsefile> on the twig
will load  the DTD information. This information can then be accessed through 
the twig, in a C<DTD_handler> for example. This will load even an external DTD.

Default and fixed values for attributes will also be filled, based on the DTD.

Note that to do this the module will generate a temporary file in the current
directory. If this is a problem let me know and I will add an option to
specify an alternate directory.

See L<DTD Handling> for more information

=item DTD_base <path_to_DTD_directory>

If the DTD is in a different directory, looks for it there, useful to make up 
somewhat for the lack of catalog suport in C<expat>. You still need a SYSTEM
declaration

=item DTD_handler

Set a handler that will be called once the doctype (and the DTD) have been 
loaded, with 2 arguments, the twig and the DTD.

=item no_prolog

Does not output a prolog (XML declaration and DTD)

=item id

This optional argument gives the name of an attribute that can be used as
an ID in the document. Elements whose ID is known can be accessed through
the elt_id method. id defaults to 'id'.
See C<L<BUGS> >

=item discard_spaces

If this optional argument is set to a true value then spaces are discarded
when they look non-significant: strings containing only spaces and at least
one line feed are discarded. This argument is set to true by default.

The exact algorithm to drop spaces is: strings including only spaces (perl \s)
and at least one \n right before an open or close tag are dropped.

=item discard_all_spaces

If this argument is set to a true value, spaces are discarded more 
aggressively than with C<discard_spaces>: strings not including a \n are also
dropped. This option is appropriate for data-oriented XML. 


=item keep_spaces

If this optional argument is set to a true value then all spaces in the
document are kept, and stored as C<PCDATA>.

B<Warning>: adding this option can result in changes in the twig generated:
space that was previously discarded might end up in a new text element. see
the difference by calling the following code with 0 and 1 as arguments:

  perl -MXML::Twig -e'print XML::Twig->new( keep_spaces => shift)->parse( "<d> \n<e/></d>")->_dump'


C<keep_spaces> and C<discard_spaces> cannot be both set.

=item discard_spaces_in

This argument sets C<keep_spaces> to true but will cause the twig builder to
discard spaces in the elements listed.

The syntax for using this argument is:

  XML::Twig->new( discard_spaces_in => [ 'elt1', 'elt2']);

=item keep_spaces_in

This argument sets C<discard_spaces> to true but will cause the twig builder to
keep spaces in the elements listed.

The syntax for using this argument is: 

  XML::Twig->new( keep_spaces_in => [ 'elt1', 'elt2']);

B<Warning>: adding this option can result in changes in the twig generated:
space that was previously discarded might end up in a new text element.

=item pretty_print

Set the pretty print method, amongst 'C<none>' (default), 'C<nsgmls>', 
'C<nice>', 'C<indented>', 'C<indented_c>', 'C<indented_a>', 
'C<indented_close_tag>', 'C<cvs>', 'C<wrapped>', 'C<record>' and 'C<record_c>'

pretty_print formats:

=over 4

=item none

The document is output as one ling string, with no line breaks except those 
found within text elements

=item nsgmls

Line breaks are inserted in safe places: that is within tags, between a tag 
and an attribute, between attributes and before the > at the end of a tag.

This is quite ugly but better than C<none>, and it is very safe, the document 
will still be valid (conforming to its DTD).

This is how the SGML parser C<sgmls> splits documents, hence the name.

=item nice

This option inserts line breaks before any tag that does not contain text (so
element with textual content are not broken as the \n is the significant).

B<WARNING>: this option leaves the document well-formed but might make it
invalid (not conformant to its DTD). If you have elements declared as

  <!ELEMENT foo (#PCDATA|bar)>

then a C<foo> element including a C<bar> one will be printed as

  <foo>
  <bar>bar is just pcdata</bar>
  </foo>

This is invalid, as the parser will take the line break after the C<foo> tag 
as a sign that the element contains PCDATA, it will then die when it finds the 
C<bar> tag. This may or may not be important for you, but be aware of it!

=item indented

Same as C<nice> (and with the same warning) but indents elements according to 
their level 

=item indented_c

Same as C<indented> but a little more compact: the closing tags are on the 
same line as the preceding text

=item indented_close_tag

Same as C<indented> except that the closing tag is also indented, to line up 
with the tags within the element

=item idented_a

This formats XML files in a line-oriented version control friendly way. 
The format is described in L<http://tinyurl.com/2kwscq> (that's an Oracle
document with an insanely long URL).

Note that to be totaly conformant to the "spec", the order of attributes
should not be changed, so if they are not already in alphabetical order
you will need to use the C<L<keep_atts_order>> option.

=item cvs

Same as C<L<idented_a>>.

=item wrapped

Same as C<indented_c> but lines are wrapped using L<Text::Wrap::wrap>. The 
default length for lines is the default for C<$Text::Wrap::columns>, and can
be changed by changing that variable.

=item record

This is a record-oriented pretty print, that display data in records, one field 
per line (which looks a LOT like C<indented>)

=item record_c

Stands for record compact, one record per line

=back


=item empty_tags

Set the empty tag display style ('C<normal>', 'C<html>' or 'C<expand>').

C<normal> outputs an empty tag 'C<< <tag/> >>', C<html> adds a space 
'C<< <tag /> >>' for elements that can be empty in XHTML and C<expand> outputs
'C<< <tag></tag> >>'

=item quote

Set the quote character for attributes ('C<single>' or 'C<double>').

=item escape_gt

By default XML::Twig does not escape the character > in its output, as it is not
mandated by the XML spec. With this option on, > will be replaced by C<&gt;>

=item comments

Set the way comments are processed: 'C<drop>' (default), 'C<keep>' or 
'C<process>' 

Comments processing options:

=over 4

=item drop

drops the comments, they are not read, nor printed to the output

=item keep

comments are loaded and will appear on the output, they are not 
accessible within the twig and will not interfere with processing
though

B<Note>: comments in the middle of a text element such as 

  <p>text <!-- comment --> more text --></p>

are kept at their original position in the text. Using ˝"print"
methods like C<print> or C<sprint> will return the comments in the
text. Using C<text> or C<field> on the other hand will not.

Any use of C<set_pcdata> on the C<#PCDATA> element (directly or 
through other methods like C<set_content>) will delete the comment(s).

=item process

comments are loaded in the twig and will be treated as regular elements 
(their C<tag> is C<#COMMENT>) this can interfere with processing if you
expect C<< $elt->{first_child} >> to be an element but find a comment there.
Validation will not protect you from this as comments can happen anywhere.
You can use C<< $elt->first_child( 'tag') >> (which is a good habit anyway)
to get where you want. 

Consider using C<process> if you are outputting SAX events from XML::Twig.

=back

=item pi

Set the way processing instructions are processed: 'C<drop>', 'C<keep>' 
(default) or 'C<process>'

Note that you can also set PI handlers in the C<twig_handlers> option: 

  '?'       => \&handler
  '?target' => \&handler 2

The handlers will be called with 2 parameters, the twig and the PI element if
C<pi> is set to C<process>, and with 3, the twig, the target and the data if
C<pi> is set to C<keep>. Of course they will not be called if C<pi> is set to 
C<drop>.

If C<pi> is set to C<keep> the handler should return a string that will be used
as-is as the PI text (it should look like "C< <?target data?> >" or '' if you
want to remove the PI), 

Only one handler will be called, C<?target> or C<?> if no specific handler for
that target is available.

=item map_xmlns 

This option is passed a hashref that maps uri's to prefixes. The prefixes in
the document will be replaced by the ones in the map. The mapped prefixes can
(actually have to) be used to trigger handlers, navigate or query the document.

Here is an example:

  my $t= XML::Twig->new( map_xmlns => {'http://www.w3.org/2000/svg' => "svg"},
                         twig_handlers => 
                           { 'svg:circle' => sub { $_->set_att( r => 20) } },
                         pretty_print => 'indented', 
                       )
                  ->parse( '<doc xmlns:gr="http://www.w3.org/2000/svg">
                              <gr:circle cx="10" cy="90" r="10"/>
                           </doc>'
                         )
                  ->print;

This will output:

  <doc xmlns:svg="http://www.w3.org/2000/svg">
     <svg:circle cx="10" cy="90" r="20"/>
  </doc>

=item keep_original_prefix

When used with C<L<map_xmlns>> this option will make C<XML::Twig> use the original
namespace prefixes when outputting a document. The mapped prefix will still be used
for triggering handlers and in navigation and query methods.

  my $t= XML::Twig->new( map_xmlns => {'http://www.w3.org/2000/svg' => "svg"},
                         twig_handlers => 
                           { 'svg:circle' => sub { $_->set_att( r => 20) } },
                         keep_original_prefix => 1,
                         pretty_print => 'indented', 
                       )
                  ->parse( '<doc xmlns:gr="http://www.w3.org/2000/svg">
                              <gr:circle cx="10" cy="90" r="10"/>
                           </doc>'
                         )
                  ->print;

This will output:

  <doc xmlns:gr="http://www.w3.org/2000/svg">
     <gr:circle cx="10" cy="90" r="20"/>
  </doc>

=item original_uri ($prefix)

called within a handler, this will return the uri bound to the namespace prefix
in the original document.

=item index ($arrayref or $hashref)

This option creates lists of specific elements during the parsing of the XML.
It takes a reference to either a list of triggering expressions or to a hash 
name => expression, and for each one generates the list of elements that 
match the expression. The list can be accessed through the C<L<index>> method.

example:

  # using an array ref
  my $t= XML::Twig->new( index => [ 'div', 'table' ])
                  ->parsefile( "foo.xml");
  my $divs= $t->index( 'div');
  my $first_div= $divs->[0];
  my $last_table= $t->index( table => -1);

  # using a hashref to name the indexes
  my $t= XML::Twig->new( index => { email => 'a[@href=~/^ \s*mailto:/]'})
                  ->parsefile( "foo.xml");
  my $last_emails= $t->index( email => -1);

Note that the index is not maintained after the parsing. If elements are 
deleted, renamed or otherwise hurt during processing, the index is NOT updated.
(changing the id element OTOH will update the index)

=item att_accessors <list of attribute names>

creates methods that give direct access to attribute:

  my $t= XML::Twig->new( att_accessors => [ 'href', 'src'])
                  ->parsefile( $file);
  my $first_href= $t->first_elt( 'img')->src; # same as ->att( 'src')
  $t->first_elt( 'img')->src( 'new_logo.png') # changes the attribute value

=item elt_accessors

creates methods that give direct access to the first child element (in scalar context) 
or the list of elements (in list context):

the list of accessors to create can be given 1 2 different ways: in an array, 
or in a hash alias => expression
  my $t=  XML::Twig->new( elt_accessors => [ 'head'])
                  ->parsefile( $file);
  my $title_text= $t->root->head->field( 'title');
  # same as $title_text= $t->root->first_child( 'head')->field( 'title');

  my $t=  XML::Twig->new( elt_accessors => { warnings => 'p[@class="warning"]', d2 => 'div[2]'}, )
                  ->parsefile( $file);
  my $body= $t->first_elt( 'body');
  my @warnings= $body->warnings; # same as $body->children( 'p[@class="warning"]');
  my $s2= $body->d2;             # same as $body->first_child( 'div[2]')

=item field_accessors

creates methods that give direct access to the first child element text:

  my $t=  XML::Twig->new( field_accessors => [ 'h1'])
                  ->parsefile( $file);
  my $div_title_text= $t->first_elt( 'div')->title;
  # same as $title_text= $t->first_elt( 'div')->field( 'title');

=item use_tidy

set this option to use HTML::Tidy instead of HTML::TreeBuilder to convert 
HTML to XML. HTML, especially real (real "crap") HTML found in the wild,
so depending on the data, one module or the other does a better job at 
the conversion. Also, HTML::Tidy can be a bit difficult to install, so
XML::Twig offers both option. TIMTOWTDI 

=item output_html_doctype

when using HTML::TreeBuilder to convert HTML, this option causes the DOCTYPE
declaration to be output, which may be important for some legacy browsers.
Without that option the DOCTYPE definition is NOT output. Also if the definition
is completely wrong (ie not easily parsable), it is not output either.

=back

B<Note>: I _HATE_ the Java-like name of arguments used by most XML modules.
So in pure TIMTOWTDI fashion all arguments can be written either as
C<UglyJavaLikeName> or as C<readable_perl_name>: C<twig_print_outside_roots>
or C<TwigPrintOutsideRoots> (or even C<twigPrintOutsideRoots> {shudder}). 
XML::Twig normalizes them before processing them.

=item parse ( $source)

The C<$source> parameter should either be a string containing the whole XML
document, or it should be an open C<IO::Handle> (aka a filehandle). 

A die call is thrown if a parse error occurs. Otherwise it will return 
the twig built by the parse. Use C<safe_parse> if you want the parsing
to return even when an error occurs.

If this method is called as a class method
(C<< XML::Twig->parse( $some_xml_or_html) >>) then an XML::Twig object is 
created, using the parameters except the last one (eg 
C<< XML::Twig->parse( pretty_print => 'indented', $some_xml_or_html) >>)
and C<L<xparse>> is called on it.

Note that when parsing a filehandle, the handle should NOT be open with an 
encoding (ie open with C<open( my $in, '<', $filename)>. The file will be
parsed by C<expat>, so specifying the encoding actually causes problems
for the parser (as in: it can crash it, see
https://rt.cpan.org/Ticket/Display.html?id=78877). For parsing a file it
is actually recommended to use C<parsefile> on the file name, instead of
<parse> on the open file.

=item parsestring

This is just an alias for C<parse> for backwards compatibility.

=item parsefile (FILE [, OPT => OPT_VALUE [...]])

Open C<FILE> for reading, then call C<parse> with the open handle. The file
is closed no matter how C<parse> returns. 

A C<die> call is thrown if a parse error occurs. Otherwise it will return 
the twig built by the parse. Use C<safe_parsefile> if you want the parsing
to return even when an error occurs.

=item parsefile_inplace ( $file, $optional_extension)

Parse and update a file "in place". It does this by creating a temp file,
selecting it as the default for print() statements (and methods), then parsing
the input file. If the parsing is successful, then the temp file is 
moved to replace the input file.

If an extension is given then the original file is backed-up (the rules for
the extension are the same as the rule for the -i option in perl).

=item parsefile_html_inplace ( $file, $optional_extension)

Same as parsefile_inplace, except that it parses HTML instead of XML 

=item parseurl ($url $optional_user_agent)

Gets the data from C<$url> and parse it. The data is piped to the parser in 
chunks the size of the XML::Parser::Expat buffer, so memory consumption and
hopefully speed are optimal.

For most (read "small") XML it is probably as efficient (and easier to debug)
to just C<get> the XML file and then parse it as a string.

  use XML::Twig;
  use LWP::Simple;
  my $twig= XML::Twig->new();
  $twig->parse( LWP::Simple::get( $URL ));

or

  use XML::Twig;
  my $twig= XML::Twig->nparse( $URL);


If the C<$optional_user_agent> argument is used then it is used, otherwise a
new one is created.

=item safe_parse ( SOURCE [, OPT => OPT_VALUE [...]])

This method is similar to C<parse> except that it wraps the parsing in an
C<eval> block. It returns the twig on success and 0 on failure (the twig object
also contains the parsed twig). C<$@> contains the error message on failure.

Note that the parsing still stops as soon as an error is detected, there is
no way to keep going after an error.

=item safe_parsefile (FILE [, OPT => OPT_VALUE [...]])

This method is similar to C<parsefile> except that it wraps the parsing in an
C<eval> block. It returns the twig on success and 0 on failure (the twig object
also contains the parsed twig) . C<$@> contains the error message on failure

Note that the parsing still stops as soon as an error is detected, there is
no way to keep going after an error.

=item safe_parseurl ($url $optional_user_agent)

Same as C<parseurl> except that it wraps the parsing in an C<eval> block. It 
returns the twig on success and 0 on failure (the twig object also contains
the parsed twig) . C<$@> contains the error message on failure

=item parse_html ($string_or_fh)

parse an HTML string or file handle (by converting it to XML using
HTML::TreeBuilder, which needs to be available).

This works nicely, but some information gets lost in the process:
newlines are removed, and (at least on the version I use), comments
get an extra CDATA section inside ( <!-- foo --> becomes
<!-- <![CDATA[ foo ]]> -->

=item parsefile_html ($file)

parse an HTML file (by converting it to XML using HTML::TreeBuilder, which 
needs to be available, or HTML::Tidy if the C<use_tidy> option was used).
The file is loaded completely in memory and converted to XML before being parsed.

this method is to be used with caution though, as it doesn't know about the
file encoding, it is usually better to use C<L<parse_html>>, which gives you
a chance to open the file with the proper encoding layer.

=item parseurl_html ($url $optional_user_agent)

parse an URL as html the same way C<L<parse_html>> does

=item safe_parseurl_html ($url $optional_user_agent)

Same as C<L<parseurl_html>>> except that it wraps the parsing in an C<eval>
block.  It returns the twig on success and 0 on failure (the twig object also
contains the parsed twig) . C<$@> contains the error message on failure

=item safe_parsefile_html ($file $optional_user_agent)

Same as C<L<parsefile_html>>> except that it wraps the parsing in an C<eval> 
block.  It returns the twig on success and 0 on failure (the twig object also 
contains the parsed twig) . C<$@> contains the error message on failure

=item safe_parse_html ($string_or_fh)

Same as C<L<parse_html>> except that it wraps the parsing in an C<eval> block. 
It returns the twig on success and 0 on failure (the twig object also contains
the parsed twig) . C<$@> contains the error message on failure

=item xparse ($thing_to_parse)

parse the C<$thing_to_parse>, whether it is a filehandle, a string, an HTML 
file, an HTML URL, an URL or a file.

Note that this is mostly a convenience method for one-off scripts. For example
files that end in '.htm' or '.html' are parsed first as XML, and if this fails
as HTML. This is certainly not the most efficient way to do this in general.

=item nparse ($optional_twig_options, $thing_to_parse)

create a twig with the C<$optional_options>, and parse the C<$thing_to_parse>, 
whether it is a filehandle, a string, an HTML file, an HTML URL, an URL or a 
file.

Examples:

   XML::Twig->nparse( "file.xml");
   XML::Twig->nparse( error_context => 1, "file://file.xml");

=item nparse_pp ($optional_twig_options, $thing_to_parse)

same as C<L<nparse>> but also sets the C<pretty_print> option to C<indented>.

=item nparse_e ($optional_twig_options, $thing_to_parse)

same as C<L<nparse>> but also sets the C<error_context> option to 1.

=item nparse_ppe ($optional_twig_options, $thing_to_parse)

same as C<L<nparse>> but also sets the C<pretty_print> option to C<indented>
and the C<error_context> option to 1.

=item parser

This method returns the C<expat> object (actually the XML::Parser::Expat object) 
used during parsing. It is useful for example to call XML::Parser::Expat methods
on it. To get the line of a tag for example use C<< $t->parser->current_line >>.

=item setTwigHandlers ($handlers)

Set the twig_handlers. C<$handlers> is a reference to a hash similar to the
one in the C<twig_handlers> option of new. All previous handlers are unset.
The method returns the reference to the previous handlers.

=item setTwigHandler ($exp $handler)

Set a single twig_handler for elements matching C<$exp>. C<$handler> is a 
reference to a subroutine. If the handler was previously set then the reference 
to the previous handler is returned.

=item setStartTagHandlers ($handlers)

Set the start_tag handlers. C<$handlers> is a reference to a hash similar to the
one in the C<start_tag_handlers> option of new. All previous handlers are unset.
The method returns the reference to the previous handlers.

=item setStartTagHandler ($exp $handler)

Set a single start_tag handlers for elements matching C<$exp>. C<$handler> is a 
reference to a subroutine. If the handler was previously set then the reference
to the previous handler is returned.

=item setEndTagHandlers ($handlers)

Set the end_tag handlers. C<$handlers> is a reference to a hash similar to the
one in the C<end_tag_handlers> option of new. All previous handlers are unset.
The method returns the reference to the previous handlers.

=item setEndTagHandler ($exp $handler)

Set a single end_tag handlers for elements matching C<$exp>. C<$handler> is a 
reference to a subroutine. If the handler was previously set then the 
reference to the previous handler is returned.

=item setTwigRoots ($handlers)

Same as using the C<L<twig_roots>> option when creating the twig

=item setCharHandler ($exp $handler)

Set a C<char_handler>

=item setIgnoreEltsHandler ($exp)

Set a C<ignore_elt> handler (elements that match C<$exp> will be ignored

=item setIgnoreEltsHandlers ($exp)

Set all C<ignore_elt> handlers (previous handlers are replaced)

=item dtd

Return the dtd (an L<XML::Twig::DTD> object) of a twig

=item xmldecl

Return the XML declaration for the document, or a default one if it doesn't
have one

=item doctype

Return the doctype for the document

=item doctype_name

returns the doctype of the document from the doctype declaration

=item system_id

returns the system value of the DTD of the document from the doctype declaration

=item public_id

returns the public doctype of the document from the doctype declaration

=item internal_subset

returns the internal subset of the DTD

=item dtd_text

Return the DTD text

=item dtd_print

Print the DTD

=item model ($tag)

Return the model (in the DTD) for the element C<$tag>

=item root

Return the root element of a twig

=item set_root ($elt)

Set the root of a twig

=item first_elt ($optional_condition)

Return the first element matching C<$optional_condition> of a twig, if
no condition is given then the root is returned

=item last_elt ($optional_condition)

Return the last element matching C<$optional_condition> of a twig, if
no condition is given then the last element of the twig is returned

=item elt_id        ($id)

Return the element whose C<id> attribute is $id

=item getEltById

Same as C<L<elt_id>>

=item index ($index_name, $optional_index)

If the C<$optional_index> argument is present, return the corresponding element
in the index (created using the C<index> option for C<XML::Twig->new>)

If the argument is not present, return an arrayref to the index

=item normalize

merge together all consecutive pcdata elements in the document (if for example
you have turned some elements into pcdata using C<L<erase>>, this will give you
a "clean" document in which there all text elements are as long as possible).

=item encoding

This method returns the encoding of the XML document, as defined by the 
C<encoding> attribute in the XML declaration (ie it is C<undef> if the attribute
is not defined)

=item set_encoding

This method sets the value of the C<encoding> attribute in the XML declaration. 
Note that if the document did not have a declaration it is generated (with
an XML version of 1.0)

=item xml_version

This method returns the XML version, as defined by the C<version> attribute in 
the XML declaration (ie it is C<undef> if the attribute is not defined)

=item set_xml_version

This method sets the value of the C<version> attribute in the XML declaration. 
If the declaration did not exist it is created.

=item standalone

This method returns the value of the C<standalone> declaration for the document

=item set_standalone

This method sets the value of the C<standalone> attribute in the XML 
declaration.  Note that if the document did not have a declaration it is 
generated (with an XML version of 1.0)

=item set_output_encoding

Set the C<encoding> "attribute" in the XML declaration

=item set_doctype ($name, $system, $public, $internal)

Set the doctype of the element. If an argument is C<undef> (or not present)
then its former value is retained, if a false ('' or 0) value is passed then
the former value is deleted;

=item entity_list

Return the entity list of a twig

=item entity_names

Return the list of all defined entities

=item entity ($entity_name)

Return the entity 

=item notation_list

Return the notation list of a twig

=item notation_names

Return the list of all defined notations

=item notation ($notation_name)

Return the notation 

=item change_gi      ($old_gi, $new_gi)

Performs a (very fast) global change. All elements C<$old_gi> are now 
C<$new_gi>. This is a bit dangerous though and should be avoided if
< possible, as the new tag might be ignored in subsequent processing.

See C<L<BUGS> >

=item flush            ($optional_filehandle, %options)

Flushes a twig up to (and including) the current element, then deletes
all unnecessary elements from the tree that's kept in memory.
C<flush> keeps track of which elements need to be open/closed, so if you
flush from handlers you don't have to worry about anything. Just keep 
flushing the twig every time you're done with a sub-tree and it will
come out well-formed. After the whole parsing don't forget toC<flush> 
one more time to print the end of the document.
The doctype and entity declarations are also printed.

flush take an optional filehandle as an argument.

If you use C<flush> at any point during parsing, the document will be flushed
one last time at the end of the parsing, to the proper filehandle.

options: use the C<update_DTD> option if you have updated the (internal) DTD 
and/or the entity list and you want the updated DTD to be output 

The C<pretty_print> option sets the pretty printing of the document.

   Example: $t->flush( Update_DTD => 1);
            $t->flush( $filehandle, pretty_print => 'indented');
            $t->flush( \*FILE);


=item flush_up_to ($elt, $optional_filehandle, %options)

Flushes up to the C<$elt> element. This allows you to keep part of the
tree in memory when you C<flush>.

options: see flush.

=item purge

Does the same as a C<flush> except it does not print the twig. It just deletes
all elements that have been completely parsed so far.

=item purge_up_to ($elt)

Purges up to the C<$elt> element. This allows you to keep part of the tree in 
memory when you C<purge>.

=item print            ($optional_filehandle, %options)

Prints the whole document associated with the twig. To be used only AFTER the
parse.

options: see C<flush>.

=item print_to_file    ($filename, %options)

Prints the whole document associated with the twig to file C<$filename>.
To be used only AFTER the parse.

options: see C<flush>.

=item safe_print_to_file    ($filename, %options)

Prints the whole document associated with the twig to file C<$filename>.
This variant, which probably only works on *nix prints to a temp file,
then move the temp file to overwrite the original file.

This is a bit safer when 2 processes an potentiallywrite the same file: 
only the last one will succeed, but the file won't be corruted. I often
use this for cron jobs, so testing the code doesn't interfere with the
cron job running at the same time.   

options: see C<flush>.

=item sprint

Return the text of the whole document associated with the twig. To be used only
AFTER the parse.

options: see C<flush>.

=item trim

Trim the document: gets rid of initial and trailing spaces, and replaces multiple spaces
by a single one.

=item toSAX1 ($handler)

Send SAX events for the twig to the SAX1 handler C<$handler>

=item toSAX2 ($handler)

Send SAX events for the twig to the SAX2 handler C<$handler>

=item flush_toSAX1 ($handler)

Same as flush, except that SAX events are sent to the SAX1 handler
C<$handler> instead of the twig being printed

=item flush_toSAX2 ($handler)

Same as flush, except that SAX events are sent to the SAX2 handler
C<$handler> instead of the twig being printed

=item ignore

This method should be called during parsing, usually in C<start_tag_handlers>.
It causes the element to be skipped during the parsing: the twig is not built
for this element, it will not be accessible during parsing or after it. The 
element will not take up any memory and parsing will be faster.

Note that this method can also be called on an element. If the element is a 
parent of the current element then this element will be ignored (the twig will
not be built any more for it and what has already been built will be deleted).

=item set_pretty_print  ($style)

Set the pretty print method, amongst 'C<none>' (default), 'C<nsgmls>', 
'C<nice>', 'C<indented>', C<indented_c>, 'C<wrapped>', 'C<record>' and 
'C<record_c>'

B<WARNING:> the pretty print style is a B<GLOBAL> variable, so once set it's
applied to B<ALL> C<print>'s (and C<sprint>'s). Same goes if you use XML::Twig
with C<mod_perl> . This should not be a problem as the XML that's generated 
is valid anyway, and XML processors (as well as HTML processors, including
browsers) should not care. Let me know if this is a big problem, but at the
moment the performance/cleanliness trade-off clearly favors the global 
approach.

=item set_empty_tag_style  ($style)

Set the empty tag display style ('C<normal>', 'C<html>' or 'C<expand>'). As 
with C<L<set_pretty_print>> this sets a global flag.  

C<normal> outputs an empty tag 'C<< <tag/> >>', C<html> adds a space 
'C<< <tag /> >>' for elements that can be empty in XHTML and C<expand> outputs
'C<< <tag></tag> >>'

=item set_remove_cdata  ($flag)

set (or unset) the flag that forces the twig to output CDATA sections as 
regular (escaped) PCDATA

=item print_prolog     ($optional_filehandle, %options)

Prints the prolog (XML declaration + DTD + entity declarations) of a document.

options: see C<L<flush>>.

=item prolog     ($optional_filehandle, %options)

Return the prolog (XML declaration + DTD + entity declarations) of a document.

options: see C<L<flush>>.

=item finish

Call Expat C<finish> method.
Unsets all handlers (including internal ones that set context), but expat
continues parsing to the end of the document or until it finds an error.
It should finish up a lot faster than with the handlers set.

=item finish_print

Stops twig processing, flush the twig and proceed to finish printing the 
document as fast as possible. Use this method when modifying a document and 
the modification is done. 

=item finish_now

Stops twig processing, does not finish parsing the document (which could
actually be not well-formed after the point where C<finish_now> is called).
Execution resumes after the C<Lparse>> or C<L<parsefile>> call. The content
of the twig is what has been parsed so far (all open elements at the time 
C<finish_now> is called are considered closed).

=item set_expand_external_entities

Same as using the C<L<expand_external_ents>> option when creating the twig

=item set_input_filter

Same as using the C<L<input_filter>> option when creating the twig

=item set_keep_atts_order

Same as using the C<L<keep_atts_order>> option when creating the twig

=item set_keep_encoding

Same as using the C<L<keep_encoding>> option when creating the twig

=item escape_gt

usually XML::Twig does not escape > in its output. Using this option
makes it replace > by &gt;

=item do_not_escape_gt

reverts XML::Twig behavior to its default of not escaping > in its output.

=item set_output_filter

Same as using the C<L<output_filter>> option when creating the twig

=item set_output_text_filter

Same as using the C<L<output_text_filter>> option when creating the twig

=item add_stylesheet ($type, @options)

Adds an external stylesheet to an XML document.

Supported types and options:

=over 4

=item xsl

option: the url of the stylesheet

Example:

  $t->add_stylesheet( xsl => "xsl_style.xsl");

will generate the following PI at the beginning of the document:

  <?xml-stylesheet type="text/xsl" href="xsl_style.xsl"?>

=item css

option: the url of the stylesheet

=item active_twig

a class method that returns the last processed twig, so you don't necessarily
need the object to call methods on it.

=back

=item Methods inherited from XML::Parser::Expat

A twig inherits all the relevant methods from XML::Parser::Expat. These 
methods can only be used during the parsing phase (they will generate
a fatal error otherwise).

Inherited methods are:

=over 4

=item depth

Returns the size of the context list.

=item in_element

Returns true if NAME is equal to the name of the innermost cur‐
rently opened element. If namespace processing is being used and
you want to check against a name that may be in a namespace, then
use the generate_ns_name method to create the NAME argument.

=item within_element

Returns the number of times the given name appears in the context
list.  If namespace processing is being used and you want to check
against a name that may be in a namespace, then use the gener‐
ate_ns_name method to create the NAME argument.

=item context

Returns a list of element names that represent open elements, with
the last one being the innermost. Inside start and end tag han‐
dlers, this will be the tag of the parent element.

=item current_line

Returns the line number of the current position of the parse.

=item current_column

Returns the column number of the current position of the parse.

=item current_byte

Returns the current position of the parse.

=item position_in_context

Returns a string that shows the current parse position. LINES
should be an integer >= 0 that represents the number of lines on
either side of the current parse line to place into the returned
string.

=item base ([NEWBASE])

Returns the current value of the base for resolving relative URIs.
If NEWBASE is supplied, changes the base to that value.

=item current_element

Returns the name of the innermost currently opened element. Inside
start or end handlers, returns the parent of the element associated
with those tags.

=item element_index

Returns an integer that is the depth-first visit order of the cur‐
rent element. This will be zero outside of the root element. For
example, this will return 1 when called from the start handler for
the root element start tag.

=item recognized_string

Returns the string from the document that was recognized in order
to call the current handler. For instance, when called from a start
handler, it will give us the start-tag string. The string is
encoded in UTF-8.  This method doesn't return a meaningful string
inside declaration handlers.

=item original_string

Returns the verbatim string from the document that was recognized
in order to call the current handler. The string is in the original
document encoding. This method doesn't return a meaningful string
inside declaration handlers.

=item xpcroak

Concatenate onto the given message the current line number within
the XML document plus the message implied by ErrorContext. Then
croak with the formed message.

=item xpcarp 

Concatenate onto the given message the current line number within
the XML document plus the message implied by ErrorContext. Then
carp with the formed message.

=item xml_escape(TEXT [, CHAR [, CHAR ...]])

Returns TEXT with markup characters turned into character entities.
Any additional characters provided as arguments are also turned
into character references where found in TEXT.

(this method is broken on some versions of expat/XML::Parser)

=back

=item path ( $optional_tag)

Return the element context in a form similar to XPath's short
form: 'C</root/tag1/../tag>'

=item get_xpath  ( $optional_array_ref, $xpath, $optional_offset)

Performs a C<get_xpath> on the document root (see <Elt|"Elt">)

If the C<$optional_array_ref> argument is used the array must contain
elements. The C<$xpath> expression is applied to each element in turn 
and the result is union of all results. This way a first query can be
refined in further steps.


=item find_nodes ( $optional_array_ref, $xpath, $optional_offset)

same as C<get_xpath> 

=item findnodes ( $optional_array_ref, $xpath, $optional_offset)

same as C<get_xpath> (similar to the XML::LibXML method)

=item findvalue ( $optional_array_ref, $xpath, $optional_offset)

Return the C<join> of all texts of the results of applying C<L<get_xpath>>
to the node (similar to the XML::LibXML method)

=item findvalues ( $optional_array_ref, $xpath, $optional_offset)

Return an array of all texts of the results of applying C<L<get_xpath>>
to the node 

=item subs_text ($regexp, $replace)

subs_text does text substitution on the whole document, similar to perl's 
C< s///> operator.

=item dispose

Useful only if you don't have C<Scalar::Util> or C<WeakRef> installed.

Reclaims properly the memory used by an XML::Twig object. As the object has
circular references it never goes out of scope, so if you want to parse lots 
of XML documents then the memory leak becomes a problem. Use
C<< $twig->dispose >> to clear this problem.

=item att_accessors (list_of_attribute_names)

A convenience method that creates l-valued accessors for attributes. 
So C<< $twig->create_accessors( 'foo') >> will create a C<foo> method
that can be called on elements:

  $elt->foo;         # equivalent to $elt->{'att'}->{'foo'};
  $elt->foo( 'bar'); # equivalent to $elt->set_att( foo => 'bar');

The methods are l-valued only under those perl's that support this
feature (5.6 and above)

=item create_accessors (list_of_attribute_names)

Same as att_accessors

=item elt_accessors (list_of_attribute_names)

A convenience method that creates accessors for elements. 
So C<< $twig->create_accessors( 'foo') >> will create a C<foo> method
that can be called on elements:

  $elt->foo;         # equivalent to $elt->first_child( 'foo');

=item field_accessors (list_of_attribute_names)

A convenience method that creates accessors for element values (C<field>). 
So C<< $twig->create_accessors( 'foo') >> will create a C<foo> method
that can be called on elements:

  $elt->foo;         # equivalent to $elt->field( 'foo');

=item set_do_not_escape_amp_in_atts

An evil method, that I only document because Test::Pod::Coverage complaints otherwise,
but really, you don't want to know about it.

=back 

=head2 XML::Twig::Elt

=over 4

=item new          ($optional_tag, $optional_atts, @optional_content)

The C<tag> is optional (but then you can't have a content ), the C<$optional_atts> 
argument is a reference to a hash of attributes, the content can be just a 
string or a list of strings and element. A content of 'C<#EMPTY>' creates an empty 
element;

 Examples: my $elt= XML::Twig::Elt->new();
           my $elt= XML::Twig::Elt->new( para => { align => 'center' });  
           my $elt= XML::Twig::Elt->new( para => { align => 'center' }, 'foo');  
           my $elt= XML::Twig::Elt->new( br   => '#EMPTY');
           my $elt= XML::Twig::Elt->new( 'para');
           my $elt= XML::Twig::Elt->new( para => 'this is a para');  
           my $elt= XML::Twig::Elt->new( para => $elt3, 'another para'); 

The strings are not parsed, the element is not attached to any twig.

B<WARNING>: if you rely on ID's then you will have to set the id yourself. At
this point the element does not belong to a twig yet, so the ID attribute
is not known so it won't be stored in the ID list.

Note that C<#COMMENT>, C<#PCDATA> or C<#CDATA> are valid tag names, that will 
create text elements.

To create an element C<foo> containing a CDATA section:

           my $foo= XML::Twig::Elt->new( '#CDATA' => "content of the CDATA section")
                                  ->wrap_in( 'foo');

An attribute of '#CDATA', will create the content of the element as CDATA:

  my $elt= XML::Twig::Elt->new( 'p' => { '#CDATA' => 1}, 'foo < bar');

creates an element 

  <p><![CDATA[foo < bar]]></>

=item parse         ($string, %args)

Creates an element from an XML string. The string is actually
parsed as a new twig, then the root of that twig is returned.
The arguments in C<%args> are passed to the twig.
As always if the parse fails the parser will die, so use an
eval if you want to trap syntax errors.

As obviously the element does not exist beforehand this method has to be 
called on the class: 

  my $elt= parse XML::Twig::Elt( "<a> string to parse, with <sub/>
                                  <elements>, actually tons of </elements>
                  h</a>");

=item set_inner_xml ($string)

Sets the content of the element to be the tree created from the string

=item set_inner_html ($string)

Sets the content of the element, after parsing the string with an HTML
parser (HTML::Parser)

=item set_outer_xml ($string)

Replaces the element with the tree created from the string

=item print         ($optional_filehandle, $optional_pretty_print_style)

Prints an entire element, including the tags, optionally to a 
C<$optional_filehandle>, optionally with a C<$pretty_print_style>.

The print outputs XML data so base entities are escaped.

=item print_to_file    ($filename, %options)

Prints the element to file C<$filename>.

options: see C<flush>.
=item sprint       ($elt, $optional_no_enclosing_tag)

Return the xml string for an entire element, including the tags. 
If the optional second argument is true then only the string inside the 
element is returned (the start and end tag for $elt are not).
The text is XML-escaped: base entities (& and < in text, & < and " in
attribute values) are turned into entities.

=item gi                       

Return the gi of the element (the gi is the C<generic identifier> the tag
name in SGML parlance).

C<tag> and C<name> are synonyms of C<gi>.

=item tag

Same as C<L<gi>>

=item name

Same as C<L<tag>>

=item set_gi         ($tag)

Set the gi (tag) of an element

=item set_tag        ($tag)

Set the tag (=C<L<tag>>) of an element

=item set_name       ($name)

Set the name (=C<L<tag>>) of an element

=item root 

Return the root of the twig in which the element is contained.

=item twig 

Return the twig containing the element. 

=item parent        ($optional_condition)

Return the parent of the element, or the first ancestor matching the 
C<$optional_condition>

=item first_child   ($optional_condition)

Return the first child of the element, or the first child matching the 
C<$optional_condition>

=item has_child ($optional_condition)

Return the first child of the element, or the first child matching the 
C<$optional_condition> (same as L<first_child>)

=item has_children ($optional_condition)

Return the first child of the element, or the first child matching the 
C<$optional_condition> (same as L<first_child>)


=item first_child_text   ($optional_condition)

Return the text of the first child of the element, or the first child
 matching the C<$optional_condition>
If there is no first_child then returns ''. This avoids getting the
child, checking for its existence then getting the text for trivial cases.

Similar methods are available for the other navigation methods: 

=over 4

=item last_child_text

=item prev_sibling_text

=item next_sibling_text

=item prev_elt_text

=item next_elt_text

=item child_text

=item parent_text

=back

All this methods also exist in "trimmed" variant: 

=over 4

=item first_child_trimmed_text

=item last_child_trimmed_text

=item prev_sibling_trimmed_text

=item next_sibling_trimmed_text

=item prev_elt_trimmed_text

=item next_elt_trimmed_text

=item child_trimmed_text

=item parent_trimmed_text

=back

=item field         ($condition)

Same method as C<first_child_text> with a different name

=item fields         ($condition_list)

Return the list of field (text of first child matching the conditions),
missing fields are returned as the empty string.

Same method as C<first_child_text> with a different name

=item trimmed_field         ($optional_condition)

Same method as C<first_child_trimmed_text> with a different name

=item set_field ($condition, $optional_atts, @list_of_elt_and_strings)

Set the content of the first child of the element that matches
C<$condition>, the rest of the arguments is the same as for C<L<set_content>>

If no child matches C<$condition> _and_ if C<$condition> is a valid
XML element name, then a new element by that name is created and 
inserted as the last child.

=item first_child_matches   ($optional_condition)

Return the element if the first child of the element (if it exists) passes
the C<$optional_condition> C<undef> otherwise

  if( $elt->first_child_matches( 'title')) ... 

is equivalent to

  if( $elt->{first_child} && $elt->{first_child}->passes( 'title')) 

C<first_child_is> is an other name for this method

Similar methods are available for the other navigation methods: 

=over 4

=item last_child_matches

=item prev_sibling_matches

=item next_sibling_matches

=item prev_elt_matches

=item next_elt_matches

=item child_matches

=item parent_matches

=back

=item is_first_child ($optional_condition)

returns true (the element) if the element is the first child of its parent
(optionally that satisfies the C<$optional_condition>)

=item is_last_child ($optional_condition)

returns true (the element) if the element is the last child of its parent
(optionally that satisfies the C<$optional_condition>)

=item prev_sibling  ($optional_condition)

Return the previous sibling of the element, or the previous sibling matching
C<$optional_condition>

=item next_sibling  ($optional_condition)

Return the next sibling of the element, or the first one matching 
C<$optional_condition>.

=item next_elt     ($optional_elt, $optional_condition)

Return the next elt (optionally matching C<$optional_condition>) of the element. This 
is defined as the next element which opens after the current element opens.
Which usually means the first child of the element.
Counter-intuitive as it might look this allows you to loop through the
whole document by starting from the root.

The C<$optional_elt> is the root of a subtree. When the C<next_elt> is out of the
subtree then the method returns undef. You can then walk a sub-tree with:

  my $elt= $subtree_root;
  while( $elt= $elt->next_elt( $subtree_root))
    { # insert processing code here
    }

=item prev_elt     ($optional_condition)

Return the previous elt (optionally matching C<$optional_condition>) of the
element. This is the first element which opens before the current one.
It is usually either the last descendant of the previous sibling or
simply the parent

=item next_n_elt   ($offset, $optional_condition)

Return the C<$offset>-th element that matches the C<$optional_condition> 

=item following_elt

Return the following element (as per the XPath following axis)

=item preceding_elt

Return the preceding element (as per the XPath preceding axis)

=item following_elts

Return the list of following elements (as per the XPath following axis)

=item preceding_elts

Return the list of preceding elements (as per the XPath preceding axis)

=item children     ($optional_condition)

Return the list of children (optionally which matches C<$optional_condition>) of 
the element. The list is in document order.

=item children_count ($optional_condition)

Return the number of children of the element (optionally which matches 
C<$optional_condition>)

=item children_text ($optional_condition)

In array context, returns an array containing the text of children of the
element (optionally which matches C<$optional_condition>)

In scalar context, returns the concatenation of the text of children of
the element

=item children_trimmed_text ($optional_condition)

In array context, returns an array containing the trimmed text of children 
of the element (optionally which matches C<$optional_condition>)

In scalar context, returns the concatenation of the trimmed text of children of
the element


=item children_copy ($optional_condition)

Return a list of elements that are copies of the children of the element, 
optionally which matches C<$optional_condition>

=item descendants     ($optional_condition)

Return the list of all descendants (optionally which matches 
C<$optional_condition>) of the element. This is the equivalent of the 
C<getElementsByTagName> of the DOM (by the way, if you are really a DOM 
addict, you can use C<getElementsByTagName> instead)

=item getElementsByTagName ($optional_condition)

Same as C<L<descendants>>

=item find_by_tag_name ($optional_condition)

Same as C<L<descendants>>

=item descendants_or_self ($optional_condition)

Same as C<L<descendants>> except that the element itself is included in the list
if it matches the C<$optional_condition> 

=item first_descendant  ($optional_condition)

Return the first descendant of the element that matches the condition  

=item last_descendant  ($optional_condition)

Return the last descendant of the element that matches the condition  

=item ancestors    ($optional_condition)

Return the list of ancestors (optionally matching C<$optional_condition>) of the 
element.  The list is ordered from the innermost ancestor to the outermost one

NOTE: the element itself is not part of the list, in order to include it 
you will have to use ancestors_or_self

=item ancestors_or_self     ($optional_condition)

Return the list of ancestors (optionally matching C<$optional_condition>) of the 
element, including the element (if it matches the condition>).  
The list is ordered from the innermost ancestor to the outermost one

=item passes ($condition)

Return the element if it passes the C<$condition> 

=item att          ($att)

Return the value of attribute C<$att> or C<undef>

=item latt          ($att)

Return the value of attribute C<$att> or C<undef>

this method is an lvalue, so you can do C<< $elt->latt( 'foo')= 'bar' >> or C<< $elt->latt( 'foo')++; >>

=item set_att      ($att, $att_value)

Set the attribute of the element to the given value

You can actually set several attributes this way:

  $elt->set_att( att1 => "val1", att2 => "val2");

=item del_att      ($att)

Delete the attribute for the element

You can actually delete several attributes at once:

  $elt->del_att( 'att1', 'att2', 'att3');

=item att_exists ($att)

Returns true if the attribute C<$att> exists for the element, false 
otherwise

=item cut

Cut the element from the tree. The element still exists, it can be copied
or pasted somewhere else, it is just not attached to the tree anymore.

Note that the "old" links to the parent, previous and next siblings can
still be accessed using the former_* methods

=item former_next_sibling

Returns the former next sibling of a cut node (or undef if the node has not been cut)

This makes it easier to write loops where you cut elements:

    my $child= $parent->first_child( 'achild');
    while( $child->{'att'}->{'cut'}) 
      { $child->cut; $child= ($child->{former} && $child->{former}->{next_sibling}); }

=item former_prev_sibling

Returns the former previous sibling of a cut node (or undef if the node has not been cut)

=item former_parent

Returns the former parent of a cut node (or undef if the node has not been cut)

=item cut_children ($optional_condition)

Cut all the children of the element (or all of those which satisfy the
C<$optional_condition>).

Return the list of children 

=item cut_descendants ($optional_condition)

Cut all the descendants of the element (or all of those which satisfy the
C<$optional_condition>).

Return the list of descendants 

=item copy        ($elt)

Return a copy of the element. The copy is a "deep" copy: all sub-elements of 
the element are duplicated.

=item paste       ($optional_position, $ref)

Paste a (previously C<cut> or newly generated) element. Die if the element
already belongs to a tree.

Note that the calling element is pasted:

  $child->paste( first_child => $existing_parent);
  $new_sibling->paste( after => $this_sibling_is_already_in_the_tree);

or

  my $new_elt= XML::Twig::Elt->new( tag => $content);
  $new_elt->paste( $position => $existing_elt);

Example:

  my $t= XML::Twig->new->parse( 'doc.xml')
  my $toc= $t->root->new( 'toc');
  $toc->paste( $t->root); # $toc is pasted as first child of the root 
  foreach my $title ($t->findnodes( '/doc/section/title'))
    { my $title_toc= $title->copy;
      # paste $title_toc as the last child of toc
      $title_toc->paste( last_child => $toc) 
    }

Position options:

=over 4

=item first_child (default)

The element is pasted as the first child of C<$ref>

=item last_child

The element is pasted as the last child of C<$ref>

=item before

The element is pasted before C<$ref>, as its previous sibling.

=item after

The element is pasted after C<$ref>, as its next sibling.

=item within

In this case an extra argument, C<$offset>, should be supplied. The element
will be pasted in the reference element (or in its first text child) at the
given offset. To achieve this the reference element will be split at the 
offset.

=back

Note that you can call directly the underlying method:

=over 4

=item paste_before

=item paste_after

=item paste_first_child

=item paste_last_child

=item paste_within

=back

=item move       ($optional_position, $ref)

Move an element in the tree.
This is just a C<cut> then a C<paste>.  The syntax is the same as C<paste>.

=item replace       ($ref)

Replaces an element in the tree. Sometimes it is just not possible toC<cut> 
an element then C<paste> another in its place, so C<replace> comes in handy.
The calling element replaces C<$ref>.

=item replace_with   (@elts)

Replaces the calling element with one or more elements 

=item delete

Cut the element and frees the memory.

=item prefix       ($text, $optional_option)

Add a prefix to an element. If the element is a C<PCDATA> element the text
is added to the pcdata, if the elements first child is a C<PCDATA> then the
text is added to it's pcdata, otherwise a new C<PCDATA> element is created 
and pasted as the first child of the element.

If the option is C<asis> then the prefix is added asis: it is created in
a separate C<PCDATA> element with an C<asis> property. You can then write:

  $elt1->prefix( '<b>', 'asis');

to create a C<< <b> >> in the output of C<print>.

=item suffix       ($text, $optional_option)

Add a suffix to an element. If the element is a C<PCDATA> element the text
is added to the pcdata, if the elements last child is a C<PCDATA> then the
text is added to it's pcdata, otherwise a new PCDATA element is created 
and pasted as the last child of the element.

If the option is C<asis> then the suffix is added asis: it is created in
a separate C<PCDATA> element with an C<asis> property. You can then write:

  $elt2->suffix( '</b>', 'asis');

=item trim

Trim the element in-place: spaces at the beginning and at the end of the element
are discarded and multiple spaces within the element (or its descendants) are 
replaced by a single space.

Note that in some cases you can still end up with multiple spaces, if they are
split between several elements:

  <doc>  text <b>  hah! </b>  yep</doc>

gets trimmed to

  <doc>text <b> hah! </b> yep</doc>

This is somewhere in between a bug and a feature.

=item normalize

merge together all consecutive pcdata elements in the element (if for example
you have turned some elements into pcdata using C<L<erase>>, this will give you
a "clean" element in which there all text fragments are as long as possible).


=item simplify (%options)

Return a data structure suspiciously similar to XML::Simple's. Options are
identical to XMLin options, see XML::Simple doc for more details (or use
DATA::dumper or YAML to dump the data structure)

B<Note>: there is no magic here, if you write 
C<< $twig->parsefile( $file )->simplify(); >> then it will load the entire 
document in memory. I am afraid you will have to put some work into it to 
get just the bits you want and discard the rest. Look at the synopsis or
the XML::Twig 101 section at the top of the docs for more information.

=over 4

=item content_key

=item forcearray 

=item keyattr 

=item noattr 

=item normalize_space

aka normalise_space

=item variables (%var_hash)

%var_hash is a hash { name => value }

This option allows variables in the XML to be expanded when the file is read. (there is no facility for putting the variable names back if you regenerate XML using XMLout).

A 'variable' is any text of the form ${name} (or $name) which occurs in an attribute value or in the text content of an element. If 'name' matches a key in the supplied hashref, ${name} will be replaced with the corresponding value from the hashref. If no matching key is found, the variable will not be replaced. 

=item var_att ($attribute_name)

This option gives the name of an attribute that will be used to create 
variables in the XML:

  <dirs>
    <dir name="prefix">/usr/local</dir>
    <dir name="exec_prefix">$prefix/bin</dir>
  </dirs>

use C<< var => 'name' >> to get $prefix replaced by /usr/local in the
generated data structure  

By default variables are captured by the following regexp: /$(\w+)/

=item var_regexp (regexp)

This option changes the regexp used to capture variables. The variable
name should be in $1

=item group_tags { grouping tag => grouped tag, grouping tag 2 => grouped tag 2...}

Option used to simplify the structure: elements listed will not be used.
Their children will be, they will be considered children of the element
parent.

If the element is:

  <config host="laptop.xmltwig.org">
    <server>localhost</server>
    <dirs>
      <dir name="base">/home/mrodrigu/standards</dir>
      <dir name="tools">$base/tools</dir>
    </dirs>
    <templates>
      <template name="std_def">std_def.templ</template>
      <template name="dummy">dummy</template>
    </templates>
  </config>

Then calling simplify with C<< group_tags => { dirs => 'dir',
templates => 'template'} >>
makes the data structure be exactly as if the start and end tags for C<dirs> and
C<templates> were not there.

A YAML dump of the structure 

  base: '/home/mrodrigu/standards'
  host: laptop.xmltwig.org
  server: localhost
  template:
    - std_def.templ
    - dummy.templ
  tools: '$base/tools'


=back

=item split_at        ($offset)

Split a text (C<PCDATA> or C<CDATA>) element in 2 at C<$offset>, the original
element now holds the first part of the string and a new element holds the
right part. The new element is returned

If the element is not a text element then the first text child of the element
is split

=item split        ( $optional_regexp, $tag1, $atts1, $tag2, $atts2...)

Split the text descendants of an element in place, the text is split using 
the C<$regexp>, if the regexp includes () then the matched separators will be 
wrapped in elements.  C<$1> is wrapped in $tag1, with attributes C<$atts1> if
C<$atts1> is given (as a hashref), C<$2> is wrapped in $tag2... 

if $elt is C<< <p>tati tata <b>tutu tati titi</b> tata tati tata</p> >>

  $elt->split( qr/(ta)ti/, 'foo', {type => 'toto'} )

will change $elt to

  <p><foo type="toto">ta</foo> tata <b>tutu <foo type="toto">ta</foo>
      titi</b> tata <foo type="toto">ta</foo> tata</p> 

The regexp can be passed either as a string or as C<qr//> (perl 5.005 and 
later), it defaults to \s+ just as the C<split> built-in (but this would be 
quite a useless behaviour without the C<$optional_tag> parameter)

C<$optional_tag> defaults to PCDATA or CDATA, depending on the initial element
type

The list of descendants is returned (including un-touched original elements 
and newly created ones)

=item mark        ( $regexp, $optional_tag, $optional_attribute_ref)

This method behaves exactly as L<split>, except only the newly created 
elements are returned

=item wrap_children ( $regexp_string, $tag, $optional_attribute_hashref)

Wrap the children of the element that match the regexp in an element C<$tag>.
If $optional_attribute_hashref is passed then the new element will
have these attributes.

The $regexp_string includes tags, within pointy brackets, as in 
C<< <title><para>+ >> and the usual Perl modifiers (+*?...). 
Tags can be further qualified with attributes:
C<< <para type="warning" classif="cosmic_secret">+ >>. The values
for attributes should be xml-escaped: C<< <candy type="M&amp;Ms">* >>
(C<E<lt>>, C<&> B<C<E<gt>>> and C<"> should be escaped). 

Note that elements might get extra C<id> attributes in the process. See L<add_id>.
Use L<strip_att> to remove unwanted id's. 

Here is an example:

If the element C<$elt> has the following content:

  <elt>
   <p>para 1</p>
   <l_l1_1>list 1 item 1 para 1</l_l1_1>
     <l_l1>list 1 item 1 para 2</l_l1>
   <l_l1_n>list 1 item 2 para 1 (only para)</l_l1_n>
   <l_l1_n>list 1 item 3 para 1</l_l1_n>
     <l_l1>list 1 item 3 para 2</l_l1>
     <l_l1>list 1 item 3 para 3</l_l1>
   <l_l1_1>list 2 item 1 para 1</l_l1_1>
     <l_l1>list 2 item 1 para 2</l_l1>
   <l_l1_n>list 2 item 2 para 1 (only para)</l_l1_n>
   <l_l1_n>list 2 item 3 para 1</l_l1_n>
     <l_l1>list 2 item 3 para 2</l_l1>
     <l_l1>list 2 item 3 para 3</l_l1>
  </elt>

Then the code

  $elt->wrap_children( q{<l_l1_1><l_l1>*} , li => { type => "ul1" });
  $elt->wrap_children( q{<l_l1_n><l_l1>*} , li => { type => "ul" });

  $elt->wrap_children( q{<li type="ul1"><li type="ul">+}, "ul");
  $elt->strip_att( 'id');
  $elt->strip_att( 'type');
  $elt->print;

will output:

  <elt>
     <p>para 1</p>
     <ul>
       <li>
         <l_l1_1>list 1 item 1 para 1</l_l1_1>
         <l_l1>list 1 item 1 para 2</l_l1>
       </li>
       <li>
         <l_l1_n>list 1 item 2 para 1 (only para)</l_l1_n>
       </li>
       <li>
         <l_l1_n>list 1 item 3 para 1</l_l1_n>
         <l_l1>list 1 item 3 para 2</l_l1>
         <l_l1>list 1 item 3 para 3</l_l1>
       </li>
     </ul>
     <ul>
       <li>
         <l_l1_1>list 2 item 1 para 1</l_l1_1>
         <l_l1>list 2 item 1 para 2</l_l1>
       </li>
       <li>
         <l_l1_n>list 2 item 2 para 1 (only para)</l_l1_n>
       </li>
       <li>
         <l_l1_n>list 2 item 3 para 1</l_l1_n>
         <l_l1>list 2 item 3 para 2</l_l1>
         <l_l1>list 2 item 3 para 3</l_l1>
       </li>
     </ul>
  </elt>

=item subs_text ($regexp, $replace)

subs_text does text substitution, similar to perl's C< s///> operator.

C<$regexp> must be a perl regexp, created with the C<qr> operator.

C<$replace> can include C<$1, $2>... from the C<$regexp>. It can also be
used to create element and entities, by using 
C<< &elt( tag => { att => val }, text) >> (similar syntax as C<L<new>>) and
C<< &ent( name) >>.

Here is a rather complex example:

  $elt->subs_text( qr{(?<!do not )link to (http://([^\s,]*))},
                   'see &elt( a =>{ href => $1 }, $2)'
                 );

This will replace text like I<link to http://www.xmltwig.org> by 
I<< see <a href="www.xmltwig.org">www.xmltwig.org</a> >>, but not
I<do not link to...>

Generating entities (here replacing spaces with &nbsp;):

  $elt->subs_text( qr{ }, '&ent( "&nbsp;")');

or, using a variable:

  my $ent="&nbsp;";
  $elt->subs_text( qr{ }, "&ent( '$ent')");

Note that the substitution is always global, as in using the C<g> modifier
in a perl substitution, and that it is performed on all text descendants
of the element.

B<Bug>: in the C<$regexp>, you can only use C<\1>, C<\2>... if the replacement
expression does not include elements or attributes. eg

  $t->subs_text( qr/((t[aiou])\2)/, '$2');             # ok, replaces toto, tata, titi, tutu by to, ta, ti, tu
  $t->subs_text( qr/((t[aiou])\2)/, '&elt(p => $1)' ); # NOK, does not find toto...

=item add_id ($optional_coderef)

Add an id to the element.

The id is an attribute, C<id> by default, see the C<id> option for XML::Twig
C<new> to change it. Use an id starting with C<#> to get an id that's not 
output by L<print>, L<flush> or L<sprint>, yet that allows you to use the
L<elt_id> method to get the element easily.

If the element already has an id, no new id is generated.

By default the method create an id of the form C<< twig_id_<nnnn> >>,
where C<< <nnnn> >> is a number, incremented each time the method is called
successfully.

=item set_id_seed ($prefix)

by default the id generated by C<L<add_id>> is C<< twig_id_<nnnn> >>, 
C<set_id_seed> changes the prefix to C<$prefix> and resets the number
to 1

=item strip_att ($att)

Remove the attribute C<$att> from all descendants of the element (including 
the element)

Return the element

=item change_att_name ($old_name, $new_name)

Change the name of the attribute from C<$old_name> to C<$new_name>. If there is no
attribute C<$old_name> nothing happens.

=item lc_attnames

Lower cases the name all the attributes of the element.

=item sort_children_on_value( %options)

Sort the children of the element in place according to their text.
All children are sorted. 

Return the element, with its children sorted.


C<%options> are

  type  : numeric |  alpha     (default: alpha)
  order : normal  |  reverse   (default: normal)

Return the element, with its children sorted


=item sort_children_on_att ($att, %options)

Sort the children of the  element in place according to attribute C<$att>. 
C<%options> are the same as for C<sort_children_on_value>

Return the element.


=item sort_children_on_field ($tag, %options)

Sort the children of the element in place, according to the field C<$tag> (the 
text of the first child of the child with this tag). C<%options> are the same
as for C<sort_children_on_value>.

Return the element, with its children sorted


=item sort_children( $get_key, %options) 

Sort the children of the element in place. The C<$get_key> argument is
a reference to a function that returns the sort key when passed an element.

For example:

  $elt->sort_children( sub { $_[0]->{'att'}->{"nb"} + $_[0]->text }, 
                       type => 'numeric', order => 'reverse'
                     );

=item field_to_att ($cond, $att)

Turn the text of the first sub-element matched by C<$cond> into the value of 
attribute C<$att> of the element. If C<$att> is omitted then C<$cond> is used 
as the name of the attribute, which makes sense only if C<$cond> is a valid
element (and attribute) name.

The sub-element is then cut.

=item att_to_field ($att, $tag)

Take the value of attribute C<$att> and create a sub-element C<$tag> as first
child of the element. If C<$tag> is omitted then C<$att> is used as the name of
the sub-element. 


=item get_xpath  ($xpath, $optional_offset)

Return a list of elements satisfying the C<$xpath>. C<$xpath> is an XPATH-like 
expression.

A subset of the XPATH abbreviated syntax is covered:

  tag
  tag[1] (or any other positive number)
  tag[last()]
  tag[@att] (the attribute exists for the element)
  tag[@att="val"]
  tag[@att=~ /regexp/]
  tag[att1="val1" and att2="val2"]
  tag[att1="val1" or att2="val2"]
  tag[string()="toto"] (returns tag elements which text (as per the text method) 
                       is toto)
  tag[string()=~/regexp/] (returns tag elements which text (as per the text 
                          method) matches regexp)
  expressions can start with / (search starts at the document root)
  expressions can start with . (search starts at the current element)
  // can be used to get all descendants instead of just direct children
  * matches any tag

So the following examples from the 
F<XPath recommendationL<http://www.w3.org/TR/xpath.html#path-abbrev>> work:

  para selects the para element children of the context node
  * selects all element children of the context node
  para[1] selects the first para child of the context node
  para[last()] selects the last para child of the context node
  */para selects all para grandchildren of the context node
  /doc/chapter[5]/section[2] selects the second section of the fifth chapter 
     of the doc 
  chapter//para selects the para element descendants of the chapter element 
     children of the context node
  //para selects all the para descendants of the document root and thus selects
     all para elements in the same document as the context node
  //olist/item selects all the item elements in the same document as the 
     context node that have an olist parent
  .//para selects the para element descendants of the context node
  .. selects the parent of the context node
  para[@type="warning"] selects all para children of the context node that have
     a type attribute with value warning 
  employee[@secretary and @assistant] selects all the employee children of the
     context node that have both a secretary attribute and an assistant 
     attribute


The elements will be returned in the document order.

If C<$optional_offset> is used then only one element will be returned, the one 
with the appropriate offset in the list, starting at 0

Quoting and interpolating variables can be a pain when the Perl syntax and the 
XPATH syntax collide, so use alternate quoting mechanisms like q or qq 
(I like q{} and qq{} myself).

Here are some more examples to get you started:

  my $p1= "p1";
  my $p2= "p2";
  my @res= $t->get_xpath( qq{p[string( "$p1") or string( "$p2")]});

  my $a= "a1";
  my @res= $t->get_xpath( qq{//*[@att="$a"]});

  my $val= "a1";
  my $exp= qq{//p[ \@att='$val']}; # you need to use \@ or you will get a warning
  my @res= $t->get_xpath( $exp);

Note that the only supported regexps delimiters are / and that you must 
backslash all / in regexps AND in regular strings.

XML::Twig does not provide natively full XPATH support, but you can use 
C<L<XML::Twig::XPath>> to get C<findnodes> to use C<XML::XPath> as the
XPath engine, with full coverage of the spec.

C<L<XML::Twig::XPath>> to get C<findnodes> to use C<XML::XPath> as the
XPath engine, with full coverage of the spec.

=item find_nodes

same asC<get_xpath> 

=item findnodes

same as C<get_xpath> 


=item text @optional_options

Return a string consisting of all the C<PCDATA> and C<CDATA> in an element, 
without any tags. The text is not XML-escaped: base entities such as C<&> 
and C<< < >> are not escaped.

The 'C<no_recurse>' option will only return the text of the element, not
of any included sub-elements (same as C<L<text_only>>).

=item text_only

Same as C<L<text>> except that the text returned doesn't include 
the text of sub-elements.

=item trimmed_text

Same as C<text> except that the text is trimmed: leading and trailing spaces
are discarded, consecutive spaces are collapsed

=item set_text        ($string)

Set the text for the element: if the element is a C<PCDATA>, just set its
text, otherwise cut all the children of the element and create a single
C<PCDATA> child for it, which holds the text.

=item merge ($elt2)

Move the content of C<$elt2> within the element

=item insert         ($tag1, [$optional_atts1], $tag2, [$optional_atts2],...)

For each tag in the list inserts an element C<$tag> as the only child of the 
element.  The element gets the optional attributes inC<< $optional_atts<n>. >> 
All children of the element are set as children of the new element.
The upper level element is returned.

  $p->insert( table => { border=> 1}, 'tr', 'td') 

put C<$p> in a table with a visible border, a single C<tr> and a single C<td> 
and return the C<table> element:

  <p><table border="1"><tr><td>original content of p</td></tr></table></p>

=item wrap_in        (@tag)

Wrap elements in C<@tag> as the successive ancestors of the element, returns the 
new element.
C<< $elt->wrap_in( 'td', 'tr', 'table') >> wraps the element as a single cell in a 
table for example.

Optionally each tag can be followed by a hashref of attributes, that will be 
set on the wrapping element:

  $elt->wrap_in( p => { class => "advisory" }, div => { class => "intro", id => "div_intro" });

=item insert_new_elt ($opt_position, $tag, $opt_atts_hashref, @opt_content)

Combines a C<L<new> > and a C<L<paste> >: creates a new element using 
C<$tag>, C<$opt_atts_hashref >and C<@opt_content> which are arguments similar 
to those for C<new>, then paste it, using C<$opt_position> or C<'first_child'>,
relative to C<$elt>.

Return the newly created element

=item erase

Erase the element: the element is deleted and all of its children are
pasted in its place.

=item set_content    ( $optional_atts, @list_of_elt_and_strings)
                     ( $optional_atts, '#EMPTY')

Set the content for the element, from a list of strings and
elements.  Cuts all the element children, then pastes the list
elements as the children.  This method will create a C<PCDATA> element
for any strings in the list.

The C<$optional_atts> argument is the ref of a hash of attributes. If this
argument is used then the previous attributes are deleted, otherwise they
are left untouched. 

B<WARNING>: if you rely on ID's then you will have to set the id yourself. At
this point the element does not belong to a twig yet, so the ID attribute
is not known so it won't be stored in the ID list.

A content of 'C<#EMPTY>' creates an empty element;

=item namespace ($optional_prefix)

Return the URI of the namespace that C<$optional_prefix> or the element name
belongs to. If the name doesn't belong to any namespace, C<undef> is returned.

=item local_name

Return the local name (without the prefix) for the element

=item ns_prefix

Return the namespace prefix for the element

=item current_ns_prefixes

Return a list of namespace prefixes valid for the element. The order of the
prefixes in the list has no meaning. If the default namespace is currently 
bound, '' appears in the list.


=item inherit_att  ($att, @optional_tag_list)

Return the value of an attribute inherited from parent tags. The value
returned is found by looking for the attribute in the element then in turn
in each of its ancestors. If the C<@optional_tag_list> is supplied only those
ancestors whose tag is in the list will be checked. 

=item all_children_are ($optional_condition)

return 1 if all children of the element pass the C<$optional_condition>, 
0 otherwise

=item level       ($optional_condition)

Return the depth of the element in the twig (root is 0).
If C<$optional_condition> is given then only ancestors that match the condition are 
counted.

B<WARNING>: in a tree created using the C<twig_roots> option this will not return
the level in the document tree, level 0 will be the document root, level 1 
will be the C<twig_roots> elements. During the parsing (in a C<twig_handler>)
you can use the C<depth> method on the twig object to get the real parsing depth.

=item in           ($potential_parent)

Return true if the element is in the potential_parent (C<$potential_parent> is 
an element)

=item in_context   ($cond, $optional_level)

Return true if the element is included in an element which passes C<$cond>
optionally within C<$optional_level> levels. The returned value is the 
including element.

=item pcdata

Return the text of a C<PCDATA> element or C<undef> if the element is not 
C<PCDATA>.

=item pcdata_xml_string

Return the text of a C<PCDATA> element or undef if the element is not C<PCDATA>. 
The text is "XML-escaped" ('&' and '<' are replaced by '&amp;' and '&lt;')

=item set_pcdata     ($text)

Set the text of a C<PCDATA> element. This method does not check that the element is
indeed a C<PCDATA> so usually you should use C<L<set_text>> instead. 

=item append_pcdata  ($text)

Add the text at the end of a C<PCDATA> element.

=item is_cdata

Return 1 if the element is a C<CDATA> element, returns 0 otherwise.

=item is_text

Return 1 if the element is a C<CDATA> or C<PCDATA> element, returns 0 otherwise.

=item cdata

Return the text of a C<CDATA> element or C<undef> if the element is not 
C<CDATA>.

=item cdata_string

Return the XML string of a C<CDATA> element, including the opening and
closing markers.

=item set_cdata     ($text)

Set the text of a C<CDATA> element. 

=item append_cdata  ($text)

Add the text at the end of a C<CDATA> element.

=item remove_cdata

Turns all C<CDATA> sections in the element into regular C<PCDATA> elements. This is useful
when converting XML to HTML, as browsers do not support CDATA sections. 

=item extra_data 

Return the extra_data (comments and PI's) attached to an element

=item set_extra_data     ($extra_data)

Set the extra_data (comments and PI's) attached to an element

=item append_extra_data  ($extra_data)

Append extra_data to the existing extra_data before the element (if no
previous extra_data exists then it is created)

=item set_asis

Set a property of the element that causes it to be output without being XML
escaped by the print functions: if it contains C<< a < b >> it will be output
as such and not as C<< a &lt; b >>. This can be useful to create text elements
that will be output as markup. Note that all C<PCDATA> descendants of the 
element are also marked as having the property (they are the ones that are
actually impacted by the change).

If the element is a C<CDATA> element it will also be output asis, without the
C<CDATA> markers. The same goes for any C<CDATA> descendant of the element

=item set_not_asis

Unsets the C<asis> property for the element and its text descendants.

=item is_asis

Return the C<asis> property status of the element ( 1 or C<undef>)

=item closed                   

Return true if the element has been closed. Might be useful if you are
somewhere in the tree, during the parse, and have no idea whether a parent
element is completely loaded or not.

=item get_type

Return the type of the element: 'C<#ELT>' for "real" elements, or 'C<#PCDATA>',
'C<#CDATA>', 'C<#COMMENT>', 'C<#ENT>', 'C<#PI>'

=item is_elt

Return the tag if the element is a "real" element, or 0 if it is C<PCDATA>, 
C<CDATA>...

=item contains_only_text

Return 1 if the element does not contain any other "real" element

=item contains_only ($exp)

Return the list of children if all children of the element match
the expression C<$exp> 

  if( $para->contains_only( 'tt')) { ... }

=item contains_a_single ($exp)

If the element contains a single child that matches the expression C<$exp>
returns that element. Otherwise returns 0.

=item is_field

same as C<contains_only_text> 

=item is_pcdata

Return 1 if the element is a C<PCDATA> element, returns 0 otherwise.

=item is_ent

Return 1 if the element is an entity (an unexpanded entity) element, 
return 0 otherwise.

=item is_empty

Return 1 if the element is empty, 0 otherwise

=item set_empty

Flags the element as empty. No further check is made, so if the element
is actually not empty the output will be messed. The only effect of this 
method is that the output will be C<< <tag att="value""/> >>.

=item set_not_empty

Flags the element as not empty. if it is actually empty then the element will
be output as C<< <tag att="value""></tag> >>

=item is_pi

Return 1 if the element is a processing instruction (C<#PI>) element,
return 0 otherwise.

=item target

Return the target of a processing instruction

=item set_target ($target)

Set the target of a processing instruction

=item data

Return the data part of a processing instruction

=item set_data ($data)

Set the data of a processing instruction

=item set_pi ($target, $data)

Set the target and data of a processing instruction

=item pi_string

Return the string form of a processing instruction
(C<< <?target data?> >>)

=item is_comment

Return 1 if the element is a comment (C<#COMMENT>) element,
return 0 otherwise.

=item set_comment ($comment_text)

Set the text for a comment

=item comment

Return the content of a comment (just the text, not the C<< <!-- >>
and C<< --> >>)

=item comment_string 

Return the XML string for a comment (C<< <!-- comment --> >>)

Note that an XML comment cannot start or end with a '-', or include '--'
(http://www.w3.org/TR/2008/REC-xml-20081126/#sec-comments),
if that is the case (because you have created the comment yourself presumably,
as it could not be in the input XML), then a space will be inserted before
an initial '-', after a trailing one or between two '-' in the comment
(which could presumably mangle javascript "hidden" in an XHTML comment);

=item set_ent ($entity)

Set an (non-expanded) entity (C<#ENT>). C<$entity>) is the entity
text (C<&ent;>)

=item ent

Return the entity for an entity (C<#ENT>) element (C<&ent;>)

=item ent_name

Return the entity name for an entity (C<#ENT>) element (C<ent>)

=item ent_string

Return the entity, either expanded if the expanded version is available,
or non-expanded (C<&ent;>) otherwise

=item child ($offset, $optional_condition)

Return the C<$offset>-th child of the element, optionally the C<$offset>-th 
child that matches C<$optional_condition>. The children are treated as a list, so 
C<< $elt->child( 0) >> is the first child, while C<< $elt->child( -1) >> is 
the last child.

=item child_text ($offset, $optional_condition)

Return the text of a child or C<undef> if the sibling does not exist. Arguments
are the same as child.

=item last_child    ($optional_condition)

Return the last child of the element, or the last child matching 
C<$optional_condition> (ie the last of the element children matching
the condition).

=item last_child_text   ($optional_condition)

Same as C<first_child_text> but for the last child.

=item sibling  ($offset, $optional_condition)

Return the next or previous C<$offset>-th sibling of the element, or the 
C<$offset>-th one matching C<$optional_condition>. If C<$offset> is negative then a 
previous sibling is returned, if $offset is positive then  a next sibling is 
returned. C<$offset=0> returns the element if there is no condition or
if the element matches the condition>, C<undef> otherwise.

=item sibling_text ($offset, $optional_condition)

Return the text of a sibling or C<undef> if the sibling does not exist. 
Arguments are the same as C<sibling>.

=item prev_siblings ($optional_condition)

Return the list of previous siblings (optionally matching C<$optional_condition>)
for the element. The elements are ordered in document order.

=item next_siblings ($optional_condition)

Return the list of siblings (optionally matching C<$optional_condition>)
following the element. The elements are ordered in document order.

=item siblings ($optional_condition)

Return the list of siblings (optionally matching C<$optional_condition>)
of the element (excluding the element itself). The elements are ordered
in document order.

=item pos ($optional_condition)

Return the position of the element in the children list. The first child has a
position of 1 (as in XPath).

If the C<$optional_condition> is given then only siblings that match the condition 
are counted. If the element itself does not match the  condition then
0 is returned.

=item atts

Return a hash ref containing the element attributes

=item set_atts      ({ att1=>$att1_val, att2=> $att2_val... })

Set the element attributes with the hash ref supplied as the argument. The previous 
attributes are lost (ie the attributes set by C<set_atts> replace all of the
attributes of the element).

You can also pass a list instead of a hashref: C<< $elt->set_atts( att1 => 'val1',...) >>

=item del_atts

Deletes all the element attributes.

=item att_nb

Return the number of attributes for the element

=item has_atts

Return true if the element has attributes (in fact return the number of
attributes, thus being an alias to C<L<att_nb>>

=item has_no_atts

Return true if the element has no attributes, false (0) otherwise

=item att_names

return a list of the attribute names for the element

=item att_xml_string ($att, $options)

Return the attribute value, where '&', '<' and quote (" or the value of the quote option
at twig creation) are XML-escaped. 

The options are passed as a hashref, setting C<escape_gt> to a true value will also escape 
'>' ($elt( 'myatt', { escape_gt => 1 });

=item set_id       ($id)

Set the C<id> attribute of the element to the value.
See C<L<elt_id> > to change the id attribute name

=item id

Gets the id attribute value

=item del_id       ($id)

Deletes the C<id> attribute of the element and remove it from the id list
for the document

=item class

Return the C<class> attribute for the element (methods on the C<class>
attribute are quite convenient when dealing with XHTML, or plain XML that
will eventually be displayed using CSS)

=item lclass

same as class, except that
this method is an lvalue, so you can do C<< $elt->lclass= "foo" >>

=item set_class ($class)

Set the C<class> attribute for the element to C<$class>

=item add_class ($class)

Add C<$class> to the element C<class> attribute: the new class is added
only if it is not already present.

Note that classes are then sorted alphabetically, so the C<class> attribute
can be changed even if the class is already there

=item remove_class ($class)

Remove C<$class> from the element C<class> attribute. 

Note that classes are then sorted alphabetically, so the C<class> attribute can be
changed even if the class is already there


=item add_to_class ($class)

alias for add_class

=item att_to_class ($att)

Set the C<class> attribute to the value of attribute C<$att>

=item add_att_to_class ($att)

Add the value of attribute C<$att> to the C<class> attribute of the element

=item move_att_to_class ($att)

Add the value of attribute C<$att> to the C<class> attribute of the element
and delete the attribute

=item tag_to_class

Set the C<class> attribute of the element to the element tag

=item add_tag_to_class

Add the element tag to its C<class> attribute

=item set_tag_class ($new_tag)

Add the element tag to its C<class> attribute and sets the tag to C<$new_tag>

=item in_class ($class)

Return true (C<1>) if the element is in the class C<$class> (if C<$class> is
one of the tokens in the element C<class> attribute)

=item tag_to_span

Change the element tag tp C<span> and set its class to the old tag

=item tag_to_div

Change the element tag tp C<div> and set its class to the old tag

=item DESTROY

Frees the element from memory.

=item start_tag

Return the string for the start tag for the element, including 
the C<< /> >> at the end of an empty element tag

=item end_tag

Return the string for the end tag of an element.  For an empty
element, this returns the empty string ('').

=item xml_string @optional_options

Equivalent to C<< $elt->sprint( 1) >>, returns the string for the entire 
element, excluding the element's tags (but nested element tags are present)

The 'C<no_recurse>' option will only return the text of the element, not
of any included sub-elements (same as C<L<xml_text_only>>).

=item inner_xml

Another synonym for xml_string

=item outer_xml

An other synonym for sprint

=item xml_text 

Return the text of the element, encoded (and processed by the current 
C<L<output_filter>> or C<L<output_encoding>> options, without any tag.

=item xml_text_only

Same as C<L<xml_text>> except that the text returned doesn't include 
the text of sub-elements.

=item set_pretty_print ($style)

Set the pretty print method, amongst 'C<none>' (default), 'C<nsgmls>', 
'C<nice>', 'C<indented>', 'C<record>' and 'C<record_c>'

pretty_print styles:

=over 4

=item none

the default, no C<\n> is used

=item nsgmls

nsgmls style, with C<\n> added within tags

=item nice

adds C<\n> wherever possible (NOT SAFE, can lead to invalid XML)

=item indented

same as C<nice> plus indents elements (NOT SAFE, can lead to invalid XML) 

=item record

table-oriented pretty print, one field per line 

=item record_c

table-oriented pretty print, more compact than C<record>, one record per line 

=back

=item set_empty_tag_style ($style)

Set the method to output empty tags, amongst 'C<normal>' (default), 'C<html>',
and 'C<expand>', 

C<normal> outputs an empty tag 'C<< <tag/> >>', C<html> adds a space 
'C<< <tag /> >>' for elements that can be empty in XHTML and C<expand> outputs
'C<< <tag></tag> >>'

=item set_remove_cdata  ($flag)

set (or unset) the flag that forces the twig to output CDATA sections as 
regular (escaped) PCDATA


=item set_indent ($string)

Set the indentation for the indented pretty print style (default is 2 spaces)

=item set_quote ($quote)

Set the quotes used for attributes. can be 'C<double>' (default) or 'C<single>'

=item cmp       ($elt)

  Compare the order of the 2 elements in a twig.

  C<$a> is the <A>..</A> element, C<$b> is the <B>...</B> element

  document                        $a->cmp( $b)
  <A> ... </A> ... <B>  ... </B>     -1
  <A> ... <B>  ... </B> ... </A>     -1
  <B> ... </B> ... <A>  ... </A>      1
  <B> ... <A>  ... </A> ... </B>      1
   $a == $b                           0
   $a and $b not in the same tree   undef

=item before       ($elt)

Return 1 if C<$elt> starts before the element, 0 otherwise. If the 2 elements 
are not in the same twig then return C<undef>.

    if( $a->cmp( $b) == -1) { return 1; } else { return 0; }

=item after       ($elt)

Return 1 if $elt starts after the element, 0 otherwise. If the 2 elements 
are not in the same twig then return C<undef>.

    if( $a->cmp( $b) == -1) { return 1; } else { return 0; }

=item other comparison methods

=over 4

=item lt

=item le

=item gt

=item ge

=back

=item path

Return the element context in a form similar to XPath's short
form: 'C</root/tag1/../tag>'

=item xpath

Return a unique XPath expression that can be used to find the element
again. 

It looks like C</doc/sect[3]/title>: unique elements do not have an index,
the others do.

=item flush

flushes the twig up to the current element (strictly equivalent to 
C<< $elt->root->flush >>)

=item private methods

Low-level methods on the twig:

=over 4

=item set_parent        ($parent)

=item set_first_child   ($first_child)

=item set_last_child    ($last_child)

=item set_prev_sibling  ($prev_sibling)

=item set_next_sibling  ($next_sibling)

=item set_twig_current

=item del_twig_current

=item twig_current

=item contains_text

=back

Those methods should not be used, unless of course you find some creative 
and interesting, not to mention useful, ways to do it.

=back

=head2 cond

Most of the navigation functions accept a condition as an optional argument
The first element (or all elements for C<L<children> > or 
C<L<ancestors> >) that passes the condition is returned.

The condition is a single step of an XPath expression using the XPath subset
defined by C<L<get_xpath>>. Additional conditions are:

The condition can be 

=over 4

=item #ELT

return a "real" element (not a PCDATA, CDATA, comment or pi element) 

=item #TEXT

return a PCDATA or CDATA element

=item regular expression

return an element whose tag matches the regexp. The regexp has to be created 
with C<qr//> (hence this is available only on perl 5.005 and above)

=item code reference

applies the code, passing the current element as argument, if the code returns
true then the element is returned, if it returns false then the code is applied
to the next candidate.

=back

=head2 XML::Twig::XPath

XML::Twig implements a subset of XPath through the C<L<get_xpath>> method. 

If you want to use the whole XPath power, then you can use C<XML::Twig::XPath>
instead. In this case C<XML::Twig> uses C<XML::XPath> to execute XPath queries.
You will of course need C<XML::XPath> installed to be able to use C<XML::Twig::XPath>.

See L<XML::XPath> for more information.

The methods you can use are:

=over 4

=item findnodes              ($path)

return a list of nodes found by C<$path>.

=item findnodes_as_string    ($path)

return the nodes found reproduced as XML. The result is not guaranteed
to be valid XML though.

=item findvalue              ($path)

return the concatenation of the text content of the result nodes

=back

In order for C<XML::XPath> to be used as the XPath engine the following methods
are included in C<XML::Twig>:

in XML::Twig

=over 4

=item getRootNode

=item getParentNode

=item getChildNodes 

=back

in XML::Twig::Elt

=over 4

=item string_value

=item toString

=item getName

=item getRootNode

=item getNextSibling

=item getPreviousSibling

=item isElementNode

=item isTextNode

=item isPI

=item isPINode

=item isProcessingInstructionNode

=item isComment

=item isCommentNode

=item getTarget 

=item getChildNodes 

=item getElementById

=back

=head2 XML::Twig::XPath::Elt

The methods you can use are the same as on C<XML::Twig::XPath> elements:

=over 4

=item findnodes              ($path)

return a list of nodes found by C<$path>.

=item findnodes_as_string    ($path)

return the nodes found reproduced as XML. The result is not guaranteed
to be valid XML though.

=item findvalue              ($path)

return the concatenation of the text content of the result nodes

=back


=head2 XML::Twig::Entity_list

=over 4

=item new

Create an entity list.

=item add         ($ent)

Add an entity to an entity list.

=item add_new_ent ($name, $val, $sysid, $pubid, $ndata, $param)

Create a new entity and add it to the entity list

=item delete     ($ent or $tag).

Delete an entity (defined by its name or by the Entity object)
from the list.

=item print      ($optional_filehandle)

Print the entity list.

=item list

Return the list as an array

=back


=head2 XML::Twig::Entity

=over 4

=item new        ($name, $val, $sysid, $pubid, $ndata, $param)

Same arguments as the Entity handler for XML::Parser.

=item print       ($optional_filehandle)

Print an entity declaration.

=item name 

Return the name of the entity

=item val  

Return the value of the entity

=item sysid

Return the system id for the entity (for NDATA entities)

=item pubid

Return the public id for the entity (for NDATA entities)

=item ndata

Return true if the entity is an NDATA entity

=item param

Return true if the entity is a parameter entity


=item text

Return the entity declaration text.

=back

=head2 XML::Twig::Notation_list

=over 4

=item new

Create an notation list.

=item add         ($notation)

Add an notation to an notation list.

=item add_new_notation ($name, $base, $sysid, $pubid)

Create a new notation and add it to the notation list

=item delete     ($notation or $tag).

Delete an notation (defined by its name or by the Notation object)
from the list.

=item print      ($optional_filehandle)

Print the notation list.

=item list

Return the list as an array

=back


=head2 XML::Twig::Notation

=over 4

=item new        ($name, $base, $sysid, $pubid)

Same argumnotations as the Notation handler for XML::Parser.

=item print       ($optional_filehandle)

Print an notation declaration.

=item name 

Return the name of the notation

=item base 

Return the base to be used for resolving a relative URI

=item sysid

Return the system id for the notation

=item pubid

Return the public id for the notation


=item text

Return the notation declaration text.

=back
 

=head1 EXAMPLES

Additional examples (and a complete tutorial) can be found  on the
F<XML::Twig PageL<http://www.xmltwig.org/xmltwig/>>

To figure out what flush does call the following script with an
XML file and an element name as arguments

  use XML::Twig;

  my ($file, $elt)= @ARGV;
  my $t= XML::Twig->new( twig_handlers => 
      { $elt => sub {$_[0]->flush; print "\n[flushed here]\n";} });
  $t->parsefile( $file, ErrorContext => 2);
  $t->flush;
  print "\n";


=head1 NOTES

=head2 Subclassing XML::Twig

Useful methods:

=over 4

=item elt_class

In order to subclass C<XML::Twig> you will probably need to subclass also
C<L<XML::Twig::Elt>>. Use the C<elt_class> option when you create the
C<XML::Twig> object to get the elements created in a different class
(which should be a subclass of C<XML::Twig::Elt>.

=item add_options

If you inherit C<XML::Twig> new method but want to add more options to it
you can use this method to prevent XML::Twig to issue warnings for those
additional options.

=back

=head2 DTD Handling

There are 3 possibilities here.  They are:

=over 4

=item No DTD

No doctype, no DTD information, no entity information, the world is simple...

=item Internal DTD

The XML document includes an internal DTD, and maybe entity declarations.

If you use the load_DTD option when creating the twig the DTD information and
the entity declarations can be accessed.

The DTD and the entity declarations will be C<flush>'ed (or C<print>'ed) either
as is (if they have not been modified) or as reconstructed (poorly, comments 
are lost, order is not kept, due to it's content this DTD should not be viewed 
by anyone) if they have been modified. You can also modify them directly by 
changing the C<< $twig->{twig_doctype}->{internal} >> field (straight from 
XML::Parser, see the C<Doctype> handler doc)

=item External DTD

The XML document includes a reference to an external DTD, and maybe entity 
declarations.

If you use the C<load_DTD> when creating the twig the DTD information and the 
entity declarations can be accessed. The entity declarations will be
C<flush>'ed (or C<print>'ed) either as is (if they have not been modified) or
as reconstructed (badly, comments are lost, order is not kept).

You can change the doctype through the C<< $twig->set_doctype >> method and 
print the dtd through the C<< $twig->dtd_text >> or C<< $twig->dtd_print >>
 methods.

If you need to modify the entity list this is probably the easiest way to do it.

=back


=head2 Flush

Remember that element handlers are called when the element is CLOSED, so
if you have handlers for nested elements the inner handlers will be called
first. It makes it for example trickier than it would seem to number nested
sections (or clauses, or divs), as the titles in the inner sections are handled
before the outer sections.


=head1 BUGS

=over 4

=item segfault during parsing

This happens when parsing huge documents, or lots of small ones, with a version
of Perl before 5.16. 

This is due to a bug in the way weak references are handled in Perl itself.

The fix is either to upgrade to Perl 5.16 or later (C<perlbrew> is a great
tool to manage several installations of perl on the same machine).

An other, NOT RECOMMENDED, way of fixing the problem, is to switch off weak
references by writing C<XML::Twig::_set_weakrefs( 0);> at the top of the code. 
This is totally unsupported, and may lead to other problems though, 

=item entity handling

Due to XML::Parser behaviour, non-base entities in attribute values disappear if
they are not declared in the document:
C<att="val&ent;"> will be turned into C<< att => val >>, unless you use the 
C<keep_encoding> argument to C<< XML::Twig->new >> 

=item DTD handling

The DTD handling methods are quite bugged. No one uses them and
it seems very difficult to get them to work in all cases, including with 
several slightly incompatible versions of XML::Parser and of libexpat.

Basically you can read the DTD, output it back properly, and update entities,
but not much more.

So use XML::Twig with standalone documents, or with documents referring to an
external DTD, but don't expect it to properly parse and even output back the
DTD.

=item memory leak

If you use a REALLY old Perl (5.005!) and 
a lot of twigs you might find that you leak quite a lot of memory
(about 2Ks per twig). You can use the C<L<dispose> > method to free 
that memory after you are done.

If you create elements the same thing might happen, use the C<L<delete>>
method to get rid of them.

Alternatively installing the C<Scalar::Util> (or C<WeakRef>) module on a version 
of Perl that supports it (>5.6.0) will get rid of the memory leaks automagically.

=item ID list

The ID list is NOT updated when elements are cut or deleted.

=item change_gi

This method will not function properly if you do:

     $twig->change_gi( $old1, $new);
     $twig->change_gi( $old2, $new);
     $twig->change_gi( $new, $even_newer);

=item sanity check on XML::Parser method calls

XML::Twig should really prevent calls to some XML::Parser methods, especially 
the C<setHandlers> method.

=item pretty printing

Pretty printing (at least using the 'C<indented>' style) is hard to get right! 
Only elements that belong to the document will be properly indented. Printing 
elements that do not belong to the twig makes it impossible for XML::Twig to 
figure out their depth, and thus their indentation level.

Also there is an unavoidable bug when using C<flush> and pretty printing for
elements with mixed content that start with an embedded element:

  <elt><b>b</b>toto<b>bold</b></elt>

  will be output as 

  <elt>
    <b>b</b>toto<b>bold</b></elt>

if you flush the twig when you find the C<< <b> >> element


=back

=head1 Globals

These are the things that can mess up calling code, especially if threaded.
They might also cause problem under mod_perl. 

=over 4

=item Exported constants

Whether you want them or not you get them! These are subroutines to use
as constant when creating or testing elements

  PCDATA  return '#PCDATA'
  CDATA   return '#CDATA'
  PI      return '#PI', I had the choice between PROC and PI :--(

=item Module scoped values: constants

these should cause no trouble:

  %base_ent= ( '>' => '&gt;',
               '<' => '&lt;',
               '&' => '&amp;',
               "'" => '&apos;',
               '"' => '&quot;',
             );
  CDATA_START   = "<![CDATA[";
  CDATA_END     = "]]>";
  PI_START      = "<?";
  PI_END        = "?>";
  COMMENT_START = "<!--";
  COMMENT_END   = "-->";

pretty print styles

  ( $NSGMLS, $NICE, $INDENTED, $INDENTED_C, $WRAPPED, $RECORD1, $RECORD2)= (1..7);

empty tag output style

  ( $HTML, $EXPAND)= (1..2);

=item Module scoped values: might be changed

Most of these deal with pretty printing, so the worst that can
happen is probably that XML output does not look right, but is
still valid and processed identically by XML processors.

C<$empty_tag_style> can mess up HTML bowsers though and changing C<$ID> 
would most likely create problems.

  $pretty=0;           # pretty print style
  $quote='"';          # quote for attributes
  $INDENT= '  ';       # indent for indented pretty print
  $empty_tag_style= 0; # how to display empty tags
  $ID                  # attribute used as an id ('id' by default)

=item Module scoped values: definitely changed

These 2 variables are used to replace tags by an index, thus 
saving some space when creating a twig. If they really cause
you too much trouble, let me know, it is probably possible to
create either a switch or at least a version of XML::Twig that 
does not perform this optimization.

  %gi2index;     # tag => index
  @index2gi;     # list of tags

=back

If you need to manipulate all those values, you can use the following methods on the
XML::Twig object:

=over 4

=item global_state

Return a hashref with all the global variables used by XML::Twig

The hash has the following fields:  C<pretty>, C<quote>, C<indent>, 
C<empty_tag_style>, C<keep_encoding>, C<expand_external_entities>, 
C<output_filter>, C<output_text_filter>, C<keep_atts_order>

=item set_global_state ($state)

Set the global state, C<$state> is a hashref

=item save_global_state

Save the current global state

=item restore_global_state

Restore the previously saved (using C<Lsave_global_state>> state

=back

=head1 TODO 

=over 4

=item SAX handlers

Allowing XML::Twig to work on top of any SAX parser

=item multiple twigs are not well supported

A number of twig features are just global at the moment. These include
the ID list and the "tag pool" (if you use C<change_gi> then you change the tag 
for ALL twigs).

A future version will try to support this while trying not to be to
hard on performance (at least when a single twig is used!).

=back

=head1 AUTHOR

Michel Rodriguez <mirod@cpan.org>

=head1 LICENSE

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

Bug reports should be sent using:
F<RT L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=XML-Twig>>

Comments can be sent to mirod@cpan.org

The XML::Twig page is at L<http://www.xmltwig.org/xmltwig/>
It includes the development version of the module, a slightly better version 
of the documentation, examples, a tutorial and a: 
F<Processing XML efficiently with Perl and XML::Twig: 
L<http://www.xmltwig.org/xmltwig/tutorial/index.html>>

=head1 SEE ALSO

Complete docs, including a tutorial, examples, an easier to use HTML version of
the docs, a quick reference card and a FAQ are available at 
L<http://www.xmltwig.org/xmltwig/>

git repository at L<http://github.com/mirod/xmltwig>

L<XML::Parser>, L<XML::Parser::Expat>, L<XML::XPath>, L<Encode>, 
L<Text::Iconv>, L<Scalar::Utils>


=head2 Alternative Modules

XML::Twig is not the only XML::Processing module available on CPAN (far from 
it!).

The main alternative I would recommend is L<XML::LibXML>. 

Here is a quick comparison of the 2 modules:

XML::LibXML, actually C<libxml2> on which it is based, sticks to the standards,
and implements a good number of them in a rather strict way: XML, XPath, DOM, 
RelaxNG, I must be forgetting a couple (XInclude?). It is fast and rather 
frugal memory-wise.

XML::Twig is older: when I started writing it XML::Parser/expat was the only 
game in town. It implements XML and that's about it (plus a subset of XPath, 
and you can use XML::Twig::XPath if you have XML::XPathEngine installed for full 
support). It is slower and requires more memory for a full tree than 
XML::LibXML. On the plus side (yes, there is a plus side!) it lets you process
a big document in chunks, and thus let you tackle documents that couldn't be 
loaded in memory by XML::LibXML, and it offers a lot (and I mean a LOT!) of 
higher-level methods, for everything, from adding structure to "low-level" XML,
to shortcuts for XHTML conversions and more. It also DWIMs quite a bit, getting
comments and non-significant whitespaces out of the way but preserving them in 
the output for example. As it does not stick to the DOM, is also usually leads 
to shorter code than in XML::LibXML.

Beyond the pure features of the 2 modules, XML::LibXML seems to be preferred by
"XML-purists", while XML::Twig seems to be more used by Perl Hackers who have 
to deal with XML. As you have noted, XML::Twig also comes with quite a lot of 
docs, but I am sure if you ask for help about XML::LibXML here or on Perlmonks
you will get answers.

Note that it is actually quite hard for me to compare the 2 modules: on one hand
I know XML::Twig inside-out and I can get it to do pretty much anything I need 
to (or I improve it ;--), while I have a very basic knowledge of XML::LibXML. 
So feature-wise, I'd rather use XML::Twig ;--). On the other hand, I am 
painfully aware of some of the deficiencies, potential bugs and plain ugly code
that lurk in XML::Twig, even though you are unlikely to be affected by them 
(unless for example you need to change the DTD of a document programmatically),
while I haven't looked much into XML::LibXML so it still looks shinny and clean
to me.

That said, if you need to process a document that is too big to fit memory
and XML::Twig is too slow for you, my reluctant advice would be to use "bare"
XML::Parser.  It won't be as easy to use as XML::Twig: basically with XML::Twig
you trade some speed (depending on what you do from a factor 3 to... none) 
for ease-of-use, but it will be easier IMHO than using SAX (albeit not 
standard), and at this point a LOT faster (see the last test in
L<http://www.xmltwig.org/article/simple_benchmark/>).

=cut


