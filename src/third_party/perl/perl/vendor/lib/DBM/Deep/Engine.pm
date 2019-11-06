package DBM::Deep::Engine;

use 5.008_004;

use strict;
use warnings FATAL => 'all';
no warnings 'recursion';

use DBM::Deep::Iterator ();

# File-wide notes:
# * Every method in here assumes that the storage has been appropriately
#   safeguarded. This can be anything from flock() to some sort of manual
#   mutex. But, it's the caller's responsibility to make sure that this has
#   been done.

sub SIG_HASH     () { 'H' }
sub SIG_ARRAY    () { 'A' }

=head1 NAME

DBM::Deep::Engine - mediate mapping between DBM::Deep objects and storage medium

=head1 PURPOSE

This is an internal-use-only object for L<DBM::Deep>. It mediates the low-level
mapping between the L<DBM::Deep> objects and the storage medium.

The purpose of this documentation is to provide low-level documentation for
developers. It is B<not> intended to be used by the general public. This
documentation and what it documents can and will change without notice.

=head1 OVERVIEW

The engine exposes an API to the DBM::Deep objects (DBM::Deep, DBM::Deep::Array,
and DBM::Deep::Hash) for their use to access the actual stored values. This API
is the following:

=over 4

=item * new

=item * read_value

=item * get_classname

=item * make_reference

=item * key_exists

=item * delete_key

=item * write_value

=item * get_next_key

=item * setup

=item * clear

=item * begin_work

=item * commit

=item * rollback

=item * lock_exclusive

=item * lock_shared

=item * unlock

=back

They are explained in their own sections below. These methods, in turn, may
provide some bounds-checking, but primarily act to instantiate objects in the
Engine::Sector::* hierarchy and dispatch to them.

=head1 TRANSACTIONS

Transactions in DBM::Deep are implemented using a variant of MVCC. This attempts
to keep the amount of actual work done against the file low while still providing
Atomicity, Consistency, and Isolation. Durability, unfortunately, cannot be done
with only one file.

=head2 STALENESS

If another process uses a transaction slot and writes stuff to it, then
terminates, the data that process wrote is still within the file. In order to
address this, there is also a transaction staleness counter associated within
every write.  Each time a transaction is started, that process increments that
transaction's staleness counter. If, when it reads a value, the staleness
counters aren't identical, DBM::Deep will consider the value on disk to be stale
and discard it.

=head2 DURABILITY

The fourth leg of ACID is Durability, the guarantee that when a commit returns,
the data will be there the next time you read from it. This should be regardless
of any crashes or powerdowns in between the commit and subsequent read.
DBM::Deep does provide that guarantee; once the commit returns, all of the data
has been transferred from the transaction shadow to the HEAD. The issue arises
with partial commits - a commit that is interrupted in some fashion. In keeping
with DBM::Deep's "tradition" of very light error-checking and non-existent
error-handling, there is no way to recover from a partial commit. (This is
probably a failure in Consistency as well as Durability.)

Other DBMSes use transaction logs (a separate file, generally) to achieve
Durability.  As DBM::Deep is a single-file, we would have to do something
similar to what SQLite and BDB do in terms of committing using synchronized
writes. To do this, we would have to use a much higher RAM footprint and some
serious programming that makes my head hurt just to think about it.

=cut

=head1 METHODS

=head2 read_value( $obj, $key )

This takes an object that provides _base_offset() and a string. It returns the
value stored in the corresponding Sector::Value's data section.

=cut

sub read_value { die "read_value must be implemented in a child class" }

=head2 get_classname( $obj )

This takes an object that provides _base_offset() and returns the classname (if
any) associated with it.

It delegates to Sector::Reference::get_classname() for the heavy lifting.

It performs a staleness check.

=cut

sub get_classname { die "get_classname must be implemented in a child class" }

=head2 make_reference( $obj, $old_key, $new_key )

This takes an object that provides _base_offset() and two strings. The
strings correspond to the old key and new key, respectively. This operation
is equivalent to (given C<< $db->{foo} = []; >>) C<< $db->{bar} = $db->{foo} >>.

This returns nothing.

=cut

sub make_reference { die "make_reference must be implemented in a child class" }

=head2 key_exists( $obj, $key )

This takes an object that provides _base_offset() and a string for
the key to be checked. This returns 1 for true and "" for false.

=cut

sub key_exists { die "key_exists must be implemented in a child class" }

=head2 delete_key( $obj, $key )

This takes an object that provides _base_offset() and a string for
the key to be deleted. This returns the result of the Sector::Reference
delete_key() method.

=cut

sub delete_key { die "delete_key must be implemented in a child class" }

=head2 write_value( $obj, $key, $value )

This takes an object that provides _base_offset(), a string for the
key, and a value. This value can be anything storable within L<DBM::Deep>.

This returns 1 upon success.

=cut

sub write_value { die "write_value must be implemented in a child class" }

=head2 setup( $obj )

This takes an object that provides _base_offset(). It will do everything needed
in order to properly initialize all values for necessary functioning. If this is
called upon an already initialized object, this will also reset the inode.

This returns 1.

=cut

sub setup { die "setup must be implemented in a child class" }

=head2 begin_work( $obj )

This takes an object that provides _base_offset(). It will set up all necessary
bookkeeping in order to run all work within a transaction.

If $obj is already within a transaction, an error will be thrown. If there are
no more available transactions, an error will be thrown.

This returns undef.

=cut

sub begin_work { die "begin_work must be implemented in a child class" }

=head2 rollback( $obj )

This takes an object that provides _base_offset(). It will revert all
actions taken within the running transaction.

If $obj is not within a transaction, an error will be thrown.

This returns 1.

=cut

sub rollback { die "rollback must be implemented in a child class" }

=head2 commit( $obj )

This takes an object that provides _base_offset(). It will apply all
actions taken within the transaction to the HEAD.

If $obj is not within a transaction, an error will be thrown.

This returns 1.

=cut

sub commit { die "commit must be implemented in a child class" }

=head2 get_next_key( $obj, $prev_key )

This takes an object that provides _base_offset() and an optional string
representing the prior key returned via a prior invocation of this method.

This method delegates to C<< DBM::Deep::Iterator->get_next_key() >>.

=cut

# XXX Add staleness here
sub get_next_key {
    my $self = shift;
    my ($obj, $prev_key) = @_;

    # XXX Need to add logic about resetting the iterator if any key in the
    # reference has changed
    unless ( defined $prev_key ) {
        eval "use " . $self->iterator_class; die $@ if $@;
        $obj->{iterator} = $self->iterator_class->new({
            base_offset => $obj->_base_offset,
            engine      => $self,
        });
    }

    return $obj->{iterator}->get_next_key( $obj );
}

=head2 lock_exclusive()

This takes an object that provides _base_offset(). It will guarantee that
the storage has taken precautions to be safe for a write.

This returns nothing.

=cut

sub lock_exclusive {
    my $self = shift;
    my ($obj) = @_;
    return $self->storage->lock_exclusive( $obj );
}

=head2 lock_shared()

This takes an object that provides _base_offset(). It will guarantee that
the storage has taken precautions to be safe for a read.

This returns nothing.

=cut

sub lock_shared {
    my $self = shift;
    my ($obj) = @_;
    return $self->storage->lock_shared( $obj );
}

=head2 unlock()

This takes an object that provides _base_offset(). It will guarantee that
the storage has released the most recently-taken lock.

This returns nothing.

=cut

sub unlock {
    my $self = shift;
    my ($obj) = @_;

    my $rv = $self->storage->unlock( $obj );

    $self->flush if $rv;

    return $rv;
}

=head1 INTERNAL METHODS

The following methods are internal-use-only to DBM::Deep::Engine and its
child classes.

=cut

=head2 flush()

This takes no arguments. It will do everything necessary to flush all things to
disk. This is usually called during unlock() and setup().

This returns nothing.

=cut

sub flush {
    my $self = shift;

    # Why do we need to have the storage flush? Shouldn't autoflush take care of
    # things? -RobK, 2008-06-26
    $self->storage->flush;

    return;
}

=head2 load_sector( $loc )

This takes an id/location/offset and loads the sector based on the engine's
defined sector type.

=cut

sub load_sector { $_[0]->sector_type->load( @_ ) }

=head2 clear( $obj )

This takes an object that provides _base_offset() and deletes all its 
elements, returning nothing.

=cut

sub clear { die "clear must be implemented in a child class" }

=head2 cache / clear_cache

This is the cache of loaded Reference sectors.

=cut

sub cache       { $_[0]{cache} ||= {} }
sub clear_cache { %{$_[0]->cache} = () }

=head2 supports( $option )

This returns a boolean depending on if this instance of DBM::Dep supports
that feature. C<$option> can be one of:

=over 4

=item * transactions

=item * singletons

=back

Any other value will return false.

=cut

sub supports { die "supports must be implemented in a child class" }

=head1 ACCESSORS

The following are readonly attributes.

=over 4

=item * storage

=item * sector_type

=item * iterator_class

=back

=cut

sub storage { $_[0]{storage} }

sub sector_type { die "sector_type must be implemented in a child class" }
sub iterator_class { die "iterator_class must be implemented in a child class" }

# This code is to make sure we write all the values in the $value to the
# disk and to make sure all changes to $value after the assignment are
# reflected on disk. This may be counter-intuitive at first, but it is
# correct dwimmery.
#   NOTE - simply tying $value won't perform a STORE on each value. Hence,
# the copy to a temp value.
sub _descend {
    my $self = shift;
    my ($value, $value_sector) = @_;
    my $r = Scalar::Util::reftype( $value ) || '';

    if ( $r eq 'ARRAY' ) {
        my @temp = @$value;
        tie @$value, 'DBM::Deep', {
            base_offset => $value_sector->offset,
            staleness   => $value_sector->staleness,
            storage     => $self->storage,
            engine      => $self,
        };
        @$value = @temp;
        bless $value, 'DBM::Deep::Array' unless Scalar::Util::blessed( $value );
    }
    elsif ( $r eq 'HASH' ) {
        my %temp = %$value;
        tie %$value, 'DBM::Deep', {
            base_offset => $value_sector->offset,
            staleness   => $value_sector->staleness,
            storage     => $self->storage,
            engine      => $self,
        };
        %$value = %temp;
        bless $value, 'DBM::Deep::Hash' unless Scalar::Util::blessed( $value );
    }

    return;
}

1;
__END__
