use strict;

package Path::Class::Dir;
{
  $Path::Class::Dir::VERSION = '0.37';
}

use Path::Class::File;
use Carp();
use parent qw(Path::Class::Entity);

use IO::Dir ();
use File::Path ();
use File::Temp ();
use Scalar::Util ();

# updir & curdir on the local machine, for screening them out in
# children().  Note that they don't respect 'foreign' semantics.
my $Updir  = __PACKAGE__->_spec->updir;
my $Curdir = __PACKAGE__->_spec->curdir;

sub new {
  my $self = shift->SUPER::new();

  # If the only arg is undef, it's probably a mistake.  Without this
  # special case here, we'd return the root directory, which is a
  # lousy thing to do to someone when they made a mistake.  Return
  # undef instead.
  return if @_==1 && !defined($_[0]);

  my $s = $self->_spec;
  
  my $first = (@_ == 0     ? $s->curdir :
	       !ref($_[0]) && $_[0] eq '' ? (shift, $s->rootdir) :
	       shift()
	      );
  
  $self->{dirs} = [];
  if ( Scalar::Util::blessed($first) && $first->isa("Path::Class::Dir") ) {
    $self->{volume} = $first->{volume};
    push @{$self->{dirs}}, @{$first->{dirs}};
  }
  else {
    ($self->{volume}, my $dirs) = $s->splitpath( $s->canonpath("$first") , 1);
    push @{$self->{dirs}}, $dirs eq $s->rootdir ? "" : $s->splitdir($dirs);
  }

  push @{$self->{dirs}}, map {
    Scalar::Util::blessed($_) && $_->isa("Path::Class::Dir")
      ? @{$_->{dirs}}
      : $s->splitdir( $s->canonpath($_) )
  } @_;


  return $self;
}

sub file_class { "Path::Class::File" }

sub is_dir { 1 }

sub as_foreign {
  my ($self, $type) = @_;

  my $foreign = do {
    local $self->{file_spec_class} = $self->_spec_class($type);
    $self->SUPER::new;
  };
  
  # Clone internal structure
  $foreign->{volume} = $self->{volume};
  my ($u, $fu) = ($self->_spec->updir, $foreign->_spec->updir);
  $foreign->{dirs} = [ map {$_ eq $u ? $fu : $_} @{$self->{dirs}}];
  return $foreign;
}

sub stringify {
  my $self = shift;
  my $s = $self->_spec;
  return $s->catpath($self->{volume},
		     $s->catdir(@{$self->{dirs}}),
		     '');
}

sub volume { shift()->{volume} }

sub file {
  local $Path::Class::Foreign = $_[0]->{file_spec_class} if $_[0]->{file_spec_class};
  return $_[0]->file_class->new(@_);
}

sub basename { shift()->{dirs}[-1] }

sub dir_list {
  my $self = shift;
  my $d = $self->{dirs};
  return @$d unless @_;
  
  my $offset = shift;
  if ($offset < 0) { $offset = $#$d + $offset + 1 }
  
  return wantarray ? @$d[$offset .. $#$d] : $d->[$offset] unless @_;
  
  my $length = shift;
  if ($length < 0) { $length = $#$d + $length + 1 - $offset }
  return @$d[$offset .. $length + $offset - 1];
}

sub components {
  my $self = shift;
  return $self->dir_list(@_);
}

sub subdir {
  my $self = shift;
  return $self->new($self, @_);
}

sub parent {
  my $self = shift;
  my $dirs = $self->{dirs};
  my ($curdir, $updir) = ($self->_spec->curdir, $self->_spec->updir);

  if ($self->is_absolute) {
    my $parent = $self->new($self);
    pop @{$parent->{dirs}} if @$dirs > 1;
    return $parent;

  } elsif ($self eq $curdir) {
    return $self->new($updir);

  } elsif (!grep {$_ ne $updir} @$dirs) {  # All updirs
    return $self->new($self, $updir); # Add one more

  } elsif (@$dirs == 1) {
    return $self->new($curdir);

  } else {
    my $parent = $self->new($self);
    pop @{$parent->{dirs}};
    return $parent;
  }
}

sub relative {
  # File::Spec->abs2rel before version 3.13 returned the empty string
  # when the two paths were equal - work around it here.
  my $self = shift;
  my $rel = $self->_spec->abs2rel($self->stringify, @_);
  return $self->new( length $rel ? $rel : $self->_spec->curdir );
}

sub open  { IO::Dir->new(@_) }
sub mkpath { File::Path::mkpath(shift()->stringify, @_) }
sub rmtree { File::Path::rmtree(shift()->stringify, @_) }

sub remove {
  rmdir( shift() );
}

sub traverse {
  my $self = shift;
  my ($callback, @args) = @_;
  my @children = $self->children;
  return $self->$callback(
    sub {
      my @inner_args = @_;
      return map { $_->traverse($callback, @inner_args) } @children;
    },
    @args
  );
}

sub traverse_if {
  my $self = shift;
  my ($callback, $condition, @args) = @_;
  my @children = grep { $condition->($_) } $self->children;
  return $self->$callback(
    sub {
      my @inner_args = @_;
      return map { $_->traverse_if($callback, $condition, @inner_args) } @children;
    },
    @args
  );
}

sub recurse {
  my $self = shift;
  my %opts = (preorder => 1, depthfirst => 0, @_);
  
  my $callback = $opts{callback}
    or Carp::croak( "Must provide a 'callback' parameter to recurse()" );
  
  my @queue = ($self);
  
  my $visit_entry;
  my $visit_dir = 
    $opts{depthfirst} && $opts{preorder}
    ? sub {
      my $dir = shift;
      my $ret = $callback->($dir);
      unless( ($ret||'') eq $self->PRUNE ) {
          unshift @queue, $dir->children;
      }
    }
    : $opts{preorder}
    ? sub {
      my $dir = shift;
      my $ret = $callback->($dir);
      unless( ($ret||'') eq $self->PRUNE ) {
          push @queue, $dir->children;
      }
    }
    : sub {
      my $dir = shift;
      $visit_entry->($_) foreach $dir->children;
      $callback->($dir);
    };
  
  $visit_entry = sub {
    my $entry = shift;
    if ($entry->is_dir) { $visit_dir->($entry) } # Will call $callback
    else { $callback->($entry) }
  };
  
  while (@queue) {
    $visit_entry->( shift @queue );
  }
}

sub children {
  my ($self, %opts) = @_;
  
  my $dh = $self->open or Carp::croak( "Can't open directory $self: $!" );
  
  my @out;
  while (defined(my $entry = $dh->read)) {
    next if !$opts{all} && $self->_is_local_dot_dir($entry);
    next if ($opts{no_hidden} && $entry =~ /^\./);
    push @out, $self->file($entry);
    $out[-1] = $self->subdir($entry) if -d $out[-1];
  }
  return @out;
}

sub _is_local_dot_dir {
  my $self = shift;
  my $dir  = shift;

  return ($dir eq $Updir or $dir eq $Curdir);
}

sub next {
  my $self = shift;
  unless ($self->{dh}) {
    $self->{dh} = $self->open or Carp::croak( "Can't open directory $self: $!" );
  }
  
  my $next = $self->{dh}->read;
  unless (defined $next) {
    delete $self->{dh};
    ## no critic
    return undef;
  }
  
  # Figure out whether it's a file or directory
  my $file = $self->file($next);
  $file = $self->subdir($next) if -d $file;
  return $file;
}

sub subsumes {
  Carp::croak "Too many arguments given to subsumes()" if $#_ > 2;
  my ($self, $other) = @_;
  Carp::croak( "No second entity given to subsumes()" ) unless defined $other;

  $other = $self->new($other) unless eval{$other->isa( "Path::Class::Entity")};
  $other = $other->dir unless $other->is_dir;

  if ($self->is_absolute) {
    $other = $other->absolute;
  } elsif ($other->is_absolute) {
    $self = $self->absolute;
  }

  $self = $self->cleanup;
  $other = $other->cleanup;

  if ($self->volume || $other->volume) {
    return 0 unless $other->volume eq $self->volume;
  }

  # The root dir subsumes everything (but ignore the volume because
  # we've already checked that)
  return 1 if "@{$self->{dirs}}" eq "@{$self->new('')->{dirs}}";

  # The current dir subsumes every relative path (unless starting with updir)
  if ($self eq $self->_spec->curdir) {
    return $other->{dirs}[0] ne $self->_spec->updir;
  }

  my $i = 0;
  while ($i <= $#{ $self->{dirs} }) {
    return 0 if $i > $#{ $other->{dirs} };
    return 0 if $self->{dirs}[$i] ne $other->{dirs}[$i];
    $i++;
  }
  return 1;
}

sub contains {
  Carp::croak "Too many arguments given to contains()" if $#_ > 2;
  my ($self, $other) = @_;
  Carp::croak "No second entity given to contains()" unless defined $other;
  return unless -d $self and (-e $other or -l $other);

  # We're going to resolve the path, and don't want side effects on the objects
  # so clone them.  This also handles strings passed as $other.
  $self= $self->new($self)->resolve;
  $other= $self->new($other)->resolve;
  
  return $self->subsumes($other);
}

sub tempfile {
  my $self = shift;
  return File::Temp::tempfile(@_, DIR => $self->stringify);
}

1;
__END__

=head1 NAME

Path::Class::Dir - Objects representing directories

=head1 VERSION

version 0.37

=head1 SYNOPSIS

  use Path::Class;  # Exports dir() by default
  
  my $dir = dir('foo', 'bar');       # Path::Class::Dir object
  my $dir = Path::Class::Dir->new('foo', 'bar');  # Same thing
  
  # Stringifies to 'foo/bar' on Unix, 'foo\bar' on Windows, etc.
  print "dir: $dir\n";
  
  if ($dir->is_absolute) { ... }
  if ($dir->is_relative) { ... }
  
  my $v = $dir->volume; # Could be 'C:' on Windows, empty string
                        # on Unix, 'Macintosh HD:' on Mac OS
  
  $dir->cleanup; # Perform logical cleanup of pathname
  $dir->resolve; # Perform physical cleanup of pathname
  
  my $file = $dir->file('file.txt'); # A file in this directory
  my $subdir = $dir->subdir('george'); # A subdirectory
  my $parent = $dir->parent; # The parent directory, 'foo'
  
  my $abs = $dir->absolute; # Transform to absolute path
  my $rel = $abs->relative; # Transform to relative path
  my $rel = $abs->relative('/foo'); # Relative to /foo
  
  print $dir->as_foreign('Mac');   # :foo:bar:
  print $dir->as_foreign('Win32'); #  foo\bar

  # Iterate with IO::Dir methods:
  my $handle = $dir->open;
  while (my $file = $handle->read) {
    $file = $dir->file($file);  # Turn into Path::Class::File object
    ...
  }
  
  # Iterate with Path::Class methods:
  while (my $file = $dir->next) {
    # $file is a Path::Class::File or Path::Class::Dir object
    ...
  }


=head1 DESCRIPTION

The C<Path::Class::Dir> class contains functionality for manipulating
directory names in a cross-platform way.

=head1 METHODS

=over 4

=item $dir = Path::Class::Dir->new( <dir1>, <dir2>, ... )

=item $dir = dir( <dir1>, <dir2>, ... )

Creates a new C<Path::Class::Dir> object and returns it.  The
arguments specify names of directories which will be joined to create
a single directory object.  A volume may also be specified as the
first argument, or as part of the first argument.  You can use
platform-neutral syntax:

  my $dir = dir( 'foo', 'bar', 'baz' );

or platform-native syntax:

  my $dir = dir( 'foo/bar/baz' );

or a mixture of the two:

  my $dir = dir( 'foo/bar', 'baz' );

All three of the above examples create relative paths.  To create an
absolute path, either use the platform native syntax for doing so:

  my $dir = dir( '/var/tmp' );

or use an empty string as the first argument:

  my $dir = dir( '', 'var', 'tmp' );

If the second form seems awkward, that's somewhat intentional - paths
like C</var/tmp> or C<\Windows> aren't cross-platform concepts in the
first place (many non-Unix platforms don't have a notion of a "root
directory"), so they probably shouldn't appear in your code if you're
trying to be cross-platform.  The first form is perfectly natural,
because paths like this may come from config files, user input, or
whatever.

As a special case, since it doesn't otherwise mean anything useful and
it's convenient to define this way, C<< Path::Class::Dir->new() >> (or
C<dir()>) refers to the current directory (C<< File::Spec->curdir >>).
To get the current directory as an absolute path, do C<<
dir()->absolute >>.

Finally, as another special case C<dir(undef)> will return undef,
since that's usually an accident on the part of the caller, and
returning the root directory would be a nasty surprise just asking for
trouble a few lines later.

=item $dir->stringify

This method is called internally when a C<Path::Class::Dir> object is
used in a string context, so the following are equivalent:

  $string = $dir->stringify;
  $string = "$dir";

=item $dir->volume

Returns the volume (e.g. C<C:> on Windows, C<Macintosh HD:> on Mac OS,
etc.) of the directory object, if any.  Otherwise, returns the empty
string.

=item $dir->basename

Returns the last directory name of the path as a string.

=item $dir->is_dir

Returns a boolean value indicating whether this object represents a
directory.  Not surprisingly, L<Path::Class::File> objects always
return false, and C<Path::Class::Dir> objects always return true.

=item $dir->is_absolute

Returns true or false depending on whether the directory refers to an
absolute path specifier (like C</usr/local> or C<\Windows>).

=item $dir->is_relative

Returns true or false depending on whether the directory refers to a
relative path specifier (like C<lib/foo> or C<./dir>).

=item $dir->cleanup

Performs a logical cleanup of the file path.  For instance:

  my $dir = dir('/foo//baz/./foo')->cleanup;
  # $dir now represents '/foo/baz/foo';

=item $dir->resolve

Performs a physical cleanup of the file path.  For instance:

  my $dir = dir('/foo//baz/../foo')->resolve;
  # $dir now represents '/foo/foo', assuming no symlinks

This actually consults the filesystem to verify the validity of the
path.

=item $file = $dir->file( <dir1>, <dir2>, ..., <file> )

Returns a L<Path::Class::File> object representing an entry in C<$dir>
or one of its subdirectories.  Internally, this just calls C<<
Path::Class::File->new( @_ ) >>.

=item $subdir = $dir->subdir( <dir1>, <dir2>, ... )

Returns a new C<Path::Class::Dir> object representing a subdirectory
of C<$dir>.

=item $parent = $dir->parent

Returns the parent directory of C<$dir>.  Note that this is the
I<logical> parent, not necessarily the physical parent.  It really
means we just chop off entries from the end of the directory list
until we cain't chop no more.  If the directory is relative, we start
using the relative forms of parent directories.

The following code demonstrates the behavior on absolute and relative
directories:

  $dir = dir('/foo/bar');
  for (1..6) {
    print "Absolute: $dir\n";
    $dir = $dir->parent;
  }
  
  $dir = dir('foo/bar');
  for (1..6) {
    print "Relative: $dir\n";
    $dir = $dir->parent;
  }
  
  ########### Output on Unix ################
  Absolute: /foo/bar
  Absolute: /foo
  Absolute: /
  Absolute: /
  Absolute: /
  Absolute: /
  Relative: foo/bar
  Relative: foo
  Relative: .
  Relative: ..
  Relative: ../..
  Relative: ../../..

=item @list = $dir->children

Returns a list of L<Path::Class::File> and/or C<Path::Class::Dir>
objects listed in this directory, or in scalar context the number of
such objects.  Obviously, it is necessary for C<$dir> to
exist and be readable in order to find its children.

Note that the children are returned as subdirectories of C<$dir>,
i.e. the children of F<foo> will be F<foo/bar> and F<foo/baz>, not
F<bar> and F<baz>.

Ordinarily C<children()> will not include the I<self> and I<parent>
entries C<.> and C<..> (or their equivalents on non-Unix systems),
because that's like I'm-my-own-grandpa business.  If you do want all
directory entries including these special ones, pass a true value for
the C<all> parameter:

  @c = $dir->children(); # Just the children
  @c = $dir->children(all => 1); # All entries

In addition, there's a C<no_hidden> parameter that will exclude all
normally "hidden" entries - on Unix this means excluding all entries
that begin with a dot (C<.>):

  @c = $dir->children(no_hidden => 1); # Just normally-visible entries


=item $abs = $dir->absolute

Returns a C<Path::Class::Dir> object representing C<$dir> as an
absolute path.  An optional argument, given as either a string or a
C<Path::Class::Dir> object, specifies the directory to use as the base
of relativity - otherwise the current working directory will be used.

=item $rel = $dir->relative

Returns a C<Path::Class::Dir> object representing C<$dir> as a
relative path.  An optional argument, given as either a string or a
C<Path::Class::Dir> object, specifies the directory to use as the base
of relativity - otherwise the current working directory will be used.

=item $boolean = $dir->subsumes($other)

Returns true if this directory spec subsumes the other spec, and false
otherwise.  Think of "subsumes" as "contains", but we only look at the
I<specs>, not whether C<$dir> actually contains C<$other> on the
filesystem.

The C<$other> argument may be a C<Path::Class::Dir> object, a
L<Path::Class::File> object, or a string.  In the latter case, we
assume it's a directory.

  # Examples:
  dir('foo/bar' )->subsumes(dir('foo/bar/baz'))  # True
  dir('/foo/bar')->subsumes(dir('/foo/bar/baz')) # True
  dir('foo/..')->subsumes(dir('foo/../bar))      # True
  dir('foo/bar' )->subsumes(dir('bar/baz'))      # False
  dir('/foo/bar')->subsumes(dir('foo/bar'))      # False
  dir('foo/..')->subsumes(dir('bar'))            # False! Use C<contains> to resolve ".."


=item $boolean = $dir->contains($other)

Returns true if this directory actually contains C<$other> on the
filesystem.  C<$other> doesn't have to be a direct child of C<$dir>,
it just has to be subsumed after both paths have been resolved.

=item $foreign = $dir->as_foreign($type)

Returns a C<Path::Class::Dir> object representing C<$dir> as it would
be specified on a system of type C<$type>.  Known types include
C<Unix>, C<Win32>, C<Mac>, C<VMS>, and C<OS2>, i.e. anything for which
there is a subclass of C<File::Spec>.

Any generated objects (subdirectories, files, parents, etc.) will also
retain this type.

=item $foreign = Path::Class::Dir->new_foreign($type, @args)

Returns a C<Path::Class::Dir> object representing C<$dir> as it would
be specified on a system of type C<$type>.  Known types include
C<Unix>, C<Win32>, C<Mac>, C<VMS>, and C<OS2>, i.e. anything for which
there is a subclass of C<File::Spec>.

The arguments in C<@args> are the same as they would be specified in
C<new()>.

=item @list = $dir->dir_list([OFFSET, [LENGTH]])

Returns the list of strings internally representing this directory
structure.  Each successive member of the list is understood to be an
entry in its predecessor's directory list.  By contract, C<<
Path::Class->new( $dir->dir_list ) >> should be equivalent to C<$dir>.

The semantics of this method are similar to Perl's C<splice> or
C<substr> functions; they return C<LENGTH> elements starting at
C<OFFSET>.  If C<LENGTH> is omitted, returns all the elements starting
at C<OFFSET> up to the end of the list.  If C<LENGTH> is negative,
returns the elements from C<OFFSET> onward except for C<-LENGTH>
elements at the end.  If C<OFFSET> is negative, it counts backward
C<OFFSET> elements from the end of the list.  If C<OFFSET> and
C<LENGTH> are both omitted, the entire list is returned.

In a scalar context, C<dir_list()> with no arguments returns the
number of entries in the directory list; C<dir_list(OFFSET)> returns
the single element at that offset; C<dir_list(OFFSET, LENGTH)> returns
the final element that would have been returned in a list context.

=item $dir->components

Identical to C<dir_list()>.  It exists because there's an analogous
method C<dir_list()> in the C<Path::Class::File> class that also
returns the basename string, so this method lets someone call
C<components()> without caring whether the object is a file or a
directory.

=item $fh = $dir->open()

Passes C<$dir> to C<< IO::Dir->open >> and returns the result as an
L<IO::Dir> object.  If the opening fails, C<undef> is returned and
C<$!> is set.

=item $dir->mkpath($verbose, $mode)

Passes all arguments, including C<$dir>, to C<< File::Path::mkpath()
>> and returns the result (a list of all directories created).

=item $dir->rmtree($verbose, $cautious)

Passes all arguments, including C<$dir>, to C<< File::Path::rmtree()
>> and returns the result (the number of files successfully deleted).

=item $dir->remove()

Removes the directory, which must be empty.  Returns a boolean value
indicating whether or not the directory was successfully removed.
This method is mainly provided for consistency with
C<Path::Class::File>'s C<remove()> method.

=item $dir->tempfile(...)

An interface to L<File::Temp>'s C<tempfile()> function.  Just like
that function, if you call this in a scalar context, the return value
is the filehandle and the file is C<unlink>ed as soon as possible
(which is immediately on Unix-like platforms).  If called in a list
context, the return values are the filehandle and the filename.

The given directory is passed as the C<DIR> parameter.

Here's an example of pretty good usage which doesn't allow race
conditions, won't leave yucky tempfiles around on your filesystem,
etc.:

  my $fh = $dir->tempfile;
  print $fh "Here's some data...\n";
  seek($fh, 0, 0);
  while (<$fh>) { do something... }

Or in combination with a C<fork>:

  my $fh = $dir->tempfile;
  print $fh "Here's some more data...\n";
  seek($fh, 0, 0);
  if ($pid=fork()) {
    wait;
  } else {
    something($_) while <$fh>;
  }


=item $dir_or_file = $dir->next()

A convenient way to iterate through directory contents.  The first
time C<next()> is called, it will C<open()> the directory and read the
first item from it, returning the result as a C<Path::Class::Dir> or
L<Path::Class::File> object (depending, of course, on its actual
type).  Each subsequent call to C<next()> will simply iterate over the
directory's contents, until there are no more items in the directory,
and then the undefined value is returned.  For example, to iterate
over all the regular files in a directory:

  while (my $file = $dir->next) {
    next unless -f $file;
    my $fh = $file->open('r') or die "Can't read $file: $!";
    ...
  }

If an error occurs when opening the directory (for instance, it
doesn't exist or isn't readable), C<next()> will throw an exception
with the value of C<$!>.

=item $dir->traverse( sub { ... }, @args )

Calls the given callback for the root, passing it a continuation
function which, when called, will call this recursively on each of its
children. The callback function should be of the form:

  sub {
    my ($child, $cont, @args) = @_;
    # ...
  }

For instance, to calculate the number of files in a directory, you
can do this:

  my $nfiles = $dir->traverse(sub {
    my ($child, $cont) = @_;
    return sum($cont->(), ($child->is_dir ? 0 : 1));
  });

or to calculate the maximum depth of a directory:

  my $depth = $dir->traverse(sub {
    my ($child, $cont, $depth) = @_;
    return max($cont->($depth + 1), $depth);
  }, 0);

You can also choose not to call the callback in certain situations:

  $dir->traverse(sub {
    my ($child, $cont) = @_;
    return if -l $child; # don't follow symlinks
    # do something with $child
    return $cont->();
  });

=item $dir->traverse_if( sub { ... }, sub { ... }, @args )

traverse with additional "should I visit this child" callback.
Particularly useful in case examined tree contains inaccessible
directories.

Canonical example:

  $dir->traverse_if(
    sub {
       my ($child, $cont) = @_;
       # do something with $child
       return $cont->();
    }, 
    sub {
       my ($child) = @_;
       # Process only readable items
       return -r $child;
    });

Second callback gets single parameter: child. Only children for
which it returns true will be processed by the first callback.

Remaining parameters are interpreted as in traverse, in particular
C<traverse_if(callback, sub { 1 }, @args> is equivalent to
C<traverse(callback, @args)>.

=item $dir->recurse( callback => sub {...} )

Iterates through this directory and all of its children, and all of
its children's children, etc., calling the C<callback> subroutine for
each entry.  This is a lot like what the L<File::Find> module does,
and of course C<File::Find> will work fine on L<Path::Class> objects,
but the advantage of the C<recurse()> method is that it will also feed
your callback routine C<Path::Class> objects rather than just pathname
strings.

The C<recurse()> method requires a C<callback> parameter specifying
the subroutine to invoke for each entry.  It will be passed the
C<Path::Class> object as its first argument.

C<recurse()> also accepts two boolean parameters, C<depthfirst> and
C<preorder> that control the order of recursion.  The default is a
preorder, breadth-first search, i.e. C<< depthfirst => 0, preorder => 1 >>.
At the time of this writing, all combinations of these two parameters
are supported I<except> C<< depthfirst => 0, preorder => 0 >>.

C<callback> is normally not required to return any value. If it
returns special constant C<Path::Class::Entity::PRUNE()> (more easily
available as C<< $item->PRUNE >>),  no children of analyzed
item will be analyzed (mostly as if you set C<$File::Find::prune=1>). Of course
pruning is available only in C<preorder>, in postorder return value
has no effect.

=item $st = $file->stat()

Invokes C<< File::stat::stat() >> on this directory and returns a
C<File::stat> object representing the result.

=item $st = $file->lstat()

Same as C<stat()>, but if C<$file> is a symbolic link, C<lstat()>
stats the link instead of the directory the link points to.

=item $class = $file->file_class()

Returns the class which should be used to create file objects.

Generally overridden whenever this class is subclassed.

=back

=head1 AUTHOR

Ken Williams, kwilliams@cpan.org

=head1 SEE ALSO

L<Path::Class>, L<Path::Class::File>, L<File::Spec>

=cut
