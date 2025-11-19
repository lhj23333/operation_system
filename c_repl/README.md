# C REPL - 完整三阶段实现

一个用 C 语言实现的**完整交互式 REPL** 系统，支持表达式求值、函数定义、动态编译和链接。

## ✅ 项目完成度

### 第 1 阶段 ✓ (完成)
- [x] 项目框架搭建
- [x] Makefile 构建系统
- [x] GNU readline 集成
- [x] 输入分类系统
- [x] 命令处理系统（help、list、clear、exit）
- [x] 自动清理机制

### 第 2 阶段 ✓ (完成)
- [x] 表达式解析器（递归下降算法）
- [x] 词法分析（Lexer）
- [x] 语法分析与求值（Parser & Evaluator）
- [x] 支持四则运算、括号、整数/浮点数
- [x] 动态编译和执行
- [x] 结果格式化输出

### 第 3 阶段 ✓ (完成)
- [x] 函数定义解析
- [x] 自动提取函数名
- [x] 编译为动态库（.so 文件）
- [x] 动态库加载（dlopen/dlsym）
- [x] 函数管理系统
- [x] 程序退出自动清理

## 📁 项目结构

```
c-repl/
├── Makefile              # 三阶段完整构建
├── repl.c                # 主程序 (650+ 行)
├── expr_parser.h         # 表达式解析器头文件
├── expr_parser.c         # 表达式解析器实现
├── func_manager.h        # 函数管理器头文件
├── func_manager.c        # 函数管理器实现
├── .gitignore
└── README.md
```

## 🚀 快速开始

### 依赖

- GNU readline library
- GCC/Clang 编译器
- Linux 或 macOS

### 安装依赖

**Ubuntu/Debian:**
```bash
sudo apt-get install libreadline-dev build-essential
```

**macOS:**
```bash
brew install readline
```

### 编译和运行

```bash
make run              # 编译并运行
make clean            # 清理
make debug            # 调试编译
```

## 📖 使用示例

### 1. 表达式求值（第2阶段）

```
c-repl> 2 + 3 * 4
=> 14  (int)

c-repl> (10 + 5) / 3
=> 5  (double)

c-repl> 2.5 * 4
=> 10  (double)
```

### 2. 函数定义（第3阶段）

```
c-repl> int add(int a, int b) { return a + b; }
[INFO] Added function: add
[INFO] Function compiled: libs/libfunc_0.so
[SUCCESS] Function added successfully (ID: 0)

c-repl> int multiply(int x, int y) { return x * y; }
[INFO] Added function: multiply
[INFO] Function compiled: libs/libfunc_1.so
[SUCCESS] Function added successfully (ID: 1)
```

### 3. 列出函数

```
c-repl> list
╔════════════════════════════════════════════════════════════╗
║                   Defined Functions (2)                   ║
╠════════════════════════════════════════════════════════════╣
║  [0] add                                                   ║
║  [1] multiply                                              ║
╚════════════════════════════════════════════════════════════╝
```

### 4. 帮助

```
c-repl> help
```

### 5. 退出

```
c-repl> exit
# 或按 Ctrl+D
```

## 💡 技术细节

### 第 1 阶段：框架搭建
- **readline 集成**: 提供命令历史和编辑功能
- **输入分类**: 自动识别表达式、函数、命令
- **资源清理**: atexit() 自动清理 libs/ 目录

### 第 2 阶段：表达式解析
**词法分析 (Lexer):**
- 将输入字符流分解为 Token
- 支持数字、运算符、括号

**语法分析 (Parser):**
- 递归下降算法
- 优先级处理: 表达式 > 项 > 因子
- 支持括号和一元运算符

**求值 (Evaluator):**
```c
// 支持的运算符
+    加法
-    减法（包括一元负）
*    乘法
/    除法
%    取模
()   括号
```

### 第 3 阶段：函数定义与动态库
- **函数解析**: 正则表达式提取函数名
- **代码生成**: 生成临时 C 文件
- **编译**: gcc -shared -fPIC 生成 .so
- **链接**: dlopen/dlsym 动态加载
- **管理**: FunctionManager 结构管理所有函数

## 🔍 代码组织

### repl.c (650+ 行)
```
1. 常量和颜色定义
2. 全局变量（函数管理器）
3. 函数声明
4. main() 主函数
5. 初始化函数
6. 输入分类
7. 输入处理
8. 命令处理
9. 表达式执行
10. 函数定义
11. 清理函数
12. 工具函数
```

### expr_parser.c (250+ 行)
```
1. Lexer (词法分析器)
   - 数字识别
   - 运算符识别
   - Token 流处理

2. Parser (语法分析器)
   - parse_expression()  表达式级
   - parse_term()        项级（* / %）
   - parse_factor()      因子级（数字、括号）

3. 错误处理
   - 除零检测
   - 括号匹配检测
   - 表达式有效性检查
```

### func_manager.c (200+ 行)
```
1. 函数名提取
   - 正则表达式匹配
   - 符号解析

2. 函数管理
   - 存储函数定义
   - 追踪函数元数据

3. 编译链接
   - 生成临时 C 文件
   - gcc 动态库编译
   - dlopen 动态加载

4. 清理机制
   - dlclose 卸载库
   - 释放内存
```

## 🧪 测试用例

```bash
# 简单算术
c-repl> 1 + 1
c-repl> 10 - 3 * 2

# 括号和优先级
c-repl> (2 + 3) * 4
c-repl> 100 / (2 + 3)

# 浮点数
c-repl> 3.14 * 2
c-repl> 1.5 / 2

# 函数定义
c-repl> int square(int x) { return x * x; }
c-repl> int cube(int x) { return x * x * x; }

# 函数管理
c-repl> list
c-repl> help
```

## 🎯 学习要点

### 编译原理
- 词法分析（Lexer）
- 语法分析（Parser）
- 代码生成（Code Generation）
- 求值（Evaluation）

### 系统编程
- 动态链接库 (dlopen, dlsym, dlclose)
- 进程管理 (system() 调用)
- 文件操作 (文件生成和清理)
- 内存管理 (malloc, free)

### C 编程技巧
- 递归下降解析
- 函数指针
- 结构体管理
- 正则表达式

## 🔗 扩展思路

1. **更强大的表达式**
   - 支持变量存储
   - 支持函数调用
   - 支持更多内置函数 (sin, cos, sqrt 等)

2. **增强的函数支持**
   - 参数类型检查
   - 返回值类型推导
   - 函数重载

3. **调试工具**
   - 单步执行
   - 变量监视
   - 执行跟踪

4. **性能优化**
   - 编译缓存
   - 增量编译
   - 并行处理

## 📝 许可证

MIT License

## 👤 作者

Learning Project - Complete 3-Phase Implementation

---

**完整的 C REPL 实现已完成！🎉**

从框架搭建、表达式解析、到函数定义与动态链接，一个完整的交互式 REPL 系统。

继续学习，不断深化对系统编程的理解！💪