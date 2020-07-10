package DBM::Deep::Iterator::File::Index;

use 5.008_004;

use strict;
use warnings FATAL => 'all';

=head1 NAME

DBM::Deep::Iterator::Index - mediate between DBM::Deep::Iterator and DBM::Deep::Engine::Sector::Index

=head1 PURPOSE

This is an internal-use-only object for L<DBM::Deep>. It acts as the mediator
between the L<DBM::Deep::Iterator> object and a L<DBM::Deep::Engine::Sector::Index>
sector.

=head1 OVERVIEW

This object, despite the implied class hierarchy, does B<NOT> inherit from
L<DBM::Deep::Iterator>. Instead, it delegates to it, essentially acting as a
facade over it. L<DBM::Deep::Iterator/get_next_key> will instantiate one of
these objects as needed to handle an Index sector.

=head1 METHODS

=head2 new(\%params)

The constructor takes a hashref of params and blesses it into the invoking class. The
hashref is assumed to have the following elements:

=over 4

=item * iterator (of type L<DBM::Deep::Iterator>

=item * sector (of type L<DBM::Deep::Engine::Sector::Index>

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
    return $self->{curr_index} >= $self->{iterator}{engine}->hash_chars;
}

=head2 get_next_iterator()

This takes no arguments.

This returns an iterator (built by L<DBM::Deep::Iterator/get_sector_iterator>) based
on the sector pointed to by the next occupied location in this index.

If the sector is exhausted, it returns nothing.

=cut

sub get_next_iterator {
    my $self = shift;

    my $loc;
    while ( !$loc ) {
        return if $self->at_end;
        $loc = $self->{sector}->get_entry( $self->{curr_index}++ );
    }

    return $self->{iterator}->get_sector_iterator( $loc );
}

1;
__END__
