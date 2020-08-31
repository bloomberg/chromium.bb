package Crypt::Random::Seed;
use strict;
use warnings;
use Fcntl;
use Carp qw/carp croak/;

# cert insists on using constant, but regular critic doesn't like it.
## no critic (constant)

BEGIN {
  $Crypt::Random::Seed::AUTHORITY = 'cpan:DANAJ';
  $Crypt::Random::Seed::VERSION = '0.03';
}

use base qw( Exporter );
our @EXPORT_OK = qw( );
our %EXPORT_TAGS = (all => [ @EXPORT_OK ]);
# Export nothing by default

use constant UINT32_SIZE => 4;

# These are the pre-defined names.  We don't let user methods use these.
my %defined_methods = map { $_ => 1 }
  (qw(CryptGenRandom RtlGenRand EGD /dev/random /dev/urandom
      TESHA2-strong TESHA2-weak));
# If given one of these names as whitelist/blacklist, we add these also.
my %name_aliases = (
  'Win32'  => [qw(RtlGenRand CryptGenRandom)],
  'TESHA2' => [qw(TESHA2-strong TESHA2-weak)],
);

sub new {
  my ($class, %params) = @_;
  my $self = {};

  # Trying to handle strong vs. weak is fraught with complication, so just
  # remove the idea entirely.
  if (defined $params{Weak}) {
    # In this release, just silently don't use it.
    delete $params{Weak};
  }

  if (defined $params{Source}) {
    if (ref($params{Source}) eq 'CODE') {
      $self->{Name}      = 'User';
      $self->{SourceSub} = $params{Source};
      # We don't know if it is blocking or strong, assume neither
      $self->{Blocking} = 0;
      $self->{Strong} = 0;
    } elsif (ref($params{Source}) eq 'ARRAY') {
      ($self->{Name}, $self->{SourceSub}, $self->{Blocking}, $self->{Strong})
      = @{$params{Source}};
      # For sanity, don't let them redefine the standard names.
      croak "Invalid name: $self->{Name}.  Name reserved."
        if defined $defined_methods{$self->{Name}};
    } else {
      croak "Invalid 'Source'.  Should be code or array reference.";
    }
  } else {
    # This is a sorted list -- the first one that returns true gets used.
    my @methodlist = (
       \&_try_win32,
       \&_try_egd,
       \&_try_dev_random,
       \&_try_dev_urandom,
       \&_try_tesha2,
    );

    my %whitelist;
    my $have_whitelist = 0;
    if (defined $params{Only}) {
      croak "Parameter 'Only' must be an array ref" unless ref($params{Only}) eq 'ARRAY';
      $have_whitelist = 1;
      $whitelist{$_} = 1 for @{$params{Only}};
      while ( my($name, $list) = each %name_aliases) {
        @whitelist{@$list} = (1) x scalar @$list if $whitelist{$name};
      }
    }
    my %blacklist;
    if (defined $params{Never}) {
      croak "Parameter 'Never' must be an array ref" unless ref($params{Never}) eq 'ARRAY';
      $blacklist{$_} = 1 for @{$params{Never}};
      while ( my($name, $list) = each %name_aliases) {
        @blacklist{@$list} = (1) x scalar @$list if $blacklist{$name};
      }
    }

    foreach my $m (@methodlist) {
      my ($name, $rsub, $isblocking, $isstrong) = $m->();
      next unless defined $name;
      next if $isblocking && ($params{NonBlocking} || $params{Nonblocking} || $params{nonblocking});
      #next if !$isstrong && !$params{Weak};
      next if $blacklist{$name};
      next if $have_whitelist && !$whitelist{$name};
      $self->{Name}      = $name;
      $self->{SourceSub} = $rsub;
      $self->{Blocking}  = $isblocking;
      $self->{Strong}    = $isstrong;
      last;
    }
  }
  # Couldn't find anything appropriate
  return unless defined $self->{SourceSub};

  bless $self, $class;
  return $self;
}

# Nothing special to do on destroy
#sub DESTROY {
#  my $self = shift;
#  delete $self->{$_} for keys $self;
#  return;
#}

sub name {
  my $self = shift;
  return $self->{Name};
}
sub is_blocking {
  my $self = shift;
  return $self->{Blocking};
}
sub is_strong {
  my $self = shift;
  return $self->{Strong};
}
sub random_bytes {
  my ($self, $nbytes) = @_;
  return '' unless defined $nbytes && int($nbytes) > 0;
  my $rsub = $self->{SourceSub};
  return unless defined $rsub;
  return $rsub->(int($nbytes));
}
sub random_values {
  my ($self, $nvalues) = @_;
  return unless defined $nvalues && int($nvalues) > 0;
  my $rsub = $self->{SourceSub};
  return unless defined $rsub;
  return unpack( 'L*', $rsub->(UINT32_SIZE * int($nvalues)) );
}


sub _try_tesha2 {
  eval { require Crypt::Random::TESHA2; Crypt::Random::TESHA2->import(); 1; }
  or return;
  my $isstrong = Crypt::Random::TESHA2::is_strong();
  my $name = join('-', 'TESHA2', ($isstrong) ? 'strong' : 'weak');
  return ($name, \&Crypt::Random::TESHA2::random_bytes, 0, 1);
}

sub _try_dev_urandom {
  return unless -r "/dev/urandom";
  return ('/dev/urandom', sub { __read_file('/dev/urandom', @_); }, 0, 0);
}

sub _try_dev_random {
  return unless -r "/dev/random";
  # FreeBSD's /dev/random is 256-bit Yarrow non-blocking.
  # Is it 'strong'?  Debatable -- we'll say it is.
  my $blocking = ($^O eq 'freebsd') ? 0 : 1;
  return ('/dev/random', sub { __read_file('/dev/random', @_); }, $blocking, 1);
}

sub __read_file {
  my ($file, $nbytes) = @_;
  return unless defined $nbytes && $nbytes > 0;
  sysopen(my $fh, $file, O_RDONLY);
  binmode $fh;
  my($s, $buffer, $nread) = ('', '', 0);
  while ($nread < $nbytes) {
    my $thisread = sysread $fh, $buffer, $nbytes-$nread;
    # Count EOF as an error.
    croak "Error reading $file: $!\n" unless defined $thisread && $thisread > 0;
    $s .= $buffer;
    $nread += length($buffer);
    #die unless $nread == length($s);  # assert
  }
  croak "Internal file read error: wanted $nbytes, read $nread"
      unless $nbytes == length($s);  # assert
  return $s;
}

# Most of this is taken without notice from Crypt::URandom 0.28 and
# Crypt::Random::Source::Strong::Win32 0.07.
# Kudos to David Dick and Max Kanat-Alexander for doing all the work.
#
# See some documentation here:
#   http://msdn.microsoft.com/en-us/library/aa379942.aspx
# where they note that the output of these is really a well seeded CSPRNG:
# either FIPS 186-2 (older) or AES-CTR (Vista SP1 and newer).

sub _try_win32 {
  return unless $^O eq 'MSWin32';
  # Cygwin has /dev/random at least as far back as 2000.
  eval { require Win32; require Win32::API; require Win32::API::Type; 1; }
  or return;

  use constant CRYPT_SILENT      => 0x40;       # Never display a UI.
  use constant PROV_RSA_FULL     => 1;          # Which service provider.
  use constant VERIFY_CONTEXT    => 0xF0000000; # Don't need existing keypairs.
  use constant W2K_MAJOR_VERSION => 5;          # Windows 2000
  use constant W2K_MINOR_VERSION => 0;

  my ($major, $minor) = (Win32::GetOSVersion())[1, 2];
  return if $major < W2K_MAJOR_VERSION;

  if ($major == W2K_MAJOR_VERSION && $minor == W2K_MINOR_VERSION) {
    # We are Windows 2000.  Use the older CryptGenRandom interface.
    my $crypt_acquire_context_a =
              Win32::API->new( 'advapi32', 'CryptAcquireContextA', 'PPPNN',
                'I' );
    return unless defined $crypt_acquire_context_a;
    my $context = chr(0) x Win32::API::Type->sizeof('PULONG');
    my $result = $crypt_acquire_context_a->Call(
             $context, 0, 0, PROV_RSA_FULL, CRYPT_SILENT | VERIFY_CONTEXT );
    return unless $result;
    my $pack_type = Win32::API::Type::packing('PULONG');
    $context = unpack $pack_type, $context;
    my $crypt_gen_random =
              Win32::API->new( 'advapi32', 'CryptGenRandom', 'NNP', 'I' );
    return unless defined $crypt_gen_random;
    return ('CryptGenRandom',
            sub {
              my $nbytes = shift;
              my $buffer = chr(0) x $nbytes;
              my $result = $crypt_gen_random->Call($context, $nbytes, $buffer);
              croak "CryptGenRandom failed: $^E" unless $result;
              return $buffer;
            },
            0, 1);  # Assume non-blocking and strong
  } else {
    my $rtlgenrand = Win32::API->new( 'advapi32', <<'_RTLGENRANDOM_PROTO_');
INT SystemFunction036(
  PVOID RandomBuffer,
  ULONG RandomBufferLength
)
_RTLGENRANDOM_PROTO_
    return unless defined $rtlgenrand;
    return ('RtlGenRand',
            sub {
              my $nbytes = shift;
              my $buffer = chr(0) x $nbytes;
              my $result = $rtlgenrand->Call($buffer, $nbytes);
              croak "RtlGenRand failed: $^E" unless $result;
              return $buffer;
            },
            0, 1);  # Assume non-blocking and strong
  }
  return;
}

sub _try_egd {
  # For locations, we'll look in the files OpenSSL's RAND_egd looks, as well
  # as /etc/entropy which egd 0.9 recommends.  This also works with PRNGD.
  # PRNGD uses a seed+CSPRNG so is non-blocking, but we can't tell them apart.
  foreach my $device (qw( /var/run/egd-pool /dev/egd-pool /etc/egd-pool /etc/entropy )) {
    next unless -r $device && -S $device;
    eval { require IO::Socket; 1; } or return;
    # We're looking for a socket that returns the entropy available when given
    # that command.  Set timeout to 1 to prevent hanging -- if it is a socket
    # but won't return the available entropy in under a second, move on.
    my $socket = IO::Socket::UNIX->new(Peer => $device, Timeout => 1);
    next unless $socket;
    $socket->syswrite( pack("C", 0x00), 1) or next;
    die if $socket->error;
    my($entropy_string, $nread);
    # Sadly this doesn't honor the timeout.  We'll have to do an eval / alarm.
    # We only timeout here if this is a live socket to a sleeping process.
    eval {
      local $SIG{ALRM} = sub { die "alarm\n" };
      alarm 1;
      $nread = $socket->sysread($entropy_string, 4);
      alarm 0;
    };
    if ($@) {
      die unless $@ eq "alarm\n";
      next;
    }
    next unless defined $nread && $nread == 4;
    my $entropy_avail = unpack("N", $entropy_string);
    return ('EGD', sub { __read_egd($device, @_); }, 1, 1);
  }
  return;
}

sub __read_egd {
  my ($device, $nbytes) = @_;
  return unless defined $device;
  return unless defined $nbytes && int($nbytes) > 0;
  croak "$device doesn't exist!" unless -r $device && -S $device;
  my $socket = IO::Socket::UNIX->new(Peer => $device);
  croak "Can't talk to EGD on $device. $!" unless $socket;
  my($s, $buffer, $toread) = ('', '', $nbytes);
  while ($toread > 0) {
    my $this_request = ($toread > 255) ? 255 : $toread;
    # Use the blocking interface.
    $socket->syswrite( pack("CC", 0x02, $this_request), 2);
    my $this_grant = $socket->sysread($buffer, $this_request);
    croak "Error reading EDG data from $device: $!\n"
          unless defined $this_grant && $this_grant == $this_request;
    $s .= $buffer;
    $toread -= length($buffer);
  }
  croak "Internal EGD read error: wanted $nbytes, read ", length($s), ""
      unless $nbytes == length($s);  # assert
  return $s;
}

1;

__END__

# ABSTRACT: Simple method to get strong randomness

=pod

=head1 NAME

Crypt::Random::Seed - Simple method to get strong randomness


=head1 VERSION

Version 0.03


=head1 SYNOPSIS

  use Crypt::Random::Seed;

  my $source = new Crypt::Random::Seed;
  die "No strong sources exist" unless defined $source;
  my $seed_string = $source->random_bytes(4);
  my @seed_values = $source->random_values(4);

  # Only non-blocking sources
  my $nonblocking_source = Crypt::Random::Seed->new( NonBlocking=>1 );

  # Blacklist sources (never choose the listed sources)
  my $nowin32_source = Crypt::Random::Seed->new( Never=>['Win32'] );

  # Whitelist sources (only choose from these sources)
  my $devr_source = Crypt::Random::Seed->new( Only=>['TESHA2'] );

  # Supply a custom source.
  my $user_src = Crypt::Random::Seed->new( Source=>sub { myfunc(shift) } );
  # Or supply a list of [name, sub, is_blocking, is_strong]
  $user_src = Crypt::Random::Seed->new(
     Source=>['MyRandomFunction',sub {myfunc(shift)},0,1] );

  # Given a source there are a few things we can do:
  say "My randomness source is ", $source->name();
  say "I am a blocking source" if $source->is_blocking();
  say "I am a strong randomness source" if $source->is_strong()
  say "Four 8-bit numbers:",
      join(",", map { ord $source->random_bytes(1) } 1..4);'
  say "Four 32-bit numbers:", join(",", $source->random_values(4));


=head1 DESCRIPTION

A simple mechanism to get strong randomness.  The main purpose of this
module is to provide a simple way to generate a seed for a PRNG such as
L<Math::Random::ISAAC>, for use in cryptographic key generation, or as the
seed for an upstream module such as L<Bytes::Random::Secure>.  Flags for
requiring non-blocking sources are allowed, as well as a very simple
method for plugging in a source.

The randomness sources used are, in order:

=over 4

=item User supplied.

If the constructor is called with a Source defined, then it is used.  It
is not checked vs. other flags (NonBlocking, Never, Only).

=item Win32 Crypto API.

This will use C<CryptGenRandom> on Windows 2000 and C<RtlGenRand> on
Windows XP and newer.  According to MSDN, these are well-seeded CSPRNGs
(FIPS 186-2 or AES-CTR), so will be non-blocking.

=item EGD / PRNGD.

This looks for sockets that speak the L<EGD|http://egd.sourceforge.net/>
protocol, including L<PRNGD|http://prngd.sourceforge.net/>.  These are
userspace entropy daemons that are commonly used by OpenSSL, OpenSSH, and
GnuGP.  The locations searched are C</var/run/egd-pool>, C</dev/egd-pool>,
C</etc/egd-pool>, and C</etc/entropy>.  EGD is blocking, while PRNGD is
non-blocking (like the Win32 API, it is really a seeded CSPRNG).  However
there is no way to tell them apart, so we treat it as blocking.  If your
O/S supports /dev/random, consider L<HAVEGED|http://www.issihosts.com/haveged/>
as an alternative (a system daemon that refills /dev/random as needed).

=item /dev/random.

The strong source of randomness on most UNIX-like systems.  Cygwin uses
this, though it maps to the Win32 API.  On almost all systems this is a
blocking source of randomness -- if it runs out of estimated entropy, it
will hang until more has come into the system.  If this is an issue,
which it often is on embedded devices, running a tool such as
L<HAVEGED|http://www.issihosts.com/haveged/> will help immensely.

=item /dev/urandom.

A nonblocking source of randomness that we label as weak, since it will
continue providing output even if the actual entropy has been exhausted.

=item TESHA2.

L<Crypt::Random::TESHA2> is a Perl module that generates random bytes from
an entropy pool fed with timer/scheduler variations.  Measurements and
tests are performed on installation to determine whether the source is
considered strong or weak.  This is entirely in portable userspace,
which is good for ease of use, but really requires user verification
that it is working as expected if we expect it to be strong.  The
concept is similar to L<Math::TrulyRandom> though updated to something
closer to what TrueRand 2.1 does vs. the obsolete version 1 that
L<Math::TrulyRandom> implements.  It is very slow and has wide speed
variability across platforms : I've seen numbers ranging from 40 to
150,000 bits per second.

=back

A source can also be supplied in the constructor.  Each of these sources will
have its debatable points about perceived strength.  E.g. Why is /dev/urandom
considered weak while Win32 is strong?  Can any userspace method such as
TrueRand or TESHA2 be considered strong?


=head2 SOURCE TABLE

This table summarizes the default sources:

  +------------------+-------------+------------+--------------------+
  |      SOURCE      |  STRENGTH   |  BLOCKING  |       NOTE         |
  |------------------+-------------+------------+--------------------|
  | RtlGenRandom     |   Strong(1) |     No     | Default WinXP+     |
  |------------------+-------------+------------+--------------------|
  | CryptGenRandom   |   Strong(1) |     No     | Default Win2000    |
  |------------------+-------------+------------+--------------------|
  | EGD              |   Strong    |    Yes(2)  | also PRNGD, etc.   |
  |------------------+-------------+------------+--------------------|
  | /dev/random      |   Strong    |    Yes     | Typical UNIX       |
  |------------------+-------------+------------+--------------------|
  | /dev/urandom     |    Weak     |     No     | Typical UNIX NB    |
  |------------------+-------------+------------+--------------------|
  | TESHA2-strong    |   Strong    |     No     |                    |
  |------------------+-------------+------------+--------------------|
  | TESHA2-weak      |    Weak     |     No     |                    |
  +------------------+-------------+------------+--------------------+

The alias 'Win32' can be used in whitelist and blacklist and will match both
the Win32 sources C<RtlGenRandom> and C<CryptGenRandom>.  The alias 'TESHA2'
may be similarly used and matches both the weak and strong sources.

  1) Both CryptGenRandom and RtlGenRandom are considered strong by this
     package, even though both are seeded CSPRNGs so should be the equal of
     /dev/urandom in this respect.  The CryptGenRandom function used in
     Windows 2000 has some known issues so should be considered weaker.

  2) EGD is blocking, PRNGD is not.  We cannot tell the two apart.  There are
     other software products that use the same protocol, and each will act
     differently.  E.g. EGD mixes in system entropy on every request, while
     PRNGD mixes on a time schedule.


=head2 STRENGTH

In theory, a strong generator will provide true entropy.  Even if a third
party knew a previous result and the entire state of the generator at any
time up to when their value was returned, they could still not effectively
predict the result of the next returned value.  This implies the generator
must either be blocking to wait for entropy (e.g. /dev/random) or go through
some possibly time-consuming process to gather it (TESHA2, EGD, the HAVEGE
daemon refilling /dev/random).  Note: strong in this context means practically
strong, as most computers don't have a true hardware entropy generator.  The
goal is to make all the attackers ill-gotten knowledge give them no better
solution than if they did not have the information.

Creating a satisfactory strength measurement is problematic.  The Win32
Crypto API is considered "strong" by most customers and every other Perl
module, however it is a well seeded CSPRNG according to the MSDN docs,
so is not a strong source based on the definition in the previous paragraph.
Similarly, almost all sources consider /dev/urandom to be weak, as once it
runs out of entropy it returns a deterministic function based on its state
(albeit one that cannot be run either direction from a returned result if the
internal state is not known).

Because of this confusion, I have removed the C<Weak> configuration option
that was present in version 0.01.  It will now be ignored.  You should be
able to use a combination of whitelist, blacklist, and the source's
C<is_strong> return value to decide if this meets your needs.  On Win32, you
really only have a choice of Win32 and TESHA2.  The former is going to be
what most people want, and can be chosen even with non-blocking set.  On most
UNIX systems, C</dev/random> will be chosen for blocking and C</dev/urandom>
for non-blocking, which is what should be done in most cases.


=head2 BLOCKING

EGD and /dev/random are blocking sources.  This means that if they run out of
estimated entropy, they will pause until they've collected more.  This means
your program also pauses.  On typical workstations this may be a few seconds
or even minutes.  On an isolated network server this may cause a delay of
hours or days.  EGD is proactive about gathering more entropy as fast as it
can.  Running a tool such as the HAVEGE daemon or timer_entropyd can make
/dev/random act like a non-blocking source, as the entropy daemon will wake
up and refill the pool almost instantly.

Win32, PRNGD, and /dev/urandom are fast nonblocking sources.  When they run
out of entropy, they use a CSPRNG to keep supplying data at high speed.
However this means that there is no additional entropy being supplied.

TESHA2 is nonblocking, but can be very slow.  /dev/random can be faster if run
on a machine with lots of activity.  On an isolated server, TESHA2 may be
much faster.  Also note that the blocking sources such as EGD and /dev/random
both try to maintain reasonably large entropy pools, so small requests can be
supplied without blocking.


=head2 IN PRACTICE

Use the default to get the best source known.  If you know more about the
sources available, you can use a whitelist, blacklist, or a custom source.
In general, to get the best source (typically Win32 or /dev/random):

  my $source = Crypt::Random::Seed->new();

To get a good non-blocking source (Win32 or /dev/urandom):

  my $source = Crypt::Random::Seed->new(NonBlocking => 1);


=head1 METHODS

=head2 new

The constructor with no arguments will find the first available source in its
fixed list and return an object that performs the defined methods.  If no
sources could be found (quite unusual) then the returned value will be undef.

Optional parameters are passed in as a hash and may be mixed.

=head3 NonBlocking => I<boolean>

Only non-blocking sources will be allowed.  In practice this means EGD
and /dev/random will not be chosen (except on FreeBSD where it is
non-blocking).

=head3 Only => [I<list of strings>]

Takes an array reference containing one or more string source names.  No
source whose name does not match one of these strings will be chosen.  The
string 'Win32' will match either of the Win32 sources, and 'TESHA2' will match
both the strong and weak versions.

=head3 Never => [I<list of strings>]

Takes an array reference containing one or more string source names.  No
source whose name matches one of these strings will be chosen.  The string
'Win32' will match either of the Win32 sources, and 'TESHA2' will match both
the strong and weak versions.

=head3 Source => sub { I<...> }

Uses the given anonymous subroutine as the generator.  The subroutine will
be given an integer (the argument to C<random_bytes>) and should return
random data in a string of the given length.  For the purposes of the other
object methods, the returned object will have the name 'User', and be
considered non-blocking and non-strong.

=head3 Source => ['I<name>', sub { I<...> }, I<is_blocking>, I<is_strong>]

Similar to the simpler source routine, but also allows the other source
parameters to be defined.  The name may not be one of the standard names
listed in the L</"name"> section.


=head2 random_bytes($n)

Takes an integer and returns a string of that size filled with random data.
Returns an empty string if the argument is not defined or is not more than
zero.

=head2 random_values($n)

Takes an integer and returns an array of that many random 32-bit values.
Returns an empty array if the argument is not defined or is not more than
zero.

=head2 name

Returns the text name of the random source.  This will be one of:
C<User> for user defined,
C<CryptGenRandom> for Windows 2000 Crypto API,
C<RtlGenRand> for Windows XP and newer Crypto API,
C<EGD> for a known socket speaking the EGD protocol,
C</dev/random> for the UNIX-like strong randomness source,
C</dev/urandom> for the UNIX-like non-blocking randomness source,
C<TESHA2-strong> for the userspace entropy method when considered strong,
C<TESHA2-weak> for the userspace entropy method when considered weak.
Other methods may be supported in the future.  User supplied sources may be
named anything other than one of the defined names.

=head2 is_strong

Returns 1 or 0 indicating whether the source is considered a strong source
of randomness.  See the L</"STRENGTH"> section for more discussion of what
this means, and the L<source table|/"SOURCE TABLE"> for what we think of each
source.

=head2 is_blocking

Returns 1 or 0 indicating whether the source can block on read.  Be aware
that even if a source doesn't block, it may be extremely slow.


=head1 AUTHORS

Dana Jacobsen E<lt>dana@acm.orgE<gt>


=head1 ACKNOWLEDGEMENTS

To the best of my knowledge, Max Kanat-Alexander was the original author of
the Perl code that uses the Win32 API.  I used his code as a reference.

David Oswald gave me a lot of help with API discussions and code reviews.


=head1 SEE ALSO

The first question one may ask is "Why yet another module of this type?"
None of the modules on CPAN quite fit my needs, hence this.  Some alternatives:

=head2 L<Crypt::Random::Source>

A comprehensive system using multiple plugins.  It has a nice API, but
uses L<Any::Moose> which means you're loading up Moose or Mouse just to
read a few bytes from /dev/random.  It also has a very long dependency chain,
with on the order of 40 modules being installed as prerequisites (depending
of course on whether you use any of them on other projects).  Lastly, it
requires at least Perl 5.8, which may or may not matter to you.  But it
matters to some other module builders who end up with the restriction in
their modules.

=head2 L<Crypt::URandom>

A great little module that is almost what I was looking for.
L<Crypt::Random::Seed> will act the same if given the constructor:

  my $source = Crypt::Random::Seed->new(
     NonBlocking => 1,
     Only => [qw(/dev/random /dev/urandom Win32)]
  );
  croak "No randomness source available" unless defined $source;

Or you can leave out the C<Only> and have TESHA2 as a backup.

=head2 L<Crypt::Random>

Requires L<Math::Pari> which makes it unacceptable in some environments.
Has more features (numbers in arbitrary bigint intervals or bit sizes).
L<Crypt::Random::Seed> is taking a simpler approach, just handling returning
octets and letting upstream modules handle the rest.

=head2 L<Data::Entropy>

An interesting module that contains a source encapsulation (defaults to system
rand, but has many plugins), a good CSPRNG (AES in counter mode), and the
L<Data::Entropy::Algorithms> module with many ways to get bits, ints, bigints,
floats, bigfloats, shuffles, and so forth.  From my perspective, the
algorithms module is the highlight, with a lot of interesting code.


=head2 Upstream modules

Some modules that could use this module to help them:
L<Bytes::Random::Secure>,
L<Math::Random::ISAAC>,
L<Math::Random::Secure>,
and L<Math::Random::MT>
to name a few.


=head1 COPYRIGHT

Copyright 2013 by Dana Jacobsen E<lt>dana@acm.orgE<gt>

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

The software is provided "AS IS", without warranty of any kind, express or
implied, including but not limited to the warranties of merchantability,
fitness for a particular purpose and noninfringement. In no event shall the
authors or copyright holders be liable for any claim, damages or other
liability, whether in an action of contract, tort or otherwise, arising from,
out of or in connection with the software or the use or other dealings in
the software.

=cut
