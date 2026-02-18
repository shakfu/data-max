include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/common.cmake)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    "${MAX_SDK_INCLUDES}"
    "${MAX_SDK_MSP_INCLUDES}"
    "${MAX_SDK_JIT_INCLUDES}"
)

file(GLOB PROJECT_SRC "*.h" "*.c" "*.cpp")

add_library(${PROJECT_NAME} MODULE ${PROJECT_SRC})

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/source/thirdparty/DataFrame/src/Utils/DateTime.cc
)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/source/thirdparty/DataFrame/include
    ${THIRDPARTY_INCLUDE}
    ${CMAKE_BINARY_DIR}/thirdparty/OpenXLSX/OpenXLSX
)

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    ${THIRDPARTY_LIB}/libOpenXLSX.a
    ${THIRDPARTY_LIB}/libxlsxwriter.a
    -lz
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)

# Override the C++17 default set by max-posttarget -- DataFrame requires C++23
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

# DataFrame uses std::to_chars for floating point which requires macOS 13.3+
set_target_properties(${PROJECT_NAME} PROPERTIES
    XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "13.3"
)
