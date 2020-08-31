# Copyright (c) 2016-2017 by Pali <pali@cpan.org>

package Email::MIME::Header::AddressList;
$Email::MIME::Header::AddressList::VERSION = '1.946';
use strict;
use warnings;

use Carp ();
use Email::Address::XS;
use Email::MIME::Encode;

#pod =encoding utf8
#pod
#pod =head1 NAME
#pod
#pod Email::MIME::Header::AddressList - MIME support for list of Email::Address::XS objects
#pod
#pod =head1 SYNOPSIS
#pod
#pod   my $address1 = Email::Address::XS->new('Name1' => 'address1@host.com');
#pod   my $address2 = Email::Address::XS->new("Name2 \N{U+263A}" => 'address2@host.com');
#pod   my $mime_address = Email::Address::XS->new('=?UTF-8?B?TmFtZTIg4pi6?=' => 'address2@host.com');
#pod
#pod   my $list1 = Email::MIME::Header::AddressList->new($address1, $address2);
#pod
#pod   $list1->append_groups('undisclosed-recipients' => []);
#pod
#pod   $list1->first_address();
#pod   # returns $address1
#pod
#pod   $list1->groups();
#pod   # returns (undef, [ $address1, $address2 ], 'undisclosed-recipients', [])
#pod
#pod   $list1->as_string();
#pod   # returns 'Name1 <address1@host.com>, "Name2 ☺" <address2@host.com>, undisclosed-recipients:;'
#pod
#pod   $list1->as_mime_string();
#pod   # returns 'Name1 <address1@host.com>, =?UTF-8?B?TmFtZTIg4pi6?= <address2@host.com>, undisclosed-recipients:;'
#pod
#pod   my $list2 = Email::MIME::Header::AddressList->new_groups(Group => [ $address1, $address2 ]);
#pod
#pod   $list2->append_addresses($address2);
#pod
#pod   $list2->addresses();
#pod   # returns ($address2, $address1, $address2)
#pod
#pod   $list2->groups();
#pod   # returns (undef, [ $address2 ], 'Group', [ $address1, $address2 ])
#pod
#pod   my $list3 = Email::MIME::Header::AddressList->new_mime_groups('=?UTF-8?B?4pi6?=' => [ $mime_address ]);
#pod   $list3->as_string();
#pod   # returns '☺: "Name2 ☺" <address2@host.com>;'
#pod
#pod   my $list4 = Email::MIME::Header::AddressList->from_string('Name1 <address1@host.com>, "Name2 ☺" <address2@host.com>, undisclosed-recipients:;');
#pod   my $list5 = Email::MIME::Header::AddressList->from_string('Name1 <address1@host.com>', '"Name2 ☺" <address2@host.com>', 'undisclosed-recipients:;');
#pod
#pod   my $list6 = Email::MIME::Header::AddressList->from_mime_string('Name1 <address1@host.com>, =?UTF-8?B?TmFtZTIg4pi6?= <address2@host.com>, undisclosed-recipients:;');
#pod   my $list7 = Email::MIME::Header::AddressList->from_mime_string('Name1 <address1@host.com>', '=?UTF-8?B?TmFtZTIg4pi6?= <address2@host.com>', 'undisclosed-recipients:;');
#pod
#pod =head1 DESCRIPTION
#pod
#pod This module implements object representation for the list of the
#pod L<Email::Address::XS|Email::Address::XS> objects. It provides methods for
#pod L<RFC 2047|https://tools.ietf.org/html/rfc2047> MIME encoding and decoding
#pod suitable for L<RFC 2822|https://tools.ietf.org/html/rfc2822> address-list
#pod structure.
#pod
#pod =head2 EXPORT
#pod
#pod None
#pod
#pod =head2 Class Methods
#pod
#pod =over 4
#pod
#pod =item new_empty
#pod
#pod Construct new empty C<Email::MIME::Header::AddressList> object.
#pod
#pod =cut

sub new_empty {
  my ($class) = @_;
  return bless { addresses => [], groups => [] }, $class;
}

#pod =item new
#pod
#pod Construct new C<Email::MIME::Header::AddressList> object from list of
#pod L<Email::Address::XS|Email::Address::XS> objects.
#pod
#pod =cut

sub new {
  my ($class, @addresses) = @_;
  my $self = $class->new_empty();
  $self->append_addresses(@addresses);
  return $self;
}

#pod =item new_groups
#pod
#pod Construct new C<Email::MIME::Header::AddressList> object from named groups of
#pod L<Email::Address::XS|Email::Address::XS> objects.
#pod
#pod =cut

sub new_groups {
  my ($class, @groups) = @_;
  my $self = $class->new_empty();
  $self->append_groups(@groups);
  return $self;
}

#pod =item new_mime_groups
#pod
#pod Like L<C<new_groups>|/new_groups> but in this method group names and objects properties are
#pod expected to be already MIME encoded, thus ASCII strings.
#pod
#pod =cut

sub new_mime_groups {
  my ($class, @groups) = @_;
  if (scalar @groups % 2) {
    Carp::carp 'Odd number of elements in argument list';
    return;
  }
  foreach (0 .. scalar @groups / 2 - 1) {
    $groups[2 * $_] = Email::MIME::Encode::mime_decode($groups[2 * $_])
      if defined $groups[2 * $_] and $groups[2 * $_] =~ /=\?/;
    $groups[2 * $_ + 1] = [ @{$groups[2 * $_ + 1]} ];
    foreach (@{$groups[2 * $_ + 1]}) {
      next unless Email::Address::XS->is_obj($_);
      my $decode_phrase = (defined $_->phrase and $_->phrase =~ /=\?/);
      my $decode_comment = (defined $_->comment and $_->comment =~ /=\?/);
      next unless $decode_phrase or $decode_comment;
      $_ = ref($_)->new(copy => $_);
      $_->phrase(Email::MIME::Encode::mime_decode($_->phrase))
        if $decode_phrase;
      $_->comment(Email::MIME::Encode::mime_decode($_->comment))
        if $decode_comment;
    }
  }
  return $class->new_groups(@groups);
}

#pod =item from_string
#pod
#pod Construct new C<Email::MIME::Header::AddressList> object from input string arguments.
#pod Calls L<Email::Address::XS::parse_email_groups|Email::Address::XS/parse_email_groups>.
#pod
#pod =cut

sub from_string {
  my ($class, @strings) = @_;
  return $class->new_groups(map { Email::Address::XS::parse_email_groups($_) } @strings);
}

#pod =item from_mime_string
#pod
#pod Like L<C<from_string>|/from_string> but input string arguments are expected to
#pod be already MIME encoded.
#pod
#pod =cut

sub from_mime_string {
  my ($class, @strings) = @_;
  return $class->new_mime_groups(map { Email::Address::XS::parse_email_groups($_) } @strings);
}

#pod =back
#pod
#pod =head2 Object Methods
#pod
#pod =over 4
#pod
#pod =item as_string
#pod
#pod Returns string representation of C<Email::MIME::Header::AddressList> object.
#pod Calls L<Email::Address::XS::format_email_groups|Email::Address::XS/format_email_groups>.
#pod
#pod =cut

sub as_string {
  my ($self) = @_;
  return Email::Address::XS::format_email_groups($self->groups());
}

#pod =item as_mime_string
#pod
#pod Like L<C<as_string>|/as_string> but output string will be properly and
#pod unambiguously MIME encoded. MIME encoding is done before calling
#pod L<Email::Address::XS::format_email_groups|Email::Address::XS/format_email_groups>.
#pod
#pod =cut

sub as_mime_string {
  my ($self, $arg) = @_;
  my $charset = $arg->{charset};
  my $header_name_length = $arg->{header_name_length};

  my @groups = $self->groups();
  foreach (0 .. scalar @groups / 2 - 1) {
    $groups[2 * $_] = Email::MIME::Encode::mime_encode($groups[2 * $_], $charset)
      if Email::MIME::Encode::_needs_mime_encode_addr($groups[2 * $_]);
    $groups[2 * $_ + 1] = [ @{$groups[2 * $_ + 1]} ];
    foreach (@{$groups[2 * $_ + 1]}) {
      my $encode_phrase = Email::MIME::Encode::_needs_mime_encode_addr($_->phrase);
      my $encode_comment = Email::MIME::Encode::_needs_mime_encode_addr($_->comment);
      next unless $encode_phrase or $encode_comment;
      $_ = ref($_)->new(copy => $_);
      $_->phrase(Email::MIME::Encode::mime_encode($_->phrase, $charset))
        if $encode_phrase;
      $_->comment(Email::MIME::Encode::mime_encode($_->comment, $charset))
        if $encode_comment;
    }
  }
  return Email::Address::XS::format_email_groups(@groups);
}

#pod =item first_address
#pod
#pod Returns first L<Email::Address::XS|Email::Address::XS> object.
#pod
#pod =cut

sub first_address {
  my ($self) = @_;
  return $self->{addresses}->[0] if @{$self->{addresses}};
  my $groups = $self->{groups};
  foreach (0 .. @{$groups} / 2 - 1) {
    next unless @{$groups->[2 * $_ + 1]};
    return $groups->[2 * $_ + 1]->[0];
  }
  return undef;
}

#pod =item addresses
#pod
#pod Returns list of all L<Email::Address::XS|Email::Address::XS> objects.
#pod
#pod =cut

sub addresses {
  my ($self) = @_;
  my $t = 1;
  my @addresses = @{$self->{addresses}};
  push @addresses, map { @{$_} } grep { $t ^= 1 } @{$self->{groups}};
  return @addresses;
}

#pod =item groups
#pod
#pod Like L<C<addresses>|/addresses> but returns objects with named groups.
#pod
#pod =cut

sub groups {
  my ($self) = @_;
  my @groups = @{$self->{groups}};
  $groups[2 * $_ + 1] = [ @{$groups[2 * $_ + 1]} ]
    foreach 0 .. scalar @groups / 2 - 1;
  unshift @groups, undef, [ @{$self->{addresses}} ]
    if @{$self->{addresses}};
  return @groups;
}

#pod =item append_addresses
#pod
#pod Append L<Email::Address::XS|Email::Address::XS> objects.
#pod
#pod =cut

sub append_addresses {
  my ($self, @addresses) = @_;
  my @valid_addresses = grep { Email::Address::XS->is_obj($_) } @addresses;
  Carp::carp 'Argument is not an Email::Address::XS object' if scalar @valid_addresses != scalar @addresses;
  push @{$self->{addresses}}, @valid_addresses;
}

#pod =item append_groups
#pod
#pod Like L<C<append_addresses>|/append_addresses> but arguments are pairs of name of
#pod group and array reference of L<Email::Address::XS|Email::Address::XS> objects.
#pod
#pod =cut

sub append_groups {
  my ($self, @groups) = @_;
  if (scalar @groups % 2) {
    Carp::carp 'Odd number of elements in argument list';
    return;
  }
  my $carp_invalid = 1;
  my @valid_groups;
  foreach (0 .. scalar @groups / 2 - 1) {
    push @valid_groups, $groups[2 * $_];
    my $addresses = $groups[2 * $_ + 1];
    my @valid_addresses = grep { Email::Address::XS->is_obj($_) } @{$addresses};
    if ($carp_invalid and scalar @valid_addresses != scalar @{$addresses}) {
      Carp::carp 'Array element is not an Email::Address::XS object';
      $carp_invalid = 0;
    }
    push @valid_groups, \@valid_addresses;
  }
  push @{$self->{groups}}, @valid_groups;
}

#pod =back
#pod
#pod =head1 SEE ALSO
#pod
#pod L<RFC 2047|https://tools.ietf.org/html/rfc2047>,
#pod L<RFC 2822|https://tools.ietf.org/html/rfc2822>,
#pod L<Email::MIME>,
#pod L<Email::Address::XS>
#pod
#pod =head1 AUTHOR
#pod
#pod Pali E<lt>pali@cpan.orgE<gt>
#pod
#pod =cut

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::MIME::Header::AddressList

=head1 VERSION

version 1.946

=head1 SYNOPSIS

  my $address1 = Email::Address::XS->new('Name1' => 'address1@host.com');
  my $address2 = Email::Address::XS->new("Name2 \N{U+263A}" => 'address2@host.com');
  my $mime_address = Email::Address::XS->new('=?UTF-8?B?TmFtZTIg4pi6?=' => 'address2@host.com');

  my $list1 = Email::MIME::Header::AddressList->new($address1, $address2);

  $list1->append_groups('undisclosed-recipients' => []);

  $list1->first_address();
  # returns $address1

  $list1->groups();
  # returns (undef, [ $address1, $address2 ], 'undisclosed-recipients', [])

  $list1->as_string();
  # returns 'Name1 <address1@host.com>, "Name2 ☺" <address2@host.com>, undisclosed-recipients:;'

  $list1->as_mime_string();
  # returns 'Name1 <address1@host.com>, =?UTF-8?B?TmFtZTIg4pi6?= <address2@host.com>, undisclosed-recipients:;'

  my $list2 = Email::MIME::Header::AddressList->new_groups(Group => [ $address1, $address2 ]);

  $list2->append_addresses($address2);

  $list2->addresses();
  # returns ($address2, $address1, $address2)

  $list2->groups();
  # returns (undef, [ $address2 ], 'Group', [ $address1, $address2 ])

  my $list3 = Email::MIME::Header::AddressList->new_mime_groups('=?UTF-8?B?4pi6?=' => [ $mime_address ]);
  $list3->as_string();
  # returns '☺: "Name2 ☺" <address2@host.com>;'

  my $list4 = Email::MIME::Header::AddressList->from_string('Name1 <address1@host.com>, "Name2 ☺" <address2@host.com>, undisclosed-recipients:;');
  my $list5 = Email::MIME::Header::AddressList->from_string('Name1 <address1@host.com>', '"Name2 ☺" <address2@host.com>', 'undisclosed-recipients:;');

  my $list6 = Email::MIME::Header::AddressList->from_mime_string('Name1 <address1@host.com>, =?UTF-8?B?TmFtZTIg4pi6?= <address2@host.com>, undisclosed-recipients:;');
  my $list7 = Email::MIME::Header::AddressList->from_mime_string('Name1 <address1@host.com>', '=?UTF-8?B?TmFtZTIg4pi6?= <address2@host.com>', 'undisclosed-recipients:;');

=head1 DESCRIPTION

This module implements object representation for the list of the
L<Email::Address::XS|Email::Address::XS> objects. It provides methods for
L<RFC 2047|https://tools.ietf.org/html/rfc2047> MIME encoding and decoding
suitable for L<RFC 2822|https://tools.ietf.org/html/rfc2822> address-list
structure.

=head2 EXPORT

None

=head2 Class Methods

=over 4

=item new_empty

Construct new empty C<Email::MIME::Header::AddressList> object.

=item new

Construct new C<Email::MIME::Header::AddressList> object from list of
L<Email::Address::XS|Email::Address::XS> objects.

=item new_groups

Construct new C<Email::MIME::Header::AddressList> object from named groups of
L<Email::Address::XS|Email::Address::XS> objects.

=item new_mime_groups

Like L<C<new_groups>|/new_groups> but in this method group names and objects properties are
expected to be already MIME encoded, thus ASCII strings.

=item from_string

Construct new C<Email::MIME::Header::AddressList> object from input string arguments.
Calls L<Email::Address::XS::parse_email_groups|Email::Address::XS/parse_email_groups>.

=item from_mime_string

Like L<C<from_string>|/from_string> but input string arguments are expected to
be already MIME encoded.

=back

=head2 Object Methods

=over 4

=item as_string

Returns string representation of C<Email::MIME::Header::AddressList> object.
Calls L<Email::Address::XS::format_email_groups|Email::Address::XS/format_email_groups>.

=item as_mime_string

Like L<C<as_string>|/as_string> but output string will be properly and
unambiguously MIME encoded. MIME encoding is done before calling
L<Email::Address::XS::format_email_groups|Email::Address::XS/format_email_groups>.

=item first_address

Returns first L<Email::Address::XS|Email::Address::XS> object.

=item addresses

Returns list of all L<Email::Address::XS|Email::Address::XS> objects.

=item groups

Like L<C<addresses>|/addresses> but returns objects with named groups.

=item append_addresses

Append L<Email::Address::XS|Email::Address::XS> objects.

=item append_groups

Like L<C<append_addresses>|/append_addresses> but arguments are pairs of name of
group and array reference of L<Email::Address::XS|Email::Address::XS> objects.

=back

=head1 NAME

Email::MIME::Header::AddressList - MIME support for list of Email::Address::XS objects

=head1 SEE ALSO

L<RFC 2047|https://tools.ietf.org/html/rfc2047>,
L<RFC 2822|https://tools.ietf.org/html/rfc2822>,
L<Email::MIME>,
L<Email::Address::XS>

=head1 AUTHOR

Pali E<lt>pali@cpan.orgE<gt>

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
