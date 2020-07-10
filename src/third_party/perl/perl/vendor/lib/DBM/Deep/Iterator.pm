package DBM::Deep::Iterator;

use 5.008_004;

use strict;
use warnings FATAL => 'all';

=head1 NAME

DBM::Deep::Iterator - iterator for FIRSTKEY() and NEXTKEY()

=head1 PURPOSE

This is an internal-use-only object for L<DBM::Deep>. It is the iterator
for FIRSTKEY() and NEXTKEY().

=head1 OVERVIEW

This object 

=head1 METHODS

=head2 new(\%params)

The constructor takes a hashref of params. The hashref is assumed to have the
following elements:

=over 4

=item * engine (of type L<DBM::Deep::Engine>

=item * base_offset (the base_offset of the invoking DBM::Deep object)

=back

=cut

sub new {
    my $class = shift;
    my ($args) = @_;

    my $self = bless {
        engine      => $args->{engine},
        base_offset => $args->{base_offset},
    }, $class;

    Scalar::Util::weaken( $self->{engine} );

    $self->reset;

    return $self;
}

=head2 reset()

This method takes no arguments.

It will reset the iterator so that it will start from the beginning again.

This method returns nothing.

=cut

sub reset { die "reset must be implemented in a child class" }

=head2 get_next_key( $obj )

=cut

sub get_next_key { die "get_next_key must be implemented in a child class" }

1;
__END__
