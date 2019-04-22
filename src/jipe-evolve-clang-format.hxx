#if defined(_MSC_VER)

#define _CRT_SECURE_NO_WARNINGS 1

// pre-C11 functions may be unsafe
#pragma warning(disable : 4996)

// nonstandard extension used: bit field types other than int
#pragma warning(disable : 4214)

// nonstandard extension used: initialisation w/automatic variable
#pragma warning(disable : 4221)

// nonstandard extension used: non-constant aggregate initializer
#pragma warning(disable : 4204)

// unreferenced formal parameter
#pragma warning(disable : 4100)

// unreferenced local variabkle
#pragma warning(disable : 4101)

// function selected for automatic inline expansion
#pragma warning(disable : 4711)

// function not inlined
#pragma warning(disable : 4710)

// 'x' is not defined as a preprocessor macro, replacing ...
#pragma warning(disable : 4668)

// 'x' bytes padding added after data member 'y'
#pragma warning(disable : 4820)

// structure was padded due to alignment specifier
#pragma warning(disable : 4324)

// has C-linkage specified, but returns UDT
#pragma warning(disable : 4190)

#elif defined(__GNUC__)

#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Woverlength-strings"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#if !defined(__clang__)
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wc++17-extensions"
#pragma GCC diagnostic ignored "-Wgnu-empty-initializer"
#pragma GCC diagnostic ignored                                       \
  "-Wtautological-constant-out-of-range-compare"
#endif

#endif
