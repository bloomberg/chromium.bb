package DBM::Deep::Array;

use 5.008_004;

use strict;
use warnings FATAL => 'all';
no warnings 'recursion';

# This is to allow DBM::Deep::Array to handle negative indices on
# its own. Otherwise, Perl would intercept the call to negative
# indices for us. This was causing bugs for negative index handling.
our $NEGATIVE_INDICES = 1;

use base 'DBM::Deep';

use Scalar::Util ();

sub _get_self {
    # We used to have
    #  eval { local $SIG{'__DIE__'}; tied( @{$_[0]} ) } || $_[0]
    # but this does not always work during global destruction (DBM::Deep’s
    # destructor calls this method), but will return $_[0] even when $_[0]
    # is tied, if it’s tied to undef. In those cases it’s better to return
    # undef, so the destructor can tell not to do anything,  and,  if any-
    # thing else calls us, it will fail with a more helpful error message.

    Scalar::Util::reftype $_[0] eq 'ARRAY' ? tied @{$_[0]} : $_[0];
}

sub _repr { [] }

sub TIEARRAY {
    my $class = shift;
    my $args = $class->_get_args( @_ );

    $args->{type} = $class->TYPE_ARRAY;

    return $class->_init($args);
}

sub FETCH {
    my $self = shift->_get_self;
    my ($key) = @_;

    $self->lock_shared;

    if ( !defined $key ) {
        $self->unlock;
        DBM::Deep->_throw_error( "Cannot use an undefined array index." );
    }
    elsif ( $key =~ /^-?\d+$/ ) {
        if ( $key < 0 ) {
            $key += $self->FETCHSIZE;
            unless ( $key >= 0 ) {
                $self->unlock;
                return;
            }
        }
    }
    elsif ( $key ne 'length' ) {
        $self->unlock;
        DBM::Deep->_throw_error( "Cannot use '$key' as an array index." );
    }

    my $rv = $self->SUPER::FETCH( $key );

    $self->unlock;

    return $rv;
}

sub STORE {
    my $self = shift->_get_self;
    my ($key, $value) = @_;

    $self->lock_exclusive;

    my $size;
    my $idx_is_numeric;
    if ( !defined $key ) {
        $self->unlock;
        DBM::Deep->_throw_error( "Cannot use an undefined array index." );
    }
    elsif ( $key =~ /^-?\d+$/ ) {
        $idx_is_numeric = 1;
        if ( $key < 0 ) {
            $size = $self->FETCHSIZE;
            if ( $key + $size < 0 ) {
                die( "Modification of non-creatable array value attempted, subscript $key" );
            }
            $key += $size
        }
    }
    elsif ( $key ne 'length' ) {
        $self->unlock;
        DBM::Deep->_throw_error( "Cannot use '$key' as an array index." );
    }

    my $rv = $self->SUPER::STORE( $key, $value );

    if ( $idx_is_numeric ) {
        $size = $self->FETCHSIZE unless defined $size;
        if ( $key >= $size ) {
            $self->STORESIZE( $key + 1 );
        }
    }

    $self->unlock;

    return $rv;
}

sub EXISTS {
    my $self = shift->_get_self;
    my ($key) = @_;

    $self->lock_shared;

    if ( !defined $key ) {
        $self->unlock;
        DBM::Deep->_throw_error( "Cannot use an undefined array index." );
    }
    elsif ( $key =~ /^-?\d+$/ ) {
        if ( $key < 0 ) {
            $key += $self->FETCHSIZE;
            unless ( $key >= 0 ) {
                $self->unlock;
                return;
            }
        }
    }
    elsif ( $key ne 'length' ) {
        $self->unlock;
        DBM::Deep->_throw_error( "Cannot use '$key' as an array index." );
    }

    my $rv = $self->SUPER::EXISTS( $key );

    $self->unlock;

    return $rv;
}

sub DELETE {
    my $self = shift->_get_self;
    my ($key) = @_;
    warn "ARRAY::DELETE($self,$key)\n" if DBM::Deep::DEBUG;

    $self->lock_exclusive;

    my $size = $self->FETCHSIZE;
    if ( !defined $key ) {
        $self->unlock;
        DBM::Deep->_throw_error( "Cannot use an undefined array index." );
    }
    elsif ( $key =~ /^-?\d+$/ ) {
        if ( $key < 0 ) {
            $key += $size;
            unless ( $key >= 0 ) {
                $self->unlock;
                return;
            }
        }
    }
    elsif ( $key ne 'length' ) {
        $self->unlock;
        DBM::Deep->_throw_error( "Cannot use '$key' as an array index." );
    }

    my $rv = $self->SUPER::DELETE( $key );

    if ($rv && $key == $size - 1) {
        $self->STORESIZE( $key );
    }

    $self->unlock;

    return $rv;
}

# Now that we have a real Reference sector, we should store arrayzize there.
# However, arraysize needs to be transactionally-aware, so a simple location to
# store it isn't going to work.
sub FETCHSIZE {
    my $self = shift->_get_self;

    $self->lock_shared;

    my $SAVE_FILTER = $self->_engine->storage->{filter_fetch_value};
    $self->_engine->storage->{filter_fetch_value} = undef;

    my $size = $self->FETCH('length') || 0;

    $self->_engine->storage->{filter_fetch_value} = $SAVE_FILTER;

    $self->unlock;

    return $size;
}

sub STORESIZE {
    my $self = shift->_get_self;
    my ($new_length) = @_;

    $self->lock_exclusive;

    my $SAVE_FILTER = $self->_engine->storage->{filter_store_value};
    $self->_engine->storage->{filter_store_value} = undef;

    my $result = $self->STORE('length', $new_length, 'length');

    $self->_engine->storage->{filter_store_value} = $SAVE_FILTER;

    $self->unlock;

    return $result;
}

sub POP {
    my $self = shift->_get_self;

    $self->lock_exclusive;

    my $length = $self->FETCHSIZE();

    if ($length) {
        my $content = $self->FETCH( $length - 1 );
        $self->DELETE( $length - 1 );

        $self->unlock;

        return $content;
    }
    else {
        $self->unlock;
        return;
    }
}

sub PUSH {
    my $self = shift->_get_self;

    $self->lock_exclusive;

    my $length = $self->FETCHSIZE();

    for my $content (@_) {
        $self->STORE( $length, $content );
        $length++;
    }

    $self->unlock;

    return $length;
}

# XXX This really needs to be something more direct within the file, not a
# fetch and re-store. -RobK, 2007-09-20
sub _move_value {
    my $self = shift;
    my ($old_key, $new_key) = @_;

    return $self->_engine->make_reference( $self, $old_key, $new_key );
}

sub SHIFT {
    my $self = shift->_get_self;
    warn "SHIFT($self)\n" if DBM::Deep::DEBUG;

    $self->lock_exclusive;

    my $length = $self->FETCHSIZE();

    if ( !$length ) {
        $self->unlock;
        return;
    }

    my $content = $self->DELETE( 0 );

    # Unless the deletion above has cleared the array ...
    if ( $length > 1 ) {
        for (my $i = 0; $i < $length - 1; $i++) {
            $self->_move_value( $i+1, $i );
        }

        $self->DELETE( $length - 1 );
    }

    $self->unlock;

    return $content;
}

sub UNSHIFT {
    my $self = shift->_get_self;
    my @new_elements = @_;

    $self->lock_exclusive;

    my $length = $self->FETCHSIZE();
    my $new_size = scalar @new_elements;

    if ($length) {
        for (my $i = $length - 1; $i >= 0; $i--) {
            $self->_move_value( $i, $i+$new_size );
        }

        $self->STORESIZE( $length + $new_size );
    }

    for (my $i = 0; $i < $new_size; $i++) {
        $self->STORE( $i, $new_elements[$i] );
    }

    $self->unlock;

    return $length + $new_size;
}

sub SPLICE {
    my $self = shift->_get_self;

    $self->lock_exclusive;

    my $length = $self->FETCHSIZE();

    ##
    # Calculate offset and length of splice
    ##
    my $offset = shift;
    $offset = 0 unless defined $offset;
    if ($offset < 0) { $offset += $length; }

    my $splice_length;
    if (scalar @_) { $splice_length = shift; }
    else { $splice_length = $length - $offset; }
    if ($splice_length < 0) { $splice_length += ($length - $offset); }

    ##
    # Setup array with new elements, and copy out old elements for return
    ##
    my @new_elements = @_;
    my $new_size = scalar @new_elements;

    my @old_elements = map {
        $self->FETCH( $_ )
    } $offset .. ($offset + $splice_length - 1);

    ##
    # Adjust array length, and shift elements to accommodate new section.
    ##
    if ( $new_size != $splice_length ) {
        if ($new_size > $splice_length) {
            for (my $i = $length - 1; $i >= $offset + $splice_length; $i--) {
                $self->_move_value( $i, $i + ($new_size - $splice_length) );
            }
            $self->STORESIZE( $length + $new_size - $splice_length );
        }
        else {
            for (my $i = $offset + $splice_length; $i < $length; $i++) {
                $self->_move_value( $i, $i + ($new_size - $splice_length) );
            }
            for (my $i = 0; $i < $splice_length - $new_size; $i++) {
                $self->DELETE( $length - 1 );
                $length--;
            }
        }
    }

    ##
    # Insert new elements into array
    ##
    for (my $i = $offset; $i < $offset + $new_size; $i++) {
        $self->STORE( $i, shift @new_elements );
    }

    $self->unlock;

    ##
    # Return deleted section, or last element in scalar context.
    ##
    return wantarray ? @old_elements : $old_elements[-1];
}

# We don't need to populate it, yet.
# It will be useful, though, when we split out HASH and ARRAY
# Perl will call EXTEND() when the array is likely to grow.
# We don't care, but include it because it gets called at times.
sub EXTEND {}

sub _copy_node {
    my $self = shift;
    my ($db_temp) = @_;

    my $length = $self->length();
    for (my $index = 0; $index < $length; $index++) {
        $self->_copy_value( \$db_temp->[$index], $self->get($index) );
    }

    return 1;
}

sub _clear {
    my $self = shift;

    my $size = $self->FETCHSIZE;
    for my $key ( 0 .. $size - 1 ) {
        $self->_engine->delete_key( $self, $key, $key );
    }
    $self->STORESIZE( 0 );

    return;
}

sub length  { (shift)->FETCHSIZE(@_) }
sub pop     { (shift)->POP(@_)       }
sub push    { (shift)->PUSH(@_)      }
sub unshift { (shift)->UNSHIFT(@_)   }
sub splice  { (shift)->SPLICE(@_)    }

# This must be last otherwise we have to qualify all other calls to shift
# as calls to CORE::shift
sub shift { (CORE::shift)->SHIFT(@_) }

1;
__END__
