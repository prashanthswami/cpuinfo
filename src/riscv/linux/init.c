#include <string.h>

#include <cpuinfo/internal-api.h>
#include <cpuinfo/log.h>
#include <linux/api.h>
#include <riscv/linux/api.h>

/* ISA structure to hold supported extensions. */
struct cpuinfo_riscv_isa cpuinfo_isa;

/* Helper function to bitmask flags and ensure operator precedence. */
static inline bool bitmask_all(uint32_t flags, uint32_t mask) {
  return (flags & mask) == mask;
}

/**
 * Parses the core cpus list for each processor. This function is called once
 * per-processor, with the IDs of all other processors in the core list.
 *
 * The 'processor_[start|count]' are populated in the processor's 'core'
 * attribute, with 'start' being the smallest ID in the core list.
 *
 * The 'core_leader_id' of each processor is set to the smallest ID in it's
 * cluster CPU list.
 *
 * Precondition: The element in the 'processors' list must be initialized with
 * their 'core_leader_id' to their index in the list.
 * E.g. processors[0].core_leader_id = 0.
 */
static bool core_cpus_parser(uint32_t processor,
			     uint32_t core_cpus_start,
			     uint32_t core_cpus_end,
			     struct cpuinfo_riscv_linux_processor* processors) {
	uint32_t processor_start = UINT32_MAX;
	uint32_t processor_count = 0;

	/* If the processor already has a leader, use it. */
	if (bitmask_all(processors[processor].flags, CPUINFO_LINUX_FLAG_CORE_CLUSTER)) {
		processor_start = processors[processor].core_leader_id;
	}

	for (size_t core_cpu = core_cpus_start; core_cpu < core_cpus_end; core_cpu++) {
		if (!bitmask_all(processors[core_cpu].flags, CPUINFO_LINUX_FLAG_VALID)) {
			continue;
		}
		/**
		 * The first valid processor observed is the smallest ID in the
		 * list that attaches to this core.
		 */
		if (processor_start == UINT32_MAX) {
			processor_start = core_cpu;
		}
		processors[core_cpu].core_leader_id = processor_start;
		processor_count++;
	}
	/**
	 * If the cluster flag has not been set, assign the processor start. If
	 * it has been set, only apply the processor start if it's less than the
	 * held value. This can happen if the callback is invoked twice:
	 *
	 * e.g. core_cpu_list=1,10-12
	 */
	if (!bitmask_all(processors[processor].flags, CPUINFO_LINUX_FLAG_CORE_CLUSTER)
	    || processors[processor].core.processor_start > processor_start) {
		processors[processor].core.processor_start = processor_start;
		processors[processor].core_leader_id = processor_start;
	}
	processors[processor].core.processor_count += processor_count;
	processors[processor].flags |= CPUINFO_LINUX_FLAG_CORE_CLUSTER;
	/* The parser has failed only if no processors were found. */
	return processor_count != 0;
}

/**
 * Parses the cluster cpu list for each processor. This function is called once
 * per-processor, with the IDs of all other processors in the cluster.
 *
 * The 'cluster_leader_id' of each processor is set to the smallest ID in it's
 * cluster CPU list.
 *
 * Precondition: The element in the 'processors' list must be initialized with
 * their 'cluster_leader_id' to their index in the list.
 * E.g. processors[0].cluster_leader_id = 0.
 */
static bool cluster_cpus_parser(uint32_t processor,
				uint32_t cluster_cpus_start,
				uint32_t cluster_cpus_end,
				struct cpuinfo_riscv_linux_processor* processors) {
	uint32_t processor_start = UINT32_MAX;
	uint32_t processor_count = 0;
	uint32_t core_count = 0;

	/* If the processor already has a leader, use it. */
	if (bitmask_all(processors[processor].flags, CPUINFO_LINUX_FLAG_CLUSTER_CLUSTER)) {
		processor_start = processors[processor].cluster_leader_id;
	}

	for (size_t cluster_cpu = cluster_cpus_start; cluster_cpu < cluster_cpus_end; cluster_cpu++) {
		if (!bitmask_all(processors[cluster_cpu].flags, CPUINFO_LINUX_FLAG_VALID)) {
			continue;
		}
		/**
		 * The first valid processor observed is the smallest ID in the
		 * list that attaches to this core.
		 */
		if (processor_start == UINT32_MAX) {
			processor_start = cluster_cpu;
		}
		processors[cluster_cpu].cluster_leader_id = processor_start;
		processor_count++;
		/**
		 * A processor should only represent it's core if it is the
		 * assigned leader of that core.
		 */
		if (processors[cluster_cpu].core_leader_id == cluster_cpu) {
			core_count++;
		}
	}
	/**
	 * If the cluster flag has not been set, assign the processor start. If
	 * it has been set, only apply the processor start if it's less than the
	 * held value. This can happen if the callback is invoked twice:
	 *
	 * e.g. cluster_cpus_list=1,10-12
	 */
	if (!bitmask_all(processors[processor].flags, CPUINFO_LINUX_FLAG_CLUSTER_CLUSTER)
	    || processors[processor].cluster.processor_start > processor_start) {
		processors[processor].cluster.processor_start = processor_start;
		processors[processor].cluster.core_start = processor_start;
		processors[processor].cluster.cluster_id = processor_start;
		processors[processor].cluster_leader_id = processor_start;
	}
	processors[processor].cluster.processor_count += processor_count;
	processors[processor].cluster.core_count += core_count;
	processors[processor].flags |= CPUINFO_LINUX_FLAG_CLUSTER_CLUSTER;
	return true;
}

/**
 * Parses the package cpus list for each processor. This function is called once
 * per-processor, with the IDs of all other processors in the package list.
 *
 * The 'processor_[start|count]' are populated in the processor's 'package'
 * attribute, with 'start' being the smallest ID in the package list.
 *
 * The 'package_leader_id' of each processor is set to the smallest ID in it's
 * cluster CPU list.
 *
 * Precondition: The element in the 'processors' list must be initialized with
 * their 'package_leader_id' to their index in the list.
 * E.g. processors[0].package_leader_id = 0.
 */
static bool package_cpus_parser(uint32_t processor,
			     uint32_t package_cpus_start,
			     uint32_t package_cpus_end,
			     struct cpuinfo_riscv_linux_processor* processors) {
	uint32_t processor_start = UINT32_MAX;
	uint32_t processor_count = 0;
	uint32_t cluster_count = 0;
	uint32_t core_count = 0;

	/* If the processor already has a leader, use it. */
	if (bitmask_all(processors[processor].flags, CPUINFO_LINUX_FLAG_PACKAGE_CLUSTER)) {
		processor_start = processors[processor].package_leader_id;
	}

	for (size_t package_cpu = package_cpus_start; package_cpu < package_cpus_end; package_cpu++) {
		if (!bitmask_all(processors[package_cpu].flags, CPUINFO_LINUX_FLAG_VALID)) {
			continue;
		}
		/**
		 * The first valid processor observed is the smallest ID in the
		 * list that attaches to this package.
		 */
		if (processor_start == UINT32_MAX) {
			processor_start = package_cpu;
		}
		processors[package_cpu].package_leader_id = processor_start;
		processor_count++;
		/**
		 * A processor should only represent it's core if it is the
		 * assigned leader of that core, and similarly for it's cluster.
		 */
		if (processors[package_cpu].cluster_leader_id == package_cpu) {
			cluster_count++;
		}
		if (processors[package_cpu].core_leader_id == package_cpu) {
			core_count++;
		}
	}
	/**
	 * If the cluster flag has not been set, assign the processor start. If
	 * it has been set, only apply the processor start if it's less than the
	 * held value. This can happen if the callback is invoked twice:
	 *
	 * e.g. package_cpus_list=1,10-12
	 */
	if (!bitmask_all(processors[processor].flags, CPUINFO_LINUX_FLAG_PACKAGE_CLUSTER)
	    || processors[processor].package.processor_start > processor_start) {
		processors[processor].package.processor_start = processor_start;
		processors[processor].package.cluster_start = processor_start;
		processors[processor].package.core_start = processor_start;
		processors[processor].package_leader_id = processor_start;
	}
	processors[processor].package.processor_count += processor_count;
	processors[processor].package.cluster_count += cluster_count;
	processors[processor].package.core_count += core_count;
	processors[processor].flags |= CPUINFO_LINUX_FLAG_PACKAGE_CLUSTER;
	return true;
}

/* Initialization for the RISC-V Linux system. */
void cpuinfo_riscv_linux_init(void) {
	struct cpuinfo_riscv_linux_processor* riscv_linux_processors = NULL;
	struct cpuinfo_processor* processors = NULL;
	struct cpuinfo_package* packages = NULL;
	struct cpuinfo_cluster* clusters = NULL;
	struct cpuinfo_core* cores = NULL;
	struct cpuinfo_uarch_info* uarchs = NULL;
	const struct cpuinfo_processor** linux_cpu_to_processor_map = NULL;
	const struct cpuinfo_core** linux_cpu_to_core_map = NULL;
	uint32_t* linux_cpu_to_uarch_index_map = NULL;

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
						     CPUINFO_LINUX_FLAG_PRESENT | CPUINFO_LINUX_FLAG_VALID)) {
		cpuinfo_log_error("failed to detect present processors");
		goto cleanup;
	}

        /* Populate processor information. */
        for (size_t processor = 0; processor < max_processor_id; processor++) {
		if (!bitmask_all(riscv_linux_processors[processor].flags, CPUINFO_LINUX_FLAG_VALID)) {
			continue;
                }
                /* TODO: Determine if an 'smt_id' is available. */
                riscv_linux_processors[processor].processor.linux_id = processor;
        }

	/* Populate core information. */
	for (size_t processor = 0; processor < max_processor_id; processor++) {
		if (!bitmask_all(riscv_linux_processors[processor].flags, CPUINFO_LINUX_FLAG_VALID)) {
			continue;
		}

		/* Populate processor start and count information. */
		if (!cpuinfo_linux_detect_core_cpus(
				max_processor_id,
				processor,
				(cpuinfo_siblings_callback) core_cpus_parser,
				riscv_linux_processors)) {
			cpuinfo_log_error("failed to detect core cpus for processor %zu.", processor);
			goto cleanup;
		}

		/* Populate core ID information. */
		if (cpuinfo_linux_get_processor_core_id(
				processor,
				&riscv_linux_processors[processor].core.core_id)) {
			riscv_linux_processors[processor].flags |= CPUINFO_LINUX_FLAG_CORE_ID;
		}

		/* TODO: Determine the vendor and uarch of this core. */
		/* TODO: Determine the frequency of this core. */
	}

	/* Populate cluster information. */
	for (size_t processor = 0; processor < max_processor_id; processor++) {
		if (!bitmask_all(riscv_linux_processors[processor].flags, CPUINFO_LINUX_FLAG_VALID)) {
			continue;
		}
		if (!cpuinfo_linux_detect_cluster_cpus(
				max_processor_id,
				processor,
				(cpuinfo_siblings_callback) cluster_cpus_parser,
				riscv_linux_processors)) {
			cpuinfo_log_warning("failed to detect cluster cpus for processor %zu.", processor);
			goto cleanup;
		}
	}

	/* Populate package information. */
	for (size_t processor = 0; processor < max_processor_id; processor++) {
		if (!bitmask_all(riscv_linux_processors[processor].flags, CPUINFO_LINUX_FLAG_VALID)) {
			continue;
		}
		if (!cpuinfo_linux_detect_package_cpus(
				max_processor_id,
				processor,
				(cpuinfo_siblings_callback) package_cpus_parser,
				riscv_linux_processors)) {
			cpuinfo_log_warning("failed to detect package cpus for processor %zu.", processor);
			goto cleanup;
		}
	}

	/* Determine the micro-architecture of each processor. */
	for (size_t processor = 0; processor < max_processor_id; processor++) {
		if (!bitmask_all(riscv_linux_processors[processor].flags, CPUINFO_LINUX_FLAG_VALID)) {
			continue;
		}
		/* TODO: Determine the uarch based on discovered parameters. */
		riscv_linux_processors[processor].uarch_info.uarch = cpuinfo_uarch_unknown;
	}

	/* Populate ISA structure with hwcap information. */
	cpuinfo_riscv_linux_decode_isa_from_hwcap(&cpuinfo_isa);

	/**
	 * Determine the number of *valid* detected processors, cores, clusters
	 * and packages in the list.
	 */
	size_t valid_processors_count = 0;
	size_t valid_cores_count = 0;
	size_t valid_clusters_count = 0;
	size_t valid_packages_count = 0;
	for (size_t processor = 0; processor < max_processor_id; processor++) {
		if (!bitmask_all(riscv_linux_processors[processor].flags, CPUINFO_LINUX_FLAG_VALID)) {
			continue;
		}
		valid_processors_count++;
		if (riscv_linux_processors[processor].core_leader_id == processor) {
			valid_cores_count++;
		}
		if (riscv_linux_processors[processor].cluster_leader_id == processor) {
			valid_clusters_count++;
		}
		if (riscv_linux_processors[processor].package_leader_id == processor) {
			valid_packages_count++;
		}
	}

	/* Allocate and populate final public ABI structures. */
	processors = calloc(valid_processors_count,
			    sizeof(struct cpuinfo_processor));
	if (processors == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for %zu processors.",
			valid_processors_count * sizeof(struct cpuinfo_processor),
			valid_processors_count);
		goto cleanup;
	}

	cores = calloc(valid_cores_count,
			    sizeof(struct cpuinfo_core));
	if (cores == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for %zu cores.",
			valid_cores_count * sizeof(struct cpuinfo_core),
			valid_cores_count);
		goto cleanup;
	}

	clusters = calloc(valid_clusters_count,
			    sizeof(struct cpuinfo_cluster));
	if (clusters == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for %zu clusters.",
			valid_clusters_count * sizeof(struct cpuinfo_cluster),
			valid_clusters_count);
		goto cleanup;
	}

	packages = calloc(valid_packages_count,
			    sizeof(struct cpuinfo_package));
	if (packages == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for %zu packages.",
			valid_packages_count * sizeof(struct cpuinfo_package),
			valid_packages_count);
		goto cleanup;
	}

	/* TODO: When micro-architectures are learned, use actual count. */
	const size_t valid_uarchs_count = 1;
	uarchs = calloc(valid_uarchs_count, sizeof(struct cpuinfo_uarch_info));
	if (uarchs == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for %zu packages.",
			valid_uarchs_count * sizeof(struct cpuinfo_uarch_info),
			valid_uarchs_count);
		goto cleanup;
	}
	uarchs[0] = (struct cpuinfo_uarch_info) {
		.uarch = cpuinfo_uarch_unknown,
		.processor_count = valid_processors_count,
		.core_count = valid_cores_count,
	};

	linux_cpu_to_processor_map = calloc(max_processor_id,
					    sizeof(struct cpuinfo_processor*));
	if (linux_cpu_to_processor_map == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for %zu processor map.",
				  max_processor_id * sizeof(struct cpuinfo_processor*),
				  max_processor_id);
		goto cleanup;
	}

	linux_cpu_to_core_map = calloc(max_processor_id,
				       sizeof(struct cpuinfo_core*));
	if (linux_cpu_to_core_map == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for %zu core map.",
				  max_processor_id * sizeof(struct cpuinfo_core*),
				  max_processor_id);
		goto cleanup;
	}

	linux_cpu_to_uarch_index_map = calloc(max_processor_id,
					      sizeof(struct cpuinfo_uarch_info*));
	if (linux_cpu_to_uarch_index_map == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for %zu uarch map.",
				  max_processor_id * sizeof(struct cpuinfo_uarch_info*),
				  max_processor_id);
		goto cleanup;
	}

	/* Transfer contents of processor list to ABI structures. */
	size_t valid_processors_index = 0;
	size_t valid_cores_index = 0;
	size_t valid_clusters_index = 0;
	size_t valid_packages_index = 0;
	for (size_t processor = 0; processor < max_processor_id; processor++) {
		if (!bitmask_all(riscv_linux_processors[processor].flags, CPUINFO_LINUX_FLAG_VALID)) {
			continue;
		}

		/* Copy cpuinfo_processor information. */
		memcpy(&processors[valid_processors_index++],
		       &riscv_linux_processors[processor].processor,
			sizeof(struct cpuinfo_processor));

		/* Copy cpuinfo_core information, if this is the leader. */
		if (riscv_linux_processors[processor].core_leader_id == processor) {
			memcpy(&cores[valid_cores_index++],
			       &riscv_linux_processors[processor].core,
			       sizeof(struct cpuinfo_core));
		}

		/* Copy cpuinfo_cluster information, if this is the leader. */
		if (riscv_linux_processors[processor].cluster_leader_id == processor) {
			memcpy(&clusters[valid_clusters_index++],
			       &riscv_linux_processors[processor].cluster,
			       sizeof(struct cpuinfo_cluster));
		}

		/* Copy cpuinfo_package information, if this is the leader. */
		if (riscv_linux_processors[processor].package_leader_id == processor) {
			memcpy(&packages[valid_packages_index++],
			       &riscv_linux_processors[processor].package,
			       sizeof(struct cpuinfo_package));
		}

		/* Commit pointers on the final structures. */
		processors[valid_processors_index - 1].core = &cores[valid_cores_index - 1];
		processors[valid_processors_index - 1].cluster = &clusters[valid_clusters_index - 1];
		processors[valid_processors_index - 1].package = &packages[valid_packages_index - 1];

		cores[valid_cores_index - 1].cluster = &clusters[valid_clusters_index - 1];
		cores[valid_cores_index - 1].package = &packages[valid_packages_index - 1];

		clusters[valid_clusters_index - 1].package = &packages[valid_packages_index - 1];

		uint32_t linux_id = riscv_linux_processors[processor].processor.linux_id;
		linux_cpu_to_processor_map[linux_id] = &processors[valid_processors_index - 1];
		linux_cpu_to_core_map[linux_id] = &cores[valid_cores_index - 1];

		/* TODO: Defaults to unknown for now, populate. */
		linux_cpu_to_uarch_index_map[linux_id] = 0;
	}

	/* Commit */
	cpuinfo_processors = processors;
	cpuinfo_processors_count = valid_processors_count;
	cpuinfo_cores = cores;
	cpuinfo_cores_count = valid_cores_count;
	cpuinfo_clusters = clusters;
	cpuinfo_clusters_count = valid_clusters_count;
	cpuinfo_packages = packages;
	cpuinfo_packages_count = valid_packages_count;
	cpuinfo_uarchs = uarchs;
	cpuinfo_uarchs_count = valid_uarchs_count;

	cpuinfo_linux_cpu_max = max_processor_id;
	cpuinfo_linux_cpu_to_processor_map = linux_cpu_to_processor_map;
	cpuinfo_linux_cpu_to_core_map = linux_cpu_to_core_map;
	cpuinfo_linux_cpu_to_uarch_index_map = linux_cpu_to_uarch_index_map;

	__sync_synchronize();

	cpuinfo_is_initialized = true;

	/* Mark all public structures NULL to prevent cleanup from erasing them. */
	processors = NULL;
	cores = NULL;
	clusters = NULL;
	packages = NULL;
	uarchs = NULL;
	linux_cpu_to_processor_map = NULL;
	linux_cpu_to_core_map = NULL;
	linux_cpu_to_uarch_index_map = NULL;
cleanup:
	free(riscv_linux_processors);
	free(processors);
	free(cores);
	free(clusters);
	free(packages);
	free(uarchs);
	free(linux_cpu_to_processor_map);
	free(linux_cpu_to_core_map);
	free(linux_cpu_to_uarch_index_map);
}
