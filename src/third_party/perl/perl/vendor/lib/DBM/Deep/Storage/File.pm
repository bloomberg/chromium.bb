package DBM::Deep::Storage::File;

use 5.008_004;

use strict;
use warnings FATAL => 'all';

use Fcntl qw( :DEFAULT :flock :seek );

use constant DEBUG => 0;

use base 'DBM::Deep::Storage';

=head1 NAME

DBM::Deep::Storage::File - mediate low-level interaction with storage mechanism

=head1 PURPOSE

This is an internal-use-only object for L<DBM::Deep>. It mediates the low-level
interaction with the storage mechanism.

Currently, the only storage mechanism supported is the file system.

=head1 OVERVIEW

This class provides an abstraction to the storage mechanism so that the Engine
(the only class that uses this class) doesn't have to worry about that.

=head1 METHODS

=head2 new( \%args )

=cut

sub new {
    my $class = shift;
    my ($args) = @_;

    my $self = bless {
        autobless          => 1,
        autoflush          => 1,
        end                => 0,
        fh                 => undef,
        file               => undef,
        file_offset        => 0,
        locking            => 1,
        locked             => 0,
#XXX Migrate this to the engine, where it really belongs.
        filter_store_key   => undef,
        filter_store_value => undef,
        filter_fetch_key   => undef,
        filter_fetch_value => undef,
    }, $class;

    # Grab the parameters we want to use
    foreach my $param ( keys %$self ) {
        next unless exists $args->{$param};
        $self->{$param} = $args->{$param};
    }

    if ( $self->{fh} && !$self->{file_offset} ) {
        $self->{file_offset} = tell( $self->{fh} );
    }

    $self->open unless $self->{fh};

    return $self;
}

=head2 open()

This method opens the filehandle for the filename in C< file >. 

There is no return value.

=cut

# TODO: What happens if we ->open when we already have a $fh?
sub open {
    my $self = shift;

    # Adding O_BINARY should remove the need for the binmode below. However,
    # I'm not going to remove it because I don't have the Win32 chops to be
    # absolutely certain everything will be ok.
    my $flags = O_CREAT | O_BINARY;

    if ( !-e $self->{file} || -w _ ) {
      $flags |= O_RDWR;
    }
    else {
      $flags |= O_RDONLY;
    }

    my $fh;
    sysopen( $fh, $self->{file}, $flags )
        or die "DBM::Deep: Cannot sysopen file '$self->{file}': $!\n";
    $self->{fh} = $fh;

    # Even though we use O_BINARY, better be safe than sorry.
    binmode $fh;

    if ($self->{autoflush}) {
        my $old = select $fh;
        $|=1;
        select $old;
    }

    return 1;
}

=head2 close()

If the filehandle is opened, this will close it.

There is no return value.

=cut

sub close {
    my $self = shift;

    if ( $self->{fh} ) {
        close $self->{fh};
        $self->{fh} = undef;
    }

    return 1;
}

=head2 size()

This will return the size of the DB. If file_offset is set, this will take that into account.

B<NOTE>: This function isn't used internally anywhere.

=cut

sub size {
    my $self = shift;

    return 0 unless $self->{fh};
    return( (-s $self->{fh}) - $self->{file_offset} );
}

=head2 set_inode()

This will set the inode value of the underlying file object.

This is only needed to handle some obscure Win32 bugs. It really shouldn't be
needed outside this object.

There is no return value.

=cut

sub set_inode {
    my $self = shift;

    unless ( defined $self->{inode} ) {
        my @stats = stat($self->{fh});
        $self->{inode} = $stats[1];
        $self->{end} = $stats[7];
    }

    return 1;
}

=head2 print_at( $offset, @data )

This takes an optional offset and some data to print.

C< $offset >, if defined, will be used to seek into the file. If file_offset is
set, it will be used as the zero location. If it is undefined, no seeking will
occur. Then, C< @data > will be printed to the current location.

There is no return value.

=cut

sub print_at {
    my $self = shift;
    my $loc  = shift;

    local ($,,$\);

    my $fh = $self->{fh};
    if ( defined $loc ) {
        seek( $fh, $loc + $self->{file_offset}, SEEK_SET );
    }

    if ( DEBUG ) {
        my $caller = join ':', (caller)[0,2];
        my $len = length( join '', @_ );
        warn "($caller) print_at( " . (defined $loc ? $loc : '<undef>') . ", $len )\n";
    }

    print( $fh @_ ) or die "Internal Error (print_at($loc)): $!\n";

    return 1;
}

=head2 read_at( $offset, $length )

This takes an optional offset and a length.

C< $offset >, if defined, will be used to seek into the file. If file_offset is
set, it will be used as the zero location. If it is undefined, no seeking will
occur. Then, C< $length > bytes will be read from the current location.

The data read will be returned.

=cut

sub read_at {
    my $self = shift;
    my ($loc, $size) = @_;

    my $fh = $self->{fh};
    if ( defined $loc ) {
        seek( $fh, $loc + $self->{file_offset}, SEEK_SET );
    }

    if ( DEBUG ) {
        my $caller = join ':', (caller)[0,2];
        warn "($caller) read_at( " . (defined $loc ? $loc : '<undef>') . ", $size )\n";
    }

    my $buffer;
    read( $fh, $buffer, $size);

    return $buffer;
}

=head2 DESTROY

When the ::Storage::File object goes out of scope, it will be closed.

=cut

sub DESTROY {
    my $self = shift;
    return unless $self;

    $self->close;

    return;
}

=head2 request_space( $size )

This takes a size and adds that much space to the DBM.

This returns the offset for the new location.

=cut

sub request_space {
    my $self = shift;
    my ($size) = @_;

    #XXX Do I need to reset $self->{end} here? I need a testcase
    my $loc = $self->{end};
    $self->{end} += $size;

    return $loc;
}

=head2 copy_stats( $target_filename )

This will take the stats for the current filehandle and apply them to
C< $target_filename >. The stats copied are:

=over 4

=item * Onwer UID and GID

=item * Permissions

=back

=cut

sub copy_stats {
    my $self = shift;
    my ($temp_filename) = @_;

    my @stats = stat( $self->{fh} );
    my $perms = $stats[2] & 07777;
    my $uid = $stats[4];
    my $gid = $stats[5];
    chown( $uid, $gid, $temp_filename );
    chmod( $perms, $temp_filename );
}

sub flush {
    my $self = shift;

    # Flush the filehandle
    my $old_fh = select $self->{fh};
    my $old_af = $|; $| = 1; $| = $old_af;
    select $old_fh;

    return 1;
}

sub is_writable {
    my $self = shift;

    my $fh = $self->{fh};
    return unless defined $fh;
    return unless defined fileno $fh;
    local $\ = '';  # just in case
    no warnings;    # temporarily disable warnings
    local $^W;      # temporarily disable warnings
    return print $fh '';
}

sub lock_exclusive {
    my $self = shift;
    my ($obj) = @_;
    return $self->_lock( $obj, LOCK_EX );
}

sub lock_shared {
    my $self = shift;
    my ($obj) = @_;
    return $self->_lock( $obj, LOCK_SH );
}

sub _lock {
    my $self = shift;
    my ($obj, $type) = @_;

    $type = LOCK_EX unless defined $type;

    #XXX This is a temporary fix for Win32 and autovivification. It
    # needs to improve somehow. -RobK, 2008-03-09
    if ( $^O eq 'MSWin32' || $^O eq 'cygwin' ) {
        $type = LOCK_EX;
    }

    if (!defined($self->{fh})) { return; }

    #XXX This either needs to allow for upgrading a shared lock to an
    # exclusive lock or something else with autovivification.
    # -RobK, 2008-03-09
    if ($self->{locking}) {
        if (!$self->{locked}) {
            flock($self->{fh}, $type);

            # refresh end counter in case file has changed size
            my @stats = stat($self->{fh});
            $self->{end} = $stats[7];

            # double-check file inode, in case another process
            # has optimize()d our file while we were waiting.
            if (defined($self->{inode}) && $stats[1] != $self->{inode}) {
                $self->close;
                $self->open;

                #XXX This needs work
                $obj->{engine}->setup( $obj );

                flock($self->{fh}, $type); # re-lock

                # This may not be necessary after re-opening
                $self->{end} = (stat($self->{fh}))[7]; # re-end
            }
        }
        $self->{locked}++;

        return 1;
    }

    return;
}

sub unlock {
    my $self = shift;

    if (!defined($self->{fh})) { return; }

    if ($self->{locking} && $self->{locked} > 0) {
        $self->{locked}--;

        if (!$self->{locked}) {
            flock($self->{fh}, LOCK_UN);
            return 1;
        }

        return;
    }

    return;
}

1;
__END__
