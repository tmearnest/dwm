#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <X11/Xlib.h>

#define N_CPU_DATA 45

double cpu_data[N_CPU_DATA];

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
cpuGraph(char *str, int n_str, char *bg, char *fg, int y0, int y1, int margin) {
    int i;
    static int initialTicks = 0;
    static unsigned long idle0 = 0;
    static unsigned long total0 = 0;
    unsigned long idle, total;

    if (!initialTicks) {
        for (i=0; i < N_CPU_DATA; i++)
            cpu_data[i]  = 0;
        cputicks(&idle0, &total0);
        sleep(1);
        initialTicks = 1;
    }

    cputicks(&idle, &total);

    memmove(cpu_data+1, cpu_data, (N_CPU_DATA-1)*sizeof(double));
    double dtot = 1.0*(total-total0);
    double didl = 1.0*(idle-idle0);
    cpu_data[0] = (dtot>0) ? (1.0 - 1.0*didl/dtot) : 0.0;

    idle0 = idle;
    total0 = total;

    int cur = 0;
    int x0 = margin;
    int h = y1-y0;
    int w = 2*margin+N_CPU_DATA;

    cur += snprintf(str+cur, n_str-cur-1, "^c%s^^r%d,%d,%d,%d^^c%s^", bg, x0, y0, N_CPU_DATA, h, fg);
    for (i=0; i < N_CPU_DATA; i++) {
        int y =  h*cpu_data[i];
        int y0_ = y0 + h-y;
        cur += snprintf(str+cur, n_str-cur-1, "^r%d,%d,%d,%d^", x0+(N_CPU_DATA-i-1), y0_, 1, y);
    }
    cur += snprintf(str+cur, n_str-cur-1, "^f%d^^d^", w);

    return cur;
}


void
status(Display *dpy, int device_count)
{
    unsigned int i;

    int cur = 0;
    char str[1024];

    cur += cpuGraph(str, sizeof(str)-cur-1, "#202020", "#575757", 2, 16, 5);

    time_t t;
    struct tm *info;
    time(&t);
    info = localtime(&t);
    cur += strftime(str+cur, sizeof(str)-cur-1, " %a %b %_2d %H:%M ", info);
    XStoreName(dpy, DefaultRootWindow(dpy), str);
    XSync(dpy, False);
}


int
main(int argc, char *argv[]) {
    int device_count = 0;

    if (fork() == 0) {
        Display *dpy = XOpenDisplay(NULL);
        if (!dpy) {
            fprintf(stderr, "%s: Failed to open display\n", argv[0]);
            exit(-1);
        }

        for (;;) {
            status(dpy, device_count);
            sleep(2);
        }
    }
}
