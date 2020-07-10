package Alien::Base::Wrapper;

use strict;
use warnings;
use 5.008001;
use Config;
use Text::ParseWords qw( shellwords );

# NOTE: Although this module is now distributed with Alien-Build,
# it should have NO non-perl-core dependencies for all Perls 
# 5.8.1-5.26.0 (as of this writing, and any Perl more recent).
# You should be able to extract this module from the rest of
# Alien-Build and use it by itself.
# ALSO NOTE: Although it was lost in the spin-off, this used
# to be called Alien::Build::Wrapper, and was originally
# distributed with Alien-Build.

# ABSTRACT: Compiler and linker wrapper for Alien
our $VERSION = '1.74'; # VERSION


my @cflags_I;
my @cflags_other;
my @ldflags_L;
my @ldflags_l;
my @ldflags_other;
my @mm;
my @mb;

sub _reset
{
  @cflags_I      = ();
  @cflags_other  = ();
  @ldflags_L     = ();
  @ldflags_l     = ();
  @ldflags_other = ();
  @mm            = ();
  @mb            = ();
}


sub _myexec
{
  my @command = @_;
  if($^O eq 'MSWin32')
  {
    # To handle weird quoting on MSWin32
    # this logic needs to be improved.
    my $command = "@command";
    $command =~ s{"}{\\"}g;
    system $command;

    if($? == -1 )
    {
      die "failed to execute: $!\n";
    }
    elsif($? & 127)
    {
      die "child died with signal @{[ $? & 128 ]}";
    }
    else
    {
      exit($? >> 8);
    }
  }
  else
  {
    exec @command;
  }
}

sub cc
{
  my @command = (
    shellwords($Config{cc}),
    @cflags_I,
    @cflags_other,
    @ARGV,
  );
  print "@command\n" unless $ENV{ALIEN_BASE_WRAPPER_QUIET};
  _myexec @command;
}


sub ld
{
  my @command = (
    shellwords($Config{ld}),
    @ldflags_L,
    @ldflags_other,
    @ARGV,
    @ldflags_l,
  );
  print "@command\n" unless $ENV{ALIEN_BASE_WRAPPER_QUIET};
  _myexec @command;
}


sub mm_args
{
  @mm;
}


sub mb_args
{
  @mb;
}

sub _join
{
  join ' ', map { s/(\s)/\\$1/g; $_ } @_;
}

sub import
{
  my(undef, @aliens) = @_;

  my $export = 1;

  foreach my $alien (@aliens)
  {
    if($alien eq '!export')
    {
      $export = 0;
      next;
    }
    $alien = "Alien::$alien" unless $alien =~ /::/;
    my $alien_pm = $alien . '.pm';
    $alien_pm =~ s/::/\//g;
    require $alien_pm unless eval { $alien->can('cflags') } && eval { $alien->can('libs') };
    my $cflags;
    my $libs;
    if($alien->install_type eq 'share' && $alien->can('cflags_static'))
    {
      $cflags = $alien->cflags_static;
      $libs   = $alien->libs_static;
    }
    else
    {
      $cflags = $alien->cflags;
      $libs   = $alien->libs;
    }
    
    push @cflags_I,     grep  /^-I/, shellwords $cflags;
    push @cflags_other, grep !/^-I/, shellwords $cflags;
    
    push @ldflags_L,     grep  /^-L/,    shellwords $libs;
    push @ldflags_l,     grep  /^-l/,    shellwords $libs;
    push @ldflags_other, grep !/^-[Ll]/, shellwords $libs;
  }
  
  my @cflags_define = grep  /^-D/, @cflags_other;
  my @cflags_other2 = grep !/^-D/, @cflags_other;
  
  @mm = ();

  push @mm, INC       => _join @cflags_I                             if @cflags_I;
  push @mm, CCFLAGS   => _join(@cflags_other2) . " $Config{ccflags}" if @cflags_other2;
  push @mm, DEFINE    => _join(@cflags_define)                       if @cflags_define;

  # TODO: handle spaces in -L paths
  push @mm, LIBS      => ["@ldflags_L @ldflags_l"];
  my @ldflags = (@ldflags_L, @ldflags_other);
  push @mm, LDDLFLAGS => _join(@ldflags) . " $Config{lddlflags}"     if @ldflags;
  push @mm, LDFLAGS   => _join(@ldflags) . " $Config{ldflags}"       if @ldflags;

  @mb = ();
  
  push @mb, extra_compiler_flags => _join(@cflags_I, @cflags_other);
  push @mb, extra_linker_flags   => _join(@ldflags_l);
  
  if(@ldflags)
  {
    push @mb, config => {
      lddlflags => _join(@ldflags) . " $Config{lddlflags}",
      ldflags   => _join(@ldflags) . " $Config{ldflags}",
    },
  }
  
  if($export)
  {
    my $caller = caller;
    no strict 'refs';
    *{"${caller}::cc"} = \&cc;
    *{"${caller}::ld"} = \&ld;
  }  
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Alien::Base::Wrapper - Compiler and linker wrapper for Alien

=head1 VERSION

version 1.74

=head1 SYNOPSIS

From the command line:

 % perl -MAlien::Base::Wrapper=Alien::Foo,Alien::Bar -e cc -- -o foo.o -c foo.c
 % perl -MAlien::Base::Wrapper=Alien::Foo,Alien::Bar -e ld -- -o foo foo.o

From Makefile.PL (non-dynamic):

 use ExtUtils::MakeMaker;
 use Alien::Base::Wrapper qw( Alien::Foo Alien::Bar !export );
 
 WriteMakefile(
   'NAME'              => 'Foo::XS',
   'VERSION_FROM'      => 'lib/Foo/XS.pm',
   'CONFIGURE_REQUIRES => {
     'ExtUtils::MakeMaker' => 6.52,
     'Alien::Foo'          => 0,
     'Alien::Bar'          => 0,
   },
   Alien::Base::Wrapper->mm_args,
 );

From Makefile.PL (dynamic):

 use Devel::CheckLib qw( check_lib );
 use ExtUtils::MakeMaker 6.52;
 
 my @mm_args;
 my @libs;
 
 if(check_lib( lib => [ 'foo' ] )
 {
   push @mm_args, LIBS => [ '-lfoo' ];
 }
 else
 {
   push @mm_args,
     CC => '$(FULLPERL) -MAlien::Base::Wrapper=Alien::Foo -e cc --',
     LD => '$(FULLPERL) -MAlien::Base::Wrapper=Alien::Foo -e ld --',
     BUILD_REQUIRES => {
       'Alien::Foo'           => 0,
       'Alien::Base::Wrapper' => 0,
     }
   ;
 }

 WriteMakefile(
   'NAME'         => 'Foo::XS',
   'VERSION_FROM' => 'lib/Foo/XS.pm',
   'CONFIGURE_REQUIRES => {
     'ExtUtils::MakeMaker' => 6.52,
   },
   @mm_args,
 );

=head1 DESCRIPTION

This module acts as a wrapper around one or more L<Alien> modules.  It is designed to work
with L<Alien::Base> based aliens, but it should work with any L<Alien> which uses the same
essential API.

In the first example (from the command line), this class acts as a wrapper around the
compiler and linker that Perl is configured to use.  It takes the normal compiler and
linker flags and adds the flags provided by the Aliens specified, and then executes the
command.  It will print the command to the console so that you can see exactly what is
happening.

In the second example (from Makefile.PL non-dynamic), this class is used to generate the
appropriate L<ExtUtils::MakeMaker> (EUMM) arguments needed to C<WriteMakefile>.

In the third example (from Makefile.PL dynamic), we do a quick check to see if the simple
linker flag C<-lfoo> will work, if so we use that.  If not, we use a wrapper around the
compiler and linker that will use the alien flags that are known at build time.  The
problem that this form attempts to solve is that compiler and linker flags typically
need to be determined at I<configure> time, when a distribution is installed, meaning
if you are going to use an L<Alien> module then it needs to be a configure prerequisite,
even if the library is already installed and easily detected on the operating system.

The author of this module believes that the third (from Makefile.PL dynamic) form is
somewhat unnecessary.  L<Alien> modules based on L<Alien::Base> have a few prerequisites,
but they are well maintained and reliable, so while there is a small cost in terms of extra
dependencies, the overall reliability thanks to reduced overall complexity.

=head1 FUNCTIONS

=head2 cc

 % perl -MAlien::Base::Wrapper=Alien::Foo -e cc -- cflags

Invoke the C compiler with the appropriate flags from C<Alien::Foo> and what
is provided on the command line.

=head2 ld

 % perl -MAlien::Base::Wrapper=Alien::Foo -e ld -- ldflags

Invoke the linker with the appropriate flags from C<Alien::Foo> and what
is provided on the command line.

=head2 mm_args

 my %args = Alien::Base::Wrapper->mm_args;

Returns arguments that you can pass into C<WriteMakefile> to compile/link against
the specified Aliens.

=head2 mb_args

 my %args = Alien::Base::Wrapper->mb_args;

Returns arguments that you can pass into the constructor to L<Module::Build>.

=head1 ENVIRONMENT

Alien::Base::Wrapper responds to these environment variables:

=over 4

=item ALIEN_BASE_WRAPPER_QUIET

If set to true, do not print the command before executing

=back

=head1 SEE ALSO

L<Alien::Base>, L<Alien::Base>

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
