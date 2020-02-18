package DBM::Deep::Iterator::File::BucketList;

use 5.008_004;

use strict;
use warnings FATAL => 'all';

=head1 NAME

DBM::Deep::Iterator::BucketList - mediate between DBM::Deep::Iterator and DBM::Deep::Engine::Sector::BucketList

=head1 PURPOSE

This is an internal-use-only object for L<DBM::Deep>. It acts as the mediator
between the L<DBM::Deep::Iterator> object and a L<DBM::Deep::Engine::Sector::BucketList>
sector.

=head1 OVERVIEW

This object, despite the implied class hierarchy, does B<NOT> inherit from
L<DBM::Deep::Iterator>. Instead, it delegates to it, essentially acting as a
facade over it. L<DBM::Deep::Iterator/get_next_key> will instantiate one of
these objects as needed to handle an BucketList sector.

=head1 METHODS

=head2 new(\%params)

The constructor takes a hashref of params and blesses it into the invoking class. The
hashref is assumed to have the following elements:

=over 4

=item * iterator (of type L<DBM::Deep::Iterator>

=item * sector (of type L<DBM::Deep::Engine::Sector::BucketList>

=back

=cut

sub new {
    my $self = bless $_[1] => $_[0];
    $self->{curr_index} = 0;
    return $self;
}

=head2 at_end()

This takes no arguments.

This returns true/false indicating whether this sector has any more elements that can be
iterated over.

=cut

sub at_end {
    my $self = shift;
    return $self->{curr_index} >= $self->{iterator}{engine}->max_buckets;
}

=head2 get_next_iterator()

This takes no arguments.

This returns the next key pointed to by this bucketlist. This value is suitable for
returning by FIRSTKEY or NEXTKEY().

If the bucketlist is exhausted, it returns nothing.

=cut

sub get_next_key {
    my $self = shift;

    return if $self->at_end;

    my $idx = $self->{curr_index}++;

    my $data_loc = $self->{sector}->get_data_location_for({
        allow_head => 1,
        idx        => $idx,
    }) or return;

    #XXX Do we want to add corruption checks here?
    return $self->{sector}->get_key_for( $idx )->data;
}

1;
__END__
