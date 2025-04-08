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
			-DCMAKE_INSTALL_PREFIX=${PREFIX} \
			-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
			&& \
		cmake --build . --config Release && \
		cmake --build . --target install
	else
		echo "libxlsxwriter.a already built"
	fi
}



setup && \
	install_libxlsxwriter 
