package Email::MIME::Kit;
# ABSTRACT: build messages from templates
$Email::MIME::Kit::VERSION = '3.000006';
require 5.008;
use Moose 0.65; # maybe_type
use Moose::Util::TypeConstraints;

use Email::MIME 1.930; # header_raw
use Email::MessageID 1.400; # for in_brackets method
use Module::Runtime ();
use String::RewritePrefix;

#pod =head1 SYNOPSIS
#pod
#pod   use Email::MIME::Kit;
#pod
#pod   my $kit = Email::MIME::Kit->new({ source => 'mkits/sample.mkit' });
#pod
#pod   my $email = $kit->assemble({
#pod     account           => $new_signup,
#pod     verification_code => $token,
#pod     ... and any other template vars ...
#pod   });
#pod
#pod   $transport->send($email, { ... });
#pod
#pod =head1 DESCRIPTION
#pod
#pod Email::MIME::Kit is a templating system for email messages.  Instead of trying
#pod to be yet another templating system for chunks of text, it makes it easy to
#pod build complete email messages.
#pod
#pod It handles the construction of multipart messages, text and HTML alternatives,
#pod attachments, interpart linking, string encoding, and parameter validation.
#pod
#pod Although nearly every part of Email::MIME::Kit is a replaceable component, the
#pod stock configuration is probably enough for most use.  A message kit will be
#pod stored as a directory that might look like this:
#pod
#pod   sample.mkit/
#pod     manifest.json
#pod     body.txt
#pod     body.html
#pod     logo.jpg
#pod
#pod The manifest file tells Email::MIME::Kit how to put it all together, and might
#pod look something like this:
#pod
#pod   {
#pod     "renderer": "TT",
#pod     "header": [
#pod       { "From": "WY Corp <noreplies@wy.example.com>" },
#pod       { "Subject": "Welcome aboard, [% recruit.name %]!" }
#pod     ],
#pod     "alternatives": [
#pod       { "type": "text/plain", "path": "body.txt" },
#pod       {
#pod         "type": "text/html",
#pod         "path": "body.html",
#pod         "container_type": "multipart/related",
#pod         "attachments": [ { "type": "image/jpeg", "path": "logo.jpg" } ]
#pod       }
#pod     ]
#pod   }
#pod
#pod B<Inline images> may be accessed with the function C<cid_for>, for example to include the above logo.jpg:
#pod
#pod     <img style="margin: 0 auto" src="cid:[% cid_for("logo.jpg") %]">
#pod
#pod B<Please note:> the assembly of HTML documents as multipart/related bodies may
#pod be simplified with an alternate assembler in the future.
#pod
#pod The above manifest would build a multipart alternative message.  GUI mail
#pod clients would see a rendered HTML document with the logo graphic visible from
#pod the attachment.  Text mail clients would see the plaintext.
#pod
#pod Both the HTML and text parts would be rendered using the named renderer, which
#pod here is Template-Toolkit.
#pod
#pod The message would be assembled and returned as an Email::MIME object, just as
#pod easily as suggested in the L</SYNOPSIS> above.
#pod
#pod =head1 ENCODING ISSUES
#pod
#pod In general, "it should all just work" ... starting in version v3.
#pod
#pod Email::MIME::Kit assumes that any file read for the purpose of becoming a
#pod C<text/*>-type part is encoded in UTF-8.  It will decode them and work with
#pod their contents as text strings.  Renderers will be passed text strings to
#pod render, and so on.  This, further, means that strings passed to the C<assemble>
#pod method for use in rendering should also be text strings.
#pod
#pod In older versions of Email::MIME::Kit, files read from disk were read in raw
#pod mode and then handled as octet strings.  Meanwhile, the manifest's contents
#pod (and, thus, any templates stored as strings in the manifest) were decoded into
#pod text strings.  This could lead to serious problems.  For example: the
#pod F<manifest.json> file might contain:
#pod
#pod   "header": [
#pod     { "Subject": "Message for [% customer_name %]" },
#pod     ...
#pod   ]
#pod
#pod ...while a template on disk might contain:
#pod
#pod   Dear [% customer_name %],
#pod   ...
#pod
#pod If the customer's name isn't ASCII, there was no right way to pass it in.  The
#pod template on disk would expect UTF-8, but the template in the manifest would
#pod expect Unicode text.  Users prior to v3 may have taken strange steps to get
#pod around this problem, understanding that some templates were treated differently
#pod than others.  This means that some review of kits is in order when upgrading
#pod from earlier versions of Email::MIME::Kit.
#pod
#pod =cut

has source => (is => 'ro', required => 1);

has manifest => (reader => 'manifest', writer => '_set_manifest');

my @auto_attrs = (
  [ manifest_reader => ManifestReader => JSON => [ 'read_manifest' ] ],
  [ kit_reader      => KitReader      => Dir  => [ 'get_kit_entry',
                                                   'get_decoded_kit_entry' ] ],
);

for my $tuple (@auto_attrs) {
  my ($attr, $role, $default, $handles) = @$tuple;

  my $seed = "_${attr}_seed";
  my $base_ns  = "Email::MIME::Kit::$role";
  my $role_pkg = "Email::MIME::Kit::Role::$role";

  has $seed => (
    is => 'ro',
    init_arg => $attr,
    default  => "=Email::MIME::Kit::${role}::$default",
  );

  has $attr => (
    reader      => $attr,
    writer      => "_set_$attr",
    isa         => role_type($role_pkg),
    init_arg    => undef,
    lazy        => 1,
    default     => sub {
      my ($self) = @_;

      my $comp = $self->_build_component($base_ns, $self->$seed);

      return $comp;
    },
    handles => $handles,
  );
}

has validator => (
  is   => 'ro',
  isa  => maybe_type(role_type('Email::MIME::Kit::Role::Validator')),
  lazy    => 1, # is this really needed? -- rjbs, 2009-01-20
  default => sub {
    my ($self) = @_;
    return $self->_build_component(
      'Email::MIME::Kit::Validator',
      $self->manifest->{validator},
    );
  },
);

sub _build_component {
  my ($self, $base_namespace, $entry, $extra) = @_;

  return unless $entry;

  my ($class, $arg);
  if (ref $entry) {
    ($class, $arg) = @$entry;
  } else {
    ($class, $arg) = ($entry, {});
  }

  $class = String::RewritePrefix->rewrite(
    { '=' => '', '' => ($base_namespace . q{::}) },
    $class,
  );

  Module::Runtime::require_module($class);
  $class->new({ %$arg, %{ $extra || {} }, kit => $self });
}

sub BUILD {
  my ($self) = @_;

  my $manifest = $self->read_manifest;
  $self->_set_manifest($manifest);

  if ($manifest->{kit_reader}) {
    my $kit_reader = $self->_build_component(
      'Email::MIME::Kit::KitReader',
      $manifest->{kit_reader},
    );

    $self->_set_kit_reader($kit_reader);
  }

  $self->_setup_default_renderer;
}

sub _setup_default_renderer {
  my ($self) = @_;
  return unless my $renderer = $self->_build_component(
    'Email::MIME::Kit::Renderer',
    $self->manifest->{renderer},
  );

  $self->_set_default_renderer($renderer);
}

sub assemble {
  my ($self, $stash) = @_;

  $self->validator->validate($stash) if $self->validator;

  # Do I really need or want to do this?  Anything that alters the stash should
  # do so via localization. -- rjbs, 2009-01-20
  my $copied_stash = { %{ $stash || {} } };

  my $email = $self->assembler->assemble($copied_stash);   

  my $header = $email->header('Message-ID');
  $email->header_set('Message-ID' => $self->_generate_content_id->in_brackets)
    unless defined $header;

  return $email;
}

sub kit { $_[0] }

sub _assembler_from_manifest {
  my ($self, $manifest, $parent) = @_;

  $self->_build_component(
    'Email::MIME::Kit::Assembler',
    $manifest->{assembler} || 'Standard',
    {
      manifest => $manifest,
      parent   => $parent,
    },
  );
}

has default_renderer => (
  reader => 'default_renderer',
  writer => '_set_default_renderer',
  isa    => role_type('Email::MIME::Kit::Role::Renderer'),
);

has assembler => (
  reader    => 'assembler',
  isa       => role_type('Email::MIME::Kit::Role::Assembler'),
  required  => 1,
  lazy      => 1,
  default   => sub {
    my ($self) = @_;
    return $self->_assembler_from_manifest($self->manifest);
  }
);

sub _generate_content_id {
  Email::MessageID->new;
}

#pod =head1 AUTHOR
#pod
#pod This code was written in 2009 by Ricardo SIGNES.  It was based on a previous
#pod implementation by Hans Dieter Pearcey written in 2006.
#pod
#pod The development of this code was sponsored by Pobox.com.  Thanks, Pobox!
#pod
#pod =cut

no Moose::Util::TypeConstraints;
no Moose;
__PACKAGE__->meta->make_immutable;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::MIME::Kit - build messages from templates

=head1 VERSION

version 3.000006

=head1 SYNOPSIS

  use Email::MIME::Kit;

  my $kit = Email::MIME::Kit->new({ source => 'mkits/sample.mkit' });

  my $email = $kit->assemble({
    account           => $new_signup,
    verification_code => $token,
    ... and any other template vars ...
  });

  $transport->send($email, { ... });

=head1 DESCRIPTION

Email::MIME::Kit is a templating system for email messages.  Instead of trying
to be yet another templating system for chunks of text, it makes it easy to
build complete email messages.

It handles the construction of multipart messages, text and HTML alternatives,
attachments, interpart linking, string encoding, and parameter validation.

Although nearly every part of Email::MIME::Kit is a replaceable component, the
stock configuration is probably enough for most use.  A message kit will be
stored as a directory that might look like this:

  sample.mkit/
    manifest.json
    body.txt
    body.html
    logo.jpg

The manifest file tells Email::MIME::Kit how to put it all together, and might
look something like this:

  {
    "renderer": "TT",
    "header": [
      { "From": "WY Corp <noreplies@wy.example.com>" },
      { "Subject": "Welcome aboard, [% recruit.name %]!" }
    ],
    "alternatives": [
      { "type": "text/plain", "path": "body.txt" },
      {
        "type": "text/html",
        "path": "body.html",
        "container_type": "multipart/related",
        "attachments": [ { "type": "image/jpeg", "path": "logo.jpg" } ]
      }
    ]
  }

B<Inline images> may be accessed with the function C<cid_for>, for example to include the above logo.jpg:

    <img style="margin: 0 auto" src="cid:[% cid_for("logo.jpg") %]">

B<Please note:> the assembly of HTML documents as multipart/related bodies may
be simplified with an alternate assembler in the future.

The above manifest would build a multipart alternative message.  GUI mail
clients would see a rendered HTML document with the logo graphic visible from
the attachment.  Text mail clients would see the plaintext.

Both the HTML and text parts would be rendered using the named renderer, which
here is Template-Toolkit.

The message would be assembled and returned as an Email::MIME object, just as
easily as suggested in the L</SYNOPSIS> above.

=head1 ENCODING ISSUES

In general, "it should all just work" ... starting in version v3.

Email::MIME::Kit assumes that any file read for the purpose of becoming a
C<text/*>-type part is encoded in UTF-8.  It will decode them and work with
their contents as text strings.  Renderers will be passed text strings to
render, and so on.  This, further, means that strings passed to the C<assemble>
method for use in rendering should also be text strings.

In older versions of Email::MIME::Kit, files read from disk were read in raw
mode and then handled as octet strings.  Meanwhile, the manifest's contents
(and, thus, any templates stored as strings in the manifest) were decoded into
text strings.  This could lead to serious problems.  For example: the
F<manifest.json> file might contain:

  "header": [
    { "Subject": "Message for [% customer_name %]" },
    ...
  ]

...while a template on disk might contain:

  Dear [% customer_name %],
  ...

If the customer's name isn't ASCII, there was no right way to pass it in.  The
template on disk would expect UTF-8, but the template in the manifest would
expect Unicode text.  Users prior to v3 may have taken strange steps to get
around this problem, understanding that some templates were treated differently
than others.  This means that some review of kits is in order when upgrading
from earlier versions of Email::MIME::Kit.

=head1 AUTHOR

This code was written in 2009 by Ricardo SIGNES.  It was based on a previous
implementation by Hans Dieter Pearcey written in 2006.

The development of this code was sponsored by Pobox.com.  Thanks, Pobox!

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 CONTRIBUTORS

=for stopwords Charlie Garrison fREW Schmidt hdp Kaitlyn Parkhurst

=over 4

=item *

Charlie Garrison <garrison@zeta.org.au>

=item *

fREW Schmidt <frioux@gmail.com>

=item *

hdp <hdp@1bcdbe44-fcfd-0310-b51b-975661d93aa0>

=item *

Kaitlyn Parkhurst <symkat@symkat.com>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2018 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
