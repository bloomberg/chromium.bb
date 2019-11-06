package Test::Object;

=pod

=head1 NAME

Test::Object - Thoroughly testing objects via registered handlers

=head1 SYNOPSIS

  ###################################################################
  # In your test module, register test handlers again class names   #
  ###################################################################
  
  package My::ModuleTester;
  
  use Test::More;
  use Test::Object;
  
  # Foo::Bar is a subclass of Foo
  Test::Object->register(
  	class => 'Foo',
  	tests => 5,
  	code  => \&foo_ok,
  	);
  Test::Object->register(
  	class => 'Foo::Bar',
  	# No fixed number of tests
  	code  => \&foobar_ok,
  	);
  
  sub foo_ok {
  	my $object = shift;
  	ok( $object->foo, '->foo returns true' );
  }
  
  sub foobar_ok {
  	my $object = shift;
  	is( $object->foo, 'bar', '->foo returns "bar"' );
  }
  
  1;
  
  
  
  ###################################################################
  # In test script, test object against all registered classes      #
  ###################################################################
  
  #!/usr/bin/perl -w
  
  use Test::More 'no_plan';
  use Test::Object;
  use My::ModuleTester;
  
  my $object = Foo::Bar->new;
  isa_ok( $object, 'Foo::Bar' );
  object_ok( $object );

=head1 DESCRIPTION

In situations where you have deep trees of classes, there is a common
situation in which you test a module 4 or 5 subclasses down, which should
follow the correct behaviour of not just the subclass, but of all the
parent classes.

This should be done to ensure that the implementation of a subclass has
not somehow "broken" the object's behaviour in a more general sense.

C<Test::Object> is a testing package designed to allow you to easily test
what you believe is a valid object against the expected behaviour of B<all>
of the classes in its inheritance tree in one single call.

To do this, you "register" tests (in the form of CODE or function
references) with C<Test::Object>, with each test associated with a
particular class.

When you call C<object_ok> in your test script, C<Test::Object> will check
the object against all registered tests. For each class that your object
responds to C<$object-E<gt>isa($class)> for, the appropriate testing
function will be called.

Doing it this way allows adapter objects and other things that respond
to C<isa> differently that the default to still be tested against the
classes that it is advertising itself as correctly.

This also means that more than one test might be "counted" for each call
to C<object_ok>. You should account for this correctly in your expected
test count.

=cut

use 5.005;
use strict;
use Carp               ();
use Exporter           ();
use Test::More         ();
use Scalar::Util       ();
use Test::Object::Test ();

use vars qw{$VERSION @ISA @EXPORT};
BEGIN {
	$VERSION = '0.07';
	@ISA     = 'Exporter';
	@EXPORT  = 'object_ok';
}





#####################################################################
# Registration and Planning

my @TESTS = ();

sub register {
	my $class = shift;
	push @TESTS, Test::Object::Test->new( @_ );
}





#####################################################################
# Testing Functions

sub object_ok {
	my $object = Scalar::Util::blessed($_[0]) ? shift
		: Carp::croak("Did not provide an object to object_ok");

	# Iterate over the tests and run any we ->isa
	foreach my $test ( @TESTS ) {
		$test->run( $object ) if $object->isa( $test->class );
	}

	1;
}

1;

=pod

=head1 SUPPORT

Bugs should be submitted via the CPAN bug tracker, located at

L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=Test-Object>

For other issues, contact the author.

=head1 AUTHOR

Adam Kennedy E<lt>cpan@ali.asE<gt>

=head1 SEE ALSO

L<http://ali.as/>, L<Test::More>, L<Test::Builder::Tester>, L<Test::Class>

=head1 COPYRIGHT

Copyright 2005, 2006 Adam Kennedy. All rights reserved.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
