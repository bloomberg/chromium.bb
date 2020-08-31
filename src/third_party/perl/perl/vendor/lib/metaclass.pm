package metaclass;
our $VERSION = '2.2011';

use strict;
use warnings;

use Module::Runtime 'use_package_optimistically', 'use_module';
use Class::MOP;

sub import {
    my ( $class, @args ) = @_;

    unshift @args, "metaclass" if @args % 2 == 1;
    my %options = @args;

    my $meta_name = exists $options{meta_name} ? $options{meta_name} : 'meta';
    my $metaclass = delete $options{metaclass};

    unless ( defined $metaclass ) {
        $metaclass = "Class::MOP::Class";
    } else {
        use_package_optimistically($metaclass);
    }

    ($metaclass->isa('Class::MOP::Class'))
        || die use_module('Moose::Exception::MetaclassMustBeDerivedFromClassMOPClass')->new( class_name => $metaclass );

    # make sure the custom metaclasses get loaded
    foreach my $key (grep { /_(?:meta)?class$/ } keys %options) {
        unless ( ref( my $class = $options{$key} ) ) {
            use_package_optimistically($class)
        }
    }

    my $package = caller();

    # create a meta object so we can install &meta
    my $meta = $metaclass->initialize($package => %options);
    $meta->_add_meta_method($meta_name)
        if defined $meta_name;
}

1;

# ABSTRACT: a pragma for installing and using Class::MOP metaclasses

__END__

=pod

=encoding UTF-8

=head1 NAME

metaclass - a pragma for installing and using Class::MOP metaclasses

=head1 VERSION

version 2.2011

=head1 SYNOPSIS

  package MyClass;

  # use Class::MOP::Class
  use metaclass;

  # ... or use a custom metaclass
  use metaclass 'MyMetaClass';

  # ... or use a custom metaclass
  # and custom attribute and method
  # metaclasses
  use metaclass 'MyMetaClass' => (
      'attribute_metaclass' => 'MyAttributeMetaClass',
      'method_metaclass'    => 'MyMethodMetaClass',
  );

  # ... or just specify custom attribute
  # and method classes, and Class::MOP::Class
  # is the assumed metaclass
  use metaclass (
      'attribute_metaclass' => 'MyAttributeMetaClass',
      'method_metaclass'    => 'MyMethodMetaClass',
  );

  # if we'd rather not install a 'meta' method, we can do this
  use metaclass meta_name => undef;
  # or if we'd like it to have a different name,
  use metaclass meta_name => 'my_meta';

=head1 DESCRIPTION

This is a pragma to make it easier to use a specific metaclass
and a set of custom attribute and method metaclasses. It also
installs a C<meta> method to your class as well, unless C<undef>
is passed to the C<meta_name> option.

Note that if you are using Moose, you most likely do B<not> want
to be using this - look into L<Moose::Util::MetaRole> instead.

=head1 AUTHORS

=over 4

=item *

Stevan Little <stevan.little@iinteractive.com>

=item *

Dave Rolsky <autarch@urth.org>

=item *

Jesse Luehrs <doy@tozt.net>

=item *

Shawn M Moore <code@sartak.org>

=item *

יובל קוג'מן (Yuval Kogman) <nothingmuch@woobling.org>

=item *

Karen Etheridge <ether@cpan.org>

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Hans Dieter Pearcey <hdp@weftsoar.net>

=item *

Chris Prather <chris@prather.org>

=item *

Matt S Trout <mst@shadowcat.co.uk>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2006 by Infinity Interactive, Inc.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
