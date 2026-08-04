/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file toolchain.h
 * @brief Compiler-specific helpers used by the public Hubble headers.
 **/

#ifndef INCLUDE_HUBBLE_TOOLCHAIN_H
#define INCLUDE_HUBBLE_TOOLCHAIN_H

/**
 * @def HUBBLE_EXPERIMENTAL
 * @brief Marks a public API as experimental.
 *
 * An experimental API is functional but its signature, semantics or very
 * existence may change or be removed in any release. It is not covered by the
 * SDK's API stability guarantees. Where the toolchain supports it, calling one
 * emits a compiler warning naming the function.
 *
 * Applied to the declaration in the header, never to the definition:
 *
 * @code
 * HUBBLE_EXPERIMENTAL
 * int hubble_some_api(void);
 * @endcode
 *
 * Define @c HUBBLE_SILENCE_EXPERIMENTAL_WARNINGS before including any Hubble
 * header to acknowledge the risk and silence the diagnostic globally. With GCC
 * a single call site can be silenced instead:
 *
 * @code
 * #pragma GCC diagnostic push
 * #pragma GCC diagnostic ignored "-Wattribute-warning"
 * ret = hubble_some_api();
 * #pragma GCC diagnostic pop
 * @endcode
 */
#define _HUBBLE_EXPERIMENTAL_MSG                                               \
	"this Hubble API is experimental: it may change or be "                \
	"removed in any release. Define HUBBLE_SILENCE_EXPERIMENTAL_WARNINGS " \
	"to acknowledge and silence this warning."

#if defined(HUBBLE_SILENCE_EXPERIMENTAL_WARNINGS) || defined(__DOXYGEN__)
#define HUBBLE_EXPERIMENTAL
#elif defined(__has_attribute)
#if defined(__clang__) && __has_attribute(diagnose_if)
#define HUBBLE_EXPERIMENTAL                                                    \
	__attribute__((diagnose_if(1, _HUBBLE_EXPERIMENTAL_MSG, "warning")))
#elif __has_attribute(warning)
#define HUBBLE_EXPERIMENTAL __attribute__((warning(_HUBBLE_EXPERIMENTAL_MSG)))
#else
#define HUBBLE_EXPERIMENTAL
#endif
#else
#define HUBBLE_EXPERIMENTAL
#endif

#endif /* INCLUDE_HUBBLE_TOOLCHAIN_H */
