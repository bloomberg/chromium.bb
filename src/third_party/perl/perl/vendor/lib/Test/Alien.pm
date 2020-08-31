package Test::Alien;

use strict;
use warnings;
use 5.008001;
use Env qw( @PATH );
use File::Which 1.10 qw( which );
use Capture::Tiny qw( capture capture_merged );
use File::Temp qw( tempdir );
use File::Copy qw( move );
use Text::ParseWords qw( shellwords );
use Test2::API qw( context run_subtest );
use base qw( Exporter );
use Path::Tiny qw( path );
use Alien::Build::Util qw( _dump );
use Config;

our @EXPORT = qw( alien_ok run_ok xs_ok ffi_ok with_subtest synthetic helper_ok interpolate_template_is );

# ABSTRACT: Testing tools for Alien modules
our $VERSION = '1.74'; # VERSION


our @aliens;

sub alien_ok ($;$)
{
  my($alien, $message) = @_;

  my $name = ref $alien ? ref($alien) . '[instance]' : $alien;
  $name = 'undef' unless defined $name;
  my @methods = qw( cflags libs dynamic_libs bin_dir );
  $message ||= "$name responds to: @methods";
  
  my $ok;
  my @diag;
  
  if(defined $alien)
  {
    my @missing = grep { ! $alien->can($_) } @methods;
  
    $ok = !@missing;
    push @diag, map { "  missing method $_" } @missing;

    if($ok)
    {
      push @aliens, $alien;
      unshift @PATH, $alien->bin_dir;
    }
  }
  else
  {
    $ok = 0;
    push @diag, "  undefined alien";
  }

  my $ctx = context();
  $ctx->ok($ok, $message);
  $ctx->diag($_) for @diag;
  $ctx->release;
  
  $ok;
}


sub synthetic
{
  my($opt) = @_;
  $opt ||= {};
  my %alien = %$opt;
  require Test::Alien::Synthetic;
  bless \%alien, 'Test::Alien::Synthetic', 
}


sub run_ok
{
  my($command, $message) = @_;
  
  my(@command) = ref $command ? @$command : ($command);
  $message ||= "run @command";
  
  require Test::Alien::Run;
  my $run = bless {
    out    => '',
    err    => '',
    exit   => 0,
    sig    => 0,
    cmd    => [@command],
  }, 'Test::Alien::Run';
  
  my $ctx = context();
  my $exe = which $command[0];
  if(defined $exe)
  {
    shift @command;
    $run->{cmd} = [$exe, @command];
    my @diag;
    my $ok = 1;
    my($exit, $errno);
    ($run->{out}, $run->{err}, $exit, $errno) = capture { system $exe, @command; ($?,$!); };
  
    if($exit == -1)
    {
      $ok = 0;
      $run->{fail} = "failed to execute: $errno";
      push @diag, "  failed to execute: $errno";
    }
    elsif($exit & 127)
    {
      $ok = 0;
      push @diag, "  killed with signal: @{[ $exit & 127 ]}";
      $run->{sig} = $exit & 127;
    }
    else
    {
      $run->{exit} = $exit >> 8;
    }

    $ctx->ok($ok, $message);
    $ok 
      ? $ctx->note("  using $exe") 
      : $ctx->diag("  using $exe");
    $ctx->diag(@diag) for @diag;

  }
  else
  {
    $ctx->ok(0, $message);
    $ctx->diag("  command not found");
    $run->{fail} = 'command not found';
  }
  
  $ctx->release;
  
  $run;
}


sub _flags
{
  my($class, $method) = @_;
  my $static = "${method}_static";
  $class->can($static) && $class->can('install_type') && $class->install_type eq 'share' && (!$class->can('xs_load'))
    ? $class->$static
    : $class->$method;
}

sub xs_ok
{
  my $cb;
  $cb = pop if defined $_[-1] && ref $_[-1] eq 'CODE';
  my($xs, $message) = @_;
  $message ||= 'xs';

  require ExtUtils::CBuilder;
  my $skip = !ExtUtils::CBuilder->new->have_compiler;

  if($skip)
  {
    my $ctx = context();
    $ctx->skip($message, 'test requires a compiler');
    $ctx->skip("$message subtest", 'test requires a compiler') if $cb;
    $ctx->release;
    return;
  }
  
  $xs = { xs => $xs } unless ref $xs;
  # make sure this is a copy because we may
  # modify it.
  $xs->{xs} = "@{[ $xs->{xs} ]}";
  $xs->{pxs} ||= {};
  $xs->{cbuilder_compile} ||= {};
  $xs->{cbuilder_link}    ||= {};

  if($xs->{cpp} || $xs->{'C++'})
  {
    my $ctx = context();
    $ctx->bail("The cpp and C++ options have been removed from xs_ok");
  }
  else
  {
    $xs->{c_ext} ||= 'c';
  }

  my $verbose = $xs->{verbose} || 0;
  my $ok = 1;
  my @diag;
  my $dir = _tempdir( CLEANUP => 1, TEMPLATE => 'testalienXXXXX' );
  my $xs_filename = path($dir)->child('test.xs')->stringify;
  my $c_filename  = path($dir)->child("test.@{[ $xs->{c_ext} ]}")->stringify;
  
  my $ctx = context();
  my $module;

  if($xs->{xs} =~ /\bTA_MODULE\b/)
  {
    our $count;
    $count = 0 unless defined $count;
    my $name = sprintf "Test::Alien::XS::Mod%s", $count++;
    my $code = $xs->{xs};
    $code =~ s{\bTA_MODULE\b}{$name}g;
    $xs->{xs} = $code;
  }

  # this regex copied shamefully from ExtUtils::ParseXS
  # in part because we need the module name to do the bootstrap
  # and also because if this regex doesn't match then ParseXS
  # does an exit() which we don't want.
  if($xs->{xs} =~ /^MODULE\s*=\s*([\w:]+)(?:\s+PACKAGE\s*=\s*([\w:]+))?(?:\s+PREFIX\s*=\s*(\S+))?\s*$/m)
  {
    $module = $1;
    $ctx->note("detect module name $module") if $verbose;
  }
  else
  {
    $ok = 0;
    push @diag, '  XS does not have a module decleration that we could find';
  }

  if($ok)
  {
    open my $fh, '>', $xs_filename;
    print $fh $xs->{xs};
    close $fh;
  
    require ExtUtils::ParseXS;
    my $pxs = ExtUtils::ParseXS->new;
  
    my($out, $err) = capture_merged {
      eval {
        $pxs->process_file(
          filename     => $xs_filename,
          output       => $c_filename,
          versioncheck => 0,
          prototypes   => 0,
          %{ $xs->{pxs} },
        );
      };
      $@;
    };
    
    $ctx->note("parse xs $xs_filename => $c_filename") if $verbose;
    $ctx->note($out) if $verbose;
    $ctx->note("error: $err") if $verbose && $err;

    unless($pxs->report_error_count == 0)
    {
      $ok = 0;
      push @diag, '  ExtUtils::ParseXS failed:';
      push @diag, "    $err" if $err;
      push @diag, "    $_" for split /\r?\n/, $out;
    }
  }

  if($ok)
  {
    my $cb = ExtUtils::CBuilder->new(
      config => {
        lddlflags => join(' ', grep !/^-l/, shellwords map { _flags $_, 'libs' } @aliens) . " $Config{lddlflags}",
      },
    );

    my %compile_options = (
      source               => $c_filename,
      %{ $xs->{cbuilder_compile} },
    );
    
    if(defined $compile_options{extra_compiler_flags} && ref($compile_options{extra_compiler_flags}) eq '')
    {
      $compile_options{extra_compiler_flags} = [ shellwords $compile_options{extra_compiler_flags} ];
    }
    
    push @{ $compile_options{extra_compiler_flags} }, shellwords map { _flags $_, 'cflags' } @aliens;

    my($out, $obj, $err) = capture_merged {
      my $obj = eval {
        $cb->compile(%compile_options);
      };
      ($obj, $@);
    };
    
    $ctx->note("compile $c_filename") if $verbose;
    $ctx->note($out) if $verbose;
    $ctx->note($err) if $verbose && $err;

    if($verbose > 1)
    {
      $ctx->note(_dump({ compile_options => \%compile_options }));
    }
    
    unless($obj)
    {
      $ok = 0;
      push @diag, '  ExtUtils::CBuilder->compile failed';
      push @diag, "    $err" if $err;
      push @diag, "    $_" for split /\r?\n/, $out;
    }
    
    if($ok)
    {

      my %link_options = (
        objects            => [$obj],
        module_name        => $module,
        %{ $xs->{cbuilder_link} },
      );

      if(defined $link_options{extra_linker_flags} && ref($link_options{extra_linker_flags}) eq '')
      {
        $link_options{extra_linker_flags} = [ shellwords $link_options{extra_linker_flags} ];
      }
      
      unshift @{ $link_options{extra_linker_flags} }, grep /^-l/, shellwords map { _flags $_, 'libs' } @aliens;

      my($out, $lib, $err) = capture_merged {
        my $lib = eval { 
          $cb->link(%link_options);
        };
        ($lib, $@);
      };
      
      $ctx->note("link $obj") if $verbose;
      $ctx->note($out) if $verbose;
      $ctx->note($err) if $verbose && $err;

      if($verbose > 1)
      {
        $ctx->note(_dump({ link_options => \%link_options }));
      }

      if($lib && -f $lib)
      {
        $ctx->note("created lib $lib") if $xs->{verbose};
      }
      else
      {
        $ok = 0;
        push @diag, '  ExtUtils::CBuilder->link failed';
        push @diag, "    $err" if $err;
        push @diag, "    $_" for split /\r?\n/, $out;
      }
      
      if($ok)
      {
        my @modparts = split(/::/,$module);
        my $dl_dlext = $Config{dlext};
        my $modfname = $modparts[-1];

        my $libpath = path($dir)->child('auto', @modparts, "$modfname.$dl_dlext");
        $libpath->parent->mkpath;
        move($lib, "$libpath") || die "unable to copy $lib => $libpath $!";
        
        pop @modparts;
        my $pmpath = path($dir)->child(@modparts, "$modfname.pm");
        $pmpath->parent->mkpath;
        open my $fh, '>', "$pmpath";
        
        my($alien_with_xs_load, @rest) = grep { $_->can('xs_load') } @aliens;
        
        if($alien_with_xs_load)
        {
          {
            no strict 'refs';
            @{join '::', $module, 'rest'} = @rest;
            ${join '::', $module, 'alien_with_xs_load'} = $alien_with_xs_load;
          }
          print $fh '# line '. __LINE__ . ' "' . __FILE__ . qq("\n) . qq{
            package $module;
            
            use strict;
            use warnings;
            our \$VERSION = '0.01';
            our \@rest;
            our \$alien_with_xs_load;
            
            \$alien_with_xs_load->xs_load('$module', \$VERSION, \@rest);
            
            1;
          };
        }
        else
        {
          print $fh '# line '. __LINE__ . ' "' . __FILE__ . qq("\n) . qq{
            package $module;
          
            use strict;
            use warnings;
            require XSLoader;
            our \$VERSION = '0.01';
            XSLoader::load('$module',\$VERSION);
          
            1;
          };
        }
        close $fh;

        {
          local @INC = @INC;
          unshift @INC, $dir;
          eval '# line '. __LINE__ . ' "' . __FILE__ . qq("\n) . qq{
            use $module;
          };
        }
        
        if(my $error = $@)
        {
          $ok = 0;
          push @diag, '  XSLoader failed';
          push @diag, "    $error";
        }
      }
    }
  }

  $ctx->ok($ok, $message);
  $ctx->diag($_) for @diag;
  $ctx->release;
  
  if($cb)
  {
    $cb = sub {
      my $ctx = context();
      $ctx->plan(0, 'SKIP', "subtest requires xs success");
      $ctx->release;
    } unless $ok;

    @_ = ("$message subtest", $cb, 1, $module);

    goto \&Test2::API::run_subtest;
  }

  $ok;
}

sub with_subtest (&) { $_[0]; }


sub ffi_ok
{
  my $cb;
  $cb = pop if defined $_[-1] && ref $_[-1] eq 'CODE';
  my($opt, $message) = @_;
  
  $message ||= 'ffi';
  
  my $ok = 1;
  my $skip;
  my $ffi;
  my @diag;
  
  {
    my $min = '0.12'; # the first CPAN release
    $min = '0.15' if $opt->{ignore_not_found};
    $min = '0.18' if $opt->{lang};
    unless(eval { require FFI::Platypus; FFI::Platypus->VERSION($min) })
    {
      $ok = 0;
      $skip = "Test requires FFI::Platypus $min";
    }
  }
  
  if($ok && $opt->{lang})
  {
    my $class = "FFI::Platypus::Lang::@{[ $opt->{lang} ]}";
    eval qq{ use $class () };
    if($@)
    {
      $ok = 0;
      $skip = "Test requires FFI::Platypus::Lang::@{[ $opt->{lang} ]}";
    }
  }
  
  if($ok)
  {
    $ffi = FFI::Platypus->new(
      lib              => [map { $_->dynamic_libs } @aliens],
      ignore_not_found => $opt->{ignore_not_found},
      lang             => $opt->{lang},
    );
    foreach my $symbol (@{ $opt->{symbols} || [] })
    {
      unless($ffi->find_symbol($symbol))
      {
        $ok = 0;
        push @diag, "  $symbol not found"
      }
    }
  }
  
  my $ctx = context(); 
  
  if($skip)
  {
    $ctx->skip($message, $skip);
  }
  else
  {
    $ctx->ok($ok, $message);
  }
  $ctx->diag($_) for @diag;
  
  $ctx->release;

  if($cb)
  {
    $cb = sub {
      my $ctx = context();
      $ctx->plan(0, 'SKIP', "subtest requires ffi success");
      $ctx->release;
    } unless $ok;

    @_ = ("$message subtest", $cb, 1, $ffi);

    goto \&Test2::API::run_subtest;
  }
  
  $ok;
}


sub _interpolator
{  
  require Alien::Build::Interpolate::Default;
  my $intr = Alien::Build::Interpolate::Default->new;
  
  foreach my $alien (@aliens)
  {
    if($alien->can('alien_helper'))
    {
      my $help = $alien->alien_helper;
      foreach my $name (keys %$help)
      {
        my $code = $help->{$name};
        $intr->replace_helper($name, $code);
      }
    }
  }
  
  $intr;
}

sub helper_ok
{
  my($name, $message) = @_;

  $message ||= "helper $name exists";

  my $intr = _interpolator; 

  my $code = $intr->has_helper($name);

  my $ok = defined $code;

  my $ctx = context();
  $ctx->ok($ok, $message);
  $ctx->release;
  
  $ok;
}


sub interpolate_template_is
{
  my($template, $pattern, $message) = @_;
  
  $message ||= "template matches";
  
  my $intr = _interpolator;
  
  my $value = eval { $intr->interpolate($template) };
  my $error = $@;
  my @diag;
  my $ok;
  
  if($error)
  {
    $ok = 0;
    push @diag, "error in evaluation:";
    push @diag, "  $error";
  }
  elsif(ref($pattern) eq 'Regexp')
  {
    $ok = $value =~ $pattern;
    push @diag, "value '$value' does not match $pattern'" unless $ok;
  }
  else
  {
    $ok = $value eq "$pattern";
    push @diag, "value '$value' does not equal '$pattern'" unless $ok;
  }
  
  my $ctx = context();
  $ctx->ok($ok, $message, [@diag]);
  $ctx->release;
  
  $ok;
}

sub _tempdir {
  # makes sure /tmp or whatever isn't mounted noexec,
  # which will cause xs_ok tests to fail.

  my $dir = tempdir(@_);

  if($^O ne 'MSWin32')
  {
    my $filename = path($dir, 'foo.pl');
    my $fh;
    open $fh, '>', $filename;
    print $fh "#!$^X";
    close $fh;
    chmod 0755, $filename;
    system $filename, 'foo';
    if($?)
    {
      $dir = tempdir( DIR => path('.')->absolute->stringify );
    }
  }
  
  $dir;  
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test::Alien - Testing tools for Alien modules

=head1 VERSION

version 1.74

=head1 SYNOPSIS

Test commands that come with your Alien:

 use Test2::V0;
 use Test::Alien;
 use Alien::patch;
 
 alien_ok 'Alien::patch';
 run_ok([ 'patch', '--version' ])
   ->success
   # we only accept the version written
   # by Larry ...
   ->out_like(qr{Larry Wall}); 
 
 done_testing;

Test that your library works with C<XS>:

 use Test2::V0;
 use Test::Alien;
 use Alien::Editline;
 
 alien_ok 'Alien::Editline';
 my $xs = do { local $/; <DATA> };
 xs_ok $xs, with_subtest {
   my($module) = @_;
   ok $module->version;
 };
 
 done_testing;

 __DATA__
 
 #include "EXTERN.h"
 #include "perl.h"
 #include "XSUB.h"
 #include <editline/readline.h>
 
 const char *
 version(const char *class)
 {
   return rl_library_version;
 }
 
 MODULE = TA_MODULE PACKAGE = TA_MODULE
 
 const char *version(class);
     const char *class;

Test that your library works with L<FFI::Platypus>:

 use Test2::V0;
 use Test::Alien;
 use Alien::LibYAML;
 
 alien_ok 'Alien::LibYAML';
 ffi_ok { symbols => ['yaml_get_version'] }, with_subtest {
   my($ffi) = @_;
   my $get_version = $ffi->function(yaml_get_version => ['int*','int*','int*'] => 'void');
   $get_version->call(\my $major, \my $minor, \my $patch);
   like $major, qr{[0-9]+};
   like $minor, qr{[0-9]+};
   like $patch, qr{[0-9]+};
 };
 
 done_testing;

=head1 DESCRIPTION

This module provides tools for testing L<Alien> modules.  It has hooks
to work easily with L<Alien::Base> based modules, but can also be used
via the synthetic interface to test non L<Alien::Base> based L<Alien>
modules.  It has very modest prerequisites.

Prior to this module the best way to test a L<Alien> module was via L<Test::CChecker>.
The main downside to that module is that it is heavily influenced by and uses
L<ExtUtils::CChecker>, which is a tool for checking at install time various things
about your compiler.  It was also written before L<Alien::Base> became as stable as it
is today.  In particular, L<Test::CChecker> does its testing by creating an executable
and running it.  Unfortunately Perl uses extensions by creating dynamic libraries
and linking them into the Perl process, which is different in subtle and error prone
ways.  This module attempts to test the libraries in the way that they will actually
be used, via either C<XS> or L<FFI::Platypus>.  It also provides a mechanism for
testing binaries that are provided by the various L<Alien> modules (for example
L<Alien::gmake> and L<Alien::patch>).

L<Alien> modules can actually be useable without a compiler, or without L<FFI::Platypus>
(for example, if the library is provided by the system, and you are using L<FFI::Platypus>,
or if you are building from source and you are using C<XS>), so tests with missing
prerequisites are automatically skipped.  For example, L</xs_ok> will automatically skip
itself if a compiler is not found, and L</ffi_ok> will automatically skip itself
if L<FFI::Platypus> is not installed.

=head1 FUNCTIONS

=head2 alien_ok

 alien_ok $alien, $message;
 alien_ok $alien;

Load the given L<Alien> instance or class.  Checks that the instance or class conforms to the same
interface as L<Alien::Base>.  Will be used by subsequent tests.  The C<$alien> module only needs to
provide these methods in order to conform to the L<Alien::Base> interface:

=over 4

=item cflags

String containing the compiler flags

=item libs

String containing the linker and library flags

=item dynamic_libs

List of dynamic libraries.  Returns empty list if the L<Alien> module does not provide this.

=item bin_dir

Directory containing tool binaries.  Returns empty list if the L<Alien> module does not provide
this.

=back

If your L<Alien> module does not conform to this interface then you can create a synthetic L<Alien>
module using the L</synthetic> function.

=head2 synthetic

 my $alien = synthetic \%config;

Create a synthetic L<Alien> module which can be passed into L</alien_ok>.  C<\%config>
can contain these keys (all of which are optional):

=over 4

=item cflags

String containing the compiler flags.

=item cflags_static

String containing the static compiler flags (optional).

=item libs

String containing the linker and library flags.

=item libs_static

String containing the static linker flags (optional).

=item dynamic_libs

List reference containing the dynamic libraries.

=item bin_dir

Tool binary directory.

=item runtime_prop

Runtime properties.

=back

See L<Test::Alien::Synthetic> for more details.

=head2 run_ok

 my $run = run_ok $command;
 my $run = run_ok $command, $message;

Runs the given command, falling back on any C<Alien::Base#bin_dir> methods provided by L<Alien> modules
specified with L</alien_ok>.

C<$command> can be either a string or an array reference.

Only fails if the command cannot be found, or if it is killed by a signal!  Returns a L<Test::Alien::Run>
object, which you can use to test the exit status, output and standard error.

Always returns an instance of L<Test::Alien::Run>, even if the command could not be found.

=head2 xs_ok

 xs_ok $xs;
 xs_ok $xs, $message;

Compiles, links the given C<XS> code and attaches to Perl.

If you use the special module name C<TA_MODULE> in your C<XS>
code, it will be replaced by an automatically generated
package name.  This can be useful if you want to pass the same
C<XS> code to multiple calls to C<xs_ok> without subsequent
calls replacing previous ones.

C<$xs> may be either a string containing the C<XS> code,
or a hash reference with these keys:

=over 4

=item xs

The XS code.  This is the only required element.

=item pxs

Extra L<ExtUtils::ParseXS> arguments passed in as a hash reference.

=item cbuilder_compile

Extra The L<ExtUtils::CBuilder> arguments passed in as a hash reference.

=item cbuilder_link

Extra The L<ExtUtils::CBuilder> arguments passed in as a hash reference.

=item verbose

Spew copious debug information via test note.

=back

You can use the C<with_subtest> keyword to conditionally
run a subtest if the C<xs_ok> call succeeds.  If C<xs_ok>
does not work, then the subtest will automatically be
skipped.  Example:

 xs_ok $xs, with_subtest {
   # skipped if $xs fails for some reason
   my($module) = @_;
   is $module->foo, 1;
 };

The module name detected during the XS parsing phase will
be passed in to the subtest.  This is helpful when you are
using a generated module name.

=head2 ffi_ok

 ffi_ok;
 ffi_ok \%opt;
 ffi_ok \%opt, $message;

Test that L<FFI::Platypus> works.

C<\%opt> is a hash reference with these keys (all optional):

=over 4

=item symbols

List references of symbols that must be found for the test to succeed.

=item ignore_not_found

Ignores symbols that aren't found.  This affects functions accessed via
L<FFI::Platypus#attach> and L<FFI::Platypus#function> methods, and does
not influence the C<symbols> key above.

=item lang

Set the language.  Used primarily for language specific native types.

=back

As with L</xs_ok> above, you can use the C<with_subtest> keyword to specify
a subtest to be run if C<ffi_ok> succeeds (it will skip otherwise).  The
L<FFI::Platypus> instance is passed into the subtest as the first argument.
For example:

 ffi_ok with_subtest {
   my($ffi) = @_;
   is $ffi->function(foo => [] => 'void')->call, 42;
 };

=head2 helper_ok

 helper_ok $name;
 helper_ok $name, $message;

Tests that the given helper has been defined.

=head2 interpolate_template_is

 interpolate_template_is $template, $string;
 interpolate_template_is $template, $string, $message;
 interpolate_template_is $template, $regex;
 interpolate_template_is $template, $regex, $message;

Tests that the given template when evaluated with the appropriate helpers will match
either the given string or regular expression.

=head1 SEE ALSO

=over 4

=item L<Alien>

=item L<Alien::Base>

=item L<Alien::Build>

=item L<alienfile>

=item L<Test2>

=item L<Test::Alien::Run>

=item L<Test::Alien::CanCompile>

=item L<Test::Alien::CanPlatypus>

=item L<Test::Alien::Synthetic>

=back

=head1 AUTHOR

Author: Graham Ollis E<lt>plicease@cpan.orgE<gt>

Contributors:

Diab Jerius (DJERIUS)

Roy Storey (KIWIROY)

Ilya Pavlov

David Mertens (run4flat)

Mark Nunberg (mordy, mnunberg)

Christian Walde (Mithaldu)

Brian Wightman (MidLifeXis)

Zaki Mughal (zmughal)

mohawk (mohawk2, ETJ)

Vikas N Kumar (vikasnkumar)

Flavio Poletti (polettix)

Salvador Fandiño (salva)

Gianni Ceccarelli (dakkar)

Pavel Shaydo (zwon, trinitum)

Kang-min Liu (劉康民, gugod)

Nicholas Shipp (nshp)

Juan Julián Merelo Guervós (JJ)

Joel Berger (JBERGER)

Petr Pisar (ppisar)

Lance Wicks (LANCEW)

Ahmad Fatoum (a3f, ATHREEF)

José Joaquín Atria (JJATRIA)

Duke Leto (LETO)

Shoichi Kaji (SKAJI)

Shawn Laffan (SLAFFAN)

Paul Evans (leonerd, PEVANS)

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011-2019 by Graham Ollis.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
