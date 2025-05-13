# install dependencies

CWD=`pwd`
THIRDPARTY=${CWD}/build/thirdparty
PREFIX=${THIRDPARTY}/install
LIBXLSXWRITER_VERSION=v1.2.0
LIBOPENXLSX_VERSION=v0.3.2

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
			-DCMAKE_INSTALL_PREFIX=${PREFIX} \
			-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
			&& \
		cmake --build . --config Release && \
		cmake --build . --target install
	else
		echo "libxlsxwriter.a already built"
	fi
}

function install_openxlsx() {
	VERSION=${LIBOPENXLSX_VERSION}
	REPO=https://github.com/troldal/OpenXLSX.git
	SRC=${THIRDPARTY}/OpenXLSX
	BUILD=${SRC}/build
    if [ ! -f ${THIRDPARTY}/install/lib/libOpenXLSX.a ]; then
    	rm -rf ${THIRDPARTY}/OpenXLSX && \
    	mkdir -p build/thirdparty && \
		git clone --depth=1 ${REPO} ${SRC} && \
		mkdir -p ${BUILD} && \
		cd ${BUILD} && \
		cmake .. \
			-DOPENXLSX_CREATE_DOCS=OFF \
			-DOPENXLSX_BUILD_SAMPLES=OFF \
			-DOPENXLSX_BUILD_TESTS=OFF \
			-DOPENXLSX_BUILD_BENCHMARKS=OFF \
			-DOPENXLSX_ENABLE_LIBZIP=OFF \
			-DCMAKE_INSTALL_PREFIX=${PREFIX} \
			-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
			&& \
		cmake --build . --config Release && \
		cmake --build . --target install
	else
		echo "libOpenXLSX.a already built"
	fi
}



setup && \
	install_libxlsxwriter && \
	install_openxlsx


