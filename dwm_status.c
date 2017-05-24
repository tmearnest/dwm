#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <X11/Xlib.h>

char *program;

#ifdef CUDA
#include <nvml.h>

#define NVML_EXEC(x) do { \
    nvmlReturn_t status = (x);  \
    if (NVML_SUCCESS != status) { \
        fprintf(stderr, "%s: NVML call failed: %s\n", program, nvmlErrorString(status)); \
        nvmlShutdown(); \
        exit(-1); \
    } \
} while(0) 
#endif

int
get_mem(void)
{
  unsigned long memAvail=0, memTotal=0;
  char buf[256];
  
  FILE *fp = fopen("/proc/meminfo", "r");

  while (fgets(buf, sizeof(buf), fp)) {
      if (sscanf(buf, "MemTotal: %lu kB\n", &memTotal) == 1)
          continue;
      if (sscanf(buf, "MemAvailable: %lu kB\n", &memAvail) == 1)
          break;
  }
  fclose(fp);

  return (memTotal-memAvail) >> 20;
}

void
cputicks(unsigned long* idleTicks, unsigned long* totalTicks)
{
  FILE *fp = fopen("/proc/stat", "r");
  unsigned long user, nice, sys, idle, iowait, irq, softirq, steal, guest, guest_nice;
  fscanf(fp, "cpu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", &user, &nice, &sys, 
          &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice);
  fclose(fp);

  *totalTicks = user + nice + sys + idle + iowait + irq + softirq + steal + guest + guest_nice;
  *idleTicks = idle;
}


int
get_cpu(void)
{
  static int initialTicks = 0;
  static unsigned long idle0 = 0;
  static unsigned long total0 = 0;
  static int nth = 0;
  unsigned long idle, total;

  if (!initialTicks) {
      nth = sysconf(_SC_NPROCESSORS_CONF);
      cputicks(&idle0, &total0);
      sleep(1);
      initialTicks = 1;
  }

  cputicks(&idle, &total);

  double dtot = 1.0*(total-total0);
  double didl = 1.0*(idle-idle0);
  double cpu = (dtot>0) ? (1.0 - 1.0*didl/dtot) : 0.0;
  idle0 = idle;
  total0 = total;
  return nth*cpu;
}


void
status(Display *dpy, int device_count)
{
    unsigned int i;

    int cur = 0;
    char str[64];
    str[0] = 0;

#define statusf(format, ...) cur += snprintf(str+cur, sizeof(str)-cur-1, format, __VA_ARGS__)

    int mem = get_mem();
    if (mem > 32)
        statusf("m%d ", (int)get_mem());

    int cpu = get_cpu();
    if (cpu > 0)
        statusf("c%d ", cpu);

#ifdef CUDA
    nvmlReturn_t result;
    nvmlDevice_t device;
    for (int i=0; i < device_count; i++) {
        nvmlUtilization_t util;
        NVML_EXEC(nvmlDeviceGetHandleByIndex(i, &device));
        NVML_EXEC(nvmlDeviceGetUtilizationRates (device, &util));
        if (util.gpu > 0) 
            statusf("g%dc%dm%d ", i, util.gpu, util.memory);
    }
#endif

    char buf[256];
    time_t t;
    struct tm *info;
    time(&t);
    info = localtime(&t);
    strftime(buf, sizeof(buf), "%a %b %_2d %H:%M", info);
    statusf("%s", buf);
    XStoreName(dpy, DefaultRootWindow(dpy), str);
    XSync(dpy, False);

#undef statusf
}

int
main(int argc, char *argv[]) {
    program = argv[0];
    int device_count = 0;

    if (fork() == 0) {
        Display *dpy = XOpenDisplay(NULL);
        if (!dpy) {
            fprintf(stderr, "%s: Failed to open display\n", program);
            exit(-1);
        }

#ifdef CUDA
        NVML_EXEC(nvmlInit());
        NVML_EXEC(nvmlDeviceGetCount(&device_count));
#endif

        for (;;) {
            status(dpy, device_count);
            sleep(1);
        }
    }
}
