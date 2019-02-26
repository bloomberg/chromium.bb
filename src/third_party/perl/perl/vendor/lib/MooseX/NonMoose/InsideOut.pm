package MooseX::NonMoose::InsideOut;
BEGIN {
  $MooseX::NonMoose::InsideOut::VERSION = '0.22';
}
use Moose::Exporter;
# ABSTRACT: easy subclassing of non-Moose non-hashref classes


my ($import, $unimport, $init_meta) = Moose::Exporter->build_import_methods(
    class_metaroles => {
        class       => ['MooseX::NonMoose::Meta::Role::Class'],
        constructor => ['MooseX::NonMoose::Meta::Role::Constructor'],
        instance    => ['MooseX::InsideOut::Role::Meta::Instance'],
    },
    install => [qw(import unimport)],
);

sub init_meta {
    my $package = shift;
    my %options = @_;
    my $meta = Class::MOP::class_of($options{for_class});
    Carp::cluck('Roles have no use for MooseX::NonMoose')
        if $meta && $meta->isa('Moose::Meta::Role');
    $package->$init_meta(@_);
}


1;

__END__
=pod

=head1 NAME

MooseX::NonMoose::InsideOut - easy subclassing of non-Moose non-hashref classes

=head1 VERSION

version 0.22

=head1 SYNOPSIS

  package Term::VT102::NBased;
  use Moose;
  use MooseX::NonMoose::InsideOut;
  extends 'Term::VT102';

  has [qw/x_base y_base/] => (
      is      => 'ro',
      isa     => 'Int',
      default => 1,
  );

  around x => sub {
      my $orig = shift;
      my $self = shift;
      $self->$orig(@_) + $self->x_base - 1;
  };

  # ... (wrap other methods)

  no Moose;
  # no need to fiddle with inline_constructor here
  __PACKAGE__->meta->make_immutable;

  my $vt = Term::VT102::NBased->new(x_base => 0, y_base => 0);

=head1 DESCRIPTION

=for Pod::Coverage   init_meta

=head1 AUTHOR

Jesse Luehrs <doy at tozt dot net>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by Jesse Luehrs.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

