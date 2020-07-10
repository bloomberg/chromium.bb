use 5.006;    # pragmas
use strict;
use warnings;

package Test::File::ShareDir::Object::Module;

our $VERSION = '1.001002';

# ABSTRACT: Object Oriented ShareDir creation for modules

our $AUTHORITY = 'cpan:KENTNL'; # AUTHORITY













use Class::Tiny {
  inc => sub {
    require Test::File::ShareDir::Object::Inc;
    return Test::File::ShareDir::Object::Inc->new();
  },
  modules => sub {
    return {};
  },
  root => sub {
    require Path::Tiny;
    return Path::Tiny::path(q[./])->absolute;
  },
};

use Carp qw( carp );



















sub __rcopy { require File::Copy::Recursive; goto \&File::Copy::Recursive::rcopy; }









sub module_names {
  my ( $self, ) = @_;
  return keys %{ $self->modules };
}









sub module_share_target_dir {
  my ( $self, $module ) = @_;

  $module =~ s/::/-/msxg;

  return $self->inc->module_tempdir->child($module);
}









sub module_share_source_dir {
  my ( $self, $module ) = @_;
  require Path::Tiny;
  return Path::Tiny::path( $self->modules->{$module} )->absolute( $self->root );
}









sub install_module {
  my ( $self, $module ) = @_;
  my $source = $self->module_share_source_dir($module);
  my $target = $self->module_share_target_dir($module);
  return __rcopy( $source, $target );
}









sub install_all_modules {
  my ($self) = @_;
  for my $module ( $self->module_names ) {
    $self->install_module($module);
  }
  return;
}







sub add_to_inc {
  my ($self) = @_;
  carp 'add_to_inc deprecated since 1.001000, use register';
  return $self->register;
}











sub register {
  my ($self) = @_;
  $self->inc->register;
  return;
}











sub clear {
  my ($self) = @_;
  $self->inc->clear;
  return;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test::File::ShareDir::Object::Module - Object Oriented ShareDir creation for modules

=head1 VERSION

version 1.001002

=head1 SYNOPSIS

    use Test::File::ShareDir::Object::Module;

    my $dir = Test::File::ShareDir::Object::Module->new(
        root    => "some/path",
        modules => {
            "Hello::Nurse" => "share/HN"
        },
    );

    $dir->install_all_modules;
    $dir->add_to_inc;

=head1 METHODS

=head2 C<module_names>

    my @names = $instance->module_names();

Returns the names of all modules listed in the C<modules> set.

=head2 C<module_share_target_dir>

    my $dir = $instance->module_share_target_dir("Module::Name");

Returns the path where the C<ShareDir> will be created for C<Module::Name>

=head2 C<module_share_source_dir>

    my $dir = $instance->module_share_source_dir("Module::Name");

Returns the path where the C<ShareDir> will be B<COPIED> I<FROM> for C<Module::Name>

=head2 C<install_module>

    $instance->install_module("Module::Name");

Installs C<Module::Name>'s C<ShareDir>

=head2 C<install_all_modules>

    $instance->install_all_modules();

Installs all C<module_names>.

=head2 C<add_to_inc>

B<DEPRECATED:> Use C<register> instead.

=head2 C<register>

    $instance->register();

Adds the C<Tempdir> C<ShareDir> (  C<inc> ) to the global C<@INC>

I<Since 1.001000>

=head2 C<clear>

    $instance->clear();

Removes the C<Tempdir> C<ShareDir> ( C<inc> ) from the global C<@INC>

I<Since 1.001000>

=head1 ATTRIBUTES

=head2 C<inc>

A C<Test::File::ShareDir::Object::Inc> object.

=head2 C<modules>

A hash of :

    Module::Name => "relative/path"

=head2 C<root>

The origin all paths's are relative to.

( Defaults to C<cwd> )

=begin MetaPOD::JSON v1.1.0

{
    "namespace":"Test::File::ShareDir::Object::Module",
    "interface":"class",
    "inherits":"Class::Tiny::Object"
}


=end MetaPOD::JSON

=head1 AUTHOR

Kent Fredric <kentnl@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Kent Fredric <kentnl@cpan.org>.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
