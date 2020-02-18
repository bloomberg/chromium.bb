package PPI::Exception;

=head1 NAME

PPI::Exception - The PPI exception base class

=head1 SYNOPSIS

  use PPI::Exception;
  
  my $e = PPI::Exception->new( 'something happened' );
  $e->throw;

  PPI::Exception->new( message => 'something happened' )->throw;
  PPI::Exception->throw( message => 'something happened' );

=head1 DESCRIPTION

All exceptions thrown from within PPI will be instances or derivations
of this class.

=cut

use strict;
use Params::Util qw{_INSTANCE};

our $VERSION = '1.269'; # VERSION


=head1 METHODS

=head2 new $message | message => $message, ...

Constructs and returns a new C<PPI::Exception> object.

A message for the exception can be passed, either as a string
or as C<< message => $message >>. The message is available via the
C<message> method.

=cut

sub new {
	my $class = shift;
	return bless { @_ }, $class if @_ > 1;
	return bless { message => $_[0] }, $class if @_;
	return bless { message => 'Unknown Exception' }, $class;
}


=head2 throw

If called on a C<PPI::Exception> object, throws the object.
If called on the class name, uses the arguments to construct a
C<PPI::Exception> and then throw it.

Each time the object is thrown, information from the Perl <caller(0)>
call is saved and made available via the C<callers> method.

This method never returns.

=cut

sub throw {
	my $it = shift;
	if ( _INSTANCE($it, 'PPI::Exception') ) {
		if ( $it->{callers} ) {
			push @{ $it->{callers} }, [ caller(0) ];
		} else {
			$it->{callers} ||= [];
		}
	} else {
		my $message = $_[0] || 'Unknown Exception';
		$it = $it->new(
			message => $message,
			callers => [
				[ caller(0) ],
			],
		);
	}
	die $it;
}


=head2 message

Returns the exception message passed to the object's constructor,
or a default message.

=cut

sub message {
	$_[0]->{message};
}


=head2 callers

Returns a listref, each element of which is a listref of C<caller(0)>
information.  The returned listref can be empty.

=cut

sub callers {
	@{ $_[0]->{callers} || [] };
}


1;
