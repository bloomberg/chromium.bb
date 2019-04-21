package DateTime::TimeZone::Local::Win32;
{
  $DateTime::TimeZone::Local::Win32::VERSION = '1.46';
}

use strict;
use warnings;

use parent 'DateTime::TimeZone::Local';

use Win32::TieRegistry ( 'KEY_READ', Delimiter => q{/} );

sub Methods { return qw( FromEnv FromRegistry ) }

sub EnvVars { return 'TZ' }

{
    # This list comes (mostly) in the zipball for the Chronos project
    # - a Smalltalk datetime library. Thanks, Chronos!
    my %WinToOlson = (
        'Afghanistan'                     => 'Asia/Kabul',
        'Afghanistan Standard Time'       => 'Asia/Kabul',
        'Alaskan'                         => 'America/Anchorage',
        'Alaskan Standard Time'           => 'America/Anchorage',
        'Arab'                            => 'Asia/Riyadh',
        'Arab Standard Time'              => 'Asia/Riyadh',
        'Arabian'                         => 'Asia/Muscat',
        'Arabian Standard Time'           => 'Asia/Muscat',
        'Arabic Standard Time'            => 'Asia/Baghdad',
        'Argentina Standard Time'         => 'America/Argentina/Buenos_Aires',
        'Armenian Standard Time'          => 'Asia/Yerevan',
        'Atlantic'                        => 'America/Halifax',
        'Atlantic Standard Time'          => 'America/Halifax',
        'AUS Central'                     => 'Australia/Darwin',
        'AUS Central Standard Time'       => 'Australia/Darwin',
        'AUS Eastern'                     => 'Australia/Sydney',
        'AUS Eastern Standard Time'       => 'Australia/Sydney',
        'Azerbaijan Standard Time'        => 'Asia/Baku',
        'Azores'                          => 'Atlantic/Azores',
        'Azores Standard Time'            => 'Atlantic/Azores',
        'Bangkok'                         => 'Asia/Bangkok',
        'Bangkok Standard Time'           => 'Asia/Bangkok',
        'Bangladesh Standard Time'        => 'Asia/Dhaka',
        'Beijing'                         => 'Asia/Shanghai',
        'Canada Central'                  => 'America/Regina',
        'Canada Central Standard Time'    => 'America/Regina',
        'Cape Verde Standard Time'        => 'Atlantic/Cape_Verde',
        'Caucasus'                        => 'Asia/Yerevan',
        'Caucasus Standard Time'          => 'Asia/Yerevan',
        'Cen. Australia'                  => 'Australia/Adelaide',
        'Cen. Australia Standard Time'    => 'Australia/Adelaide',
        'Central'                         => 'America/Chicago',
        'Central America Standard Time'   => 'America/Regina',
        'Central Asia'                    => 'Asia/Almaty',
        'Central Asia Standard Time'      => 'Asia/Almaty',
        'Central Brazilian Standard Time' => 'America/Cuiaba',
        'Central Europe'                  => 'Europe/Prague',
        'Central Europe Standard Time'    => 'Europe/Prague',
        'Central European'                => 'Europe/Belgrade',
        'Central European Standard Time'  => 'Europe/Belgrade',
        'Central Pacific'                 => 'Pacific/Guadalcanal',
        'Central Pacific Standard Time'   => 'Pacific/Guadalcanal',
        'Central Standard Time'           => 'America/Chicago',
        'Central Standard Time (Mexico)'  => 'America/Mexico_City',
        'China'                           => 'Asia/Shanghai',
        'China Standard Time'             => 'Asia/Shanghai',
        'Dateline'                        => '-1200',
        'Dateline Standard Time'          => '-1200',
        'E. Africa'                       => 'Africa/Nairobi',
        'E. Africa Standard Time'         => 'Africa/Nairobi',
        'E. Australia'                    => 'Australia/Brisbane',
        'E. Australia Standard Time'      => 'Australia/Brisbane',
        'E. Europe'                       => 'Europe/Minsk',
        'E. Europe Standard Time'         => 'Europe/Minsk',
        'E. South America'                => 'America/Sao_Paulo',
        'E. South America Standard Time'  => 'America/Sao_Paulo',
        'Eastern'                         => 'America/New_York',
        'Eastern Standard Time'           => 'America/New_York',
        'Egypt'                           => 'Africa/Cairo',
        'Egypt Standard Time'             => 'Africa/Cairo',
        'Ekaterinburg'                    => 'Asia/Yekaterinburg',
        'Ekaterinburg Standard Time'      => 'Asia/Yekaterinburg',
        'Fiji'                            => 'Pacific/Fiji',
        'Fiji Standard Time'              => 'Pacific/Fiji',
        'FLE'                             => 'Europe/Helsinki',
        'FLE Standard Time'               => 'Europe/Helsinki',
        'Georgian Standard Time'          => 'Asia/Tbilisi',
        'GFT'                             => 'Europe/Athens',
        'GFT Standard Time'               => 'Europe/Athens',
        'GMT'                             => 'Europe/London',
        'GMT Standard Time'               => 'Europe/London',
        'Greenland Standard Time'         => 'America/Godthab',
        'Greenwich'                       => 'GMT',
        'Greenwich Standard Time'         => 'GMT',
        'GTB'                             => 'Europe/Athens',
        'GTB Standard Time'               => 'Europe/Athens',
        'Hawaiian'                        => 'Pacific/Honolulu',
        'Hawaiian Standard Time'          => 'Pacific/Honolulu',
        'India'                           => 'Asia/Calcutta',
        'India Standard Time'             => 'Asia/Calcutta',
        'Iran'                            => 'Asia/Tehran',
        'Iran Standard Time'              => 'Asia/Tehran',
        'Israel'                          => 'Asia/Jerusalem',
        'Israel Standard Time'            => 'Asia/Jerusalem',
        'Jordan Standard Time'            => 'Asia/Amman',
        'Kamchatka Standard Time'         => 'Asia/Kamchatka',
        'Korea'                           => 'Asia/Seoul',
        'Korea Standard Time'             => 'Asia/Seoul',
        'Magadan Standard Time'           => 'Asia/Magadan',
        'Mauritius Standard Time'         => 'Indian/Mauritius',
        'Mexico'                          => 'America/Mexico_City',
        'Mexico Standard Time'            => 'America/Mexico_City',
        'Mexico Standard Time 2'          => 'America/Chihuahua',
        'Mid-Atlantic'                    => 'Atlantic/South_Georgia',
        'Mid-Atlantic Standard Time'      => 'Atlantic/South_Georgia',
        'Middle East Standard Time'       => 'Asia/Beirut',
        'Montevideo Standard Time'        => 'America/Montevideo',
        'Morocco Standard Time'           => 'Africa/Casablanca',
        'Mountain'                        => 'America/Denver',
        'Mountain Standard Time'          => 'America/Denver',
        'Mountain Standard Time (Mexico)' => 'America/Chihuahua',
        'Myanmar Standard Time'           => 'Asia/Rangoon',
        'N. Central Asia Standard Time'   => 'Asia/Novosibirsk',
        'Namibia Standard Time'           => 'Africa/Windhoek',
        'Nepal Standard Time'             => 'Asia/Katmandu',
        'New Zealand'                     => 'Pacific/Auckland',
        'New Zealand Standard Time'       => 'Pacific/Auckland',
        'Newfoundland'                    => 'America/St_Johns',
        'Newfoundland Standard Time'      => 'America/St_Johns',
        'North Asia East Standard Time'   => 'Asia/Irkutsk',
        'North Asia Standard Time'        => 'Asia/Krasnoyarsk',
        'Pacific'                         => 'America/Los_Angeles',
        'Pacific SA'                      => 'America/Santiago',
        'Pacific SA Standard Time'        => 'America/Santiago',
        'Pacific Standard Time'           => 'America/Los_Angeles',
        'Pacific Standard Time (Mexico)'  => 'America/Tijuana',
        'Pakistan Standard Time'          => 'Asia/Karachi',
        'Paraguay Standard Time'          => 'America/Asuncion',
        'Prague Bratislava'               => 'Europe/Prague',
        'Romance'                         => 'Europe/Paris',
        'Romance Standard Time'           => 'Europe/Paris',
        'Russian'                         => 'Europe/Moscow',
        'Russian Standard Time'           => 'Europe/Moscow',
        'SA Eastern'                      => 'America/Cayenne',
        'SA Eastern Standard Time'        => 'America/Cayenne',
        'SA Pacific'                      => 'America/Bogota',
        'SA Pacific Standard Time'        => 'America/Bogota',
        'SA Western'                      => 'America/Guyana',
        'SA Western Standard Time'        => 'America/Guyana',
        'Samoa'                           => 'Pacific/Apia',
        'Samoa Standard Time'             => 'Pacific/Apia',
        'Saudi Arabia'                    => 'Asia/Riyadh',
        'Saudi Arabia Standard Time'      => 'Asia/Riyadh',
        'SE Asia'                         => 'Asia/Bangkok',
        'SE Asia Standard Time'           => 'Asia/Bangkok',
        'Singapore'                       => 'Asia/Singapore',
        'Singapore Standard Time'         => 'Asia/Singapore',
        'South Africa'                    => 'Africa/Harare',
        'South Africa Standard Time'      => 'Africa/Harare',
        'Sri Lanka'                       => 'Asia/Colombo',
        'Sri Lanka Standard Time'         => 'Asia/Colombo',
        'Syria Standard Time'             => 'Asia/Damascus',
        'Sydney Standard Time'            => 'Australia/Sydney',
        'Taipei'                          => 'Asia/Taipei',
        'Taipei Standard Time'            => 'Asia/Taipei',
        'Tasmania'                        => 'Australia/Hobart',
        'Tasmania Standard Time'          => 'Australia/Hobart',
        'Tokyo'                           => 'Asia/Tokyo',
        'Tokyo Standard Time'             => 'Asia/Tokyo',
        'Tonga Standard Time'             => 'Pacific/Tongatapu',
        'Ulaanbaatar Standard Time'       => 'Asia/Ulaanbaatar',
        'US Eastern'                      => 'America/Indianapolis',
        'US Eastern Standard Time'        => 'America/Indianapolis',
        'US Mountain'                     => 'America/Phoenix',
        'US Mountain Standard Time'       => 'America/Phoenix',
        'UTC'                             => 'UTC',
        'UTC+12'                          => '+1200',
        'UTC-02'                          => '-0200',
        'UTC-11'                          => '-1100',
        'Venezuela Standard Time'         => 'America/Caracas',
        'Vladivostok'                     => 'Asia/Vladivostok',
        'Vladivostok Standard Time'       => 'Asia/Vladivostok',
        'W. Australia'                    => 'Australia/Perth',
        'W. Australia Standard Time'      => 'Australia/Perth',
        'W. Central Africa Standard Time' => 'Africa/Luanda',
        'W. Europe'                       => 'Europe/Berlin',
        'W. Europe Standard Time'         => 'Europe/Berlin',
        'Warsaw'                          => 'Europe/Warsaw',
        'West Asia'                       => 'Asia/Karachi',
        'West Asia Standard Time'         => 'Asia/Karachi',
        'West Pacific'                    => 'Pacific/Guam',
        'West Pacific Standard Time'      => 'Pacific/Guam',
        'Western Brazilian Standard Time' => 'America/Rio_Branco',
        'Yakutsk'                         => 'Asia/Yakutsk',
        'Yakutsk Standard Time'           => 'Asia/Yakutsk',
    );

    sub FromRegistry {
        my $class = shift;

        my $win_name = $class->_FindWindowsTZName();

        # On Windows 2008 Server, there is additional junk after a
        # null character.
        $win_name =~ s/\0.*$//
            if defined $win_name;

        return unless defined $win_name;

        my $olson = $WinToOlson{$win_name};

        return unless $olson;

        return unless $class->_IsValidName($olson);

        local $@;
        local $SIG{__DIE__};
        return eval { DateTime::TimeZone->new( name => $olson ) };
    }
}

sub _FindWindowsTZName {
    my $class = shift;

    my $LMachine = $Registry->Open( 'LMachine/', { Access => KEY_READ } );

    my $TimeZoneInfo = $LMachine->{
        'SYSTEM/CurrentControlSet/Control/TimeZoneInformation/'};

    # Windows Vista, Windows 2008 Server
    if ( defined $TimeZoneInfo->{'/TimeZoneKeyName'}
        && $TimeZoneInfo->{'/TimeZoneKeyName'} ne '' ) {
        return $TimeZoneInfo->{'/TimeZoneKeyName'};
    }
    else {
        my $AllTimeZones = $LMachine->{
            'SOFTWARE/Microsoft/Windows NT/CurrentVersion/Time Zones/'}

            # Windows NT, Windows 2000, Windows XP, Windows 2003 Server
            ? $LMachine->{
            'SOFTWARE/Microsoft/Windows NT/CurrentVersion/Time Zones/'}

            # Windows 95, Windows 98, Windows Millenium Edition
            : $LMachine->{
            'SOFTWARE/Microsoft/Windows/CurrentVersion/Time Zones/'};

        foreach my $zone ( $AllTimeZones->SubKeyNames() ) {
            if ( $AllTimeZones->{ $zone . '/Std' } eq
                $TimeZoneInfo->{'/StandardName'} ) {
                return $zone;
            }
        }
    }

    return;
}

1;

# ABSTRACT: Determine the local system's time zone on Windows



=pod

=head1 NAME

DateTime::TimeZone::Local::Win32 - Determine the local system's time zone on Windows

=head1 VERSION

version 1.46

=head1 SYNOPSIS

  my $tz = DateTime::TimeZone->new( name => 'local' );

  my $tz = DateTime::TimeZone::Local->TimeZone();

=head1 DESCRIPTION

This module provides methods for determining the local time zone on a
Windows platform.

=head1 HOW THE TIME ZONE IS DETERMINED

This class tries the following methods of determining the local time
zone:

=over 4

=item * $ENV{TZ}

It checks C<< $ENV{TZ} >> for a valid time zone name.

=item * Windows Registry

When using the registry, we look for the Windows time zone and use a
mapping to translate this to an Olson time zone name.

=over 8

=item * Windows Vista and 2008

We look in "SYSTEM/CurrentControlSet/Control/TimeZoneInformation/" for
a node named "/TimeZoneKeyName". If this exists, we use this key to
look up the Olson time zone name in our mapping.

=item * Windows NT, Windows 2000, Windows XP, Windows 2003 Server

We look in "SOFTWARE/Microsoft/Windows NT/CurrentVersion/Time Zones/"
and loop through all of its sub keys.

For each sub key, we compare the value of the key with "/Std" appended
to the end to the value of
"SYSTEM/CurrentControlSet/Control/TimeZoneInformation/StandardName". This
gives us the I<English> name of the Windows time zone, which we use to
look up the Olson time zone name.

=item * Windows 95, Windows 98, Windows Millenium Edition

The algorithm is the same as for NT, but we loop through the sub keys
of "SOFTWARE/Microsoft/Windows/CurrentVersion/Time Zones/"

=back

=back

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2012 by Dave Rolsky.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut


__END__

