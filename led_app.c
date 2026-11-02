/*
 * led_app.c
 *
 *  Created on: 15 Sept 2026
 *      Author: albert.minasyan@gmail.com
 */

// Build the user mode application to call the driver write / read functions:
// aarch64-linux-gnu-gcc -o led_app led_app.c
// copy to pi4 and run there
// file led_app  - check if it is 64 bit executable
// sudo ./led_app 1
// sudo cat /sys/kernel/debug/gpio | grep -A1 "gpio-17"   # should show "out hi"
// sudo ./led_app 0
// sudo cat /sys/kernel/debug/gpio | grep -A1 "gpio-17"   # should show "out lo"

/* led_app.c */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s [0|1]\n", argv[0]);
        return 1;
    }

    int fd = open("/dev/led0", O_WRONLY);

    if (fd < 0)
    {
    	perror("open"); return 1;
    }

    write(fd, argv[1], 1);
    close(fd);

    return 0;
}
