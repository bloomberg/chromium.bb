use 5.008001;
use strict;
use warnings;
package Email::MIME;
# ABSTRACT: easy MIME message handling
$Email::MIME::VERSION = '1.946';
use Email::Simple 2.212; # nth header value
use parent qw(Email::Simple);

use Carp ();
use Email::MessageID;
use Email::MIME::Creator;
use Email::MIME::ContentType 1.022; # parse_content_disposition
use Email::MIME::Encode;
use Email::MIME::Encodings 1.314;
use Email::MIME::Header;
use Email::MIME::Modifier;
use Encode 1.9801 ();
use Scalar::Util qw(reftype weaken);

our @CARP_NOT = qw(Email::MIME::ContentType);

#pod =head1 SYNOPSIS
#pod
#pod B<Wait!>  Before you read this, maybe you just need L<Email::Stuffer>, which is
#pod a much easier-to-use tool for building simple email messages that might have
#pod attachments or both plain text and HTML.  If that doesn't do it for you, then
#pod by all means keep reading.
#pod
#pod   use Email::MIME;
#pod   my $parsed = Email::MIME->new($message);
#pod
#pod   my @parts = $parsed->parts; # These will be Email::MIME objects, too.
#pod   my $decoded = $parsed->body;
#pod   my $non_decoded = $parsed->body_raw;
#pod
#pod   my $content_type = $parsed->content_type;
#pod
#pod ...or...
#pod
#pod   use Email::MIME;
#pod   use IO::All;
#pod
#pod   # multipart message
#pod   my @parts = (
#pod       Email::MIME->create(
#pod           attributes => {
#pod               filename     => "report.pdf",
#pod               content_type => "application/pdf",
#pod               encoding     => "quoted-printable",
#pod               name         => "2004-financials.pdf",
#pod           },
#pod           body => io( "2004-financials.pdf" )->binary->all,
#pod       ),
#pod       Email::MIME->create(
#pod           attributes => {
#pod               content_type => "text/plain",
#pod               disposition  => "attachment",
#pod               charset      => "US-ASCII",
#pod           },
#pod           body_str => "Hello there!",
#pod       ),
#pod   );
#pod
#pod   my $email = Email::MIME->create(
#pod       header_str => [
#pod           From => 'casey@geeknest.com',
#pod           To => [ 'user1@host.com', 'Name <user2@host.com>' ],
#pod           Cc => Email::Address::XS->new("Display Name \N{U+1F600}", 'user@example.com'),
#pod       ],
#pod       parts      => [ @parts ],
#pod   );
#pod
#pod   # nesting parts
#pod   $email->parts_set(
#pod       [
#pod         $email->parts,
#pod         Email::MIME->create( parts => [ @parts ] ),
#pod       ],
#pod   );
#pod
#pod   # standard modifications
#pod   $email->header_str_set( 'X-PoweredBy' => 'RT v3.0'      );
#pod   $email->header_str_set( To            => rcpts()        );
#pod   $email->header_str_set( Cc            => aux_rcpts()    );
#pod   $email->header_str_set( Bcc           => sekrit_rcpts() );
#pod
#pod   # more advanced
#pod   $_->encoding_set( 'base64' ) for $email->parts;
#pod
#pod   # Quick multipart creation
#pod   my $email = Email::MIME->create(
#pod       header_str => [
#pod           From => 'my@address',
#pod           To   => 'your@address',
#pod       ],
#pod       parts => [
#pod           q[This is part one],
#pod           q[This is part two],
#pod           q[These could be binary too],
#pod       ],
#pod   );
#pod
#pod   print $email->as_string;
#pod
#pod =head1 DESCRIPTION
#pod
#pod This is an extension of the L<Email::Simple> module, to handle MIME
#pod encoded messages. It takes a message as a string, splits it up into its
#pod constituent parts, and allows you access to various parts of the
#pod message. Headers are decoded from MIME encoding.
#pod
#pod =head1 METHODS
#pod
#pod Please see L<Email::Simple> for the base set of methods. It won't take
#pod very long. Added to that, you have:
#pod
#pod =cut

our $CREATOR = 'Email::MIME::Creator';

my $NO_ENCODE_RE = qr/
  \A
  (?:7bit|8bit|binary)\s*(?:;|$)
/ix;

sub new {
  my ($class, $text, $arg, @rest) = @_;
  $arg ||= {};

  my $encode_check = exists $arg->{encode_check}
                   ? delete $arg->{encode_check}
                   : Encode::FB_CROAK;

  my $self = shift->SUPER::new($text, $arg, @rest);
  $self->encode_check_set($encode_check);
  $self->{ct} = parse_content_type($self->content_type);
  $self->parts;
  return $self;
}

#pod =method create
#pod
#pod   my $single = Email::MIME->create(
#pod     header_str => [ ... ],
#pod     body_str   => '...',
#pod     attributes => { ... },
#pod   );
#pod
#pod   my $multi = Email::MIME->create(
#pod     header_str => [ ... ],
#pod     parts      => [ ... ],
#pod     attributes => { ... },
#pod   );
#pod
#pod This method creates a new MIME part. The C<header_str> parameter is a list of
#pod headers pairs to include in the message. The value for each pair is expected to
#pod be a text string that will be MIME-encoded as needed.  Alternatively it can be
#pod an object with C<as_mime_string> method which implements conversion of that
#pod object to MIME-encoded string.  That object method is called with two named
#pod input parameters: C<charset> and C<header_name_length>.  It should return
#pod MIME-encoded representation of the object.  As of 2017-07-25, the
#pod header-value-as-object code is very young, and may yet change.
#pod
#pod In case header name is registered in C<%Email::MIME::Header::header_to_class_map>
#pod hash then registered class is used for conversion from Unicode string to 8bit
#pod MIME encoding.  Value can be either string or array reference to strings.
#pod Object is constructed via method C<from_string> with string value (or values
#pod in case of array reference) and converted to MIME-encoded string via
#pod C<as_mime_string> method.
#pod
#pod A similar C<header> parameter can be provided in addition to or instead of
#pod C<header_str>.  Its values will be used verbatim.
#pod
#pod C<attributes> is a hash of MIME attributes to assign to the part, and may
#pod override portions of the header set in the C<header> parameter. The hash keys
#pod correspond directly to methods or modifying a message from
#pod C<Email::MIME::Modifier>. The allowed keys are: content_type, charset, name,
#pod format, boundary, encoding, disposition, and filename. They will be mapped to
#pod C<"$attr\_set"> for message modification.
#pod
#pod The C<parts> parameter is a list reference containing C<Email::MIME>
#pod objects. Elements of the C<parts> list can also be a non-reference
#pod string of data. In that case, an C<Email::MIME> object will be created
#pod for you. Simple checks will determine if the part is binary or not, and
#pod all parts created in this fashion are encoded with C<base64>, just in case.
#pod
#pod If C<body> is given instead of C<parts>, it specifies the body to be used for a
#pod flat (subpart-less) MIME message.  It is assumed to be a sequence of octets.
#pod
#pod If C<body_str> is given instead of C<body> or C<parts>, it is assumed to be a
#pod character string to be used as the body.  If you provide a C<body_str>
#pod parameter, you B<must> provide C<charset> and C<encoding> attributes.
#pod
#pod =cut

my %CT_SETTER = map {; $_ => 1 } qw(
  content_type charset name format boundary
  encoding
  disposition filename
);

sub create {
  my ($class, %args) = @_;

  my $header = '';
  my %headers;
  if (exists $args{header}) {
    my @headers = @{ $args{header} };
    pop @headers if @headers % 2 == 1;
    while (my ($key, $value) = splice @headers, 0, 2) {
      $headers{$key} = 1;
      $CREATOR->_add_to_header(\$header, $key, $value);
    }
  }

  if (exists $args{header_str}) {
    my @headers = @{ $args{header_str} };
    pop @headers if @headers % 2 == 1;
    while (my ($key, $value) = splice @headers, 0, 2) {
      $headers{$key} = 1;

      $value = Email::MIME::Encode::maybe_mime_encode_header(
          $key, $value, 'UTF-8'
      );
      $CREATOR->_add_to_header(\$header, $key, $value);
    }
  }

  $CREATOR->_add_to_header(\$header, Date => $CREATOR->_date_header)
    unless exists $headers{Date};
  $CREATOR->_add_to_header(\$header, 'MIME-Version' => '1.0',);

  my %attrs = $args{attributes} ? %{ $args{attributes} } : ();

  # XXX: This is awful... but if we don't do this, then Email::MIME->new will
  # end up calling parse_content_type($self->content_type) which will mean
  # parse_content_type(undef) which, for some reason, returns the default.
  # It's really sort of mind-boggling.  Anyway, the default ends up being
  # q{text/plain; charset="us-ascii"} so that if content_type is in the
  # attributes, but not charset, then charset isn't changed and you up with
  # something that's q{image/jpeg; charset="us-ascii"} and you look like a
  # moron. -- rjbs, 2009-01-20
  if (
    grep { exists $attrs{$_} } qw(content_type charset name format boundary)
  ) {
    $CREATOR->_add_to_header(\$header, 'Content-Type' => 'text/plain',);
  }

  my %pass_on;

  if (exists $args{encode_check}) {
    $pass_on{encode_check} = $args{encode_check};
  }

  my $email = $class->new($header, \%pass_on);

  for my $key (keys %attrs) {
    $email->content_type_attribute_set($key => $attrs{$key});
  }

  my $body_args = grep { defined $args{$_} } qw(parts body body_str);
  Carp::confess("only one of parts, body, or body_str may be given")
    if $body_args > 1;

  if ($args{parts} && @{ $args{parts} }) {
    foreach my $part (@{ $args{parts} }) {
      $part = $CREATOR->_construct_part($part)
        unless ref($part);
    }
    $email->parts_set($args{parts});
  } elsif (defined $args{body}) {
    $email->body_set($args{body});
  } elsif (defined $args{body_str}) {
    Carp::confess("body_str was given, but no charset is defined")
      unless my $charset = $attrs{charset};

    Carp::confess("body_str was given, but no encoding is defined")
      unless $attrs{encoding};

    my $body_octets = Encode::encode($attrs{charset}, $args{body_str}, $email->encode_check);
    $email->body_set($body_octets);
  }

  $email;
}

sub as_string {
  my $self = shift;
  return $self->__head->as_string
    . ($self->{mycrlf} || "\n")  # XXX: replace with ->crlf
    . $self->body_raw;
}

sub parts {
  my $self = shift;

  $self->fill_parts unless $self->{parts};

  my @parts = @{ $self->{parts} };
  @parts = $self unless @parts;
  return @parts;
}

sub subparts {
  my ($self) = @_;

  $self->fill_parts unless $self->{parts};
  my @parts = @{ $self->{parts} };
  return @parts;
}

sub fill_parts {
  my $self = shift;

  if (
    $self->{ct}{type} eq "multipart"
    or $self->{ct}{type} eq "message"
  ) {
    $self->parts_multipart;
  } else {
    $self->parts_single_part;
  }

  return $self;
}

sub body {
  my $self = shift;
  my $body = $self->SUPER::body;
  my $cte  = $self->header("Content-Transfer-Encoding") || '';

  $cte =~ s/\A\s+//;
  $cte =~ s/\s+\z//;
  $cte =~ s/;.+//; # For S/MIME, etc.

  return $body unless $cte;

  if (!$self->force_decode_hook and $cte =~ $NO_ENCODE_RE) {
    return $body;
  }

  $body = $self->decode_hook($body) if $self->can("decode_hook");

  $body = Email::MIME::Encodings::decode($cte, $body, '7bit');
  return $body;
}

sub parts_single_part {
  my $self = shift;
  $self->{parts} = [];
  return $self;
}

sub body_raw {
  return $_[0]->{body_raw} || $_[0]->SUPER::body;
}

sub body_str {
  my ($self) = @_;
  my $encoding = $self->{ct}{attributes}{charset};

  unless ($encoding) {
    if ($self->{ct}{type} eq 'text'
      and
      ($self->{ct}{subtype} eq 'plain' or $self->{ct}{subtype} eq 'html')
    ) {

      # assume that plaintext or html without ANY charset is us-ascii
      return $self->body;
    }

    Carp::confess("can't get body as a string for " . $self->content_type);
  }

  my $str = Encode::decode($encoding, $self->body, $self->encode_check);
  return $str;
}

sub parts_multipart {
  my $self     = shift;
  my $boundary = $self->{ct}->{attributes}->{boundary};

  # Take a message, join all its lines together.  Now try to Email::MIME->new
  # it with 1.861 or earlier.  Death!  It tries to recurse endlessly on the
  # body, because every time it splits on boundary it gets itself. Obviously
  # that means it's a bogus message, but a mangled result (or exception) is
  # better than endless recursion. -- rjbs, 2008-01-07
  return $self->parts_single_part
    unless $boundary and $self->body_raw =~ /^--\Q$boundary\E\s*$/sm;

  $self->{body_raw} = $self->SUPER::body;

  # rfc1521 7.2.1
  my ($body, $epilogue) = split /^--\Q$boundary\E--\s*$/sm, $self->body_raw, 2;

  my @bits = split /^--\Q$boundary\E\s*$/sm, ($body || '');

  $self->SUPER::body_set(undef);

  # If there are no headers in the potential MIME part, it's just part of the
  # body.  This is a horrible hack, although it's debatable whether it was
  # better or worse when it was $self->{body} = shift @bits ... -- rjbs,
  # 2006-11-27
  $self->SUPER::body_set(shift @bits) if ($bits[0] || '') !~ /.*:.*/;

  my $bits = @bits;

  my @parts;
  for my $bit (@bits) {
    $bit =~ s/\A[\n\r]+//smg;
    $bit =~ s/(?<!\x0d)$self->{mycrlf}\Z//sm;
    my $email = (ref $self)->new($bit, { encode_check => $self->encode_check });
    push @parts, $email;
  }

  $self->{parts} = \@parts;

  return @{ $self->{parts} };
}

sub force_decode_hook { 0 }
sub decode_hook       { return $_[1] }
sub content_type      { scalar shift->header("Content-type"); }

sub debug_structure {
  my ($self, $level) = @_;
  $level ||= 0;
  my $rv = " " x (5 * $level);
  $rv .= "+ " . ($self->content_type || '') . "\n";
  my @parts = $self->subparts;
  $rv .= $_->debug_structure($level + 1) for @parts;
  return $rv;
}

my %gcache;

sub filename {
  my ($self, $force) = @_;
  return $gcache{$self} if exists $gcache{$self};

  my $dis = $self->header("Content-Disposition") || '';
  my $attrs = parse_content_disposition($dis)->{attributes};
  my $name = $attrs->{filename}
    || $self->{ct}{attributes}{name};
  return $name if $name or !$force;
  return $gcache{$self} = $self->invent_filename(
    $self->{ct}->{type} . "/" . $self->{ct}->{subtype});
}

my $gname = 0;

sub invent_filename {
  my ($self, $ct) = @_;
  require MIME::Types;
  my $type = MIME::Types->new->type($ct);
  my $ext = $type && (($type->extensions)[0]);
  $ext ||= "dat";
  return "attachment-$$-" . $gname++ . ".$ext";
}

sub default_header_class { 'Email::MIME::Header' }

sub header_str {
  my $self = shift;
  $self->header_obj->header_str(@_);
}

sub header_str_set {
  my $self = shift;
  $self->header_obj->header_str_set(@_);
}

sub header_str_pairs {
  my $self = shift;
  $self->header_obj->header_str_pairs(@_);
}

sub header_as_obj {
  my $self = shift;
  $self->header_obj->header_as_obj(@_);
}

#pod =method content_type_set
#pod
#pod   $email->content_type_set( 'text/html' );
#pod
#pod Change the content type. All C<Content-Type> header attributes
#pod will remain intact.
#pod
#pod =cut

sub content_type_set {
  my ($self, $ct) = @_;
  my $ct_header = parse_content_type($self->header('Content-Type'));
  @{$ct_header}{qw[type subtype]} = split m[/], $ct;
  $self->_compose_content_type($ct_header);
  $self->_reset_cids;
  return $ct;
}

#pod =method charset_set
#pod
#pod =method name_set
#pod
#pod =method format_set
#pod
#pod =method boundary_set
#pod
#pod   $email->charset_set( 'UTF-8' );
#pod   $email->name_set( 'some_filename.txt' );
#pod   $email->format_set( 'flowed' );
#pod   $email->boundary_set( undef ); # remove the boundary
#pod
#pod These four methods modify common C<Content-Type> attributes. If set to
#pod C<undef>, the attribute is removed. All other C<Content-Type> header
#pod information is preserved when modifying an attribute.
#pod
#pod =cut

BEGIN {
  foreach my $attr (qw[charset name format]) {
    my $code = sub {
      my ($self, $value) = @_;
      my $ct_header = parse_content_type($self->header('Content-Type'));
      if ($value) {
        $ct_header->{attributes}->{$attr} = $value;
      } else {
        delete $ct_header->{attributes}->{$attr};
      }
      $self->_compose_content_type($ct_header);
      return $value;
    };

    no strict 'refs';  ## no critic strict
    *{"$attr\_set"} = $code;
  }
}

sub boundary_set {
  my ($self, $value) = @_;
  my $ct_header = parse_content_type($self->header('Content-Type'));

  if ($value) {
    $ct_header->{attributes}->{boundary} = $value;
  } else {
    delete $ct_header->{attributes}->{boundary};
  }
  $self->_compose_content_type($ct_header);

  $self->parts_set([ $self->parts ]) if $self->parts > 1;
}

sub content_type_attribute_set {
  my ($self, $key, $value) = @_;
  $key = lc $key;

  if ($CT_SETTER{$key}) {
    my $method = "$key\_set";
    return $self->$method($value);
  }

  my $ct_header = parse_content_type($self->header('Content-Type'));
  my $attrs = $ct_header->{attributes};

  for my $existing_key (keys %$attrs) {
    delete $attrs->{$existing_key} if lc $existing_key eq $key;
  }

  if ($value) {
    $ct_header->{attributes}->{$key} = $value;
  } else {
    delete $ct_header->{attributes}->{$key};
  }
  $self->_compose_content_type($ct_header);
}

#pod =method encode_check
#pod
#pod =method encode_check_set
#pod
#pod   $email->encode_check;
#pod   $email->encode_check_set(0);
#pod   $email->encode_check_set(Encode::FB_DEFAULT);
#pod
#pod Gets/sets the current C<encode_check> setting (default: I<FB_CROAK>).
#pod This is the parameter passed to L<Encode/"decode"> and L<Encode/"encode">
#pod when C<body_str()>, C<body_str_set()>, and C<create()> are called.
#pod
#pod With the default setting, Email::MIME may crash if the claimed charset
#pod of a body does not match its contents (for example - utf8 data in a
#pod text/plain; charset=us-ascii message).
#pod
#pod With an C<encode_check> of 0, the unrecognized bytes will instead be
#pod replaced with the C<REPLACEMENT CHARACTER> (U+0FFFD), and may end up
#pod as either that or question marks (?).
#pod
#pod See L<Encode/"Handling Malformed Data"> for more information.
#pod
#pod =cut

sub encode_check {
  my ($self) = @_;

  return $self->{encode_check};
}

sub encode_check_set {
  my ($self, $val) = @_;

  return $self->{encode_check} = $val;
}

#pod =method encoding_set
#pod
#pod   $email->encoding_set( 'base64' );
#pod   $email->encoding_set( 'quoted-printable' );
#pod   $email->encoding_set( '8bit' );
#pod
#pod Convert the message body and alter the C<Content-Transfer-Encoding>
#pod header using this method. Your message body, the output of the C<body()>
#pod method, will remain the same. The raw body, output with the C<body_raw()>
#pod method, will be changed to reflect the new encoding.
#pod
#pod =cut

sub encoding_set {
  my ($self, $enc) = @_;
  $enc ||= '7bit';
  my $body = $self->body;
  $self->header_raw_set('Content-Transfer-Encoding' => $enc);
  $self->body_set($body);
}

#pod =method body_set
#pod
#pod   $email->body_set( $unencoded_body_string );
#pod
#pod This method will encode the new body you send using the encoding
#pod specified in the C<Content-Transfer-Encoding> header, then set
#pod the body to the new encoded body.
#pod
#pod This method overrides the default C<body_set()> method.
#pod
#pod =cut

sub body_set {
  my ($self, $body) = @_;
  my $body_ref;

  if (ref $body) {
    Carp::croak("provided body reference is not a scalar reference")
      unless reftype($body) eq 'SCALAR';
    $body_ref = $body;
  } else {
    $body_ref = \$body;
  }
  my $enc = $self->header('Content-Transfer-Encoding');

  # XXX: This is a disgusting hack and needs to be fixed, probably by a
  # clearer definition and reengineering of Simple construction.  The bug
  # this fixes is an indirect result of the previous behavior in which all
  # Simple subclasses were free to alter the guts of the Email::Simple
  # object. -- rjbs, 2007-07-16
  unless (((caller(1))[3] || '') eq 'Email::Simple::new') {
    $$body_ref = Email::MIME::Encodings::encode($enc, $$body_ref)
      unless !$enc || $enc =~ $NO_ENCODE_RE;
  }

  $self->{body_raw} = $$body_ref;
  $self->SUPER::body_set($body_ref);
}

#pod =method body_str_set
#pod
#pod   $email->body_str_set($unicode_str);
#pod
#pod This method behaves like C<body_set>, but assumes that the given value is a
#pod Unicode string that should be encoded into the message's charset
#pod before being set.
#pod
#pod The charset must already be set, either manually (via the C<attributes>
#pod argument to C<create> or C<charset_set>) or through the C<Content-Type> of a
#pod parsed message.  If the charset can't be determined, an exception is thrown.
#pod
#pod =cut

sub body_str_set {
  my ($self, $body_str) = @_;

  my $ct = parse_content_type($self->content_type);
  Carp::confess("body_str was given, but no charset is defined")
    unless my $charset = $ct->{attributes}{charset};

  my $body_octets = Encode::encode($charset, $body_str, $self->encode_check);
  $self->body_set($body_octets);
}

#pod =method disposition_set
#pod
#pod   $email->disposition_set( 'attachment' );
#pod
#pod Alter the C<Content-Disposition> of a message. All header attributes
#pod will remain intact.
#pod
#pod =cut

sub disposition_set {
  my ($self, $dis) = @_;
  $dis ||= 'inline';
  my $dis_header = $self->header('Content-Disposition');
  $dis_header
    ? ($dis_header =~ s/^([^;]+)/$dis/)
    : ($dis_header = $dis);
  $self->header_raw_set('Content-Disposition' => $dis_header);
}

#pod =method filename_set
#pod
#pod   $email->filename_set( 'boo.pdf' );
#pod
#pod Sets the filename attribute in the C<Content-Disposition> header. All other
#pod header information is preserved when setting this attribute.
#pod
#pod =cut

sub filename_set {
  my ($self, $filename) = @_;
  my $dis_header = $self->header('Content-Disposition');
  my ($disposition, $attrs);
  if ($dis_header) {
    my $struct = parse_content_disposition($dis_header);
    $disposition = $struct->{type};
    $attrs = $struct->{attributes};
  }
  $filename ? $attrs->{filename} = $filename : delete $attrs->{filename};
  $disposition ||= 'inline';

  my $dis = $disposition;
  while (my ($attr, $val) = each %{$attrs}) {
    $dis .= qq[; $attr="$val"];
  }

  $self->header_raw_set('Content-Disposition' => $dis);
}

#pod =method parts_set
#pod
#pod   $email->parts_set( \@new_parts );
#pod
#pod Replaces the parts for an object. Accepts a reference to a list of
#pod C<Email::MIME> objects, representing the new parts. If this message was
#pod originally a single part, the C<Content-Type> header will be changed to
#pod C<multipart/mixed>, and given a new boundary attribute.
#pod
#pod =cut

sub parts_set {
  my ($self, $parts) = @_;
  my $body = q{};

  my $ct_header = parse_content_type($self->header('Content-Type'));

  if (@{$parts} > 1 or $ct_header->{type} eq 'multipart') {

    # setup multipart
    $ct_header->{attributes}->{boundary} ||= Email::MessageID->new->user;
    my $bound = $ct_header->{attributes}->{boundary};
    foreach my $part (@{$parts}) {
      $body .= "$self->{mycrlf}--$bound$self->{mycrlf}";
      $body .= $part->as_string;
    }
    $body .= "$self->{mycrlf}--$bound--$self->{mycrlf}";

    unless (grep { $ct_header->{type} eq $_ } qw[multipart message]) {
      if (scalar $self->header('Content-Type')) {
        Carp::carp("replacing non-multipart type ($ct_header->{type}/$ct_header->{subtype}) with multipart/mixed");
      }
      @{$ct_header}{qw[type subtype]} = qw[multipart mixed];
    }

    $self->encoding_set('7bit');
    delete $ct_header->{attributes}{charset};
  } elsif (@$parts == 1) {  # setup singlepart
    $body .= $parts->[0]->body;

    my $from_ct = parse_content_type($parts->[0]->header('Content-Type'));
    @{$ct_header}{qw[type subtype]} = @{ $from_ct }{qw[type subtype]};

    $ct_header->{attributes}{charset} = $from_ct->{attributes}{charset};

    $self->encoding_set($parts->[0]->header('Content-Transfer-Encoding'));
    delete $ct_header->{attributes}->{boundary};
  }

  $self->_compose_content_type($ct_header);
  $self->body_set($body);
  $self->fill_parts;
  $self->_reset_cids;
}

#pod =method parts_add
#pod
#pod   $email->parts_add( \@more_parts );
#pod
#pod Adds MIME parts onto the current MIME part. This is a simple extension
#pod of C<parts_set> to make our lives easier. It accepts an array reference
#pod of additional parts.
#pod
#pod =cut

sub parts_add {
  my ($self, $parts) = @_;
  $self->parts_set([ $self->parts, @{$parts}, ]);
}

#pod =method walk_parts
#pod
#pod   $email->walk_parts(sub {
#pod       my ($part) = @_;
#pod       return if $part->subparts; # multipart
#pod
#pod       if ( $part->content_type =~ m[text/html]i ) {
#pod           my $body = $part->body;
#pod           $body =~ s/<link [^>]+>//; # simple filter example
#pod           $part->body_set( $body );
#pod       }
#pod   });
#pod
#pod Walks through all the MIME parts in a message and applies a callback to
#pod each. Accepts a code reference as its only argument. The code reference
#pod will be passed a single argument, the current MIME part within the
#pod top-level MIME object. All changes will be applied in place.
#pod
#pod =cut

sub walk_parts {
  my ($self, $callback) = @_;

  my %changed;

  my $walk_weak;
  my $walk = sub {
    my ($part) = @_;
    $callback->($part);

    if (my @orig_subparts = $part->subparts) {
      my $differ;
      my @subparts;

      for my $part (@orig_subparts) {
        my $str = $part->as_string;
        next unless my $new = $walk_weak->($part);
        $differ = 1 if $str ne $new->as_string;
        push @subparts, $new;
      }

      $differ
        ||=  (@subparts != @orig_subparts)
        ||   (grep { $subparts[$_] != $orig_subparts[$_] } (0 .. $#subparts))
        ||   (grep { $changed{ 0+$subparts[$_] } } (0 .. $#subparts));

      if ($differ) {
        $part->parts_set(\@subparts);
        $changed{ 0+$part }++;
      }
    }

    return $part;
  };

  $walk_weak = $walk;
  weaken $walk_weak;

  my $rv = $walk->($self);

  undef $walk;

  return $rv;
}

sub _compose_content_type {
  my ($self, $ct_header) = @_;
  my $ct = join q{/}, @{$ct_header}{qw[type subtype]};
  for my $attr (sort keys %{ $ct_header->{attributes} }) {
    next unless defined (my $value = $ct_header->{attributes}{$attr});
    $ct .= qq[; $attr="$value"];
  }
  $self->header_raw_set('Content-Type' => $ct);
  $self->{ct} = $ct_header;
}

sub _get_cid {
  Email::MessageID->new->address;
}

sub _reset_cids {
  my ($self) = @_;

  my $ct_header = parse_content_type($self->header('Content-Type'));

  if ($self->parts > 1) {
    if ($ct_header->{subtype} eq 'alternative') {
      my %cids;
      for my $part ($self->parts) {
        my $cid
          = defined $part->header('Content-ID')
          ? $part->header('Content-ID')
          : q{};
        $cids{$cid}++;
      }
      return if keys(%cids) == 1;

      my $cid = $self->_get_cid;
      $_->header_raw_set('Content-ID' => "<$cid>") for $self->parts;
    } else {
      foreach ($self->parts) {
        my $cid = $self->_get_cid;
        $_->header_raw_set('Content-ID' => "<$cid>")
          unless $_->header('Content-ID');
      }
    }
  }
}

1;

=pod

=encoding UTF-8

=head1 NAME

Email::MIME - easy MIME message handling

=head1 VERSION

version 1.946

=head1 SYNOPSIS

B<Wait!>  Before you read this, maybe you just need L<Email::Stuffer>, which is
a much easier-to-use tool for building simple email messages that might have
attachments or both plain text and HTML.  If that doesn't do it for you, then
by all means keep reading.

  use Email::MIME;
  my $parsed = Email::MIME->new($message);

  my @parts = $parsed->parts; # These will be Email::MIME objects, too.
  my $decoded = $parsed->body;
  my $non_decoded = $parsed->body_raw;

  my $content_type = $parsed->content_type;

...or...

  use Email::MIME;
  use IO::All;

  # multipart message
  my @parts = (
      Email::MIME->create(
          attributes => {
              filename     => "report.pdf",
              content_type => "application/pdf",
              encoding     => "quoted-printable",
              name         => "2004-financials.pdf",
          },
          body => io( "2004-financials.pdf" )->binary->all,
      ),
      Email::MIME->create(
          attributes => {
              content_type => "text/plain",
              disposition  => "attachment",
              charset      => "US-ASCII",
          },
          body_str => "Hello there!",
      ),
  );

  my $email = Email::MIME->create(
      header_str => [
          From => 'casey@geeknest.com',
          To => [ 'user1@host.com', 'Name <user2@host.com>' ],
          Cc => Email::Address::XS->new("Display Name \N{U+1F600}", 'user@example.com'),
      ],
      parts      => [ @parts ],
  );

  # nesting parts
  $email->parts_set(
      [
        $email->parts,
        Email::MIME->create( parts => [ @parts ] ),
      ],
  );

  # standard modifications
  $email->header_str_set( 'X-PoweredBy' => 'RT v3.0'      );
  $email->header_str_set( To            => rcpts()        );
  $email->header_str_set( Cc            => aux_rcpts()    );
  $email->header_str_set( Bcc           => sekrit_rcpts() );

  # more advanced
  $_->encoding_set( 'base64' ) for $email->parts;

  # Quick multipart creation
  my $email = Email::MIME->create(
      header_str => [
          From => 'my@address',
          To   => 'your@address',
      ],
      parts => [
          q[This is part one],
          q[This is part two],
          q[These could be binary too],
      ],
  );

  print $email->as_string;

=head1 DESCRIPTION

This is an extension of the L<Email::Simple> module, to handle MIME
encoded messages. It takes a message as a string, splits it up into its
constituent parts, and allows you access to various parts of the
message. Headers are decoded from MIME encoding.

=head1 METHODS

Please see L<Email::Simple> for the base set of methods. It won't take
very long. Added to that, you have:

=head2 create

  my $single = Email::MIME->create(
    header_str => [ ... ],
    body_str   => '...',
    attributes => { ... },
  );

  my $multi = Email::MIME->create(
    header_str => [ ... ],
    parts      => [ ... ],
    attributes => { ... },
  );

This method creates a new MIME part. The C<header_str> parameter is a list of
headers pairs to include in the message. The value for each pair is expected to
be a text string that will be MIME-encoded as needed.  Alternatively it can be
an object with C<as_mime_string> method which implements conversion of that
object to MIME-encoded string.  That object method is called with two named
input parameters: C<charset> and C<header_name_length>.  It should return
MIME-encoded representation of the object.  As of 2017-07-25, the
header-value-as-object code is very young, and may yet change.

In case header name is registered in C<%Email::MIME::Header::header_to_class_map>
hash then registered class is used for conversion from Unicode string to 8bit
MIME encoding.  Value can be either string or array reference to strings.
Object is constructed via method C<from_string> with string value (or values
in case of array reference) and converted to MIME-encoded string via
C<as_mime_string> method.

A similar C<header> parameter can be provided in addition to or instead of
C<header_str>.  Its values will be used verbatim.

C<attributes> is a hash of MIME attributes to assign to the part, and may
override portions of the header set in the C<header> parameter. The hash keys
correspond directly to methods or modifying a message from
C<Email::MIME::Modifier>. The allowed keys are: content_type, charset, name,
format, boundary, encoding, disposition, and filename. They will be mapped to
C<"$attr\_set"> for message modification.

The C<parts> parameter is a list reference containing C<Email::MIME>
objects. Elements of the C<parts> list can also be a non-reference
string of data. In that case, an C<Email::MIME> object will be created
for you. Simple checks will determine if the part is binary or not, and
all parts created in this fashion are encoded with C<base64>, just in case.

If C<body> is given instead of C<parts>, it specifies the body to be used for a
flat (subpart-less) MIME message.  It is assumed to be a sequence of octets.

If C<body_str> is given instead of C<body> or C<parts>, it is assumed to be a
character string to be used as the body.  If you provide a C<body_str>
parameter, you B<must> provide C<charset> and C<encoding> attributes.

=head2 content_type_set

  $email->content_type_set( 'text/html' );

Change the content type. All C<Content-Type> header attributes
will remain intact.

=head2 charset_set

=head2 name_set

=head2 format_set

=head2 boundary_set

  $email->charset_set( 'UTF-8' );
  $email->name_set( 'some_filename.txt' );
  $email->format_set( 'flowed' );
  $email->boundary_set( undef ); # remove the boundary

These four methods modify common C<Content-Type> attributes. If set to
C<undef>, the attribute is removed. All other C<Content-Type> header
information is preserved when modifying an attribute.

=head2 encode_check

=head2 encode_check_set

  $email->encode_check;
  $email->encode_check_set(0);
  $email->encode_check_set(Encode::FB_DEFAULT);

Gets/sets the current C<encode_check> setting (default: I<FB_CROAK>).
This is the parameter passed to L<Encode/"decode"> and L<Encode/"encode">
when C<body_str()>, C<body_str_set()>, and C<create()> are called.

With the default setting, Email::MIME may crash if the claimed charset
of a body does not match its contents (for example - utf8 data in a
text/plain; charset=us-ascii message).

With an C<encode_check> of 0, the unrecognized bytes will instead be
replaced with the C<REPLACEMENT CHARACTER> (U+0FFFD), and may end up
as either that or question marks (?).

See L<Encode/"Handling Malformed Data"> for more information.

=head2 encoding_set

  $email->encoding_set( 'base64' );
  $email->encoding_set( 'quoted-printable' );
  $email->encoding_set( '8bit' );

Convert the message body and alter the C<Content-Transfer-Encoding>
header using this method. Your message body, the output of the C<body()>
method, will remain the same. The raw body, output with the C<body_raw()>
method, will be changed to reflect the new encoding.

=head2 body_set

  $email->body_set( $unencoded_body_string );

This method will encode the new body you send using the encoding
specified in the C<Content-Transfer-Encoding> header, then set
the body to the new encoded body.

This method overrides the default C<body_set()> method.

=head2 body_str_set

  $email->body_str_set($unicode_str);

This method behaves like C<body_set>, but assumes that the given value is a
Unicode string that should be encoded into the message's charset
before being set.

The charset must already be set, either manually (via the C<attributes>
argument to C<create> or C<charset_set>) or through the C<Content-Type> of a
parsed message.  If the charset can't be determined, an exception is thrown.

=head2 disposition_set

  $email->disposition_set( 'attachment' );

Alter the C<Content-Disposition> of a message. All header attributes
will remain intact.

=head2 filename_set

  $email->filename_set( 'boo.pdf' );

Sets the filename attribute in the C<Content-Disposition> header. All other
header information is preserved when setting this attribute.

=head2 parts_set

  $email->parts_set( \@new_parts );

Replaces the parts for an object. Accepts a reference to a list of
C<Email::MIME> objects, representing the new parts. If this message was
originally a single part, the C<Content-Type> header will be changed to
C<multipart/mixed>, and given a new boundary attribute.

=head2 parts_add

  $email->parts_add( \@more_parts );

Adds MIME parts onto the current MIME part. This is a simple extension
of C<parts_set> to make our lives easier. It accepts an array reference
of additional parts.

=head2 walk_parts

  $email->walk_parts(sub {
      my ($part) = @_;
      return if $part->subparts; # multipart

      if ( $part->content_type =~ m[text/html]i ) {
          my $body = $part->body;
          $body =~ s/<link [^>]+>//; # simple filter example
          $part->body_set( $body );
      }
  });

Walks through all the MIME parts in a message and applies a callback to
each. Accepts a code reference as its only argument. The code reference
will be passed a single argument, the current MIME part within the
top-level MIME object. All changes will be applied in place.

=head2 header

B<Achtung!>  Beware this method!  In Email::MIME, it means the same as
C<header_str>, but on an Email::Simple object, it means C<header_raw>.  Unless
you always know what kind of object you have, you could get one of two
significantly different behaviors.

Try to use either C<header_str> or C<header_raw> as appropriate.

=head2 header_str_set

  $email->header_str_set($header_name => @value_strings);

This behaves like C<header_raw_set>, but expects Unicode (character) strings as
the values to set, rather than pre-encoded byte strings.  It will encode them
as MIME encoded-words if they contain any control or 8-bit characters.

Alternatively, values can be objects with C<as_mime_string> method.  Same as in
method C<create>.

=head2 header_str_pairs

  my @pairs = $email->header_str_pairs;

This method behaves like C<header_raw_pairs>, returning a list of field
name/value pairs, but the values have been decoded to character strings, when
possible.

=head2 header_as_obj

  my $first_obj = $email->header_as_obj($field);
  my $nth_obj   = $email->header_as_obj($field, $index);
  my @all_objs  = $email->header_as_obj($field);

  my $nth_obj_of_class  = $email->header_as_obj($field, $index, $class);
  my @all_objs_of_class = $email->header_as_obj($field, undef, $class);

This method returns an object representation of the header value.  It instances
new object via method C<from_mime_string> of specified class.  Input argument
for that class method is list of the raw MIME-encoded values.  If class argument
is not specified then class name is taken from the hash
C<%Email::MIME::Header::header_to_class_map> via key field.  Use class method
C<< Email::MIME::Header->set_class_for_header($class, $field) >> for adding new
mapping.

=head2 parts

This returns a list of C<Email::MIME> objects reflecting the parts of the
message. If it's a single-part message, you get the original object back.

In scalar context, this method returns the number of parts.

This is a stupid method.  Don't use it.

=head2 subparts

This returns a list of C<Email::MIME> objects reflecting the parts of the
message.  If it's a single-part message, this method returns an empty list.

In scalar context, this method returns the number of subparts.

=head2 body

This decodes and returns the body of the object I<as a byte string>. For
top-level objects in multi-part messages, this is highly likely to be something
like "This is a multi-part message in MIME format."

=head2 body_str

This decodes both the Content-Transfer-Encoding layer of the body (like the
C<body> method) as well as the charset encoding of the body (unlike the C<body>
method), returning a Unicode string.

If the charset is known, it is used.  If there is no charset but the content
type is either C<text/plain> or C<text/html>, us-ascii is assumed.  Otherwise,
an exception is thrown.

=head2 body_raw

This returns the body of the object, but doesn't decode the transfer encoding.

=head2 decode_hook

This method is called before the L<Email::MIME::Encodings> C<decode> method, to
decode the body of non-binary messages (or binary messages, if the
C<force_decode_hook> method returns true).  By default, this method does
nothing, but subclasses may define behavior.

This method could be used to implement the decryption of content in secure
email, for example.

=head2 content_type

This is a shortcut for access to the content type header.

=head2 filename

This provides the suggested filename for the attachment part. Normally
it will return the filename from the headers, but if C<filename> is
passed a true parameter, it will generate an appropriate "stable"
filename if one is not found in the MIME headers.

=head2 invent_filename

  my $filename = Email::MIME->invent_filename($content_type);

This routine is used by C<filename> to generate filenames for attached files.
It will attempt to choose a reasonable extension, falling back to F<dat>.

=head2 debug_structure

  my $description = $email->debug_structure;

This method returns a string that describes the structure of the MIME entity.
For example:

  + multipart/alternative; boundary="=_NextPart_2"; charset="BIG-5"
    + text/plain
    + text/html

=head1 TODO

All of the Email::MIME-specific guts should move to a single entry on the
object's guts.  This will require changes to both Email::MIME and
L<Email::MIME::Modifier>, sadly.

=head1 SEE ALSO

L<Email::Simple>, L<Email::MIME::Modifier>, L<Email::MIME::Creator>.

=head1 THANKS

This module was generously sponsored by Best Practical
(http://www.bestpractical.com/), Pete Sergeant, and Pobox.com.

=head1 AUTHORS

=over 4

=item *

Ricardo SIGNES <rjbs@cpan.org>

=item *

Casey West <casey@geeknest.com>

=item *

Simon Cozens <simon@cpan.org>

=back

=head1 CONTRIBUTORS

=for stopwords Alex Vandiver Arthur Axel 'fREW' Schmidt Brian Cassidy Dan Book David Steinbrunner Dotan Dimet Geraint Edwards Jesse Luehrs Kurt Anderson Lance A. Brown Matthew Horsfall (alh) memememomo Michael McClimon Pali Shawn Sorichetti Tomohiro Hosaka

=over 4

=item *

Alex Vandiver <alexmv@mit.edu>

=item *

Arthur Axel 'fREW' Schmidt <frioux@gmail.com>

=item *

Brian Cassidy <bricas@cpan.org>

=item *

Dan Book <grinnz@gmail.com>

=item *

David Steinbrunner <dsteinbrunner@pobox.com>

=item *

Dotan Dimet <dotan@corky.net>

=item *

Geraint Edwards <gedge-oss@yadn.org>

=item *

Jesse Luehrs <doy@tozt.net>

=item *

Kurt Anderson <kboth@drkurt.com>

=item *

Lance A. Brown <lance@bearcircle.net>

=item *

Matthew Horsfall (alh) <wolfsage@gmail.com>

=item *

memememomo <memememomo@gmail.com>

=item *

Michael McClimon <michael@mcclimon.org>

=item *

Pali <pali@cpan.org>

=item *

Shawn Sorichetti <ssoriche@coloredblocks.com>

=item *

Tomohiro Hosaka <bokutin@bokut.in>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2004 by Simon Cozens and Casey West.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

__END__

#pod =method header
#pod
#pod B<Achtung!>  Beware this method!  In Email::MIME, it means the same as
#pod C<header_str>, but on an Email::Simple object, it means C<header_raw>.  Unless
#pod you always know what kind of object you have, you could get one of two
#pod significantly different behaviors.
#pod
#pod Try to use either C<header_str> or C<header_raw> as appropriate.
#pod
#pod =method header_str_set
#pod
#pod   $email->header_str_set($header_name => @value_strings);
#pod
#pod This behaves like C<header_raw_set>, but expects Unicode (character) strings as
#pod the values to set, rather than pre-encoded byte strings.  It will encode them
#pod as MIME encoded-words if they contain any control or 8-bit characters.
#pod
#pod Alternatively, values can be objects with C<as_mime_string> method.  Same as in
#pod method C<create>.
#pod
#pod =method header_str_pairs
#pod
#pod   my @pairs = $email->header_str_pairs;
#pod
#pod This method behaves like C<header_raw_pairs>, returning a list of field
#pod name/value pairs, but the values have been decoded to character strings, when
#pod possible.
#pod
#pod =method header_as_obj
#pod
#pod   my $first_obj = $email->header_as_obj($field);
#pod   my $nth_obj   = $email->header_as_obj($field, $index);
#pod   my @all_objs  = $email->header_as_obj($field);
#pod
#pod   my $nth_obj_of_class  = $email->header_as_obj($field, $index, $class);
#pod   my @all_objs_of_class = $email->header_as_obj($field, undef, $class);
#pod
#pod This method returns an object representation of the header value.  It instances
#pod new object via method C<from_mime_string> of specified class.  Input argument
#pod for that class method is list of the raw MIME-encoded values.  If class argument
#pod is not specified then class name is taken from the hash
#pod C<%Email::MIME::Header::header_to_class_map> via key field.  Use class method
#pod C<< Email::MIME::Header->set_class_for_header($class, $field) >> for adding new
#pod mapping.
#pod
#pod =method parts
#pod
#pod This returns a list of C<Email::MIME> objects reflecting the parts of the
#pod message. If it's a single-part message, you get the original object back.
#pod
#pod In scalar context, this method returns the number of parts.
#pod
#pod This is a stupid method.  Don't use it.
#pod
#pod =method subparts
#pod
#pod This returns a list of C<Email::MIME> objects reflecting the parts of the
#pod message.  If it's a single-part message, this method returns an empty list.
#pod
#pod In scalar context, this method returns the number of subparts.
#pod
#pod =method body
#pod
#pod This decodes and returns the body of the object I<as a byte string>. For
#pod top-level objects in multi-part messages, this is highly likely to be something
#pod like "This is a multi-part message in MIME format."
#pod
#pod =method body_str
#pod
#pod This decodes both the Content-Transfer-Encoding layer of the body (like the
#pod C<body> method) as well as the charset encoding of the body (unlike the C<body>
#pod method), returning a Unicode string.
#pod
#pod If the charset is known, it is used.  If there is no charset but the content
#pod type is either C<text/plain> or C<text/html>, us-ascii is assumed.  Otherwise,
#pod an exception is thrown.
#pod
#pod =method body_raw
#pod
#pod This returns the body of the object, but doesn't decode the transfer encoding.
#pod
#pod =method decode_hook
#pod
#pod This method is called before the L<Email::MIME::Encodings> C<decode> method, to
#pod decode the body of non-binary messages (or binary messages, if the
#pod C<force_decode_hook> method returns true).  By default, this method does
#pod nothing, but subclasses may define behavior.
#pod
#pod This method could be used to implement the decryption of content in secure
#pod email, for example.
#pod
#pod =method content_type
#pod
#pod This is a shortcut for access to the content type header.
#pod
#pod =method filename
#pod
#pod This provides the suggested filename for the attachment part. Normally
#pod it will return the filename from the headers, but if C<filename> is
#pod passed a true parameter, it will generate an appropriate "stable"
#pod filename if one is not found in the MIME headers.
#pod
#pod =method invent_filename
#pod
#pod   my $filename = Email::MIME->invent_filename($content_type);
#pod
#pod This routine is used by C<filename> to generate filenames for attached files.
#pod It will attempt to choose a reasonable extension, falling back to F<dat>.
#pod
#pod =method debug_structure
#pod
#pod   my $description = $email->debug_structure;
#pod
#pod This method returns a string that describes the structure of the MIME entity.
#pod For example:
#pod
#pod   + multipart/alternative; boundary="=_NextPart_2"; charset="BIG-5"
#pod     + text/plain
#pod     + text/html
#pod
#pod =head1 TODO
#pod
#pod All of the Email::MIME-specific guts should move to a single entry on the
#pod object's guts.  This will require changes to both Email::MIME and
#pod L<Email::MIME::Modifier>, sadly.
#pod
#pod =head1 SEE ALSO
#pod
#pod L<Email::Simple>, L<Email::MIME::Modifier>, L<Email::MIME::Creator>.
#pod
#pod =head1 THANKS
#pod
#pod This module was generously sponsored by Best Practical
#pod (http://www.bestpractical.com/), Pete Sergeant, and Pobox.com.
#pod
#pod =cut
