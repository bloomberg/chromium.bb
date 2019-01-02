package MooseX::NonMoose::Meta::Role::Constructor;
BEGIN {
  $MooseX::NonMoose::Meta::Role::Constructor::VERSION = '0.22';
}
use Moose::Role;
# ABSTRACT: constructor method trait for L<MooseX::NonMoose>


around can_be_inlined => sub {
    my $orig = shift;
    my $self = shift;

    my $meta = $self->associated_metaclass;
    my $super_new = $meta->find_method_by_name($self->name);
    my $super_meta = $super_new->associated_metaclass;
    if (Class::MOP::class_of($super_meta)->can('does_role')
     && Class::MOP::class_of($super_meta)->does_role('MooseX::NonMoose::Meta::Role::Class')) {
        return 1;
    }

    return $self->$orig(@_);
};

# for Moose 1.21 compatibility
sub _generate_fallback_constructor {
    my $self = shift;
    my ($class_var) = @_;
    my $new = $self->name;
    my $meta = $self->associated_metaclass;
    my $super_new_class = $meta->_find_next_nonmoose_constructor_package;
    my $arglist = $meta->find_method_by_name('FOREIGNBUILDARGS')
                ? "${class_var}->FOREIGNBUILDARGS(\@_)"
                : '@_';
    my $instance = "${class_var}->${super_new_class}::$new($arglist)";
    # XXX: the "my $__DUMMY = " part is because "return do" triggers a weird
    # bug in pre-5.12 perls (it ends up returning undef)
    "my \$__DUMMY = do {\n"
  . "    if (ref(\$_[0]) eq 'HASH') {\n"
  . "        \$_[0]->{__INSTANCE__} = $instance\n"
  . "            unless exists \$_[0]->{__INSTANCE__};\n"
  . "    }\n"
  . "    else {\n"
  . "        unshift \@_, __INSTANCE__ => $instance;\n"
  . "    }\n"
  . "    ${class_var}->Moose::Object::new(\@_);\n"
  . "}";
}

# for Moose 1.21 compatibility
sub _generate_instance {
    my $self = shift;
    my ($var, $class_var) = @_;
    my $new = $self->name;
    my $meta = $self->associated_metaclass;
    my $super_new_class = $meta->_find_next_nonmoose_constructor_package;
    my $arglist = $meta->find_method_by_name('FOREIGNBUILDARGS')
                ? "${class_var}->FOREIGNBUILDARGS(\@_)"
                : '@_';
    my $instance = "${class_var}->${super_new_class}::$new($arglist)";
    return "my $var = $instance;\n"
         . "if (!Scalar::Util::blessed($var)) {\n"
         . "    " . $self->_inline_throw_error(
               "'The constructor for $super_new_class did not return a blessed instance'"
           ) . ";\n"
         . "}\n"
         . "elsif (!$var->isa($class_var)) {\n"
         . "    if (!$class_var->isa(Scalar::Util::blessed($var))) {\n"
         . "        " . $self->_inline_throw_error(
               "\"The constructor for $super_new_class returned an object whose class is not a parent of $class_var\""
           ) . ";\n"
         . "    }\n"
         . "    else {\n"
         . "        " . $self->associated_metaclass->inline_rebless_instance($var, $class_var) . ";\n"
         . "    }\n"
         . "}\n";
}

no Moose::Role;

1;

__END__
=pod

=head1 NAME

MooseX::NonMoose::Meta::Role::Constructor - constructor method trait for L<MooseX::NonMoose>

=head1 VERSION

version 0.22

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
      return Class::MOP::class_of($options{for_class});
  }

=head1 DESCRIPTION

This trait implements inlining of the constructor for classes using the
L<MooseX::NonMoose::Meta::Role::Class> metaclass trait; it has no effect unless
that trait is also used. See those docs and the docs for L<MooseX::NonMoose>
for more information.

=head1 AUTHOR

Jesse Luehrs <doy at tozt dot net>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by Jesse Luehrs.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

