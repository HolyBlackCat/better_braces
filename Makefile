CXX ?= $(error CXX is not set)

# Optimization modes to test. Override this with a subset of modes if you want to.
OPTIMIZE := O0_asan_ubsan O0 O3
OPTIM_NAME_O0_asan_ubsan := '-O0, ASAN+UBSAN'
OPTIM_FLAGS_O0_asan_ubsan := -O0 -fsanitize=address -fsanitize=undefined
OPTIM_NAME_O0 := '-O0            '
OPTIM_FLAGS_O0 := -O0
OPTIM_NAME_O3 := '-O3            '
OPTIM_FLAGS_O3 := -O3
$(foreach x,$(OPTIMIZE),$(if $(OPTIM_FLAGS_$x),,$(error Unknown optimization level: $x)))

# Compilers to test. If not specified, we find all versions of GCC and Clang in PATH.
# Note that we only use version-suffixed compilers. The unsufixed compilers should be linked to one of those anyway?
COMPILER := $(sort $(shell bash -c 'compgen -c g++-; compgen -c clang++-' | grep -Po '^(clan)?g\+\+(-[0-9]+)?(?=.exe)?'))
$(if $(COMPILER),,$(error Unable to detect compilers, set `COMPILER=??` to a space-separated compiler list))

# C++ standards to test. Override this with a subset of standards if you want to.
STANDARD := 20 17 14

# C++ standard libraries to test.
STDLIB := libstdc++ libc++

# Per-compiler flag customization. A list of `compiler=flag`. All matching flags for the current compiler are applied.
CXXFLAGS_PER_COMPILER :=

CXXFLAGS_DEFAULT := -Iinclude -g -pedantic-errors -Wall -Wextra -Wdeprecated -Wextra-semi -ftemplate-backtrace-limit=0
CXXFLAGS :=
override CXXFLAGS += $(CXXFLAGS_DEFAULT)

SRC := tests.cpp

.PHONY: tests
tests:
ifneq ($(words $(COMPILER)),1)
	@true $(foreach x,$(COMPILER),&& make --no-print-directory COMPILER=$x)
else ifneq ($(words $(STANDARD)),1)
	@true $(foreach x,$(STANDARD),&& make --no-print-directory STANDARD=$x)
else ifneq ($(words $(STDLIB)),1)
	@true $(foreach x,$(STDLIB),&& make --no-print-directory STDLIB=$x)
else ifneq ($(words $(OPTIMIZE)),1)
	@true $(foreach x,$(OPTIMIZE),&& make --no-print-directory OPTIMIZE=$x)
else ifneq ($(and $(filter g++%,$(COMPILER)),$(filter libc++,$(STDLIB))),)
	@true # Unsupported C++ standard library for this compiler.
else ifeq ($(shell $(if $(filter g++%,$(COMPILER)),$(COMPILER) -v --help,$(COMPILER) -std=c++0 -xc++ /dev/null) 2>&1 | grep 'c++$(STANDARD)'),)
	@true # Unsupported standard version for this compiler.
else ifneq ($(and $(filter clang++%,$(COMPILER)),$(filter libc++,$(STDLIB)),$(if $(wildcard /usr/lib/llvm-$(shell $(COMPILER) --version | grep -Po '(?<=version )[0-9]+')/include/c++),,x)),)
	@true # Using Clang with libc++, but libc++ is not installed.
else
	@printf "%-11s C++%-3s %-10s %-15s...  " $(COMPILER) $(STANDARD) $(STDLIB) $(OPTIMIZE)
	@$(COMPILER) $(SRC) -o tests $(CXXFLAGS) $(OPTIM_FLAGS_$(OPTIMIZE)) -std=c++$(STANDARD) $(if $(filter g++%,$(COMPILER)),,-stdlib=$(STDLIB)) \
		$(patsubst $(COMPILER)=%,%,$(filter $(COMPILER)=%,$(CXXFLAGS_PER_COMPILER))) && ./tests
endif

.PHONY: commands
commands:
	$(eval override cxx := $(lastword $(filter clang++%,$(COMPILER))))
	$(if $(cxx),,$(error Unable to guess the compiler))
	$(eval override std := $(firstword $(STANDARD)))
	$(if $(std),,$(error Unable to guess the C++ standard version))
	$(file >compile_commands.json,[{"directory":"$(CURDIR)", "file":"$(abspath $(SRC))", "command":"$(cxx) $(SRC) $(CXXFLAGS) -std=c++$(std)"}])
	@true
