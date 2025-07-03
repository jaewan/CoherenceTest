#include "utils.h"

void pin_thread_to_cpu(int cpu_core) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_core, &cpuset);
    pthread_t current_thread = pthread_self();
    if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("pthread_setaffinity_np");
        exit(EXIT_FAILURE);
    }
}

int get_cpu_node(int cpu_core) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", cpu_core);
    FILE* fp = fopen(path, "r");
    if (!fp) {
        // Fallback for systems without this topology file
        return 0; 
    }
    int node = -1;
    if (fscanf(fp, "%d", &node) != 1) {
        perror("fscanf");
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return node;
}

long long get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000L + ts.tv_nsec;
}
