package Crypt::OpenPGP::Words;
use strict;

use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

## Biometric word lists as defined in manual for
## PGPFreeware for Windows 6.5.1, Appendix D

## Code based on Mike Dillon's PGPWords.pm

{
    my @WORDS;

    sub encode_hex { $_[0]->encode(pack 'H*', $_[1]) }

    sub encode {
        my $class = shift;
        my($data) = @_;
        my $toggle = 1;
        map { $WORDS[$toggle = !$toggle][$_] } unpack 'C*', $data;
    }

    @WORDS = (

        ## Two-syllable words for encoding odd bytes
        [ qw(
                aardvark       absurd         accrue         acme
                adrift         adult          afflict        ahead
                aimless        Algol          allow          alone
                ammo           ancient        apple          artist
                assume         Athens         atlas          Aztec
                baboon         backfield      backward       banjo
                beaming        bedlamp        beehive        beeswax
                befriend       Belfast        berserk        billiard
                bison          blackjack      blockade       blowtorch
                bluebird       bombast        bookshelf      brackish
                breadline      breakup        brickyard      briefcase
                Burbank        button         buzzard        cement
                chairlift      chatter        checkup        chisel
                choking        chopper        Christmas      clamshell
                classic        classroom      cleanup        clockwork
                cobra          commence       concert        cowbell
                crackdown      cranky         crowfoot       crucial
                crumpled       crusade        cubic          dashboard
                deadbolt       deckhand       dogsled        dragnet
                drainage       dreadful       drifter        dropper
                drumbeat       drunken        Dupont         dwelling
                eating         edict          egghead        eightball
                endorse        endow          enlist         erase
                escape         exceed         eyeglass       eyetooth
                facial         fallout        flagpole       flatfoot
                flytrap        fracture       framework      freedom
                frighten       gazelle        Geiger         glitter
                glucose        goggles        goldfish       gremlin
                guidance       hamlet         highchair      hockey
                indoors        indulge        inverse        involve
                island         jawbone        keyboard       kickoff
                kiwi           klaxon         locale         lockup
                merit          minnow         miser          Mohawk
                mural          music          necklace       Neptune
                newborn        nightbird      Oakland        obtuse
                offload        optic          orca           payday
                peachy         pheasant       physique       playhouse
                Pluto          preclude       prefer         preshrunk
                printer        prowler        pupil          puppy
                python         quadrant       quiver         quota
                ragtime        ratchet        rebirth        reform
                regain         reindeer       rematch        repay
                retouch        revenge        reward         rhythm
                ribcage        ringbolt       robust         rocker
                ruffled        sailboat       sawdust        scallion
                scenic         scorecard      Scotland       seabird
                select         sentence       shadow         shamrock
                showgirl       skullcap       skydive        slingshot
                slowdown       snapline       snapshot       snowcap
                snowslide      solo           southward      soybean
                spaniel        spearhead      spellbind      spheroid
                spigot         spindle        spyglass       stagehand
                stagnate       stairway       standard       stapler
                steamship      sterling       stockman       stopwatch
                stormy         sugar          surmount       suspense
                sweatband      swelter        tactics        talon
                tapeworm       tempest        tiger          tissue
                tonic          topmost        tracker        transit
                trauma         treadmill      Trojan         trouble
                tumor          tunnel         tycoon         uncut
                unearth        unwind         uproot         upset
                upshot         vapor          village        virus
                Vulcan         waffle         wallet         watchword
                wayside        willow         woodlark       Zulu
        ) ],

        ## Three-syllable words for encoding even bytes
        [ qw(
                adroitness     adviser        aftermath      aggregate
                alkali         almighty       amulet         amusement
                antenna        applicant      Apollo         armistice
                article        asteroid       Atlantic       atmosphere
                autopsy        Babylon        backwater      barbecue
                belowground    bifocals       bodyguard      bookseller
                borderline     bottomless     Bradbury       bravado
                Brazilian      breakaway      Burlington     businessman
                butterfat      Camelot        candidate      cannonball
                Capricorn      caravan        caretaker      celebrate
                cellulose      certify        chambermaid    Cherokee
                Chicago        clergyman      coherence      combustion
                commando       company        component      concurrent
                confidence     conformist     congregate     consensus
                consulting     corporate      corrosion      councilman
                crossover      crucifix       cumbersome     customer
                Dakota         decadence      December       decimal
                designing      detector       detergent      determine
                dictator       dinosaur       direction      disable
                disbelief      disruptive     distortion     document
                embezzle       enchanting     enrollment     enterprise
                equation       equipment      escapade       Eskimo
                everyday       examine        existence      exodus
                fascinate      filament       finicky        forever
                fortitude      frequency      gadgetry       Galveston
                getaway        glossary       gossamer       graduate
                gravity        guitarist      hamburger      Hamilton
                handiwork      hazardous      headwaters     hemisphere
                hesitate       hideaway       holiness       hurricane
                hydraulic      impartial      impetus        inception
                indigo         inertia        infancy        inferno
                informant      insincere      insurgent      integrate
                intention      inventive      Istanbul       Jamaica
                Jupiter        leprosy        letterhead     liberty
                maritime       matchmaker     maverick       Medusa
                megaton        microscope     microwave      midsummer
                millionaire    miracle        misnomer       molasses
                molecule       Montana        monument       mosquito
                narrative      nebula         newsletter     Norwegian
                October        Ohio           onlooker       opulent
                Orlando        outfielder     Pacific        pandemic
                Pandora        paperweight    paragon        paragraph
                paramount      passenger      pedigree       Pegasus
                penetrate      perceptive     performance    pharmacy
                phonetic       photograph     pioneer        pocketful
                politeness     positive       potato         processor
                provincial     proximate      puberty        publisher
                pyramid        quantity       racketeer      rebellion
                recipe         recover        repellent      replica
                reproduce      resistor       responsive     retraction
                retrieval      retrospect     revenue        revival
                revolver       sandalwood     sardonic       Saturday
                savagery       scavenger      sensation      sociable
                souvenir       specialist     speculate      stethoscope
                stupendous     supportive     surrender      suspicious
                sympathy       tambourine     telephone      therapist
                tobacco        tolerance      tomorrow       torpedo
                tradition      travesty       trombonist     truncated
                typewriter     ultimate       undaunted      underfoot
                unicorn        unify          universe       unravel
                upcoming       vacancy        vagabond       vertigo
                Virginia       visitor        vocalist       voyager
                warranty       Waterloo       whimsical      Wichita
                Wilmington     Wyoming        yesteryear     Yucatan
        ) ]

    );
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::Words - Create English-word encodings

=head1 SYNOPSIS

    use Crypt::OpenPGP::Words;
    my $cert = Crypt::OpenPGP::Certificate->new;
    my @words = Crypt::OpenPGP::Words->encode( $cert->fingerprint );

=head1 DESCRIPTION

I<Crypt::OpenPGP::Words> provides routines to convert either octet or
hexadecimal strings into a list of English words, using the same
algorithm and biometric word lists as used in PGP (see
I<AUTHOR & COPYRIGHTS> for source of word lists).

In PGP this is often used for creating memorable fingerprints, the idea
being that it is easier to associate a list of words with one's key
than a string of hex digits. See the I<fingerprint_words> method in
I<Crypt::OpenPGP::Certificate> for an interface to word fingerprints.

=head1 USAGE

=head2 Crypt::OpenPGP::Words->encode( $octet_str )

Given an octet string I<$octet_str>, encodes that string into a list of
English words.

The encoding is performed by splitting the string into octets; the list
of octets is then iterated over. There are two lists of words, 256 words
each. Two-syllable words are used for encoding odd iterations through
the loop; three-syllable words for even iterations. The word list is
formed by treating each octet as an index into the appropriate word list
(two- or three-syllable), then adding the word at that index to the list.

Returns the list of words.

=head2 Crypt::OpenPGP::Words->encode_hex( $hex_str )

Performs the exact same encoding as I<encode>; I<$hex_str>, a string
of hexadecimal digits, is first transformed into a string of octets,
then passed to I<encode>.

Returns the list of words.

=head1 AUTHOR & COPYRIGHTS

Based on PGPWords.pm by Mike Dillon. Biometric word lists as defined in
manual for PGPFreeware for Windows 6.5.1, Appendix D

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
