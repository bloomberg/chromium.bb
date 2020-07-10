package Alien::Build::Plugin::Probe::CBuilder;

use strict;
use warnings;
use Alien::Build::Plugin;
use File::chdir;
use File::Temp ();
use Capture::Tiny qw( capture_merged capture );

# ABSTRACT: Probe for system libraries by guessing with ExtUtils::CBuilder
our $VERSION = '1.74'; # VERSION


has options => sub { {} };


has cflags  => '';


has libs    => '';


has program => 'int main(int argc, char *argv[]) { return 0; }';


has version => undef;


has aliens => [];


has lang => 'C';

sub init
{
  my($self, $meta) = @_;
  
  $meta->add_requires('configure' => 'ExtUtils::CBuilder' => 0 );  

  if(@{ $self->aliens })
  {  
    die "You can't specify both 'aliens' and either 'cflags' or 'libs' for the Probe::CBuilder plugin" if $self->cflags || $self->libs;

    $meta->add_requires('configure' => $_ => 0 ) for @{ $self->aliens };
    $meta->add_requires('Alien::Build::Plugin::Probe::CBuilder' => '0.53');

    my $cflags = '';
    my $libs   = '';
    foreach my $alien (@{ $self->aliens })
    {
      require Module::Load;
      Module::Load::load($alien);
      $cflags .= $alien->cflags . ' ';
      $libs   .= $alien->libs   . ' ';
    }
    $self->cflags($cflags);
    $self->libs($libs);
  }
  
  my @cpp;
  
  if($self->lang ne 'C')
  {
    $meta->add_requires('Alien::Build::Plugin::Probe::CBuilder' => '0.53');
    @cpp = ('C++' => 1) if $self->lang eq 'C++';
  }
  
  $meta->register_hook(
    probe => sub {
      my($build) = @_;
      local $CWD = File::Temp::tempdir( CLEANUP => 1 );
      
      open my $fh, '>', 'mytest.c';
      print $fh $self->program;
      close $fh;
      
      $build->log("trying: cflags=@{[ $self->cflags ]} libs=@{[ $self->libs ]}");
      
      my $b = ExtUtils::CBuilder->new(%{ $self->options });

      my($out1, $obj) = capture_merged { eval {
        $b->compile(
          source               => 'mytest.c',
          extra_compiler_flags => $self->cflags,
          @cpp,
        );
      } };
      
      if(my $error = $@)
      {
        $build->log("compile failed: $error");
        $build->log("compile failed: $out1");
        die $@;
      }
      
      my($out2, $exe) = capture_merged { eval {
        $b->link_executable(
          objects              => [$obj],
          extra_linker_flags   => $self->libs,
        );
      } };
      
      if(my $error = $@)
      {
        $build->log("link failed: $error");
        $build->log("link failed: $out2");
        die $@;
      }      
      
      my($out, $err, $ret) = capture { system($^O eq 'MSWin32' ? $exe : "./$exe") };
      die "execute failed" if $ret;
      
      my $cflags = $self->cflags;
      my $libs   = $self->libs;
      
      $cflags =~ s{\s*$}{ };
      $libs =~ s{\s*$}{ };
      
      $build->install_prop->{plugin_probe_cbuilder_gather} = {
        cflags  => $cflags,
        libs    => $libs,
      };
      
      if(defined $self->version)
      {
        my($version) = $out =~ $self->version;
        $build->hook_prop->{version} = $version;
        $build->install_prop->{plugin_probe_cbuilder_gather}->{version} = $version;
      }
      
      'system';
    }
  );

  $meta->register_hook(
    gather_system => sub {
      my($build) = @_;
      if(my $p = $build->install_prop->{plugin_probe_cbuilder_gather})
      {
        $build->runtime_prop->{$_} = $p->{$_} for keys %$p;
      }
      else
      {
        die "cbuilder unable to gather; if you are using multiple probe steps you may need to provide your own gather.";
      }
    },
  );
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Alien::Build::Plugin::Probe::CBuilder - Probe for system libraries by guessing with ExtUtils::CBuilder

=head1 VERSION

version 1.74

=head1 SYNOPSIS

 use alienfile;
 plugin 'Probe::CBuilder' => (
   cflags => '-I/opt/libfoo/include',
   libs   => '-L/opt/libfoo/lib -lfoo',
 );

alternately:

 ues alienfile;
 plugin 'Probe::CBuilder' => (
   aliens => [ 'Alien::libfoo', 'Alien::libbar' ],
 );

=head1 DESCRIPTION

This plugin probes for compiler and linker flags using L<ExtUtils::CBuilder>.  This is a useful
alternative to L<Alien::Build::Plugin::PkgConfig::Negotiate> for packages that do not provide
a pkg-config C<.pc> file, or for when those C<.pc> files may not be available.  (For example,
on FreeBSD, C<libarchive> is a core part of the operating system, but doesn't include a C<.pc>
file which is usually provided when you install the C<libarchive> package on Linux).

=head1 PROPERTIES

=head2 options

Any extra options that you want to have passed into the constructor to L<ExtUtils::CBuilder>.

=head2 cflags

The compiler flags.

=head2 libs

The linker flags

=head2 program

The program to use in the test.

=head2 version

This is a regular expression to parse the version out of the output from the
test program.

=head2 aliens

List of aliens to query fro compiler and linker flags.

=head2 lang

The programming language to use.  One of either C<C> or C<C++>.

=head1 SEE ALSO

L<Alien::Build>, L<alienfile>, L<Alien::Build::MM>, L<Alien>

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
