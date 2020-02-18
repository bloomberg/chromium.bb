# $Id: Error.pm,v 1.1.2.1 2004/04/20 20:09:48 pajas Exp $
#
# This is free software, you may use it and distribute it under the same terms as
# Perl itself.
#
# Copyright 2001-2003 AxKit.com Ltd., 2002-2006 Christian Glahn, 2006-2009 Petr Pajas
#
#
package XML::LibXML::Error;

use strict;
use warnings;

# To avoid a "Deep recursion on subroutine as_string" warning
no warnings 'recursion';

use Encode ();

use vars qw(@error_domains $VERSION $WARNINGS);
use Carp;
use overload
  '""' => \&as_string,
  'eq' => sub {
    ("$_[0]" eq "$_[1]")
  },
  'cmp' => sub {
    ("$_[0]" cmp "$_[1]")
  },
  fallback => 1;

$WARNINGS = 0; # 0: suppress, 1: report via warn, 2: report via die
$VERSION = "2.0200"; # VERSION TEMPLATE: DO NOT CHANGE

use constant XML_ERR_NONE            => 0;
use constant XML_ERR_WARNING         => 1; # A simple warning
use constant XML_ERR_ERROR           => 2; # A recoverable error
use constant XML_ERR_FATAL           => 3; # A fatal error

use constant XML_ERR_FROM_NONE       => 0;
use constant XML_ERR_FROM_PARSER     => 1; # The XML parser
use constant XML_ERR_FROM_TREE       => 2; # The tree module
use constant XML_ERR_FROM_NAMESPACE  => 3; # The XML Namespace module
use constant XML_ERR_FROM_DTD        => 4; # The XML DTD validation
use constant XML_ERR_FROM_HTML       => 5; # The HTML parser
use constant XML_ERR_FROM_MEMORY     => 6; # The memory allocator
use constant XML_ERR_FROM_OUTPUT     => 7; # The serialization code
use constant XML_ERR_FROM_IO         => 8; # The Input/Output stack
use constant XML_ERR_FROM_FTP        => 9; # The FTP module
use constant XML_ERR_FROM_HTTP       => 10; # The FTP module
use constant XML_ERR_FROM_XINCLUDE   => 11; # The XInclude processing
use constant XML_ERR_FROM_XPATH      => 12; # The XPath module
use constant XML_ERR_FROM_XPOINTER   => 13; # The XPointer module
use constant XML_ERR_FROM_REGEXP     => 14;     # The regular expressions module
use constant XML_ERR_FROM_DATATYPE   => 15; # The W3C XML Schemas Datatype module
use constant XML_ERR_FROM_SCHEMASP   => 16; # The W3C XML Schemas parser module
use constant XML_ERR_FROM_SCHEMASV   => 17; # The W3C XML Schemas validation module
use constant XML_ERR_FROM_RELAXNGP   => 18; # The Relax-NG parser module
use constant XML_ERR_FROM_RELAXNGV   => 19; # The Relax-NG validator module
use constant XML_ERR_FROM_CATALOG    => 20; # The Catalog module
use constant XML_ERR_FROM_C14N       => 21; # The Canonicalization module
use constant XML_ERR_FROM_XSLT       => 22; # The XSLT engine from libxslt
use constant XML_ERR_FROM_VALID      => 23; # The DTD validation module with valid context
use constant XML_ERR_FROM_CHECK      => 24; # The error-checking module
use constant XML_ERR_FROM_WRITER     => 25; # The xmlwriter module
use constant XML_ERR_FROM_MODULE     => 26; # The dynamically-loaded module module
use constant XML_ERR_FROM_I18N       => 27; # The module handling character conversion
use constant XML_ERR_FROM_SCHEMATRONV=> 28; # The Schematron validator module

@error_domains = ("", "parser", "tree", "namespace", "validity",
                  "HTML parser", "memory", "output", "I/O", "ftp",
                  "http", "XInclude", "XPath", "xpointer", "regexp",
                  "Schemas datatype", "Schemas parser", "Schemas validity",
                  "Relax-NG parser", "Relax-NG validity",
                  "Catalog", "C14N", "XSLT", "validity", "error-checking",
                  "xmlwriter", "dynamic loading", "i18n",
                  "Schematron validity");

my $MAX_ERROR_PREV_DEPTH = 100;

for my $field (qw<code _prev level file line nodename message column context
                  str1 str2 str3 num1 num2 __prev_depth>) {
    my $method = sub { $_[0]{$field} };
    no strict 'refs';
    *$field = $method;
}

{

  sub new {
    my ($class,$xE) = @_;
    my $terr;
    if (ref($xE)) {
      my ($context,$column) = $xE->context_and_column();
      $terr =bless {
        domain  => $xE->domain(),
        level   => $xE->level(),
        code    => $xE->code(),
        message => $xE->message(),
        file    => $xE->file(),
        line    => $xE->line(),
        str1    => $xE->str1(),
        str2    => $xE->str2(),
        str3    => $xE->str3(),
        num1    => $xE->num1(),
        num2    => $xE->num2(),
        __prev_depth => 0,
        (defined($context) ?
           (
             context => $context,
             column => $column,
            ) : ()),
      }, $class;
    } else {
      # !!!! problem : got a flat error
      # warn("PROBLEM: GOT A FLAT ERROR $xE\n");
      $terr =bless {
        domain  => 0,
        level   => 2,
        code    => -1,
        message => $xE,
        file    => undef,
        line    => undef,
        str1    => undef,
        str2    => undef,
        str3    => undef,
        num1    => undef,
        num2    => undef,
        __prev_depth => 0,
      }, $class;
    }
    return $terr;
  }

    sub _callback_error {
      #print "CALLBACK\n";
      my ($xE,$prev) = @_;
      my $terr;
      $terr=XML::LibXML::Error->new($xE);
      if ($terr->{level} == XML_ERR_WARNING and $WARNINGS!=2) {
        warn $terr if $WARNINGS;
        return $prev;
      }
      #unless ( defined $terr->{file} and length $terr->{file} ) {
        # this would make it easier to recognize parsed strings
        # but it breaks old implementations
        # [CG] $terr->{file} = 'string()';
      #}
      #warn "Saving the error ",$terr->dump;

      if (ref($prev))
      {
          if ($prev->__prev_depth()  >= $MAX_ERROR_PREV_DEPTH)
          {
              return $prev;
          }
          $terr->{_prev} = $prev;
          $terr->{__prev_depth} = $prev->__prev_depth() + 1;
      }
      else
      {
          $terr->{_prev} = defined($prev) && length($prev) ? XML::LibXML::Error->new($prev) : undef;
      }
      return $terr;
    }
    sub _instant_error_callback {
      my $xE = shift;
      my $terr= XML::LibXML::Error->new($xE);
      print "Reporting an instanteous error ",$terr->dump;
      die $terr;
    }
    sub _report_warning {
      my ($saved_error) = @_;
      #print "CALLBACK WARN\n";
      if ( defined $saved_error ) {
        #print "reporting a warning ",$saved_error->dump;
        warn $saved_error;
      }
    }
    sub _report_error {
      my ($saved_error) = @_;
      #print "CALLBACK ERROR: $saved_error\n";
      if ( defined $saved_error ) {
        die $saved_error;
      }
    }
}


# backward compatibility
sub int1 { $_[0]->num1 }
sub int2 { $_[0]->num2 }

sub domain {
    my ($self)=@_;
    return undef unless ref($self);
    my $domain = $self->{domain};
    # Newer versions of libxml2 might yield errors in domains that aren't
    # listed above.  Invent something reasonable in that case.
    return $domain < @error_domains ? $error_domains[$domain] : "domain_$domain";
}

sub as_string {
    my ($self)=@_;
    my $msg = "";
    my $level;

    if (defined($self->{_prev})) {
        $msg = $self->{_prev}->as_string;
    }

    if ($self->{level} == XML_ERR_NONE) {
        $level = "";
    } elsif ($self->{level} == XML_ERR_WARNING) {
        $level = "warning";
    } elsif ($self->{level} == XML_ERR_ERROR ||
             $self->{level} == XML_ERR_FATAL) {
        $level = "error";
    }
    my $where="";
    if (defined($self->{file})) {
        $where="$self->{file}:$self->{line}";
    } elsif (($self->{domain} == XML_ERR_FROM_PARSER)
             and
             $self->{line})  {
        $where="Entity: line $self->{line}";
    }
    if ($self->{nodename}) {
        $where.=": element ".$self->{nodename};
    }
    $msg.=$where.": " if $where ne "";
    $msg.=$self->domain." ".$level." :";
    my $str=$self->{message}||"";
    chomp($str);
    $msg.=" ".$str."\n";
    if (($self->{domain} == XML_ERR_FROM_XPATH) and
          defined($self->{str1})) {
      $msg.=$self->{str1}."\n";
      $msg.=(" " x $self->{num1})."^\n";
    } elsif (defined $self->{context}) {
      # If the error relates to character-encoding problems in the context,
      # then doing textual operations on it will spew warnings that
      # XML::LibXML can do nothing to fix.  So just disable all such
      # warnings.  This has the pleasing benefit of making the test suite
      # run warning-free.
      no warnings 'utf8';
      my $context = Encode::encode('utf8', $self->{context}, Encode::FB_DEFAULT);
      $msg.=$context."\n";
      $context = substr($context,0,$self->{column});
      $context=~s/[^\t]/ /g;
      $msg.=$context."^\n";
    }
    return $msg;
}

sub dump {
  my ($self)=@_;
  use Data::Dumper;
  return Data::Dumper->new([$self],['error'])->Dump;
}

1;
