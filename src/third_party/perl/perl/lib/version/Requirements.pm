use strict;
use warnings;
package Version::Requirements;
{
  $Version::Requirements::VERSION = '0.101022';
}
# ABSTRACT: a set of version requirements for a CPAN dist


use Carp ();
use Scalar::Util ();
use version 0.77 (); # the ->parse method

# We silence this warning during core tests, because this is only in core
# because it has to be, and nobody wants to see this stupid warning.
# -- rjbs, 2012-01-20
Carp::cluck(
  "Version::Requirements is deprecated; replace with CPAN::Meta::Requirements"
) unless $ENV{PERL_CORE};


sub new {
  my ($class) = @_;
  return bless {} => $class;
}

sub _version_object {
  my ($self, $version) = @_;

  $version = (! defined $version)                ? version->parse(0)
           : (! Scalar::Util::blessed($version)) ? version->parse($version)
           :                                       $version;

  return $version;
}


BEGIN {
  for my $type (qw(minimum maximum exclusion exact_version)) {
    my $method = "with_$type";
    my $to_add = $type eq 'exact_version' ? $type : "add_$type";

    my $code = sub {
      my ($self, $name, $version) = @_;

      $version = $self->_version_object( $version );

      $self->__modify_entry_for($name, $method, $version);

      return $self;
    };
    
    no strict 'refs';
    *$to_add = $code;
  }
}


sub add_requirements {
  my ($self, $req) = @_;

  for my $module ($req->required_modules) {
    my $modifiers = $req->__entry_for($module)->as_modifiers;
    for my $modifier (@$modifiers) {
      my ($method, @args) = @$modifier;
      $self->$method($module => @args);
    };
  }

  return $self;
}


sub accepts_module {
  my ($self, $module, $version) = @_;

  $version = $self->_version_object( $version );

  return 1 unless my $range = $self->__entry_for($module);
  return $range->_accepts($version);
}


sub clear_requirement {
  my ($self, $module) = @_;

  return $self unless $self->__entry_for($module);

  Carp::confess("can't clear requirements on finalized requirements")
    if $self->is_finalized;

  delete $self->{requirements}{ $module };

  return $self;
}


sub required_modules { keys %{ $_[0]{requirements} } }


sub clone {
  my ($self) = @_;
  my $new = (ref $self)->new;

  return $new->add_requirements($self);
}

sub __entry_for     { $_[0]{requirements}{ $_[1] } }

sub __modify_entry_for {
  my ($self, $name, $method, $version) = @_;

  my $fin = $self->is_finalized;
  my $old = $self->__entry_for($name);

  Carp::confess("can't add new requirements to finalized requirements")
    if $fin and not $old;

  my $new = ($old || 'Version::Requirements::_Range::Range')
          ->$method($version);

  Carp::confess("can't modify finalized requirements")
    if $fin and $old->as_string ne $new->as_string;

  $self->{requirements}{ $name } = $new;
}


sub is_simple {
  my ($self) = @_;
  for my $module ($self->required_modules) {
    # XXX: This is a complete hack, but also entirely correct.
    return if $self->__entry_for($module)->as_string =~ /\s/;
  }

  return 1;
}


sub is_finalized { $_[0]{finalized} }


sub finalize { $_[0]{finalized} = 1 }


sub as_string_hash {
  my ($self) = @_;

  my %hash = map {; $_ => $self->{requirements}{$_}->as_string }
             $self->required_modules;

  return \%hash;
}


my %methods_for_op = (
  '==' => [ qw(exact_version) ],
  '!=' => [ qw(add_exclusion) ],
  '>=' => [ qw(add_minimum)   ],
  '<=' => [ qw(add_maximum)   ],
  '>'  => [ qw(add_minimum add_exclusion) ],
  '<'  => [ qw(add_maximum add_exclusion) ],
);

sub from_string_hash {
  my ($class, $hash) = @_;

  my $self = $class->new;

  for my $module (keys %$hash) {
    my @parts = split qr{\s*,\s*}, $hash->{ $module };
    for my $part (@parts) {
      my ($op, $ver) = split /\s+/, $part, 2;

      if (! defined $ver) {
        $self->add_minimum($module => $op);
      } else {
        Carp::confess("illegal requirement string: $hash->{ $module }")
          unless my $methods = $methods_for_op{ $op };

        $self->$_($module => $ver) for @$methods;
      }
    }
  }

  return $self;
}

##############################################################

{
  package
    Version::Requirements::_Range::Exact;
  sub _new     { bless { version => $_[1] } => $_[0] }

  sub _accepts { return $_[0]{version} == $_[1] }

  sub as_string { return "== $_[0]{version}" }

  sub as_modifiers { return [ [ exact_version => $_[0]{version} ] ] }

  sub _clone {
    (ref $_[0])->_new( version->new( $_[0]{version} ) )
  }

  sub with_exact_version {
    my ($self, $version) = @_;

    return $self->_clone if $self->_accepts($version);

    Carp::confess("illegal requirements: unequal exact version specified");
  }

  sub with_minimum {
    my ($self, $minimum) = @_;
    return $self->_clone if $self->{version} >= $minimum;
    Carp::confess("illegal requirements: minimum above exact specification");
  }

  sub with_maximum {
    my ($self, $maximum) = @_;
    return $self->_clone if $self->{version} <= $maximum;
    Carp::confess("illegal requirements: maximum below exact specification");
  }

  sub with_exclusion {
    my ($self, $exclusion) = @_;
    return $self->_clone unless $exclusion == $self->{version};
    Carp::confess("illegal requirements: excluded exact specification");
  }
}

##############################################################

{
  package
    Version::Requirements::_Range::Range;

  sub _self { ref($_[0]) ? $_[0] : (bless { } => $_[0]) }

  sub _clone {
    return (bless { } => $_[0]) unless ref $_[0];

    my ($s) = @_;
    my %guts = (
      (exists $s->{minimum} ? (minimum => version->new($s->{minimum})) : ()),
      (exists $s->{maximum} ? (maximum => version->new($s->{maximum})) : ()),

      (exists $s->{exclusions}
        ? (exclusions => [ map { version->new($_) } @{ $s->{exclusions} } ])
        : ()),
    );

    bless \%guts => ref($s);
  }

  sub as_modifiers {
    my ($self) = @_;
    my @mods;
    push @mods, [ add_minimum => $self->{minimum} ] if exists $self->{minimum};
    push @mods, [ add_maximum => $self->{maximum} ] if exists $self->{maximum};
    push @mods, map {; [ add_exclusion => $_ ] } @{$self->{exclusions} || []};
    return \@mods;
  }

  sub as_string {
    my ($self) = @_;

    return 0 if ! keys %$self;

    return "$self->{minimum}" if (keys %$self) == 1 and exists $self->{minimum};

    my @exclusions = @{ $self->{exclusions} || [] };

    my @parts;

    for my $pair (
      [ qw( >= > minimum ) ],
      [ qw( <= < maximum ) ],
    ) {
      my ($op, $e_op, $k) = @$pair;
      if (exists $self->{$k}) {
        my @new_exclusions = grep { $_ != $self->{ $k } } @exclusions;
        if (@new_exclusions == @exclusions) {
          push @parts, "$op $self->{ $k }";
        } else {
          push @parts, "$e_op $self->{ $k }";
          @exclusions = @new_exclusions;
        }
      }
    }

    push @parts, map {; "!= $_" } @exclusions;

    return join q{, }, @parts;
  }

  sub with_exact_version {
    my ($self, $version) = @_;
    $self = $self->_clone;

    Carp::confess("illegal requirements: exact specification outside of range")
      unless $self->_accepts($version);

    return Version::Requirements::_Range::Exact->_new($version);
  }

  sub _simplify {
    my ($self) = @_;

    if (defined $self->{minimum} and defined $self->{maximum}) {
      if ($self->{minimum} == $self->{maximum}) {
        Carp::confess("illegal requirements: excluded all values")
          if grep { $_ == $self->{minimum} } @{ $self->{exclusions} || [] };

        return Version::Requirements::_Range::Exact->_new($self->{minimum})
      }

      Carp::confess("illegal requirements: minimum exceeds maximum")
        if $self->{minimum} > $self->{maximum};
    }

    # eliminate irrelevant exclusions
    if ($self->{exclusions}) {
      my %seen;
      @{ $self->{exclusions} } = grep {
        (! defined $self->{minimum} or $_ >= $self->{minimum})
        and
        (! defined $self->{maximum} or $_ <= $self->{maximum})
        and
        ! $seen{$_}++
      } @{ $self->{exclusions} };
    }

    return $self;
  }

  sub with_minimum {
    my ($self, $minimum) = @_;
    $self = $self->_clone;

    if (defined (my $old_min = $self->{minimum})) {
      $self->{minimum} = (sort { $b cmp $a } ($minimum, $old_min))[0];
    } else {
      $self->{minimum} = $minimum;
    }

    return $self->_simplify;
  }

  sub with_maximum {
    my ($self, $maximum) = @_;
    $self = $self->_clone;

    if (defined (my $old_max = $self->{maximum})) {
      $self->{maximum} = (sort { $a cmp $b } ($maximum, $old_max))[0];
    } else {
      $self->{maximum} = $maximum;
    }

    return $self->_simplify;
  }

  sub with_exclusion {
    my ($self, $exclusion) = @_;
    $self = $self->_clone;

    push @{ $self->{exclusions} ||= [] }, $exclusion;

    return $self->_simplify;
  }

  sub _accepts {
    my ($self, $version) = @_;

    return if defined $self->{minimum} and $version < $self->{minimum};
    return if defined $self->{maximum} and $version > $self->{maximum};
    return if defined $self->{exclusions}
          and grep { $version == $_ } @{ $self->{exclusions} };

    return 1;
  }
}

1;

__END__
=pod

=head1 NAME

Version::Requirements - a set of version requirements for a CPAN dist

=head1 VERSION

version 0.101022

=head1 SYNOPSIS

  use Version::Requirements;

  my $build_requires = Version::Requirements->new;

  $build_requires->add_minimum('Library::Foo' => 1.208);

  $build_requires->add_minimum('Library::Foo' => 2.602);

  $build_requires->add_minimum('Module::Bar'  => 'v1.2.3');

  $METAyml->{build_requires} = $build_requires->as_string_hash;

=head1 DESCRIPTION

A Version::Requirements object models a set of version constraints like those
specified in the F<META.yml> or F<META.json> files in CPAN distributions.  It
can be built up by adding more and more constraints, and it will reduce them to
the simplest representation.

Logically impossible constraints will be identified immediately by thrown
exceptions.

=head1 METHODS

=head2 new

  my $req = Version::Requirements->new;

This returns a new Version::Requirements object.  It ignores any arguments
given.

=head2 add_minimum

  $req->add_minimum( $module => $version );

This adds a new minimum version requirement.  If the new requirement is
redundant to the existing specification, this has no effect.

Minimum requirements are inclusive.  C<$version> is required, along with any
greater version number.

This method returns the requirements object.

=head2 add_maximum

  $req->add_maximum( $module => $version );

This adds a new maximum version requirement.  If the new requirement is
redundant to the existing specification, this has no effect.

Maximum requirements are inclusive.  No version strictly greater than the given
version is allowed.

This method returns the requirements object.

=head2 add_exclusion

  $req->add_exclusion( $module => $version );

This adds a new excluded version.  For example, you might use these three
method calls:

  $req->add_minimum( $module => '1.00' );
  $req->add_maximum( $module => '1.82' );

  $req->add_exclusion( $module => '1.75' );

Any version between 1.00 and 1.82 inclusive would be acceptable, except for
1.75.

This method returns the requirements object.

=head2 exact_version

  $req->exact_version( $module => $version );

This sets the version required for the given module to I<exactly> the given
version.  No other version would be considered acceptable.

This method returns the requirements object.

=head2 add_requirements

  $req->add_requirements( $another_req_object );

This method adds all the requirements in the given Version::Requirements object
to the requirements object on which it was called.  If there are any conflicts,
an exception is thrown.

This method returns the requirements object.

=head2 accepts_module

  my $bool = $req->accepts_modules($module => $version);

Given an module and version, this method returns true if the version
specification for the module accepts the provided version.  In other words,
given:

  Module => '>= 1.00, < 2.00'

We will accept 1.00 and 1.75 but not 0.50 or 2.00.

For modules that do not appear in the requirements, this method will return
true.

=head2 clear_requirement

  $req->clear_requirement( $module );

This removes the requirement for a given module from the object.

This method returns the requirements object.

=head2 required_modules

This method returns a list of all the modules for which requirements have been
specified.

=head2 clone

  $req->clone;

This method returns a clone of the invocant.  The clone and the original object
can then be changed independent of one another.

=head2 is_simple

This method returns true if and only if all requirements are inclusive minimums
-- that is, if their string expression is just the version number.

=head2 is_finalized

This method returns true if the requirements have been finalized by having the
C<finalize> method called on them.

=head2 finalize

This method marks the requirements finalized.  Subsequent attempts to change
the requirements will be fatal, I<if> they would result in a change.  If they
would not alter the requirements, they have no effect.

If a finalized set of requirements is cloned, the cloned requirements are not
also finalized.

=head2 as_string_hash

This returns a reference to a hash describing the requirements using the
strings in the F<META.yml> specification.

For example after the following program:

  my $req = Version::Requirements->new;

  $req->add_minimum('Version::Requirements' => 0.102);

  $req->add_minimum('Library::Foo' => 1.208);

  $req->add_maximum('Library::Foo' => 2.602);

  $req->add_minimum('Module::Bar'  => 'v1.2.3');

  $req->add_exclusion('Module::Bar'  => 'v1.2.8');

  $req->exact_version('Xyzzy'  => '6.01');

  my $hashref = $req->as_string_hash;

C<$hashref> would contain:

  {
    'Version::Requirements' => '0.102',
    'Library::Foo' => '>= 1.208, <= 2.206',
    'Module::Bar'  => '>= v1.2.3, != v1.2.8',
    'Xyzzy'        => '== 6.01',
  }

=head2 from_string_hash

  my $req = Version::Requirements->from_string_hash( \%hash );

This is an alternate constructor for a Version::Requirements object.  It takes
a hash of module names and version requirement strings and returns a new
Version::Requirements object.

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2010 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

