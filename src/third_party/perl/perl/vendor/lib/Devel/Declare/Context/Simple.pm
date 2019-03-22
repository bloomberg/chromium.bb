package Devel::Declare::Context::Simple;

use strict;
use warnings;
use Devel::Declare ();
use B::Hooks::EndOfScope;
use Carp qw/confess/;

our $VERSION = '0.006011';

sub new {
  my $class = shift;
  bless {@_}, $class;
}

sub init {
  my $self = shift;
  @{$self}{ qw(Declarator Offset WarningOnRedefined) } = @_;
  return $self;
}

sub offset {
  my $self = shift;
  return $self->{Offset}
}

sub inc_offset {
  my $self = shift;
  $self->{Offset} += shift;
}

sub declarator {
  my $self = shift;
  return $self->{Declarator}
}

sub warning_on_redefine {
  my $self = shift;
  return $self->{WarningOnRedefined}
}

sub skip_declarator {
  my $self = shift;
  my $decl = $self->declarator;
  my $len = Devel::Declare::toke_scan_word($self->offset, 0);
  confess "Couldn't find declarator '$decl'"
    unless $len;

  my $linestr = $self->get_linestr;
  my $name = substr($linestr, $self->offset, $len);
  confess "Expected declarator '$decl', got '${name}'"
    unless $name eq $decl;

  $self->inc_offset($len);
}

sub skipspace {
  my $self = shift;
  $self->inc_offset(Devel::Declare::toke_skipspace($self->offset));
}

sub get_linestr {
  my $self = shift;
  my $line = Devel::Declare::get_linestr();
  return $line;
}

sub set_linestr {
  my $self = shift;
  my ($line) = @_;
  Devel::Declare::set_linestr($line);
}

sub strip_name {
  my $self = shift;
  $self->skipspace;
  if (my $len = Devel::Declare::toke_scan_word( $self->offset, 1 )) {
    my $linestr = $self->get_linestr();
    my $name = substr( $linestr, $self->offset, $len );
    substr( $linestr, $self->offset, $len ) = '';
    $self->set_linestr($linestr);
    return $name;
  }

  $self->skipspace;
  return;
}

sub strip_ident {
  my $self = shift;
  $self->skipspace;
  if (my $len = Devel::Declare::toke_scan_ident( $self->offset )) {
    my $linestr = $self->get_linestr();
    my $ident = substr( $linestr, $self->offset, $len );
    substr( $linestr, $self->offset, $len ) = '';
    $self->set_linestr($linestr);
    return $ident;
  }

  $self->skipspace;
  return;
}

sub strip_proto {
  my $self = shift;
  $self->skipspace;

  my $linestr = $self->get_linestr();
  if (substr($linestr, $self->offset, 1) eq '(') {
    my $length = Devel::Declare::toke_scan_str($self->offset);
    my $proto = Devel::Declare::get_lex_stuff();
    Devel::Declare::clear_lex_stuff();
    $linestr = $self->get_linestr();

    substr($linestr, $self->offset,
      defined($length) ? $length : length($linestr)) = '';
    $self->set_linestr($linestr);

    return $proto;
  }
  return;
}

sub strip_names_and_args {
  my $self = shift;
  $self->skipspace;

  my @args;

  my $linestr = $self->get_linestr;
  if (substr($linestr, $self->offset, 1) eq '(') {
    # We had a leading paren, so we will now expect comma separated
    # arguments
    substr($linestr, $self->offset, 1) = '';
    $self->set_linestr($linestr);
    $self->skipspace;

    # At this point we expect to have a comma-separated list of
    # barewords with optional protos afterward, so loop until we
    # run out of comma-separated values
    while (1) {
      # Get the bareword
      my $thing = $self->strip_name;
      # If there's no bareword here, bail
      confess "failed to parse bareword. found ${linestr}"
        unless defined $thing;

      $linestr = $self->get_linestr;
      if (substr($linestr, $self->offset, 1) eq '(') {
        # This one had a proto, pull it out
        push(@args, [ $thing, $self->strip_proto ]);
      } else {
        # This had no proto, so store it with an undef
        push(@args, [ $thing, undef ]);
      }
      $self->skipspace;
      $linestr = $self->get_linestr;

      if (substr($linestr, $self->offset, 1) eq ',') {
        # We found a comma, strip it out and set things up for
        # another iteration
        substr($linestr, $self->offset, 1) = '';
        $self->set_linestr($linestr);
        $self->skipspace;
      } else {
        # No comma, get outta here
        last;
      }
    }

    # look for the final closing paren of the list
    if (substr($linestr, $self->offset, 1) eq ')') {
      substr($linestr, $self->offset, 1) = '';
      $self->set_linestr($linestr);
      $self->skipspace;
    }
    else {
      # fail if it isn't there
      confess "couldn't find closing paren for argument. found ${linestr}"
    }
  } else {
    # No parens, so expect a single arg
    my $thing = $self->strip_name;
    # If there's no bareword here, bail
    confess "failed to parse bareword. found ${linestr}"
      unless defined $thing;
    $linestr = $self->get_linestr;
    if (substr($linestr, $self->offset, 1) eq '(') {
      # This one had a proto, pull it out
      push(@args, [ $thing, $self->strip_proto ]);
    } else {
      # This had no proto, so store it with an undef
      push(@args, [ $thing, undef ]);
    }
  }

  return \@args;
}

sub strip_attrs {
  my $self = shift;
  $self->skipspace;

  my $linestr = Devel::Declare::get_linestr;
  my $attrs   = '';

  if (substr($linestr, $self->offset, 1) eq ':') {
    while (substr($linestr, $self->offset, 1) ne '{') {
      if (substr($linestr, $self->offset, 1) eq ':') {
        substr($linestr, $self->offset, 1) = '';
        Devel::Declare::set_linestr($linestr);

        $attrs .= ':';
      }

      $self->skipspace;
      $linestr = Devel::Declare::get_linestr();

      if (my $len = Devel::Declare::toke_scan_word($self->offset, 0)) {
        my $name = substr($linestr, $self->offset, $len);
        substr($linestr, $self->offset, $len) = '';
        Devel::Declare::set_linestr($linestr);

        $attrs .= " ${name}";

        if (substr($linestr, $self->offset, 1) eq '(') {
          my $length = Devel::Declare::toke_scan_str($self->offset);
          my $arg    = Devel::Declare::get_lex_stuff();
          Devel::Declare::clear_lex_stuff();
          $linestr = Devel::Declare::get_linestr();
          substr($linestr, $self->offset, $length) = '';
          Devel::Declare::set_linestr($linestr);

          $attrs .= "(${arg})";
        }
      }
    }

    $linestr = Devel::Declare::get_linestr();
  }

  return $attrs;
}


sub get_curstash_name {
  return Devel::Declare::get_curstash_name;
}

sub shadow {
  my $self = shift;
  my $pack = $self->get_curstash_name;
  Devel::Declare::shadow_sub( $pack . '::' . $self->declarator, $_[0] );
}

sub inject_if_block {
  my $self   = shift;
  my $inject = shift;
  my $before = shift || '';

  $self->skipspace;

  my $linestr = $self->get_linestr;
  if (substr($linestr, $self->offset, 1) eq '{') {
    substr($linestr, $self->offset + 1, 0) = $inject;
    substr($linestr, $self->offset, 0) = $before;
    $self->set_linestr($linestr);
    return 1;
  }
  return 0;
}

sub scope_injector_call {
  my $self = shift;
  my $inject = shift || '';
  return ' BEGIN { ' . ref($self) . "->inject_scope('${inject}') }; ";
}

sub inject_scope {
  my $class = shift;
  my $inject = shift;
  on_scope_end {
      my $linestr = Devel::Declare::get_linestr;
      return unless defined $linestr;
      my $offset  = Devel::Declare::get_linestr_offset;
      substr( $linestr, $offset, 0 ) = ';' . $inject;
      Devel::Declare::set_linestr($linestr);
  };
}

1;
# vi:sw=2 ts=2
