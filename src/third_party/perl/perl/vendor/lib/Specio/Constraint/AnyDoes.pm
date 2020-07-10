package Specio::Constraint::AnyDoes;

use strict;
use warnings;

our $VERSION = '0.43';

use B ();
use Role::Tiny::With;
use Scalar::Util ();
use Specio::Library::Builtins;
use Specio::OO;

use Specio::Constraint::Role::DoesType;
with 'Specio::Constraint::Role::DoesType';

{
    my $Defined = t('Defined');
    sub _build_parent {$Defined}
}

{
    my $_inline_generator = sub {
        my $self = shift;
        my $val  = shift;

        return sprintf( <<'EOF', ($val) x 8, B::perlstring( $self->role ) );
(
    (
        Scalar::Util::blessed(%s) || (
              !ref(%s)
            && defined(%s)
            && length(%s)
            && %s !~ /\A
                      \s*
                      -?[0-9]+(?:\.[0-9]+)?
                      (?:[Ee][\-+]?[0-9]+)?
                      \s*
                      \z/xs
            && ref( \%s ) ne 'GLOB'
        )
    )
        && %s->can('does')
        && %s->does(%s)
    )
EOF
    };

    sub _build_inline_generator {$_inline_generator}
}

## no critic (Subroutines::ProhibitUnusedPrivateSubroutines)
sub _allow_classes {1}
## use critic

__PACKAGE__->_ooify;

1;

# ABSTRACT: A class for constraints which require a class name or an object that does a specific role

__END__

=pod

=encoding UTF-8

=head1 NAME

Specio::Constraint::AnyDoes - A class for constraints which require a class name or an object that does a specific role

=head1 VERSION

version 0.43

=head1 SYNOPSIS

    my $type = Specio::Constraint::AnyDoes->new(...);
    print $type->role;

=head1 DESCRIPTION

This is a specialized type constraint class for types which require a class
name or an object that does a specific role.

=head1 API

This class provides all of the same methods as L<Specio::Constraint::Simple>,
with a few differences:

=head2 Specio::Constraint::AnyDoes->new( ... )

The C<parent> parameter is ignored if it passed, as it is always set to the
C<Defined> type.

The C<inline_generator> and C<constraint> parameters are also ignored. This
class provides its own default inline generator subroutine reference.

This class overrides the C<message_generator> default if none is provided.

Finally, this class requires an additional parameter, C<role>. This must be a
single role name.

=head2 $any_isa->role

Returns the role name passed to the constructor.

=head1 ROLES

This class does the L<Specio::Constraint::Role::DoesType>,
L<Specio::Constraint::Role::Interface>, L<Specio::Role::Inlinable>, and
L<MooseX::Clone> roles.

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
