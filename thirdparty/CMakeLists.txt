
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE asio/asio/include)

add_library(asio::asio ALIAS asio)

add_subdirectory(asio-grpc)
add_subdirectory(cxxopts)
add_subdirectory(spdlog)
