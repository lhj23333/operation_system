# Makefile - 并行内存分配器完整构建系统
#
# 使用方法：
#   make              # 构建库和示例（Release 版本）
#   make debug        # Debug 版本构建
#   make test         # 运行所有测试
#   make clean        # 清理所有构建输出
#   make check        # 代码检查
#   make run-examples # 运行所有示例

CC := gcc
CFLAGS := -Wall -Wextra -Werror -fPIC -std=c99 -pthread
DEBUG_CFLAGS := $(CFLAGS) -g -O0 -DDEBUG
RELEASE_CFLAGS := $(CFLAGS) -O2 -DNDEBUG

LDFLAGS := -lpthread -lm

# 目录定义
SRC_DIR := src
INCLUDE_DIR := inc
BUILD_DIR := build
BIN_DIR := $(BUILD_DIR)/bin
OBJ_DIR := $(BUILD_DIR)/obj
LIB_DIR := $(BUILD_DIR)/lib
TEST_DIR := tests
EXAMPLES_DIR := examples


# 源文件
CORE_SRCS := $(SRC_DIR)/vmalloc.c $(SRC_DIR)/mem_block.c $(SRC_DIR)/heap.c $(SRC_DIR)/allocator.c $(SRC_DIR)/debug.c
CORE_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))

HEADERS := $(wildcard $(INCLUDE_DIR)/*.h)

# 测试文件
TEST_SRCS := $(wildcard $(TEST_DIR)/test_*.c)
TEST_BINS := $(patsubst $(TEST_DIR)/%.c,$(BIN_DIR)/%,$(TEST_SRCS))

# 示例文件
EXAMPLE_SRCS := $(wildcard $(EXAMPLES_DIR)/*.c)
EXAMPLE_BINS := $(patsubst $(EXAMPLES_DIR)/%.c,$(BIN_DIR)/%,$(EXAMPLE_SRCS))

# 库文件
LIB_NAME := liballocator.a
LIB_TARGET := $(LIB_DIR)/$(LIB_NAME)

# 默认目标
.PHONY: all release debug test clean check run-examples help install

all: release

# ========== 创建目录 ==========
$(BUILD_DIR) $(BIN_DIR) $(OBJ_DIR) $(LIB_DIR):
	@mkdir -p $@


# ========== 编译对象文件 ==========
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS) | $(OBJ_DIR)
	@echo "[CC] $< → $@"
	@$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@


# ========== 构建静态库 ==========
$(LIB_TARGET): $(CORE_OBJS) | $(LIB_DIR)
	@echo "[AR] 构建库: $@"
	@ar rcs $@ $(CORE_OBJS)
	@echo "[OK] 库构建完成"


# ========== Release 版本 ==========
release: CFLAGS = $(RELEASE_CFLAGS)
release: $(LIB_TARGET)
	@echo "[✓] Release 版本构建完成"


# ========== Debug 版本 ==========
debug: CFLAGS = $(DEBUG_CFLAGS)
debug: clean $(LIB_TARGET)
	@echo "[✓] Debug 版本构建完成"


# ========== 编译测试 ==========
$(BIN_DIR)/%: $(TEST_DIR)/%.c $(LIB_TARGET) $(HEADERS) | $(BIN_DIR)
	@echo "[CC] 测试: $< → $@"
	@$(CC) $(CFLAGS) -I$(INCLUDE_DIR) $< -o $@ $(LIB_TARGET) $(LDFLAGS)


# ========== 编译示例 ==========
$(BIN_DIR)/%: $(EXAMPLES_DIR)/%.c $(LIB_TARGET) $(HEADERS) | $(BIN_DIR)
	@echo "[CC] 示例: $< → $@"
	@$(CC) $(CFLAGS) -I$(INCLUDE_DIR) $< -o $@ $(LIB_TARGET) $(LDFLAGS)


# ========== 运行测试 ==========
test: debug $(TEST_BINS)
	@echo ""
	@echo "========== 运行测试 =========="
	@echo ""
	@for test in $(TEST_BINS); do \
		echo "运行: $$test"; \
		$$test || exit 1; \
		echo ""; \
	done
	@echo "========== 所有测试通过 =========="
	@echo ""


# ========== 运行示例 ==========
run-examples: $(EXAMPLE_BINS)
	@echo ""
	@echo "========== 运行示例 =========="
	@echo ""
	@for example in $(EXAMPLE_BINS); do \
		echo "运行: $$example"; \
		$$example || exit 1; \
		echo ""; \
	done
	@echo "========== 所有示例完成 =========="
	@echo ""


# ========== 代码检查 ==========
check:
	@echo "[CHECK] 代码静态检查..."
	@cppcheck --enable=all --suppress=missingIncludeSystem $(SRC_DIR) $(INCLUDE_DIR)
	@echo "[✓] 检查完成"


# ========== 内存检测（需要 valgrind）==========
valgrind: debug $(TEST_BINS)
	@echo "[VALGRIND] 内存检测..."
	@for test in $(TEST_BINS); do \
		echo "检测: $$test"; \
		valgrind --leak-check=full --show-leak-kinds=all $$test; \
	done


# ========== 清理 ==========
clean:
	@echo "[CLEAN] 清理构建输出..."
	@rm -rf $(BUILD_DIR)
	@echo "[✓] 清理完成"


# ========== 安装 ==========
install: release
	@echo "[INSTALL] 安装库和头文件..."
	@mkdir -p /usr/local/lib /usr/local/inc/allocator
	@cp $(LIB_TARGET) /usr/local/lib/
	@cp $(INCLUDE_DIR)/*.h /usr/local/inc/allocator/
	@echo "[✓] 安装完成"


# ========== 帮助信息 ==========
help:
	@echo "并行内存分配器 - 构建系统"
	@echo ""
	@echo "目标："
	@echo "  make             - 构建库和示例（Release）"
	@echo "  make debug       - Debug 版本构建"
	@echo "  make test        - 构建并运行所有测试"
	@echo "  make clean       - 清理所有构建输出"
	@echo "  make check       - 代码静态检查（需要 cppcheck）"
	@echo "  make valgrind    - 内存检测（需要 valgrind）"
	@echo "  make run-examples - 运行所有示例"
	@echo "  make install     - 安装库和头文件"
	@echo "  make help        - 显示此帮助"
	@echo ""
	@echo "示例："
	@echo "  make debug test  # Debug 版本构建并测试"
	@echo "  make clean all   # 完全重建"
	@echo "  make check       # 代码检查"