package Alien::Build::Plugin::PkgConfig::CommandLine;

use strict;
use warnings;
use Alien::Build::Plugin;
use Carp ();

# ABSTRACT: Probe system and determine library or tool properties using the pkg-config command line interface
our $VERSION = '1.74'; # VERSION


has '+pkg_name' => sub {
  Carp::croak "pkg_name is a required property";
};

# NOT used, for compat with other PkgConfig plugins
has register_prereqs => 1;

sub _bin_name {

  # We prefer pkgconf to pkg-config because it seems to be the future.

  require File::Which;
  File::Which::which($ENV{PKG_CONFIG})
    ? $ENV{PKG_CONFIG}
    : File::Which::which('pkgconf')
      ? 'pkgconf'
      : File::Which::which('pkg-config')
        ? 'pkg-config'
        : undef;
};

has bin_name => \&_bin_name;


has atleast_version => undef;


has exact_version => undef;


has max_version => undef;


has minimum_version => undef;

sub _val
{
  my($build, $args, $prop_name) = @_;
  my $string = $args->{out};
  chomp $string;
  $string =~ s{^\s+}{};
  if($prop_name =~ /version$/)
  { $string =~ s{\s*$}{} }
  else
  { $string =~ s{\s*$}{ } }
  if($prop_name =~ /^(.*?)\.(.*?)\.(.*?)$/)
  { $build->runtime_prop->{$1}->{$2}->{$3} = $string }
  else
  { $build->runtime_prop->{$prop_name} = $string }
  ();
}


sub available
{
  !!_bin_name();
}

sub init
{
  my($self, $meta) = @_;

  my $pkgconf = $self->bin_name;

  unless(defined $meta->prop->{env}->{PKG_CONFIG})
  {
    $meta->prop->{env}->{PKG_CONFIG} = $pkgconf;
  }

  my($pkg_name, @alt_names) = (ref $self->pkg_name) ? (@{ $self->pkg_name }) : ($self->pkg_name);

  my @probe = map { [$pkgconf, '--exists', $_] } ($pkg_name, @alt_names);

  if(defined $self->minimum_version)
  {
    push @probe, [ $pkgconf, '--atleast-version=' . $self->minimum_version, $pkg_name ];
  }
  elsif(defined $self->atleast_version)
  {
    push @probe, [ $pkgconf, '--atleast-version=' . $self->atleast_version, $pkg_name ];
  }

  if(defined $self->exact_version)
  {
    push @probe, [ $pkgconf, '--exact-version=' . $self->exact_version, $pkg_name ];
  }

  if(defined $self->max_version)
  {
    push @probe, [ $pkgconf, '--max-version=' . $self->max_version, $pkg_name ];
  }

  push @probe, [ $pkgconf, '--modversion', $pkg_name, sub {
    my($build, $args) = @_;
    my $version = $args->{out};
    $version =~ s{^\s+}{};
    $version =~ s{\s*$}{};
    $build->hook_prop->{version} = $version;
  }];

  unshift @probe, sub {
    my($build) = @_;
    $build->runtime_prop->{legacy}->{name} ||= $pkg_name;
  };

  $meta->register_hook(
    probe => \@probe
  );

  my @gather = map { [ $pkgconf, '--exists', $_] } ($pkg_name, @alt_names);

  foreach my $prop_name (qw( cflags libs version ))
  {
    my $flag = $prop_name eq 'version' ? '--modversion' : "--$prop_name";
    push @gather,
      [ $pkgconf, $flag, $pkg_name, sub { _val @_, $prop_name } ];
    if(@alt_names)
    {
      foreach my $alt ($pkg_name, @alt_names)
      {
        push @gather,
          [ $pkgconf, $flag, $alt, sub { _val @_, "alt.$alt.$prop_name" } ];
      }
    }
  }

  foreach my $prop_name (qw( cflags libs ))
  {
    push @gather,
      [ $pkgconf, '--static', "--$prop_name", $pkg_name, sub { _val @_, "${prop_name}_static" } ];
    if(@alt_names)
    {
      foreach my $alt ($pkg_name, @alt_names)
      {
        push @gather,
          [ $pkgconf, '--static', "--$prop_name", $alt, sub { _val @_, "alt.$alt.${prop_name}_static" } ];
      }
    }
  }

  $meta->register_hook(gather_system => [@gather]);

  if($meta->prop->{platform}->{system_type} eq 'windows-mingw')
  {
    @gather = map {
      my($pkgconf, @rest) = @$_;
      [$pkgconf, '--dont-define-prefix', @rest],
    } @gather;
  }

  $meta->register_hook(gather_share => [@gather]);

  $meta->after_hook(
    $_ => sub {
      my($build) = @_;
      if(keys %{ $build->runtime_prop->{alt} } < 2)
      {
        delete $build->runtime_prop->{alt};
      }
    },
  ) for qw( gather_system gather_share );

  $self;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Alien::Build::Plugin::PkgConfig::CommandLine - Probe system and determine library or tool properties using the pkg-config command line interface

=head1 VERSION

version 1.74

=head1 SYNOPSIS

 use alienfile;
 plugin 'PkgConfig::CommandLine' => (
   pkg_name => 'libfoo',
 );

=head1 DESCRIPTION

Note: in most case you will want to use L<Alien::Build::Plugin::Download::Negotiate>
instead.  It picks the appropriate fetch plugin based on your platform and environment.
In some cases you may need to use this plugin directly instead.

This plugin provides Probe and Gather steps for pkg-config based packages.  It uses
the best command line tools to accomplish this task.

=head1 PROPERTIES

=head2 pkg_name

The package name.  If this is a list reference then .pc files with all those package
names must be present.

=head2 atleast_version

The minimum required version that is acceptable version as provided by the system.

=head2 exact_version

The exact required version that is acceptable version as provided by the system.

=head2 max_version

The max required version that is acceptable version as provided by the system.

=head2 minimum_version

Alias for C<atleast_version> for backward compatibility.

=head1 METHODS

=head2 available

 my $bool = Alien::Build::Plugin::PkgConfig::CommandLine->available;

Returns true if the necessary prereqs for this plugin are I<already> installed.

=head1 SEE ALSO

L<Alien::Build::Plugin::PkgConfig::Negotiate>, L<Alien::Build>, L<alienfile>, L<Alien::Build::MM>, L<Alien>

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
