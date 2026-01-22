cmake_minimum_required(VERSION 3.10)

project(argos_native_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Attempt to use PkgConfig if needed, but vcpkg is preferred.
# find_package(PkgConfig REQUIRED)

# Find packages
# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(SentencePiece REQUIRED sentencepiece)

# Dependencies from vcpkg (Protobuf 22+ depends on Abseil)
find_package(absl CONFIG REQUIRED) 
find_package(Protobuf CONFIG REQUIRED)

# CTranslate2 (System installed)
find_package(ctranslate2 REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src ${SentencePiece_INCLUDE_DIRS})
link_directories(/home/reimen/vcpkg/installed/x64-linux/lib)

add_executable(argos_native_cpp
    src/main.cpp
    src/utils.cpp
    src/translation.cpp
    src/tokenizer_bpe.cpp
)

target_link_libraries(argos_native_cpp
    PRIVATE
    ${SentencePiece_LIBRARIES}
    ctranslate2
    protobuf::libprotobuf
)
