# Copyright (c) 2015-2018 by Pali <pali@cpan.org>

package Email::Address::XS;

use 5.006;
use strict;
use warnings;

our $VERSION = '1.04';

use Carp;

use base 'Exporter';
our @EXPORT_OK = qw(parse_email_addresses parse_email_groups format_email_addresses format_email_groups compose_address split_address);

use XSLoader;
XSLoader::load(__PACKAGE__, $VERSION);

=head1 NAME

Email::Address::XS - Parse and format RFC 5322 email addresses and groups

=head1 SYNOPSIS

  use Email::Address::XS;

  my $winstons_address = Email::Address::XS->new(phrase => 'Winston Smith', user => 'winston.smith', host => 'recdep.minitrue', comment => 'Records Department');
  print $winstons_address->address();
  # winston.smith@recdep.minitrue

  my $julias_address = Email::Address::XS->new('Julia', 'julia@ficdep.minitrue');
  print $julias_address->format();
  # Julia <julia@ficdep.minitrue>

  my $users_address = Email::Address::XS->parse('user <user@oceania>');
  print $users_address->host();
  # oceania

  my $goldsteins_address = Email::Address::XS->parse_bare_address('goldstein@brotherhood.oceania');
  print $goldsteins_address->user();
  # goldstein

  my @addresses = Email::Address::XS->parse('"Winston Smith" <winston.smith@recdep.minitrue> (Records Department), Julia <julia@ficdep.minitrue>');
  # ($winstons_address, $julias_address)


  use Email::Address::XS qw(format_email_addresses format_email_groups parse_email_addresses parse_email_groups);

  my $addresses_string = format_email_addresses($winstons_address, $julias_address, $users_address);
  # "Winston Smith" <winston.smith@recdep.minitrue> (Records Department), Julia <julia@ficdep.minitrue>, user <user@oceania>

  my @addresses = map { $_->address() } parse_email_addresses($addresses_string);
  # ('winston.smith@recdep.minitrue', 'julia@ficdep.minitrue', 'user@oceania')

  my $groups_string = format_email_groups('Brotherhood' => [ $winstons_address, $julias_address ], undef() => [ $users_address ]);
  # Brotherhood: "Winston Smith" <winston.smith@recdep.minitrue> (Records Department), Julia <julia@ficdep.minitrue>;, user <user@oceania>

  my @groups = parse_email_groups($groups_string);
  # ('Brotherhood' => [ $winstons_address, $julias_address ], undef() => [ $users_address ])


  use Email::Address::XS qw(compose_address split_address);

  my ($user, $host) = split_address('julia(outer party)@ficdep.minitrue');
  # ('julia', 'ficdep.minitrue')

  my $string = compose_address('charrington"@"shop', 'thought.police.oceania');
  # "charrington\"@\"shop"@thought.police.oceania

=head1 DESCRIPTION

This module implements L<RFC 5322|https://tools.ietf.org/html/rfc5322>
parser and formatter of email addresses and groups. It parses an input
string from email headers which contain a list of email addresses or
a groups of email addresses (like From, To, Cc, Bcc, Reply-To, Sender,
...). Also it can generate a string value for those headers from a
list of email addresses objects. Module is backward compatible with
L<RFC 2822|https://tools.ietf.org/html/rfc2822> and
L<RFC 822|https://tools.ietf.org/html/rfc822>.

Parser and formatter functionality is implemented in XS and uses
shared code from Dovecot IMAP server.

It is a drop-in replacement for L<the Email::Address module|Email::Address>
which has several security issues. E.g. issue L<CVE-2015-7686 (Algorithmic complexity vulnerability)|https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2015-7686>,
which allows remote attackers to cause denial of service, is still
present in L<Email::Address|Email::Address> version 1.908.

Email::Address::XS module was created to finally fix CVE-2015-7686.

Existing applications that use Email::Address module could be easily
switched to Email::Address::XS module. In most cases only changing
C<use Email::Address> to C<use Email::Address::XS> and replacing every
C<Email::Address> occurrence with C<Email::Address::XS> is sufficient.

So unlike L<Email::Address|Email::Address>, this module does not use
regular expressions for parsing but instead native XS implementation
parses input string sequentially according to RFC 5322 grammar.

Additionally it has support also for named groups and so can be use
instead of L<the Email::Address::List module|Email::Address::List>.

If you are looking for the module which provides object representation
for the list of email addresses suitable for the MIME email headers,
see L<Email::MIME::Header::AddressList|Email::MIME::Header::AddressList>.

=head2 EXPORT

None by default. Exportable functions are:
L<C<parse_email_addresses>|/parse_email_addresses>,
L<C<parse_email_groups>|/parse_email_groups>,
L<C<format_email_addresses>|/format_email_addresses>,
L<C<format_email_groups>|/format_email_groups>,
L<C<compose_address>|/compose_address>,
L<C<split_address>|/split_address>.

=head2 Exportable Functions

=over 4

=item format_email_addresses

  use Email::Address::XS qw(format_email_addresses);

  my $winstons_address = Email::Address::XS->new(phrase => 'Winston Smith', address => 'winston@recdep.minitrue');
  my $julias_address = Email::Address::XS->new(phrase => 'Julia', address => 'julia@ficdep.minitrue');
  my @addresses = ($winstons_address, $julias_address);
  my $string = format_email_addresses(@addresses);
  print $string;
  # "Winston Smith" <winston@recdep.minitrue>, Julia <julia@ficdep.minitrue>

Takes a list of email address objects and returns one formatted string
of those email addresses.

=cut

sub format_email_addresses {
	my (@args) = @_;
	return format_email_groups(undef, \@args);
}

=item format_email_groups

  use Email::Address::XS qw(format_email_groups);

  my $winstons_address = Email::Address::XS->new(phrase => 'Winston Smith', user => 'winston.smith', host => 'recdep.minitrue');
  my $julias_address = Email::Address::XS->new('Julia', 'julia@ficdep.minitrue');
  my $users_address = Email::Address::XS->new(address => 'user@oceania');

  my $groups_string = format_email_groups('Brotherhood' => [ $winstons_address, $julias_address ], undef() => [ $users_address ]);
  print $groups_string;
  # Brotherhood: "Winston Smith" <winston.smith@recdep.minitrue>, Julia <julia@ficdep.minitrue>;, user@oceania

  my $undisclosed_string = format_email_groups('undisclosed-recipients' => []);
  print $undisclosed_string;
  # undisclosed-recipients:;

Like L<C<format_email_addresses>|/format_email_addresses> but this
method takes pairs which consist of a group display name and a
reference to address list. If a group is not undef then address
list is formatted inside named group.

=item parse_email_addresses

  use Email::Address::XS qw(parse_email_addresses);

  my $string = '"Winston Smith" <winston.smith@recdep.minitrue>, Julia <julia@ficdep.minitrue>, user@oceania';
  my @addresses = parse_email_addresses($string);
  # @addresses now contains three Email::Address::XS objects, one for each address

Parses an input string and returns a list of Email::Address::XS
objects. Optional second string argument specifies class name for
blessing new objects.

=cut

sub parse_email_addresses {
	my (@args) = @_;
	my $t = 1;
	return map { @{$_} } grep { $t ^= 1 } parse_email_groups(@args);
}

=item parse_email_groups

  use Email::Address::XS qw(parse_email_groups);

  my $string = 'Brotherhood: "Winston Smith" <winston.smith@recdep.minitrue>, Julia <julia@ficdep.minitrue>;, user@oceania, undisclosed-recipients:;';
  my @groups = parse_email_groups($string);
  # @groups now contains list ('Brotherhood' => [ $winstons_object, $julias_object ], undef() => [ $users_object ], 'undisclosed-recipients' => [])

Like L<C<parse_email_addresses>|/parse_email_addresses> but this
function returns a list of pairs: a group display name and a
reference to a list of addresses which belongs to that named group.
An undef value for a group means that a following list of addresses
is not inside any named group. An output is in a same format as a
input for the function L<C<format_email_groups>|/format_email_groups>.
This function preserves order of groups and does not do any
de-duplication or merging.

=item compose_address

  use Email::Address::XS qw(compose_address);
  my $string_address = compose_address($user, $host);

Takes an unescaped user part and unescaped host part of an address
and returns escaped address.

Available since version 1.01.

=item split_address

  use Email::Address::XS qw(split_address);
  my ($user, $host) = split_address($string_address);

Takes an escaped address and split it into pair of unescaped user
part and unescaped host part of address. If splitting input address
into these two parts is not possible then this function returns
pair of undefs.

Available since version 1.01.

=back

=head2 Class Methods

=over 4

=item new

  my $empty_address = Email::Address::XS->new();
  my $winstons_address = Email::Address::XS->new(phrase => 'Winston Smith', user => 'winston.smith', host => 'recdep.minitrue', comment => 'Records Department');
  my $julias_address = Email::Address::XS->new('Julia', 'julia@ficdep.minitrue');
  my $users_address = Email::Address::XS->new(address => 'user@oceania');
  my $only_name = Email::Address::XS->new(phrase => 'Name');
  my $copy_of_winstons_address = Email::Address::XS->new(copy => $winstons_address);

Constructs and returns a new C<Email::Address::XS> object. Takes named
list of arguments: phrase, address, user, host, comment and copy.
An argument address takes precedence over user and host.

When an argument copy is specified then it is expected an
Email::Address::XS object and a cloned copy of that object is
returned. All other parameters are ignored.

Old syntax L<from the Email::Address module|Email::Address/new> is
supported too. Takes one to four positional arguments: phrase, address
comment, and original string. Passing an argument original is
deprecated, ignored and throws a warning.

=cut

sub new {
	my ($class, @args) = @_;

	my %hash_keys = (phrase => 1, address => 1, user => 1, host => 1, comment => 1, copy => 1);
	my $is_hash;
	if ( scalar @args == 2 and defined $args[0] ) {
		$is_hash = 1 if exists $hash_keys{$args[0]};
	} elsif ( scalar @args == 4 and defined $args[0] and defined $args[2] ) {
		$is_hash = 1 if exists $hash_keys{$args[0]} and exists $hash_keys{$args[2]};
	} elsif ( scalar @args > 4 ) {
		$is_hash = 1;
	}

	my %args;
	if ( $is_hash ) {
		%args = @args;
	} else {
		carp 'Argument original is deprecated and ignored' if scalar @args > 3;
		$args{comment} = $args[2] if scalar @args > 2;
		$args{address} = $args[1] if scalar @args > 1;
		$args{phrase} = $args[0] if scalar @args > 0;
	}

	my $invalid;
	my $original;
	if ( exists $args{copy} ) {
		if ( $class->is_obj($args{copy}) ) {
			$args{phrase} = $args{copy}->phrase();
			$args{comment} = $args{copy}->comment();
			$args{user} = $args{copy}->user();
			$args{host} = $args{copy}->host();
			$invalid = $args{copy}->{invalid};
			$original = $args{copy}->{original};
			delete $args{address};
		} else {
			carp 'Named argument copy does not contain a valid object';
		}
	}

	my $self = bless {}, $class;

	$self->phrase($args{phrase});
	$self->comment($args{comment});

	if ( exists $args{address} ) {
		$self->address($args{address});
	} else {
		$self->user($args{user});
		$self->host($args{host});
	}

	$self->{invalid} = 1 if $invalid;
	$self->{original} = $original;

	return $self;
}

=item parse

  my $winstons_address = Email::Address::XS->parse('"Winston Smith" <winston.smith@recdep.minitrue> (Records Department)');
  my @users_addresses = Email::Address::XS->parse('user1@oceania, user2@oceania');

Parses an input string and returns a list of an Email::Address::XS
objects. Same as the function L<C<parse_email_addresses>|/parse_email_addresses>
but this one is class method.

In scalar context this function returns just first parsed object.
If more then one object was parsed then L<C<is_valid>|/is_valid>
method on returned object returns false. If no object was parsed
then empty Email::Address::XS object is returned.

Prior to version 1.01 return value in scalar context is undef when
no object was parsed.

=cut

sub parse {
	my ($class, $string) = @_;
	my @addresses = parse_email_addresses($string, $class);
	return @addresses if wantarray;
	my $self = @addresses ? $addresses[0] : Email::Address::XS->new();
	$self->{invalid} = 1 if scalar @addresses != 1;
	$self->{original} = $string unless defined $self->{original};
	return $self;
}

=item parse_bare_address

  my $winstons_address = Email::Address::XS->parse_bare_address('winston.smith@recdep.minitrue');

Parses an input string as one bare email address (addr spec) which
does not allow phrase part or angle brackets around email address and
returns an Email::Address::XS object. It is just a wrapper around
L<C<address>|/address> method. Method L<C<is_valid>|/is_valid> can be
used to check if parsing was successful.

Available since version 1.01.

=cut

sub parse_bare_address {
	my ($class, $string) = @_;
	my $self = $class->new();
	if ( defined $string ) {
		$self->address($string);
		$self->{original} = $string;
	} else {
		carp 'Use of uninitialized value for string';
	}
	return $self;
}

=back

=head2 Object Methods

=over 4

=item format

  my $string = $address->format();

Returns formatted Email::Address::XS object as a string. This method
throws a warning when L<C<user>|/user> or L<C<host>|/host> part of
the email address is invalid or empty string.

=cut

sub format {
	my ($self) = @_;
	return format_email_addresses($self);
}

=item is_valid

  my $is_valid = $address->is_valid();

Returns true if the parse function or method which created this
Email::Address::XS object had not received any syntax error on input
string and also that L<C<user>|/user> and L<C<host>|/host> part of
the email address are not empty strings.

Thus this function can be used for checking if Email::Address::XS
object is valid before calling L<C<format>|/format> method on it.

Available since version 1.01.

=cut

sub is_valid {
	my ($self) = @_;
	my $user = $self->user();
	my $host = $self->host();
	return (defined $user and defined $host and length $host and not $self->{invalid});
}

=item phrase

  my $phrase = $address->phrase();
  $address->phrase('Winston Smith');

Accessor and mutator for the phrase (display name).

=cut

sub phrase {
	my ($self, @args) = @_;
	return $self->{phrase} unless @args;
	delete $self->{invalid} if exists $self->{invalid};
	return $self->{phrase} = $args[0];
}

=item user

  my $user = $address->user();
  $address->user('winston.smith');

Accessor and mutator for the unescaped user (local/mailbox) part of
an address.

=cut

sub user {
	my ($self, @args) = @_;
	return $self->{user} unless @args;
	delete $self->{cached_address} if exists $self->{cached_address};
	delete $self->{invalid} if exists $self->{invalid};
	return $self->{user} = $args[0];
}

=item host

  my $host = $address->host();
  $address->host('recdep.minitrue');

Accessor and mutator for the unescaped host (domain) part of an address.

Since version 1.03 this method checks if setting a new value is syntactically
valid. If not undef is set and returned.

=cut

sub host {
	my ($self, @args) = @_;
	return $self->{host} unless @args;
	delete $self->{cached_address} if exists $self->{cached_address};
	delete $self->{invalid} if exists $self->{invalid};
	if (defined $args[0] and $args[0] =~ /^(?:\[.*\]|[^\x00-\x20\x7F()<>\[\]:;@\\,"]+)$/) {
		return $self->{host} = $args[0];
	} else {
		return $self->{host} = undef;
	}
}

=item address

  my $string_address = $address->address();
  $address->address('winston.smith@recdep.minitrue');

Accessor and mutator for the escaped address (addr spec).

Internally this module stores a user and a host part of an address
separately. Function L<C<compose_address>|/compose_address> is used
for composing full address and function L<C<split_address>|/split_address>
for splitting into a user and a host parts. If splitting new address
into these two parts is not possible then this method returns undef
and sets both parts to undef.

=cut

sub address {
	my ($self, @args) = @_;
	my $user;
	my $host;
	if ( @args ) {
		delete $self->{invalid} if exists $self->{invalid};
		($user, $host) = split_address($args[0]) if defined $args[0];
		if ( not defined $user or not defined $host ) {
			$user = undef;
			$host = undef;
		}
		$self->{user} = $user;
		$self->{host} = $host;
	} else {
		return $self->{cached_address} if exists $self->{cached_address};
		$user = $self->user();
		$host = $self->host();
	}
	if ( defined $user and defined $host and length $host ) {
		return $self->{cached_address} = compose_address($user, $host);
	} else {
		return $self->{cached_address} = undef;
	}
}

=item comment

  my $comment = $address->comment();
  $address->comment('Records Department');

Accessor and mutator for the comment which is formatted after an
address. A comment can contain another nested comments in round
brackets. When setting new comment this method check if brackets are
balanced. If not undef is set and returned.

=cut

sub comment {
	my ($self, @args) = @_;
	return $self->{comment} unless @args;
	delete $self->{invalid} if exists $self->{invalid};
	return $self->{comment} = undef unless defined $args[0];
	my $count = 0;
	my $cleaned = $args[0];
	$cleaned =~ s/(?:\\.|[^\(\)\x00])//g;
	foreach ( split //, $cleaned ) {
		$count++ if $_ eq '(';
		$count-- if $_ eq ')';
		$count = -1 if $_ eq "\x00";
		last if $count < 0;
	}
	return $self->{comment} = undef if $count != 0;
	return $self->{comment} = $args[0];
}

=item name

  my $name = $address->name();

This method tries to return a name which belongs to the address. It
returns either L<C<phrase>|/phrase> or L<C<comment>|/comment> or
L<C<user>|/user> part of the address or empty string (first defined
value in this order). But it never returns undef.

=cut

sub name {
	my ($self) = @_;
	my $phrase = $self->phrase();
	return $phrase if defined $phrase and length $phrase;
	my $comment = $self->comment();
	return $comment if defined $comment and length $comment;
	my $user = $self->user();
	return $user if defined $user;
	return '';
}

=item as_string

  my $address = Email::Address::XS->new(phrase => 'Winston Smith', address => 'winston.smith@recdep.minitrue');
  my $stringified = $address->as_string();

This method is used for object L<stringification|/stringify>. It
returns string representation of object. By default object is
stringified to L<C<format>|/format>.

Available since version 1.01.

=cut

our $STRINGIFY; # deprecated

sub as_string {
	my ($self) = @_;
	return $self->format() unless defined $STRINGIFY;
	carp 'Variable $Email::Address::XS::STRINGIFY is deprecated; subclass instead';
	my $method = $self->can($STRINGIFY);
	croak 'Stringify method ' . $STRINGIFY . ' does not exist' unless defined $method;
	return $method->($self);
}

=item original

  my $address = Email::Address::XS->parse('(Winston) "Smith"   <winston.smith@recdep.minitrue> (Minitrue)');
  my $original = $address->original();
  # (Winston) "Smith"   <winston.smith@recdep.minitrue> (Minitrue)
  my $format = $address->format();
  # Smith <winston.smith@recdep.minitrue> (Minitrue)

This method returns original part of the string which was used for
parsing current Email::Address::XS object. If object was not created
by parsing input string, then this method returns undef.

Note that L<C<format>|/format> method does not have to return same
original string.

Available since version 1.01.

=cut

sub original {
	my ($self) = @_;
	return $self->{original};
}

=back

=head2 Overloaded Operators

=over 4

=item stringify

  my $address = Email::Address::XS->new(phrase => 'Winston Smith', address => 'winston.smith@recdep.minitrue');
  print "Winston's address is $address.";
  # Winston's address is "Winston Smith" <winston.smith@recdep.minitrue>.

Stringification is done by method L<C<as_string>|/as_string>.

=cut

use overload '""' => \&as_string;

=back

=head2 Deprecated Functions and Variables

For compatibility with L<the Email::Address module|Email::Address>
there are defined some deprecated functions and variables.
Do not use them in new code. Their usage throws warnings.

Altering deprecated variable C<$Email::Address::XS::STRINGIFY> changes
method which is called for objects stringification.

Deprecated cache functions C<purge_cache>, C<disable_cache> and
C<enable_cache> are noop and do nothing.

=cut

sub purge_cache {
	carp 'Function purge_cache is deprecated and does nothing';
}

sub disable_cache {
	carp 'Function disable_cache is deprecated and does nothing';
}

sub enable_cache {
	carp 'Function enable_cache is deprecated and does nothing';
}

=head1 SEE ALSO

L<RFC 822|https://tools.ietf.org/html/rfc822>,
L<RFC 2822|https://tools.ietf.org/html/rfc2822>,
L<RFC 5322|https://tools.ietf.org/html/rfc5322>,
L<Email::MIME::Header::AddressList>,
L<Email::Address>,
L<Email::Address::List>,
L<Email::AddressParser>

=head1 AUTHOR

Pali E<lt>pali@cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2015-2018 by Pali E<lt>pali@cpan.orgE<gt>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.6.0 or,
at your option, any later version of Perl 5 you may have available.

Dovecot parser is licensed under The MIT License and copyrighted by
Dovecot authors.

=cut

1;
