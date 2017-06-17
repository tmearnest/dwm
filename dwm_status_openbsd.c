#include <stdio.h>
#include <err.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <X11/Xlib.h>
#include <sys/types.h>
#include <machine/apmvar.h>
#include <sys/ioctl.h>
#include <fcntl.h>

void
status(Display *dpy, int device_count)
{
	int fd;
	int cur = 0;
	char str[256];

        struct apm_power_info power_info;

	bzero(&power_info, sizeof power_info);
	fd = open("/dev/apm", O_RDONLY);
        const char* ac;
        const char* color_start;
        const char* color_end;

	ac = color_start = color_end = "";

	if (ioctl(fd, APM_IOC_GETPOWER, &power_info) == 0) {
        	switch (power_info.ac_state) {
			case APM_AC_OFF:
				break;
			case APM_AC_ON:
				ac = "ac";
				break;
			case APM_AC_BACKUP:
				ac = "bk";
				break;
			default:
				ac = "??";
				break;
		}

		if (power_info.battery_life<10) {
			color_start = "^c#ffff00^";
			color_end = "^d^";
		}

	} else {
		XStoreName(dpy, DefaultRootWindow(dpy), "ioctl failed");
		XSync(dpy, False);
		err(1, NULL);
	}

	close(fd);

	cur += snprintf(str, sizeof(str)-cur-1, "%s%3d%%%s %s", color_start, 
		        power_info.battery_life, color_end, ac);

	time_t t;
	struct tm *info;
	time(&t);
	info = localtime(&t);
	cur += strftime(str+cur, sizeof(str)-cur-1, " %a %b %d %H:%M ", info);
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}


int
main(int argc, char *argv[])
{
	int device_count = 0;

	if (fork() == 0) {
		Display *dpy = XOpenDisplay(NULL);
		if (!dpy) {
			errx(1, "%s: Failed to open display\n", argv[0]);
		}

		for (;;) {
			status(dpy, device_count);
			sleep(2);
		}
	}
	return 0;
}
