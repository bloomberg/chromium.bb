package MooseX::NonMoose::Meta::Role::Class;
BEGIN {
  $MooseX::NonMoose::Meta::Role::Class::VERSION = '0.22';
}
use Moose::Role;
use List::MoreUtils qw(any);
# ABSTRACT: metaclass trait for L<MooseX::NonMoose>


has has_nonmoose_constructor => (
    is      => 'rw',
    isa     => 'Bool',
    default => 0,
);

has has_nonmoose_destructor => (
    is  => 'rw',
    isa => 'Bool',
    default => 0,
);

# overrides the constructor_name attr that already exists
has constructor_name => (
    is      => 'rw',
    isa     => 'Str',
    lazy    => 1,
    default => sub { shift->throw_error("No constructor name has been set") },
);

# XXX ugh, really need to fix this in moose
around reinitialize => sub {
    my $orig = shift;
    my $class = shift;
    my ($pkg) = @_;

    my $meta = blessed($pkg) ? $pkg : Class::MOP::class_of($pkg);

    $class->$orig(
        @_,
        (map { $_->init_arg => $_->get_value($meta) }
             grep { $_->has_value($meta) }
                  map { $meta->meta->find_attribute_by_name($_) }
                      qw(has_nonmoose_constructor
                         has_nonmoose_destructor
                         constructor_name)),
    );
};

sub _determine_constructor_options {
    my $self = shift;
    my @options = @_;

    # if we're using just the metaclass trait, but not the constructor trait,
    # then suppress the warning about not inlining a constructor
    my $cc_meta = Class::MOP::class_of($self->constructor_class);
    return (@options, inline_constructor => 0)
        unless $cc_meta->can('does_role')
            && $cc_meta->does_role('MooseX::NonMoose::Meta::Role::Constructor');

    # do nothing if we explicitly ask for the constructor to not be inlined
    my %options = @options;
    return @options if !$options{inline_constructor};

    my $constructor_name = $self->constructor_name;

    my $local_constructor = $self->get_method($constructor_name);
    if (!defined($local_constructor)) {
        warn "Not inlining a constructor for " . $self->name . " since "
           . "its parent " . ($self->superclasses)[0] . " doesn't contain a "
           . "constructor named '$constructor_name'. "
           . "If you are certain you don't need to inline your"
           . " constructor, specify inline_constructor => 0 in your"
           . " call to " . $self->name . "->meta->make_immutable\n";
        return @options;
    }

    # do nothing if extends was called, but we then added a method modifier to
    # the constructor (this will warn, but that's okay)
    # XXX: this is a fairly big hack, but it should cover most of the cases
    # that actually show up in practice... it would be nice to do this properly
    # though
    return @options
        if $local_constructor->isa('Class::MOP::Method::Wrapped');

    # otherwise, explicitly ask for the constructor to be replaced (to suppress
    # the warning message), since this is the expected usage, and shouldn't
    # cause a warning
    return (replace_constructor => 1, @options);
}

sub _determine_destructor_options {
    my $self = shift;
    my @options = @_;

    return (@options, inline_destructor => 0);
}

around _immutable_options => sub {
    my $orig = shift;
    my $self = shift;

    my @options = $self->$orig(@_);

    # do nothing if extends was never called
    return @options if !$self->has_nonmoose_constructor
                    && !$self->has_nonmoose_destructor;

    @options = $self->_determine_constructor_options(@options);
    @options = $self->_determine_destructor_options(@options);

    return @options;
};

sub _check_superclass_constructor {
    my $self = shift;

    # if the current class defined a custom new method (since subs happen at
    # BEGIN time), don't try to override it
    return if $self->has_method($self->constructor_name);

    # we need to get the non-moose constructor from the superclass
    # of the class where this method actually exists, regardless of what class
    # we're calling it on
    my $super_new = $self->find_next_method_by_name($self->constructor_name);

    # if we're trying to extend a (non-immutable) moose class, just do nothing
    return if $super_new->package_name eq 'Moose::Object';

    if ($super_new->associated_metaclass->can('constructor_class')) {
        my $constructor_class_meta = Class::MOP::Class->initialize(
            $super_new->associated_metaclass->constructor_class
        );

        # if the constructor we're inheriting is already one of ours, there's
        # no reason to install a new one
        return if $constructor_class_meta->can('does_role')
               && $constructor_class_meta->does_role('MooseX::NonMoose::Meta::Role::Constructor');

        # if the constructor we're inheriting is an inlined version of the
        # default moose constructor, don't do anything either
        return if any { $_->isa($constructor_class_meta->name) }
                      $super_new->associated_metaclass->_inlined_methods;
    }

    $self->add_method($self->constructor_name => sub {
        my $class = shift;

        my $params = $class->BUILDARGS(@_);
        my @foreign_params = $class->can('FOREIGNBUILDARGS')
                           ? $class->FOREIGNBUILDARGS(@_)
                           : @_;
        my $instance = $super_new->execute($class, @foreign_params);
        if (!blessed($instance)) {
            confess "The constructor for "
                  . $super_new->associated_metaclass->name
                  . " did not return a blessed instance";
        }
        elsif (!$instance->isa($class)) {
            if (!$class->isa(blessed($instance))) {
                confess "The constructor for "
                      . $super_new->associated_metaclass->name
                      . " returned an object whose class is not a parent of "
                      . $class;
            }
            else {
                bless $instance, $class;
            }
        }
        return Class::MOP::Class->initialize($class)->new_object(
            __INSTANCE__ => $instance,
            %$params,
        );
    });
    $self->has_nonmoose_constructor(1);
}

sub _check_superclass_destructor {
    my $self = shift;

    # if the current class defined a custom DESTROY method (since subs happen
    # at BEGIN time), don't try to override it
    return if $self->has_method('DESTROY');

    # we need to get the non-moose destructor from the superclass
    # of the class where this method actually exists, regardless of what class
    # we're calling it on
    my $super_DESTROY = $self->find_next_method_by_name('DESTROY');

    # if we're trying to extend a (non-immutable) moose class, just do nothing
    return if $super_DESTROY->package_name eq 'Moose::Object';

    if ($super_DESTROY->associated_metaclass->can('destructor_class')
     && $super_DESTROY->associated_metaclass->destructor_class) {
        my $destructor_class_meta = Class::MOP::Class->initialize(
            $super_DESTROY->associated_metaclass->destructor_class
        );

        # if the destructor we're inheriting is an inlined version of the
        # default moose destructor, don't do anything
        return if any { $_->isa($destructor_class_meta->name) }
                      $super_DESTROY->associated_metaclass->_inlined_methods;
    }

    $self->add_method(DESTROY => sub {
        my $self = shift;

        local $?;

        Try::Tiny::try {
            $super_DESTROY->execute($self);
            $self->DEMOLISHALL(Devel::GlobalDestruction::in_global_destruction);
        }
        Try::Tiny::catch {
            # Without this, Perl will warn "\t(in cleanup)$@" because of some
            # bizarre fucked-up logic deep in the internals.
            no warnings 'misc';
            die $_;
        };

        return;
    });
    $self->has_nonmoose_destructor(1);
}

around superclasses => sub {
    my $orig = shift;
    my $self = shift;

    return $self->$orig unless @_;

    # XXX lots of duplication between here and MMC::superclasses
    my ($constructor_name, $constructor_class);
    for my $super (@{ Data::OptList::mkopt(\@_) }) {
        my ($name, $opts) = @{ $super };

        my $cur_constructor_name = delete $opts->{'-constructor_name'};

        if (defined($constructor_name) && defined($cur_constructor_name)) {
            $self->throw_error(
                "You have already specified "
              . "${constructor_class}::${constructor_name} as the parent "
              . "constructor; ${name}::${cur_constructor_name} cannot also be "
              . "the constructor"
            );
        }

        Class::MOP::load_class($name, $opts);

        if (defined($cur_constructor_name)) {
            my $meta = Class::MOP::class_of($name);
            $self->throw_error(
                "You specified '$cur_constructor_name' as the constructor for "
              . "$name, but $name has no method by that name"
            ) unless $meta
                  ? $meta->find_method_by_name($cur_constructor_name)
                  : $name->can($cur_constructor_name);
        }

        if (!defined($constructor_name)) {
            $constructor_name  = $cur_constructor_name;
            $constructor_class = $name;
        }

        delete $opts->{'-constructor_name'};
    }

    $self->constructor_name(
        defined($constructor_name) ? $constructor_name : 'new'
    );

    my @superclasses = @_;
    push @superclasses, 'Moose::Object'
        unless grep { !ref($_) && $_->isa('Moose::Object') } @superclasses;

    my @ret = $self->$orig(@superclasses);

    $self->_check_superclass_constructor;
    $self->_check_superclass_destructor;

    return @ret;
};

sub _generate_fallback_constructor {
    my $self = shift;
    my ($class_var) = @_;

    my $new = $self->constructor_name;
    my $super_new_class = $self->_find_next_nonmoose_constructor_package;
    my $arglist = $self->find_method_by_name('FOREIGNBUILDARGS')
                ? "${class_var}->FOREIGNBUILDARGS(\@_)"
                : '@_';
    my $instance = "${class_var}->${super_new_class}::$new($arglist)";
    # XXX: the "my $__DUMMY = " part is because "return do" triggers a weird
    # bug in pre-5.12 perls (it ends up returning undef)
    return '(my $__DUMMY = do { '
             . 'if (ref($_[0]) eq \'HASH\') { '
                 . '$_[0]->{__INSTANCE__} = ' . $instance . ' '
                     . 'unless exists $_[0]->{__INSTANCE__}; '
             . '} '
             . 'else { '
                 . 'unshift @_, __INSTANCE__ => ' . $instance . '; '
             . '} '
             . $class_var . '->Moose::Object::new(@_); '
         . '})';
}

sub _inline_generate_instance {
    my $self = shift;
    my ($var, $class_var) = @_;

    my $new = $self->constructor_name;
    my $super_new_class = $self->_find_next_nonmoose_constructor_package;
    my $arglist = $self->find_method_by_name('FOREIGNBUILDARGS')
                ? "${class_var}->FOREIGNBUILDARGS(\@_)"
                : '@_';
    my $instance = "${class_var}->${super_new_class}::$new($arglist)";
    return (
        'my ' . $var . ' = ' . $instance . ';',
        'if (!Scalar::Util::blessed(' . $var . ')) {',
            $self->_inline_throw_error(
                '"The constructor for ' . $super_new_class . ' did not return a blessed instance"',
            ) . ';',
        '}',
        'elsif (!' . $var . '->isa(' . $class_var . ')) {',
            'if (!' . $class_var . '->isa(Scalar::Util::blessed(' . $var . '))) {',
                $self->_inline_throw_error(
                    '"The constructor for ' . $super_new_class . ' returned an object whose class is not a parent of ' . $class_var . '"',
                ) . ';',
            '}',
            'else {',
                $self->_inline_rebless_instance($var, $class_var) . ';',
            '}',
        '}',
    );
}

sub _find_next_nonmoose_constructor_package {
    my $self = shift;
    my $new = $self->constructor_name;
    for my $method (map { $_->{code} } $self->find_all_methods_by_name($new)) {
        next if $method->associated_metaclass->meta->can('does_role')
             && $method->associated_metaclass->meta->does_role('MooseX::NonMoose::Meta::Role::Class');
        return $method->package_name;
    }
    # this should never happen (it should find Moose::Object at least)
    $self->throw_error("Couldn't find a non-Moose constructor for " . $self->name);
}

no Moose::Role;

1;

__END__
=pod

=head1 NAME

MooseX::NonMoose::Meta::Role::Class - metaclass trait for L<MooseX::NonMoose>

=head1 VERSION

version 0.22

=head1 SYNOPSIS

  package Foo;
  use Moose -traits => 'MooseX::NonMoose::Meta::Role::Class';

  # or

  package My::Moose;
  use Moose ();
  use Moose::Exporter;

  Moose::Exporter->setup_import_methods;
  sub init_meta {
      shift;
      my %options = @_;
      Moose->init_meta(%options);
      Moose::Util::MetaRole::apply_metaclass_roles(
          for_class       => $options{for_class},
          metaclass_roles => ['MooseX::NonMoose::Meta::Role::Class'],
      );
      return Class::MOP::class_of($options{for_class});
  }

=head1 DESCRIPTION

This trait implements everything involved with extending non-Moose classes,
other than doing the actual inlining at C<make_immutable> time. See
L<MooseX::NonMoose> for more details.

=head1 AUTHOR

Jesse Luehrs <doy at tozt dot net>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by Jesse Luehrs.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

