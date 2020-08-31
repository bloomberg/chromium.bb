use 5.006;    # pragmas
use strict;
use warnings;

package Test::File::ShareDir::Object::Dist;

our $VERSION = '1.001002';

# ABSTRACT: Object Oriented ShareDir creation for distributions

our $AUTHORITY = 'cpan:KENTNL'; # AUTHORITY













use Class::Tiny {
  inc => sub {
    require Test::File::ShareDir::Object::Inc;
    return Test::File::ShareDir::Object::Inc->new();
  },
  dists => sub {
    return {};
  },
  root => sub {
    require Path::Tiny;
    return Path::Tiny::path(q[./])->absolute;
  },
};

use Carp qw( carp );



















sub __rcopy { require File::Copy::Recursive; goto \&File::Copy::Recursive::rcopy; }









sub dist_names {
  my ($self) = @_;
  return keys %{ $self->dists };
}









sub dist_share_target_dir {
  my ( $self, $distname ) = @_;
  return $self->inc->dist_tempdir->child($distname);
}









sub dist_share_source_dir {
  my ( $self, $distname ) = @_;
  require Path::Tiny;
  return Path::Tiny::path( $self->dists->{$distname} )->absolute( $self->root );
}









sub install_dist {
  my ( $self, $distname ) = @_;
  my $source = $self->dist_share_source_dir($distname);
  my $target = $self->dist_share_target_dir($distname);
  return __rcopy( $source, $target );
}









sub install_all_dists {
  my ($self) = @_;
  for my $dist ( $self->dist_names ) {
    $self->install_dist($dist);
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

Test::File::ShareDir::Object::Dist - Object Oriented ShareDir creation for distributions

=head1 VERSION

version 1.001002

=head1 SYNOPSIS

    use Test::File::ShareDir::Object::Dist;

    my $dir = Test::File::ShareDir::Object::Dist->new(
        root    => "some/path",
        dists => {
            "Hello-Nurse" => "share/HN"
        },
    );

    $dir->install_all_dists;
    $dir->add_to_inc;

=head1 METHODS

=head2 C<dist_names>

    my @names = $instance->dist_names();

Returns the names of all distributions listed in the C<dists> set.

=head2 C<dist_share_target_dir>

    my $dir = $instance->dist_share_target_dir("Dist-Name");

Returns the path where the C<ShareDir> will be created for C<Dist-Name>

=head2 C<dist_share_source_dir>

    my $dir = $instance->dist_share_source_dir("Dist-Name");

Returns the path where the C<ShareDir> will be B<COPIED> I<FROM> for C<Dist-Name>

=head2 C<install_dist>

    $instance->install_dist("Dist-Name");

Installs C<Dist-Name>'s C<ShareDir>

=head2 C<install_all_dists>

    $instance->install_all_dists();

Installs all C<dist_names>

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

=head2 C<dists>

A hash of :

    Dist-Name => "relative/path"

=head2 C<root>

The origin all paths's are relative to.

( Defaults to C<cwd> )

=begin MetaPOD::JSON v1.1.0

{
    "namespace":"Test::File::ShareDir::Object::Dist",
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
