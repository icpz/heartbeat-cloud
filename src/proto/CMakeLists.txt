
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf REQUIRED)

add_library(heartbeat_grpc heartbeat.proto)
target_link_libraries(heartbeat_grpc PUBLIC gRPC::grpc++)

protobuf_generate(TARGET heartbeat_grpc)
protobuf_generate(
    TARGET heartbeat_grpc
    LANGUAGE grpc
    PLUGIN protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
    #    PLUGIN_OPTIONS generate_mock_code=true
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc)

target_include_directories(heartbeat_grpc PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
