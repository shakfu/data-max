MAX_VERSION := 9
PACKAGE_NAME := data-max
MAX_PACKAGE := "$(HOME)/Documents/Max\ $(MAX_VERSION)/Packages/$(PACKAGE_NAME)"
SCRIPTS := source/scripts
BUILD := build
CONFIG = Release
THIRDPARTY = $(BUILD)/thirdparty
LIB = $(THIRDPARTY)/install/lib
DIST = $(BUILD)/dist/$(PACKAGE_NAME)
ARCH=$(shell uname -m)
DMG=$(PACKAGE_NAME)-$(VERSION)-$(ARCH).dmg
ENTITLEMENTS = source/scripts/entitlements.plist
VERSION=0.1.0

.PHONY: all install_deps build universal setup clean reset

all: build


build: install_deps
	@mkdir -p build && \
		cd build && \
		cmake -GXcode ..  \
			-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
			&& \
		cmake --build . --config '$(CONFIG)' && \
		cmake --install . --config '$(CONFIG)'

universal: install_deps
	@mkdir -p build && \
		cd build && \
		cmake -GXcode ..  \
			-DC74_BUILD_FAT=ON \
			-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
			&& \
		cmake --build . --config '$(CONFIG)' && \
		cmake --install . --config '$(CONFIG)'


install_deps:
	./source/scripts/install_deps.sh

clean:
	@rm -rf \
		build/$(CONFIG) \
		build/*.ck \
		build/*.cmake \
		build/*.xcodeproj \
		build/build \
		build/CMake* \
		build/install* \
		build/package \
		build/source \
		externals

reset:
	@rm -rf build externals

setup:
	@git submodule init
	@git submodule update
	@if ! [ -L "$(MAX_PACKAGE)" ]; then \
		ln -s "$(shell pwd)" "$(MAX_PACKAGE)" ; \
		echo "... symlink created" ; \
	else \
		echo "... symlink already exists" ; \
	fi

