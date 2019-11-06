use strict; use warnings;
package IO::All::Filesys;

use IO::All::Base -base;
use Fcntl qw(:flock);

my %spec_map = (
    unix  => 'Unix',
    win32 => 'Win32',
    vms   => 'VMS',
    mac   => 'Mac',
    os2   => 'OS2',
);
sub os {
    my ($self, $type) = @_;

    my ($v, $d, $f) = $self->_spec_class->splitpath($self->name);
    my @d = $self->_spec_class->splitdir($d);

    $self->_spec_class($spec_map{$type});

    $self->name( $self->_spec_class->catfile( @d, $f ) );

    return $self
}

sub exists { my $self = shift; -e $self->name }

sub filename {
    my $self = shift;
    my $filename;
    (undef, undef, $filename) = $self->splitpath;
    return $filename;
}

sub ext {
   my $self = shift;

   return $1 if $self->filename =~ m/\.([^\.]+)$/
}
{
    no warnings 'once';
    *extension = \&ext;
}

sub mimetype {
   require File::MimeInfo;
   return File::MimeInfo::mimetype($_[0]->filename)
}

sub is_absolute {
    my $self = shift;
    return *$self->{is_absolute} = shift if @_;
    return *$self->{is_absolute}
      if defined *$self->{is_absolute};
    *$self->{is_absolute} = IO::All::is_absolute($self) ? 1 : 0;
}

sub is_executable { my $self = shift; -x $self->name }
sub is_readable { my $self = shift; -r $self->name }
sub is_writable { my $self = shift; -w $self->name }
{
    no warnings 'once';
    *is_writeable = \&is_writable;
}

sub pathname {
    my $self = shift;
    return *$self->{pathname} = shift if @_;
    return *$self->{pathname} if defined *$self->{pathname};
    return $self->name;
}

sub relative {
    my $self = shift;
    if (my $base = $_[0]) {
       $self->pathname(File::Spec->abs2rel($self->pathname, $base))
    } elsif ($self->is_absolute) {
       $self->pathname(File::Spec->abs2rel($self->pathname))
    }
    $self->is_absolute(0);
    return $self;
}

sub rename {
    my $self = shift;
    my $new = shift;
    rename($self->name, "$new")
      ? UNIVERSAL::isa($new, 'IO::All')
        ? $new
        : $self->_constructor->($new)
      : undef;
}

sub set_lock {
    my $self = shift;
    return unless $self->_lock;
    my $io_handle = $self->io_handle;
    my $flag = $self->mode =~ /^>>?$/
    ? LOCK_EX
    : LOCK_SH;
    flock $io_handle, $flag;
}

sub stat {
    my $self = shift;
    return IO::All::stat($self, @_)
      if $self->is_open;
      CORE::stat($self->pathname);
}

sub touch {
    my $self = shift;
    $self->utime;
}

sub unlock {
    my $self = shift;
    flock $self->io_handle, LOCK_UN
      if $self->_lock;
}

sub utime {
    my $self = shift;
    my $atime = shift;
    my $mtime = shift;
    $atime = time unless defined $atime;
    $mtime = $atime unless defined $mtime;
    utime($atime, $mtime, $self->name);
    return $self;
}

1;
