#pragma once

#include <agrpc/client_rpc.hpp>
#include <agrpc/server_rpc.hpp>
#include <asio/use_awaitable.hpp>

#include <exception>

template <auto PrepareAsync>
using AwaitableClientRPC = asio::use_awaitable_t<>::as_default_on_t<agrpc::ClientRPC<PrepareAsync>>;

template <auto RequestRPC>
using AwaitableServerRPC = asio::use_awaitable_t<>::as_default_on_t<agrpc::ServerRPC<RequestRPC>>;

struct RethrowFirstArg
{
    template <class... T>
    void operator()(std::exception_ptr ep, T&&...)
    {
        if (ep)
        {
            std::rethrow_exception(ep);
        }
    }

    template <class... T>
    void operator()(T&&...)
    {
    }
};
