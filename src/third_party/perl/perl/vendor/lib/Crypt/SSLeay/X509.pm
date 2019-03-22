package Crypt::SSLeay::X509;

use strict;

sub not_before {
    my $cert = shift;
    not_string2time($cert->get_notBeforeString);
}

sub not_after {
    my $cert = shift;
    not_string2time($cert->get_notAfterString);
}

sub not_string2time {
    my $string = shift;
    # $string has the form 021019235959Z
    my($year, $month, $day, $hour, $minute, $second, $GMT)=
        $string=~m/(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(Z)?/;
    $year += 2000;
    my $time="$year-$month-$day $hour:$minute:$second";
    $time .= " GMT" if $GMT;
    $time;
}

1;
