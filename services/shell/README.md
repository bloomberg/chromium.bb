# Service Manager User Guide

## What is the Service Manager?

The Service Manager is a tool that brokers connections and capabilities between
and manages instances of components, referred to henceforth as services.

The Service Manager performs the following functions:

* Brokering connections between services, including communicating policies such
  as capabilities (which include access to interfaces), user identity, etc.
* Launching and managing the lifecycle services and processes (though services
  may also create their own processes and tell the Service Manager about them).
* Tracks running services, and provides an API that allows services to
  understand whats running.

The Service Manager presents a series of Mojo interfaces to services, though in
practice interacting with the Service is made simpler with a client library.
Currently, there is only a client library written in C++, since that meets the
needs of most of the use cases in Chrome.

## Details

### Mojo Recap

The Mojo system provides two key components of interest here - a lightweight
message pipe concept allowing two endpoints to communicate, and a bindings layer
that allows interfaces to be described to bind to those endpoints, with
ergonomic bindings for languages used in Chrome.

Mojo message pipes are designed to be lightweight and may be read from/written
to and passed around from one process to the next. In most situations however
the developer wont interact with the pipes directly, rather with a generated
types encapsulating a bound interface.

To use the bindings, a developer defines their interface in the Mojo IDL format,
**mojom**. With some build magic, the generated headers can then be included and
used from C++, JS and Java.

It is important to note here that Mojo Interfaces have fully qualified
identifiers in string form, generated from the module path and interface name:
**`module.path.InterfaceName`**. This is how interfaces are referenced in
Service Manifests, and how they will be referenced throughout this document.

This would be a good place for me to refer to this in-depth Mojo User Guide,
which spells all of this out in great detail.

### Services

A Service is any bit of code the Service Manager knows about. This could be a
unique process, or just a bit of code run in some existing process.

The Service Manager disambiguates services by their **Identity**. Every service
has its own unique Identity. From the Service Managers perspective, a services
Identity is represented by the tuple of the its Name, UserId and Instance Name.
The Name is a formatted string that superficially represents a scheme:host pair,
but actually isnt a URL. More on the structure of these names later. The UserId
is a string GUID, representing the user the service is run as. The Instance Name
is a string, typically (but not necessarily) derived from the Name, which can be
used to allow multiple instances of a service to exist for the same Name,UserId
pair. In Chrome an example of this would be multiple instances of the renderer
or the same profile.

A Service implements the Mojo interface shell.mojom.Service, which is the
primary means the Service Manager has of communicating with its service. Service
has two methods: OnStart(), called once at when the Service Manager first learns
about the service, and OnConnect(), which the Service Manager calls every time
some other service tries to connect to this one.

Services have a link back to the Service Manager too, primarily in the form of
the shell.mojom.Connector interface. The Connector allows services to open
connections to other services.

A unique connection from the Service Manager to a service is called an
instance, each with its own unique identifier, called an instance id. Every
instance has a unique Identity. It is possible to locate an existing instance
purely using its Identity.

Services define their own lifetimes. Services in processes started by other
services (rather than the Service Manager) may even outlive the connection with
the Service Manager. For processes launched by the Service Manager, when a
service wishes to terminate it closes the Service pipe with the Service Manager
and the Service Manager destroys its corresponding instance and asks the process
to exit.

#### A simple Service example

Consider this simple application that implements the Service interface:

**app.cc:**

    #include "mojo/public/c/system/main.h"
    #include "services/shell/public/cpp/application_runner.h"
    #include "services/shell/public/cpp/connector.h"
    #include "services/shell/public/cpp/connection.h"
    #include "services/shell/public/cpp/identity.h"
    #include "services/shell/public/cpp/service.h"

    class Service : public shell::Service {
     public:
      Service() {}
      ~Service() override {}

      // Overridden from shell::Service:
      void OnStart(const shell::Identity& identity) override {
      }
      bool OnConnect(shell::Connection* connection) override {
        return true;
      }
    };

    MojoResult ServiceMain(MojoHandle service_request_handle) {
      return shell::ServiceRunner(new Service).Run(service_request_handle);
    }

    app_manifest.json:

    {
      "manifest_version": 1,
      "name": "mojo:app",
      "display_name": "Example App",
      "capabilities": {}
    }

**BUILD.gn:**

    import("//mojo/public/mojo_application.gni")

    service("app") {
      sources = [ "app.cc" ]
      deps = [ "//base", "//mojo/shell/public/cpp" ]
      data_deps = [ ":manifest" ]
    }

    service_manifest("manifest") {
      name = "app"
      source = "app_manifest.json"
    }

What does all this do? Building the app target produces two files in the output
directory: app/app.library and app/manifest.json. app.library is a DSO loaded by
the Service Manager in its own process when another service connects to the
mojo:app name. This is not the only way (nor even the most likely one) you can
implement a Service, but its the simplest and easiest to reason about.

This service doesnt do much. Its implementation of OnStart() is empty, and its
implementation of OnConnect just returns true to allow the inbound connection to
complete. Lets study the parameters to these methods though, since theyll be
important as we begin to do more in our service.

##### OnStart Parameters

###### const shell::Identity& identity
This is the identity this service is known to the Service Manager as. It
includes the services Name, User ID and Instance Name.

##### OnConnect Parameters

###### shell::Connection* connection
This is a pointer to an object that encapsulates the connection with a remote
service. The service uses this object to learn about the service at the remote
end, to bind interfaces from it, and to expose interfaces to it. The
Connection concept is implemented under the hood by a pair of
shell.mojom.InterfaceProviders - this is the physical link between the service
that give the Connection its utility. The Connection object is owned by the
caller of OnConnect, and will outlive the underlying pipes.

The service can decide to block the connection outright by returning false from
this method. In that scenario the underlying pipes will be closed and the remote
end will see an error and have the chance to recover.

Before we add any functionality to our service, such as exposing an interface,
we should look at how we connect to another service and bind an interface from
it. This will lay the groundwork to understanding how to export an interface.

### Connecting

Once we have a Connector, we can connect to other services and bind interfaces
from them. In the trivial app above we can do this directly in OnStart:

    void OnStart(const shell::Identity& identity) override {
      scoped_ptr<shell::Connection> connection = 
          connector()->Connect("mojo:service");
      mojom::SomeInterfacePtr some_interface;
      connection->GetInterface(&some_interface);
      some_interface->Foo();
    }

This assumes an interface called mojo.SomeInterface with a method Foo()
exported by another Mojo client identified by the name mojo:service.

What is happening here? Lets look line-by-line


    scoped_ptr<shell::Connection> connection = 
        connector->Connect("mojo:service");

This asks the Service Manager to open a connection to the service named
mojo:service. The Connect() method returns a Connection object similar to the
one received by OnConnect() - in fact this Connection object binds the other
ends of the pipes of the Connection object received by OnConnect in the remote
service. This time, the caller of Connect() takes ownership of the Connection,
and when it is destroyed the connection (and the underlying pipes) is closed. A
note on this later.

    mojom::SomeInterfacePtr some_interface;

This is a shorthand from the mojom bindings generator, producing an
instantiation of a mojo::InterfacePtr<mojom::SomeInterface>. At this point the
InterfacePtr is unbound (has no pipe handle), and calling is_bound() on it will
return false. Before we can call any methods, we need to bind it to a Mojo
message pipe. This is accomplished on the following line:

    connection->GetInterface(&some_interface);

Calling this method allocates a Mojo message pipe, binds the client handle to
the provided InterfacePtr, and sends the server handle to the remote service,
where it will eventually (asynchronously) be bound to an object implementing the
requested interface. Now that our InterfacePtr has been bound, we can start
calling methods on it:

    some_interface->Foo();

Now an important note about lifetimes. At this point the Connection returned by
Connect() goes out of scope, and is destroyed. This closes the underlying
InterfaceProvider pipes with the remote client. But Mojo methods are
asynchronous. Does this mean that the call to Foo() above is lost? No. Before
closing, queued writes to the pipe are flushed.

### Implementing an Interface

Lets look at how to implement an interface now from a client and expose it to
inbound connections from other clients. To do this well need to implement
OnConnect() in our Service implementation, and implement a couple of other
interfaces. For the sake of this example, well imagine now were writing the
mojo:service client, implementing the interface defined in this mojom:

**some_interface.mojom:**

    module mojom;

    interface SomeInterface {
      Foo();
    };

To build this mojom we need to invoke the mojom gn template from
`//mojo/public/tools/bindings/mojom.gni`. Once we do that and look at the
output, we can see that the C++ class mojom::SomeInterface is generated and can
be #included from the same path as the .mojom file at some_interface.mojom.h.
In our implementation of the mojo:service client, well need to derive from this
class to implement the interface. But thats not enough. Well also have to find
a way to bind inbound requests to bind this interface to the object that
implements it. Lets look at a snippet of a class that does all of this:

**service.cc:**

    class Service : public shell::Service,
                    public shell::InterfaceFactory<mojom::SomeInterface>,
                    public mojom::SomeInterface {
     public:
      ..

      // Overridden from shell::Service:
      bool OnConnect(shell::Connection* connection) override {
        connection->AddInterface<mojom::SomeInterface>(this);
        return true;
      }

      // Overridden from shell::InterfaceFactory<mojom::SomeInterface>:
      void Create(shell::Connection* connection,
                  mojom::SomeInterfaceRequest request) override {
        bindings_.AddBinding(this, std::move(request));
      }

      // Overridden from mojom::SomeInterface:
      void Foo() override { /* .. */ }

      mojo::BindingSet<mojom::SomeInterface> bindings_;
    };

Lets study whats going on, starting with the obvious - we derive from
`mojom::SomeInterface` and implement `Foo()`. How do we bind this implementation
to a pipe handle from a connected service? First we have to advertise the
interface to the client through the inbound connection. This is accomplished in
OnConnect():

    connection->AddInterface<mojom::SomeInterface>(this);

This adds the `mojom.SomeInterface` interface name to the inbound Connection
objects InterfaceRegistry, and tells the InterfaceRegistry to consult this
object when it needs to construct an implementation to bind. Why this object?
Well in addition to Service and SomeInterface, we also implement an
instantiation of the generic interface InterfaceFactory. InterfaceFactory is the
missing piece - it binds a request for SomeInterface (in the form of a message
pipe server handle) to the object that implements the interface (this). This is
why we implement Create():

    bindings_.AddBinding(this, std::move(request));

In this case, this single instance binds requests for this interface from all
connected clients, so we use a mojo::BindingSet to hold them all. Alternatively,
we could construct an object per request, and use mojo::Binding.

### Capabilities

While the code above looks like it should work, if we were to type it all in,
build it and run it it still wouldnt. In fact, if we ran it, wed see this
error in the console:

`Capabilities prevented connection from: mojo:app to mojo:service`

The answer lies in an omission in one of the files I didnt discuss earlier, the
manifest.json, specifically the empty capabilities dictionary.

You can think of an interface (and its underlying client handle) as a
capability. If you have it, and its bound, you can call methods on it and
something will happen. If you dont have a bound InterfacePtr, you (effectively)
dont have that capability.

At the top level, the Service Manager implements the delegation of capabilities
in accordance with rules spelled out in each services manifest.

Each service produces a manifest file with some typical metadata about itself,
and a capability spec. A capability spec describes classes of
capabilities offered by the service, classes of capabilities and individual
capabilities consumed by the service. Lets study a fairly complete capability
spec from another services manifest:

    "capabilities": {
      "provided": {
        "web": ["if1", "if2"],
        "uid": []
        "god-mode": ["*"]
      },
      "required": {
        "*": { "classes": ["c1", "c2"], "interfaces": ["if3", "if4"] },
        "mojo:foo": { "classes": ["c3"], "interfaces": ["if5"] }
      }
    }

At the top level of the capabilities dictionary are two sub-dictionaries.

#### Provided Capability Classes

The provided dictionary enumerates the capability classes provided by the
service. A capability class is an alias, either to some special behavior exposed
by the service to remote services that request that class, or to a set of
interfaces exposed by the service to remote services. In the former case, in the
dictionary we provide an empty array as the value of the class name key, in the
latter case we provide an array with a list of the fully qualified Mojo
interface names (module.path.InterfaceName). A special case of array is one that
contains the single entry *, which means all interfaces. In the example
above, when another service connects to this one and requests the god-mode
class in its manifest, it can connect to all interfaces exposed by this service.

#### Required Capabilities

The required dictionary enumerates the capability classes and interfaces
required by the service. The keys into this dictionary are the names of the
services it intends to connect to, and the values for each key are capability
specs that describe the capability classes and individual interfaces that this
class needs to operate correctly. Here again, an array value for the
interfaces key in the capability spec consisting of a single * means the
service needs to bind all interfaces exposed by that service. Additionally, a
* key in the required dictionary allows the service to provide a capability
spec that must be adhered to by all applications it connects to.

Note that a service need not enumerate every interface it provides in the
provided dictionary. This is done effectively at runtime when the service calls
AddInterface() on inbound connections. The service merely describes groups of
interfaces in capability classes as an ergonomic measure. Without capability
classes, services would have to explicitly state every interface they intended
to bind, which would make the manifests very cumbersome to author.

Armed with this knowledge, we can return to app_manifest.json from the first
example and fill out the capability spec:

    {
      "manifest_version": 1,
      "name": "mojo:app",
      "display_name": "Example App",
      "capabilities": {
        "required": {
          "mojo:service": [],
        }
      }
    }

If we just run now, it still wont work, and well see this error:

    Connection CapabilitySpec prevented binding to interface mojom.SomeInterface
    connection_name: mojo:service remote_name: mojo:app

The connection was allowed to complete, but the attempt to bind
`mojom.SomeInterface` was blocked. We need to add that interface to the array in
the manifest:

    "required": {
      "mojo:service": [ "mojom::SomeInterface" ],
    }

Now everything should work.

(Note that we didnt write a manifest for mojo:service. Wed need to do that
too, though for this example we wouldnt have to describe mojom.SomeInterface in
the provided section of its capability spec, since it wasnt part of a class.
Connecting services like mojo:app just need to state that interface.)

### Testing

Now that weve built a simple application and service, its time to write a test
for them. The Shell client library provides a gtest base class
**shell::test::ServiceTest** that makes writing integration tests of services
straightforward. Lets look at a simple test of our service:

    #include "base/bind.h"
    #include "base/run_loop.h"
    #include "mojo/shell/public/cpp/service_test.h"
    #include "path/to/some_interface.mojom.h"

    void QuitLoop(base::RunLoop* loop) {
      loop->Quit();
    }

    class Test : public shell::test::ServiceTest {
     public:
      Test() : shell::test::ServiceTest(exe:service_unittest) {}
      ~Test() override {}
    }

    TEST_F(Test, Basic) {
      mojom::SomeInterface some_interface;
      connector()->ConnectToInterface("mojo:service", &some_interface);
      base::RunLoop loop;
      some_interface->Foo(base::Bind(&QuitLoop, &loop));
      loop.Run();
    }

The BUILD.gn for this test file looks like any other using the test() template.
It must also depend on //services/shell/public/cpp:shell_test_support.

ServiceTest does a few things, but most importantly it register the test itself
as a Service, with the name you pass it via its constructor. In the example
above, we supplied the name exe:service_unittest. This name is has no special
meaning other than that henceforth it will be used to identify the test service.

Behind the scenes, ServiceTest spins up the Service Manager on a background
thread, and asks it to create an instance for the test service on the main
thread, with the name supplied. ServiceTest blocks the main thread while the
Service Manager thread does this initialization. Once the Service Manager has
created the instance, it calls OnStart() (as for any other service), and the
main thread continues, running the test. At this point accessors defined in
service_test.h like connector() can be used to connect to other services.

Youll note in the example above I made Foo() take a callback, this is to give
the test something interesting to do. In the mojom for SomeInterface wed have
the Foo() method return an empty response. In mojo:service, wed have Foo() take
the callback as a parameter, and run it. In the test, we spin a RunLoop until we
get that response. In real world cases we can pass back state & validate
expectations. You can see real examples of this test framework in use in the
Service Managers own suite of tests, under //services/shell/tests.

### Packaging

By default a .library statically links its dependencies, so having many of them
will yield an installed product many times larger than Chrome today. For this
reason its desirable to package several Services together in a single binary.
The Service Manager provides an interface **shell.mojom.ServiceFactory**:

    interface ServiceFactory {
      CreateService(Service& service, string name);
    };

When implemented by a service, the service becomes a package of other
services, which are instantiated by this interface. Imagine we have two services
mojo:service1 and mojo:service2, and we wish to package them together in a
single package mojo:services. We write the Service implementations for
mojo:service1 and mojo:service2, and then a Service implementation for
mojo:services - the latter implements ServiceFactory and instantiates the other
two:

    using shell::mojom::ServiceFactory;
    using shell::mojom::ServiceRequest;

    class Services : public shell::Service,
                     public shell::InterfaceFactory<ServiceFactory>,
                     public ServiceFactory {

      // Expose ServiceFactory to inbound connections and implement
      // InterfaceFactory to bind requests for it to this object.
      void CreateService(ServiceRequest request,
                         const std::string& name) {
        if (name == mojo:service1)
          new Service1(std::move(request));
        else if (name == mojo:service2)
          new Service2(std::move(request));
      }
    }

This is only half the story though. While this does mean that mojo:service1 and
mojo:service2 are now packaged (statically linked) with mojo:services, as it
stands to connect to either packaged service youd have to connect to
mojo:services first, and call CreateService yourself. This is undesirable for a
couple of reasons, firstly in that it complicates the connect flow, secondly in
that it forces details of the packaging, which are a distribution-level
implementation detail on clients wishing to use a service.

To solve this, the Service Manager actually automates resolving packaged service
names to the package service. The Service Manager considers the name of a
service provided by some other package service to be an alias to that package
service. The Service Manager resolves these aliases based on information found,
you guessed it, in the manifests for the package client.

Lets imagine mojo:service1 and mojo:service2 have typical manifests of the form
we covered earlier. Now imagine mojo:services, the package service that combines
the two. In the application install directory rather than the following
structure:

    service1/service1.library,manifest.json
    service2/service2.library,manifest.json

Instead well have:

    package/services.library,manifest.json

The manifest for the package service describes not only itself, but includes the
manifests of all the services it provides. Fortunately there is some GN build
magic that automates generating this meta-manifest, so you dont need to write
it by hand. In the service_manifest() template instantiation for services, we
add the following lines:

    deps = [ ":service1_manifest", ":service2_manifest" ]
    packaged_services = [ "service1", "service2" ]

The deps line lists the service_manifest targets for the packaged services to be
consumed, and the packaged_services line provides the service names, without the
mojo: prefix. The presence of these two lines will cause the Manifest Collator
script to run, merging the dependent manifests into the package manifest. You
can study the resulting manifest to see what gets generated.

At startup, the Service Manager will scan the package directory and consume the
manifests it finds, so it can learn about how to resolve aliases that it might
encounter subsequently. 

### Executables

Thus far, the examples weve covered have packaged Services in .library files.
Its also possible to have a conventional executable provide a Service. There
are two different ways to use executables with the Service Manager, the first is
to have the Service Manager start the executable itself, the second is to have
some other executable start the process and then tell the Service Manager about
it. In both cases, the target executable has to perform a handshake with the
Service Manager early on so it can bind the Service request the Service Manager
sends it.

Assuming you have an executable that properly initializes the Mojo EDK, you add
the following lines at some point early in application startup to establish the
connection with the Service Manager:

    #include "services/shell/public/cpp/service.h"
    #include "services/shell/public/cpp/service_context.h"
    #include "services/shell/runner/child/runner_connection.h"

    class MyClient : public shell::Service {
    ..
    };

    shell::mojom::ServiceRequest request;
    scoped_ptr<shell::RunnerConnection> connection(
       shell::RunnerConnection::ConnectToRunner(
            &request, ScopedMessagePipeHandle()));
    MyService service;
    shell::ServiceContext context(&service, std::move(request));

Whats happening here? The Service/ServiceContext usage should be familiar from
our earlier examples. The interesting part here happens in
`RunnerConnection::ConnectToRunner()`. Before we look at what ConnectToRunner
does, its important to cover how this process is launched. In this example,
this process is launched by the Service Manager. This is achieved through the
use of the exe Service Name type. The Service Names weve covered thus far
have looked like mojo:foo. The mojo prefix means that the Shell should look
for a .library file at foo/foo.library alongside the Service Manager
executable. If the code above was linked into an executable app.exe alongside
the Service Manager executable in the output directory, it can be launched by
connecting to the name exe:app. When the Service Manager launches an
executable, it passes a pipe to it on the command line, which the executable is
expected to bind to receive a ServiceRequest on. Now back to ConnectToRunner.
It spins up a background control thread with the Service Manager, binds the
pipe from the command line parameter, and blocks the main thread until the
ServiceRequest arrives and can be bound.

Like services provided from .library files, we have to provide a manifest for
services provided from executables. The format is identical, but in the
service_manifest template we need to set the type property to exe to cause the
generation step to put the manifest in the right place (it gets placed alongside
the executable, with the name <exe_name>_manifest.json.)

### Service-Launched Processes

There are some scenarios where a service will need to launch its own process,
rather than relying on the Service Manager to do it. The Connector API provides
the ability to tell the Shell about a process that the service has or will
create. The executable that the service launches (henceforth referred to as the
target) should be written using RunnerConnection as discussed in the previous
section. The connect flow in the service that launches the target (henceforth
referred to as the driver) works like this:

    base::FilePath target_path;
    base::PathService::Get(base::DIR_EXE, &target_path);
    target_path = target_path.Append(FILE_PATH_LITERAL("target.exe"));
    base::CommandLine target_command_line(target_path);

    mojo::edk::PlatformChannelPair pair;
    mojo::edk::HandlePassingInformation info;
    pair.PrepareToPassClientHandleToChildProcess(&target_command_line, &info);

    std::string token = mojo::edk::GenerateRandomToken();
    target_command_line.AppendSwitchASCII(switches::kPrimordialPipeToken, 
                                          token);

    mojo::ScopedMessagePipeHandle pipe =
        mojo::edk::CreateParentMessagePipe(token);

    shell::mojom::ServiceFactoryPtr factory;
    factory.Bind(
        mojo::InterfacePtrInfo<shell::mojom::ServiceFactory>(
            std::move(pipe), 0u));
    shell::mojom::PIDReceiverPtr receiver;

    shell::Identity target("exe:target",shell::mojom::kInheritUserID);
    shell::Connector::ConnectParams params(target);
    params.set_client_process_connection(std::move(factory), 
                                         GetProxy(&receiver));
    scoped_ptr<shell::Connection> connection = connector->Connect(&params);

    base::LaunchOptions options;
    options.handles_to_inherit = &info;
    base::Process process = base::LaunchProcess(target_command_line, options);
    mojo::edk::ChildProcessLaunched(process.Handle(), pair.PassServerHandle());

Thats a lot. But it boils down to these steps:
1. Creating the message pipe to connect the target process and the Service
Manager.
2. Putting the server end of the pipe onto the command line to the target
process.
3. Binding the client end to a ServiceFactoryPtr, constructing an Identity for
the target process and passing both through Connector::Connect().
4. Starting the process with the configured command line.

In this example the target executable could be the same as the previous example.

A word about process lifetimes. Processes created by the shell are managed by
the Service Manager. While a service-launched process may quit itself at any
point, when the Service Manager shuts down it will also shut down any process it
started. Processes created by services themselves are left to those services to
manage.

***

TBD:

Instances & Processes

Client lifetime strategies

Process lifetimes.

Writing tests (ShellTest)
Under the Hood
Four major components: Shell API (Mojom), Shell, Catalog, Shell Client Lib.
The connect flow, catalog, etc.
Capability brokering in the shell
Userids

Finer points:

Mojo Names: mojo, exe
Exposing services on outbound connections
