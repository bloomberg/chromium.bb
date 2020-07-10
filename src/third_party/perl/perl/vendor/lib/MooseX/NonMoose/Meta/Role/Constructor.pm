package MooseX::NonMoose::Meta::Role::Constructor;
BEGIN {
  $MooseX::NonMoose::Meta::Role::Constructor::AUTHORITY = 'cpan:DOY';
}
{
  $MooseX::NonMoose::Meta::Role::Constructor::VERSION = '0.26';
}
use Moose::Role 2.0000;
# ABSTRACT: constructor method trait for L<MooseX::NonMoose>


around can_be_inlined => sub {
    my $orig = shift;
    my $self = shift;

    my $meta = $self->associated_metaclass;
    my $super_new = $meta->find_method_by_name($self->name);
    my $super_meta = $super_new->associated_metaclass;
    if (Moose::Util::find_meta($super_meta)->can('does_role')
     && Moose::Util::find_meta($super_meta)->does_role('MooseX::NonMoose::Meta::Role::Class')) {
        return 1;
    }

    return $self->$orig(@_);
};

no Moose::Role;

1;

__END__

=pod

=head1 NAME

MooseX::NonMoose::Meta::Role::Constructor - constructor method trait for L<MooseX::NonMoose>

=head1 VERSION

version 0.26

=head1 SYNOPSIS

  package My::Moose;
  use Moose ();
  use Moose::Exporter;

  Moose::Exporter->setup_import_methods;
  sub init_meta {
      shift;
      my %options = @_;
      Moose->init_meta(%options);
      Moose::Util::MetaRole::apply_metaclass_roles(
          for_class               => $options{for_class},
          metaclass_roles         => ['MooseX::NonMoose::Meta::Role::Class'],
          constructor_class_roles =>
              ['MooseX::NonMoose::Meta::Role::Constructor'],
      );
      return Moose::Util::find_meta($options{for_class});
  }

=head1 DESCRIPTION

This trait implements inlining of the constructor for classes using the
L<MooseX::NonMoose::Meta::Role::Class> metaclass trait; it has no effect unless
that trait is also used. See those docs and the docs for L<MooseX::NonMoose>
for more information.

=head1 AUTHOR

Jesse Luehrs <doy@tozt.net>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2014 by Jesse Luehrs.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
