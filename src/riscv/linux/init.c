#include <string.h>

#include <cpuinfo/internal-api.h>
#include <cpuinfo/log.h>
#include <linux/api.h>
#include <riscv/linux/api.h>

/* ISA structure to hold supported extensions. */
struct cpuinfo_riscv_isa cpuinfo_isa;

void cpuinfo_riscv_linux_init(void) {
	struct cpuinfo_riscv_linux_processor* riscv_linux_processors = NULL;
	struct cpuinfo_processor* processors = NULL;

	/**
	 * The interesting set of processors are the number of 'present'
	 * processors on the system. There may be more 'possible' processors, but
	 * processor information cannot be gathered on non-present processors.
	 *
	 * Note: For SoCs, it is largely the case that all processors are known
	 * at boot and no processors are hotplugged at runtime, so the
	 * 'present' and 'possible' list is often the same.
	 *
	 * Note: This computes the maximum processor ID of the 'present'
	 * processors. It is not a count of the number of processors on the
	 * system.
	 */
	const size_t max_processor_id = 1 +
		cpuinfo_linux_get_max_present_processor(
				cpuinfo_linux_get_max_processors_count());
	if (max_processor_id == 0) {
		cpuinfo_log_error("failed to discover any processors");
		return;
	}

	/**
	 * Allocate space to store all processor information. This array is
	 * sized to the max processor ID as opposed to the number of 'present'
	 * processors, to leverage pointer math in the common utility functions.
	 */
	riscv_linux_processors = calloc(max_processor_id,
					sizeof(struct cpuinfo_riscv_linux_processor));
	if (riscv_linux_processors == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for %zu processors.",
			max_processor_id * sizeof(struct cpuinfo_riscv_linux_processor),
			max_processor_id);
		goto cleanup;
        }

	/**
	 * Attempt to detect all processors and apply the corresponding flag to
	 * each processor struct that we find.
	 */
	if (!cpuinfo_linux_detect_present_processors(max_processor_id,
						     &riscv_linux_processors->flags,
						     sizeof(struct cpuinfo_riscv_linux_processor),
						     CPUINFO_LINUX_FLAG_PRESENT)) {
		cpuinfo_log_error("failed to detect present processors");
		goto cleanup;
	}

	/* Determine the number of detected processors in the list. */
	size_t processors_count = 0;
	for (size_t processor = 0; processor < max_processor_id; processor++) {
		if ((riscv_linux_processors[processor].flags & CPUINFO_LINUX_FLAG_PRESENT) != 0) {
			processors_count++;
		}
	}

	/* Populate ISA structure with hwcap information. */
	cpuinfo_riscv_linux_decode_isa_from_hwcap(&cpuinfo_isa);

	/* Allocate and populate final public ABI structures. */
	processors = calloc(max_processor_id,
			    sizeof(struct cpuinfo_riscv_linux_processor));
	if (processors == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for %zu processors.",
			processors_count * sizeof(struct cpuinfo_processor),
			processors_count);
		goto cleanup;
	}

	size_t processors_index = 0;
	for (size_t processor = 0; processor < max_processor_id; processor++) {
		/**
		 * If this element of the riscv_linux_processors list isn't
		 * marked with the appropriate flag, it was not marked during
		 * detection. Skip it.
		 */
		if ((riscv_linux_processors[processor].flags & CPUINFO_LINUX_FLAG_PRESENT) == 0) {
			continue;
		}

		/* Copy the cpuinfo_processor information. */
		memcpy(&processors[processors_index++],
			&riscv_linux_processors[processor].processor,
			sizeof(struct cpuinfo_processor));
	}

	/* Commit */
	cpuinfo_processors = processors;
	cpuinfo_processors_count = processors_count;

	__sync_synchronize();

	// The cpuinfo is intentionally left uninitialized for now, to prevent
	// users of this library relying on RISC-V support before it is ready.
	cpuinfo_is_initialized = false;

	/* Mark all public structures NULL to prevent cleanup from erasing them. */
	processors = NULL;
cleanup:
	free(riscv_linux_processors);
	free(processors);
}
