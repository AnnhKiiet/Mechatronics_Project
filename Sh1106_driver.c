#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

#include "sh1106.h"

#define DEVICE_NAME "oled_sh1106"
#define CLASS_NAME "oled"

// IOCTL command codes
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

// Structures for complex IOCTL commands
struct cursor_pos {
    uint8_t line_no;
    uint8_t cursor_pos;
};

struct scroll_horizontal {
    bool is_left_scroll;
    uint8_t start_line_no;
    uint8_t end_line_no;
};

struct scroll_vertical_horizontal {
    bool is_vertical_left_scroll;
    uint8_t start_line_no;
    uint8_t end_line_no;
    uint8_t vertical_area;
    uint8_t rows;
};

// Function prototypes for character device operations
static int oled_open(struct inode *inodep, struct file *filep);
static int oled_release(struct inode *inodep, struct file *filep);
static long oled_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);

// File operations structure
static struct file_operations fops =
{
    .open = oled_open,
    .release = oled_release,
    .unlocked_ioctl = oled_ioctl,
};

// Open function
static int oled_open(struct inode *inodep, struct file *filep)
{
    pr_info("OLED device opened\n");
    return 0;
}

// Release function
static int oled_release(struct inode *inodep, struct file *filep)
{
    pr_info("OLED device closed\n");
    return 0;
}

// IOCTL function
static long oled_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct cursor_pos cursor;
    struct scroll_horizontal scroll_h;
    struct scroll_vertical_horizontal scroll_vh;
    char *str = NULL;
    unsigned char c;
    bool invert;
    uint8_t value;
    switch (cmd)
    {
        case IOCTL_INIT_DISPLAY:
            OLED_SH1106_DisplayInit();
            pr_info("OLED initialized\n");
            break;
        case IOCTL_DEINIT_DISPLAY:
            OLED_SH1106_DisplayDeInit();
            pr_info("OLED deinitialized\n");
            break;
        case IOCTL_SET_CURSOR:
            if (copy_from_user(&cursor, (struct cursor_pos __user *)arg, sizeof(cursor)))
                return -EFAULT;
            OLED_SH1106_SetCursor(cursor.line_no, cursor.cursor_pos);
            pr_info("Cursor set to line %d, position %d\n", cursor.line_no, cursor.cursor_pos);
            break;
        case IOCTL_NEXT_LINE:
            OLED_SH1106_GoToNextLine();
            pr_info("Cursor moved to next line\n");
            break;
        case IOCTL_PRINT_CHAR:
            if (copy_from_user(&c, (unsigned char __user *)arg, sizeof(c)))
                return -EFAULT;
            OLED_SH1106_PrintChar(c);
            pr_info("Printed character %c\n", c);
            break;
        case IOCTL_PRINT_STRING:
            str = kzalloc(256, GFP_KERNEL);
            if (!str)
                return -ENOMEM;
            if (copy_from_user(str, (char __user *)arg, 255))
            {
                kfree(str);
                return -EFAULT;
            }
            OLED_SH1106_String(str);
            kfree(str);
            pr_info("Printed string\n");
            break;
        case IOCTL_INVERT_DISPLAY:
            if (copy_from_user(&invert, (bool __user *)arg, sizeof(invert)))
                return -EFAULT;
            OLED_SH1106_InvertDisplay(invert);
            pr_info("Inverted display: %d\n", invert);
            break;
        case IOCTL_SET_BRIGHTNESS:
            if (copy_from_user(&value, (uint8_t __user *)arg, sizeof(value)))
                return -EFAULT;
            OLED_SH1106_SetBrightness(value);
            pr_info("Set brightness to %d\n", value);
            break;
        case IOCTL_START_SCROLL_HORIZONTAL:
            if (copy_from_user(&scroll_h, (struct scroll_horizontal __user *)arg, sizeof(scroll_h)))
                return -EFAULT;
            OLED_SH1106_StartScrollHorizontal(scroll_h.is_left_scroll, scroll_h.start_line_no, scroll_h.end_line_no);
            pr_info("Started horizontal scroll\n");
            break;
        case IOCTL_START_SCROLL_VERT_HOR:
            if (copy_from_user(&scroll_vh, (struct scroll_vertical_horizontal __user *)arg, sizeof(scroll_vh)))
                return -EFAULT;
            OLED_SH1106_StartScrollVerticalHorizontal(scroll_vh.is_vertical_left_scroll, scroll_vh.start_line_no, scroll_vh.end_line_no, scroll_vh.vertical_area, scroll_vh.rows);
            pr_info("Started vertical-horizontal scroll\n");
            break;
        case IOCTL_DEACTIVATE_SCROLL:
            OLED_SH1106_DeactivateScroll();
            pr_info("Deactivated scroll\n");
            break;
        case IOCTL_FILL_DISPLAY:
            if (copy_from_user(&value, (uint8_t __user *)arg, sizeof(value)))
                return -EFAULT;
            OLED_SH1106_fill(value);
            pr_info("Filled display with 0x%x\n", value);
            break;
        case IOCTL_CLEAR_DISPLAY:
            OLED_SH1106_ClearDisplay();
            pr_info("Cleared display\n");
            break;
        case IOCTL_PRINT_LOGO:
            OLED_SH1106_PrintLogo();
            pr_info("Printed logo\n");
            break;
        default:
            return -EINVAL;
    }
    return 0;
}
static struct spi_device *OLED_spi_device;

// register information about your slave device
struct spi_board_info OLED_spi_device_info = 
{
    .modalias       = "OLED-spi-sh1106-driver",
    .max_speed_hz   = 4000000,          // speed your device (slave) can handle
    .bus_num        = SPI_BUS_NUM,      //SPI 1
    .chip_select    = 0,                // Use 0 chip select (GPIO 18)
    .mode           = SPI_MODE_0        // SPI mode 0
};

/****************************************************************************
 * Name: OLED_spi_write
 *
 * Details : This function writes the 1-byte data to the slave device using SPI.
 ****************************************************************************/
 int OLED_spi_write( uint8_t data)
 {
    int ret    = -1;
    uint8_t rx = 0x00;

    if(OLED_spi_device)  // after declaration, it will return 1
    {
        // prepare data
        struct spi_transfer tr = 
        {
            .tx_buf = &data,
            .rx_buf = &rx,
            .len    = 1,
        };
        // transfer data
        ret = spi_sync_transfer(OLED_spi_device, &tr, 1);
    }
    return (ret);
 }
 /****************************************************************************
 * Name: OLED_spi_init
 *
 * Details : This function Register and Initilize the SPI.
 ****************************************************************************/

static int __init OLED_spi_init(void)
{
    int ret;
    // Get the spi controller driver
    struct spi_master *master;
    // Allocate major number
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0)
    {
        pr_err("Failed to register a major number\n");
        return major_number;
    }
    pr_info("Registered correctly with major number %d\n", major_number);

    // Register the device class
    oled_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(oled_class))
    {
        unregister_chrdev(major_number, DEVICE_NAME);
        pr_err("Failed to register device class\n");
        return PTR_ERR(oled_class);
    }
    pr_info("Device class registered correctly\n");

    // Register the device driver
    oled_device = device_create(oled_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(oled_device))
    {
        class_destroy(oled_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        pr_err("Failed to create the device\n");
        return PTR_ERR(oled_device);
    }
    pr_info("Device class created correctly\n");

    master = spi_busnum_to_master(OLED_spi_device_info.bus_num);

    if(master == NULL)
    {
        pr_err("SPI Master not found.\n");
        return -ENODEV;
    }


    //create a new slave device, given the master and device info
    OLED_spi_device = spi_new_device(master, &OLED_spi_device_info);
    if(OLED_spi_device == NULL)
    {   
        pr_err("Failed to create slave.\n");
        return -ENODEV;
    }

    // 8bits in a word
    OLED_spi_device-> bits_per_word = 8;

    // set the SPI slave device

    ret = spi_setup(OLED_spi_device);
    if (ret)
    {
        pr_err("Failed to setup slave.\n");
        spi_unregister_device(OLED_spi_device);
        return -ENODEV;
    }
    return 0;
}
static void __exit OLED_spi_exit(void)
{
    if(OLED_spi_device)
    {
        OLED_SH1106_ClearDisplay();
        OLED_SH1106_DisplayDeInit();
        spi_unregister_device(OLED_spi_device);
        pr_info("SPI driver Unregistered.\n");
    }
    device_destroy(oled_class, MKDEV(major_number, 0));
    class_unregister(oled_class);
    class_destroy(oled_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("OLED device driver unregistered\n");
}
module_init(OLED_spi_init);
module_exit(OLED_spi_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("GROUP 9");
MODULE_DESCRIPTION("OLED driver");
MODULE_VERSION("0.0");