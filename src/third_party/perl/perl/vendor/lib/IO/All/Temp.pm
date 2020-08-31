use strict; use warnings;
package IO::All::Temp;

use IO::All::File -base;

sub temp {
    my $self = shift;
    bless $self, __PACKAGE__;
    my $temp_file = IO::File::new_tmpfile()
      or $self->throw("Can't create temporary file");
    $self->io_handle($temp_file);
    $self->_error_check;
    $self->autoclose(0);
    $self->is_open(1);
    return $self;
}

1;
