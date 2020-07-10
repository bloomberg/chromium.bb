use 5.006;    # pragmas
use strict;
use warnings;

package Test::File::ShareDir::Object::Inc;

our $VERSION = '1.001002';

# ABSTRACT: Shared tempdir object code to inject into @INC

our $AUTHORITY = 'cpan:KENTNL'; # AUTHORITY













my @cache;

use Class::Tiny {
  tempdir => sub {
    require Path::Tiny;
    my $dir = Path::Tiny::tempdir( CLEANUP => 1 );
    push @cache, $dir;    # explicit keepalive
    return $dir;
  },
  module_tempdir => sub {
    my ($self) = @_;
    my $dir = $self->tempdir->child('auto/share/module');
    $dir->mkpath();
    return $dir->absolute;
  },
  dist_tempdir => sub {
    my ($self) = @_;
    my $dir = $self->tempdir->child('auto/share/dist');
    $dir->mkpath();
    return $dir->absolute;
  },
};
use Carp qw( carp );



















sub add_to_inc {
  my ($self) = @_;
  carp 'add_to_inc deprecated sice 1.001000, use register instead';
  return $self->register;
}













sub register {
  my ($self) = @_;
  unshift @INC, $self->tempdir->stringify;
  return;
}













sub clear {
  my ($self) = @_;
  ## no critic (Variables::RequireLocalizedPunctuationVars)
  @INC = grep { ref or $_ ne $self->tempdir->stringify } @INC;
  return;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test::File::ShareDir::Object::Inc - Shared tempdir object code to inject into @INC

=head1 VERSION

version 1.001002

=head1 SYNOPSIS

    use Test::File::ShareDir::Object::Inc;

    my $inc = Test::File::ShareDir::Object::Inc->new();

    $inc->tempdir() # add files to here

    $inc->module_tempdir() # or here

    $inc->dist_tempdir() # or here

    $inc->add_to_inc;

=head1 DESCRIPTION

This class doesn't do very much on its own.

It simply exists to facilitate C<tempdir> creation,
and the injection of those C<tempdir>'s into C<@INC>

=head1 METHODS

=head2 C<add_to_inc>

B<DEPRECATED:> Use C<register> instead.

=head2 C<register>

    $instance->register;

Allows this C<Inc> to be used.

Presently, this injects the associated C<tempdir> into C<@INC>

I<Since 1.001000>

=head2 C<clear>

    $instance->clear();

Prevents this C<Inc> from being used.

Presently, this removes the C<tempdir> from C<@INC>

I<Since 1.001000>

=head1 ATTRIBUTES

=head2 C<tempdir>

A path to a C<tempdir> of some description.

=head2 C<module_tempdir>

The C<module> C<ShareDir> base directory within the C<tempdir>

=head2 C<dist_tempdir>

The C<dist> C<ShareDir> base directory within the C<tempdir>

=begin MetaPOD::JSON v1.1.0

{
    "namespace":"Test::File::ShareDir::Object::Inc",
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
