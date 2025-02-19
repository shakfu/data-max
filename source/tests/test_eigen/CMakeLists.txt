
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/tests)

set(PROJECT_NAME test_eigen)

file(GLOB PROJECT_SRC
    "*.h"
	"*.c"
    "*.cpp"
)


add_executable( 
	${PROJECT_NAME}
	${PROJECT_SRC}
)

# target_link_libraries(
# 	${PROJECT_NAME}
# 	PUBLIC
# )

target_include_directories(
	${PROJECT_NAME}
	PUBLIC
	${CMAKE_SOURCE_DIR}/source/thirdparty/include/eigen3
)
