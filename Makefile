.PHONY: build test check clean tidy cppcheck sanitize configure

BUILD_DIR=build

CMAKE_FLAGS= \
	-DCMAKE_BUILD_TYPE=Debug \
	-DENABLE_ASAN=ON \
	-DENABLE_UBSAN=ON \
	-DENABLE_CLANG_TIDY=ON \
	-DENABLE_CPPCHECK=ON

configure:
	cmake -S . -B $(BUILD_DIR) -G Ninja $(CMAKE_FLAGS)

build: configure
	cmake --build $(BUILD_DIR)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

tidy: configure
	cmake --build $(BUILD_DIR) --target tidy

cppcheck: configure
	cmake --build $(BUILD_DIR) --target cppcheck

sanitize:
	cmake -S . -B $(BUILD_DIR)-asan -G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DENABLE_ASAN=ON \
		-DENABLE_UBSAN=ON
	cmake --build $(BUILD_DIR)-asan
	ctest --test-dir $(BUILD_DIR)-asan --output-on-failure

check: 
	cmake -S . -B build -G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_COLOR_DIAGNOSTICS=ON \
		-DENABLE_ASAN=ON \
		-DENABLE_UBSAN=ON \
		-DENABLE_CLANG_TIDY=ON \
		-DENABLE_CPPCHECK=ON
	cmake --build build
	ctest --test-dir build --output-on-failure

clean:
	rm -rf $(BUILD_DIR) $(BUILD_DIR)-asan
