package DBM::Deep::Sector::File::BucketList;

use 5.008_004;

use strict;
use warnings FATAL => 'all';

use base qw( DBM::Deep::Sector::File );

my $STALE_SIZE = 2;

# Please refer to the pack() documentation for further information
my %StP = (
    1 => 'C', # Unsigned char value (no order needed as it's just one byte)
    2 => 'n', # Unsigned short in "network" (big-endian) order
    4 => 'N', # Unsigned long in "network" (big-endian) order
    8 => 'Q', # Usigned quad (no order specified, presumably machine-dependent)
);

sub _init {
    my $self = shift;

    my $engine = $self->engine;

    unless ( $self->offset ) {
        my $leftover = $self->size - $self->base_size;

        $self->{offset} = $engine->_request_blist_sector( $self->size );
        $engine->storage->print_at( $self->offset, $engine->SIG_BLIST ); # Sector type
        # Skip staleness counter
        $engine->storage->print_at( $self->offset + $self->base_size,
            chr(0) x $leftover, # Zero-fill the data
        );
    }

    if ( $self->{key_md5} ) {
        $self->find_md5;
    }

    return $self;
}

sub wipe {
    my $self = shift;
    $self->engine->storage->print_at( $self->offset + $self->base_size,
        chr(0) x ($self->size - $self->base_size), # Zero-fill the data
    );
}

sub size {
    my $self = shift;
    unless ( $self->{size} ) {
        my $e = $self->engine;
        # Base + numbuckets * bucketsize
        $self->{size} = $self->base_size + $e->max_buckets * $self->bucket_size;
    }
    return $self->{size};
}

sub free_meth { '_add_free_blist_sector' }

sub free {
    my $self = shift;

    my $e = $self->engine;
    foreach my $bucket ( $self->chopped_up ) {
        my $rest = $bucket->[-1];

        # Delete the keysector
        my $l = unpack( $StP{$e->byte_size}, substr( $rest, $e->hash_size, $e->byte_size ) );
        my $s = $e->load_sector( $l ); $s->free if $s;

        # Delete the HEAD sector
        $l = unpack( $StP{$e->byte_size},
            substr( $rest,
                $e->hash_size + $e->byte_size,
                $e->byte_size,
            ),
        );
        $s = $e->load_sector( $l ); $s->free if $s;

        foreach my $txn ( 0 .. $e->num_txns - 2 ) {
            my $l = unpack( $StP{$e->byte_size},
                substr( $rest,
                    $e->hash_size + 2 * $e->byte_size + $txn * ($e->byte_size + $STALE_SIZE),
                    $e->byte_size,
                ),
            );
            my $s = $e->load_sector( $l ); $s->free if $s;
        }
    }

    $self->SUPER::free();
}

sub bucket_size {
    my $self = shift;
    unless ( $self->{bucket_size} ) {
        my $e = $self->engine;
        # Key + head (location) + transactions (location + staleness-counter)
        my $location_size = $e->byte_size + $e->byte_size + ($e->num_txns - 1) * ($e->byte_size + $STALE_SIZE);
        $self->{bucket_size} = $e->hash_size + $location_size;
    }
    return $self->{bucket_size};
}

# XXX This is such a poor hack. I need to rethink this code.
sub chopped_up {
    my $self = shift;

    my $e = $self->engine;

    my @buckets;
    foreach my $idx ( 0 .. $e->max_buckets - 1 ) {
        my $spot = $self->offset + $self->base_size + $idx * $self->bucket_size;
        my $md5 = $e->storage->read_at( $spot, $e->hash_size );

        #XXX If we're chopping, why would we ever have the blank_md5?
        last if $md5 eq $e->blank_md5;

        my $rest = $e->storage->read_at( undef, $self->bucket_size - $e->hash_size );
        push @buckets, [ $spot, $md5 . $rest ];
    }

    return @buckets;
}

sub write_at_next_open {
    my $self = shift;
    my ($entry) = @_;

    #XXX This is such a hack!
    $self->{_next_open} = 0 unless exists $self->{_next_open};

    my $spot = $self->offset + $self->base_size + $self->{_next_open}++ * $self->bucket_size;
    $self->engine->storage->print_at( $spot, $entry );

    return $spot;
}

sub has_md5 {
    my $self = shift;
    unless ( exists $self->{found} ) {
        $self->find_md5;
    }
    return $self->{found};
}

sub find_md5 {
    my $self = shift;

    $self->{found} = undef;
    $self->{idx}   = -1;

    if ( @_ ) {
        $self->{key_md5} = shift;
    }

    # If we don't have an MD5, then what are we supposed to do?
    unless ( exists $self->{key_md5} ) {
        DBM::Deep->_throw_error( "Cannot find_md5 without a key_md5 set" );
    }

    my $e = $self->engine;
    foreach my $idx ( 0 .. $e->max_buckets - 1 ) {
        my $potential = $e->storage->read_at(
            $self->offset + $self->base_size + $idx * $self->bucket_size, $e->hash_size,
        );

        if ( $potential eq $e->blank_md5 ) {
            $self->{idx} = $idx;
            return;
        }

        if ( $potential eq $self->{key_md5} ) {
            $self->{found} = 1;
            $self->{idx} = $idx;
            return;
        }
    }

    return;
}

sub write_md5 {
    my $self = shift;
    my ($args) = @_;

    DBM::Deep->_throw_error( "write_md5: no key" ) unless exists $args->{key};
    DBM::Deep->_throw_error( "write_md5: no key_md5" ) unless exists $args->{key_md5};
    DBM::Deep->_throw_error( "write_md5: no value" ) unless exists $args->{value};

    my $engine = $self->engine;

    $args->{trans_id} = $engine->trans_id unless exists $args->{trans_id};

    my $spot = $self->offset + $self->base_size + $self->{idx} * $self->bucket_size;
    $engine->add_entry( $args->{trans_id}, $spot );

    unless ($self->{found}) {
        my $key_sector = DBM::Deep::Sector::File::Scalar->new({
            engine => $engine,
            data   => $args->{key},
        });

        $engine->storage->print_at( $spot,
            $args->{key_md5},
            pack( $StP{$engine->byte_size}, $key_sector->offset ),
        );
    }

    my $loc = $spot
      + $engine->hash_size
      + $engine->byte_size;

    if ( $args->{trans_id} ) {
        $loc += $engine->byte_size + ($args->{trans_id} - 1) * ( $engine->byte_size + $STALE_SIZE );

        $engine->storage->print_at( $loc,
            pack( $StP{$engine->byte_size}, $args->{value}->offset ),
            pack( $StP{$STALE_SIZE}, $engine->get_txn_staleness_counter( $args->{trans_id} ) ),
        );
    }
    else {
        $engine->storage->print_at( $loc,
            pack( $StP{$engine->byte_size}, $args->{value}->offset ),
        );
    }
}

sub mark_deleted {
    my $self = shift;
    my ($args) = @_;
    $args ||= {};

    my $engine = $self->engine;

    $args->{trans_id} = $engine->trans_id unless exists $args->{trans_id};

    my $spot = $self->offset + $self->base_size + $self->{idx} * $self->bucket_size;
    $engine->add_entry( $args->{trans_id}, $spot );

    my $loc = $spot
      + $engine->hash_size
      + $engine->byte_size;

    if ( $args->{trans_id} ) {
        $loc += $engine->byte_size + ($args->{trans_id} - 1) * ( $engine->byte_size + $STALE_SIZE );

        $engine->storage->print_at( $loc,
            pack( $StP{$engine->byte_size}, 1 ), # 1 is the marker for deleted
            pack( $StP{$STALE_SIZE}, $engine->get_txn_staleness_counter( $args->{trans_id} ) ),
        );
    }
    else {
        $engine->storage->print_at( $loc,
            pack( $StP{$engine->byte_size}, 1 ), # 1 is the marker for deleted
        );
    }
}

sub delete_md5 {
    my $self = shift;
    my ($args) = @_;

    my $engine = $self->engine;
    return undef unless $self->{found};

    # Save the location so that we can free the data
    my $location = $self->get_data_location_for({
        allow_head => 0,
    });
    my $key_sector = $self->get_key_for;

    my $spot = $self->offset + $self->base_size + $self->{idx} * $self->bucket_size;
    $engine->storage->print_at( $spot,
        $engine->storage->read_at(
            $spot + $self->bucket_size,
            $self->bucket_size * ( $engine->max_buckets - $self->{idx} - 1 ),
        ),
        chr(0) x $self->bucket_size,
    );

    $key_sector->free;

    my $data_sector = $self->engine->load_sector( $location );
    my $data = $data_sector->data({ export => 1 });
    $data_sector->free;

    return $data;
}

sub get_data_location_for {
    my $self = shift;
    my ($args) = @_;
    $args ||= {};

    $args->{allow_head} = 0 unless exists $args->{allow_head};
    $args->{trans_id}   = $self->engine->trans_id unless exists $args->{trans_id};
    $args->{idx}        = $self->{idx} unless exists $args->{idx};

    my $e = $self->engine;

    my $spot = $self->offset + $self->base_size
      + $args->{idx} * $self->bucket_size
      + $e->hash_size
      + $e->byte_size;

    if ( $args->{trans_id} ) {
        $spot += $e->byte_size + ($args->{trans_id} - 1) * ( $e->byte_size + $STALE_SIZE );
    }

    my $buffer = $e->storage->read_at(
        $spot,
        $e->byte_size + $STALE_SIZE,
    );
    my ($loc, $staleness) = unpack( $StP{$e->byte_size} . ' ' . $StP{$STALE_SIZE}, $buffer );

    # XXX Merge the two if-clauses below
    if ( $args->{trans_id} ) {
        # We have found an entry that is old, so get rid of it
        if ( $staleness != (my $s = $e->get_txn_staleness_counter( $args->{trans_id} ) ) ) {
            $e->storage->print_at(
                $spot,
                pack( $StP{$e->byte_size} . ' ' . $StP{$STALE_SIZE}, (0) x 2 ), 
            );
            $loc = 0;
        }
    }

    # If we're in a transaction and we never wrote to this location, try the
    # HEAD instead.
    if ( $args->{trans_id} && !$loc && $args->{allow_head} ) {
        return $self->get_data_location_for({
            trans_id   => 0,
            allow_head => 1,
            idx        => $args->{idx},
        });
    }

    return $loc <= 1 ? 0 : $loc;
}

sub get_data_for {
    my $self = shift;
    my ($args) = @_;
    $args ||= {};

    return unless $self->{found};
    my $location = $self->get_data_location_for({
        allow_head => $args->{allow_head},
    });
    return $self->engine->load_sector( $location );
}

sub get_key_for {
    my $self = shift;
    my ($idx) = @_;
    $idx = $self->{idx} unless defined $idx;

    if ( $idx >= $self->engine->max_buckets ) {
        DBM::Deep->_throw_error( "get_key_for(): Attempting to retrieve $idx" );
    }

    my $location = $self->engine->storage->read_at(
        $self->offset + $self->base_size + $idx * $self->bucket_size + $self->engine->hash_size,
        $self->engine->byte_size,
    );
    $location = unpack( $StP{$self->engine->byte_size}, $location );
    DBM::Deep->_throw_error( "get_key_for: No location?" ) unless $location;

    return $self->engine->load_sector( $location );
}

1;
__END__
