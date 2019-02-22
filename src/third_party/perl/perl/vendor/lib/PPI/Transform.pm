package PPI::Transform;

=pod

=head1 NAME

PPI::Transform - Abstract base class for document transformation classes

=head1 DESCRIPTION

C<PPI::Transform> provides an API for the creation of classes and objects
that modify or transform PPI documents.

=head1 METHODS

=cut

use strict;
use Carp          ();
use List::Util    ();
use PPI::Document ();
use Params::Util  qw{_INSTANCE _CLASS _CODE _SCALAR0};

use vars qw{$VERSION};
BEGIN {
	$VERSION = '1.215';
}





#####################################################################
# Apply Handler Registration

my %HANDLER = ();
my @ORDER   = ();

# Yes, you can use this yourself.
# I'm just leaving it undocumented for now.
sub register_apply_handler {
	my $class   = shift;
	my $handler = _CLASS(shift) or Carp::croak("Invalid PPI::Transform->register_apply_handler param");
	my $get     = _CODE(shift)  or Carp::croak("Invalid PPI::Transform->register_apply_handler param");
	my $set     = _CODE(shift)  or Carp::croak("Invalid PPI::Transform->register_apply_handler param");
	if ( $HANDLER{$handler} ) {
		Carp::croak("PPI::Transform->apply handler '$handler' already exists");
	}

	# Register the handler
	$HANDLER{$handler} = [ $get, $set ];
	unshift @ORDER, $handler;
}

# Register the default handlers
__PACKAGE__->register_apply_handler( 'SCALAR', \&_SCALAR_get, \&_SCALAR_set );
__PACKAGE__->register_apply_handler( 'PPI::Document', sub { $_[0] }, sub { 1 } );





#####################################################################
# Constructor

=pod

=head2 new

  my $transform = PPI::Transform->new(
      param1 => 'value1',
      param2 => 'value2',
  );

The C<new> constructor creates a new object for your C<PPI::Transform>
subclass. A default constructor is provided for you which takes no params
and creates a basic, empty, object.

If you wish to have your transform constructor take params, these B<must>
be in the form of a list of key/value pairs.

Returns a new C<PPI::Transform>-compatible object, or returns
C<undef> on error.

=cut

sub new {
	my $class = shift;
	bless { @_ }, $class;
}

=pod

=head2 document

The C<document> method should be implemented by each subclass, and
takes a single argument of a L<PPI::Document> object, modifying it
B<in place> as appropriate for the particular transform class.

That's right, this method B<will not clone> and B<should not clone>
the document object. If you do not want the original to be modified,
you need to clone it yourself before passing it in.

Returns the numbers of changes made to the document. If the transform
is unable to track the quantity (including the situation where it cannot
tell B<IF> it made a change) it should return 1. Returns zero if no
changes were made to the document, or C<undef> if an error occurs.

By default this error is likely to only mean that you passed in something
that wasn't a L<PPI::Document>, but may include additional errors
depending on the subclass.

=cut

sub document {
	my $class = shift;
	die "$class does not implement the required ->document method";
}

=pod

=head2 apply

The C<apply> method is used to apply the transform to something. The
argument must be a L<PPI::Document>, or something which can be turned
into a one and then be written back to again.

Currently, this list is limited to a C<SCALAR> reference, although a
handler registration process is available for you to add support for
additional types of object should you wish (see the source for this module).

Returns true if the transform was applied, false if there is an error in the
transform process, or may die if there is a critical error in the apply
handler.

=cut

sub apply {
	my $self = _SELF(shift);
	my $it   = defined $_[0] ? shift : return undef;

	# Try to find an apply handler
	my $class = _SCALAR0($it) ? 'SCALAR'
		: List::Util::first { _INSTANCE($it, $_) } @ORDER
		or return undef;
	my $handler = $HANDLER{$class}
		or die("->apply handler for $class missing! Panic");

	# Get, change, set
	my $Document = _INSTANCE($handler->[0]->($it), 'PPI::Document')
		or Carp::croak("->apply handler for $class failed to get a PPI::Document");
	$self->document( $Document ) or return undef;
	$handler->[1]->($it, $Document)
		or Carp::croak("->apply handler for $class failed to save the changed document");
	1;		
}

=pod

=head2 file

  # Read from one file and write to another
  $transform->file( 'Input.pm' => 'Output.pm' );
  
  # Change a file in place
  $transform->file( 'Change.pm' );

The C<file> method modifies a Perl document by filename. If passed a single
parameter, it modifies the file in-place. If provided a second parameter,
it will attempt to save the modified file to the alternative filename.

Returns true on success, or C<undef> on error.

=cut

sub file {
	my $self = _SELF(shift);

	# Where do we read from and write to
	my $input  = defined $_[0] ? shift : return undef;
	my $output = @_ ? defined $_[0] ? "$_[0]" : undef : $input or return undef;

	# Process the file
	my $Document = PPI::Document->new( "$input" ) or return undef;
	$self->document( $Document )                  or return undef;
	$Document->save( $output );
}





#####################################################################
# Apply Hander Methods

sub _SCALAR_get {
	PPI::Document->new( $_[0] );
}

sub _SCALAR_set {
	my $it = shift;
	$$it = $_[0]->serialize;
	1;
}





#####################################################################
# Support Functions

sub _SELF {
	return shift if ref $_[0];
	my $self = $_[0]->new or Carp::croak(
		"Failed to auto-instantiate new $_[0] object"
	);
	$self;
}

1;

=pod

=head1 SUPPORT

See the L<support section|PPI/SUPPORT> in the main module.

=head1 AUTHOR

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

=head1 COPYRIGHT

Copyright 2001 - 2011 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
