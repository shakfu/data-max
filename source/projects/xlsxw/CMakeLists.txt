include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/common.cmake)


#############################################################
# MAX EXTERNAL
#############################################################

include_directories( 
	"${MAX_SDK_INCLUDES}"
	"${MAX_SDK_MSP_INCLUDES}"
	"${MAX_SDK_JIT_INCLUDES}"
)

file(GLOB PROJECT_SRC
     "*.h"
	"*.c"
     "*.cpp"
)


add_library( 
	${PROJECT_NAME}
	MODULE
	${PROJECT_SRC}
)


target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
	${THIRDPARTY_LIB}/libxlsxwriter.a
	-lz
)


target_include_directories(
	${PROJECT_NAME}
	PRIVATE
	${THIRDPARTY_INCLUDE}
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)
