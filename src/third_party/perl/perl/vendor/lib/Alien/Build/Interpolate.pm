package Alien::Build::Interpolate;

use strict;
use warnings;

# ABSTRACT: Advanced interpolation engine for Alien builds
our $VERSION = '1.74'; # VERSION


sub new
{
  my($class) = @_;
  my $self = bless {
    helper  => {},
    classes => {},
  }, $class;
  $self;
}


sub add_helper
{
  my $self = shift;
  my $name = shift;
  my $code = shift;

  if(defined $self->{helper}->{$name}->{code})
  {
    require Carp;
    Carp::croak("duplicate implementation for interpolated key $name");
  }
  
  while(@_)
  {
    my $module = shift;
    my $version = shift;
    $version ||= 0;
    $self->{helper}->{$name}->{require}->{$module} = $version;
  }
  
  $self->{helper}->{$name}->{code} = $code;
}


sub replace_helper
{
  my $self = shift;
  my($name) = @_;
  delete $self->{helper}->{$name};
  $self->add_helper(@_);
}


sub has_helper
{
  my($self, $name) = @_;

  foreach my $module (keys %{ $self->{helper}->{$name}->{require} })
  {
    my $version = $self->{helper}->{$name}->{require}->{$module};

    # yeah we do have to eval every time in case $version is different
    # from the last load.
    eval qq{ use $module @{[ $version ? $version : '' ]} (); 1 };
    die $@ if $@;

    unless($self->{classes}->{$module})
    {
      if($module->can('alien_helper'))
      {
        my $helpers = $module->alien_helper;
        while(my($k,$v) = each %$helpers)
        {
          $self->{helper}->{$k}->{code} = $v;
        }
      }
      $self->{classes}->{$module} = 1;
    }
  }
  
  my $code = $self->{helper}->{$name}->{code};
  
  return unless defined $code;

  if(ref($code) ne 'CODE')
  {
    my $perl = $code;
    package Alien::Build::Interpolate::Helper;
    $code = sub {
      my $value = eval $perl;
      die $@ if $@;
      $value;
    };
  }
  
  $code;
}


sub execute_helper
{
  my($self, $name) = @_;
  
  my $code = $self->has_helper($name);
  die "no helper defined for $name" unless defined $code;
  
  $code->();
}


sub _get_prop
{
  my($name, $prop, $orig) = @_;
  
  $name =~ s/^\./alien./;
  
  if($name =~ /^(.*?)\.(.*)$/)
  {
    my($key,$rest) = ($1,$2);
    return _get_prop($rest, $prop->{$key}, $orig);
  }
  elsif(exists $prop->{$name})
  {
    return $prop->{$name};
  }
  else
  {
    require Carp;
    Carp::croak("No property $orig is defined");
  }
}

sub interpolate
{
  my($self, $string, $prop) = @_;
  $prop ||= {};
  
  $string =~ s{(?<!\%)\%\{([a-zA-Z_][a-zA-Z_0-9]+)\}}{$self->execute_helper($1)}eg;
  $string =~ s{(?<!\%)\%\{([a-zA-Z_\.][a-zA-Z_0-9\.]+)\}}{_get_prop($1,$prop,$1)}eg;
  $string =~ s/\%(?=\%)//g;
  $string;
}


sub requires
{
  my($self, $string) = @_;
  map {
    my $name = $_;
    map { $_ => $self->{helper}->{$name}->{require}->{$_} || 0 } keys %{ $self->{helper}->{$name}->{require} }
  } $string =~ m{(?<!\%)\%\{([a-zA-Z_][a-zA-Z_0-9]+)\}}g;
}


sub clone
{
  my($self) = @_;
  
  require Storable;
  
  my %help;
  foreach my $helper (keys %{ $self->{helper} })
  {
    $help{$helper}->{code}    = $self->{helper}->{$helper}->{code};
    $help{$helper}->{require} = $self->{helper}->{$helper}->{require };
  }

  my $new = bless {
    helper => \%help,
    classes => Storable::dclone($self->{classes}),
  }, ref $self;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Alien::Build::Interpolate - Advanced interpolation engine for Alien builds

=head1 VERSION

version 1.74

=head1 CONSTRUCTOR

=head2 new

 my $intr = Alien::Build::Interpolate->new;

=head2 add_helper

 $intr->add_helper($name => $code);
 $intr->add_helper($name => $code, %requirements);

=head2 replace_helper

 $intr->replace_helper($name => $code);
 $intr->replace_helper($name => $code, %requirements);

=head2 has_helper

 my $coderef = $intr->has_helper($name);

Used to discover if a helper exists with the given name.
Returns the code reference.

=head2 execute_helper

 my $value = $intr->execute_helper($name);

=head2 interpolate

 my $string = $intr->interpolate($template);

=head2 requires

 my %requires = $intr->requires($template);

=head2 clone

 my $intr2 = $intr->clone;

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
