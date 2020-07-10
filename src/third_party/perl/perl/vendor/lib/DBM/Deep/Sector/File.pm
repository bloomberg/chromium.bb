package DBM::Deep::Sector::File;

use 5.008_004;

use strict;
use warnings FATAL => 'all';

use base qw( DBM::Deep::Sector );

use DBM::Deep::Sector::File::BucketList ();
use DBM::Deep::Sector::File::Index ();
use DBM::Deep::Sector::File::Null ();
use DBM::Deep::Sector::File::Reference ();
use DBM::Deep::Sector::File::Scalar ();

my $STALE_SIZE = 2;

sub base_size {
    my $self = shift;
    return $self->engine->SIG_SIZE + $STALE_SIZE;
}

sub free_meth { die "free_meth must be implemented in a child class" }

sub free {
    my $self = shift;

    my $e = $self->engine;

    $e->storage->print_at( $self->offset, $e->SIG_FREE );
    # Skip staleness counter
    $e->storage->print_at( $self->offset + $self->base_size,
        chr(0) x ($self->size - $self->base_size),
    );

    my $free_meth = $self->free_meth;
    $e->$free_meth( $self->offset, $self->size );

    return;
}

#=head2 load( $offset )
#
#This will instantiate and return the sector object that represents the data
#found at $offset.
#
#=cut

sub load {
    my $self = shift;
    my ($engine, $offset) = @_;

    # Add a catch for offset of 0 or 1
    return if !$offset || $offset <= 1;

    my $type = $engine->storage->read_at( $offset, 1 );
    return if $type eq chr(0);

    if ( $type eq $engine->SIG_ARRAY || $type eq $engine->SIG_HASH ) {
        return DBM::Deep::Sector::File::Reference->new({
            engine => $engine,
            type   => $type,
            offset => $offset,
        });
    }
    # XXX Don't we need key_md5 here?
    elsif ( $type eq $engine->SIG_BLIST ) {
        return DBM::Deep::Sector::File::BucketList->new({
            engine => $engine,
            type   => $type,
            offset => $offset,
        });
    }
    elsif ( $type eq $engine->SIG_INDEX ) {
        return DBM::Deep::Sector::File::Index->new({
            engine => $engine,
            type   => $type,
            offset => $offset,
        });
    }
    elsif ( $type eq $engine->SIG_NULL ) {
        return DBM::Deep::Sector::File::Null->new({
            engine => $engine,
            type   => $type,
            offset => $offset,
        });
    }
    elsif ( $type eq $engine->SIG_DATA || $type eq $engine->SIG_UNIDATA ) {
        return DBM::Deep::Sector::File::Scalar->new({
            engine => $engine,
            type   => $type,
            offset => $offset,
        });
    }
    # This was deleted from under us, so just return and let the caller figure it out.
    elsif ( $type eq $engine->SIG_FREE ) {
        return;
    }

    DBM::Deep->_throw_error( "'$offset': Don't know what to do with type '$type'" );
}

1;
__END__
