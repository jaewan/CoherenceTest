CC = gcc
CFLAGS = -g -Wall -pthread -Iinclude -O2
LDFLAGS = -pthread

SRC_DIR = src
BUILD_DIR = build

# Source files (excluding test files)
MAIN_SRCS = $(filter-out $(SRC_DIR)/test_%.c $(SRC_DIR)/standalone_%.c, $(wildcard $(SRC_DIR)/*.c))
TEST_SRCS = $(wildcard $(SRC_DIR)/test_*.c) $(wildcard $(SRC_DIR)/standalone_*.c)

# Object files
MAIN_OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(MAIN_SRCS))
TEST_OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(TEST_SRCS))

# Main executables
MAIN_EXECUTABLE = main
EXPERIMENT_EXECUTABLE = experiment

# Test programs
TEST_PROGRAMS = test_header test_minimal test_sync test_workload test_workload_minimal standalone_test

.PHONY: all clean test run_experiment help

all: $(MAIN_EXECUTABLE) $(EXPERIMENT_EXECUTABLE)

# Main test program
$(MAIN_EXECUTABLE): $(BUILD_DIR)/main.o $(filter-out $(BUILD_DIR)/experiment.o, $(MAIN_OBJS))
	$(CC) $(LDFLAGS) $^ -o $@

# Full experiment program
$(EXPERIMENT_EXECUTABLE): $(BUILD_DIR)/experiment.o $(filter-out $(BUILD_DIR)/main.o, $(MAIN_OBJS))
	$(CC) $(LDFLAGS) $^ -o $@

# Generic object file rule
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Test programs
test: $(TEST_PROGRAMS)

$(TEST_PROGRAMS): %: $(BUILD_DIR)/%.o $(MAIN_OBJS)
	$(CC) $(LDFLAGS) $^ -o $(BUILD_DIR)/$@

# Run basic tests
check: $(MAIN_EXECUTABLE)
	@echo "Running basic functionality tests..."
	./$(MAIN_EXECUTABLE)

# Run full experiment
run_experiment: $(EXPERIMENT_EXECUTABLE)
	@echo "Running federated coherence experiment..."
	./$(EXPERIMENT_EXECUTABLE)

# Run quick experiment
quick: $(EXPERIMENT_EXECUTABLE)
	@echo "Running quick experiment (reduced workload)..."
	./$(EXPERIMENT_EXECUTABLE) -t 2 -i 100 -c 10000 -v

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(MAIN_EXECUTABLE) $(EXPERIMENT_EXECUTABLE)

# Help target
help:
	@echo "Federated Coherence Experiment Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all            - Build main test program and experiment"
	@echo "  main           - Build basic functionality test"
	@echo "  experiment     - Build full experiment program"
	@echo "  test           - Build all test programs"
	@echo "  check          - Run basic functionality tests"
	@echo "  run_experiment - Run full experiment"
	@echo "  quick          - Run quick experiment with reduced workload"
	@echo "  clean          - Clean build artifacts"
	@echo "  help           - Show this help"
	@echo ""
	@echo "Example usage:"
	@echo "  make && make check          # Build and run basic tests"
	@echo "  make run_experiment         # Build and run full experiment"
	@echo "  make quick                  # Build and run quick experiment"
	@echo "  ./experiment -s federated   # Run only federated coherence system"
	@echo "  ./experiment -h             # Show experiment help"

# Verbose option
ifdef VERBOSE
Q = 
else
Q = @
endif
