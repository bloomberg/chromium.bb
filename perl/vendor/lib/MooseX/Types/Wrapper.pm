package MooseX::Types::Wrapper;
{
  $MooseX::Types::Wrapper::VERSION = '0.31';
}

#ABSTRACT: Wrap exports from a library

use Moose;

use Carp::Clan      qw( ^MooseX::Types );
use Class::MOP;

use namespace::clean -except => [qw( meta )];

extends 'MooseX::Types';


sub import {
    my ($class, @args) = @_;
    my %libraries = @args == 1 ? (Moose => $args[0]) : @args;

    for my $l (keys %libraries) {

        croak qq($class expects an array reference as import spec)
            unless ref $libraries{ $l } eq 'ARRAY';

        my $library_class 
          = ($l eq 'Moose' ? 'MooseX::Types::Moose' : $l );
        Class::MOP::load_class($library_class);

        $library_class->import({ 
            -into    => scalar(caller),
            -wrapper => $class,
        }, @{ $libraries{ $l } });
    }
    return 1;
}

1;


__END__
=pod

=head1 NAME

MooseX::Types::Wrapper - Wrap exports from a library

=head1 VERSION

version 0.31

=head1 DESCRIPTION

See L<MooseX::Types/SYNOPSIS> for detailed usage.

=head1 METHODS

=head2 import

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

