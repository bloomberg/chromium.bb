// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Tests some higher-level functionality of Mojom interfaces.
//!
//! Test failure is defined as the function returning via panicking
//! and the result being caught in the test! macro. If a test function
//! returns without panicking, it is assumed to pass.

#[macro_use]
extern crate mojo;

#[macro_use]
mod util;

use mojo::bindings::mojom::{MojomInterface, MojomInterfaceRecv, MojomInterfaceSend};
use mojo::system::message_pipe;
use mojo::system::{Handle, MOJO_INDEFINITE};

use std::thread;

use util::mojom_validation::*;

tests! {
    // Tests basic client and server interaction over a thread
    fn send_and_recv() {
        let (endpt0, endpt1) = message_pipe::create(mpflags!(Create::None)).unwrap();
        // Client and server handles
        let client = IntegrationTestInterfaceClient::new(endpt0);
        let server = IntegrationTestInterfaceServer::with_version(endpt1, 0);
        // Client thread
        let handle = thread::spawn(move || {
            // Send request
            client.send_request(5, IntegrationTestInterfaceMethod0Request {
                param0: BasicStruct {
                    a: -1,
                },
            }).unwrap();
            // Wait for response
            client.pipe().wait(signals!(Signals::Readable), MOJO_INDEFINITE);
            // Decode response
            let (req_id, options) = client.recv_response().unwrap();
            assert_eq!(req_id, 5);
            match options {
                IntegrationTestInterfaceResponseOption::IntegrationTestInterfaceMethod0(msg) => {
                    assert_eq!(msg.param0, vec![1, 2, 3]);
                },
            }
        });
        // Wait for request
        server.pipe().wait(signals!(Signals::Readable), MOJO_INDEFINITE);
        // Decode request
        let (req_id, options) = server.recv_response().unwrap();
        assert_eq!(req_id, 5);
        match options {
            IntegrationTestInterfaceRequestOption::IntegrationTestInterfaceMethod0(msg) => {
                assert_eq!(msg.param0.a, -1);
            },
        }
        // Send response
        server.send_request(5, IntegrationTestInterfaceMethod0Response {
            param0: vec![1, 2, 3],
        }).unwrap();
        let _ = handle.join();
    }
}
