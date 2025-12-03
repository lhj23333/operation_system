# ========================================
# GPT-2 Parallel Inference - Makefile
# ========================================

# 项目名称
PROJECT = gpt2-parallel

# 目录定义
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin
OBJ_DIR = $(BUILD_DIR)/obj

# 编译器和标志
CC = gcc
CFLAGS = -I$(INC_DIR) -Wall -Wextra -std=c11
LDFLAGS = -pthread -lm

# Debug 和 Release 配置
DEBUG_FLAGS = -g -O0 -DDEBUG -fsanitize=address -fsanitize=undefined
VALGRIND_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O3 -DNDEBUG -march=native -flto

# 源文件
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))

# 测试文件
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_BINS = $(patsubst $(TEST_DIR)/%.c, $(BIN_DIR)/%, $(TEST_SOURCES))

# ========================================
# 目录创建（必须在使用前定义）
# ========================================

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# 默认目标
.PHONY: all
all: release

# ========================================
# 编译目标
# ========================================

# Debug 版本
.PHONY: debug
debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(OBJECTS) $(TEST_BINS)
	@echo "=== Debug build complete ==="

# Release 版本
.PHONY: release
release: CFLAGS += $(RELEASE_FLAGS)
release: $(OBJECTS) $(TEST_BINS)
	@echo "=== Release build complete ==="

# 编译对象文件
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "[CC] $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

# 编译测试程序
$(BIN_DIR)/%: $(TEST_DIR)/%.c $(OBJECTS) | $(BIN_DIR)
	@echo "[LD] $< -> $@"
	$(CC) $(CFLAGS) $< $(OBJECTS) $(LDFLAGS) -o $@

# ========================================
# 测试目标
# ========================================

.PHONY: test
test: debug
	@echo "=== Running tests ==="
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done
	@echo "=== All tests passed ==="

.PHONY: test-tensor
test-tensor: debug
	@echo "=== Running tensor tests ==="
	@if [ -f $(BIN_DIR)/test_tensor_basic ]; then \
		$(BIN_DIR)/test_tensor_basic; \
	elif [ -f $(BIN_DIR)/test_tensor ]; then \
		$(BIN_DIR)/test_tensor; \
	else \
		echo "No tensor test found"; exit 1; \
	fi

.PHONY: test-threadpool
test-threadpool: debug
	@echo "=== Running thread pool tests ==="
	@if [ -f $(BIN_DIR)/test_thread_pool ]; then \
		$(BIN_DIR)/test_thread_pool; \
	else \
		echo "No thread pool test found"; exit 1; \
	fi

.PHONY: test-matrix
test-matrix: debug
	@echo "=== Running matrix tests ==="
	@if [ -f $(BIN_DIR)/test_matrix ]; then \
		$(BIN_DIR)/test_matrix; \
	else \
		echo "No matrix test found"; exit 1; \
	fi

./PHONY: test-attention
test-attention: CFLAGS += -DUSE_PARALLEL_MATMUL
test-attention: debug
	@echo "=== Running attention tests ==="
	@if [ -f $(BIN_DIR)/test_attention ]; then \
		$(BIN_DIR)/test_attention; \
	else \
		echo "No attention test found"; exit 1; \
	fi

.PHONY: benchmark
benchmark: release
	@echo "=== Running benchmarks ==="
	@for test in $(BIN_DIR)/*benchmark*; do \
		if [ -x $$test ]; then \
			echo "Running $$test..."; \
			$$test || exit 1; \
		fi; \
	done
	@echo "=== Benchmarking complete ==="
	
# ========================================
# 性能分析
# ========================================

.PHONY: perf
perf: release
	@echo "=== Running perf analysis ==="
	@perf record -e task-clock -g $(BIN_DIR)/benchmarks
	@perf report

.PHONY: perf-stat
perf-stat: release
	@echo "=== Running perf stat ==="
	@perf stat -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses \
		$(BIN_DIR)/benchmarks

.PHONY: valgrind
valgrind:
	@$(MAKE) clean_build
	@$(MAKE) valgrind-run

.PHONY: valgrind-run
valgrind-run: CFLAGS += $(VALGRIND_FLAGS)
valgrind-run: $(OBJECTS) $(TEST_BINS)
	@echo "=== Valgrind build complete ==="
	@echo "=== Running valgrind memory check on all test binaries ==="
	@for test in $(BIN_DIR)/*; do \
		if [ -x $$test ]; then \
			echo "Running valgrind on $$test..."; \
			valgrind --leak-check=full --show-leak-kinds=all \
				--track-origins=yes $$test || exit 1; \
		fi; \
	done
	@echo "=== All valgrind checks complete ==="
# ========================================
# 清理和安装
# ========================================

.PHONY: clean
clean:
	@echo "=== Cleaning build artifacts ==="
	@rm -rf $(BUILD_DIR)
	@rm -f perf.data perf.data.old
	@echo "Clean complete"

.PHONY: clean_build
clean_build:
	@rm -rf $(BUILD_DIR)

.PHONY: install
install: release
	@echo "=== Installing to /usr/local/bin ==="
	@install -d /usr/local/bin
	@install -m 755 $(BIN_DIR)/benchmark /usr/local/bin/$(PROJECT)-benchmark
	@echo "Install complete"

# ========================================
# 帮助信息
# ========================================

.PHONY: help
help:
	@echo "GPT-2 Parallel Inference - Makefile Help"
	@echo ""
	@echo "Available targets:"
	@echo "  make all		  		- Build release version (default)"
	@echo "  make debug				- Build debug version with sanitizers"
	@echo "  make release			- Build optimized release version"
	@echo "  make test				- Run all tests"
	@echo "  make test-tensor  		- Run tensor tests only"
	@echo "  make test-threadpool 	- Run thread pool tests only"
	@echo "  make test-matrix  		- Run matrix tests only"
	@echo "  make benchmark			- Run performance benchmarks"
	@echo "  make perf				- Run perf profiling"
	@echo "  make perf-stat			- Run perf cache analysis"
	@echo "  make valgrind			- Run valgrind memory check"
	@echo "  make clean				- Remove all build artifacts"
	@echo "  make install			- Install binaries to /usr/local/bin"
	@echo "  make help				- Show this help message"
	@echo ""
	@echo "Build flags:"
	@echo "  DEBUG_FLAGS   = $(DEBUG_FLAGS)"
	@echo "  RELEASE_FLAGS = $(RELEASE_FLAGS)"
	@echo ""

# ========================================
# 依赖关系
# ========================================

# 自动生成依赖
-include $(OBJECTS:.o=.d)

$(OBJ_DIR)/%.d: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@$(CC) $(CFLAGS) -MM -MT '$(OBJ_DIR)/$*.o' $< > $@