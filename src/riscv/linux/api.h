#pragma once

#include <cpuinfo.h>
#include <cpuinfo/common.h>

/**
 * Definition of a RISC-V Linux processor. It is composed of the base processor
 * definition in "include/cpuinfo.h" and flags specific to RISC-V Linux
 * implementations.
 */
struct cpuinfo_riscv_linux_processor {
	/* Public ABI cpuinfo structures. */
	struct cpuinfo_processor processor;

	/**
	 * Linux-specific flags for the logical processor:
	 * - Bit field that can be masked with CPUINFO_LINUX_FLAG_*.
	 */
	uint32_t flags;
};

/**
 * Reads AT_HWCAP from `getauxval` and populates the cpuinfo_riscv_isa
 * structure.
 *
 * @param[isa] - Reference to cpuinfo_riscv_isa structure to populate.
 */
CPUINFO_INTERNAL void cpuinfo_riscv_linux_decode_isa_from_hwcap(
	struct cpuinfo_riscv_isa isa[restrict static 1]);
