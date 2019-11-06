use strict;
use warnings;
package Email::MIME::Header;
# ABSTRACT: the header of a MIME message
$Email::MIME::Header::VERSION = '1.946';
use parent 'Email::Simple::Header';

use Carp ();
use Email::MIME::Encode;
use Encode 1.9801;
use Module::Runtime ();

our @CARP_NOT;

our %header_to_class_map;

{
  my @address_list_headers = qw(from sender reply-to to cc bcc);
  push @address_list_headers, map { "resent-$_" } @address_list_headers;
  push @address_list_headers, map { "downgraded-$_" } @address_list_headers; # RFC 5504
  push @address_list_headers, qw(original-from disposition-notification-to); # RFC 5703 and RFC 3798
  $header_to_class_map{$_} = 'Email::MIME::Header::AddressList' foreach @address_list_headers;
}

#pod =head1 DESCRIPTION
#pod
#pod This object behaves like a standard Email::Simple header, with the following
#pod changes:
#pod
#pod =for :list
#pod * the C<header> method automatically decodes encoded headers if possible
#pod * the C<header_as_obj> method returns an object representation of the header value
#pod * the C<header_raw> method returns the raw header; (read only for now)
#pod * stringification uses C<header_raw> rather than C<header>
#pod
#pod Note that C<header_set> does not do encoding for you, and expects an
#pod encoded header.  Thus, C<header_set> round-trips with C<header_raw>,
#pod not C<header>!  Be sure to properly encode your headers with
#pod C<Encode::encode('MIME-Header', $value)> before passing them to
#pod C<header_set>.  And be sure to use minimal version 2.83 of Encode
#pod module due to L<bugs in MIME-Header|Encode::MIME::Header/BUGS>.
#pod
#pod Alternately, if you have Unicode (character) strings to set in headers, use the
#pod C<header_str_set> method.
#pod
#pod =cut

sub header_str {
  my $self  = shift;
  my $wanta = wantarray;

  return unless defined $wanta; # ??

  my @header = $wanta ? $self->header_raw(@_)
                      : scalar $self->header_raw(@_);

  foreach my $header (@header) {
    next unless defined $header;
    next unless $header =~ /=\?/;

    _maybe_decode($_[0], \$header);
  }
  return $wanta ? @header : $header[0];
}

sub header {
  my $self = shift;
  return $self->header_str(@_);
}

sub header_str_set {
  my ($self, $name, @vals) = @_;

  my @values = map {
    Email::MIME::Encode::maybe_mime_encode_header($name, $_, 'UTF-8')
  } @vals;

  $self->header_set($name => @values);
}

sub header_str_pairs {
  my ($self) = @_;

  my @pairs = $self->header_pairs;
  for (grep { $_ % 2 } (1 .. $#pairs)) {
    _maybe_decode($pairs[$_-1], \$pairs[$_]);
  }

  return @pairs;
}

sub header_as_obj {
  my ($self, $name, $index, $class) = @_;

  $class = $self->get_class_for_header($name) unless defined $class;

  {
    local @CARP_NOT = qw(Email::MIME);
    local $@;
    Carp::croak("No class for header '$name' was specified") unless defined $class;
    Carp::croak("Cannot load package '$class' for header '$name': $@") unless eval { Module::Runtime::require_module($class) };
    Carp::croak("Class '$class' does not have method 'from_mime_string'") unless $class->can('from_mime_string');
  }

  my @values = $self->header_raw($name, $index);
  if (wantarray) {
    return map { $class->from_mime_string($_) } @values;
  } else {
    return $class->from_mime_string(@values);
  }
}

sub _maybe_decode {
  my ($name, $str_ref) = @_;
  $$str_ref = Email::MIME::Encode::maybe_mime_decode_header($name, $$str_ref);
  return;
}

sub get_class_for_header {
  my ($self, $name) = @_;
  return $header_to_class_map{lc $name};
}

sub set_class_for_header {
  my ($self, $class, $header) = @_;
  $header = lc $header;
  Carp::croak("Class for header '$header' is already set") if defined $header_to_class_map{$header};
  $header_to_class_map{$header} = $class;
  return;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::MIME::Header - the header of a MIME message

=head1 VERSION

version 1.946

=head1 DESCRIPTION

This object behaves like a standard Email::Simple header, with the following
changes:

=over 4

=item *

the C<header> method automatically decodes encoded headers if possible

=item *

the C<header_as_obj> method returns an object representation of the header value

=item *

the C<header_raw> method returns the raw header; (read only for now)

=item *

stringification uses C<header_raw> rather than C<header>

=back

Note that C<header_set> does not do encoding for you, and expects an
encoded header.  Thus, C<header_set> round-trips with C<header_raw>,
not C<header>!  Be sure to properly encode your headers with
C<Encode::encode('MIME-Header', $value)> before passing them to
C<header_set>.  And be sure to use minimal version 2.83 of Encode
module due to L<bugs in MIME-Header|Encode::MIME::Header/BUGS>.

Alternately, if you have Unicode (character) strings to set in headers, use the
C<header_str_set> method.

=head1 AUTHORS

=over 4

=item *

Ricardo SIGNES <rjbs@cpan.org>

=item *

Casey West <casey@geeknest.com>

=item *

Simon Cozens <simon@cpan.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2004 by Simon Cozens and Casey West.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
