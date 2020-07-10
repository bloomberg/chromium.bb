package Test::Script;

# ABSTRACT: Basic cross-platform tests for scripts
our $VERSION = '1.25'; # VERSION


use 5.008001;
use strict;
use warnings;
use Carp qw( croak );
use Exporter;
use File::Spec;
use File::Spec::Unix;
use Probe::Perl;
use Capture::Tiny qw( capture );
use Test2::API qw( context );
use File::Temp qw( tempdir );
use IO::Handle;

our @ISA     = 'Exporter';
our @EXPORT  = qw{
  script_compiles
  script_compiles_ok
  script_runs
  script_stdout_is
  script_stdout_isnt
  script_stdout_like
  script_stdout_unlike
  script_stderr_is
  script_stderr_isnt
  script_stderr_like
  script_stderr_unlike
};

sub import {
  my $self = shift;
  my $pack = caller;
  if(defined $_[0] && $_[0] =~ /^(?:no_plan|skip_all|tests)$/)
  {
    # icky back compat.
    # do not use.
    my $ctx = context();
    if($_[0] eq 'tests')
    {
      $ctx->plan($_[1]);
    }
    elsif($_[0] eq 'skip_all')
    {
      $ctx->plan(0, 'SKIP', $_[1]);
    }
    else
    {
      $ctx->hub->plan('NO PLAN');
    }
    $ctx->release;
  }
  foreach ( @EXPORT ) {
    $self->export_to_level(1, $self, $_);
  }
}

my $perl = undef;

sub perl () {
  $perl or
  $perl = Probe::Perl->find_perl_interpreter;
}

sub path ($) {
  my $path = shift;
  unless ( defined $path ) {
    croak("Did not provide a script name");
  }
  if ( File::Spec::Unix->file_name_is_absolute($path) ) {
    croak("Script name must be relative");
  }
  File::Spec->catfile(
    File::Spec->curdir,
    split /\//, $path
  );
}

#####################################################################
# Test Functions


sub script_compiles {
  my $args   = _script(shift);
  my $unix   = shift @$args;
  my $path   = path( $unix );
  my $pargs  = _perl_args($path);
  my $dir    = _preload_module();
  my $cmd    = [ perl, @$pargs, "-I$dir", '-M__TEST_SCRIPT__', '-c', $path, @$args ];
  my ($stdout, $stderr) = capture { system(@$cmd) };
  my $error  = $@;
  my $exit   = $? ? ($? >> 8) : 0;
  my $signal = $? ? ($? & 127) : 0;
  my $ok     = !! (
    $error eq '' and $exit == 0 and $signal == 0 and $stderr =~ /syntax OK\s+\z/si
  );

  my $ctx = context();
  $ctx->ok( $ok, $_[0] || "Script $unix compiles" );
  $ctx->diag( "$exit - $stderr" ) unless $ok;
  $ctx->diag( "exception: $error" ) if $error;
  $ctx->diag( "signal: $signal" ) if $signal;
  $ctx->release;

  return $ok;
}

# this is noticeably slower for long @INC lists (sometimes present in cpantesters
# boxes) than the previous implementation, which added a -I for every element in
# @INC.  (also slower for more reasonable @INCs, but not noticeably).  But it is
# safer as very long argument lists can break calls to system
sub _preload_module
{
  my @opts = ( '.test-script-XXXXXXXX', CLEANUP => 1);
  if(-w File::Spec->curdir)
  { push @opts, DIR => File::Spec->curdir }
  else
  { push @opts, DIR => File::Spec->tmpdir }
  my $dir = tempdir(@opts);
  $dir = File::Spec->rel2abs($dir);
  # this is hopefully a pm file that nobody would use
  my $filename = File::Spec->catfile($dir, '__TEST_SCRIPT__.pm');
  my $fh;
  open($fh, '>', $filename)
    || die "unable to open $filename: $!";
  print($fh 'unshift @INC, ',
    join ',',
    # quotemeta is overkill, but it will make sure that characters
    # like " are quoted
    map { '"' . quotemeta($_) . '"' }
    grep { ! ref } @INC)
      || die "unable to write $filename: $!";
  close($fh) || die "unable to close $filename: $!";;
  $dir;
}


my $stdout;
my $stderr;

sub script_runs {
  my $args   = _script(shift);
  my $opt    = _options(\@_);
  my $unix   = shift @$args;
  my $path   = path( $unix );
  my $pargs  = [ @{ _perl_args($path) }, @{ $opt->{interpreter_options} } ];
  my $dir    = _preload_module();
  my $cmd    = [ perl, @$pargs, "-I$dir", '-M__TEST_SCRIPT__', $path, @$args ];
  $stdout = '';
  $stderr = '';

  if($opt->{stdin})
  {
    my $filename;

    if(ref($opt->{stdin}) eq 'SCALAR')
    {
      $filename = File::Spec->catfile(
        tempdir(CLEANUP => 1),
        'stdin.txt',
      );
      my $tmp;
      open($tmp, '>', $filename) || die "unable to write to $filename";
      print $tmp ${ $opt->{stdin} };
      close $tmp;
    }
    elsif(ref($opt->{stdin}) eq '')
    {
      $filename = $opt->{stdin};
    }
    else
    {
      croak("stdin MUST be either a scalar reference or a string filename");
    }

    my $fh;
    open($fh, '<', $filename) || die "unable to open $filename $!";
    STDIN->fdopen( $fh, 'r' ) or die "unable to reopen stdin to $filename $!";
  }

  (${$opt->{stdout}}, ${$opt->{stderr}}) = capture { system(@$cmd) };
  
  my $error  = $@;
  my $exit   = $? ? ($? >> 8) : 0;
  my $signal = $? ? ($? & 127) : 0;
  my $ok     = !! ( $error eq '' and $exit == $opt->{exit} and $signal == $opt->{signal} );

  my $ctx = context();
  $ctx->ok( $ok, $_[0] || "Script $unix runs" );
  $ctx->diag( "$exit - $stderr" ) unless $ok;
  $ctx->diag( "exception: $error" ) if $error;
  $ctx->diag( "signal: $signal" ) unless $signal == $opt->{signal};
  $ctx->release;

  return $ok;
}

sub _like
{
  my($text, $pattern, $regex, $not, $name) = @_;

  my $ok = $regex ? $text =~ $pattern : $text eq $pattern;
  $ok = !$ok if $not;

  my $ctx = context;
  $ctx->ok( $ok, $name );
  unless($ok) {
    $ctx->diag( "The output" );
    $ctx->diag( "  $_") for split /\n/, $text;
    $ctx->diag( $not ? "does match" : "does not match" );
    if($regex) {
      $ctx->diag( "  $pattern" );
    } else {
      $ctx->diag( "  $_" ) for split /\n/, $pattern;
    }
  }
  $ctx->release;

  $ok;
}


sub script_stdout_is
{
  my($pattern, $name) = @_;
  @_ = ($stdout, $pattern, 0, 0, $name || 'stdout matches' );
  goto &_like;
}


sub script_stdout_isnt
{
  my($pattern, $name) = @_;
  @_ = ($stdout, $pattern, 0, 1, $name || 'stdout does not match' );
  goto &_like;
}


sub script_stdout_like
{
  my($pattern, $name) = @_;
  @_ = ($stdout, $pattern, 1, 0, $name || 'stdout matches' );
  goto &_like;
}


sub script_stdout_unlike
{
  my($pattern, $name) = @_;
  @_ = ($stdout, $pattern, 1, 1, $name || 'stdout does not match' );
  goto &_like;
}


sub script_stderr_is
{
  my($pattern, $name) = @_;
  @_ = ($stderr, $pattern, 0, 0, $name || 'stderr matches' );
  goto &_like;
}


sub script_stderr_isnt
{
  my($pattern, $name) = @_;
  @_ = ($stderr, $pattern, 0, 1, $name || 'stderr does not match' );
  goto &_like;
}


sub script_stderr_like
{
  my($pattern, $name) = @_;
  @_ = ($stderr, $pattern, 1, 0, $name || 'stderr matches' );
  goto &_like;
}


sub script_stderr_unlike
{
  my($pattern, $name) = @_;
  @_ = ($stderr, $pattern, 1, 1, $name || 'stderr does not match' );
  goto &_like;
}

######################################################################
# Support Functions

# Script params must be either a simple non-null string with the script
# name, or an array reference with one or more non-null strings.
sub _script {
  my $in = shift;
  if ( defined _STRING($in) ) {
    return [ $in ];
  }
  if ( _ARRAY($in) ) {
    unless ( scalar grep { not defined _STRING($_) } @$in ) {
      return [ @$in ];
    }
  }
  croak("Invalid command parameter");
}

# Determine any extra arguments that need to be passed into Perl.
# ATM this is just -T.
sub _perl_args {
  my($script) = @_;
  my $fh;
  my $first_line = '';
  if(open($fh, '<', $script))
  {
    $first_line = <$fh>;
    close $fh;
  }
  (grep /^-.*T/, split /\s+/, $first_line) ? ['-T'] : [];
}

# Inline some basic Params::Util functions

sub _options {
  my %options = ref($_[0]->[0]) eq 'HASH' ? %{ shift @{ $_[0] } }: ();

  $options{exit}   = 0        unless defined $options{exit};
  $options{signal} = 0        unless defined $options{signal};
  my $stdin = '';
  #$options{stdin}  = \$stdin  unless defined $options{stdin};
  $options{stdout} = \$stdout unless defined $options{stdout};
  $options{stderr} = \$stderr unless defined $options{stderr};

  if(defined $options{interpreter_options})
  {
    unless(ref $options{interpreter_options} eq 'ARRAY')
    {
      require Text::ParseWords;
      $options{interpreter_options} = [ Text::ParseWords::shellwords($options{interpreter_options}) ];
    }
  }
  else
  {
    $options{interpreter_options} = [];
  }

  \%options;
}

sub _ARRAY ($) {
  (ref $_[0] eq 'ARRAY' and @{$_[0]}) ? $_[0] : undef;
}

sub _STRING ($) {
  (defined $_[0] and ! ref $_[0] and length($_[0])) ? $_[0] : undef;
}

BEGIN {
  # Alias to old name
  *script_compiles_ok = *script_compiles;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test::Script - Basic cross-platform tests for scripts

=head1 VERSION

version 1.25

=head1 SYNOPSIS

 use Test2::V0;
 use Test::Script;
 
 script_compiles('script/myscript.pl');
 script_runs(['script/myscript.pl', '--my-argument']);
 
 done_testing;

=head1 DESCRIPTION

The intent of this module is to provide a series of basic tests for 80%
of the testing you will need to do for scripts in the F<script> (or F<bin>
as is also commonly used) paths of your Perl distribution.

Further, it aims to provide this functionality with perfect
platform-compatibility, and in a way that is as unobtrusive as possible.

That is, if the program works on a platform, then B<Test::Script>
should always work on that platform as well. Anything less than 100% is
considered unacceptable.

In doing so, it is hoped that B<Test::Script> can become a module that
you can safely make a dependency of all your modules, without risking that
your module won't on some platform because of the dependency.

Where a clash exists between wanting more functionality and maintaining
platform safety, this module will err on the side of platform safety.

=head1 FUNCTIONS

=head2 script_compiles

 script_compiles( $script, $test_name );

The L</script_compiles> test calls the script with "perl -c script.pl",
and checks that it returns without error.

The path it should be passed is a relative Unix-format script name. This
will be localised when running C<perl -c> and if the test fails the local
name used will be shown in the diagnostic output.

Note also that the test will be run with the same L<perl> interpreter that
is running the test script (and not with the default system perl). This
will also be shown in the diagnostic output on failure.

=head2 script_runs

 script_runs( $script, $test_name );
 script_runs( \@script_and_arguments, $test_name );
 script_runs( $script, \%options, $test_name );
 script_runs( \@script_and_arguments, \%options, $test_name );

The L</script_runs> test executes the script with "perl script.pl" and checks
that it returns success.

The path it should be passed is a relative unix-format script name. This
will be localised when running C<perl -c> and if the test fails the local
name used will be shown in the diagnostic output.

The test will be run with the same L<perl> interpreter that is running the
test script (and not with the default system perl). This will also be shown
in the diagnostic output on failure.

You may pass in options as a hash as the second argument.

=over 4

=item exit

The expected exit value.  The default is to use whatever indicates success
on your platform (usually 0).

=item interpreter_options

Array reference of Perl options to be passed to the interpreter.  Things
like C<-w> or C<-x> can be passed this way.  This may be either a single
string or an array reference.

=item signal

The expected signal.  The default is 0.  Use with care!  This may not be
portable, and is known not to work on Windows.

=item stdin

The input to be passed into the script via stdin.  The value may be one of

=over 4

=item simple scalar

Is considered to be a filename.

=item scalar reference

In which case the input will be drawn from the data contained in the referenced
scalar.

=back

The behavior for any other types is undefined (the current implementation uses
L<Capture::Tiny>).  Any already opened stdin will be closed.

=item stdout

Where to send the standard output to.  If you use this option, then the the
behavior of the C<script_stdout_> functions below are undefined.  The value
may be one of

=over 4

=item simple scalar

Is considered to be a filename.

=item scalar reference

=back

In which case the standard output will be places into the referenced scalar

The behavior for any other types is undefined (the current implementation uses
L<Capture::Tiny>).

=item stderr

Same as C<stdout> above, except for stderr.

=back

=head2 script_stdout_is

 script_stdout_is $expected_stdout, $test_name;

Tests if the output to stdout from the previous L</script_runs> matches the
expected value exactly.

=head2 script_stdout_isnt

 script_stdout_is $expected_stdout, $test_name;

Tests if the output to stdout from the previous L</script_runs> does NOT match the
expected value exactly.

=head2 script_stdout_like

 script_stdout_like $regex, $test_name;

Tests if the output to stdout from the previous L</script_runs> matches the regular
expression.

=head2 script_stdout_unlike

 script_stdout_unlike $regex, $test_name;

Tests if the output to stdout from the previous L</script_runs> does NOT match the regular
expression.

=head2 script_stderr_is

 script_stderr_is $expected_stderr, $test_name;

Tests if the output to stderr from the previous L</script_runs> matches the
expected value exactly.

=head2 script_stderr_isnt

 script_stderr_is $expected_stderr, $test_name;

Tests if the output to stderr from the previous L</script_runs> does NOT match the
expected value exactly.

=head2 script_stderr_like

 script_stderr_like $regex, $test_name;

Tests if the output to stderr from the previous L</script_runs> matches the regular
expression.

=head2 script_stderr_unlike

 script_stderr_unlike $regex, $test_name;

Tests if the output to stderr from the previous L</script_runs> does NOT match the regular
expression.

=head1 CAVEATS

This module is fully supported back to Perl 5.8.1.  In the near future, support
for the older pre-Test2 Test::Builer will be dropped.

The STDIN handle will be closed when using script_runs with the stdin option.
An older version used L<IPC::Run3>, which attempted to save STDIN, but
apparently this cannot be done consistently or portably.  We now use
L<Capture::Tiny> instead and explicitly do not support saving STDIN handles.

=head1 SEE ALSO

L<Test::Script::Run>, L<Test2::Suite>

=head1 AUTHOR

Original author: Adam Kennedy

Current maintainer: Graham Ollis E<lt>plicease@cpan.orgE<gt>

Contributors:

Brendan Byrd

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Adam Kennedy.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
