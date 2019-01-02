package MooseX::Types::Combine;
{
  $MooseX::Types::Combine::VERSION = '0.31';
}

# ABSTRACT: Combine type libraries for exporting

use strict;
use warnings;
use Class::MOP ();


sub import {
    my ($class, @types) = @_;
    my $caller = caller;

    my %types = $class->_provided_types;

    if ( grep { $_ eq ':all' } @types ) {
        $_->import( { -into => $caller }, q{:all} )
            for $class->provide_types_from;
        return;
    }

    my %from;
    for my $type (@types) {
        unless ($types{$type}) {
            my @type_libs = $class->provide_types_from;

            die
                "$caller asked for a type ($type) which is not found in any of the"
                . " type libraries (@type_libs) combined by $class\n";
        }

        push @{ $from{ $types{$type} } }, $type;
    }

    $_->import({ -into => $caller }, @{ $from{ $_ } })
        for keys %from;
}


sub provide_types_from {
    my ($class, @libs) = @_;

    my $store =
     do { no strict 'refs'; \@{ "${class}::__MOOSEX_TYPELIBRARY_LIBRARIES" } };

    if (@libs) {
        $class->_check_type_lib($_) for @libs;
        @$store = @libs;

        my %types = map {
            my $lib = $_;
            map +( $_ => $lib ), $lib->type_names
        } @libs;

        $class->_provided_types(%types);
    }

    @$store;
}

sub _check_type_lib {
    my ($class, $lib) = @_;

    Class::MOP::load_class($lib);

    die "Cannot use $lib in a combined type library, it does not provide any types"
        unless $lib->can('type_names');
}

sub _provided_types {
    my ($class, %types) = @_;

    my $types =
     do { no strict 'refs'; \%{ "${class}::__MOOSEX_TYPELIBRARY_TYPES" } };

    %$types = %types
        if keys %types;

    %$types;
}


1;

__END__
=pod

=head1 NAME

MooseX::Types::Combine - Combine type libraries for exporting

=head1 VERSION

version 0.31

=head1 SYNOPSIS

    package CombinedTypeLib;

    use base 'MooseX::Types::Combine';

    __PACKAGE__->provide_types_from(qw/TypeLib1 TypeLib2/);

    package UserClass;

    use CombinedTypeLib qw/Type1 Type2 ... /;

=head1 DESCRIPTION

Allows you to export types from multiple type libraries. 

Libraries on the right side of the type libs passed to L</provide_types_from>
take precedence over those on the left in case of conflicts.

=head1 CLASS METHODS

=head2 provide_types_from

Sets or returns a list of type libraries to re-export from.

=head1 SEE ALSO

L<MooseX::Types>

=head1 LICENSE

This program is free software; you can redistribute it and/or modify
it under the same terms as perl itself.

=head1 AUTHOR

Robert "phaylon" Sedlacek <rs@474.at>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by Robert "phaylon" Sedlacek.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

