package DateTime::Locale;

use strict;
use warnings;

use 5.006;

# Loading this here isn't necessary, but it makes it easier to catch
# syntax errors when testing.
use DateTime::Locale::Base;
use DateTime::Locale::Catalog;
use Params::Validate qw( validate validate_pos SCALAR );

our $VERSION = '0.45';

my %Class;
my %DataForID;
my %NameToID;
my %NativeNameToID;
my %AliasToID;
my %IDToExtra;

my %LoadCache;

sub register {
    my $class = shift;

    %LoadCache = ();

    if ( ref $_[0] ) {
        $class->_register(%$_) foreach @_;
    }
    else {
        $class->_register(@_);
    }
}

sub _register {
    my $class = shift;

    my %p = validate(
        @_, {
            id => { type => SCALAR },

            en_language  => { type => SCALAR },
            en_script    => { type => SCALAR, optional => 1 },
            en_territory => { type => SCALAR, optional => 1 },
            en_variant   => { type => SCALAR, optional => 1 },

            native_language  => { type => SCALAR, optional => 1 },
            native_script    => { type => SCALAR, optional => 1 },
            native_territory => { type => SCALAR, optional => 1 },
            native_variant   => { type => SCALAR, optional => 1 },

            class   => { type => SCALAR, optional => 1 },
            replace => { type => SCALAR, default  => 0 },
        }
    );

    my $id = $p{id};

    die "'\@' or '=' are not allowed in locale ids"
        if $id =~ /[\@=]/;

    die
        "You cannot replace an existing locale ('$id') unless you also specify the 'replace' parameter as true\n"
        if !delete $p{replace} && exists $DataForID{$id};

    $p{native_language} = $p{en_language}
        unless exists $p{native_language};

    my @en_pieces;
    my @native_pieces;
    foreach my $p (qw( language script territory variant )) {
        push @en_pieces,     $p{"en_$p"}     if exists $p{"en_$p"};
        push @native_pieces, $p{"native_$p"} if exists $p{"native_$p"};
    }

    $p{en_complete_name}     = join ' ', @en_pieces;
    $p{native_complete_name} = join ' ', @native_pieces;

    $DataForID{$id} = \%p;

    $NameToID{ $p{en_complete_name} }           = $id;
    $NativeNameToID{ $p{native_complete_name} } = $id;

    $Class{$id} = $p{class} if defined exists $p{class};
}

sub _registered_id {
    shift;
    my ($id) = validate_pos( @_, { type => SCALAR } );

    return 1 if $AliasToID{$id};
    return 1 if $DataForID{$id};

    return 0;
}

sub add_aliases {
    shift;

    %LoadCache = ();

    my $aliases = ref $_[0] ? $_[0] : {@_};

    while ( my ( $alias, $id ) = each %$aliases ) {
        die
            "Unregistered locale '$id' cannot be used as an alias target for $alias"
            unless __PACKAGE__->_registered_id($id);

        die "Can't alias an id to itself"
            if $alias eq $id;

        # check for overwrite?

        my %seen = ( $alias => 1, $id => 1 );
        my $copy = $id;
        while ( $copy = $AliasToID{$copy} ) {
            die "Creating an alias from $alias to $id would create a loop.\n"
                if $seen{$copy};

            $seen{$copy} = 1;
        }

        $AliasToID{$alias} = $id;
    }
}

sub remove_alias {
    shift;

    %LoadCache = ();

    my ($alias) = validate_pos( @_, { type => SCALAR } );

    return delete $AliasToID{$alias};
}

BEGIN {
    __PACKAGE__->register( DateTime::Locale::Catalog->Locales() );
    __PACKAGE__->add_aliases( DateTime::Locale::Catalog->Aliases() );
}

sub ids   { wantarray ? keys %DataForID : [ keys %DataForID ] }
sub names { wantarray ? keys %NameToID  : [ keys %NameToID ] }

sub native_names {
    wantarray ? keys %NativeNameToID : [ keys %NativeNameToID ];
}

# These are hardcoded for backwards comaptibility with the
# DateTime::Language code.
my %OldAliases = (
    'Afar'      => 'aa',
    'Amharic'   => 'am_ET',
    'Austrian'  => 'de_AT',
    'Brazilian' => 'pt_BR',
    'Czech'     => 'cs_CZ',
    'Danish'    => 'da_DK',
    'Dutch'     => 'nl_NL',
    'English'   => 'en_US',
    'French'    => 'fr_FR',

    #      'Gedeo'             => undef, # XXX
    'German'            => 'de_DE',
    'Italian'           => 'it_IT',
    'Norwegian'         => 'no_NO',
    'Oromo'             => 'om_ET',    # Maybe om_KE or plain om ?
    'Portugese'         => 'pt_PT',
    'Sidama'            => 'sid',
    'Somali'            => 'so_SO',
    'Spanish'           => 'es_ES',
    'Swedish'           => 'sv_SE',
    'Tigre'             => 'tig',
    'TigrinyaEthiopian' => 'ti_ET',
    'TigrinyaEritrean'  => 'ti_ER',
);

sub load {
    my $class = shift;
    my ($name) = validate_pos( @_, { type => SCALAR } );

    # Support RFC 3066 language tags, which use '-' instead of '_'.
    $name =~ tr/-/_/;

    my $key = $name;

    return $LoadCache{$key} if exists $LoadCache{$key};

    # Custom class registered by user
    if ( $Class{$name} ) {
        return $LoadCache{$key}
            = $class->_load_class_from_id( $name, $Class{$name} );
    }

    # special case for backwards compatibility with DT::Language
    $name = $OldAliases{$name} if exists $OldAliases{$name};

    if ( exists $DataForID{$name} || exists $AliasToID{$name} ) {
        return $LoadCache{$key} = $class->_load_class_from_id($name);
    }

    foreach my $h ( \%NameToID, \%NativeNameToID ) {
        return $LoadCache{$key} = $class->_load_class_from_id( $h->{$name} )
            if exists $h->{$name};
    }

    if ( my $id = $class->_guess_id($name) ) {
        return $LoadCache{$key} = $class->_load_class_from_id($id);
    }

    die "Invalid locale name or id: $name\n";
}

sub _guess_id {
    my $class = shift;
    my $name  = shift;

    # Strip off charset for LC_* ids : en_GB.UTF-8 etc
    $name =~ s/\..*$//;

    my ( $language, $script, $territory, $variant ) = _parse_id($name);

    my @guesses;

    if ( defined $script ) {
        my $guess = join '_', lc $language, ucfirst lc $script;

        push @guesses, $guess;

        $guess .= '_' . uc $territory if defined $territory;

        # version with script comes first
        unshift @guesses, $guess;
    }

    if ( defined $variant ) {
        push @guesses, join '_', lc $language, uc $territory, uc $variant;
    }

    if ( defined $territory ) {
        push @guesses, join '_', lc $language, uc $territory;
    }

    push @guesses, lc $language;

    foreach my $id (@guesses) {
        return $id
            if exists $DataForID{$id} || exists $AliasToID{$id};
    }
}

sub _parse_id {
    $_[0] =~ /([a-z]+)               # id
              (?: _([A-Z][a-z]+) )?  # script - Title Case - optional
              (?: _([A-Z]+) )?       # territory - ALL CAPS - optional
              (?: _([A-Z]+) )?       # variant - ALL CAPS - optional
             /x;

    return $1, $2, $3, $4;
}

sub _load_class_from_id {
    my $class      = shift;
    my $id         = shift;
    my $real_class = shift;

    # We want the first alias for which there is data, even if it has
    # no corresponding .pm file.  There may be multiple levels of
    # alias to go through.
    my $data_id = $id;
    while ( exists $AliasToID{$data_id} && !exists $DataForID{$data_id} ) {
        $data_id = $AliasToID{$data_id};
    }

    $real_class ||= "DateTime::Locale::$data_id";

    unless ( $real_class->can('new') ) {
        eval "require $real_class";

        die $@ if $@;
    }

    my $locale = $real_class->new(
        %{ $DataForID{$data_id} },
        id => $id,
    );

    return $locale if $DateTime::Locale::InGenerator;

    if ( $locale->can('cldr_version') ) {
        my $object_version  = $locale->cldr_version();
        my $catalog_version = DateTime::Locale::Catalog->CLDRVersion();

        if ( $object_version ne $catalog_version ) {
            warn
                "Loaded $real_class, which is from an older version ($object_version)"
                . "of the CLDR database than this installation of"
                . "DateTime::Locale ($catalog_version).\n";
        }
    }

    return $locale;
}

1;

__END__

=pod

=encoding utf8

=head1 NAME

DateTime::Locale - Localization support for DateTime.pm

=head1 SYNOPSIS

  use DateTime::Locale;

  my $loc = DateTime::Locale->load('en_GB');

  print $loc->native_locale_name(),   "\n",
	$loc->datetime_format_long(), "\n";

  # but mostly just things like ...

  my $dt = DateTime->now( locale => 'fr' );
  print "Aujourd'hui le mois est " . $dt->month_name(), "\n";

=head1 DESCRIPTION

DateTime::Locale is primarily a factory for the various locale
subclasses. It also provides some functions for getting information on
all the available locales.

If you want to know what methods are available for locale objects,
then please read the C<DateTime::Locale::Base> documentation.

=head1 USAGE

This module provides the following class methods:

=head2 DateTime::Locale->load( $locale_id | $locale_name | $alias )

Returns the locale object for the specified locale id, name, or alias
- see the C<DateTime::Locale::Catalog> documentation for a list of
built in names and ids. The name provided may be either the English
or native name.

If the requested locale is not found, a fallback search takes place to
find a suitable replacement.

The fallback search order is:

  {language}_{script}_{territory}
  {language}_{script}
  {language}_{territory}_{variant}
  {language}_{territory}
  {language}

Eg. For locale C<es_XX_UNKNOWN> the fallback search would be:

  es_XX_UNKNOWN   # Fails - no such locale
  es_XX           # Fails - no such locale
  es              # Found - the es locale is returned as the
                  # closest match to the requested id

Eg. For locale C<es_Latn_XX> the fallback search would be:

  es_Latn_XX      # Fails - no such locale
  es_Latn         # Fails - no such locale
  es_XX           # Fails - no such locale
  es              # Found - the es locale is returned as the
                  # closest match to the requested id

If no suitable replacement is found, then an exception is thrown.

Please note that if you provide an B<id> to this method, then the
returned locale object's C<id()> method will B<always> return the
value you gave, even if that value was an alias to some other id.

This is done for forwards compatibility, in case something that is
currently an alias becomes a unique locale in the future.

This means that the value of C<< $locale->id() >> and the object's
class may not match.

The loaded locale is cached, so that B<locale objects may be
singletons>. Calling C<< DateTime::Locale->register() >>, C<<
DateTime::Locale->add_aliases() >>, or C<<
DateTime::Locale->remove_alias() >> clears the cache.

=head2 DateTime::Locale->ids()

  my @ids = DateTime::Locale->ids();
  my $ids = DateTime::Locale->ids();

Returns an unsorted list of the available locale ids, or an array
reference if called in a scalar context. This list does not include
aliases.

=head2 DateTime::Locale->names()

  my @names = DateTime::Locale->names();
  my $names = DateTime::Locale->names();

Returns an unsorted list of the available locale names in English, or
an array reference if called in a scalar context.

=head2 DateTime::Locale->native_names()

  my @names = DateTime::Locale->native_names();
  my $names = DateTime::Locale->native_names();

Returns an unsorted list of the available locale names in their native
language, or an array reference if called in a scalar context. All
native names are utf8 encoded.

B<NB>: Some locales are only partially translated, so their native locale
names may still contain some English.

=head2 DateTime::Locale->add_aliases ( $alias1 => $id1, $alias2 => $id2, ... )

Adds an alias to an existing locale id. This allows a locale to be
loaded by its alias rather than id or name. Multiple aliases are
allowed.

If the passed locale id is neither registered nor listed in
L<DateTime::Local::Catalog>'s list of ids, an exception is thrown.

 DateTime::Locale->add_aliases( LastResort => 'es_ES' );

 # Equivalent to DateTime::Locale->load('es_ES');
 DateTime::Locale->load('LastResort');

You can also pass a hash reference to this method.

 DateTime::Locale->add_aliases( { Default     => 'en_GB',
                                  Alternative => 'en_US',
                                  LastResort  => 'es_ES' } );

=head2 DateTime::Locale->remove_alias( $alias )

Removes a locale id alias, and returns true if the specified alias
actually existed.

 DateTime::Locale->add_aliases( LastResort => 'es_ES' );

 # Equivalent to DateTime::Locale->load('es_ES');
 DateTime::Locale->load('LastResort');

 DateTime::Locale->remove_alias('LastResort');

 # Throws an exception, 'LastResort' no longer exists
 DateTime::Locale->load('LastResort');

=head2 DateTime::Locale->register( { ... }, { ... } )

This method allows you to register custom locales with the module. A
single locale is specified as a hash, and you may register multiple
locales at once by passing an array of hash references.

Until registered, custom locales cannot be instantiated via C<load()>
and will not be returned by querying methods such as C<ids()> or
C<names()>.

 register( id           => $locale_id,
           en_language  => ..., # something like 'English' or 'Afar',

           # All other keys are optional. These are:
           en_script    => ...,
           en_territory => ...,
           en_variant   => ...,

           native_language  => ...,
           native_sript     => ...,
           native_territory => ...,
           native_variant   => ...,

           # Optional - defaults to DateTime::Locale::$locale_id
           class   => $class_name,

           replace => $boolean
         )

The locale id and English name are required, and the following formats
should used wherever possible:

 id:   languageId[_script][_territoryId[_variantId]]

 Where:  languageId = Lower case ISO 639 code -
         Always choose 639-1 over 639-2 where possible.

 script = Title Case ISO 15924 script code

 territoryId = Upper case ISO 3166 code -
               Always choose 3166-1 over 3166-2 where possible.

 variantId = Upper case variant id -
             Basically anything you want, since this is typically the
             component that uniquely identifies a custom locale.

You cannot not use '@' or '=' in locale ids - these are reserved for
future use. The underscore (_) is the component separator, and should
not be used for any other purpose.

If the "native_*" components are supplied, they must be utf8 encoded.

If omitted, the native name is assumed to be identical to the English
name.

If class is supplied, it must be the full module name of your custom
locale. If omitted, the locale module is assumed to be a
DateTime::Locale subclass.

Examples:

 DateTime::Locale->register
     ( id           => 'en_GB_RIDAS',
       en_language  => 'English',
       en_territory => 'United Kingdom',
       en_variant   => 'Ridas Custom Locale',
     );

 # Returns instance of class DateTime::Locale::en_GB_RIDAS
 my $l = DateTime::Locale->load('en_GB_RIDAS');

 DateTime::Locale->register
     ( id               => 'hu_HU',
       en_language      => 'Hungarian',
       en_territory     => Hungary',
       native_language  => 'Magyar',
       native_territory => 'Magyarország',
     );

 # Returns instance of class DateTime::Locale::hu_HU
 my $l = DateTime::Locale->load('hu_HU');

 DateTime::Locale->register
     ( id    => 'en_GB_RIDAS',
       name  => 'English United Kingdom Ridas custom locale',
       class => 'Ridas::Locales::CustomGB',
     );

 # Returns instance of class Ridas::Locales::CustomGB
 my $l = DateTime::Locale->load('en_GB_RIDAS');

If you register a locale for an id that is already registered, the
"replace" parameter must be true or an exception will be thrown.

The complete name for a registered locale is generated by joining
together the language, territory, and variant components with a single
space.

This means that in the first example, the complete English and native
names for the locale would be "English United Kingdom Ridas Custom
Locale", and in the second example the complete English name is
"Hungarian Hungary", while the complete native name is "Magyar
Magyarország". The locale will be loadable by these complete names
(English and native), via the C<load()> method.

=head1 ADDING CUSTOM LOCALES

These are added in one of two ways:

=over 4

=item 1.

Subclass an existing locale implementing only the changes you require.

=item 2.

Create a completely new locale as a new class.

=back

In either case the locale MUST be registered before use.

=head2 Subclassing an existing locale

The following example sublasses the United Kingdom English locale to change
some the full date and time formats.

  package Ridas::Locale::en_GB_RIDAS1;

  use strict;
  use DateTime::Locale::en_GB;

  use base 'DateTime::Locale::en_GB';

  sub date_format_full   { 'EEEE d MMMM y' }

  sub time_format_full   { 'HH mm zzzz' }

  1;

Now register it:

 DateTime::Locale->register
     ( id    => 'en_GB_RIDAS1',

       # name, territory, and variant as described in register() documentation

       class => 'Ridas::Locale::en_GB_RIDAS1',
     );

=head2 Creating a completely new locale

You are, of course, free to subclass L<DateTime::Locale::Base> if you
want to, though this is not required.

Remember to register your custom locale!

Of course, you can always do the registration in the module itself,
and simply load it before using it.

A completely new custom locale, one which does not subclass
L<DateTime::Locale::Base>, must implement a number of methods.

The following methods can be used to get information about the
locale's id and name.

=over 4

=item * $locale->id()

The complete locale id, something like "en_US".

=item * $locale->language_id()

The language portion of the id, like "en".

=item * $locale->script_id()

The script portion of the id, like "Hant".

=item * $locale->territory_id()

The territory portion of the id, like "US".

=item * $locale->variant_id()

The variant portion of the id, like "PREEURO".

=item * $locale->name()

The locale's complete name, which always includes at least a language
component, plus optional territory and variant components. Something
like "English United States". The value returned will always be in
English.

=item * $locale->language()

=item * $locale->script()

=item * $locale->territory()

=item * $locale->variant()

The relevant component from the locale's complete name, like "English"
or "United States".

=item * $locale->native_name()

The locale's complete name in localized form as a UTF-8 string.

=item * $locale->native_language()

=item * $locale->native_script()

=item * $locale->native_territory()

=item * $locale->native_variant()

The relevant component from the locale's complete native name as a
UTF-8 string.

=back

The following methods all return an array reference containing the
specified data.

The methods with "format" in the name should return strings that can be used a
part of a string, like "the month of July". The stand alone values are for
use in things like calendars, and the narrow form may not be unique (for
example, in day column heading for a calendar it's okay to have "T" for both
Tuesday and Thursday).

The wide name should always be the full name of thing in question. The narrow
name should be just one or two characters.

=over 4

=item * $locale->month_format_wide()

=item * $locale->month_format_abbreviated()

=item * $locale->month_format_narrow()

=item * $locale->month_stand_alone_wide()

=item * $locale->month_stand_alone_abbreviated()

=item * $locale->month_stand_alone_narrow()

=item * $locale->day_format_wide()

=item * $locale->day_format_abbreviated()

=item * $locale->day_format_narrow()

=item * $locale->day_stand_alone_wide()

=item * $locale->day_stand_alone_abbreviated()

=item * $locale->day_stand_alone_narrow()

=item * $locale->quarter_format_wide()

=item * $locale->quarter_format_abbreviated()

=item * $locale->quarter_format_narrow()

=item * $locale->quarter_stand_alone_wide()

=item * $locale->quarter_stand_alone_abbreviated()

=item * $locale->quarter_stand_alone_narrow()

=item * $locale->am_pm_abbreviated()

=item * $locale->era_wide()

=item * $locale->era_abbreviated()

=item * $locale->era_narrow()

=back

The following methods return strings appropriate for the
C<< DateTime->format_cldr() >> method:

=over 4

=item * $locale->date_format_full()

=item * $locale->date_format_long()

=item * $locale->date_format_medium()

=item * $locale->date_format_short()

=item * $locale->date_format_default()

=item * $locale->time_format_full()

=item * $locale->time_format_long()

=item * $locale->time_format_medium()

=item * $locale->time_format_short()

=item * $locale->time_format_default()

=item * $locale->datetime_format_full()

=item * $locale->datetime_format_long()

=item * $locale->datetime_format_medium()

=item * $locale->datetime_format_short()

=item * $locale->datetime_format_default()

=back

A locale may also offer one or more formats for displaying part of a
datetime, such as the year and month, or hour and minute.

=over 4

=item * $locale->format_for($name)

These are accessed by passing a name to C<< $locale->format_for(...)  >>,
where the name is a CLDR-style format specifier.

The return value is a string suitable for passing to C<< $dt->format_cldr()
>>, so you can do something like this:

  print $dt->format_cldr( $dt->locale()->format_for('MMMdd') )

which for the "en" locale would print out something like "08 Jul".

Note that the localization may also include additional text specific to the
locale. For example, the "MMMMd" format for the "zh" locale includes the
Chinese characters for "day" (日) and month (月), so you get something like
"8月23日".

=item * $locale->available_formats()

This should return a list of all the format names that could be passed
to C<< $locale->format_for() >>.

=back

The following methods deal with the default format lengths:

=over 4

=item * $locale->default_date_format_length()

=item * $locale->default_time_format_length()

These methods return one of "full", "long", "medium", or "short",
indicating the current default format length.

The default when an object is created is determined by the CLDR locale
data.

=item * $locale->set_default_date_format_length($length)

=item * $locale->set_default_time_format_length($length)

These methods accept one of "full", "long", "medium", or "short",
indicating the new default format length.

=back

There are also some miscellaneous methods locales should support:

=over 4

=item * $locale->prefers_24_hour_time()

Returns a boolean indicating whether or not the locale prefers 24-hour time.

=item * $locale->first_day_of_week()

Returns a number from 1 to 7 indicating the I<local> first day of the
week, with Monday being 1 and Sunday being 7.

=back

=head1 SUPPORT

Please be aware that all locale data has been generated from the CLDR (Common
Locale Data Repository) project locales data). The data is incomplete, and
will contain errors in some locales.

When reporting errors in data, please check the primary data sources
first, then where necessary report errors directly to the primary
source via the CLDR bug report system. See
http://unicode.org/cldr/filing_bug_reports.html for details.

Once these errors have been confirmed, please forward the error report
and corrections to the DateTime mailing list, datetime@perl.org.

Support for this module is provided via the datetime@perl.org email
list. See http://lists.perl.org/ for more details.

=head1 DONATIONS

If you'd like to thank me for the work I've done on this module,
please consider making a "donation" to me via PayPal. I spend a lot of
free time creating free software, and would appreciate any support
you'd care to offer.

Please note that B<I am not suggesting that you must do this> in order
for me to continue working on this particular software. I will
continue to do so, inasmuch as I have in the past, for as long as it
interests me.

Similarly, a donation made in this way will probably not make me work
on this software much more, unless I get so many donations that I can
consider working on free software full time, which seems unlikely at
best.

To donate, log into PayPal and send money to autarch@urth.org or use
the button on this page:
L<http://www.urth.org/~autarch/fs-donation.html>

=head1 AUTHORS

Richard Evans <rich@ridas.com>

Dave Rolsky <autarch@urth.org>

These modules are loosely based on the DateTime::Language modules,
which were in turn based on the Date::Language modules from Graham
Barr's TimeDate distribution.

=head1 COPYRIGHT

Copyright (c) 2003 Richard Evans. Copyright (c) 2004-2009 David
Rolsky. All rights reserved. This program is free software; you can
redistribute it and/or modify it under the same terms as Perl itself.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

The full text of the license can be found in the F<LICENSE> file included
with this module.

The locale modules in directory F<DateTime/Locale/> have been
generated from data provided by the CLDR project, see
F<DateTime/Locale/LICENSE.cldr> for details on the CLDR data's
license.

=head1 SEE ALSO

L<DateTime::Locale::Base>

datetime@perl.org mailing list

http://datetime.perl.org/

=cut
