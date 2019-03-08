# What does GrpcAsyncDispatcher do?

gRPC++ uses
[completion queue](https://grpc.io/docs/tutorials/async/helloasync-cpp.html)
to handle async operations. It doesn't fit well with Chromium's callback-based
async handling paradigm, and it is technically still a blocking API unless you
create a new thread to run the queue. gRPC is working on adding callback-based
APIs but it won't be ready to use in the near future, so we created a
GrpcAsyncDispatcher class to help adapting gRPC's completion queue logic into
Chromium's callback paradigm.

# Basic usage

```cpp
class MyClass {
  public:
  MyClass() : weak_factory_(this) {}
  ~MyClass() {}

  void SayHello() {
    HelloRequest request;
    dispatcher_->ExecuteAsyncRpc(
        // This is run immediately inside the call stack of |ExecuteAsyncRpc|.
        base::BindOnce(&HelloService::Stub::AsyncSayHello,
                        base::Unretained(stub_.get())),
        std::make_unique<grpc::ClientContext>(), request,
        // Callback might be called after the dispatcher is destroyed. Make sure
        // to bind WeakPtr here.
        base::BindOnce(&MyClass::OnHelloResult,
                        weak_factory_.GetWeakPtr()));
  }

  void StartHelloStream() {
    StreamHelloRequest request;
    scoped_hello_stream_ = dispatcher_->ExecuteAsyncServerStreamingRpc(
        base::BindOnce(&HelloService::Stub::AsyncStreamHello,
                      base::Unretained(stub_.get())),
        std::make_unique<grpc::ClientContext>(), request,
        base::BindRepeating(&MyClass::OnHelloStreamMessage,
                            weak_factory_.GetWeakPtr()),
        base::BindOnce(&MyClass::OnHelloStreamClosed,
                        weak_factory_.GetWeakPtr()));
  }

  void CloseHelloStream() {
    scoped_hello_stream_.reset();
  }

  private:
  void OnHelloResult(const grpc::Status& status,
                      const HelloResponse& response) {
    if (status.error_code() == grpc::StatusCode::CANCELLED) {
      // The request has been canceled because |dispatcher_| is destroyed.
      return;
    }

    if (!status.ok()) {
      // Handle other error here.
      return;
    }

    // Response is received. Use the result here.
  }

  void OnHelloStreamMessage(const HelloStreamResponse& response) {
    // This will be called every time the server sends back messages
    // through the stream.
  }

  void OnHelloStreamClosed(const grpc::Status& status) {
    switch (status.error_code()) {
      case grpc::StatusCode::CANCELLED:
        // The stream is closed by the client, either because the scoped stream
        // is deleted or the dispatcher is deleted.
        break;
      case grpc::StatusCode::UNAVAILABLE:
        // The stream is either closed by the server or dropped due to
        // network issues.
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  std::unique_ptr<HelloService::Stub> stub_;
  GrpcAsyncDispatcher dispatcher_;
  std::unique_ptr<ScopedGrpcServerStream> scoped_hello_stream_;
  base::WeakPtrFactory<MyClass> weak_factory_;
};
```
