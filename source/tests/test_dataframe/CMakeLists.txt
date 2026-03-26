
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/tests)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_NAME test_dataframe)

file(GLOB PROJECT_SRC
    "*.h"
    "*.c"
    "*.cpp"
)

add_executable(
    ${PROJECT_NAME}
    ${PROJECT_SRC}
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX _USE_MATH_DEFINES)
    # match static CRT (/MT) used by Max SDK and thirdparty libs
    set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/source/thirdparty/DataFrame/src/Utils/DateTime.cc
)

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
    ${CMAKE_SOURCE_DIR}/source/thirdparty/DataFrame/include
    ${THIRDPARTY_INCLUDE}
    ${CMAKE_BINARY_DIR}/thirdparty/OpenXLSX/OpenXLSX
)

find_package(Threads REQUIRED)
find_library(OPENXLSX_LIB_TEST NAMES OpenXLSX PATHS ${THIRDPARTY_LIB} NO_DEFAULT_PATH REQUIRED)
find_library(XLSXWRITER_LIB_TEST NAMES xlsxwriter PATHS ${THIRDPARTY_LIB} NO_DEFAULT_PATH REQUIRED)
find_package(ZLIB REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    ${OPENXLSX_LIB_TEST}
    ${XLSXWRITER_LIB_TEST}
    ZLIB::ZLIB
)

# DataFrame uses std::to_chars for floating point which requires macOS 13.3+
set_target_properties(${PROJECT_NAME} PROPERTIES
    XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "13.3"
)
