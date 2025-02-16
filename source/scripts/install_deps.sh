# install dependencies

CWD=`pwd`
THIRDPARTY=${CWD}/build/thirdparty
PREFIX=${THIRDPARTY}/install
LIBXLSXWRITER_VERSION=v1.2.0
MATPLOTPLUSPLUS_VERSION=v1.2.2

function setup() {
	mkdir -p ${PREFIX}/include && \
	mkdir -p ${PREFIX}/lib
}

function install_libxlsxwriter() {
	VERSION=${LIBXLSXWRITER_VERSION}
	REPO=https://github.com/jmcnamara/libxlsxwriter
	SRC=${THIRDPARTY}/libxlsxwriter
	BUILD=${SRC}/build
    if [ ! -f ${THIRDPARTY}/install/lib/libxlsxwriter.a ]; then
    	rm -rf ${THIRDPARTY}/libxlsxwriter && \
    	mkdir -p build/thirdparty && \
		git clone -b "${VERSION}" --depth=1 ${REPO} ${SRC} && \
		mkdir -p ${BUILD} && \
		cd ${BUILD} && \
		cmake .. \
			-DBUILD_TESTS=OFF \
			-DBUILD_EXAMPLES=OFF \
			-DBUILD_FUZZERS=OFF \
			-DUSE_SYSTEM_MINIZIP=OFF \
			-DUSE_STANDARD_TMPFILE=OFF \
			-DUSE_NO_MD5=OFF \
			-DUSE_OPENSSL_MD5=OFF \
			-DUSE_MEM_FILE=ON \
			-DIOAPI_NO_64=OFF \
			-DUSE_DTOA_LIBRARY=OFF \
			-DCMAKE_INSTALL_PREFIX=${PREFIX} && \
		cmake --build . --config Release && \
		cmake --build . --target install
	else
		echo "libxlsxwriter.a already built"
	fi
}


function install_matplotplusplus() {
	VERSION=${MATPLOTPLUSPLUS_VERSION}
	REPO=https://github.com/alandefreitas/matplotplusplus.git
	SRC=${THIRDPARTY}/matplotplusplus
	BUILD=${SRC}/build
    if [ ! -d ${THIRDPARTY}/matplotplusplus ]; then
    	rm -rf ${THIRDPARTY}/matplotplusplus && \
    	mkdir -p build/thirdparty && \
		git clone -b "${VERSION}" --depth=1 ${REPO} ${SRC}
	else
		echo "libmatplotplusplus already built"
	fi
}

# function install_matplotplusplus() {
# 	VERSION=${MATPLOTPLUSPLUS_VERSION}
# 	REPO=https://github.com/alandefreitas/matplotplusplus.git
# 	SRC=${THIRDPARTY}/matplotplusplus
# 	BUILD=${SRC}/build
#     if [ ! -f ${THIRDPARTY}/install/lib/libmatplot.a ]; then
#     	rm -rf ${THIRDPARTY}/matplotplusplus && \
#     	mkdir -p build/thirdparty && \
# 		git clone -b "${VERSION}" --depth=1 ${REPO} ${SRC} && \
# 		mkdir -p ${BUILD} && \
# 		cd ${BUILD} && \
# 		cmake .. \
# 			-DMATPLOTPP_BUILD_TESTS=OFF \
# 			-DMATPLOTPP_BUILD_EXAMPLES=OFF \
# 			-DMATPLOTPP_BUILD_INSTALLER=OFF \
# 			-DMATPLOTPP_BUILD_PACKAGE=OFF \
# 			-DMATPLOTPP_BUILD_SHARED_LIBS=OFF \
# 			-DMATPLOTPP_BUILD_WITH_PEDANTIC_WARNING=OFF \
# 			-DMATPLOTPP_BUILD_WITH_SANITIZERS=OFF \
# 			-DMATPLOTPP_BUILD_WITH_MSVC_HACKS=ON \
# 			-DMATPLOTPP_BUILD_WITH_UTF8=ON \
# 			-DMATPLOTPP_BUILD_WITH_EXCEPTIONS=OFF \
# 			-DMATPLOTPP_BUILD_HIGH_RESOLUTION_WORLD_MAP=OFF \
# 			-DMATPLOTPP_BUILD_FOR_DOCUMENTATION_IMAGES=OFF \
# 			-DMATPLOTPP_BUILD_EXPERIMENTAL_OPENGL_BACKEND=OFF \
# 			-DMATPLOTPP_WITH_OPENCV=OFF \
# 			-DMATPLOTPP_WITH_SYSTEM_CIMG=OFF \
# 			-DMATPLOTPP_WITH_SYSTEM_NODESOUP=OFF \
# 			-DCMAKE_BUILD_TYPE=Release \
# 			-DCMAKE_INSTALL_PREFIX=${PREFIX} && \
# 		cmake --build . --config Release && \
# 		cmake --build . --target install && \
# 		mv ${BUILD}/source/matplot/libmatplot.a ${PREFIX}/lib/ && \
# 		mv ${BUILD}/source/3rd_party/libnodesoup.a ${PREFIX}/lib/ && \
# 		rm -rf ${PREFIX}/lib/Matplot++ && \
# 		cp -rf ${SRC}/source/matplot/axes_objects ${PREFIX}/include/matplot && \
# 		cp -rf ${SRC}/source/matplot/backend ${PREFIX}/include/matplot && \
# 		cp -rf ${SRC}/source/matplot/core ${PREFIX}/include/matplot && \
# 		cp -rf ${SRC}/source/matplot/freestanding ${PREFIX}/include/matplot && \
# 		cp -rf ${SRC}/source/matplot/util ${PREFIX}/include/matplot && \
# 		cp -rf ${SRC}/source/matplot/detail/config.h ${PREFIX}/include/matplot/detail && \
# 		cp -rf ${SRC}/source/matplot/matplot.h ${PREFIX}/include/matplot && \
# 		cp -f ${SRC}/source/3rd_party/nodesoup/include/nodesoup.hpp ${PREFIX}/include/ && \
# 		cp -f ${SRC}/source/3rd_party/cimg/Cimg.h ${PREFIX}/include/ && \
# 		rm -f ${PREFIX}/include/matplot/CMakeLists.txt && \
# 		find ${PREFIX}/include/matplot -type f -name '*.cpp' -exec rm {} \;
# 	else
# 		echo "libmatplotplusplus.a already built"
# 	fi
# }

setup && \
	install_libxlsxwriter && \
	install_matplotplusplus
