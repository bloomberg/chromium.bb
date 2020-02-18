package Throwable;
# ABSTRACT: a role for classes that can be thrown
$Throwable::VERSION = '0.200013';
use Moo::Role;
use Sub::Quote ();
use Scalar::Util ();
use Carp ();

#pod =head1 SYNOPSIS
#pod
#pod   package Redirect;
#pod   # NOTE: Moo can also be used here instead of Moose
#pod   use Moose;
#pod   with 'Throwable';
#pod
#pod   has url => (is => 'ro');
#pod
#pod ...then later...
#pod
#pod   Redirect->throw({ url => $url });
#pod
#pod =head1 DESCRIPTION
#pod
#pod Throwable is a role for classes that are meant to be thrown as exceptions to
#pod standard program flow.  It is very simple and does only two things: saves any
#pod previous value for C<$@> and calls C<die $self>.
#pod
#pod Throwable is implemented with L<Moo>, so you can stick to Moo or use L<Moose>,
#pod as you prefer.
#pod
#pod =attr previous_exception
#pod
#pod This attribute is created automatically, and stores the value of C<$@> when the
#pod Throwable object is created.  This is done on a I<best effort basis>.  C<$@> is
#pod subject to lots of spooky action-at-a-distance.  For now, there are clearly
#pod ways that the previous exception could be lost.
#pod
#pod =cut

our %_HORRIBLE_HACK;

has 'previous_exception' => (
  is       => 'ro',
  default  => Sub::Quote::quote_sub(q<
    if (defined $Throwable::_HORRIBLE_HACK{ERROR}) {
      $Throwable::_HORRIBLE_HACK{ERROR}
    } elsif (defined $@ and (ref $@ or length $@)) {
      $@;
    } else {
      undef;
    }
  >),
);

#pod =method throw
#pod
#pod   Something::Throwable->throw({ attr => $value });
#pod
#pod This method will call new, passing all arguments along to new, and will then
#pod use the created object as the only argument to C<die>.
#pod
#pod If called on an object that does Throwable, the object will be rethrown.
#pod
#pod =cut

sub throw {
  my ($inv) = shift;

  if (Scalar::Util::blessed($inv)) {
    Carp::confess "throw called on Throwable object with arguments" if @_;
    die $inv;
  }

  local $_HORRIBLE_HACK{ERROR} = $@;
  my $throwable = $inv->new(@_);
  die $throwable;
}

no Moo::Role;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Throwable - a role for classes that can be thrown

=head1 VERSION

version 0.200013

=head1 SYNOPSIS

  package Redirect;
  # NOTE: Moo can also be used here instead of Moose
  use Moose;
  with 'Throwable';

  has url => (is => 'ro');

...then later...

  Redirect->throw({ url => $url });

=head1 DESCRIPTION

Throwable is a role for classes that are meant to be thrown as exceptions to
standard program flow.  It is very simple and does only two things: saves any
previous value for C<$@> and calls C<die $self>.

Throwable is implemented with L<Moo>, so you can stick to Moo or use L<Moose>,
as you prefer.

=head1 ATTRIBUTES

=head2 previous_exception

This attribute is created automatically, and stores the value of C<$@> when the
Throwable object is created.  This is done on a I<best effort basis>.  C<$@> is
subject to lots of spooky action-at-a-distance.  For now, there are clearly
ways that the previous exception could be lost.

=head1 METHODS

=head2 throw

  Something::Throwable->throw({ attr => $value });

This method will call new, passing all arguments along to new, and will then
use the created object as the only argument to C<die>.

If called on an object that does Throwable, the object will be rethrown.

=head1 AUTHORS

=over 4

=item *

Ricardo SIGNES <rjbs@cpan.org>

=item *

Florian Ragwitz <rafl@debian.org>

=back

=head1 CONTRIBUTORS

=for stopwords Arthur Axel 'fREW' Schmidt Brian Manning Dagfinn Ilmari Mannsåker Dave Rolsky David E. Wheeler Graham Knop Jeffrey Ryan Thalhammer Justin Hunter Matt S Trout Olaf Alders Toby Inkster

=over 4

=item *

Arthur Axel 'fREW' Schmidt <frioux@gmail.com>

=item *

Brian Manning <brian@xaoc.org>

=item *

Brian Manning <xaoc@cpan.org>

=item *

Dagfinn Ilmari Mannsåker <ilmari@ilmari.org>

=item *

Dave Rolsky <autarch@urth.org>

=item *

David E. Wheeler <david@justatheory.com>

=item *

Graham Knop <haarg@haarg.org>

=item *

Jeffrey Ryan Thalhammer <jeff@imaginative-software.com>

=item *

Justin Hunter <justin.d.hunter@gmail.com>

=item *

Matt S Trout <mst@shadowcat.co.uk>

=item *

Olaf Alders <olaf@wundersolutions.com>

=item *

Toby Inkster <mail@tobyinkster.co.uk>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2015 by Ricardo SIGNES.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
