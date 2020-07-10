package Alien::Build::Plugin;

use strict;
use warnings;
use Module::Load ();
use Carp ();

our @CARP_NOT = qw( alienfile Alien::Build Alien::Build::Meta );

# ABSTRACT: Plugin base class for Alien::Build
our $VERSION = '1.74'; # VERSION


sub new
{
  my $class = shift;
  my %args = @_ == 1 ? ($class->meta->default => $_[0]) : @_;
  my $self = bless {}, $class;
  
  my $prop = $self->meta->prop;
  foreach my $name (keys %$prop)
  {
    $self->{$name} = defined $args{$name} 
      ? delete $args{$name} 
      : ref($prop->{$name}) eq 'CODE'
        ? $prop->{$name}->()
        : $prop->{$name};
  }
  
  foreach my $name (keys %args)
  {
    Carp::carp "$class has no $name property";
  }
  
  $self;
}


sub init
{
  my($self) = @_;
  $self;
}

sub import
{
  my($class) = @_;

  return if $class ne __PACKAGE__;

  my $caller = caller;
  { no strict 'refs'; @{ "${caller}::ISA" } = __PACKAGE__ }
  
  my $meta = $caller->meta;
  my $has = sub {
    my($name, $default) = @_;
    $meta->add_property($name, $default);
  };
  
  { no strict 'refs'; *{ "${caller}::has" } = $has }
}


sub subplugin
{
  my(undef, $name, %args) = @_;
  Carp::carp("subplugin method is deprecated");
  my $class = "Alien::Build::Plugin::$name";
  Module::Load::load($class) unless eval { $class->can('new') };
  delete $args{$_} for grep { ! defined $args{$_} } keys %args;
  $class->new(%args);
}


my %meta;
sub meta
{
  my($class) = @_;
  $class = ref $class if ref $class;
  $meta{$class} ||= Alien::Build::PluginMeta->new( class => $class );
}

package Alien::Build::PluginMeta;

sub new
{
  my($class, %args) = @_;
  my $self = bless {
    prop => {},
    %args,
  }, $class;
}

sub default
{
  my($self) = @_;
  $self->{default} || do {
    Carp::croak "No default for @{[ $self->{class} ]}";
  };
}

sub add_property
{
  my($self, $name, $default) = @_;
  my $single = $name =~ s{^(\+)}{};
  $self->{default} = $name if $single;
  $self->{prop}->{$name} = $default;

  my $accessor = sub {
    my($self, $new) = @_;
    $self->{$name} = $new if defined $new;
    $self->{$name};
  };
  
  # add the accessor
  { no strict 'refs'; *{ $self->{class} . '::' . $name} = $accessor }

  $self;
}

sub prop
{
  shift->{prop};
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Alien::Build::Plugin - Plugin base class for Alien::Build

=head1 VERSION

version 1.74

=head1 SYNOPSIS

Create your plugin:

 package Alien::Build::Plugin::Type::MyPlugin;
 
 use Alien::Build::Plugin;
 use Carp ();
 
 has prop1 => 'default value';
 has prop2 => sub { 'default value' };
 has prop3 => sub { Carp::croak 'prop3 is a required property' };
 
 sub init
 {
   my($self, $meta) = @_;

   my $prop1 = $self->prop1;
   my $prop2 = $self->prop2;
   my $prop3 = $self->prop3;
   
   $meta->register_hook(sub {
     build => [ '%{make}', '%{make} install' ],
   });
 }

From your L<alienfile>

 use alienfile;
 plugin 'Type::MyPlugin' => (
   prop2 => 'different value',
   prop3 => 'need to provide since it is required',
 );

=head1 DESCRIPTION

This document describes the L<Alien::Build> plugin base class.  For details
on how to write a plugin, see L<Alien::Build::Manual::PluginAuthor>.

Listed are some common types of plugins:

=over 4

=item L<Alien::Build::Plugin::Build>

Tools for building.

=item L<Alien::Build::Plugin::Core>

Tools already included.

=item L<Alien::Build::Plugin::Download>

Methods for retrieving from the internet.

=item L<Alien::Build::Plugin::Decode>

Normally use Download plugins which will pick the correct Decode plugins.

=item L<Alien::Build::Plugin::Extract>

Extract from archives that have been downloaded.

=item L<Alien::Build::Plugin::Fetch>

Normally use Download plugins which will pick the correct Fetch plugins.

=item L<Alien::Build::Plugin::Prefer>

Normally use Download plugins which will pick the correct Prefer plugins.

=item L<Alien::Build::Plugin::Probe>

Look for packages already installed on the system.

=back

=head1 CONSTRUCTOR

=head2 new

 my $plugin = Alien::Build::Plugin->new(%props);

=head1 METHODS

=head2 init

 $plugin->init($ab_class->meta); # $ab is an Alien::Build class name

You provide the implementation for this.  The intent is to register
hooks and set meta properties on the L<Alien::Build> class.

=head2 subplugin

B<DEPRECATED>: Maybe removed, but not before 1 October 2018.

 my $plugin2 = $plugin1->subplugin($plugin_name, %args);

Finds the given plugin and loads it using L<Module::Load> (unless already loaded)
and creats a new instance and returns it.  Most useful from a Negotiate plugin,
like this:

 sub init
 {
   my($self, $meta) = @_;
   $self->subplugin(
     'Foo::Bar',  # loads Alien::Build::Plugin::Foo::Bar,
                  # or throw exception
     foo => 1,    # these key/value pairs passsed into new
     bar => 2,    # for the plugin instance.
   )->init($meta);
 }

=head2 has

 has $prop_name;
 has $prop_name => $default;

Specifies a property of the plugin.  You may provide a default value as either
a string scalar, or a code reference.  The code reference will be called to
compute the default value, and if you want the default to be a list or hash
reference, this is how you want to do it:

 has foo => sub { [1,2,3] };

=head2 meta

 my $meta = $plugin->meta;

Returns the plugin meta object.

=head1 SEE ALSO

L<Alien::Build>, L<alienfile>, L<Alien::Build::Manual::PluginAuthor>

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
