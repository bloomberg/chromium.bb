package Crypt::OpenPGP::Key::Public;
use strict;

use Crypt::OpenPGP::Key;
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::Key Crypt::OpenPGP::ErrorHandler );

sub all_props { $_[0]->public_props }
sub is_secret { 0 }
sub public_key { $_[0] }

1;
