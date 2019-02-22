package DBM::Deep::Iterator::File;

use strict;
use warnings FATAL => 'all';

use base qw( DBM::Deep::Iterator );

use DBM::Deep::Iterator::File::BucketList ();
use DBM::Deep::Iterator::File::Index ();

sub reset { $_[0]{breadcrumbs} = []; return }

sub get_sector_iterator {
    my $self = shift;
    my ($loc) = @_;

    my $sector = $self->{engine}->load_sector( $loc )
        or return;

    if ( $sector->isa( 'DBM::Deep::Sector::File::Index' ) ) {
        return DBM::Deep::Iterator::File::Index->new({
            iterator => $self,
            sector   => $sector,
        });
    }
    elsif ( $sector->isa( 'DBM::Deep::Sector::File::BucketList' ) ) {
        return DBM::Deep::Iterator::File::BucketList->new({
            iterator => $self,
            sector   => $sector,
        });
    }

    DBM::Deep->_throw_error( "get_sector_iterator(): Why did $loc make a $sector?" );
}

sub get_next_key {
    my $self = shift;
    my ($obj) = @_;

    my $crumbs = $self->{breadcrumbs};
    my $e = $self->{engine};

    unless ( @$crumbs ) {
        # This will be a Reference sector
        my $sector = $e->load_sector( $self->{base_offset} )
            # If no sector is found, this must have been deleted from under us.
            or return;

        if ( $sector->staleness != $obj->_staleness ) {
            return;
        }

        my $loc = $sector->get_blist_loc
            or return;

        push @$crumbs, $self->get_sector_iterator( $loc );
    }

    FIND_NEXT_KEY: {
        # We're at the end.
        unless ( @$crumbs ) {
            $self->reset;
            return;
        }

        my $iterator = $crumbs->[-1];

        # This level is done.
        if ( $iterator->at_end ) {
            pop @$crumbs;
            redo FIND_NEXT_KEY;
        }

        if ( $iterator->isa( 'DBM::Deep::Iterator::File::Index' ) ) {
            # If we don't have any more, it will be caught at the
            # prior check.
            if ( my $next = $iterator->get_next_iterator ) {
                push @$crumbs, $next;
            }
            redo FIND_NEXT_KEY;
        }

        unless ( $iterator->isa( 'DBM::Deep::Iterator::File::BucketList' ) ) {
            DBM::Deep->_throw_error(
                "Should have a bucketlist iterator here - instead have $iterator"
            );
        }

        # At this point, we have a BucketList iterator
        my $key = $iterator->get_next_key;
        if ( defined $key ) {
            return $key;
        }
        #XXX else { $iterator->set_to_end() } ?

        # We hit the end of the bucketlist iterator, so redo
        redo FIND_NEXT_KEY;
    }

    DBM::Deep->_throw_error( "get_next_key(): How did we get here?" );
}

1;
__END__
