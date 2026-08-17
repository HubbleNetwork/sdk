/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONFIG_MPSL_FEM_API_AVAILABLE

void hubble_board_fem_setup(void);

void hubble_board_fem_enable(void);

void hubble_board_fem_bypass(void);

void hubble_board_fem_sleep(void);

#else

static inline void hubble_board_fem_setup(void)
{
}

static inline void hubble_board_fem_enable(void)
{
}

static inline void hubble_board_fem_bypass(void)
{
}

static inline void hubble_board_fem_sleep(void)
{
}

#endif /* !CONFIG_MPSL_FEM_API_AVAILABLE */
