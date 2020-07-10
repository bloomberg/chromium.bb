package Specio::Library::Structured::Dict;

use strict;
use warnings;

our $VERSION = '0.43';

use Carp qw( confess );
use List::Util ();
use Scalar::Util qw( blessed );
use Specio::Library::Builtins;
use Specio::TypeChecks qw( does_role );

my $hashref = t('HashRef');

sub parent {$hashref}

## no critic (Subroutines::ProhibitUnusedPrivateSubroutines)
sub _inline {
    $hashref->inline_check( $_[1] );
}

sub _parameterization_args_builder {
    my $self = shift;
    my $args = shift;

    for my $p ( ( $args->{slurpy} || () ), values %{ $args->{kv} } ) {
        my $type;
        if ( blessed($p) ) {
            $type = $p;
        }
        else {
            if ( ref $p eq 'HASH' && $p->{optional} ) {
                $type = $p->{optional};
            }
            else {
                confess
                    'Can only pass types, optional types, and slurpy types when defining a Dict';
            }
        }

        does_role( $type, 'Specio::Constraint::Role::Interface' )
            or confess
            'All parameters passed to ->parameterize must be objects which do the Specio::Constraint::Role::Interface role';

        confess
            'All parameters passed to ->parameterize must be inlinable constraints'
            unless $type->can_be_inlined;
    }

    return %{$args};
}

sub _name_builder {
    my $self = shift;
    my $p    = shift;

    ## no critic (Subroutines::ProtectPrivateSubs)
    my @kv;
    for my $k ( sort keys %{ $p->{kv} } ) {
        my $v = $p->{kv}{$k};
        if ( blessed($v) ) {
            push @kv, "$k => " . $self->_name_or_anon($v);
        }
        elsif ( $v->{optional} ) {
            push @kv,
                "$k => " . $self->_name_or_anon( $v->{optional} ) . '?';
        }
    }

    if ( $p->{slurpy} ) {
        push @kv, $self->_name_or_anon( $p->{slurpy} ) . '...';
    }

    return 'Dict{ ' . ( join ', ', @kv ) . ' }';
}

sub _structured_inline_generator {
    my $self = shift;
    my $val  = shift;
    my %args = @_;

    my @code = sprintf( '( %s )', $hashref->_inline_check($val) );

    for my $k ( sort keys %{ $args{kv} } ) {
        my $p = $args{kv}{$k};
        my $access = sprintf( '%s->{%s}', $val, B::perlstring($k) );

        if ( !blessed($p) ) {
            my $type = $p->{optional};

            push @code,
                sprintf(
                '( exists %s ? ( %s ) : 1 )',
                $access, $type->_inline_check($access)
                );
        }
        else {
            push @code, sprintf( '( %s )', $p->_inline_check($access) );
        }
    }

    if ( $args{slurpy} ) {
        my $check
            = '( do { my %%_____known_____ = map { $_ => 1 } ( %s ); List::Util::all { %s } grep { ! $_____known_____{$_} } sort keys %%{ %s } } )';
        push @code,
            sprintf(
            $check,
            ( join ', ', map { B::perlstring($_) } keys %{ $args{kv} } ),
            $args{slurpy}->_inline_check( sprintf( '%s->{$_}', $val ) ),
            $val,
            );
    }

    return '( ' . ( join ' && ', @code ) . ' )';
}

1;

# ABSTRACT: Guts of Dict structured type

__END__

=pod

=encoding UTF-8

=head1 NAME

Specio::Library::Structured::Dict - Guts of Dict structured type

=head1 VERSION

version 0.43

=head1 DESCRIPTION

There are no user facing parts here.

=for Pod::Coverage .*

=head1 SUPPORT

Bugs may be submitted at L<https://github.com/houseabsolute/Specio/issues>.

I am also usually active on IRC as 'autarch' on C<irc://irc.perl.org>.

=head1 SOURCE

The source code repository for Specio can be found at L<https://github.com/houseabsolute/Specio>.

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2012 - 2018 by Dave Rolsky.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

The full text of the license can be found in the
F<LICENSE> file included with this distribution.

=cut
