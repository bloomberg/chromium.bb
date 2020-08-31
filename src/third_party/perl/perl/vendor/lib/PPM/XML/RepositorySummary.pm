#
# PPM::XML::PPMConfig
#
# Definition of the PPMConfig file format; configuration options for the Perl
# Package Manager.
#
###############################################################################

###############################################################################
# Import everything from PPM::XML::PPD into our own namespace.
###############################################################################
package PPM::XML::RepositorySummary;

use PPM::XML::PPD ':elements';

###############################################################################
# RepositorySummary Element: Characters
###############################################################################
package PPM::XML::RepositorySummary::Characters;
@ISA = qw( PPM::XML::Element );

###############################################################################
# RepositorySummary Element: REPOSITORYSUMMARY
###############################################################################
package PPM::XML::RepositorySummary::REPOSITORYSUMMARY;
@ISA = qw( PPM::XML::ValidatingElement );
@okids  = qw( SOFTPKG );

__END__
