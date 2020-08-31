package Alien::Build::Plugin::Core::Setup;

use strict;
use warnings;
use Alien::Build::Plugin;
use Config;
use File::Which qw( which );

# ABSTRACT: Core setup plugin
our $VERSION = '1.74'; # VERSION


sub init
{
  my($self, $meta) = @_;  
  $meta->prop->{platform} ||= {};
  $self->_platform($meta->prop->{platform});
}

sub _platform
{
  my(undef, $hash) = @_;
  
  if($^O eq 'MSWin32' && $Config{ccname} eq 'cl')
  {
    $hash->{compiler_type} = 'microsoft';
  }
  else
  {
    $hash->{compiler_type} = 'unix';
  }
  
  if($^O eq 'MSWin32')
  {
    $hash->{system_type} = 'windows-unknown';

    if(defined &Win32::BuildNumber)
    {
      $hash->{system_type} = 'windows-activestate';
    }
    elsif($Config{myuname} =~ /strawberry-perl/)
    {
      $hash->{system_type} = 'windows-strawberry';
    }
    elsif($hash->{compiler_type} eq 'microsoft')
    {
      $hash->{system_type} = 'windows-microsoft';
    }
    else
    {
      my $uname_exe = which('uname');
      if($uname_exe)
      {
        my $uname = `$uname_exe`;
        if($uname =~ /^(MINGW)(32|64)_NT/)
        {
          $hash->{system_type} = 'windows-' . lc $1;
        }
      }
    }
  }
  elsif($^O =~ /^(VMS)$/)
  {
    # others probably belong in here...
    $hash->{system_type} = lc $^O;
  }
  else
  {
    $hash->{system_type} = 'unix';
  }
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Alien::Build::Plugin::Core::Setup - Core setup plugin

=head1 VERSION

version 1.74

=head1 SYNOPSIS

 use alienfile;
 # already loaded

=head1 DESCRIPTION

This plugin does some core setup for you.

=head1 SEE ALSO

L<Alien::Build>, L<Alien::Base::ModuleBuild>

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
