
add_subdirectory(proto)

list(APPEND DEPS
    spdlog::spdlog_header_only
    cxxopts::cxxopts
    asio-grpc::asio-grpc-standalone-asio
    asio::asio
    heartbeat_grpc
    )

add_executable(heartbeat-server heartbeat-server.cc)
target_link_libraries(heartbeat-server ${DEPS})

add_executable(heartbeat-client heartbeat-client.cc)
target_link_libraries(heartbeat-client ${DEPS})
