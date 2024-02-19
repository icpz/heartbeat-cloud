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

#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include <agrpc/asio_grpc.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/signal_set.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include "heartbeat.grpc.pb.h"

#include "common.hh"

// begin-snippet: server-side-heartbeat
// ---------------------------------------------------
// Server-side hello world which handles exactly one request from the client before shutting down.
// ---------------------------------------------------
// end-snippet

struct Client {
    std::chrono::seconds timeout;
    std::chrono::time_point<std::chrono::steady_clock> last_active;
    uint64_t count;

    void Refresh(uint64_t timeout) {
        this->timeout = std::chrono::seconds{timeout};
        last_active = std::chrono::steady_clock::now();
        count++;
    }

    int64_t LastActive() const {
        return std::chrono::duration_cast<std::chrono::seconds>(last_active.time_since_epoch()).count();
    }
};

class Server {
public:
    enum Status {
        IDLE,
        UPDATE,
        STOP,
    };

public:
    Server() = default;

    ~Server() {
        if (thread_.joinable()) {
            {
                std::lock_guard<std::mutex> _{mutex_};
                status_ = STOP;
            }
            cv_.notify_one();
            thread_.join();
        }
    }

    bool Setup() {
        thread_ = std::thread(
            [this]() {
                std::chrono::seconds period = std::chrono::seconds::max();
                std::unique_lock<std::mutex> lk;
                while (true) {
                    bool cv_timeout = !cv_.wait_for(lk, period, [this]() { return status_ != IDLE; });
                    if (status_ == STOP) {
                        SPDLOG_INFO("Worker thread exits");
                        break;
                    }

                    if (!cv_timeout) {
                        SPDLOG_INFO("Worker thread got a refresh");
                    }

                    auto now = std::chrono::steady_clock::now();
                    std::chrono::seconds new_period = std::chrono::seconds::max();
                    std::vector<std::string> inactive_clients;
                    for (auto &[name, client] : clients_) {
                        auto since = std::chrono::duration_cast<std::chrono::seconds>(now - client.last_active);
                        if (since >= client.timeout) {
                            SPDLOG_ERROR("Client %s timeout, last active %lld", name, client.LastActive());
                            inactive_clients.push_back(name);
                        } else if (client.timeout - since < new_period) {
                            new_period = client.timeout - since;
                            SPDLOG_INFO("Client %s is about to timeout after %lld", name, new_period.count());
                        }
                    }

                    for (auto &name : inactive_clients) {
                        SPDLOG_INFO("Erase entry for %s due to timeout", name);
                        clients_.erase(name);
                    }

                    status_ = IDLE;
                    period = new_period;
                }
            }
        );
        return true;
    }

    uint64_t ProcessClient(const std::string &name, uint64_t timeout) {
        uint64_t count = 0;
        {
            std::lock_guard<std::mutex> _{mutex_};
            auto &client = clients_[name];

            if (client.count == 0) {
                SPDLOG_INFO("New client %s checkin, timeout %llu", name, timeout);
            } else {
                SPDLOG_INFO("Client %s checkin %d times, timeout %llu, last active %lld", name, client.count, timeout, client.LastActive());
            }

            if (status_ == STOP) {
                SPDLOG_WARN("Exiting worker thread... ignore client %s", name);
                return count;
            }
            client.Refresh(timeout);
            status_ = UPDATE;
            count = client.count;
        }
        cv_.notify_one();
        return count;
    }

private:
    std::unordered_map<std::string, Client> clients_;
    Status status_ = IDLE;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

int main(int argc, const char** argv) {
    spdlog::set_pattern("%^%L %D %T.%f %t %@] %v%$");
    cxxopts::Options options{argv[0]};
    std::string host_port;

    options.add_options()
        ("L", "Listen address", cxxopts::value(host_port)->default_value("0.0.0.0:50051"))
        ("h,help", "Show this help", cxxopts::value<bool>()->default_value("false"));

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        SPDLOG_INFO(options.help());
        exit(0);
    }

    heartbeat::Greeter::AsyncService service;
    std::unique_ptr<grpc::Server> server;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(host_port, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();

    Server ctx;
    ctx.Setup();

    using RPC = AwaitableServerRPC<&heartbeat::Greeter::AsyncService::RequestSayHello>;
    agrpc::register_awaitable_rpc_handler<RPC>(
        grpc_context, service,
        [&server, &ctx](RPC& rpc, RPC::Request& request) -> asio::awaitable<void>
        {
            heartbeat::HelloReply response;
            response.set_count(ctx.ProcessClient(request.name(), request.timeout()));
            co_await rpc.finish(response, grpc::Status::OK);
            server->Shutdown();
        },
        RethrowFirstArg{}
    );

    grpc_context.run();
}
