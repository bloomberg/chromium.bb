# $Id: /xmltwig/trunk/Twig/XPath.pm 32 2008-01-18T13:11:52.128782Z mrodrigu  $
package XML::Twig::XPath;
use strict;
use warnings;
use XML::Twig;

my $XPATH;        # XPath engine (XML::XPath or XML::XPathEngine);
my $XPATH_NUMBER; # <$XPATH>::Number, the XPath number class
BEGIN
  { foreach my $xpath_engine ( qw( XML::XPathEngine XML::XPath) )
      { if(  XML::Twig::_use( $xpath_engine) ) { $XPATH= $xpath_engine; last; } }
    unless( $XPATH) { die "cannot use XML::Twig::XPath: neither XML::XPathEngine 0.09+ nor XML::XPath are available"; }
    $XPATH_NUMBER= "${XPATH}::Number";
  }


use vars qw($VERSION);
$VERSION="0.02";

BEGIN
{ package # hide from PAUSE
    XML::XPath::NodeSet;
  no warnings; # to avoid the "Subroutine sort redefined" message
  # replace the native sort routine by a Twig'd one
  sub sort
    { my $self = CORE::shift;
      @$self = CORE::sort { $a->node_cmp( $b) } @$self;
      return $self;
    }

  package # hide from PAUSE
    XML::XPathEngine::NodeSet;
  no warnings; # to avoid the "Subroutine sort redefined" message
  # replace the native sort routine by a Twig'd one
  sub sort
    { my $self = CORE::shift;
      @$self = CORE::sort { $a->node_cmp( $b) } @$self;
      return $self;
    }
}

package XML::Twig::XPath;

use base 'XML::Twig';

my $XP; # the global xp object;

sub to_number { return $XPATH_NUMBER->new( $_[0]->root->text); }

sub new
  { my $class= shift;
    my $t= XML::Twig->new( elt_class => 'XML::Twig::XPath::Elt', @_);
    $t->{twig_xp}= $XPATH->new();
    bless $t, $class;
    return $t;
  }


sub set_namespace         { my $t= shift; $t->{twig_xp}->set_namespace( @_); }
sub set_strict_namespaces { my $t= shift; $t->{twig_xp}->set_strict_namespaces( @_); }

sub node_cmp($$)          { return $_[1] == $_[0] ? 0 : -1; } # document is before anything but itself

sub isElementNode   { 0 }
sub isAttributeNode { 0 }
sub isTextNode      { 0 }
sub isProcessingInstructionNode { 0 }
sub isPINode        { 0 }
sub isCommentNode   { 0 }
sub isNamespaceNode { 0 }
sub getAttributes   { [] }
sub getValue { return $_[0]->root->text; }

sub findnodes           { my( $t, $path)= @_; return $t->{twig_xp}->findnodes(           $path, $t); }
sub findnodes_as_string { my( $t, $path)= @_; return $t->{twig_xp}->findnodes_as_string( $path, $t); }
sub findvalue           { my( $t, $path)= @_; return $t->{twig_xp}->findvalue(           $path, $t); }
sub exists              { my( $t, $path)= @_; return $t->{twig_xp}->exists(              $path, $t); }
sub find                { my( $t, $path)= @_; return $t->{twig_xp}->find(                $path, $t); }
sub matches             { my( $t, $path, $node)= @_; $node ||= $t; return $t->{twig_xp}->matches( $node, $path, $t) || 0; }

sub getNamespaces { $_[0]->root->getNamespaces(); }

#TODO: it would be nice to be able to pass in any object in this
#distribution and cast it to the proper $XPATH class to use as a
#variable (via 'nodes' argument or something)
sub set_var {
  my ($t, $name, $value) = @_;
  if( ! ref $value) { $value= $t->findnodes( qq{"$value"}); } 
  $t->{twig_xp}->set_var($name, $value);
}

1;

# adds the appropriate methods to XML::Twig::Elt so XML::XPath can be used as the XPath engine
package XML::Twig::XPath::Elt;
use base 'XML::Twig::Elt';

*getLocalName= *XML::Twig::Elt::local_name;
*getValue    = *XML::Twig::Elt::text;
sub isAttributeNode { 0 }
sub isNamespaceNode { 0 }

sub to_number { return $XPATH_NUMBER->new( $_[0]->text); }

sub getAttributes
  { my $elt= shift;
    my $atts= $elt->atts;
    # alternate, faster but less clean, way
    my @atts= map { bless( { name => $_, value => $atts->{$_}, elt => $elt },
                           'XML::Twig::XPath::Attribute')
                  }
                   sort keys %$atts;
    # my @atts= map { XML::Twig::XPath::Attribute->new( $elt, $_) } sort keys %$atts;
    return wantarray ? @atts : \@atts;
  }

sub getNamespace
  { my $elt= shift;
    my $prefix= shift() || $elt->ns_prefix;
    if( my $expanded= $elt->namespace( $prefix))
      { return XML::Twig::XPath::Namespace->new( $prefix, $expanded); }
    else
      { return XML::Twig::XPath::Namespace->new( $prefix, ''); }
  }

# returns namespaces declared in the element
sub getNamespaces #_get_namespaces
  { my( $elt)= @_;
    my @namespaces;
    foreach my $att ($elt->att_names)
          { if( $att=~ m{^xmlns(?::(\w+))?$})
              { my $prefix= $1 || '';
                my $expanded= $elt->att( $att); 
                push @namespaces, XML::Twig::XPath::Namespace->new( $prefix, $expanded);
              }
          }
    return wantarray() ? @namespaces : \@namespaces;
  }

sub node_cmp($$)
  { my( $a, $b)= @_;
    if( UNIVERSAL::isa( $b, 'XML::Twig::XPath::Elt'))
      { # 2 elts, compare them
        return $a->cmp( $b);
      }
    elsif( UNIVERSAL::isa( $b, 'XML::Twig::XPath::Attribute'))
      { # elt <=> att, compare the elt to the att->{elt}
        # if the elt is the att->{elt} (cmp return 0) then -1, elt is before att
        return ($a->cmp( $b->{elt}) ) || -1 ;
      }
    elsif( UNIVERSAL::isa( $b, 'XML::Twig::XPath'))
      { # elt <=> document, elt is after document
        return 1;
      }
    else
      { die "unknown node type ", ref( $b); }
  }

sub getParentNode
  { return $_[0]->_parent
        || $_[0]->twig;
  }

sub findnodes           { my( $elt, $path)= @_; return $elt->twig->{twig_xp}->findnodes(           $path, $elt); }
sub findnodes_as_string { my( $elt, $path)= @_; return $elt->twig->{twig_xp}->findnodes_as_string( $path, $elt); }
sub findvalue           { my( $elt, $path)= @_; return $elt->twig->{twig_xp}->findvalue(           $path, $elt); }
sub exists              { my( $elt, $path)= @_; return $elt->twig->{twig_xp}->exists(              $path, $elt); }
sub find                { my( $elt, $path)= @_; return $elt->twig->{twig_xp}->find(                $path, $elt); }
sub matches             { my( $elt, $path)= @_; return $elt->twig->{twig_xp}->matches( $elt, $path, $elt->getParentNode) || 0; }


1;

# this package is only used to allow XML::XPath as the XPath engine, otherwise
# attributes are just attached to their parent element and are not considered objects

package XML::Twig::XPath::Attribute;

sub new
  { my( $class, $elt, $att)= @_;
    return bless { name => $att, value => $elt->att( $att), elt => $elt }, $class;
  }

sub getValue     { return $_[0]->{value}; }
sub getName      { return $_[0]->{name} ; }
sub getLocalName { (my $name= $_[0]->{name}) =~ s{^.*:}{}; $name; }
sub string_value { return $_[0]->{value}; }
sub to_number    { return $XPATH_NUMBER->new( $_[0]->{value}); }
sub isElementNode   { 0 }
sub isAttributeNode { 1 }
sub isNamespaceNode { 0 }
sub isTextNode      { 0 }
sub isProcessingInstructionNode { 0 }
sub isPINode        { 0 }
sub isCommentNode   { 0 }
sub toString { return qq{$_[0]->{name}="$_[0]->{value}"}; }

sub getNamespace
  { my $att= shift;
    my $prefix= shift();
    if( ! defined( $prefix))
      { if($att->{name}=~ m{^(.*):}) { $prefix= $1; }
        else                         { $prefix='';  }
      }

    if( my $expanded= $att->{elt}->namespace( $prefix))
      { return XML::Twig::XPath::Namespace->new( $prefix, $expanded); }
  }

sub node_cmp($$)
  { my( $a, $b)= @_;
    if( UNIVERSAL::isa( $b, 'XML::Twig::XPath::Attribute'))
      { # 2 attributes, compare their elements, then their name
        return ($a->{elt}->cmp( $b->{elt}) ) || ($a->{name} cmp $b->{name});
      }
    elsif( UNIVERSAL::isa( $b, 'XML::Twig::XPath::Elt'))
      { # att <=> elt : compare the att->elt and the elt
        # if att->elt is the elt (cmp returns 0) then 1 (elt is before att)
        return ($a->{elt}->cmp( $b) ) || 1 ;
      }
    elsif( UNIVERSAL::isa( $b, 'XML::Twig::XPath'))
      { # att <=> document, att is after document
        return 1;
      }
    else
      { die "unknown node type ", ref( $b); }
  }

*cmp=*node_cmp;

1;

package XML::Twig::XPath::Namespace;

sub new
  { my( $class, $prefix, $expanded)= @_;
    bless { prefix => $prefix, expanded => $expanded }, $class;
  }

sub isNamespaceNode { 1; }

sub getPrefix   { $_[0]->{prefix};   }
sub getExpanded { $_[0]->{expanded}; }
sub getValue    { $_[0]->{expanded}; }
sub getData     { $_[0]->{expanded}; }

sub node_cmp($$)
  { my( $a, $b)= @_;
    if( UNIVERSAL::isa( $b, 'XML::Twig::XPath::Namespace'))
      { # 2 attributes, compare their elements, then their name
        return $a->{prefix} cmp $b->{prefix};
      }
    else
      { die "unknown node type ", ref( $b); }
  }

*cmp=*node_cmp;

1

