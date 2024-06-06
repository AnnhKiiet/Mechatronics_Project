#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h> // Include errno header
#include <stdint.h>

#define IOCTL_INIT_DISPLAY               _IO('O', 0)
#define IOCTL_DEINIT_DISPLAY             _IO('O', 1)
#define IOCTL_SET_CURSOR                 _IOW('O', 2, struct cursor_pos)
#define IOCTL_NEXT_LINE                  _IO('O', 3)
#define IOCTL_PRINT_CHAR                 _IOW('O', 4, unsigned char)
#define IOCTL_PRINT_STRING               _IOW('O', 5, char *)
#define IOCTL_INVERT_DISPLAY             _IOW('O', 6, bool)
#define IOCTL_SET_BRIGHTNESS             _IOW('O', 7, uint8_t)
#define IOCTL_START_SCROLL_HORIZONTAL    _IOW('O', 8, struct scroll_horizontal)
#define IOCTL_START_SCROLL_VERT_HOR      _IOW('O', 9, struct scroll_vertical_horizontal)
#define IOCTL_DEACTIVATE_SCROLL          _IO('O', 10)
#define IOCTL_FILL_DISPLAY               _IOW('O', 11, uint8_t)
#define IOCTL_CLEAR_DISPLAY              _IO('O', 12)
#define IOCTL_PRINT_LOGO                 _IO('O', 13)

#define DEVICE_PATH "/dev/oled_sh1106"

int main() {
    // Open the device
    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open the device");
        return errno;
    }
    printf("Open devices successfull!\n");


    // Close the device file
    close(fd);
    return 0;
}
