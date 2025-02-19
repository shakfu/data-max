
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/tests)

set(PROJECT_NAME test_numcpp2)


file(GLOB PROJECT_SRC
    "*.h"
	"*.c"
    "*.cpp"
)


add_executable( 
	${PROJECT_NAME}
	${PROJECT_SRC}
)

add_compile_definitions(
	${PROJECT_NAME}
	PUBLIC
	-DNUMCPP_NO_USE_BOOST
	# -DNUMCPP_USE_MULTITHREAD
)


target_include_directories(
	${PROJECT_NAME}
	PUBLIC
	${CMAKE_SOURCE_DIR}/source/thirdparty/include/numcpp
)

