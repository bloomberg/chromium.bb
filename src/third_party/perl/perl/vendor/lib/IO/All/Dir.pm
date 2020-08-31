use strict; use warnings;
package IO::All::Dir;

use Scalar::Util 'blessed';
use File::Glob 'bsd_glob';
use IO::All::Filesys -base;
use IO::All -base;
use IO::Dir;

#===============================================================================
const type => 'dir';
option 'sort' => 1;
chain filter => undef;
option 'deep';
field 'chdir_from';

#===============================================================================
sub dir {
    my $self = shift;
    my $had_prev = blessed($self) && $self->pathname;

    bless $self, __PACKAGE__ unless $had_prev;
    if (@_ && @_ > 1 || @_ && $had_prev) {
       $self->name(
           $self->_spec_class->catdir(
               ($self->pathname ? ($self->pathname) : () ),
               @_,
           )
       )
    } elsif (@_) {
       $self->name($_[0])
    }
    return $self->_init;
}

sub dir_handle {
    my $self = shift;
    bless $self, __PACKAGE__;
    $self->_handle(shift) if @_;
    return $self->_init;
}

#===============================================================================
sub _assert_open {
    my $self = shift;
    return if $self->is_open;
    $self->open;
}

sub open {
    my $self = shift;
    $self->is_open(1);
    $self->_assert_dirpath($self->pathname)
      if $self->pathname and $self->_assert;
    my $handle = IO::Dir->new;
    $self->io_handle($handle);
    $handle->open($self->pathname)
      or $self->throw($self->open_msg);
    return $self;
}

sub open_msg {
    my $self = shift;
    my $name = defined $self->pathname
      ? " '" . $self->pathname . "'"
      : '';
    return qq{Can't open directory$name:\n$!};
}

sub exists { -d shift->pathname }

#===============================================================================
sub All {
    my $self = shift;
    $self->all(0);
}

sub all {
    my $self = shift;
    my $depth = @_ ? shift(@_) : $self->_deep ? 0 : 1;
    my $first = not @_;
    my @all;
    while (my $io = $self->next) {
        push @all, $io;
        push(@all, $io->all($depth - 1, 1))
          if $depth != 1 and $io->is_dir;
    }
    @all = grep {&{$self->filter}} @all
      if $self->filter;
    return @all unless $first and $self->_sort;
    return sort {$a->pathname cmp $b->pathname} @all;
}

sub All_Dirs {
    my $self = shift;
    $self->all_dirs(0);
}

sub all_dirs {
    my $self = shift;
    grep {$_->is_dir} $self->all(@_);
}

sub All_Files {
    my $self = shift;
    $self->all_files(0);
}

sub all_files {
    my $self = shift;
    grep {$_->is_file} $self->all(@_);
}

sub All_Links {
 my $self = shift;
 $self->all_links(0);
}

sub all_links {
    my $self = shift;
    grep {$_->is_link} $self->all(@_);
}

sub chdir {
    my $self = shift;
    require Cwd;
    $self->chdir_from(Cwd::cwd());
    CORE::chdir($self->pathname);
    return $self;
}

sub empty {
    my $self = shift;
    my $dh;
    opendir($dh, $self->pathname) or die;
    while (my $dir = readdir($dh)) {
       return 0 unless $dir =~ /^\.{1,2}$/;
    }
    return 1;
}

sub mkdir {
    my $self = shift;
    defined($self->perms)
    ? (CORE::mkdir($self->pathname, $self->perms) or die "mkdir failed: $!")
    : (CORE::mkdir($self->pathname) or die "mkdir failed: $!");
    return $self;
}

sub mkpath {
    my $self = shift;
    require File::Path;
    File::Path::mkpath($self->pathname, @_);
    return $self;
}

sub file {
    my ($self, @rest) = @_;

    return $self->_constructor->()->file($self->pathname, @rest)
}

sub next {
    my $self = shift;
    $self->_assert_open;
    my $name = $self->readdir;
    return unless defined $name;
    my $io = $self->_constructor->(File::Spec->catfile($self->pathname, $name));
    $io->absolute if $self->is_absolute;
    return $io;
}

sub readdir {
    my $self = shift;
    $self->_assert_open;
    if (wantarray) {
        my @return = grep {
            not /^\.{1,2}$/
        } $self->io_handle->read;
        $self->close;
        if ($self->_has_utf8) { utf8::decode($_) for (@return) }
        return @return;
    }
    my $name = '.';
    while ($name =~ /^\.{1,2}$/) {
        $name = $self->io_handle->read;
        unless (defined $name) {
            $self->close;
            return;
        }
    }
    if ($self->_has_utf8) { utf8::decode($name) }
    return $name;
}

sub rmdir {
    my $self = shift;
    rmdir $self->pathname;
}

sub rmtree {
    my $self = shift;
    require File::Path;
    File::Path::rmtree($self->pathname, @_);
}

sub glob {
   my ($self, @rest) = @_;

   map {;
      my $ret = $self->_constructor->($_);
      $ret->absolute if $self->is_absolute;
      $ret
   } bsd_glob $self->_spec_class->catdir( $self->pathname, @rest );
}

sub copy {
    my ($self, $new) = @_;

    require File::Copy::Recursive;

    File::Copy::Recursive::dircopy($self->name, $new)
        or die "failed to copy $self to $new: $!";
     $self->_constructor->($new)
}

sub DESTROY {
    my $self = shift;
    CORE::chdir($self->chdir_from)
      if $self->chdir_from;
      # $self->SUPER::DESTROY(@_);
}

#===============================================================================
sub _overload_table {
    (
        '${} dir' => '_overload_as_scalar',
        '@{} dir' => '_overload_as_array',
        '%{} dir' => '_overload_as_hash',
    )
}

sub _overload_as_scalar {
    \ $_[1];
}

sub _overload_as_array {
    [ $_[1]->all ];
}

sub _overload_as_hash {
    +{
        map {
            (my $name = $_->pathname) =~ s/.*[\/\\]//;
            ($name, $_);
        } $_[1]->all
    };
}

1;
