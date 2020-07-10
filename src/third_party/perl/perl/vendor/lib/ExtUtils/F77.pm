package ExtUtils::F77;

use strict;
use warnings;
use Config;
use File::Spec;
use Text::ParseWords;
use File::Which qw(which);
use List::Util qw(first);

our $VERSION = "1.23";
our $DEBUG;

sub debug { return if !$DEBUG; warn @_ }

print "Loaded ExtUtils::F77 version $VERSION\n";

my %F77config=();
my $Runtime = "-LSNAFU -lwontwork";
my $RuntimeOK = 0;
my $Trail_ = 1;
my $Pkg = "";
my $Compiler = "";
my $Cflags = "";
my ($gcc, $gfortran, $fallback_compiler);

# all the compilers and their libraries
my %COMPLIBS = (
  g77 => [qw/ g2c f2c /],
  f77 => [qw/ g2c f2c /],
  fort77 => [qw/ g2c f2c /],
  gfortran => [qw/ gfortran /],
  g95 => [qw/ f95 /],
);

########## Win32 Specific ##############

if ($^O =~ /MSWin/i) {
   my @version;
   if ($Config{cc} =~ /x86_64\-w64\-mingw32\-gcc/) {
      # This must be gcc-4.x.x
      $gcc = 'x86_64-w64-mingw32-gcc';
      $gfortran = 'x86_64-w64-mingw32-gfortran';
      $fallback_compiler = 'GFortran';
   }
   elsif ($Config{gccversion}) {
      # Different fortran compiler for gcc-4.x.x (and later) versus gcc-3.x.x
      $gcc = 'gcc';
      @version = split /\./, $Config{gccversion};
      $fallback_compiler = $version[0] >= 4 ? 'GFortran' : 'G77';
      $gfortran = 'gfortran';
   }
   else {
      $gcc = 'gcc';
      $gfortran = 'gfortran';
      $fallback_compiler = 'G77';
   }
}
else {
   # No change from version 1.16.
   $gcc = 'gcc';
   $gfortran = 'gfortran';
   $fallback_compiler = 'G77';
}

############## End of Win32 Specific ##############

# Database starts here. Basically we have a large hash specifying
# entries for each os/compiler combination. Entries can be code refs
# in which case they are executed and the returned value used. This
# allows us to be quite smart.

# Hash key convention is uppercase first letter of
# hash keys. First key is usually the name of the architecture as
# returned by Config (modulo ucfirst()).

# Format is: OS, then compiler-family, then specific keys:

# DEFAULT: as a compiler-family, gives name of default compiler-family - corresponds to one of the keys

# Link: Code to figure out and return link-string for this architecture
# Returns false if it can't find anything sensible.

# Trail_: Whether symbols (subroutine names etc.) have trailing underscores
# (true/false)

# Compiler: Program to run to actually compile stuff

# Cflags: Associated compiler flags

sub gfortran_make_linkline {
  my ($dir, $lib, $defaultdir, $defaultlib, $append) = @_;
  $dir ||= $defaultdir;
  $lib ||= $defaultlib;
  $append ||= '';
  return( qq{"-L$dir" $append -L/usr/lib -l$lib -lm} );
}

sub gfortran_find_libdir {
  my ($compiler, $lib) = @_;
  for my $suffix (qw(a so)) {
    my $filename = "lib$lib.$suffix";
    my $dir = `$compiler -print-file-name=$filename`;
    chomp $dir;
    # Note that -print-file-name returns just the library name
    # if it cant be found - make sure that we only accept the
    # directory if it returns a proper path (or matches a /)
    next if !defined $dir or $dir eq $lib;
    $dir =~ s,/$filename$,,;
    return $dir;
  }
}

$F77config{MinGW}{G77}{Link} = sub {
   my @libs = ('g2c', 'f2c');
   my ($dir, $lib, $test);
   foreach $test (@libs) {
      $dir = gfortran_find_libdir('g77', $test);
      $lib = $test, last if defined $dir;
   }
   gfortran_make_linkline($dir, $lib, "/usr/local/lib", "f2c");
};
$F77config{MinGW}{G77}{Trail_} = 1;
$F77config{MinGW}{G77}{Compiler} = find_in_path('g77','f77','fort77');
$F77config{MinGW}{G77}{Cflags} = '-O';

$F77config{MinGW}{GFortran}{Link} = sub {
  my $dir = gfortran_find_libdir($gfortran, 'gfortran');
  gfortran_make_linkline($dir, "gfortran", "/usr/local/lib", "", '-lquadmath');
};
$F77config{MinGW}{GFortran}{Trail_} = 1;
$F77config{MinGW}{GFortran}{Compiler} = "$gfortran";
$F77config{MinGW}{GFortran}{Cflags}   = '-O';

### SunOS (use this as a template for new entries) ###

$F77config{Sunos}{F77}{Link} = sub {
   my $dir = find_highest_SC("/usr/lang/SC*");
   return "" unless $dir; # Failure
   debug "$Pkg: Found Fortran latest version lib dir $dir\n";
   return qq{"-L$dir" -lF77 -lm};
};
$F77config{Sunos}{F77}{Trail_} = 1;
$F77config{Sunos}{F77}{Compiler} = 'f77';
$F77config{Sunos}{F77}{Cflags} = '-O';

$F77config{Sunos}{DEFAULT} = 'F77';

############ Rest of database is here ############

### Solaris ###

$F77config{Solaris}{F77}{Link} = sub {
   my $NSPATH;
   my $dir;

   #find the SUNWspro entry of nonstandard inst. in LD_LIBRARY_PATH
   if ( defined $ENV{'LD_LIBRARY_PATH'} &&
      $ENV{'LD_LIBRARY_PATH'} =~ m{([^:]*SUNWspro[^/]*)} )
   {
      $NSPATH = $1;
   }

   elsif ( defined $ENV{'PATH'} ) {

      foreach ( split (/:/,$ENV{PATH}) ) {
         if ( m{(.*SUNWspro[^/]*)} ) {
            $NSPATH = $1;
            last;
         }
      }
   }

   if (defined $NSPATH) {
      debug "$Pkg: Found F77 path:--->$NSPATH\n";

      $dir = find_highest_SC("$NSPATH/WS*/lib") ||
      find_highest_SC("$NSPATH/SC*/lib") ||
      find_highest_SC("$NSPATH/sunwspro*/SUNWspro/SC*/lib");

      # that failed.  try $NSPATH/lib. use glob to
      # match for libF77.*, as libF77.a isn't there anymore?
      unless ( $dir )
      {
         debug "$Pkg: Trying $NSPATH/lib\n";
         $dir = "$NSPATH/lib" if glob("$NSPATH/lib/libF77*");
      }
   }
   return "" unless $dir; # Failure
   debug "$Pkg: Found Fortran latest version lib dir $dir\n";

   my @libs;

   # determine libraries. F77 and M77 aren't available in the latest
   # compilers
   my $vcruft = qx/$F77config{Solaris}{F77}{Compiler} -V 2>&1/;
   if ( $vcruft =~ /Forte Developer 7/
      || $vcruft =~ /f90:/
   )
   {
      push @libs, qw/
      -lf77compat
      -lfui
      -lfai
      -lfai2
      -lfsumai
      -lfprodai
      -lfminlai
      -lfmaxlai
      -lfminvai
      -lfmaxvai
      -lfsu
      -lsunmath
      -lm
      /;
   }
   else
   {
      push @libs, qw/
      -lF77
      -lM77
      -lsunmath
      -lm
      /;
   }

   join( ' ', qq{"-L$dir"}, @libs );
};
$F77config{Solaris}{F77}{Trail_} = 1;
$F77config{Solaris}{F77}{Compiler} = 'f77';
$F77config{Solaris}{F77}{Cflags} = '-O';

$F77config{Solaris}{DEFAULT} = 'F77';

### Generic GNU-77 or F2C system ###

$F77config{Generic}{GNU}{Trail_} = 1;
$F77config{Generic}{GNU}{Cflags} = ' ';        # <---need this space!
$F77config{Generic}{GNU}{Link}   = link_gnufortran_compiler('gfortran', 'g77', 'g95', 'fort77');
$F77config{Generic}{GNU}{Compiler} = find_in_path("$gfortran", 'g77',  'g95','fort77');

$F77config{Generic}{DEFAULT}     = 'GNU';

### cygwin ###

$F77config{Cygwin}{GNU}{Trail_} = 1;
$F77config{Cygwin}{GNU}{Cflags} = '-O';        # <---need this space!
$F77config{Cygwin}{GNU}{Link}   = link_gnufortran_compiler('g77', 'gfortran', 'g95', 'fort77');
$F77config{Cygwin}{GNU}{Compiler} = find_in_path('g77', "$gfortran", 'g95','fort77');

$F77config{Cygwin}{DEFAULT}     = 'GNU';

### Linux ###

$F77config{Linux}{GNU}     = $F77config{Generic}{GNU};

$F77config{Linux}{DEFAULT} = 'GNU';

### DEC OSF/1 ###

$F77config{Dec_osf}{F77}{Link}   = "-L/usr/lib -lUfor -lfor -lFutil -lm -lots -lc";
$F77config{Dec_osf}{F77}{Trail_} = 1;
$F77config{Dec_osf}{F77}{Compiler} = 'f77';
$F77config{Dec_osf}{F77}{Cflags} = '-O';

$F77config{Dec_osf}{DEFAULT}     = 'F77';

### HP/UX ###

$F77config{Hpux}{F77}{Link}   = "-L/usr/lib -lcl -lm";
$F77config{Hpux}{F77}{Trail_} = 0;
$F77config{Hpux}{F77}{Compiler} = 'f77';
$F77config{Hpux}{F77}{Cflags} = '-O';

$F77config{Hpux}{DEFAULT}     = 'F77';

### IRIX ###

# From: Ovidiu Toader <ovi@physics.utoronto.ca>
# For an SGI running IRIX 6.4 or higher (probably lower than 6.4 also)
# there is a new abi, -64, which produces 64 bit executables. This is no
# longer an experimental feature and I am using it exclusively without any
# problem. The code below is what I use instead of original IRIX section
# in the ExtUtils::F77 package. It adds the -64 flag and it is supposed to
# provide the same functionality as the old code for a non -64 abi.

if (ucfirst($Config{'osname'}) eq "Irix")
{
   my ($cflags,$mips,$default_abi,$abi,$mips_dir,$libs);
   $cflags = $Config{cc};
   ($mips) = ($cflags =~ /(-mips\d)/g);
   $mips = "" if ! defined($mips);
   $mips_dir = $mips;$mips_dir =~ s/-//g;
   $default_abi = $Config{osvers} >= 6.4 ? "-n32" : "-o32";
   GET_ABI:
   {
      # -32 seems to be synonymous for -o32 (CS)
      $abi = "-o32",last GET_ABI if $cflags =~ /-o?32/;
      $abi = "-n32",last GET_ABI if $cflags =~ /-n32/;
      $abi = "-64",last GET_ABI if $cflags =~ /-64/;
      $abi = $default_abi;
   }
   if ( $abi eq "-64" ){
      $libs = ( (-r "/usr/lib64/$mips_dir") && (-d _) && (-x _) ) ?
      "-L/usr/lib64/$mips_dir" : "";
      $libs .=  " -L/usr/lib64 -lfortran -lm";
   }
   if ( $abi eq "-n32" ){
      $libs = ( (-r "/usr/lib32/$mips_dir") && (-d _) && (-x _) ) ?
      "-L/usr/lib32/$mips_dir" : "";
      $libs .=  " -L/usr/lib32 -lfortran -lm";
   }
   if ( $abi eq "-o32" ){
      $libs = "-L/usr/lib -lF77 -lI77 -lU77 -lisam -lm";
   }
   $F77config{Irix}{F77}{Cflags}   = "$abi $mips";
   $F77config{Irix}{F77}{Link}     = "$libs";
   $F77config{Irix}{F77}{Trail_}   = 1;
   $F77config{Irix}{F77}{Compiler} = "f77 $abi";

   $F77config{Irix}{DEFAULT}       = 'F77';
}

### AIX ###

$F77config{Aix}{F77}{Link}   = "-L/usr/lib -lxlf90 -lxlf -lc -lm";
$F77config{Aix}{F77}{Trail_} = 0;

$F77config{Aix}{DEFAULT}     = 'F77';

### FreeBSD ###

if($^O =~ /Freebsd/i) {
  $gfortran = 'gfortran48'; # requires rewrite
  $fallback_compiler = 'G77';
}

$F77config{Freebsd}{G77}{Link} = sub {
  my $dir = gfortran_find_libdir('g77-34', 'g2c');
  gfortran_make_linkline($dir, 'g2c', "/usr/local/lib", '');
};
$F77config{Freebsd}{G77}{Trail_} = 1;
$F77config{Freebsd}{G77}{Compiler} = 'g77-34';
$F77config{Freebsd}{G77}{Cflags} = '-O2';

$F77config{Freebsd}{GFortran}{Link} = sub {
  my $dir = gfortran_find_libdir($gfortran, 'gfortran');
  gfortran_make_linkline($dir, "gfortran", "/usr/local/lib", "");
};
$F77config{Freebsd}{GFortran}{Trail_} = 1;
$F77config{Freebsd}{GFortran}{Compiler} = "$gfortran";
$F77config{Freebsd}{GFortran}{Cflags}   = '-O2';

$F77config{Freebsd}{DEFAULT}     = 'GFortran';

### VMS ###

$F77config{VMS}{Fortran}{Link}   = ' ';         # <---need this space!
$F77config{VMS}{Fortran}{Trail_} = 0;
$F77config{VMS}{Fortran}{Compiler} = 'Fortran';

$F77config{VMS}{DEFAULT}     = 'Fortran';

### Darwin (Mac OS X) ###

$F77config{Darwin}{GNU} = $F77config{Generic}{GNU};
$F77config{Darwin}{DEFAULT}     = 'GNU';

############ End of database is here ############

sub get; # See below

# All the figuring out occurs during import - this is because
# a lot of the answers depend on a lot of the guesswork.

sub import {
   no warnings; # Shuts up complaints on Win32 when running PDL tests
   $Pkg    = shift;
   my $system   = ucfirst(shift);  # Set package variables
   my $compiler = ucfirst(shift);
   $Runtime = "-LSNAFU -lwontwork";

   # Guesses if system/compiler not specified.

   $system   = ucfirst $Config{'osname'} unless $system;
   $system = 'Cygwin' if $system =~ /Cygwin/;
   $compiler = get $F77config{$system}{DEFAULT} unless $compiler;

   debug "$Pkg: Using system=$system compiler=" .
   (defined $compiler ? $compiler : "<undefined>") . "\n";

   if (defined($ENV{F77LIBS})) {
      debug "Overriding Fortran libs from value of enviroment variable F77LIBS = $ENV{F77LIBS}\n";
      $Runtime = $ENV{F77LIBS};
   }
   else {

      # Try this combination

      my $ok;
      if ( defined( $compiler ) and defined( $F77config{$system} )){
         my $flibs = get ($F77config{$system}{$compiler}{Link});
         if ($flibs ne "") {
            $Runtime = $flibs;# . gcclibs();
            # We don't want to append gcc libs if we are using
            # non gnu compilers. Initially, explicitly check for
            # use of sun compiler
            #$Runtime .= gcclibs($flibs) unless $flibs =~ /sunmath/;
            #(Note appending gcclibs seems to be no longer required)
            $Runtime =~ s|L([a-z,A-Z]):|L//$1|g if $^O =~ /cygwin/i;
            $Runtime = ' ' if $^O eq 'VMS';  # <-- need this space!
            debug "Runtime: $Runtime\n";
            $ok = 1;
            if ($compiler eq 'GNU') { # Special gfortran case since it seems to have lots of random libs
               debug "Found compiler=$compiler - skipping validation of $Runtime \n";

            } else {
               $ok = validate_libs($Runtime) if $flibs ne "" ;
            }
         }
      } else {
         $Runtime = $ok = "";
      }

      # If it doesn't work try Generic + GNU77

      unless (("$Runtime" ne "-LSNAFU -lwontwork") && $ok) {
         $system   =
         $Config{cc} =~ /\bgcc/ && $^O =~ /MSWin32/i ? "MinGW"
                                 : $^O =~ /Freebsd/i ? "Freebsd"
         :"Generic";
         $compiler = $fallback_compiler;
         warn <<"EOD";
$Pkg: Unable to guess and/or validate system/compiler configuration
$Pkg: Will try system=$system Compiler=$compiler
EOD
         my $flibs = get ($F77config{$system}{$compiler}{Link});
         $Runtime =  $flibs ; #. gcclibs($flibs); #  Note gcclibs appears to be no longer required.
         $ok = validate_libs($Runtime) if $flibs ne "";
         warn "$Pkg: Well that didn't appear to validate. Well I will try it anyway.\n"
         unless $Runtime && $ok;
      }

      $RuntimeOK = $ok;

   } # Not overriding

   # Now get the misc info for the methods.

   if (defined( $F77config{$system}{$compiler}{Trail_} )){
      $Trail_  = get $F77config{$system}{$compiler}{Trail_};
   } else {
      warn << "EOD";
      $Pkg: There does not appear to be any configuration info about
      $Pkg: names with trailing underscores for system $system. Will assume
      $Pkg: F77 names have trailing underscores.
EOD
      $Trail_ = 1;
   }

   if (defined( $F77config{$system}{$compiler}{Compiler} )) {
      $Compiler = get $F77config{$system}{$compiler}{Compiler};
   } else {
      warn << "EOD";
      $Pkg: There does not appear to be any configuration info about
      $Pkg: the F77 compiler name. Will assume 'f77'.
EOD
      $Compiler = 'f77';
   }
   debug "$Pkg: Compiler: $Compiler\n";

   if (defined( $F77config{$system}{$compiler}{Cflags} )) {
      $Cflags = get $F77config{$system}{$compiler}{Cflags} ;
   } else {
      warn << "EOD";
      $Pkg: There does not appear to be any configuration info about
      $Pkg: the options for the F77 compiler. Will assume none
      $Pkg: necessary.
EOD
      $Cflags = '';
   }

   debug "$Pkg: Cflags: $Cflags\n";

} # End of import ()

sub runtime { return $Runtime; }
sub runtimeok { return $RuntimeOK; }
sub trail_  { return $Trail_; }
sub compiler { return $Compiler; }
sub cflags  { return $Cflags; }

### Minor internal utility routines ###

# Get hash entry, evaluating code references

sub get { ref($_[0]) eq "CODE" ? &{$_[0]} : $_[0] };

# Test if any files exist matching glob

sub any_exists {
   my @glob = glob(shift);
   return scalar(@glob);
}

# Find highest version number of SCN.N(.N) directories
# (Nasty SunOS/Solaris naming scheme for F77 libs]
sub find_highest_SC {
   debug "$Pkg: Scanning for $_[0]\n";
   my @glob = glob(shift);
   my %n=();
   for (@glob) {
      #debug "Found $_\n";
      if ( m|/SC(\d)\.(\d)/?.*$| ) {
         $n{$_} = $1 *100 + $2 * 10;
      }
      if ( m|/SC(\d)\.(\d)\.(\d)/?.*$| ) {
         $n{$_} = $1 *100 + $2 * 10 + $3;
      }
   }
   my @sorted_dirs = grep {-f "$_/libF77.a"} sort {$n{$a} <=> $n{$b}} @glob;

   return pop @sorted_dirs; # Highest N
}

# Validate a string of form "-Ldir -lfoo -lbar"

sub validate_libs {
   debug "$Pkg: Validating $_[0]   ";
   my @args = shellwords(shift());
   my $pat;
   my $ret = 1;

   # Create list of directories to search (with common defaults)

   my @path = ();
   for (@args, "/usr/lib", "/lib") {
      push @path, $1 if /^-L(.+)$/ && -d $1;
   }

   # Search directories

   for (@args) {
      next if /^-L/;
      next if $_ eq "-lm"; # Ignore this common guy
      if (/^-l(.+)$/) {
         $pat = join(" ", map {$_."/lib".$1.".*"} @path); # Join dirs + file
         #debug "Checking for $pat\n";
         unless (any_exists($pat)) {
            debug "\n$Pkg:    Unable to find library $_" ;
            $ret = 0;
         }
      }
   }
   debug $ret ? "[ok]\n" : "\n";
   return $ret;
}


sub testcompiler {

   my $file = File::Spec->tmpdir . "/testf77$$";
   $file =~ s/\\/\//g; # For Win32

   my $ret;
   open(OUT,">$file.f");
   print OUT "      write(*,*) 'Hello World'\n";
   print OUT "      end\n";
   close(OUT);
   debug "Compiling the test Fortran program...\n";
   system "$Compiler $Cflags $file.f -o ${file}_exe";
   debug "Executing the test program...\n";
   if (`${file}_exe` ne " Hello World\n") {
      warn "Test of Fortran Compiler FAILED. \n";
      warn "Do not know how to compile Fortran on your system\n";
      $ret=0;
   }
   else{
      debug "Congratulations you seem to have a working f77!\n";
      $ret=1;
   }
   unlink("${file}_exe"); unlink("$file.f"); unlink("$file.o") if -e "$file.o";
   return $ret;
};

# gcclibs() routine
#    Return gcc link libs (e.g. -L/usr/local/lib/gcc-lib/sparc-sun-sunos4.1.3_U1/2.7.0 -lgcc)
#    Note this routine appears to be no longer required - gcc3 or 4 change? - and is
#    NO LONGER CALLED from anywhere in the code. Use of this routine in the future
#    is DEPRECATED. Retain here just in case this logic is ever needed again,
#    - Karl Glazebrook Dec/2010

sub gcclibs {
   my $flibs = shift; # Fortran libs
   my $isgcc = $Config{'cc'} eq "$gcc";
   if (!$isgcc && $^O ne 'VMS') {
      debug "Checking for gcc in disguise:\n";
      $isgcc = 1 if $Config{gccversion};
      my $string;
      if ($isgcc) {
         $string = "Compiler is gcc version $Config{gccversion}";
         $string .= "\n" unless $string =~ /\n$/;
      } else {
         $string = "Not gcc\n";
      }
      debug $string;
   }
   if ($isgcc or ($flibs =~ /-lg2c/) or ($flibs =~ /-lf2c/) ) {
      # Don't link to libgcc on MS Windows iff we're using gfortran.
      unless ($fallback_compiler eq 'GFortran' && $^O =~ /MSWin/i) {
         my $gccdir = `$gcc -m32 -print-libgcc-file-name`; chomp $gccdir;
         $gccdir =~ s/\/libgcc.a//;
         return qq{ "-L$gccdir" -lgcc};
      } else {
         return "";
      }

   }else{
      return "";
   }
}

# Try and find a program in the users PATH
sub find_in_path {
  my $found = first { which $_ } @_;
  return ($^O eq 'VMS' ? '' : undef) if !$found;
  debug "Found compiler $found\n";
  return $found;
}

# Based on code from Tim Jeness, tries to figure out the correct GNU
# fortran compiler libs
sub link_gnufortran_compiler {
   return () if $^O =~ /MSWin32/i; # Unneeded for MinGW, emits warnings if allowed to proceed.
   my @try = @_;
   my $compiler = find_in_path( @try );
   return () unless defined $compiler;
   # Get compiler version number
   my @t =`$compiler --version`; $t[0] =~ /(\d+).(\d)+.(\d+)/;
   my $version = "$1.$2";  # Major version number
   debug "ExtUtils::F77: $compiler version $version.$3\n";
   # Sigh special case random extra gfortran libs to avoid PERL_DL_NONLAZY meltdowns. KG 25/10/2015
   my $append = "";
   if ( $Config{osname} =~ /darwin/ && $Config{osvers} >= 14
      && $compiler eq 'gfortran' && $version >= 4.9 ) {
      # Add extra libs for gfortran versions >= 4.9 and OS X
      $append = "-lgcc_ext.10.5 -lgcc_s.10.5 -lquadmath";
   }
   my @libs = @{$COMPLIBS{$compiler}};
   my ($dir, $lib, $test);
   foreach $test (@libs) {
      $dir = gfortran_find_libdir($compiler, $test);
      $lib = $test, last if defined $dir;
   }
   gfortran_make_linkline($dir, $lib, "/usr/local/lib", "f2c", $append);
}

1; # Return true

__END__

=head1 NAME

ExtUtils::F77 - Simple interface to F77 libs

=head1 DESCRIPTION

This module tries to figure out how to link C programs with
Fortran subroutines on your system. Basically one must add a list
of Fortran runtime libraries. The problem is their location
and name varies with each OS/compiler combination! It was originally
developed to make building and installation of the L<PGPLOT> module easier,
which links to the pgplot Fortran graphics library. It is now used by a numnber
of perl modules.

This module tries to implement a simple
'rule-of-thumb' database for various flavours of UNIX systems.
A simple self-documenting Perl database of knowledge/code
for figuring out how to link for various combinations of OS and
compiler is embedded in the modules Perl code. Please help
save the world by submitted patches for new database entries for
your system at L<https://github.com/PDLPorters/extutils-f77>

Note the default on most systems is now to search for a generic 'GNU' compiler
which can be gfortran, g77, g95 or fort77 (in that order based on usage) and then find
the appropriate link libraries automatically. (This is the 'Generic' 'GNU' database entry
in the code.)

The library list which the module returns
can be explicitly overridden by setting the environment
variable F77LIBS, e.g.

  % setenv F77LIBS "-lfoo -lbar"
  % perl -MExtUtils::F77 -e 'print ExtUtils::F77->compiler, "\n"'
  ...

=head1 SYNOPSIS

  use ExtUtils::F77;               # Automatic guess
  use ExtUtils::F77 qw(sunos);     # Specify system
  use ExtUtils::F77 qw(linux g77); # Specify system and compiler
  $fortranlibs = ExtUtils::F77->runtime;

=head1 METHODS

The following are all class methods.

=head2 runtime

Returns a list of F77 runtime libraries.

  $fortranlibs = ExtUtils::F77->runtime;

=head2 runtimeok

Returns TRUE only if runtime libraries have been found successfully.

=head2 trail_

Returns true if F77 names have trailing underscores.

=head2 compiler

Returns command to execute the compiler (e.g. 'f77').

=head2 cflags

Returns compiler flags.

=head2 testcompiler

Test to see if compiler actually works.

=head1 DEBUGGING

To debug this module, and/or get more information about its workings,
set C<$ExtUtils::F77::DEBUG> to a true value. If set, it will C<warn>
various information about its operations.

As of version 1.22, this module will no longer print or warn if it is
working normally.

=head1 SEE ALSO

L<PDL> (PDL::Minuit and PDL::Slatec), L<PGPLOT>

=head1 AUTHOR

Karl Glazebrook, with many other contributions (see git repository at https://github.com/PDLPorters/extutils-f77)
