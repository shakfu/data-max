# install dependencies

CWD=`pwd`
THIRDPARTY=${CWD}/build/thirdparty
PREFIX=${THIRDPARTY}/install
LIBXLSXWRITER_VERSION=v1.2.0
LIBOPENXLSX_VERSION=v0.3.2
ZLIB_VERSION=v1.3.1

# detect platform and set cmake flags
EXTRA_CMAKE_ARGS=()

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		IS_WINDOWS=1
		LIB_EXT=".lib"
		LIB_PREFIX=""
		# use static CRT (/MT) to match the Max SDK
		EXTRA_CMAKE_ARGS+=("-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded")
		EXTRA_CMAKE_ARGS+=("-DCMAKE_POLICY_DEFAULT_CMP0091=NEW")
		;;
	Darwin*)
		IS_WINDOWS=0
		LIB_EXT=".a"
		LIB_PREFIX="lib"
		EXTRA_CMAKE_ARGS+=("-DCMAKE_OSX_DEPLOYMENT_TARGET=10.15")
		# BUILD_UNIVERSAL env var: build thirdparty libs as universal (x86_64+arm64)
		if [ "${BUILD_UNIVERSAL}" = "YES" ] || [ "${BUILD_UNIVERSAL}" = "1" ]; then
			EXTRA_CMAKE_ARGS+=("-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64")
		fi
		;;
	*)
		IS_WINDOWS=0
		LIB_EXT=".a"
		LIB_PREFIX="lib"
		;;
esac

function setup() {
	mkdir -p ${PREFIX}/include && \
	mkdir -p ${PREFIX}/lib
}

function install_zlib() {
	# on Unix, zlib is a system library -- only build on Windows
	if [ "${IS_WINDOWS}" -eq 0 ]; then
		return 0
	fi
	VERSION=${ZLIB_VERSION}
	REPO=https://github.com/madler/zlib.git
	SRC=${THIRDPARTY}/zlib
	BUILD=${SRC}/build
	if [ ! -f ${PREFIX}/lib/zlibstatic.lib ] && [ ! -f ${PREFIX}/lib/zlibstaticd.lib ]; then
		rm -rf ${SRC} && \
		mkdir -p build/thirdparty && \
		git clone -b "${VERSION}" --depth=1 ${REPO} ${SRC} && \
		mkdir -p ${BUILD} && \
		cd ${BUILD} && \
		cmake .. \
			"${EXTRA_CMAKE_ARGS[@]}" \
			-DCMAKE_INSTALL_PREFIX=${PREFIX} \
			-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
			&& \
		cmake --build . --config Release && \
		cmake --build . --target install --config Release
	else
		echo "zlib already built"
	fi
}

function install_libxlsxwriter() {
	VERSION=${LIBXLSXWRITER_VERSION}
	REPO=https://github.com/jmcnamara/libxlsxwriter
	SRC=${THIRDPARTY}/libxlsxwriter
	BUILD=${SRC}/build
    if [ ! -f ${PREFIX}/lib/${LIB_PREFIX}xlsxwriter${LIB_EXT} ]; then
    	rm -rf ${THIRDPARTY}/libxlsxwriter && \
    	mkdir -p build/thirdparty && \
		git clone -b "${VERSION}" --depth=1 ${REPO} ${SRC} && \
		mkdir -p ${BUILD} && \
		cd ${BUILD} && \
		# USE_MEM_FILE requires fmemopen/open_memstream (POSIX-only)
		if [ "${IS_WINDOWS}" -eq 1 ]; then
			USE_MEM_FILE_FLAG="-DUSE_MEM_FILE=OFF"
		else
			USE_MEM_FILE_FLAG="-DUSE_MEM_FILE=ON"
		fi
		cmake .. \
			-DBUILD_TESTS=OFF \
			-DBUILD_EXAMPLES=OFF \
			-DBUILD_FUZZERS=OFF \
			-DUSE_SYSTEM_MINIZIP=OFF \
			-DUSE_STANDARD_TMPFILE=OFF \
			-DUSE_NO_MD5=OFF \
			-DUSE_OPENSSL_MD5=OFF \
			${USE_MEM_FILE_FLAG} \
			-DIOAPI_NO_64=OFF \
			-DUSE_DTOA_LIBRARY=OFF \
			-DCMAKE_INSTALL_PREFIX=${PREFIX} \
			-DCMAKE_PREFIX_PATH=${PREFIX} \
			-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
			"${EXTRA_CMAKE_ARGS[@]}" \
			&& \
		cmake --build . --config Release && \
		cmake --build . --target install --config Release
	else
		echo "xlsxwriter already built"
	fi
}

function install_openxlsx() {
	VERSION=${LIBOPENXLSX_VERSION}
	REPO=https://github.com/troldal/OpenXLSX.git
	SRC=${THIRDPARTY}/OpenXLSX
	BUILD=${SRC}/build
    if [ ! -f ${PREFIX}/lib/${LIB_PREFIX}OpenXLSX${LIB_EXT} ]; then
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
			"${EXTRA_CMAKE_ARGS[@]}" \
			&& \
		cmake --build . --config Release && \
		cmake --build . --target install --config Release
	else
		echo "OpenXLSX already built"
	fi
}



setup && \
	install_zlib && \
	install_libxlsxwriter && \
	install_openxlsx
