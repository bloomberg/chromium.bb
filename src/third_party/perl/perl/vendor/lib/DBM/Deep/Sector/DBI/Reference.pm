package DBM::Deep::Sector::DBI::Reference;

use 5.008_004;

use strict;
use warnings FATAL => 'all';

use base 'DBM::Deep::Sector::DBI';

use Scalar::Util;

sub table { 'refs' }

sub _init {
    my $self = shift;

    my $e = $self->engine;

    unless ( $self->offset ) {
        my $classname = Scalar::Util::blessed( delete $self->{data} );
        $self->{offset} = $self->engine->storage->write_to(
            refs => undef,
            ref_type  => $self->type,
            classname => $classname,
        );
    }
    else {
        my ($rows) = $self->engine->storage->read_from(
            refs => $self->offset,
            qw( ref_type ),
        );

        $self->{type} = $rows->[0]{ref_type};
    }

    return;
}

sub get_data_for {
    my $self = shift;
    my ($args) = @_;

    my ($rows) = $self->engine->storage->read_from(
        datas => { ref_id => $self->offset, key => $args->{key} },
        qw( id ),
    );

    return unless $rows->[0]{id};

    $self->load(
        $self->engine,
        $rows->[0]{id},
        'datas',
    );
}

sub write_data {
    my $self = shift;
    my ($args) = @_;

    if ( ( $args->{value}->type || 'S' ) eq 'S' ) {
        $args->{value}{offset} = $self->engine->storage->write_to(
            datas => $args->{value}{offset},
            ref_id    => $self->offset,
            data_type => 'S',
            key       => $args->{key},
            value     => $args->{value}{data},
        );

        $args->{value}->reload;
    }
    else {
        # Write the Scalar of the Reference
        $self->engine->storage->write_to(
            datas => undef,
            ref_id    => $self->offset,
            data_type => 'R',
            key       => $args->{key},
            value     => $args->{value}{offset},
        );
    }
}

sub delete_key {
    my $self = shift;
    my ($args) = @_;

    my $old_value = $self->get_data_for({
        key => $args->{key},
    });

    my $data;
    if ( $old_value ) {
        $data = $old_value->data({ export => 1 });

        $self->engine->storage->delete_from(
            'datas',
             { ref_id => $self->offset,
               key => $args->{key},  },
        );
        $old_value->free;
    }

    return $data;
}

sub get_classname {
    my $self = shift;
    my ($rows) = $self->engine->storage->read_from(
        'refs', $self->offset,
        qw( classname ),
    );
    return unless @$rows;
    return $rows->[0]{classname};
}

# Look to hoist this method into a ::Reference trait
sub data {
    my $self = shift;
    my ($args) = @_;
    $args ||= {};

    my $engine = $self->engine;
    my $cache = $engine->cache;
    my $off = $self->offset;
    my $obj;
    if ( !defined $cache->{ $off } ) {
        $obj = DBM::Deep->new({
            type        => $self->type,
            base_offset => $self->offset,
            storage     => $engine->storage,
            engine      => $engine,
        });

        $cache->{$off} = $obj;
        Scalar::Util::weaken($cache->{$off});
    }
    else {
        $obj = $cache->{$off};
    }

    # We're not exporting, so just return.
    unless ( $args->{export} ) {
        if ( $engine->storage->{autobless} ) {
            my $classname = $self->get_classname;
            if ( defined $classname ) {
                bless $obj, $classname;
            }
        }

        return $obj;
    }

    # We shouldn't export if this is still referred to.
    if ( $self->get_refcount > 1 ) {
        return $obj;
    }

    return $obj->export;
}

sub free {
    my $self = shift;

    # We're not ready to be removed yet.
    return if $self->decrement_refcount > 0;

    # Rebless the object into DBM::Deep::Null.
    # In external_refs mode, this will already have been removed from
    # the cache, so we can skip this.
    my $e  = $self->engine;
    if(!$e->{external_refs}) {
        eval { %{ $e->cache->{ $self->offset } } = (); };
        eval { @{ $e->cache->{ $self->offset } } = (); };
        bless $e->cache->{ $self->offset }, 'DBM::Deep::Null';
        delete $e->cache->{ $self->offset };
    }

    $e->storage->delete_from(
        'datas', { ref_id => $self->offset },
    );

    $e->storage->delete_from(
        'datas', { value => $self->offset, data_type => 'R' },
    );

    $self->SUPER::free( @_ );
}

sub increment_refcount {
    my $self = shift;
    my $refcount = $self->get_refcount;
    $refcount++;
    $self->write_refcount( $refcount );
    return $refcount;
}

sub decrement_refcount {
    my $self = shift;
    my $refcount = $self->get_refcount;
    $refcount--;
    $self->write_refcount( $refcount );
    return $refcount;
}

sub get_refcount {
    my $self = shift;
    my ($rows) = $self->engine->storage->read_from(
        'refs', $self->offset,
        qw( refcount ),
    );
    return $rows->[0]{refcount};
}

sub write_refcount {
    my $self = shift;
    my ($num) = @_;
    $self->engine->storage->{dbh}->do(
        "UPDATE refs SET refcount = ? WHERE id = ?", undef,
        $num, $self->offset,
    );
}

sub clear {
    my $self = shift;

    DBM::Deep->new({
        type        => $self->type,
        base_offset => $self->offset,
        storage     => $self->engine->storage,
        engine      => $self->engine,
    })->_clear;

    return;
}

1;
__END__
