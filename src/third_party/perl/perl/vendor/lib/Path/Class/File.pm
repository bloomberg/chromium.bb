use strict;

package Path::Class::File;
{
  $Path::Class::File::VERSION = '0.37';
}

use Path::Class::Dir;
use parent qw(Path::Class::Entity);
use Carp;

use IO::File ();

sub new {
  my $self = shift->SUPER::new;
  my $file = pop();
  my @dirs = @_;

  my ($volume, $dirs, $base) = $self->_spec->splitpath($file);

  if (length $dirs) {
    push @dirs, $self->_spec->catpath($volume, $dirs, '');
  }

  $self->{dir}  = @dirs ? $self->dir_class->new(@dirs) : undef;
  $self->{file} = $base;

  return $self;
}

sub dir_class { "Path::Class::Dir" }

sub as_foreign {
  my ($self, $type) = @_;
  local $Path::Class::Foreign = $self->_spec_class($type);
  my $foreign = ref($self)->SUPER::new;
  $foreign->{dir} = $self->{dir}->as_foreign($type) if defined $self->{dir};
  $foreign->{file} = $self->{file};
  return $foreign;
}

sub stringify {
  my $self = shift;
  return $self->{file} unless defined $self->{dir};
  return $self->_spec->catfile($self->{dir}->stringify, $self->{file});
}

sub dir {
  my $self = shift;
  return $self->{dir} if defined $self->{dir};
  return $self->dir_class->new($self->_spec->curdir);
}
BEGIN { *parent = \&dir; }

sub volume {
  my $self = shift;
  return '' unless defined $self->{dir};
  return $self->{dir}->volume;
}

sub components {
  my $self = shift;
  croak "Arguments are not currently supported by File->components()" if @_;
  return ($self->dir->components, $self->basename);
}

sub basename { shift->{file} }
sub open  { IO::File->new(@_) }

sub openr { $_[0]->open('r') or croak "Can't read $_[0]: $!"  }
sub openw { $_[0]->open('w') or croak "Can't write to $_[0]: $!" }
sub opena { $_[0]->open('a') or croak "Can't append to $_[0]: $!" }

sub touch {
  my $self = shift;
  if (-e $self) {
    utime undef, undef, $self;
  } else {
    $self->openw;
  }
}

sub slurp {
  my ($self, %args) = @_;
  my $iomode = $args{iomode} || 'r';
  my $fh = $self->open($iomode) or croak "Can't read $self: $!";

  if (wantarray) {
    my @data = <$fh>;
    chomp @data if $args{chomped} or $args{chomp};

    if ( my $splitter = $args{split} ) {
      @data = map { [ split $splitter, $_ ] } @data;
    }

    return @data;
  }


  croak "'split' argument can only be used in list context"
    if $args{split};


  if ($args{chomped} or $args{chomp}) {
    chomp( my @data = <$fh> );
    return join '', @data;
  }


  local $/;
  return <$fh>;
}

sub spew {
    my $self = shift;
    my %args = splice( @_, 0, @_-1 );

    my $iomode = $args{iomode} || 'w';
    my $fh = $self->open( $iomode ) or croak "Can't write to $self: $!";

    if (ref($_[0]) eq 'ARRAY') {
        # Use old-school for loop to avoid copying.
        for (my $i = 0; $i < @{ $_[0] }; $i++) {
            print $fh $_[0]->[$i]
                or croak "Can't write to $self: $!";
        }
    }
    else {
        print $fh $_[0]
            or croak "Can't write to $self: $!";
    }

    close $fh
        or croak "Can't write to $self: $!";

    return;
}

sub spew_lines {
    my $self = shift;
    my %args = splice( @_, 0, @_-1 );

    my $content = $_[0];

    # If content is an array ref, appends $/ to each element of the array.
    # Otherwise, if it is a simple scalar, just appends $/ to that scalar.

    $content
        = ref( $content ) eq 'ARRAY'
        ? [ map { $_, $/ } @$content ]
        : "$content$/";

    return $self->spew( %args, $content );
}

sub remove {
  my $file = shift->stringify;
  return unlink $file unless -e $file; # Sets $! correctly
  1 while unlink $file;
  return not -e $file;
}

sub copy_to {
  my ($self, $dest) = @_;
  if ( eval{ $dest->isa("Path::Class::File")} ) { 
    $dest = $dest->stringify;
    croak "Can't copy to file $dest: it is a directory" if -d $dest;
  } elsif ( eval{ $dest->isa("Path::Class::Dir") } ) {
    $dest = $dest->stringify;
    croak "Can't copy to directory $dest: it is a file" if -f $dest;
    croak "Can't copy to directory $dest: no such directory" unless -d $dest;
  } elsif ( ref $dest ) {
    croak "Don't know how to copy files to objects of type '".ref($self)."'";
  }

  require Perl::OSType;
  if ( !Perl::OSType::is_os_type('Unix') ) {

      require File::Copy;
      return unless File::Copy::cp($self->stringify, "${dest}");

  } else {

      return unless (system('cp', $self->stringify, "${dest}") == 0);

  }

  return $self->new($dest);
}

sub move_to {
  my ($self, $dest) = @_;
  require File::Copy;
  if (File::Copy::move($self->stringify, "${dest}")) {

      my $new = $self->new($dest);

      $self->{$_} = $new->{$_} foreach (qw/ dir file /);

      return $self;

  } else {

      return;

  }
}

sub traverse {
  my $self = shift;
  my ($callback, @args) = @_;
  return $self->$callback(sub { () }, @args);
}

sub traverse_if {
  my $self = shift;
  my ($callback, $condition, @args) = @_;
  return $self->$callback(sub { () }, @args);
}

1;
__END__

=head1 NAME

Path::Class::File - Objects representing files

=head1 VERSION

version 0.37

=head1 SYNOPSIS

  use Path::Class;  # Exports file() by default

  my $file = file('foo', 'bar.txt');  # Path::Class::File object
  my $file = Path::Class::File->new('foo', 'bar.txt'); # Same thing

  # Stringifies to 'foo/bar.txt' on Unix, 'foo\bar.txt' on Windows, etc.
  print "file: $file\n";

  if ($file->is_absolute) { ... }
  if ($file->is_relative) { ... }

  my $v = $file->volume; # Could be 'C:' on Windows, empty string
                         # on Unix, 'Macintosh HD:' on Mac OS

  $file->cleanup; # Perform logical cleanup of pathname
  $file->resolve; # Perform physical cleanup of pathname

  my $dir = $file->dir;  # A Path::Class::Dir object

  my $abs = $file->absolute; # Transform to absolute path
  my $rel = $file->relative; # Transform to relative path

=head1 DESCRIPTION

The C<Path::Class::File> class contains functionality for manipulating
file names in a cross-platform way.

=head1 METHODS

=over 4

=item $file = Path::Class::File->new( <dir1>, <dir2>, ..., <file> )

=item $file = file( <dir1>, <dir2>, ..., <file> )

Creates a new C<Path::Class::File> object and returns it.  The
arguments specify the path to the file.  Any volume may also be
specified as the first argument, or as part of the first argument.
You can use platform-neutral syntax:

  my $file = file( 'foo', 'bar', 'baz.txt' );

or platform-native syntax:

  my $file = file( 'foo/bar/baz.txt' );

or a mixture of the two:

  my $file = file( 'foo/bar', 'baz.txt' );

All three of the above examples create relative paths.  To create an
absolute path, either use the platform native syntax for doing so:

  my $file = file( '/var/tmp/foo.txt' );

or use an empty string as the first argument:

  my $file = file( '', 'var', 'tmp', 'foo.txt' );

If the second form seems awkward, that's somewhat intentional - paths
like C</var/tmp> or C<\Windows> aren't cross-platform concepts in the
first place, so they probably shouldn't appear in your code if you're
trying to be cross-platform.  The first form is perfectly fine,
because paths like this may come from config files, user input, or
whatever.

=item $file->stringify

This method is called internally when a C<Path::Class::File> object is
used in a string context, so the following are equivalent:

  $string = $file->stringify;
  $string = "$file";

=item $file->volume

Returns the volume (e.g. C<C:> on Windows, C<Macintosh HD:> on Mac OS,
etc.) of the object, if any.  Otherwise, returns the empty string.

=item $file->basename

Returns the name of the file as a string, without the directory
portion (if any).

=item $file->components

Returns a list of the directory components of this file, followed by
the basename.

Note: unlike C<< $dir->components >>, this method currently does not
accept any arguments to select which elements of the list will be
returned.  It may do so in the future.  Currently it throws an
exception if such arguments are present.


=item $file->is_dir

Returns a boolean value indicating whether this object represents a
directory.  Not surprisingly, C<Path::Class::File> objects always
return false, and L<Path::Class::Dir> objects always return true.

=item $file->is_absolute

Returns true or false depending on whether the file refers to an
absolute path specifier (like C</usr/local/foo.txt> or C<\Windows\Foo.txt>).

=item $file->is_relative

Returns true or false depending on whether the file refers to a
relative path specifier (like C<lib/foo.txt> or C<.\Foo.txt>).

=item $file->cleanup

Performs a logical cleanup of the file path.  For instance:

  my $file = file('/foo//baz/./foo.txt')->cleanup;
  # $file now represents '/foo/baz/foo.txt';

=item $dir->resolve

Performs a physical cleanup of the file path.  For instance:

  my $file = file('/foo/baz/../foo.txt')->resolve;
  # $file now represents '/foo/foo.txt', assuming no symlinks

This actually consults the filesystem to verify the validity of the
path.

=item $dir = $file->dir

Returns a C<Path::Class::Dir> object representing the directory
containing this file.

=item $dir = $file->parent

A synonym for the C<dir()> method.

=item $abs = $file->absolute

Returns a C<Path::Class::File> object representing C<$file> as an
absolute path.  An optional argument, given as either a string or a
L<Path::Class::Dir> object, specifies the directory to use as the base
of relativity - otherwise the current working directory will be used.

=item $rel = $file->relative

Returns a C<Path::Class::File> object representing C<$file> as a
relative path.  An optional argument, given as either a string or a
C<Path::Class::Dir> object, specifies the directory to use as the base
of relativity - otherwise the current working directory will be used.

=item $foreign = $file->as_foreign($type)

Returns a C<Path::Class::File> object representing C<$file> as it would
be specified on a system of type C<$type>.  Known types include
C<Unix>, C<Win32>, C<Mac>, C<VMS>, and C<OS2>, i.e. anything for which
there is a subclass of C<File::Spec>.

Any generated objects (subdirectories, files, parents, etc.) will also
retain this type.

=item $foreign = Path::Class::File->new_foreign($type, @args)

Returns a C<Path::Class::File> object representing a file as it would
be specified on a system of type C<$type>.  Known types include
C<Unix>, C<Win32>, C<Mac>, C<VMS>, and C<OS2>, i.e. anything for which
there is a subclass of C<File::Spec>.

The arguments in C<@args> are the same as they would be specified in
C<new()>.

=item $fh = $file->open($mode, $permissions)

Passes the given arguments, including C<$file>, to C<< IO::File->new >>
(which in turn calls C<< IO::File->open >> and returns the result
as an L<IO::File> object.  If the opening
fails, C<undef> is returned and C<$!> is set.

=item $fh = $file->openr()

A shortcut for

 $fh = $file->open('r') or croak "Can't read $file: $!";

=item $fh = $file->openw()

A shortcut for

 $fh = $file->open('w') or croak "Can't write to $file: $!";

=item $fh = $file->opena()

A shortcut for

 $fh = $file->open('a') or croak "Can't append to $file: $!";

=item $file->touch

Sets the modification and access time of the given file to right now,
if the file exists.  If it doesn't exist, C<touch()> will I<make> it
exist, and - YES! - set its modification and access time to now.

=item $file->slurp()

In a scalar context, returns the contents of C<$file> in a string.  In
a list context, returns the lines of C<$file> (according to how C<$/>
is set) as a list.  If the file can't be read, this method will throw
an exception.

If you want C<chomp()> run on each line of the file, pass a true value
for the C<chomp> or C<chomped> parameters:

  my @lines = $file->slurp(chomp => 1);

You may also use the C<iomode> parameter to pass in an IO mode to use
when opening the file, usually IO layers (though anything accepted by
the MODE argument of C<open()> is accepted here).  Just make sure it's
a I<reading> mode.

  my @lines = $file->slurp(iomode => ':crlf');
  my $lines = $file->slurp(iomode => '<:encoding(UTF-8)');

The default C<iomode> is C<r>.

Lines can also be automatically split, mimicking the perl command-line
option C<-a> by using the C<split> parameter. If this parameter is used,
each line will be returned as an array ref.

    my @lines = $file->slurp( chomp => 1, split => qr/\s*,\s*/ );

The C<split> parameter can only be used in a list context.

=item $file->spew( $content );

The opposite of L</slurp>, this takes a list of strings and prints them
to the file in write mode.  If the file can't be written to, this method
will throw an exception.

The content to be written can be either an array ref or a plain scalar.
If the content is an array ref then each entry in the array will be
written to the file.

You may use the C<iomode> parameter to pass in an IO mode to use when
opening the file, just like L</slurp> supports.

  $file->spew(iomode => '>:raw', $content);

The default C<iomode> is C<w>.

=item $file->spew_lines( $content );

Just like C<spew>, but, if $content is a plain scalar, appends $/
to it, or, if $content is an array ref, appends $/ to each element
of the array.

Can also take an C<iomode> parameter like C<spew>. Again, the
default C<iomode> is C<w>.

=item $file->traverse(sub { ... }, @args)

Calls the given callback on $file. This doesn't do much on its own,
but see the associated documentation in L<Path::Class::Dir>.

=item $file->remove()

This method will remove the file in a way that works well on all
platforms, and returns a boolean value indicating whether or not the
file was successfully removed.

C<remove()> is better than simply calling Perl's C<unlink()> function,
because on some platforms (notably VMS) you actually may need to call
C<unlink()> several times before all versions of the file are gone -
the C<remove()> method handles this process for you.

=item $st = $file->stat()

Invokes C<< File::stat::stat() >> on this file and returns a
L<File::stat> object representing the result.

=item $st = $file->lstat()

Same as C<stat()>, but if C<$file> is a symbolic link, C<lstat()>
stats the link instead of the file the link points to.

=item $class = $file->dir_class()

Returns the class which should be used to create directory objects.

Generally overridden whenever this class is subclassed.

=item $copy = $file->copy_to( $dest );

Copies the C<$file> to C<$dest>. It returns a L<Path::Class::File>
object when successful, C<undef> otherwise.

=item $moved = $file->move_to( $dest );

Moves the C<$file> to C<$dest>, and updates C<$file> accordingly.

It returns C<$file> is successful, C<undef> otherwise.

=back

=head1 AUTHOR

Ken Williams, kwilliams@cpan.org

=head1 SEE ALSO

L<Path::Class>, L<Path::Class::Dir>, L<File::Spec>

=cut
