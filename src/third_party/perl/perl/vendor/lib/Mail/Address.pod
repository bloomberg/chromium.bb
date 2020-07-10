=encoding utf8

=head1 NAME

Mail::Address - parse mail addresses

=head1 SYNOPSIS

 use Mail::Address;
 my @addrs = Mail::Address->parse($line);

 foreach $addr (@addrs) {
     print $addr->format,"\n";
 }

=head1 DESCRIPTION

C<Mail::Address> extracts and manipulates email addresses from a message
header.  It cannot be used to extract addresses from some random text.
You can use this module to create RFC822 compliant fields.

Although C<Mail::Address> is a very popular subject for books, and is
used in many applications, it does a very poor job on the more complex
message fields.  It does only handle simple address formats (which
covers about 95% of what can be found). Problems are with

=over 4

=item *

no support for address groups, even not with the semi-colon as
separator between addresses;

=item *

limited support for escapes in phrases and comments.  There are
cases where it can get wrong; and

=item *

you have to take care of most escaping when you create an address yourself:
C<Mail::Address> does not do that for you.

=back

Often requests are made to the maintainers of this code improve this
situation, but this is not a good idea, where it will break zillions
of existing applications.  If you wish for a fully RFC2822 compliant
implementation you may take a look at L<Mail::Message::Field::Full>,
part of MailBox.

B<. Example>

  my $s = Mail::Message::Field::Full->new($from_header);
  # ref $s isa Mail::Message::Field::Addresses;

  my @g = $s->groups;          # all groups, at least one
  # ref $g[0] isa Mail::Message::Field::AddrGroup;
  my $ga = $g[0]->addresses;   # group addresses

  my @a = $s->addresses;       # all addresses
  # ref $a[0] isa Mail::Message::Field::Address;

=head1 METHODS

=head2 Constructors

=over 4

=item Mail::Address-E<gt>B<new>( $phrase, $address, [ $comment ] )

Create a new C<Mail::Address> object which represents an address with the
elements given. In a message these 3 elements would be seen like:

 PHRASE <ADDRESS> (COMMENT)
 ADDRESS (COMMENT)

example: 

 Mail::Address->new("Perl5 Porters", "perl5-porters@africa.nicoh.com");

=item $obj-E<gt>B<parse>($line)

Parse the given line a return a list of extracted C<Mail::Address> objects.
The line would normally be one taken from a To,Cc or Bcc line in a message

example: 

 my @addr = Mail::Address->parse($line);

=back

=head2 Accessors

=over 4

=item $obj-E<gt>B<address>()

Return the address part of the object.

=item $obj-E<gt>B<comment>()

Return the comment part of the object

=item $obj-E<gt>B<format>(@addresses)

Return a string representing the address in a suitable form to be placed
on a C<To>, C<Cc>, or C<Bcc> line of a message.  This method is called on
the first address to be used; other specified addresses will be appended,
separated by commas.

=item $obj-E<gt>B<phrase>()

Return the phrase part of the object.

=back

=head2 Smart accessors

=over 4

=item $obj-E<gt>B<host>()

Return the address excluding the user id and '@'

=item $obj-E<gt>B<name>()

Using the information contained within the object attempt to identify what
the person or groups name is.

B<Note:> This function tries to be smart with the "phrase" of the
email address, which is probably a very bad idea.  Consider to use
L<phrase()|Mail::Address/"Accessors"> itself.

=item $obj-E<gt>B<user>()

Return the address excluding the '@' and the mail domain

=back

=head1 SEE ALSO

This module is part of the MailTools distribution,
F<http://perl.overmeer.net/mailtools/>.

=head1 AUTHORS

The MailTools bundle was developed by Graham Barr.  Later, Mark
Overmeer took over maintenance without commitment to further development.

Mail::Cap by Gisle Aas E<lt>aas@oslonett.noE<gt>.
Mail::Field::AddrList by Peter Orbaek E<lt>poe@cit.dkE<gt>.
Mail::Mailer and Mail::Send by Tim Bunce E<lt>Tim.Bunce@ig.co.ukE<gt>.
For other contributors see ChangeLog.

=head1 LICENSE

Copyrights 1995-2000 Graham Barr E<lt>gbarr@pobox.comE<gt> and
2001-2017 Mark Overmeer E<lt>perl@overmeer.netE<gt>.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.
See F<http://www.perl.com/perl/misc/Artistic.html>

