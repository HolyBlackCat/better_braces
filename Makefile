CXX ?= $(error CXX is not set)

CXXFLAGS := -Iinclude -g -std=c++20 -pedantic-errors -Wall -Wextra -Wdeprecated -Wextra-semi
SRC := tests.cpp

.PHONY: tests
tests:
	$(info CXX = $(CXX))
	@echo -n 'With -O0, sanitized: '
	@$(CXX) $(SRC) -o tests $(CXXFLAGS) -O0 -fsanitize=address -fsanitize=undefined && ./tests
	@echo -n 'With -O3:            '
	@$(CXX) $(SRC) -o tests $(CXXFLAGS) -O3 && ./tests


.PHONY: commands
commands:
	$(file >compile_commands.json,[{"directory":"$(CURDIR)", "file":"$(abspath $(SRC))", "command":"$(CXX) $(SRC) $(CXXFLAGS)"}])
	@true
