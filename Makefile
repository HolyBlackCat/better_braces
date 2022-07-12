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
COMPILER := $(sort $(shell bash -c 'compgen -c g++; compgen -c clang++' | grep -Po '^(clan)?g\+\+(-[0-9]+)?(?=.exe)?'))
$(if $(COMPILER),,$(error Unable to detect compilers, set `COMPILER=??` to a space-separated compiler list))

# C++ standards to test. Override this with a subset of standards if you want to.
STANDARD := 20 17 14 11

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
else ifneq ($(words $(OPTIMIZE)),1)
	@true $(foreach x,$(OPTIMIZE),&& make --no-print-directory OPTIMIZE=$x)
else
	@printf "%-11s C++%-3s %-15s...  " $(COMPILER) $(STANDARD) $(OPTIMIZE)
	@$(COMPILER) $(SRC) -o tests $(CXXFLAGS) $(OPTIM_FLAGS_$(OPTIMIZE)) -std=c++$(STANDARD) && ./tests
endif

.PHONY: commands
commands:
	$(eval override cxx := $(lastword $(filter clang++%,$(COMPILER))))
	$(if $(cxx),,$(error Unable to guess the compiler))
	$(eval override std := $(firstword $(STANDARD)))
	$(if $(std),,$(error Unable to guess the C++ standard version))
	$(file >compile_commands.json,[{"directory":"$(CURDIR)", "file":"$(abspath $(SRC))", "command":"$(cxx) $(SRC) $(CXXFLAGS) -std=c++$(std)"}])
	@true
