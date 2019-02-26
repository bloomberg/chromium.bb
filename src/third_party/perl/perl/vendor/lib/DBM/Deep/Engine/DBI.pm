package DBM::Deep::Engine::DBI;

use 5.008_004;

use strict;
use warnings FATAL => 'all';
no warnings 'recursion';

use base 'DBM::Deep::Engine';

use DBM::Deep::Sector::DBI ();
use DBM::Deep::Storage::DBI ();

sub sector_type { 'DBM::Deep::Sector::DBI' }
sub iterator_class { 'DBM::Deep::Iterator::DBI' }

sub new {
    my $class = shift;
    my ($args) = @_;

    $args->{storage} = DBM::Deep::Storage::DBI->new( $args )
        unless exists $args->{storage};

    my $self = bless {
        storage => undef,
        external_refs => undef,
    }, $class;

    # Grab the parameters we want to use
    foreach my $param ( keys %$self ) {
        next unless exists $args->{$param};
        $self->{$param} = $args->{$param};
    }

    return $self;
}

sub setup {
    my $self = shift;
    my ($obj) = @_;

    # Default the id to 1. This means that we will be creating a row if there
    # isn't one. The assumption is that the row_id=1 cannot never be deleted. I
    # don't know if this is a good assumption.
    $obj->{base_offset} ||= 1;

    my ($rows) = $self->storage->read_from(
        refs => $obj->_base_offset,
        qw( ref_type ),
    );

    # We don't have a row yet.
    unless ( @$rows ) {
        $self->storage->write_to(
            refs => $obj->_base_offset,
            ref_type => $obj->_type,
        );
    }

    my $sector = DBM::Deep::Sector::DBI::Reference->new({
        engine => $self,
        offset => $obj->_base_offset,
    });
}

sub read_value {
    my $self = shift;
    my ($obj, $key) = @_;

    my $sector = $self->load_sector( $obj->_base_offset, 'refs' )
        or return;

#    if ( $sector->staleness != $obj->_staleness ) {
#        return;
#    }

#    my $key_md5 = $self->_apply_digest( $key );

    my $value_sector = $sector->get_data_for({
        key => $key,
#        key_md5    => $key_md5,
        allow_head => 1,
    });

    unless ( $value_sector ) {
        return undef
    }

    return $value_sector->data;
}

sub get_classname {
    my $self = shift;
    my ($obj) = @_;

    my $sector = $self->load_sector( $obj->_base_offset, 'refs' )
        or return;

    return $sector->get_classname;
}

sub make_reference {
    my $self = shift;
    my ($obj, $old_key, $new_key) = @_;

    my $sector = $self->load_sector( $obj->_base_offset, 'refs' )
        or return;

#    if ( $sector->staleness != $obj->_staleness ) {
#        return;
#    }

    my $value_sector = $sector->get_data_for({
        key        => $old_key,
        allow_head => 1,
    });

    unless ( $value_sector ) {
        $value_sector = DBM::Deep::Sector::DBI::Scalar->new({
            engine => $self,
            data   => undef,
        });

        $sector->write_data({
            key     => $old_key,
            value   => $value_sector,
        });
    }

    if ( $value_sector->isa( 'DBM::Deep::Sector::DBI::Reference' ) ) {
        $sector->write_data({
            key     => $new_key,
            value   => $value_sector,
        });
        $value_sector->increment_refcount;
    }
    else {
        $sector->write_data({
            key     => $new_key,
            value   => $value_sector->clone,
        });
    }

    return;
}

# exists returns '', not undefined.
sub key_exists {
    my $self = shift;
    my ($obj, $key) = @_;

    my $sector = $self->load_sector( $obj->_base_offset, 'refs' )
        or return '';

#    if ( $sector->staleness != $obj->_staleness ) {
#        return '';
#    }

    my $data = $sector->get_data_for({
#        key_md5    => $self->_apply_digest( $key ),
        key        => $key,
        allow_head => 1,
    });

    # exists() returns 1 or '' for true/false.
    return $data ? 1 : '';
}

sub delete_key {
    my $self = shift;
    my ($obj, $key) = @_;

    my $sector = $self->load_sector( $obj->_base_offset, 'refs' )
        or return '';

#    if ( $sector->staleness != $obj->_staleness ) {
#        return '';
#    }

    return $sector->delete_key({
#        key_md5    => $self->_apply_digest( $key ),
        key        => $key,
        allow_head => 0,
    });
}

sub write_value {
    my $self = shift;
    my ($obj, $key, $value) = @_;

    my $r = Scalar::Util::reftype( $value ) || '';
    {
        last if $r eq '';
        last if $r eq 'HASH';
        last if $r eq 'ARRAY';

        DBM::Deep->_throw_error(
            "Storage of references of type '$r' is not supported."
        );
    }

    # Load the reference entry
    # Determine if the row was deleted under us
    # 

    my $sector = $self->load_sector( $obj->_base_offset, 'refs' )
        or die "Cannot load sector at '@{[$obj->_base_offset]}'\n";;

    my ($type, $class);
    if (
     $r eq 'ARRAY' || $r eq 'HASH' and ref $value ne 'DBM::Deep::Null'
    ) {
        my $tmpvar;
        if ( $r eq 'ARRAY' ) {
            $tmpvar = tied @$value;
        } elsif ( $r eq 'HASH' ) {
            $tmpvar = tied %$value;
        }

        if ( $tmpvar ) {
            my $is_dbm_deep = eval { local $SIG{'__DIE__'}; $tmpvar->isa( 'DBM::Deep' ); };

            unless ( $is_dbm_deep ) {
                DBM::Deep->_throw_error( "Cannot store something that is tied." );
            }

            unless ( $tmpvar->_engine->storage == $self->storage ) {
                DBM::Deep->_throw_error( "Cannot store values across DBM::Deep files. Please use export() instead." );
            }

            # Load $tmpvar's sector

            # First, verify if we're storing the same thing to this spot. If we
            # are, then this should be a no-op. -EJS, 2008-05-19
            
            # See whether or not we are storing ourselves to ourself.
            # Write the sector as data in this reference (keyed by $key)
            my $value_sector = $self->load_sector( $tmpvar->_base_offset, 'refs' );
            $sector->write_data({
                key     => $key,
#                key_md5 => $self->_apply_digest( $key ),
                value   => $value_sector,
            });
            $value_sector->increment_refcount;

            return 1;
        }

        $type = substr( $r, 0, 1 );
        $class = 'DBM::Deep::Sector::DBI::Reference';
    }
    else {
        if ( tied($value) ) {
            DBM::Deep->_throw_error( "Cannot store something that is tied." );
        }

        if ( ref $value eq 'DBM::Deep::Null' ) {
            DBM::Deep::_warnif(
             'uninitialized', 'Assignment of stale reference'
            );
            $value = undef;
        }

        $class = 'DBM::Deep::Sector::DBI::Scalar';
        $type  = 'S';
    }

    # Create this after loading the reference sector in case something bad
    # happens. This way, we won't allocate value sector(s) needlessly.
    my $value_sector = $class->new({
        engine => $self,
        data   => $value,
        type   => $type,
    });

    $sector->write_data({
        key     => $key,
#        key_md5 => $self->_apply_digest( $key ),
        value   => $value_sector,
    });

    $self->_descend( $value, $value_sector );

    return 1;
}

#sub begin_work {
#    my $self = shift;
#    die "Transactions are not supported by this engine"
#        unless $self->supports('transactions');
#
#    if ( $self->in_txn ) {
#        DBM::Deep->_throw_error( "Cannot begin_work within an active transaction" );
#    }
#
#    $self->storage->begin_work;
#
#    $self->in_txn( 1 );
#
#    return 1;
#} 
#
#sub rollback {
#    my $self = shift;
#    die "Transactions are not supported by this engine"
#        unless $self->supports('transactions');
#
#    if ( !$self->in_txn ) {
#        DBM::Deep->_throw_error( "Cannot rollback without an active transaction" );
#    }
#
#    $self->storage->rollback;
#
#    $self->in_txn( 0 );
#
#    return 1;
#} 
#
#sub commit {
#    my $self = shift;
#    die "Transactions are not supported by this engine"
#        unless $self->supports('transactions');
#
#    if ( !$self->in_txn ) {
#        DBM::Deep->_throw_error( "Cannot commit without an active transaction" );
#    }
#
#    $self->storage->commit;
#
#    $self->in_txn( 0 );
#
#    return 1;
#}
#
#sub in_txn {
#    my $self = shift;
#    $self->{in_txn} = shift if @_;
#    $self->{in_txn};
#}

sub supports {
    my $self = shift;
    my ($feature) = @_;

    return if $feature eq 'transactions';
    return 1 if $feature eq 'singletons';
    return;
}

sub db_version {
    return '1.0020'
}

sub clear {
    my $self = shift;
    my $obj = shift;

    my $sector = $self->load_sector( $obj->_base_offset, 'refs' )
        or return;

    $sector->clear;

    return;
}

1;
__END__
