// Copyright 2023 Dennis Hezel
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <agrpc/asio_grpc.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/steady_timer.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include "heartbeat.grpc.pb.h"

#include "common.hh"

int main(int argc, const char** argv) {
    spdlog::set_pattern("%^%L %D %T.%f %t %@] %v%$");
    cxxopts::Options options{argv[0]};
    std::string host_port;
    std::string name;
    uint64_t timeout;

    options.add_options()
        ("S,server", "Server address", cxxopts::value(host_port)->default_value("127.0.0.1:50051"))
        ("n,name", "Client name", cxxopts::value(name))
        ("t,timeout", "Client timeout in seconds", cxxopts::value(timeout)->default_value("3600"))
        ("h,help", "Show this help", cxxopts::value<bool>()->default_value("false"));

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        SPDLOG_INFO(options.help());
        exit(0);
    }

    if (name.empty()) {
        SPDLOG_CRITICAL("Invalid client name");
    }

    heartbeat::Greeter::Stub stub{grpc::CreateChannel(host_port, grpc::InsecureChannelCredentials())};
    agrpc::GrpcContext grpc_context;

    SPDLOG_INFO("Client has name {}, timeout {}", name, timeout);

    asio::steady_timer timer{grpc_context};
    auto period = std::chrono::seconds{timeout * 2 / 3};

    asio::co_spawn(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            do {
                grpc::Status status;
                using RPC = AwaitableClientRPC<&heartbeat::Greeter::Stub::PrepareAsyncSayHello>;
                grpc::ClientContext client_context;
                heartbeat::HelloRequest request;

                request.set_name(name);
                request.set_timeout(timeout);

                heartbeat::HelloReply response;
                SPDLOG_INFO("Sending greetings");
                status = co_await RPC::request(grpc_context, stub, client_context, request, response);

                if (status.ok()) {
                    SPDLOG_INFO("Checkin, count {}", response.count());
                } else {
                    SPDLOG_ERROR("Failed to send greetings, {}", status.error_message());
                }

                SPDLOG_INFO("Reschedule in {}s", period.count());
                timer.expires_after(period);
                SPDLOG_INFO("Checkin, count {}", response.count());
                co_await timer.async_wait(asio::use_awaitable);
            } while (true);
        },
        RethrowFirstArg{});

    grpc_context.run();

    return 0;
}
