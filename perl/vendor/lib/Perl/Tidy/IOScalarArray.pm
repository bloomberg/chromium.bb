#####################################################################
#
# This is a stripped down version of IO::ScalarArray
# Given a reference to an array, it supplies either:
# a getline method which reads lines (mode='r'), or
# a print method which reads lines (mode='w')
#
# NOTE: this routine assumes that there aren't any embedded
# newlines within any of the array elements.  There are no checks
# for that.
#
#####################################################################
package Perl::Tidy::IOScalarArray;
use strict;
use warnings;
use Carp;
our $VERSION = '20181120';

sub new {
    my ( $package, $rarray, $mode ) = @_;
    my $ref = ref $rarray;
    if ( $ref ne 'ARRAY' ) {
        confess <<EOM;
------------------------------------------------------------------------
expecting ref to ARRAY but got ref to ($ref); trace follows:
------------------------------------------------------------------------
EOM

    }
    if ( $mode eq 'w' ) {
        @{$rarray} = ();
        return bless [ $rarray, $mode ], $package;
    }
    elsif ( $mode eq 'r' ) {
        my $i_next = 0;
        return bless [ $rarray, $mode, $i_next ], $package;
    }
    else {
        confess <<EOM;
------------------------------------------------------------------------
expecting mode = 'r' or 'w' but got mode ($mode); trace follows:
------------------------------------------------------------------------
EOM
    }
}

sub getline {
    my $self = shift;
    my $mode = $self->[1];
    if ( $mode ne 'r' ) {
        confess <<EOM;
------------------------------------------------------------------------
getline requires mode = 'r' but mode = ($mode); trace follows:
------------------------------------------------------------------------
EOM
    }
    my $i = $self->[2]++;
    return $self->[0]->[$i];
}

sub print {
    my ( $self, $msg ) = @_;
    my $mode = $self->[1];
    if ( $mode ne 'w' ) {
        confess <<EOM;
------------------------------------------------------------------------
print requires mode = 'w' but mode = ($mode); trace follows:
------------------------------------------------------------------------
EOM
    }
    push @{ $self->[0] }, $msg;
    return;
}
sub close { return }
1;

