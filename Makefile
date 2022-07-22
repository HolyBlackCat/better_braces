# This makefile only runs tests.
# By default it tests all available compilers with various options, but you can restrict the test matrix by setting the variables defined below.

# Optimization modes to test. Override this with a subset of modes if you want to.
OPTIMIZE := O0_sanitized O0 Omax
OPTIM_NAME_O0_sanitized := '-O0, ASAN+UBSAN'
OPTIM_FLAGS_O0_sanitized := -O0 -fsanitize=address -fsanitize=undefined
OPTIM_NAME_O0 := '-O0            '
OPTIM_FLAGS_O0 := -O0
OPTIM_NAME_Omax := '-O3            '
OPTIM_FLAGS_Omax := -O3
$(foreach x,$(OPTIMIZE),$(if $(OPTIM_FLAGS_$x),,$(error Unknown optimization level: $x)))

# Compilers to test. If not specified, we find all versions of GCC and Clang in PATH.
# Note that we only use version-suffixed compilers. The unsufixed compilers should be linked to one of those anyway?
COMPILER := $(shell bash -c 'compgen -c g++-; compgen -c clang++-; compgen -c cl; compgen -c clang-cl' | grep -Po '^((clan)?g\+\+(-[0-9]+)?|cl|clang-cl)(?=.exe)?$$' | sort -hr -t- -k2 | uniq)
$(if $(COMPILER),,$(error Unable to detect compilers, set `COMPILER=??` to a space-separated compiler list))

# C++ standards to test. Override this with a subset of standards if you want to.
# Only MSVC supports `latest`.
STANDARD := latest 20 17 14

# C++ standard libraries to test.
STDLIB := libstdc++ libc++ msvc

# Per-compiler flag customization. A list of `compiler=flag`. All matching flags for the current compiler are applied.
CXXFLAGS_PER_COMPILER :=

# Important compiler flags.
CXXFLAGS_DEFAULT := -Iinclude -g -pedantic-errors -Wall -Wextra -Wdeprecated -Wextra-semi -ftemplate-backtrace-limit=0
CXXFLAGS_DEFAULT_MSVC := -Iinclude -EHsc
# Less important compiler flags.
CXXFLAGS :=


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
else ifneq ($(and $(filter g++%,$(COMPILER)),$(filter-out libstdc++,$(STDLIB))),)
	@true # GCC only supports libstdc++.
else ifneq ($(and $(filter clang++%,$(COMPILER)),$(filter msvc,$(STDLIB))),)
	@true # We don't use `clang` with MSVC's standard library. Instead we use `clang-cl`.
else ifneq ($(and $(filter %cl,$(COMPILER)),$(filter-out msvc,$(STDLIB))),)
	@true # MSVC only supports its own standard library.
else ifeq ($(if $(filter g++% clang++%,$(COMPILER)),$(shell $(if $(filter g++%,$(COMPILER)),$(COMPILER) -v --help,$(COMPILER) -std=c++0 -xc++ /dev/null) 2>&1 | grep 'c++$(STANDARD)'),x),)
	@true # Unsupported standard version for this compiler.
else ifneq ($(and $(filter clang++%,$(COMPILER)),$(filter libc++,$(STDLIB)),$(if $(wildcard /usr/lib/llvm-$(shell $(COMPILER) --version | grep -Po '(?<=version )[0-9]+')/include/c++),,x)),)
	@true # Using Clang with libc++, but libc++ is not installed.
else
	@+printf "%-11s C++%-7s %-10s %-14s...  " $(COMPILER) $(STANDARD) $(STDLIB) $(OPTIMIZE)
	@$(strip \
		$(COMPILER) $(SRC) \
		$(CXXFLAGS) \
		$(CXXFLAGS_DEFAULT$(if $(filter %cl,$(COMPILER)),_MSVC)) \
		$(filter-out $(if $(filter %cl,$(COMPILER)),-fsanitize=undefined -O0),$(OPTIM_FLAGS_$(OPTIMIZE))) \
		-std$(if $(filter %cl,$(COMPILER)),:,=)c++$(STANDARD) \
		$(if $(filter clang++%,$(COMPILER)),-stdlib=$(STDLIB)) \
		$(patsubst $(COMPILER)=%,%,$(filter $(COMPILER)=%,$(CXXFLAGS_PER_COMPILER))) \
		$(if $(filter %cl,$(COMPILER)),-link -out:,-o)tests \
		&& ./tests \
	)
endif

.PHONY: commands
commands:
	$(eval override cxx := $(lastword $(filter clang++%,$(COMPILER))))
	$(if $(cxx),,$(error Unable to guess the compiler))
	$(eval override std := $(firstword $(STANDARD)))
	$(if $(std),,$(error Unable to guess the C++ standard version))
	$(file >compile_commands.json,[{"directory":"$(CURDIR)", "file":"$(abspath $(SRC))", "command":"$(cxx) $(SRC) $(CXXFLAGS) $(CXXFLAGS_DEFAULT) -std=c++$(std)"}])
	@true
