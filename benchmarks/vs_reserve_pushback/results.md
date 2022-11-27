Comparing `= init{...}` vs `.reserve()`+`.push_back()` on a vector of strings.

Tested on a [local instance of quickbench](https://github.com/FredTingaud/bench-runner).

Both compilers were used in C++20 mode, with `-O3`, with libstdc++:

* Clang 15 - 75949 vs 76781 - better_braces is ~1.08% faster.
* GCC 12.2 - 72793 vs 77015 - better_braces is ~5.48% faster.
