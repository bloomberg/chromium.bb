
package String::CRC32;

require Exporter;
require DynaLoader;

@ISA = qw(Exporter DynaLoader);

$VERSION = 1.4;

# Items to export into callers namespace by default
@EXPORT = qw(crc32);

# Other items we are prepared to export if requested
@EXPORT_OK = qw();

bootstrap String::CRC32;

1;
