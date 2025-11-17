#!/bin/bash

# ============================================================================
# MPC Protocol Benchmarking Script - Independent Parameter Analysis
# ============================================================================
# This script tests each parameter independently to understand their
# individual impact on execution time.
#
# Three sets of 50 tests each:
# 1. Vary n (10-500), keep m=100, k=50
# 2. Vary m (10-500), keep n=100, k=50
# 3. Vary k (10-500), keep n=100, m=100
#
# This provides 150 datapoints total for detailed complexity analysis.
#
# Usage: ./benchmark_independent.sh
# ============================================================================

set -e

OUTPUT_FILE="timing_results_independent.csv"
LOG_DIR="benchmark_logs_independent"

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_msg() {
    local color=$1
    shift
    echo -e "${color}$@${NC}"
}

# Create log directory
mkdir -p "$LOG_DIR"

if [ ! -f "gen_queries.cpp" ]; then
    print_msg "$RED" "Error: gen_queries.cpp not found!"
    exit 1
fi

if [ ! -f "docker-compose.yml" ]; then
    print_msg "$RED" "Error: docker-compose.yml not found!"
    exit 1
fi

# Create CSV header
echo "n,m,k,q,execution_time_seconds,parameter_varied" > "$OUTPUT_FILE"

print_msg "$GREEN" "============================================"
print_msg "$GREEN" "  MPC Protocol Independent Parameter Test"
print_msg "$GREEN" "============================================"
print_msg "$BLUE" "Test 1: Vary n (10-500), keep m=100, k=50"
print_msg "$BLUE" "Test 2: Vary m (10-500), keep n=100, k=50"
print_msg "$BLUE" "Test 3: Vary k (10-500), keep n=100, m=100"
print_msg "$BLUE" "Total: 150 datapoints"
echo ""

# Compile gen_queries
print_msg "$YELLOW" "Compiling gen_queries..."
g++ gen_queries.cpp -o gen_queries

# Build Docker image ONCE
print_msg "$YELLOW" "Building Docker images..."
docker compose down -v > /dev/null 2>&1 || true
docker compose build > /dev/null 2>&1

echo ""

# Function to run tests
run_tests() {
    local param_name=$1
    local base_n=$2
    local base_m=$3
    local base_k=$4
    local vary_param=$5

    print_msg "$YELLOW" "\n=========================================="
    print_msg "$YELLOW" "$param_name Tests"
    print_msg "$YELLOW" "=========================================="

    local test_count=0
    for val in {10..500..10}; do
        test_count=$((test_count + 1))

        # Set parameters based on which one varies
        if [ "$vary_param" = "n" ]; then
            n=$val; m=$base_m; k=$base_k
        elif [ "$vary_param" = "m" ]; then
            n=$base_n; m=$val; k=$base_k
        elif [ "$vary_param" = "k" ]; then
            n=$base_n; m=$base_m; k=$val
        fi

        q=1

        percent=$((test_count * 100 / 50))
        print_msg "$BLUE" "  [$test_count/50] ($percent%) - Varying $vary_param: $val"

        # Generate query files
        echo -e "$n\n$m\n$k\n$q" | ./gen_queries > /dev/null 2>&1

        # Clean up and run
        docker compose down -v > /dev/null 2>&1 || true

        START_TIME=$(date +%s.%N)
        LOG_FILE="$LOG_DIR/test_vary_${vary_param}_${val}.log"
        docker compose up > "$LOG_FILE" 2>&1 || true
        END_TIME=$(date +%s.%N)

        EXECUTION_TIME=$(echo "$END_TIME - $START_TIME" | bc)

        echo "$n,$m,$k,$q,$EXECUTION_TIME,$vary_param=$val" >> "$OUTPUT_FILE"

        sleep 1
    done
}

# Run independent tests
run_tests "Varying n" 100 100 50 "n"
run_tests "Varying m" 100 100 50 "m"
run_tests "Varying k" 100 100 50 "k"

# Cleanup
rm -f gen_queries
docker compose down -v > /dev/null 2>&1 || true

print_msg "$GREEN" "========================================"
print_msg "$GREEN" "✓ Independent Tests Complete!"
print_msg "$GREEN" "========================================"
print_msg "$BLUE" "Results saved to: $OUTPUT_FILE"
print_msg "$BLUE" "Logs saved to: $LOG_DIR/"
echo ""

print_msg "$YELLOW" "First 15 tests (varying n):"
column -t -s',' "$OUTPUT_FILE" | head -16 | tail -15

print_msg "$YELLOW" "Next 15 tests (varying m):"
column -t -s',' "$OUTPUT_FILE" | head -31 | tail -15

print_msg "$BLUE" "\nNext: python3 plot_results.py"
