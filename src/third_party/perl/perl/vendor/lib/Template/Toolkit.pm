#============================================================= -*-perl-*-
#
# Template::Toolkit
#
# DESCRIPTION
#   Front-page for the Template Toolkit documentation
#
# AUTHOR
#   Andy Wardley  <abw@wardley.org>
#
# COPYRIGHT
#   Copyright (C) 1996-2013 Andy Wardley.  All Rights Reserved.
#
#   This module is free software; you can redistribute it and/or
#   modify it under the same terms as Perl itself.
#
#========================================================================

package Template::Toolkit;

1;

__END__

=head1 NAME

Template::Toolkit - Template Processing System

=head1 Introduction

The Template Toolkit is a collection of Perl modules which implement a
fast, flexible, powerful and extensible template processing system.

It is "input-agnostic" and can be used equally well for processing any 
kind of text documents: HTML, XML, CSS, Javascript, Perl code, plain text,
and so on.  However, it is most often used for generating static and
dynamic web content, so that's what we'll focus on here.

Although the Template Toolkit is written in Perl, you don't need to be a Perl
programmer to use it. It was designed to allow non-programmers to easily
create and maintain template-based web sites without having to mess around
writing Perl code or going crazy with cut-n-paste.

However, the Template Toolkit is also designed to be extremely flexible and
extensible. If you are a Perl programmer, or know someone who is, then you can
easily hook the Template Toolkit into your existing code, data, databases and
web applications. Furthermore, you can easily extend the Template Toolkit
through the use of its plugin mechanism and other developer APIs.

Whatever context you use it in, the primary purpose of the Template Toolkit is
to allow you to create a clear separation between the presentation elements of
your web site and everything else. 

If you're generating static web pages, then you can use it to separate the
commonly repeated user interface elements on each page (headers, menus,
footers, etc.) from the core content. If you're generating dynamic web pages
for the front end of a web application, then you'll also be using it to keep 
the back-end Perl code entirely separate from the front-end HTML templates.
Either way, a I<clear separation of concerns> is what allow you to 
concentrate on one thing at a time without the other things getting in your
way.  And that's what the Template Toolkit is all about.

=head1 Documentation

The documentation for the Template Toolkit is organised into five sections.

The L<Template::Manual> contains detailed information about using the Template
Toolkit. It gives examples of its use and includes a full reference of the
template language, configuration options, filters, plugins and other component
parts.

The L<Template::Modules> page lists the Perl modules that comprise the
Template Toolkit. It gives a brief explanation of what each of them does, and
provides a link to the complete documentation for each module for further
information. If you're a Perl programmer looking to use the Template Toolkit
from your Perl programs then this section is likely to be of interest.

Most, if not all of the information you need to call the Template Toolkit from
Perl is in the documentation for the L<Template> module. You only really need
to start thinking about the other modules if you want to extend or modify the
Template Toolkit in some way, or if you're interested in looking under the
hood to see how it all works.

The documentation for each module is embedded as POD in each
module, so you can always use C<perldoc> from the command line to read a
module's documentation.  e.g.

    $ perldoc Template
    $ perldoc Template::Context
      ...etc...

It's worth noting that all the other documentation, including the user manual
is available as POD.  e.g.

    $ perldoc Template::Manual
    $ perldoc Template::Manual::Config
      ...etc...

The L<Template::Tools> section contains the documentation for 
L<Template::Tools::tpage|tpage> and L<Template::Tools::ttree|ttree>.
These are two command line programs that are distributed with the 
Template Toolkit.  L<tpage|Template::Tools::tpage> is used to process
a single template file, L<ttree|Template::Tools::ttree> for processing
entire directories of template files.

The L<Template::Tutorial> section contains two introductory tutorials on using
the Template Toolkit. The first is L<Template::Tutorial::Web> on generating
web content. The second is L<Template::Tutorial::Datafile> on using the
Template Toolkit to generate other data formats including XML.

The final section of the manual is L<Template::FAQ> which contains answers
to some of the Frequently Asked Questions about the Template Toolkit.

You can read the documentation in HTML format either online at the Template 
Toolkit web site, L<http://template-toolkit.org/>, or by downloading the 
HTML version of the documentation from 
L<http://template-toolkit.org/download/index.html#html_docs> and unpacking
it on your local machine.

=head1 Author

The Template Toolkit was written by Andy Wardley (L<http://wardley.org/>
L<mailto:abw@wardley.org>) with assistance and contributions from a great 
number of people.  Please see L<Template::Manual::Credits> for a full list.

=head1 Copyright

Copyright (C) 1996-2013 Andy Wardley.  All Rights Reserved.

This module is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=head1 See Also

L<Template>, L<Template::Manual>, L<Template::Modules>, L<Template::Tools>,
L<Template::Tutorial>

=cut

# Local Variables:
# mode: perl
# perl-indent-level: 4
# indent-tabs-mode: nil
# End:
#
# vim: expandtab shiftwidth=4:
