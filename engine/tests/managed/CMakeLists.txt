cmake_minimum_required(VERSION 3.20)
project(AllioManagedSmoke LANGUAGES CXX)
enable_testing()
find_program(DOTNET dotnet REQUIRED)
add_executable(allio-managed-smoke BridgeSmoke.cpp ../../core/script/CSharpHost.cpp)
target_compile_features(allio-managed-smoke PRIVATE cxx_std_20)
target_link_libraries(allio-managed-smoke PRIVATE ${CMAKE_DL_LIBS})
set(managed_dir "${CMAKE_CURRENT_BINARY_DIR}/managed")
add_custom_target(smoke-game ALL
    COMMAND "${DOTNET}" build "${CMAKE_CURRENT_SOURCE_DIR}/Probe.csproj" -c Release
            --artifacts-path "${CMAKE_CURRENT_BINARY_DIR}/artifacts" -o "${managed_dir}"
    VERBATIM)
add_dependencies(allio-managed-smoke smoke-game)
add_test(NAME allio-csharp-load-callback-reload COMMAND allio-managed-smoke "${managed_dir}")
