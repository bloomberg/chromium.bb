=head1 NAME

Canary::Stability - canary to check perl compatibility for schmorp's modules

=head1 SYNOPSIS

 # in Makefile.PL
 use Canary::Stability DISTNAME => 2001, MINIMUM_PERL_VERSION;

=head1 DESCRIPTION

This module is used by Schmorp's modules during configuration stage to
test the installed perl for compatibility with his modules.

It's not, at this stage, meant as a tool for other module authors,
although in principle nothing prevents them from subscribing to the same
ideas.

See the F<Makefile.PL> in L<Coro> or L<AnyEvent> for usage examples.

=cut

package Canary::Stability;

BEGIN {
   $VERSION = 2013;
}

sub sgr {
   # we just assume ANSI almost everywhere
   # red 31, yellow 33, green 32
   local $| = 1;

   $ENV{PERL_CANARY_STABILITY_COLOUR} ne 0
   and ((-t STDOUT and length $ENV{TERM}) or $ENV{PERL_CANARY_STABILITY_COLOUR})
   and print "\e[$_[0]m";
}

sub import {
   my (undef, $distname, $minvers, $minperl) = @_;

   $ENV{PERL_CANARY_STABILITY_DISABLE}
      and return;

   $minperl ||= 5.008002;

   print <<EOF;

***
*** Canary::Stability COMPATIBILITY AND SUPPORT CHECK
*** =================================================
***
*** Hi!
***
*** I do my best to provide predictable and reliable software.
***
*** However, in recent releases, P5P (who maintain perl) have been
*** introducing regressions that are sometimes subtle and at other times
*** catastrophic, often for personal preferences with little or no concern
*** for existing code, most notably CPAN.
***
*** For this reason, it has become very hard for me to maintain the level
*** of reliability and support I have committed myself to in the past, at
*** least with some perl versions: I simply can't keep up working around new
*** bugs or gratituous incompatibilities, and in turn you might suffer from
*** unanticipated problems.
***
*** Therefore I have introduced a support and compatibility check, the results
*** of which follow below, together with a FAQ and some recommendations.
***
*** This check is just to let you know that there might be a risk, so you can
*** make judgement calls on how to proceed - it will not keep the module from
*** installing or working.
***
EOF

   if ($minvers > $VERSION) {
      sgr 33;
      print <<EOF;
*** The stability canary says: (nothing, it died of old age).
***
*** Your Canary::Stability module (used by $distname) is too old.
*** This is not a fatal problem - while you might want to upgrade to version
*** $minvers (currently installed version: $VERSION) to get better support
*** status testing, you might also not want to care at all, and all will
*** be well as long $distname works well enough for you, as the stability
*** canary is only used when installing the distribution.
***
EOF
   } elsif ($] < $minperl) {

      sgr 33;
      print <<EOF;
*** The stability canary says: chirp (it seems concerned about something).
***
*** Your perl version ($]) is older than the $distname distribution
*** likes ($minperl). This is not a fatal problem - the module might work
*** well with your version of perl, but it does mean the author likely
*** won't do anything to make it work if it breaks.
***
EOF

      if ($ENV{AUTOMATED_TESTING}) {
         print <<EOF;
*** Since this is an AUTOMATED_TESTING environment, the stability canary
*** decided to fail cleanly here, rather than to generate a false test
*** result.
***
EOF
         exit 0;
      }

   } elsif (defined $Internals::StabilityBranchVersion) {
      # note to people studying this modules sources:
      # the above test is not considered a clean or stable way to
      # test for the stability branch.

      sgr 32;
      print <<EOF;
*** The stability canary says: chirp! chirp! (it seems to be quite excited)
***
*** It seems you are running schmorp's stability branch of perl.
*** All should be well, and if it isn't, you should report this as a bug
*** to the $distname author.
***
EOF
   } elsif ($] < 5.021) {
      #sgr 32;
      print <<EOF;
*** The stability canary says: chirp! chirp! (it seems to be quite happy)
***
*** Your version of perl ($]) is quite supported by $distname, nothing
*** else to be said, hope it comes in handy.
***
EOF
   } else {
      sgr 31;
      print <<EOF;
*** The stability canary says: (nothing, it was driven away by harsh weather)
***
*** It seems you are running perl version $], likely the "official" or
*** "standard" version. While there is nothing wrong with doing that,
*** standard perl versions 5.022 and up are not supported by $distname.
*** While this might be fatal, it might also be all right - if you run into
*** problems, you might want to downgrade your perl or switch to the
*** stability branch.
***
*** If everything works fine, you can ignore this message.
***
EOF
      sgr 0;
      print <<EOF;
***
*** Stability canary mini-FAQ:
***
*** Do I need to do anything?
***    With luck, no. While some distributions are known to fail
***    already, most should probably work. This message is here
***    to alert you that your perl is not supported by $distname,
***    and if things go wrong, you either need to downgrade, or
***    sidegrade to the stability variant of your perl version,
***    or simply live with the consequences.
***
*** What is this canary thing?
***    It's purpose is to check support status of $distname with
***    respect to your perl version.
***
*** What is this "stability branch"?
***    It's a branch or fork of the official perl, by schmorp, to
***    improve stability and compatibility with existing modules.
***
*** How can I skip this prompt on automated installs?
***    Set PERL_CANARY_STABILITY_NOPROMPT=1 in your environment.
***    More info is in the Canary::Stability manpage.
***
*** Long version of this FAQ: http://stableperl.schmorp.de/faq.html
*** Stability Branch homepage: http://stableperl.schmorp.de/
***

EOF

      unless ($ENV{PERL_CANARY_STABILITY_NOPROMPT}) {
         require ExtUtils::MakeMaker;

         ExtUtils::MakeMaker::prompt ("Continue anyways? ", "y") =~ /^y/i
            or die "FATAL: User aborted configuration of $distname.\n";
      }
   }

   sgr 0;
}

=head1 ENVIRONMENT VARIABLES

=over 4

=item C<PERL_CANARY_STABILITY_NOPROMPT=1>

Do not prompt the user on alert messages.

=item C<PERL_CANARY_STABILITY_COLOUR=0>

Disable use of colour.

=item C<PERL_CANARY_STABILITY_COLOUR=1>

Force use of colour.

=item C<PERL_CANARY_STABILITY_DISABLE=1>

Disable this modules functionality completely.

=item C<AUTOMATED_TESTING=1>

When this variable is set to a true value and the perl minimum version
requirement is not met, the module will exit, which should skip testing
under automated testing environments.

This is done to avoid false failure or success reports when the chances of
success are already quite low and the failures are not supported by the
author.

=back

=head1 AUTHOR

 Marc Lehmann <schmorp@schmorp.de>
 http://software.schmorp.de/pkg/Canary-Stability.html

=cut

1

