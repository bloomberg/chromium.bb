package LWP::Online;

=pod

=head1 NAME

LWP::Online - Does your process have access to the web

=head1 SYNOPSIS

  use LWP::Online 'online';
  
  # "Is the internet working?"
  die "NO INTARWWEB!!!" unless online();
  
  # The above means something like this
  unless ( online('http') ) {
      die "No basic http access to the web";
  }
  
  # Special syntax for use in test scripts that need
  # "real" access to the internet. Scripts will automatically
  # skip if connection fails.
  use LWP::Online ':skip_all';
  use Test::More tests => 4; #after LWP::Online

=head1 DESCRIPTION

This module attempts to answer, as accurately as it can, one of the
nastiest technical questions there is.

B<Am I on the internet?>

The answer is useful in a wide range of decisions. For example...

I<Should my test scripts run the online portion of the tests or
just skip them?>

I<Do I try to fetch fresh data from the server?>

I<If my request to the server breaks, is it because I'm offline, or
because the server is offline?>

And so on, and so forth.

But a host of networking and security issues make this problem
very difficult. There are firewalls, proxies (both well behaved and
badly behaved). We might not have DNS. We might not have a network
card at all!

You might have network access, but only to a for-money wireless network
that responds to ever HTTP request with a page asking you to enter your
credit card details for paid access. Which means you don't "REALLY" have
access.

The mere nature of the question makes it practically unsolvable.

But with the answer being so useful, and the only other alternative being
to ask the user "duh... are you online?" (when you might not have a user
at all) it's my gut feeling that it is worthwhile at least making an
attempt to solve the problem, if only in a limited way.

=head2 Why LWP::Online? Why not Net::Online?

The nice thing about LWP::Online is that LWP deals with a whole range of
different transports, and is very commonly installed. HTTP, HTTPS, FTP,
and so on and so forth.

Attempting to do a more generalised Net::Online that might also check for
SSH and so on would end up most likely having to install a whole bunch of
modules that you most likely will never use.

So LWP forms a nice base on which to write a module that covers most of
the situations in which you might care, while keeping the dependency
overhead down to a minimum.

=head2 Scope

"Am I online?" is inherently an Open Problem.

That is, it's a problem that had no clean permanent solution, and for
which you could just keep writing more and more functionality
indefinitely, asymtopically approaching 100% correctness but never
reaching it.

And so this module is intended to do as good a job as possible, without
having to resort to asking any human questions (who may well get it wrong
anyway), and limiting itself to a finite amount of programming work and
a reasonable level of memory overhead to load the code.

It is thus understood the module will B<never> be perfect, and that if
any new functionality is desired, it needs to be able to implemented by
the person that desires the new behaviour, and in a reasonably small
amount of additional code.

This module is also B<not> intended to compensate for malicious behaviour
of any kind, it is quite possible that some malicious person might proxy
fake versions of sites that pass our content checks and then proceed
to show you other bad pages.

=head2 Test Mode

  use LWP::Online ':skip_all';

As a convenience when writing tests scripts base on L<Test::More>, the
special ':skip_all' param can be provided when loading B<LWP::Online>.

This implements the functional equivalent of the following.

  BEGIN {
    require Test::More;
    unless ( LWP::Online::online() ) {
      Test::More->import(
        skip_all => 'Test requires a working internet connection'
      );
    }
  }

The :skip_all special import flag can be mixed with regular imports.

=head1 FUNCTIONS

=cut

use 5.005;
use strict;
use Carp              ();
use URI         1.35  ();
use LWP::Simple 5.805 qw{ get $ua };

use vars qw{$VERSION @ISA @EXPORT_OK};
BEGIN {
	$VERSION = '1.08';

	# We are an Exporter
	require Exporter;
	@ISA       = qw{ Exporter };
	@EXPORT_OK = qw{ online offline };

	# Set the useragent timeout
	$ua->timeout(30);
}

# Set up configuration data
use vars qw{%SUPPORTED @RELIABLE_HTTP};
BEGIN {
	# What transports do we support
	%SUPPORTED = map { $_ => 1 } qw{ http };

	# (Relatively) reliable websites
	@RELIABLE_HTTP = (
		# These are some initial trivial checks.
		# The regex are case-sensitive to at least
		# deal with the "couldn't get site.com case".
		'http://www.msftncsi.com/ncsi.txt' => sub { $_ eq 'Microsoft NCSI' },
		'http://google.com/'               => sub { /About Google/         },
		'http://yahoo.com/'                => sub { /Yahoo!/               },
		'http://amazon.com/'               => sub { /Amazon/ and /Cart/    },
		'http://cnn.com/'                  => sub { /CNN/                  },
	);
}

sub import {
	my $class = shift;

	# Handle the :skip_all special case
	my @functions = grep { $_ ne ':skip_all' } @_;
	if ( @functions != @_ ) {
		require Test::More;
		unless ( online() ) {
			Test::More->import( skip_all => 'Test requires a working internet connection' );
		}
	}

	# Hand the rest of the params off to Exporter
	return $class->export_to_level( 1, $class, @functions );
}





#####################################################################
# Exportable Functions

=pod

=head2 online

  # Default check (uses http)
  online() or die "No Internet";
  
  # The above is equivalent to
  online('http') or die "No Internet";

The importable B<online> function is the main functionality provided
by B<LWP::Online>. It takes a single optional transport name ('http'
by default) and checks that LWP connectivity is available for that
transport.

Because it is intended as a Do What You Mean function, it checks not
only that a network connection is available, and http requests return
content, but also that it returns the CORRECT content instead of
unexpected content supplied by a man in the middle.

For example, many wireless connections require login or payment, and
will return a service provider page for any URI that you attempt to
fetch.

The set of websites used for the testing is the Google, Amazon,
Yahoo and CNN websites. The check is for a copyright statement on their
homepage, and the function returns true as soon as two of the website
return correctly, making the method relatively redundant.

Returns true if the computer is "online" (has a working connection via
LWP) or false if not.

=cut

sub online {
	# Shortcut the default to plain http_online test
	return http_online() unless @_;

	while ( @_ ) {
		# Get the transport to test
		my $transport = shift;
		unless ( $transport and $SUPPORTED{$transport} ) {
			Carp::croak("Invalid or unsupported transport");
		}

		# Hand off to the transport function
		if ( $transport eq 'http' ) {
			http_online() or return '';
		} else {
			Carp::croak("Invalid or unsupported transport");
		}
	}

	# All required transports available
	return 1;
}

=pod

=head2 offline

The importable B<offline> function is provided as a convenience.

It provides a simple pass-through (including any params) to the B<online>
function, but with a negated result.

=cut

sub offline {
	! online(@_);
}





#####################################################################
# Transport Functions

sub http_online {
	# Check the reliable websites list.
	# If networking is offline, an error/paysite page might still
	# give us a page that matches a page check, while any one or
	# two of the reliable websites might be offline for some
	# unknown reason (DDOS, earthquake, chinese firewall, etc)
	# So we want 2 or more sites to pass checks to make the
	# judgement call that we are online.
	my $good     = 0;
	my $bad      = 0;
	my @reliable = @RELIABLE_HTTP;
	while ( @reliable ) {
		# Check the current good/bad state and consider
		# making the online/offline judgement call.
		return 1  if $good > 1;
		return '' if $bad  > 2;

		# Try the next reliable site
		my $site  = shift @reliable;
		my $check = shift @reliable;

		# Try to fetch the site
		my $content;
		SCOPE: {
			local $@;
			$content = eval { get($site) };
			if ( $@ ) {
				# An exception is a simple failure
				$bad++;
				next;
			}
		}
		unless ( defined $content ) {
			# get() returns undef on failure
			$bad++;
			next;
		}

		# We got _something_.
		# Check if it looks like what we want
		SCOPE: {
			local $_ = $content;
			if ( $check->() ) {
				$good++;
			} else {
				$bad++;
			}
		}
	}

	# We've run out of sites to check... erm... uh...
	# We should probably fail conservatively and say not online.
	return '';
}

1;

=pod

=head1 TO DO

- Add more transport types that can be checked, somehow keeping the
code growth under control.

=head1 SUPPORT

This module is stored in an Open Repository at the following address.

L<http://svn.ali.as/cpan/trunk/LWP-Online>

Write access to the repository is made available automatically to any
published CPAN author, and to most other volunteers on request.

If you are able to submit your bug report in the form of new (failing)
unit tests (which for this module will be extremely difficult), or can
apply your fix directly instead of submitting a patch, you are B<strongly>
encouraged to do so as the author currently maintains over 100 modules
and it can take some time to deal with non-Critical bug reports or patches.

This will guarentee that your issue will be addressed in the next
release of the module.

If you cannot provide a direct test or fix, or don't have time to do so,
then regular bug reports are still accepted and appreciated via the CPAN
bug tracker.

L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=LWP-Online>

For other issues, for commercial enhancement or support, or to have your
write access enabled for the repository, contact the author at the email
address above.

=head1 AUTHOR

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

=head1 SEE ALSO

L<LWP::Simple>

=head1 COPYRIGHT

Copyright 2006 - 2011 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
