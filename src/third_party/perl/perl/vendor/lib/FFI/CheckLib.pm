package FFI::CheckLib;

use strict;
use warnings;
use File::Spec;
use Carp qw( croak carp );
use base qw( Exporter );

our @EXPORT = qw(
  find_lib
  assert_lib
  check_lib
  check_lib_or_exit
  find_lib_or_exit
  find_lib_or_die
);

our @EXPORT_OK = qw(
  which
  where
  has_symbols
);

# ABSTRACT: Check that a library is available for FFI
our $VERSION = '0.24'; # VERSION


our $system_path = [];
our $os ||= $^O;
my $try_ld_on_text = 0;

if($os eq 'MSWin32' || $os eq 'msys')
{
  $system_path = eval {
    require Env;
    Env->import('@PATH');
    \our @PATH;
  };
  die $@ if $@;
}
else
{
  $system_path = eval {
    require DynaLoader;
    \@DynaLoader::dl_library_path;
  };
  die $@ if $@;
}

our $pattern = [ qr{^lib(.*?)\.so(?:\.([0-9]+(?:\.[0-9]+)*))?$} ];
our $version_split = qr/\./;

if($os eq 'cygwin')
{
  push @$pattern, qr{^cyg(.*?)(?:-([0-9])+)?\.dll$};
}
elsif($os eq 'msys')
{
  # doesn't seem as though msys uses psudo libfoo.so files
  # in the way that cygwin sometimes does.  we can revisit
  # this if we find otherwise.
  $pattern = [ qr{^msys-(.*?)(?:-([0-9])+)?\.dll$} ];
}
elsif($os eq 'MSWin32')
{
  #  handle cases like libgeos-3-7-0___.dll and libgtk-2.0-0.dll
  $pattern = [ qr{^(?:lib)?(\w+?)(?:-([0-9-\.]+))?_*\.dll$}i ];
  $version_split = qr/\-/;
}
elsif($os eq 'darwin')
{
  push @$pattern, qr{^lib(.*?)(?:\.([0-9]+(?:\.[0-9]+)*))?\.(?:dylib|bundle)$};
}
elsif($os eq 'linux')
{
  if(-e '/etc/redhat-release' && -x '/usr/bin/ld')
  {
    $try_ld_on_text = 1;
  }
}

sub _matches
{
  my($filename, $path) = @_;

  foreach my $regex (@$pattern)
  {
    return [
      $1,                                            # 0    capture group 1 library name
      File::Spec->catfile($path, $filename),         # 1    full path to library
      defined $2 ? (split $version_split, $2) : (),  # 2... capture group 2 library version
    ] if $filename =~ $regex;
  }
  return ();
}

sub _cmp
{
  my($A,$B) = @_;

  return $A->[0] cmp $B->[0] if $A->[0] ne $B->[0];

  my $i=2;
  while(1)
  {
    return 0  if !defined($A->[$i]) && !defined($B->[$i]);
    return -1 if !defined $A->[$i];
    return 1  if !defined $B->[$i];
    return $B->[$i] <=> $A->[$i] if $A->[$i] != $B->[$i];
    $i++;
  }
}


my $diagnostic;

sub _is_binary
{
  -B $_[0]
}

sub find_lib
{
  my(%args) = @_;

  undef $diagnostic;
  croak "find_lib requires lib argument" unless defined $args{lib};

  my $recursive = $args{_r} || $args{recursive} || 0;

  # make arguments be lists.
  foreach my $arg (qw( lib libpath symbol verify ))
  {
    next if ref $args{$arg} eq 'ARRAY';
    if(defined $args{$arg})
    {
      $args{$arg} = [ $args{$arg} ];
    }
    else
    {
      $args{$arg} = [];
    }
  }

  if(defined $args{systempath} && !ref($args{systempath}))
  {
    $args{systempath} = [ $args{systempath} ];
  }

  my @path = @{ $args{libpath} };
  @path = map { _recurse($_) } @path if $recursive;
  push @path, grep { defined } defined $args{systempath}
    ? @{ $args{systempath} }
    : @$system_path;

  my $any = 1 if grep { $_ eq '*' } @{ $args{lib} };
  my %missing = map { $_ => 1 } @{ $args{lib} };
  my %symbols = map { $_ => 1 } @{ $args{symbol} };
  my @found;

  delete $missing{'*'};

  foreach my $path (@path)
  {
    next unless -d $path;
    my $dh;
    opendir $dh, $path;
    my @maybe =
      # make determinist based on names and versions
      sort { _cmp($a,$b) }
      # Filter out the items that do not match the name that we are looking for
      # Filter out any broken symbolic links
      grep { ($any || $missing{$_->[0]} ) && (-e $_->[1]) }
      # get [ name, full_path ] mapping,
      # each entry is a 2 element list ref
      map { _matches($_,$path) }
      # read all files from the directory
      readdir $dh;
    closedir $dh;

    if($try_ld_on_text && $args{try_linker_script})
    {
      # This is tested in t/ci.t only
      @maybe = map {
        -B $_->[1] ? $_ : do {
          my($name, $so) = @$_;
          my $output = `/usr/bin/ld -t $so -o /dev/null -shared`;
          $output =~ /\((.*?lib.*\.so.*?)\)/
            ? [$name, $1]
            : die "unable to parse ld output";
        }
      } @maybe;
    }

    midloop:
    foreach my $lib (@maybe)
    {
      next unless $any || $missing{$lib->[0]};

      foreach my $verify (@{ $args{verify} })
      {
        next midloop unless $verify->(@$lib);
      }

      delete $missing{$lib->[0]};

      if(%symbols)
      {
        require DynaLoader;
        my $dll = DynaLoader::dl_load_file($lib->[1],0);
        foreach my $symbol (keys %symbols)
        {
          if(DynaLoader::dl_find_symbol($dll, $symbol) ? 1 : 0)
          {
            delete $symbols{$symbol}
          }
        }
        DynaLoader::dl_unload_file($dll);
      }

      my $found = $lib->[1];

      unless($any)
      {
        while(-l $found)
        {
          require File::Basename;
          require File::Spec;
          my $dir = File::Basename::dirname($found);
          $found = File::Spec->rel2abs( readlink($found), $dir );
        }
      }

      push @found, $found;
    }
  }

  if(%missing)
  {
    my @missing = sort keys %missing;
    if(@missing > 1)
    { $diagnostic = "libraries not found: @missing" }
    else
    { $diagnostic = "library not found: @missing" }
  }
  elsif(%symbols)
  {
    my @missing = sort keys %symbols;
    if(@missing > 1)
    { $diagnostic = "symbols not found: @missing" }
    else
    { $diagnostic = "symbol not found: @missing" }
  }

  return if %symbols;
  return $found[0] unless wantarray;
  return @found;
}

sub _recurse
{
  my($dir) = @_;
  return unless -d $dir;
  my $dh;
  opendir $dh, $dir;
  my @list = grep { -d $_ } map { File::Spec->catdir($dir, $_) } grep !/^\.\.?$/, readdir $dh;
  closedir $dh;
  ($dir, map { _recurse($_) } @list);
}


sub assert_lib
{
  croak $diagnostic || 'library not found' unless check_lib(@_);
}


sub check_lib_or_exit
{
  unless(check_lib(@_))
  {
    carp $diagnostic || 'library not found';
    exit;
  }
}


sub find_lib_or_exit
{
  my(@libs) = find_lib(@_);
  unless(@libs)
  {
    carp $diagnostic || 'library not found';
    exit;
  }
  return unless @libs;
  wantarray ? @libs : $libs[0];
}


sub find_lib_or_die
{
  my(@libs) = find_lib(@_);
  unless(@libs)
  {
    croak $diagnostic || 'library not found';
  }
  return unless @libs;
  wantarray ? @libs : $libs[0];
}


sub check_lib
{
  find_lib(@_) ? 1 : 0;
}


sub which
{
  my($name) = @_;
  croak("cannot which *") if $name eq '*';
  scalar find_lib( lib => $name );
}


sub where
{
  my($name) = @_;
  $name eq '*'
    ? find_lib(lib => '*')
    : find_lib(lib => '*', verify => sub { $_[0] eq $name });
}


sub has_symbols
{
  my($path, @symbols) = @_;
  require DynaLoader;
  my $dll = DynaLoader::dl_load_file($path, 0);

  my $ok = 1;

  foreach my $symbol (@symbols)
  {
    unless(DynaLoader::dl_find_symbol($dll, $symbol))
    {
      $ok = 0;
      last;
    }
  }

  DynaLoader::dl_unload_file($dll);

  $ok;
}


sub system_path
{
  $system_path;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

FFI::CheckLib - Check that a library is available for FFI

=head1 VERSION

version 0.24

=head1 SYNOPSIS

 use FFI::CheckLib;
 
 check_lib_or_exit( lib => 'jpeg', symbol => 'jinit_memory_mgr' );
 check_lib_or_exit( lib => [ 'iconv', 'jpeg' ] );
 
 # or prompt for path to library and then:
 print "where to find jpeg library: ";
 my $path = <STDIN>;
 check_lib_or_exit( lib => 'jpeg', libpath => $path );

=head1 DESCRIPTION

This module checks whether a particular dynamic library is available for
FFI to use. It is modeled heavily on L<Devel::CheckLib>, but will find
dynamic libraries even when development packages are not installed.  It
also provides a L<find_lib|FFI::CheckLib#find_lib> function that will
return the full path to the found dynamic library, which can be feed
directly into L<FFI::Platypus> or another FFI system.

Although intended mainly for FFI modules via L<FFI::Platypus> and
similar, this module does not actually use any FFI to do its detection
and probing.  This module does not have any non-core runtime dependencies.
The test suite does depend on L<Test2::Suite>.

=head1 FUNCTIONS

All of these take the same named parameters and are exported by default.

=head2 find_lib

 my(@libs) = find_lib(%args);

This will return a list of dynamic libraries, or empty list if none were
found.

[version 0.05]

If called in scalar context it will return the first library found.

Arguments are key value pairs with these keys:

=over 4

=item lib

Must be either a string with the name of a single library or a reference
to an array of strings of library names.  Depending on your platform,
C<CheckLib> will prepend C<lib> or append C<.dll> or C<.so> when
searching.

[version 0.11]

As a special case, if C<*> is specified then any libs found will match.

=item libpath

A string or array of additional paths to search for libraries.

=item systempath

[version 0.11]

A string or array of system paths to search for instead of letting
L<FFI::CheckLib> determine the system path.  You can set this to C<[]>
in order to not search I<any> system paths.

=item symbol

A string or a list of symbol names that must be found.

=item verify

A code reference used to verify a library really is the one that you
want.  It should take two arguments, which is the name of the library
and the full path to the library pathname.  It should return true if it
is acceptable, and false otherwise.  You can use this in conjunction
with L<FFI::Platypus> to determine if it is going to meet your needs.
Example:

 use FFI::CheckLib;
 use FFI::Platypus;
 
 my($lib) = find_lib(
   lib => 'foo',
   verify => sub {
     my($name, $libpath) = @_;
     
     my $ffi = FFI::Platypus->new;
     $ffi->lib($libpath);
     
     my $f = $ffi->function('foo_version', [] => 'int');
     
     return $f->call() >= 500; # we accept version 500 or better
   },
 );

=item recursive

[version 0.11]

Recursively search for libraries in any non-system paths (those provided
via C<libpath> above).

=item try_linker_script

[version 0.24]

Some vendors provide C<.so> files that are linker scripts that point to
the real binary shared library.  These linker scripts can be used by gcc
or clang, but are not directly usable by L<FFI::Platypus> and friends.
On select platforms, this options will use the linker command (C<ld>)
to attempt to resolve the real C<.so> for non-binary files.  Since there
is extra overhead this is off by default.

An example is libyaml on RedHat based Linux distributions.  On Debian
these are handled with symlinks and no trickery is required.

=back

=head2 assert_lib

 assert_lib(%args);

This behaves exactly the same as L<find_lib|FFI::CheckLib#find_lib>,
except that instead of returning empty list of failure it throws an
exception.

=head2 check_lib_or_exit

 check_lib_or_exit(%args);

This behaves exactly the same as L<assert_lib|FFI::CheckLib#assert_lib>,
except that instead of dying, it warns (with exactly the same error
message) and exists.  This is intended for use in C<Makefile.PL> or
C<Build.PL>

=head2 find_lib_or_exit

[version 0.05]

 my(@libs) = find_lib_or_exit(%args);

This behaves exactly the same as L<find_lib|FFI::CheckLib#find_lib>,
except that if the library is not found, it will call exit with an
appropriate diagnostic.

=head2 find_lib_or_die

[version 0.06]

 my(@libs) = find_lib_or_die(%args);

This behaves exactly the same as L<find_lib|FFI::CheckLib#find_lib>,
except that if the library is not found, it will die with an appropriate
diagnostic.

=head2 check_lib

 my $bool = check_lib(%args);

This behaves exactly the same as L<find_lib|FFI::CheckLib#find_lib>,
except that it returns true (1) on finding the appropriate libraries or
false (0) otherwise.

=head2 which

[version 0.17]

 my $path = where($name);

Return the path to the first library that matches the given name.

Not exported by default.

=head2 where

[version 0.17]

 my @paths = where($name);

Return the paths to all the libraries that match the given name.

Not exported by default.

=head2 has_symbols

[version 0.17]

 my $bool = has_symbols($path, @symbol_names);

Returns true if I<all> of the symbols can be found in the dynamic library located
at the given path.  Can be useful in conjunction with C<verify> with C<find_lib>
above.

Not exported by default.

=head2 system_path

[version 0.20]

 my $path = FFI::CheckLib::system_path;

Returns the system path as a list reference.  On some systems, this is C<PATH>
on others it might be C<LD_LIBRARY_PATH> on still others it could be something
completely different.  So although you I<may> add items to this list, you should
probably do some careful consideration before you do so.

This function is not exportable, even on request.

=head1 SEE ALSO

=over 4

=item L<FFI::Platypus>

Call library functions dynamically without a compiler.

=item L<Dist::Zilla::Plugin::FFI::CheckLib>

L<Dist::Zilla> plugin for this module.

=back

=head1 AUTHOR

Author: Graham Ollis E<lt>plicease@cpan.orgE<gt>

Contributors:

Bakkiaraj Murugesan (bakkiaraj)

Dan Book (grinnz, DBOOK)

Ilya Pavlov (Ilya, ILUX)

Shawn Laffan (SLAFFAN)

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2014-2018 by Graham Ollis.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
