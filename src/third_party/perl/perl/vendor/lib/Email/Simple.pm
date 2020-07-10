use 5.008;
use strict;
use warnings;
package Email::Simple;
# ABSTRACT: simple parsing of RFC2822 message format and headers
$Email::Simple::VERSION = '2.216';
use Carp ();

use Email::Simple::Creator;
use Email::Simple::Header;

our $GROUCHY = 0;

# We are liberal in what we accept.
sub __crlf_re { qr/\x0a\x0d|\x0d\x0a|\x0a|\x0d/; }

#pod =head1 SYNOPSIS
#pod
#pod   use Email::Simple;
#pod   my $email = Email::Simple->new($text);
#pod
#pod   my $from_header = $email->header("From");
#pod   my @received = $email->header("Received");
#pod
#pod   $email->header_set("From", 'Simon Cozens <simon@cpan.org>');
#pod
#pod   my $old_body = $email->body;
#pod   $email->body_set("Hello world\nSimon");
#pod
#pod   print $email->as_string;
#pod
#pod ...or, to create a message from scratch...
#pod
#pod   my $email = Email::Simple->create(
#pod       header => [
#pod         From    => 'casey@geeknest.com',
#pod         To      => 'drain@example.com',
#pod         Subject => 'Message in a bottle',
#pod       ],
#pod       body => '...',
#pod   );
#pod
#pod   $email->header_set( 'X-Content-Container' => 'bottle/glass' );
#pod
#pod   print $email->as_string;
#pod
#pod =head1 DESCRIPTION
#pod
#pod The Email:: namespace was begun as a reaction against the increasing complexity
#pod and bugginess of Perl's existing email modules.  C<Email::*> modules are meant
#pod to be simple to use and to maintain, pared to the bone, fast, minimal in their
#pod external dependencies, and correct.
#pod
#pod =method new
#pod
#pod   my $email = Email::Simple->new($message, \%arg);
#pod
#pod This method parses an email from a scalar containing an RFC2822 formatted
#pod message and returns an object.  C<$message> may be a reference to a message
#pod string, in which case the string will be altered in place.  This can result in
#pod significant memory savings.
#pod
#pod If you want to create a message from scratch, you should use the C<L</create>>
#pod method.
#pod
#pod Valid arguments are:
#pod
#pod   header_class - the class used to create new header objects
#pod                  The named module is not 'require'-ed by Email::Simple!
#pod
#pod =cut

sub new {
  my ($class, $text, $arg) = @_;
  $arg ||= {};

  Carp::croak 'Unable to parse undefined message' if ! defined $text;

  my $text_ref = (ref $text || '' eq 'SCALAR') ? $text : \$text;

  my ($pos, $mycrlf) = $class->_split_head_from_body($text_ref);

  my $self = bless { mycrlf => $mycrlf } => $class;

  my $head;
  if (defined $pos) {
    $head = substr $$text_ref, 0, $pos, '';
    substr($head, -(length $mycrlf)) = '';
  } else {
    $head     = $$text_ref;
    $text_ref = \'';
  }

  my $header_class = $arg->{header_class} || $self->default_header_class;

  $self->header_obj_set(
    $header_class->new(\$head, { crlf => $self->crlf })
  );

  $self->body_set($text_ref);

  return $self;
}

# Given the text of an email, return ($pos, $crlf) where $pos is the position
# at which the body text begins and $crlf is the type of newline used in the
# message.
sub _split_head_from_body {
  my ($self, $text_ref) = @_;

  # For body/header division, see RFC 2822, section 2.1
  #
  # Honestly, are we *ever* going to have LFCR messages?? -- rjbs, 2015-10-11
  my $re = qr{\x0a\x0d\x0a\x0d|\x0d\x0a\x0d\x0a|\x0d\x0d|\x0a\x0a};

  if ($$text_ref =~ /($re)/gsm) {
    my $crlf = substr $1, 0, length($1)/2;
    return (pos($$text_ref), $crlf);
  } else {

    # The body is, of course, optional.
    my $re = $self->__crlf_re;
    $$text_ref =~ /($re)/gsm;
    return (undef, ($1 || "\n"));
  }
}

#pod =method create
#pod
#pod   my $email = Email::Simple->create(header => [ @headers ], body => '...');
#pod
#pod This method is a constructor that creates an Email::Simple object
#pod from a set of named parameters. The C<header> parameter's value is a
#pod list reference containing a set of headers to be created. The C<body>
#pod parameter's value is a scalar value holding the contents of the message
#pod body.  Line endings in the body will normalized to CRLF.
#pod
#pod If no C<Date> header is specified, one will be provided for you based on the
#pod C<gmtime> of the local machine. This is because the C<Date> field is a required
#pod header and is a pain in the neck to create manually for every message. The
#pod C<From> field is also a required header, but it is I<not> provided for you.
#pod
#pod =cut

our $CREATOR = 'Email::Simple::Creator';

sub create {
  my ($class, %args) = @_;

  # We default it in here as well as below because by having it here, then we
  # know that if there are no other headers, we'll get the proper CRLF.
  # Otherwise, we get a message with incorrect CRLF. -- rjbs, 2007-07-13
  my $headers = $args{header} || [ Date => $CREATOR->_date_header ];
  my $body    = $args{body} || '';

  my $empty   = q{};
  my $header  = \$empty;

  for my $idx (map { $_ * 2 } 0 .. @$headers / 2 - 1) {
    my ($key, $value) = @$headers[ $idx, $idx + 1 ];
    $CREATOR->_add_to_header($header, $key, $value);
  }

  $CREATOR->_finalize_header($header);

  my $email = $class->new($header);

  $email->header_raw_set(Date => $CREATOR->_date_header)
    unless defined $email->header_raw('Date');

  $body = (join $CREATOR->_crlf, split /\x0d\x0a|\x0a\x0d|\x0a|\x0d/, $body)
        . $CREATOR->_crlf;

  $email->body_set($body);

  return $email;
}


#pod =method header_obj
#pod
#pod   my $header = $email->header_obj;
#pod
#pod This method returns the object representing the email's header.  For the
#pod interface for this object, see L<Email::Simple::Header>.
#pod
#pod =cut

sub header_obj {
  my ($self) = @_;
  return $self->{header};
}

# Probably needs to exist in perpetuity for modules released during the "__head
# is tentative" phase, until we have a way to force modules below us on the
# dependency tree to upgrade.  i.e., never and/or in Perl 6 -- rjbs, 2006-11-28
BEGIN { *__head = \&header_obj }

#pod =method header_obj_set
#pod
#pod   $email->header_obj_set($new_header_obj);
#pod
#pod This method substitutes the given new header object for the email's existing
#pod header object.
#pod
#pod =cut

sub header_obj_set {
  my ($self, $obj) = @_;
  $self->{header} = $obj;
}

#pod =method header
#pod
#pod   my @values = $email->header($header_name);
#pod   my $first  = $email->header($header_name);
#pod   my $value  = $email->header($header_name, $index);
#pod
#pod In list context, this returns every value for the named header.  In scalar
#pod context, it returns the I<first> value for the named header.  If second
#pod parameter is specified then instead I<first> value it returns value at
#pod position C<$index> (negative C<$index> is from the end).
#pod
#pod =method header_set
#pod
#pod     $email->header_set($field, $line1, $line2, ...);
#pod
#pod Sets the header to contain the given data. If you pass multiple lines
#pod in, you get multiple headers, and order is retained.  If no values are given to
#pod set, the header will be removed from to the message entirely.
#pod
#pod =method header_raw
#pod
#pod This is another name (and the preferred one) for C<header>.
#pod
#pod =method header_raw_set
#pod
#pod This is another name (and the preferred one) for C<header_set>.
#pod
#pod =method header_raw_prepend
#pod
#pod   $email->header_raw_prepend($field => $value);
#pod
#pod This method adds a new instance of the name field as the first field in the
#pod header.
#pod
#pod =method header_names
#pod
#pod     my @header_names = $email->header_names;
#pod
#pod This method returns the list of header names currently in the email object.
#pod These names can be passed to the C<header> method one-at-a-time to get header
#pod values. You are guaranteed to get a set of headers that are unique. You are not
#pod guaranteed to get the headers in any order at all.
#pod
#pod For backwards compatibility, this method can also be called as B<headers>.
#pod
#pod =method header_pairs
#pod
#pod   my @headers = $email->header_pairs;
#pod
#pod This method returns a list of pairs describing the contents of the header.
#pod Every other value, starting with and including zeroth, is a header name and the
#pod value following it is the header value.
#pod
#pod =method header_raw_pairs
#pod
#pod This is another name (and the preferred one) for C<header_pairs>.
#pod
#pod =cut

BEGIN {
  no strict 'refs';
  for my $method (qw(
    header_raw header
    header_raw_set header_set
    header_raw_prepend
    header_raw_pairs header_pairs
    header_names
  )) {
    *$method = sub { (shift)->header_obj->$method(@_) };
  }
  *headers = \&header_names;
}

#pod =method body
#pod
#pod Returns the body text of the mail.
#pod
#pod =cut

sub body {
  my ($self) = @_;
  return (defined ${ $self->{body} }) ? ${ $self->{body} } : '';
}

#pod =method body_set
#pod
#pod Sets the body text of the mail.
#pod
#pod =cut

sub body_set {
  my ($self, $text) = @_;
  my $text_ref = ref $text ? $text : \$text;
  $self->{body} = $text_ref;
  return;
}

#pod =method as_string
#pod
#pod Returns the mail as a string, reconstructing the headers.
#pod
#pod =cut

sub as_string {
  my $self = shift;
  return $self->header_obj->as_string . $self->crlf . $self->body;
}

#pod =method crlf
#pod
#pod This method returns the type of newline used in the email.  It is an accessor
#pod only.
#pod
#pod =cut

sub crlf { $_[0]->{mycrlf} }

#pod =method default_header_class
#pod
#pod This returns the class used, by default, for header objects, and is provided
#pod for subclassing.  The default default is Email::Simple::Header.
#pod
#pod =cut

sub default_header_class { 'Email::Simple::Header' }

1;

=pod

=encoding UTF-8

=head1 NAME

Email::Simple - simple parsing of RFC2822 message format and headers

=head1 VERSION

version 2.216

=head1 SYNOPSIS

  use Email::Simple;
  my $email = Email::Simple->new($text);

  my $from_header = $email->header("From");
  my @received = $email->header("Received");

  $email->header_set("From", 'Simon Cozens <simon@cpan.org>');

  my $old_body = $email->body;
  $email->body_set("Hello world\nSimon");

  print $email->as_string;

...or, to create a message from scratch...

  my $email = Email::Simple->create(
      header => [
        From    => 'casey@geeknest.com',
        To      => 'drain@example.com',
        Subject => 'Message in a bottle',
      ],
      body => '...',
  );

  $email->header_set( 'X-Content-Container' => 'bottle/glass' );

  print $email->as_string;

=head1 DESCRIPTION

The Email:: namespace was begun as a reaction against the increasing complexity
and bugginess of Perl's existing email modules.  C<Email::*> modules are meant
to be simple to use and to maintain, pared to the bone, fast, minimal in their
external dependencies, and correct.

=head1 METHODS

=head2 new

  my $email = Email::Simple->new($message, \%arg);

This method parses an email from a scalar containing an RFC2822 formatted
message and returns an object.  C<$message> may be a reference to a message
string, in which case the string will be altered in place.  This can result in
significant memory savings.

If you want to create a message from scratch, you should use the C<L</create>>
method.

Valid arguments are:

  header_class - the class used to create new header objects
                 The named module is not 'require'-ed by Email::Simple!

=head2 create

  my $email = Email::Simple->create(header => [ @headers ], body => '...');

This method is a constructor that creates an Email::Simple object
from a set of named parameters. The C<header> parameter's value is a
list reference containing a set of headers to be created. The C<body>
parameter's value is a scalar value holding the contents of the message
body.  Line endings in the body will normalized to CRLF.

If no C<Date> header is specified, one will be provided for you based on the
C<gmtime> of the local machine. This is because the C<Date> field is a required
header and is a pain in the neck to create manually for every message. The
C<From> field is also a required header, but it is I<not> provided for you.

=head2 header_obj

  my $header = $email->header_obj;

This method returns the object representing the email's header.  For the
interface for this object, see L<Email::Simple::Header>.

=head2 header_obj_set

  $email->header_obj_set($new_header_obj);

This method substitutes the given new header object for the email's existing
header object.

=head2 header

  my @values = $email->header($header_name);
  my $first  = $email->header($header_name);
  my $value  = $email->header($header_name, $index);

In list context, this returns every value for the named header.  In scalar
context, it returns the I<first> value for the named header.  If second
parameter is specified then instead I<first> value it returns value at
position C<$index> (negative C<$index> is from the end).

=head2 header_set

    $email->header_set($field, $line1, $line2, ...);

Sets the header to contain the given data. If you pass multiple lines
in, you get multiple headers, and order is retained.  If no values are given to
set, the header will be removed from to the message entirely.

=head2 header_raw

This is another name (and the preferred one) for C<header>.

=head2 header_raw_set

This is another name (and the preferred one) for C<header_set>.

=head2 header_raw_prepend

  $email->header_raw_prepend($field => $value);

This method adds a new instance of the name field as the first field in the
header.

=head2 header_names

    my @header_names = $email->header_names;

This method returns the list of header names currently in the email object.
These names can be passed to the C<header> method one-at-a-time to get header
values. You are guaranteed to get a set of headers that are unique. You are not
guaranteed to get the headers in any order at all.

For backwards compatibility, this method can also be called as B<headers>.

=head2 header_pairs

  my @headers = $email->header_pairs;

This method returns a list of pairs describing the contents of the header.
Every other value, starting with and including zeroth, is a header name and the
value following it is the header value.

=head2 header_raw_pairs

This is another name (and the preferred one) for C<header_pairs>.

=head2 body

Returns the body text of the mail.

=head2 body_set

Sets the body text of the mail.

=head2 as_string

Returns the mail as a string, reconstructing the headers.

=head2 crlf

This method returns the type of newline used in the email.  It is an accessor
only.

=head2 default_header_class

This returns the class used, by default, for header objects, and is provided
for subclassing.  The default default is Email::Simple::Header.

=head1 CAVEATS

Email::Simple handles only RFC2822 formatted messages.  This means you cannot
expect it to cope well as the only parser between you and the outside world,
say for example when writing a mail filter for invocation from a .forward file
(for this we recommend you use L<Email::Filter> anyway).

=head1 AUTHORS

=over 4

=item *

Simon Cozens

=item *

Casey West

=item *

Ricardo SIGNES

=back

=head1 CONTRIBUTORS

=for stopwords Brian Cassidy Christian Walde Marc Bradshaw Michael Stevens Pali Ricardo SIGNES Ronald F. Guilmette William Yardley

=over 4

=item *

Brian Cassidy <bricas@cpan.org>

=item *

Christian Walde <walde.christian@googlemail.com>

=item *

Marc Bradshaw <marc@marcbradshaw.net>

=item *

Michael Stevens <mstevens@etla.org>

=item *

Pali <pali@cpan.org>

=item *

Ricardo SIGNES <rjbs@cpan.org>

=item *

Ronald F. Guilmette <rfg@tristatelogic.com>

=item *

William Yardley <pep@veggiechinese.net>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2003 by Simon Cozens.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

__END__

#pod =head1 CAVEATS
#pod
#pod Email::Simple handles only RFC2822 formatted messages.  This means you cannot
#pod expect it to cope well as the only parser between you and the outside world,
#pod say for example when writing a mail filter for invocation from a .forward file
#pod (for this we recommend you use L<Email::Filter> anyway).
#pod
#pod =cut
