cmake_minimum_required(VERSION 3.26)

project(argos_native_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------
# Configuración común
# -----------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(SentencePiece REQUIRED sentencepiece)
pkg_check_modules(Protobuf REQUIRED protobuf)

# Find CUDA libraries if available  
find_package(CUDAToolkit QUIET)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src ${SentencePiece_INCLUDE_DIRS} /usr/local/include)
link_directories(/usr/local/lib ${SentencePiece_LIBRARY_DIRS} ${Protobuf_LIBRARY_DIRS})

# -----------------------
# Definición del ejecutable principal
# -----------------------
add_executable(Fast_translator
    src/main.cpp
    src/utils.cpp
    src/translation.cpp
    src/tokenizer_bpe.cpp
    src/language_graph.cpp
)

target_link_libraries(Fast_translator
    PRIVATE
    ${SentencePiece_LIBRARIES}
    /usr/local/lib/libctranslate2.so
    ${Protobuf_LIBRARIES}
)

# Add CUDA libraries if available
if(CUDAToolkit_FOUND)
    target_link_libraries(Fast_translator PRIVATE CUDA::cudart CUDA::cublas)
    message(STATUS "Linking with CUDA libraries for GPU support")
endif()

# -----------------------
# Definición del gestor GUI (wxWidgets)
# -----------------------
# Try to find wxWidgets (from vcpkg or system)
find_package(wxWidgets CONFIG QUIET)

if(wxWidgets_FOUND OR TARGET wx::core)
    message(STATUS "wxWidgets found - building GUI manager")
    add_executable(Fast_translator_manager
        src/gui_main.cpp
    )
    target_link_libraries(Fast_translator_manager
        PRIVATE
        wx::core
        wx::base
        wx::net
    )
else()
    message(STATUS "wxWidgets not found - skipping GUI manager. Use vcpkg toolchain to build GUI.")
endif()
