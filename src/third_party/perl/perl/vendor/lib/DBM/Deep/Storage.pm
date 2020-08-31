package DBM::Deep::Storage;

use 5.008_004;

use strict;
use warnings FATAL => 'all';

=head1 NAME

DBM::Deep::Storage - abstract base class for storage

=head2 flush()

This flushes the filehandle. This takes no parameters and returns nothing.

=cut

sub flush { die "flush must be implemented in a child class" }

=head2 is_writable()

This takes no parameters. It returns a boolean saying if this filehandle is
writable.

Taken from L<http://www.perlmonks.org/?node_id=691054/>.

=cut

sub is_writable { die "is_writable must be implemented in a child class" }

=head1 LOCKING

This is where the actual locking of the storage medium is performed.
Nested locking is supported.

B<NOTE>: It is unclear what will happen if a read lock is taken, then
a write lock is taken as a nested lock, then the write lock is released.

Currently, the only locking method supported is flock(1). This is a
whole-file lock. In the future, more granular locking may be supported.
The API for that is unclear right now.

The following methods manage the locking status. In all cases, they take
a L<DBM::Deep> object and returns nothing.

=over 4

=item * lock_exclusive( $obj )

Take a lock usable for writing.

=item * lock_shared( $obj )

Take a lock usable for reading.

=item * unlock( $obj )

Releases the last lock taken. If this is the outermost lock, then the
object is actually unlocked.

=back

=cut

sub lock_exclusive { die "lock_exclusive must be implemented in a child class" }
sub lock_shared { die "lock_shared must be implemented in a child class" }
sub unlock { die "unlock must be implemented in a child class" }

1;
__END__
