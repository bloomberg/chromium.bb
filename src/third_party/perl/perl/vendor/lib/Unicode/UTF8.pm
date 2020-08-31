package Unicode::UTF8;

use strict;
use warnings;

BEGIN {
    our $VERSION    = '0.62';
    our @EXPORT_OK  = qw[ decode_utf8 encode_utf8 valid_utf8 ];
    our %EXPORT_TAGS = (
        all => [ @EXPORT_OK ],
    );

    require XSLoader;
    XSLoader::load(__PACKAGE__, $VERSION);

    require Exporter;
    *import = \&Exporter::import;
}

1;

