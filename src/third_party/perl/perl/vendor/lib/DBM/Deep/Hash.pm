package DBM::Deep::Hash;

use 5.008_004;

use strict;
use warnings FATAL => 'all';
no warnings 'recursion';

use base 'DBM::Deep';

sub _get_self {
    # See the note in Array.pm as to why this is commented out.
    # eval { local $SIG{'__DIE__'}; tied( %{$_[0]} ) } || $_[0]

    # During global destruction %{$_[0]} might get tied to undef, so we
    # need to check that case if tied returns false.
    tied %{$_[0]} or local *@, eval { exists $_[0]{_}; 1 } ? $_[0] : undef
}

sub _repr { return {} }

sub TIEHASH {
    my $class = shift;
    my $args = $class->_get_args( @_ );
    
    $args->{type} = $class->TYPE_HASH;

    return $class->_init($args);
}

sub FETCH {
    my $self = shift->_get_self;
    DBM::Deep->_throw_error( "Cannot use an undefined hash key." ) unless defined $_[0];
    my $key = ($self->_engine->storage->{filter_store_key})
        ? $self->_engine->storage->{filter_store_key}->($_[0])
        : $_[0];

    return $self->SUPER::FETCH( $key, $_[0] );
}

sub STORE {
    my $self = shift->_get_self;
    DBM::Deep->_throw_error( "Cannot use an undefined hash key." ) unless defined $_[0];
    my $key = ($self->_engine->storage->{filter_store_key})
        ? $self->_engine->storage->{filter_store_key}->($_[0])
        : $_[0];
    my $value = $_[1];

    return $self->SUPER::STORE( $key, $value, $_[0] );
}

sub EXISTS {
    my $self = shift->_get_self;
    DBM::Deep->_throw_error( "Cannot use an undefined hash key." ) unless defined $_[0];
    my $key = ($self->_engine->storage->{filter_store_key})
        ? $self->_engine->storage->{filter_store_key}->($_[0])
        : $_[0];

    return $self->SUPER::EXISTS( $key );
}

sub DELETE {
    my $self = shift->_get_self;
    DBM::Deep->_throw_error( "Cannot use an undefined hash key." ) unless defined $_[0];
    my $key = ($self->_engine->storage->{filter_store_key})
        ? $self->_engine->storage->{filter_store_key}->($_[0])
        : $_[0];

    return $self->SUPER::DELETE( $key, $_[0] );
}

# Locate and return first key (in no particular order)
sub FIRSTKEY {
    my $self = shift->_get_self;

    $self->lock_shared;
    
    my $result = $self->_engine->get_next_key( $self );
    
    $self->unlock;
    
    return ($result && $self->_engine->storage->{filter_fetch_key})
        ? $self->_engine->storage->{filter_fetch_key}->($result)
        : $result;
}

# Return next key (in no particular order), given previous one
sub NEXTKEY {
    my $self = shift->_get_self;

    my $prev_key = ($self->_engine->storage->{filter_store_key})
        ? $self->_engine->storage->{filter_store_key}->($_[0])
        : $_[0];

    $self->lock_shared;
    
    my $result = $self->_engine->get_next_key( $self, $prev_key );
    
    $self->unlock;
    
    return ($result && $self->_engine->storage->{filter_fetch_key})
        ? $self->_engine->storage->{filter_fetch_key}->($result)
        : $result;
}

sub first_key { (shift)->FIRSTKEY(@_) }
sub next_key  { (shift)->NEXTKEY(@_)  }

sub _clear {
    my $self = shift;

    while ( defined(my $key = $self->first_key) ) {
      do {
        $self->_engine->delete_key( $self, $key, $key );
      } while defined($key = $self->next_key($key));
    }

    return;
}

sub _copy_node {
    my $self = shift;
    my ($db_temp) = @_;

    my $key = $self->first_key();
    while (defined $key) {
        my $value = $self->get($key);
        $self->_copy_value( \$db_temp->{$key}, $value );
        $key = $self->next_key($key);
    }

    return 1;
}

1;
__END__
