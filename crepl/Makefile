# ============================================================================
# C REPL - Makefile
# ============================================================================

# Compiler and compilation options
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -fPIC
LDFLAGS = -lreadline -ldl -lm

# Source and object files
TARGET = crepl
SOURCES = crepl.c expr_parser.c func_manager.c
OBJECTS = $(SOURCES:.c=.o) 
LIBS_DIR = ./libs

# Build rules
all: $(LIBS_DIR) $(TARGET)

$(LIBS_DIR):
	@mkdir -p $(LIBS_DIR)
	@echo "Created $(LIBS_DIR) directory."

$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET)..."
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Build complete: ./$(TARGET)"

%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Run program
run: all
	@echo "Starting c-repl..."
	@echo "  Type 'help' for commands, 'exit' to quit"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	./$(TARGET)

# Clean up build artifacts
clean:
	@echo "Cleaning up..."
	@rm -f $(TARGET)
	@rm -f *.o
	@rm -rf $(LIBS_DIR)
	@echo "Clean complete."

distclean: clean
	@echo "Deep cleaning..."
	@rm -f temp_*.c
	@rm -f temp_*.o
	@echo "Distclean complete."

# Debugging
debug: CFLAGS += -DDEBUG
debug: clean all
	@echo "Debug build complete."

# Help message
help:
	@echo "C REPL - Makefile Targets"
	@echo "═════════════════════════════════════════════════"
	@echo "  make              - Build the project"
	@echo "  make run          - Build and run REPL"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make distclean    - Full cleanup"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make help         - Show this help"
	@echo "═════════════════════════════════════════════════"

# Phony targets
.PHONY: all run clean distclean debug help