package Package::Stash::PP;
{
  $Package::Stash::PP::VERSION = '0.33';
}
use strict;
use warnings;
# ABSTRACT: pure perl implementation of the Package::Stash API

use B;
use Carp qw(confess);
use Scalar::Util qw(blessed reftype weaken);
use Symbol;
# before 5.12, assigning to the ISA glob would make it lose its magical ->isa
# powers
use constant BROKEN_ISA_ASSIGNMENT => ($] < 5.012);
# before 5.10, stashes don't ever seem to drop to a refcount of zero, so
# weakening them isn't helpful
use constant BROKEN_WEAK_STASH     => ($] < 5.010);
# before 5.10, the scalar slot was always treated as existing if the
# glob existed
use constant BROKEN_SCALAR_INITIALIZATION => ($] < 5.010);


sub new {
    my $class = shift;
    my ($package) = @_;

    if (!defined($package) || (ref($package) && ref($package) ne 'HASH')) {
        confess "Package::Stash->new must be passed the name of the "
              . "package to access";
    }
    elsif (ref($package) eq 'HASH') {
        confess "The pure perl implementation of Package::Stash doesn't "
              . "currently support anonymous stashes. You should install "
              . "Package::Stash::XS";
    }
    elsif ($package !~ /\A[0-9A-Z_a-z]+(?:::[0-9A-Z_a-z]+)*\z/) {
        confess "$package is not a module name";
    }

    return bless {
        'package' => $package,
    }, $class;
}

sub name {
    confess "Can't call name as a class method"
        unless blessed($_[0]);
    return $_[0]->{package};
}

sub namespace {
    confess "Can't call namespace as a class method"
        unless blessed($_[0]);

    if (BROKEN_WEAK_STASH) {
        no strict 'refs';
        return \%{$_[0]->name . '::'};
    }
    else {
        return $_[0]->{namespace} if defined $_[0]->{namespace};

        {
            no strict 'refs';
            $_[0]->{namespace} = \%{$_[0]->name . '::'};
        }

        weaken($_[0]->{namespace});

        return $_[0]->{namespace};
    }
}

{
    my %SIGIL_MAP = (
        '$' => 'SCALAR',
        '@' => 'ARRAY',
        '%' => 'HASH',
        '&' => 'CODE',
        ''  => 'IO',
    );

    sub _deconstruct_variable_name {
        my ($self, $variable) = @_;

        my @ret;
        if (ref($variable) eq 'HASH') {
            @ret = @{$variable}{qw[name sigil type]};
        }
        else {
            (defined $variable && length $variable)
                || confess "You must pass a variable name";

            my $sigil = substr($variable, 0, 1, '');

            if (exists $SIGIL_MAP{$sigil}) {
                @ret = ($variable, $sigil, $SIGIL_MAP{$sigil});
            }
            else {
                @ret = ("${sigil}${variable}", '', $SIGIL_MAP{''});
            }
        }

        # XXX in pure perl, this will access things in inner packages,
        # in xs, this will segfault - probably look more into this at
        # some point
        ($ret[0] !~ /::/)
            || confess "Variable names may not contain ::";

        return @ret;
    }
}

sub _valid_for_type {
    my $self = shift;
    my ($value, $type) = @_;
    if ($type eq 'HASH' || $type eq 'ARRAY'
     || $type eq 'IO'   || $type eq 'CODE') {
        return reftype($value) eq $type;
    }
    else {
        my $ref = reftype($value);
        return !defined($ref) || $ref eq 'SCALAR' || $ref eq 'REF' || $ref eq 'LVALUE' || $ref eq 'REGEXP' || $ref eq 'VSTRING';
    }
}

sub add_symbol {
    my ($self, $variable, $initial_value, %opts) = @_;

    my ($name, $sigil, $type) = $self->_deconstruct_variable_name($variable);

    my $pkg = $self->name;

    if (@_ > 2) {
        $self->_valid_for_type($initial_value, $type)
            || confess "$initial_value is not of type $type";

        # cheap fail-fast check for PERLDBf_SUBLINE and '&'
        if ($^P and $^P & 0x10 && $sigil eq '&') {
            my $filename = $opts{filename};
            my $first_line_num = $opts{first_line_num};

            (undef, $filename, $first_line_num) = caller
                if not defined $filename;

            my $last_line_num = $opts{last_line_num} || ($first_line_num ||= 0);

            # http://perldoc.perl.org/perldebguts.html#Debugger-Internals
            $DB::sub{$pkg . '::' . $name} = "$filename:$first_line_num-$last_line_num";
        }
    }

    no strict 'refs';
    no warnings 'redefine', 'misc', 'prototype';
    *{$pkg . '::' . $name} = ref $initial_value ? $initial_value : \$initial_value;
}

sub remove_glob {
    my ($self, $name) = @_;
    delete $self->namespace->{$name};
}

sub has_symbol {
    my ($self, $variable) = @_;

    my ($name, $sigil, $type) = $self->_deconstruct_variable_name($variable);

    my $namespace = $self->namespace;

    return unless exists $namespace->{$name};

    my $entry_ref = \$namespace->{$name};
    if (reftype($entry_ref) eq 'GLOB') {
        if ($type eq 'SCALAR') {
            if (BROKEN_SCALAR_INITIALIZATION) {
                return defined ${ *{$entry_ref}{$type} };
            }
            else {
                return B::svref_2object($entry_ref)->SV->isa('B::SV');
            }
        }
        else {
            return defined *{$entry_ref}{$type};
        }
    }
    else {
        # a symbol table entry can be -1 (stub), string (stub with prototype),
        # or reference (constant)
        return $type eq 'CODE';
    }
}

sub get_symbol {
    my ($self, $variable, %opts) = @_;

    my ($name, $sigil, $type) = $self->_deconstruct_variable_name($variable);

    my $namespace = $self->namespace;

    if (!exists $namespace->{$name}) {
        if ($opts{vivify}) {
            if ($type eq 'ARRAY') {
                if (BROKEN_ISA_ASSIGNMENT) {
                    $self->add_symbol(
                        $variable,
                        $name eq 'ISA' ? () : ([])
                    );
                }
                else {
                    $self->add_symbol($variable, []);
                }
            }
            elsif ($type eq 'HASH') {
                $self->add_symbol($variable, {});
            }
            elsif ($type eq 'SCALAR') {
                $self->add_symbol($variable);
            }
            elsif ($type eq 'IO') {
                $self->add_symbol($variable, Symbol::geniosym);
            }
            elsif ($type eq 'CODE') {
                confess "Don't know how to vivify CODE variables";
            }
            else {
                confess "Unknown type $type in vivication";
            }
        }
        else {
            return undef;
        }
    }

    my $entry_ref = \$namespace->{$name};

    if (ref($entry_ref) eq 'GLOB') {
        return *{$entry_ref}{$type};
    }
    else {
        if ($type eq 'CODE') {
            no strict 'refs';
            return \&{ $self->name . '::' . $name };
        }
        else {
            return undef;
        }
    }
}

sub get_or_add_symbol {
    my $self = shift;
    $self->get_symbol(@_, vivify => 1);
}

sub remove_symbol {
    my ($self, $variable) = @_;

    my ($name, $sigil, $type) = $self->_deconstruct_variable_name($variable);

    # FIXME:
    # no doubt this is grossly inefficient and
    # could be done much easier and faster in XS

    my ($scalar_desc, $array_desc, $hash_desc, $code_desc, $io_desc) = (
        { sigil => '$', type => 'SCALAR', name => $name },
        { sigil => '@', type => 'ARRAY',  name => $name },
        { sigil => '%', type => 'HASH',   name => $name },
        { sigil => '&', type => 'CODE',   name => $name },
        { sigil => '',  type => 'IO',     name => $name },
    );

    my ($scalar, $array, $hash, $code, $io);
    if ($type eq 'SCALAR') {
        $array  = $self->get_symbol($array_desc)  if $self->has_symbol($array_desc);
        $hash   = $self->get_symbol($hash_desc)   if $self->has_symbol($hash_desc);
        $code   = $self->get_symbol($code_desc)   if $self->has_symbol($code_desc);
        $io     = $self->get_symbol($io_desc)     if $self->has_symbol($io_desc);
    }
    elsif ($type eq 'ARRAY') {
        $scalar = $self->get_symbol($scalar_desc) if $self->has_symbol($scalar_desc) || BROKEN_SCALAR_INITIALIZATION;
        $hash   = $self->get_symbol($hash_desc)   if $self->has_symbol($hash_desc);
        $code   = $self->get_symbol($code_desc)   if $self->has_symbol($code_desc);
        $io     = $self->get_symbol($io_desc)     if $self->has_symbol($io_desc);
    }
    elsif ($type eq 'HASH') {
        $scalar = $self->get_symbol($scalar_desc) if $self->has_symbol($scalar_desc) || BROKEN_SCALAR_INITIALIZATION;
        $array  = $self->get_symbol($array_desc)  if $self->has_symbol($array_desc);
        $code   = $self->get_symbol($code_desc)   if $self->has_symbol($code_desc);
        $io     = $self->get_symbol($io_desc)     if $self->has_symbol($io_desc);
    }
    elsif ($type eq 'CODE') {
        $scalar = $self->get_symbol($scalar_desc) if $self->has_symbol($scalar_desc) || BROKEN_SCALAR_INITIALIZATION;
        $array  = $self->get_symbol($array_desc)  if $self->has_symbol($array_desc);
        $hash   = $self->get_symbol($hash_desc)   if $self->has_symbol($hash_desc);
        $io     = $self->get_symbol($io_desc)     if $self->has_symbol($io_desc);
    }
    elsif ($type eq 'IO') {
        $scalar = $self->get_symbol($scalar_desc) if $self->has_symbol($scalar_desc) || BROKEN_SCALAR_INITIALIZATION;
        $array  = $self->get_symbol($array_desc)  if $self->has_symbol($array_desc);
        $hash   = $self->get_symbol($hash_desc)   if $self->has_symbol($hash_desc);
        $code   = $self->get_symbol($code_desc)   if $self->has_symbol($code_desc);
    }
    else {
        confess "This should never ever ever happen";
    }

    $self->remove_glob($name);

    $self->add_symbol($scalar_desc => $scalar) if defined $scalar;
    $self->add_symbol($array_desc  => $array)  if defined $array;
    $self->add_symbol($hash_desc   => $hash)   if defined $hash;
    $self->add_symbol($code_desc   => $code)   if defined $code;
    $self->add_symbol($io_desc     => $io)     if defined $io;
}

sub list_all_symbols {
    my ($self, $type_filter) = @_;

    my $namespace = $self->namespace;
    return keys %{$namespace} unless defined $type_filter;

    # NOTE:
    # or we can filter based on
    # type (SCALAR|ARRAY|HASH|CODE)
    if ($type_filter eq 'CODE') {
        return grep {
            # any non-typeglob in the symbol table is a constant or stub
            ref(\$namespace->{$_}) ne 'GLOB'
                # regular subs are stored in the CODE slot of the typeglob
                || defined(*{$namespace->{$_}}{CODE})
        } keys %{$namespace};
    }
    elsif ($type_filter eq 'SCALAR') {
        return grep {
            BROKEN_SCALAR_INITIALIZATION
                ? (ref(\$namespace->{$_}) eq 'GLOB'
                      && defined(${*{$namespace->{$_}}{'SCALAR'}}))
                : (do {
                      my $entry = \$namespace->{$_};
                      ref($entry) eq 'GLOB'
                          && B::svref_2object($entry)->SV->isa('B::SV')
                  })
        } keys %{$namespace};
    }
    else {
        return grep {
            ref(\$namespace->{$_}) eq 'GLOB'
                && defined(*{$namespace->{$_}}{$type_filter})
        } keys %{$namespace};
    }
}

sub get_all_symbols {
    my ($self, $type_filter) = @_;

    my $namespace = $self->namespace;
    return { %{$namespace} } unless defined $type_filter;

    return {
        map { $_ => $self->get_symbol({name => $_, type => $type_filter}) }
            $self->list_all_symbols($type_filter)
    }
}


1;

__END__
=pod

=head1 NAME

Package::Stash::PP - pure perl implementation of the Package::Stash API

=head1 VERSION

version 0.33

=head1 SYNOPSIS

  use Package::Stash;

=head1 DESCRIPTION

This is a backend for L<Package::Stash> implemented in pure perl, for those without a compiler or who would like to use this inline in scripts.

=head1 BUGS

=over 4

=item * remove_symbol also replaces the associated typeglob

This can cause unexpected behavior when doing manipulation at compile time -
removing subroutines will still allow them to be called from within the package
as subroutines (although they will not be available as methods). This can be
considered a feature in some cases (this is how L<namespace::clean> works, for
instance), but should not be relied upon - use C<remove_glob> directly if you
want this behavior.

=item * Some minor memory leaks

The pure perl implementation has a couple minor memory leaks (see the TODO
tests in t/20-leaks.t) that I'm having a hard time tracking down - these may be
core perl bugs, it's hard to tell.

=back

Please report any bugs through RT: email
C<bug-package-stash at rt.cpan.org>, or browse to
L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=Package-Stash>.

=head1 SUPPORT

You can find this documentation for this module with the perldoc command.

    perldoc Package::Stash

You can also look for information at:

=over 4

=item * AnnoCPAN: Annotated CPAN documentation

L<http://annocpan.org/dist/Package-Stash>

=item * CPAN Ratings

L<http://cpanratings.perl.org/d/Package-Stash>

=item * RT: CPAN's request tracker

L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=Package-Stash>

=item * Search CPAN

L<http://search.cpan.org/dist/Package-Stash>

=back

=head1 AUTHOR

Jesse Luehrs <doy at tozt dot net>

Mostly copied from code from L<Class::MOP::Package>, by Stevan Little and the
Moose Cabal.

=for Pod::Coverage BROKEN_ISA_ASSIGNMENT
add_symbol
get_all_symbols
get_or_add_symbol
get_symbol
has_symbol
list_all_symbols
name
namespace
new
remove_glob

=head1 SEE ALSO

=over 4

=item * L<Class::MOP::Package>

This module is a factoring out of code that used to live here

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by Jesse Luehrs.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

