CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -pedantic -O2 -g
TARGET := main
SOURCE := main.cpp

.PHONY: all run test clean help

all: $(SOURCE)
	$(CXX) $(CXXFLAGS) $< -o $(TARGET)

run: all
	./$(TARGET)

test: all
	@test "$$(./$(TARGET))" = "6040578838"
	@echo "baseline output matched"

clean:
	rm -f $(TARGET)

help:
	@echo "Targets:"
	@echo "  all    - build the baseline program"
	@echo "  run    - build and run the baseline program"
	@echo "  test   - check the baseline output"
	@echo "  clean  - remove the binary"
