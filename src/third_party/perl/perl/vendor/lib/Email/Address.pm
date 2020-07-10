use strict;
use warnings;
package Email::Address;
# ABSTRACT: RFC 2822 Address Parsing and Creation
$Email::Address::VERSION = '1.912';
our $COMMENT_NEST_LEVEL ||= 1;
our $STRINGIFY          ||= 'format';
our $COLLAPSE_SPACES      = 1 unless defined $COLLAPSE_SPACES; # I miss //=

#pod =head1 SYNOPSIS
#pod
#pod   use Email::Address;
#pod
#pod   my @addresses = Email::Address->parse($line);
#pod   my $address   = Email::Address->new(Casey => 'casey@localhost');
#pod
#pod   print $address->format;
#pod
#pod =head1 DESCRIPTION
#pod
#pod This class implements a regex-based RFC 2822 parser that locates email
#pod addresses in strings and returns a list of C<Email::Address> objects found.
#pod Alternatively you may construct objects manually. The goal of this software is
#pod to be correct, and very very fast.
#pod
#pod Version 1.909 and earlier of this module had vulnerabilies
#pod (L<CVE-2015-7686|https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2015-7686>)
#pod and (L<CVE-2015-12558|https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2018-12558>)
#pod which allowed specially constructed email to cause a denial of service. The
#pod reported vulnerabilities and some other pathalogical cases (meaning they really
#pod shouldn't occur in normal email) have been addressed in version 1.910 and newer.
#pod If you're running version 1.909 or older, you should update!
#pod
#pod Alternatively, you could switch to L<B<Email::Address::XS>|Email::Address::XS>
#pod which has a backward compatible API.
#pod
#pod =cut

my $CTL            = q{\x00-\x1F\x7F};
my $special        = q{()<>\\[\\]:;@\\\\,."};

my $text           = qr/[^\x0A\x0D]/;

my $quoted_pair    = qr/\\$text/;

my $ctext          = qr/(?>[^()\\]+)/;
my ($ccontent, $comment) = (q{})x2;
for (1 .. $COMMENT_NEST_LEVEL) {
  $ccontent = qr/$ctext|$quoted_pair|$comment/;
  $comment  = qr/(?>\s*\((?:\s*$ccontent)*\s*\)\s*)/;
}
my $cfws           = qr/$comment|(?>\s+)/;

my $atext          = qq/[^$CTL$special\\s]/;
my $atom           = qr/(?>$cfws*$atext+$cfws*)/;
my $dot_atom_text  = qr/(?>$atext+(?:\.$atext+)*)/;
my $dot_atom       = qr/(?>$cfws*$dot_atom_text$cfws*)/;

my $qtext          = qr/[^\\"]/;
my $qcontent       = qr/$qtext|$quoted_pair/;
my $quoted_string  = qr/(?>$cfws*"$qcontent*"$cfws*)/;

my $word           = qr/$atom|$quoted_string/;

# XXX: This ($phrase) used to just be: my $phrase = qr/$word+/; It was changed
# to resolve bug 22991, creating a significant slowdown.  Given current speed
# problems.  Once 16320 is resolved, this section should be dealt with.
# -- rjbs, 2006-11-11
#my $obs_phrase     = qr/$word(?:$word|\.|$cfws)*/;

# XXX: ...and the above solution caused endless problems (never returned) when
# examining this address, now in a test:
#   admin+=E6=96=B0=E5=8A=A0=E5=9D=A1_Weblog-- ATAT --test.socialtext.com
# So we disallow the hateful CFWS in this context for now.  Of modern mail
# agents, only Apple Web Mail 2.0 is known to produce obs-phrase.
# -- rjbs, 2006-11-19
my $simple_word    = qr/(?>$atom|\.|\s*"$qcontent+"\s*)/;
my $obs_phrase     = qr/(?>$simple_word+)/;

my $phrase         = qr/$obs_phrase|(?>$word+)/;

my $local_part     = qr/$dot_atom|$quoted_string/;
my $dtext          = qr/[^\[\]\\]/;
my $dcontent       = qr/$dtext|$quoted_pair/;
my $domain_literal = qr/(?>$cfws*\[(?:\s*$dcontent)*\s*\]$cfws*)/;
my $domain         = qr/$dot_atom|$domain_literal/;

my $display_name   = $phrase;

#pod =head2 Package Variables
#pod
#pod B<ACHTUNG!>  Email isn't easy (if even possible) to parse with a regex, I<at
#pod least> if you're on a C<perl> prior to 5.10.0.  Providing regular expressions
#pod for use by other programs isn't a great idea, because it makes it hard to
#pod improve the parser without breaking the "it's a regex" feature.  Using these
#pod regular expressions is not encouraged, and methods like C<<
#pod Email::Address->is_addr_spec >> should be provided in the future.
#pod
#pod Several regular expressions used in this package are useful to others.
#pod For convenience, these variables are declared as package variables that
#pod you may access from your program.
#pod
#pod These regular expressions conform to the rules specified in RFC 2822.
#pod
#pod You can access these variables using the full namespace. If you want
#pod short names, define them yourself.
#pod
#pod   my $addr_spec = $Email::Address::addr_spec;
#pod
#pod =over 4
#pod
#pod =item $Email::Address::addr_spec
#pod
#pod This regular expression defined what an email address is allowed to
#pod look like.
#pod
#pod =item $Email::Address::angle_addr
#pod
#pod This regular expression defines an C<$addr_spec> wrapped in angle
#pod brackets.
#pod
#pod =item $Email::Address::name_addr
#pod
#pod This regular expression defines what an email address can look like
#pod with an optional preceding display name, also known as the C<phrase>.
#pod
#pod =item $Email::Address::mailbox
#pod
#pod This is the complete regular expression defining an RFC 2822 email
#pod address with an optional preceding display name and optional
#pod following comment.
#pod
#pod =back
#pod
#pod =cut

our $addr_spec  = qr/$local_part\@$domain/;
our $angle_addr = qr/(?>$cfws*<$addr_spec>$cfws*)/;
our $name_addr  = qr/(?>$display_name?)$angle_addr/;
our $mailbox    = qr/(?:$name_addr|$addr_spec)(?>$comment*)/;

sub _PHRASE   () { 0 }
sub _ADDRESS  () { 1 }
sub _COMMENT  () { 2 }
sub _ORIGINAL () { 3 }
sub _IN_CACHE () { 4 }

sub __dump {
  return {
    phrase   => $_[0][_PHRASE],
    address  => $_[0][_ADDRESS],
    comment  => $_[0][_COMMENT],
    original => $_[0][_ORIGINAL],
  }
}

#pod =head2 Class Methods
#pod
#pod =over
#pod
#pod =item parse
#pod
#pod   my @addrs = Email::Address->parse(
#pod     q[me@local, Casey <me@local>, "Casey" <me@local> (West)]
#pod   );
#pod
#pod This method returns a list of C<Email::Address> objects it finds in the input
#pod string.  B<Please note> that it returns a list, and expects that it may find
#pod multiple addresses.  The behavior in scalar context is undefined.
#pod
#pod The specification for an email address allows for infinitely nestable comments.
#pod That's nice in theory, but a little over done.  By default this module allows
#pod for one (C<1>) level of nested comments. If you think you need more, modify the
#pod C<$Email::Address::COMMENT_NEST_LEVEL> package variable to allow more.
#pod
#pod   $Email::Address::COMMENT_NEST_LEVEL = 10; # I'm deep
#pod
#pod The reason for this hardly-limiting limitation is simple: efficiency.
#pod
#pod Long strings of whitespace can be problematic for this module to parse, a bug
#pod which has not yet been adequately addressed.  The default behavior is now to
#pod collapse multiple spaces into a single space, which avoids this problem.  To
#pod prevent this behavior, set C<$Email::Address::COLLAPSE_SPACES> to zero.  This
#pod variable will go away when the bug is resolved properly.
#pod
#pod In accordance with RFC 822 and its descendants, this module demands that email
#pod addresses be ASCII only.  Any non-ASCII content in the parsed addresses will
#pod cause the parser to return no results.
#pod
#pod =cut

our (%PARSE_CACHE, %FORMAT_CACHE, %NAME_CACHE);
my $NOCACHE;

sub __get_cached_parse {
    return if $NOCACHE;

    my ($class, $line) = @_;

    return @{$PARSE_CACHE{$line}} if exists $PARSE_CACHE{$line};
    return;
}

sub __cache_parse {
    return if $NOCACHE;

    my ($class, $line, $addrs) = @_;

    $PARSE_CACHE{$line} = $addrs;
}

sub parse {
    my ($class, $line) = @_;
    return unless $line;

    $line =~ s/[ \t]+/ /g if $COLLAPSE_SPACES;

    if (my @cached = $class->__get_cached_parse($line)) {
        return @cached;
    }

    my %mailboxes;
    my $str = $line;
    $str =~ s!($name_addr(?>$comment*))!$mailboxes{pos($str)} = $1; ',' x length $1!ego
        if $str =~ /$angle_addr/;
    $str =~ s!($addr_spec(?>$comment*))!$mailboxes{pos($str)} = $1; ',' x length $1!ego;
    my @mailboxes = map { $mailboxes{$_} } sort { $a <=> $b } keys %mailboxes;

    my @addrs;
    foreach (@mailboxes) {
      my $original = $_;

      my @comments = /($comment)/go;
      s/$comment//go if @comments;

      my ($user, $host, $com);
      ($user, $host) = ($1, $2) if s/<($local_part)\@($domain)>\s*\z//o;
      if (! defined($user) || ! defined($host)) {
          s/($local_part)\@($domain)//o;
          ($user, $host) = ($1, $2);
      }

      next if $user =~ /\P{ASCII}/;
      next if $host =~ /\P{ASCII}/;

      my ($phrase)       = /($display_name)/o;

      for ( $phrase, $host, $user, @comments ) {
        next unless defined $_;
        s/^\s+//;
        s/\s+$//;
        $_ = undef unless length $_;
      }

      $phrase =~ s/\\(.)/$1/g if $phrase;

      my $new_comment = join q{ }, @comments;
      push @addrs,
        $class->new($phrase, "$user\@$host", $new_comment, $original);
      $addrs[-1]->[_IN_CACHE] = [ \$line, $#addrs ]
    }

    $class->__cache_parse($line, \@addrs);
    return @addrs;
}

#pod =item new
#pod
#pod   my $address = Email::Address->new(undef, 'casey@local');
#pod   my $address = Email::Address->new('Casey West', 'casey@local');
#pod   my $address = Email::Address->new(undef, 'casey@local', '(Casey)');
#pod
#pod Constructs and returns a new C<Email::Address> object. Takes four
#pod positional arguments: phrase, email, and comment, and original string.
#pod
#pod The original string should only really be set using C<parse>.
#pod
#pod =cut

sub new {
  my ($class, $phrase, $email, $comment, $orig) = @_;
  $phrase =~ s/\A"(.+)"\z/$1/ if $phrase;

  bless [ $phrase, $email, $comment, $orig ] => $class;
}

#pod =item purge_cache
#pod
#pod   Email::Address->purge_cache;
#pod
#pod One way this module stays fast is with internal caches. Caches live
#pod in memory and there is the remote possibility that you will have a
#pod memory problem. On the off chance that you think you're one of those
#pod people, this class method will empty those caches.
#pod
#pod I've loaded over 12000 objects and not encountered a memory problem.
#pod
#pod =cut

sub purge_cache {
    %NAME_CACHE   = ();
    %FORMAT_CACHE = ();
    %PARSE_CACHE  = ();
}

#pod =item disable_cache
#pod
#pod =item enable_cache
#pod
#pod   Email::Address->disable_cache if memory_low();
#pod
#pod If you'd rather not cache address parses at all, you can disable (and
#pod re-enable) the Email::Address cache with these methods.  The cache is enabled
#pod by default.
#pod
#pod =cut

sub disable_cache {
  my ($class) = @_;
  $class->purge_cache;
  $NOCACHE = 1;
}

sub enable_cache {
  $NOCACHE = undef;
}

#pod =back
#pod
#pod =head2 Instance Methods
#pod
#pod =over 4
#pod
#pod =item phrase
#pod
#pod   my $phrase = $address->phrase;
#pod   $address->phrase( "Me oh my" );
#pod
#pod Accessor and mutator for the phrase portion of an address.
#pod
#pod =item address
#pod
#pod   my $addr = $address->address;
#pod   $addr->address( "me@PROTECTED.com" );
#pod
#pod Accessor and mutator for the address portion of an address.
#pod
#pod =item comment
#pod
#pod   my $comment = $address->comment;
#pod   $address->comment( "(Work address)" );
#pod
#pod Accessor and mutator for the comment portion of an address.
#pod
#pod =item original
#pod
#pod   my $orig = $address->original;
#pod
#pod Accessor for the original address found when parsing, or passed
#pod to C<new>.
#pod
#pod =item host
#pod
#pod   my $host = $address->host;
#pod
#pod Accessor for the host portion of an address's address.
#pod
#pod =item user
#pod
#pod   my $user = $address->user;
#pod
#pod Accessor for the user portion of an address's address.
#pod
#pod =cut

BEGIN {
  my %_INDEX = (
    phrase   => _PHRASE,
    address  => _ADDRESS,
    comment  => _COMMENT,
    original => _ORIGINAL,
  );

  for my $method (keys %_INDEX) {
    no strict 'refs';
    my $index = $_INDEX{ $method };
    *$method = sub {
      if ($_[1]) {
        if ($_[0][_IN_CACHE]) {
          my $replicant = bless [ @{$_[0]} ] => ref $_[0];
          $PARSE_CACHE{ ${ $_[0][_IN_CACHE][0] } }[ $_[0][_IN_CACHE][1] ]
            = $replicant;
          $_[0][_IN_CACHE] = undef;
        }
        $_[0]->[ $index ] = $_[1];
      } else {
        $_[0]->[ $index ];
      }
    };
  }
}

sub host { ($_[0]->[_ADDRESS] =~ /\@($domain)/o)[0]     }
sub user { ($_[0]->[_ADDRESS] =~ /($local_part)\@/o)[0] }

#pod =pod
#pod
#pod =item format
#pod
#pod   my $printable = $address->format;
#pod
#pod Returns a properly formatted RFC 2822 address representing the
#pod object.
#pod
#pod =cut

sub format {
    my $cache_str = do { no warnings 'uninitialized'; "@{$_[0]}" };
    return $FORMAT_CACHE{$cache_str} if exists $FORMAT_CACHE{$cache_str};
    $FORMAT_CACHE{$cache_str} = $_[0]->_format;
}

sub _format {
    my ($self) = @_;

    unless (
      defined $self->[_PHRASE] && length $self->[_PHRASE]
      ||
      defined $self->[_COMMENT] && length $self->[_COMMENT]
    ) {
        return defined $self->[_ADDRESS] ? $self->[_ADDRESS] : '';
    }

    my $comment = defined $self->[_COMMENT] ? $self->[_COMMENT] : '';
    $comment = "($comment)" if length $comment and $comment !~ /\A\(.*\)\z/;

    my $format = sprintf q{%s <%s> %s},
                 $self->_enquoted_phrase,
                 (defined $self->[_ADDRESS] ? $self->[_ADDRESS] : ''),
                 $comment;

    $format =~ s/^\s+//;
    $format =~ s/\s+$//;

    return $format;
}

sub _enquoted_phrase {
  my ($self) = @_;

  my $phrase = $self->[_PHRASE];

  return '' unless defined $phrase and length $phrase;

  # if it's encoded -- rjbs, 2007-02-28
  return $phrase if $phrase =~ /\A=\?.+\?=\z/;

  $phrase =~ s/\A"(.+)"\z/$1/;
  $phrase =~ s/([\\"])/\\$1/g;

  return qq{"$phrase"};
}

#pod =item name
#pod
#pod   my $name = $address->name;
#pod
#pod This method tries very hard to determine the name belonging to the address.
#pod First the C<phrase> is checked. If that doesn't work out the C<comment>
#pod is looked into. If that still doesn't work out, the C<user> portion of
#pod the C<address> is returned.
#pod
#pod This method does B<not> try to massage any name it identifies and instead
#pod leaves that up to someone else. Who is it to decide if someone wants their
#pod name capitalized, or if they're Irish?
#pod
#pod =cut

sub name {
    my $cache_str = do { no warnings 'uninitialized'; "@{$_[0]}" };
    return $NAME_CACHE{$cache_str} if exists $NAME_CACHE{$cache_str};

    my ($self) = @_;
    my $name = q{};
    if ( $name = $self->[_PHRASE] ) {
        $name =~ s/^"//;
        $name =~ s/"$//;
        $name =~ s/($quoted_pair)/substr $1, -1/goe;
    } elsif ( $name = $self->[_COMMENT] ) {
        $name =~ s/^\(//;
        $name =~ s/\)$//;
        $name =~ s/($quoted_pair)/substr $1, -1/goe;
        $name =~ s/$comment/ /go;
    } else {
        ($name) = $self->[_ADDRESS] =~ /($local_part)\@/o;
    }
    $NAME_CACHE{$cache_str} = $name;
}

#pod =back
#pod
#pod =head2 Overloaded Operators
#pod
#pod =over 4
#pod
#pod =item stringify
#pod
#pod   print "I have your email address, $address.";
#pod
#pod Objects stringify to C<format> by default. It's possible that you don't
#pod like that idea. Okay, then, you can change it by modifying
#pod C<$Email:Address::STRINGIFY>. Please consider modifying this package
#pod variable using C<local>. You might step on someone else's toes if you
#pod don't.
#pod
#pod   {
#pod     local $Email::Address::STRINGIFY = 'host';
#pod     print "I have your address, $address.";
#pod     #   geeknest.com
#pod   }
#pod   print "I have your address, $address.";
#pod   #   "Casey West" <casey@geeknest.com>
#pod
#pod Modifying this package variable is now deprecated. Subclassing is now the
#pod recommended approach.
#pod
#pod =cut

sub as_string {
  warn 'altering $Email::Address::STRINGIFY is deprecated; subclass instead'
    if $STRINGIFY ne 'format';

  $_[0]->can($STRINGIFY)->($_[0]);
}

use overload '""' => 'as_string', fallback => 1;

#pod =pod
#pod
#pod =back
#pod
#pod =cut

1;

=pod

=encoding UTF-8

=head1 NAME

Email::Address - RFC 2822 Address Parsing and Creation

=head1 VERSION

version 1.912

=head1 SYNOPSIS

  use Email::Address;

  my @addresses = Email::Address->parse($line);
  my $address   = Email::Address->new(Casey => 'casey@localhost');

  print $address->format;

=head1 DESCRIPTION

This class implements a regex-based RFC 2822 parser that locates email
addresses in strings and returns a list of C<Email::Address> objects found.
Alternatively you may construct objects manually. The goal of this software is
to be correct, and very very fast.

Version 1.909 and earlier of this module had vulnerabilies
(L<CVE-2015-7686|https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2015-7686>)
and (L<CVE-2015-12558|https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2018-12558>)
which allowed specially constructed email to cause a denial of service. The
reported vulnerabilities and some other pathalogical cases (meaning they really
shouldn't occur in normal email) have been addressed in version 1.910 and newer.
If you're running version 1.909 or older, you should update!

Alternatively, you could switch to L<B<Email::Address::XS>|Email::Address::XS>
which has a backward compatible API.

=head2 Package Variables

B<ACHTUNG!>  Email isn't easy (if even possible) to parse with a regex, I<at
least> if you're on a C<perl> prior to 5.10.0.  Providing regular expressions
for use by other programs isn't a great idea, because it makes it hard to
improve the parser without breaking the "it's a regex" feature.  Using these
regular expressions is not encouraged, and methods like C<<
Email::Address->is_addr_spec >> should be provided in the future.

Several regular expressions used in this package are useful to others.
For convenience, these variables are declared as package variables that
you may access from your program.

These regular expressions conform to the rules specified in RFC 2822.

You can access these variables using the full namespace. If you want
short names, define them yourself.

  my $addr_spec = $Email::Address::addr_spec;

=over 4

=item $Email::Address::addr_spec

This regular expression defined what an email address is allowed to
look like.

=item $Email::Address::angle_addr

This regular expression defines an C<$addr_spec> wrapped in angle
brackets.

=item $Email::Address::name_addr

This regular expression defines what an email address can look like
with an optional preceding display name, also known as the C<phrase>.

=item $Email::Address::mailbox

This is the complete regular expression defining an RFC 2822 email
address with an optional preceding display name and optional
following comment.

=back

=head2 Class Methods

=over

=item parse

  my @addrs = Email::Address->parse(
    q[me@local, Casey <me@local>, "Casey" <me@local> (West)]
  );

This method returns a list of C<Email::Address> objects it finds in the input
string.  B<Please note> that it returns a list, and expects that it may find
multiple addresses.  The behavior in scalar context is undefined.

The specification for an email address allows for infinitely nestable comments.
That's nice in theory, but a little over done.  By default this module allows
for one (C<1>) level of nested comments. If you think you need more, modify the
C<$Email::Address::COMMENT_NEST_LEVEL> package variable to allow more.

  $Email::Address::COMMENT_NEST_LEVEL = 10; # I'm deep

The reason for this hardly-limiting limitation is simple: efficiency.

Long strings of whitespace can be problematic for this module to parse, a bug
which has not yet been adequately addressed.  The default behavior is now to
collapse multiple spaces into a single space, which avoids this problem.  To
prevent this behavior, set C<$Email::Address::COLLAPSE_SPACES> to zero.  This
variable will go away when the bug is resolved properly.

In accordance with RFC 822 and its descendants, this module demands that email
addresses be ASCII only.  Any non-ASCII content in the parsed addresses will
cause the parser to return no results.

=item new

  my $address = Email::Address->new(undef, 'casey@local');
  my $address = Email::Address->new('Casey West', 'casey@local');
  my $address = Email::Address->new(undef, 'casey@local', '(Casey)');

Constructs and returns a new C<Email::Address> object. Takes four
positional arguments: phrase, email, and comment, and original string.

The original string should only really be set using C<parse>.

=item purge_cache

  Email::Address->purge_cache;

One way this module stays fast is with internal caches. Caches live
in memory and there is the remote possibility that you will have a
memory problem. On the off chance that you think you're one of those
people, this class method will empty those caches.

I've loaded over 12000 objects and not encountered a memory problem.

=item disable_cache

=item enable_cache

  Email::Address->disable_cache if memory_low();

If you'd rather not cache address parses at all, you can disable (and
re-enable) the Email::Address cache with these methods.  The cache is enabled
by default.

=back

=head2 Instance Methods

=over 4

=item phrase

  my $phrase = $address->phrase;
  $address->phrase( "Me oh my" );

Accessor and mutator for the phrase portion of an address.

=item address

  my $addr = $address->address;
  $addr->address( "me@PROTECTED.com" );

Accessor and mutator for the address portion of an address.

=item comment

  my $comment = $address->comment;
  $address->comment( "(Work address)" );

Accessor and mutator for the comment portion of an address.

=item original

  my $orig = $address->original;

Accessor for the original address found when parsing, or passed
to C<new>.

=item host

  my $host = $address->host;

Accessor for the host portion of an address's address.

=item user

  my $user = $address->user;

Accessor for the user portion of an address's address.

=item format

  my $printable = $address->format;

Returns a properly formatted RFC 2822 address representing the
object.

=item name

  my $name = $address->name;

This method tries very hard to determine the name belonging to the address.
First the C<phrase> is checked. If that doesn't work out the C<comment>
is looked into. If that still doesn't work out, the C<user> portion of
the C<address> is returned.

This method does B<not> try to massage any name it identifies and instead
leaves that up to someone else. Who is it to decide if someone wants their
name capitalized, or if they're Irish?

=back

=head2 Overloaded Operators

=over 4

=item stringify

  print "I have your email address, $address.";

Objects stringify to C<format> by default. It's possible that you don't
like that idea. Okay, then, you can change it by modifying
C<$Email:Address::STRINGIFY>. Please consider modifying this package
variable using C<local>. You might step on someone else's toes if you
don't.

  {
    local $Email::Address::STRINGIFY = 'host';
    print "I have your address, $address.";
    #   geeknest.com
  }
  print "I have your address, $address.";
  #   "Casey West" <casey@geeknest.com>

Modifying this package variable is now deprecated. Subclassing is now the
recommended approach.

=back

=head2 Did I Mention Fast?

On his 1.8GHz Apple MacBook, rjbs gets these results:

  $ perl -Ilib bench/ea-vs-ma.pl bench/corpus.txt 5
                   Rate  Mail::Address Email::Address
  Mail::Address  2.59/s             --           -44%
  Email::Address 4.59/s            77%             --

  $ perl -Ilib bench/ea-vs-ma.pl bench/corpus.txt 25
                   Rate  Mail::Address Email::Address
  Mail::Address  2.58/s             --           -67%
  Email::Address 7.84/s           204%             --

  $ perl -Ilib bench/ea-vs-ma.pl bench/corpus.txt 50
                   Rate  Mail::Address Email::Address
  Mail::Address  2.57/s             --           -70%
  Email::Address 8.53/s           232%             --

...unfortunately, a known bug causes a loss of speed the string to parse has
certain known characteristics, and disabling cache will also degrade
performance.

=head1 ACKNOWLEDGEMENTS

Thanks to Kevin Riggle and Tatsuhiko Miyagawa for tests for annoying
phrase-quoting bugs!

=head1 AUTHORS

=over 4

=item *

Casey West

=item *

Ricardo SIGNES <rjbs@cpan.org>

=back

=head1 CONTRIBUTORS

=for stopwords Alex Vandiver David Golden Steinbrunner Glenn Fowler Jim Brandt Kevin Falcone Pali Ruslan Zakirov sunnavy William Yardley

=over 4

=item *

Alex Vandiver <alex@chmrr.net>

=item *

David Golden <dagolden@cpan.org>

=item *

David Steinbrunner <dsteinbrunner@pobox.com>

=item *

Glenn Fowler <cebjyre@cpan.org>

=item *

Jim Brandt <jbrandt@bestpractical.com>

=item *

Kevin Falcone <kevin@jibsheet.com>

=item *

Pali <pali@cpan.org>

=item *

Ruslan Zakirov <ruz@bestpractical.com>

=item *

sunnavy <sunnavy@bestpractical.com>

=item *

William Yardley <pep@veggiechinese.net>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2004 by Casey West.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

__END__

#pod =head2 Did I Mention Fast?
#pod
#pod On his 1.8GHz Apple MacBook, rjbs gets these results:
#pod
#pod   $ perl -Ilib bench/ea-vs-ma.pl bench/corpus.txt 5
#pod                    Rate  Mail::Address Email::Address
#pod   Mail::Address  2.59/s             --           -44%
#pod   Email::Address 4.59/s            77%             --
#pod
#pod   $ perl -Ilib bench/ea-vs-ma.pl bench/corpus.txt 25
#pod                    Rate  Mail::Address Email::Address
#pod   Mail::Address  2.58/s             --           -67%
#pod   Email::Address 7.84/s           204%             --
#pod
#pod   $ perl -Ilib bench/ea-vs-ma.pl bench/corpus.txt 50
#pod                    Rate  Mail::Address Email::Address
#pod   Mail::Address  2.57/s             --           -70%
#pod   Email::Address 8.53/s           232%             --
#pod
#pod ...unfortunately, a known bug causes a loss of speed the string to parse has
#pod certain known characteristics, and disabling cache will also degrade
#pod performance.
#pod
#pod =head1 ACKNOWLEDGEMENTS
#pod
#pod Thanks to Kevin Riggle and Tatsuhiko Miyagawa for tests for annoying
#pod phrase-quoting bugs!
#pod
#pod =cut

