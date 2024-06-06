#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/gpio.h>



#define DEVICE_NAME "oled_sh1106"
#define CLASS_NAME "oled"

// IOCTL commands
#define IOCTL_INIT_DISPLAY _IO('O', 1)
#define IOCTL_DEINIT_DISPLAY _IO('O', 2)
// Define other IOCTL commands...

// Structures for complex IOCTL commands
struct cursor_pos
{
    uint8_t line_no;
    uint8_t cursor_pos;
};

struct scroll_horizontal
{
    bool is_left_scroll;
    uint8_t start_line_no;
    uint8_t end_line_no;
};

struct scroll_vertical_horizontal
{
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


#define SH1106_RST_PIN         (  24 )   // Reset pin is GPIO 24
#define SH1106_DC_PIN          (  23 )   // Data/Command pin is GPIO 23
#define SH1106_MAX_SEG         ( 128 )   // Maximum segment
#define SH1106_MAX_LINE        (   7 )   // Maximum line
#define SH1106_DEF_FONT_SIZE   (   5 )   // Default font size

extern int OLED_spi_write( uint8_t data );

extern int  OLED_SH1106_DisplayInit(void);
extern void OLED_SH1106_DisplayDeInit(void);

void OLED_SH1106_SetCursor( uint8_t lineNo, uint8_t cursorPos );
void OLED_SH1106_GoToNextLine( void );
void OLED_SH1106_PrintChar(unsigned char c);
void OLED_SH1106_String(char *str);
void OLED_SH1106_InvertDisplay(bool need_to_invert);
void OLED_SH1106_SetBrightness(uint8_t brightnessValue);
void OLED_SH1106_StartScrollHorizontal( bool is_left_scroll,
                                        uint8_t start_line_no,
                                        uint8_t end_line_no
                                      );
void OLED_SH1106_StartScrollVerticalHorizontal( 
                                                bool is_vertical_left_scroll,
                                                uint8_t start_line_no,
                                                uint8_t end_line_no,
                                                uint8_t vertical_area,
                                                uint8_t rows
                                              );
void OLED_SH1106_DeactivateScroll( void );
void OLED_SH1106_fill( uint8_t data );
void OLED_SH1106_ClearDisplay( void );
void OLED_SH1106_PrintLogo( void );


// File operations structure
static struct file_operations fops = {
    .open = oled_open,
    .release = oled_release,
    .unlocked_ioctl = oled_ioctl,
};

// SPI device
static struct spi_device *OLED_spi_device;

// Major number for character device
static int major_number;

// Class pointer for device class
static struct class *oled_class = NULL;

// Function prototypes for SH1106 control functions
// Declare other function prototypes...

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
    // Implement IOCTL operations...
    return 0;
}

// Probe function
static int oled_probe(struct spi_device *spi)
{
    struct device *dev;
    int ret;
    u32 spi_freq;
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0)
    {
        pr_err("Failed to register char device\n");
        return major_number;
    }
    if (!oled_class)
    {
        oled_class = class_create(THIS_MODULE, CLASS_NAME);
        if (IS_ERR(oled_class))
        {
            pr_err("Failed to create class\n");
            unregister_chrdev(major_number, DEVICE_NAME);
            printk(KERN_ERR "Failed to register device class\n");
            return PTR_ERR(oled_class);
        }
    }

    dev = device_create(oled_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(dev))
    {
        pr_err("Failed to create device\n");
        unregister_chrdev(major_number, DEVICE_NAME);
        class_destroy(oled_class);
        printk(KERN_ERR "Failed to create the device\n");
        return PTR_ERR(dev);
    }

    // Get SPI frequency from device tree
    ret = of_property_read_u32(spi->dev.of_node, "spi-max-frequency", &spi_freq);
    if (ret)
    {
        pr_err("Failed to read SPI frequency from device tree\n");
        unregister_chrdev(major_number, DEVICE_NAME);
        device_destroy(oled_class, MKDEV(major_number, 0));
        class_destroy(oled_class);
        return ret;
    }

    // Set up SPI device
    spi->max_speed_hz = spi_freq;
    spi_setup(spi);
    OLED_spi_device = spi;
    
    pr_info("OLED SPI driver probed\n");
    return 0;
}

// Remove function
static void oled_remove(struct spi_device *spi)
{
    if (OLED_spi_device)
    {
        OLED_spi_device = NULL;
    }
    if (oled_class)
    {
        unregister_chrdev(major_number, DEVICE_NAME);
        device_destroy(oled_class, MKDEV(major_number, 0));
        class_destroy(oled_class);
    }
    pr_info("OLED SPI driver removed\n");
}

// SPI write function
int OLED_spi_write(uint8_t data)
{
    int ret = -1;
    uint8_t rx = 0x00;

    if (OLED_spi_device) // after declaration, it will return 1
    {
        // prepare data
        struct spi_transfer tr = {
            .tx_buf = &data,
            .rx_buf = &rx,
            .len = 1,
        };
        // transfer data
        ret = spi_sync_transfer(OLED_spi_device, &tr, 1);
    }
    return (ret);
}

// Device tree compatible strings
static const struct of_device_id oled_spi_dt_ids[] = {
    {.compatible = "sh1106"},
    {},
};
MODULE_DEVICE_TABLE(of, oled_spi_dt_ids);

// SPI device ID
// static struct spi_device_id oled_spi_id[] = {
//     {"oled_sh1106", 0},
//     {"sh1106", 0}, // Sửa lại dòng này
//     {},
// };
//MODULE_DEVICE_TABLE(spi, oled_spi_id);

// SPI driver structure
static struct spi_driver oled_spi_driver = {
    .driver = {
        .name = "oled_spi_driver",
        .of_match_table = of_match_ptr(oled_spi_dt_ids),
    },
    .probe = oled_probe,
    .remove = oled_remove,
    //.id_table = oled_spi_id,
    
};

// Module initialization
static int __init oled_init(void)
{
    int ret;

    

    ret = spi_register_driver(&oled_spi_driver);
    if (ret < 0)
    {
        unregister_chrdev(major_number, DEVICE_NAME);
        pr_err("Failed to register SPI driver\n");
        return ret;
    }
    OLED_SH1106_DisplayDeInit();


    pr_info("OLED driver initialized\n");
    return 0;
}

// Module cleanup
static void __exit oled_exit(void)
{
    spi_unregister_driver(&oled_spi_driver);
    if (oled_class)
    {
        class_destroy(oled_class);
    }
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("OLED driver exited\n");
}
static uint8_t SH1106_LineNum   = 0;
static uint8_t SH1106_CursorPos = 0;
static uint8_t SH1106_FontSize  = SH1106_DEF_FONT_SIZE;

/*
**  EmbeTronicX Logo
*/
static const uint8_t OLED_logo[1024] = {
  0xFF, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xE1, 0xF9, 0xFF, 0xF9, 0xE1, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xFF,
  0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xF8, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0x3F, 0xFF,
  0xF8, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,
  0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0xFF, 0xFF, 0x0F, 0xF8, 0xF7, 0x00, 0xBF, 0xC0, 0x7F,
  0xFF, 0xFF, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,
  0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x3F, 0xE0, 0x0F, 0x7F, 0x00, 0xFF, 0x7F, 0x80,
  0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x04, 0xFC, 0xFC, 0x0C, 0x0C, 0x0C, 0x0C, 0x7C, 0x00, 0x00, 0x00,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x04, 0xFC, 0xF8,
  0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x08,
  0x04, 0x04, 0xFC, 0xFC, 0x04, 0x04, 0x04, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0x98, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x00, 0x00, 0x04, 0x0C, 0x38, 0xE0, 0x80, 0xE0, 0x38, 0x0C, 0x04, 0x00, 0x00, 0x00, 0x00, 0xFF,
  0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xC0, 0x3F, 0xFF, 0xFF, 0x00, 0xFD, 0xFE, 0xFF,
  0x01, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x06, 0x06, 0x06, 0x06, 0xE0, 0x00, 0x00, 0x00,
  0x00, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
  0x00, 0x00, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x7C, 0xFF, 0x11, 0x10, 0x1F, 0x1F, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x7C, 0xFF, 0x01, 0x00, 0x01, 0xFF, 0x7C, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF,
  0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x87,
  0x00, 0x00, 0x00, 0x00, 0xE0, 0x3F, 0x1F, 0xFF, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,
  0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x1F, 0x3F, 0xFC, 0xF7, 0x00, 0xDF, 0xE3, 0x7D,
  0x3E, 0x07, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00,
  0x00, 0x03, 0x03, 0x00, 0x03, 0x03, 0x00, 0x00, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03,
  0x02, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x02, 0x02, 0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x01, 0x03, 0x02, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00, 0x00, 0x03,
  0x03, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x02, 0x02, 0x03,
  0x00, 0x00, 0x02, 0x03, 0x01, 0x00, 0x00, 0x00, 0x01, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0xFF,
  0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xFF, 0x00, 0xFF, 0x03, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x88, 0x88, 0x08, 0x00, 0xE0, 0x20, 0x20, 0xE0, 0x20, 0xE0,
  0x00, 0xFC, 0x20, 0x20, 0xE0, 0x00, 0xE0, 0x20, 0x20, 0xE0, 0x00, 0xE0, 0x20, 0x20, 0xFC, 0x00,
  0xE0, 0x20, 0x20, 0xFC, 0x00, 0xE0, 0x20, 0x20, 0xE0, 0x00, 0xE0, 0x20, 0x20, 0xFC, 0x00, 0x00,
  0x00, 0x08, 0x08, 0xF8, 0x08, 0x08, 0x00, 0xE0, 0x00, 0xE0, 0x00, 0xFC, 0x20, 0x20, 0x00, 0xE0,
  0x20, 0x20, 0xE0, 0x00, 0xE0, 0x20, 0x20, 0x00, 0xEC, 0x00, 0x20, 0x20, 0x20, 0xE0, 0x00, 0xFC,
  0x00, 0xE0, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0xC8, 0x38, 0x00, 0x00, 0xE0,
  0x20, 0x20, 0xE0, 0x00, 0xE0, 0x20, 0xE0, 0x00, 0xE0, 0x20, 0x20, 0xE0, 0x00, 0x00, 0x00, 0xFF,
  0xFF, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x9C, 0x9C, 0x88, 0x8E, 0x83, 0x98, 0x83, 0x8E, 0x88,
  0x88, 0x9C, 0x9C, 0x80, 0x80, 0x87, 0x84, 0x84, 0x84, 0x80, 0x87, 0x80, 0x80, 0x87, 0x80, 0x87,
  0x80, 0x87, 0x84, 0x84, 0x87, 0x80, 0x87, 0x85, 0x85, 0x85, 0x80, 0x87, 0x84, 0x84, 0x87, 0x80,
  0x87, 0x84, 0x84, 0x87, 0x80, 0x87, 0x85, 0x85, 0x85, 0x80, 0x87, 0x84, 0x84, 0x87, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x87, 0x80, 0x80, 0x80, 0x87, 0x84, 0x87, 0x80, 0x87, 0x84, 0x84, 0x80, 0x87,
  0x84, 0x84, 0x87, 0x80, 0x87, 0x80, 0x80, 0x80, 0x87, 0x80, 0x87, 0x85, 0x85, 0x87, 0x80, 0x87,
  0x80, 0x85, 0x85, 0x85, 0x87, 0x80, 0x80, 0x80, 0x80, 0x86, 0x85, 0x84, 0x84, 0x84, 0x80, 0x87,
  0x84, 0x84, 0x87, 0x80, 0x87, 0x80, 0x87, 0x80, 0x87, 0x85, 0x85, 0x85, 0x80, 0x80, 0x80, 0xFF,
};

/*
** Array Variable to store the letters.
*/ 
static const unsigned char SH1106_font[][SH1106_DEF_FONT_SIZE]= 
{
    {0x00, 0x00, 0x00, 0x00, 0x00},   // space
    {0x00, 0x00, 0x2f, 0x00, 0x00},   // !
    {0x00, 0x07, 0x00, 0x07, 0x00},   // "
    {0x14, 0x7f, 0x14, 0x7f, 0x14},   // #
    {0x24, 0x2a, 0x7f, 0x2a, 0x12},   // $
    {0x23, 0x13, 0x08, 0x64, 0x62},   // %
    {0x36, 0x49, 0x55, 0x22, 0x50},   // &
    {0x00, 0x05, 0x03, 0x00, 0x00},   // '
    {0x00, 0x1c, 0x22, 0x41, 0x00},   // (
    {0x00, 0x41, 0x22, 0x1c, 0x00},   // )
    {0x14, 0x08, 0x3E, 0x08, 0x14},   // *
    {0x08, 0x08, 0x3E, 0x08, 0x08},   // +
    {0x00, 0x00, 0xA0, 0x60, 0x00},   // ,
    {0x08, 0x08, 0x08, 0x08, 0x08},   // -
    {0x00, 0x60, 0x60, 0x00, 0x00},   // .
    {0x20, 0x10, 0x08, 0x04, 0x02},   // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E},   // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00},   // 1
    {0x42, 0x61, 0x51, 0x49, 0x46},   // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31},   // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10},   // 4
    {0x27, 0x45, 0x45, 0x45, 0x39},   // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30},   // 6
    {0x01, 0x71, 0x09, 0x05, 0x03},   // 7
    {0x36, 0x49, 0x49, 0x49, 0x36},   // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E},   // 9
    {0x00, 0x36, 0x36, 0x00, 0x00},   // :
    {0x00, 0x56, 0x36, 0x00, 0x00},   // ;
    {0x08, 0x14, 0x22, 0x41, 0x00},   // <
    {0x14, 0x14, 0x14, 0x14, 0x14},   // =
    {0x00, 0x41, 0x22, 0x14, 0x08},   // >
    {0x02, 0x01, 0x51, 0x09, 0x06},   // ?
    {0x32, 0x49, 0x59, 0x51, 0x3E},   // @
    {0x7C, 0x12, 0x11, 0x12, 0x7C},   // A
    {0x7F, 0x49, 0x49, 0x49, 0x36},   // B
    {0x3E, 0x41, 0x41, 0x41, 0x22},   // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C},   // D
    {0x7F, 0x49, 0x49, 0x49, 0x41},   // E
    {0x7F, 0x09, 0x09, 0x09, 0x01},   // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A},   // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F},   // H
    {0x00, 0x41, 0x7F, 0x41, 0x00},   // I
    {0x20, 0x40, 0x41, 0x3F, 0x01},   // J
    {0x7F, 0x08, 0x14, 0x22, 0x41},   // K
    {0x7F, 0x40, 0x40, 0x40, 0x40},   // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},   // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F},   // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E},   // O
    {0x7F, 0x09, 0x09, 0x09, 0x06},   // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E},   // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46},   // R
    {0x46, 0x49, 0x49, 0x49, 0x31},   // S
    {0x01, 0x01, 0x7F, 0x01, 0x01},   // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F},   // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F},   // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F},   // W
    {0x63, 0x14, 0x08, 0x14, 0x63},   // X
    {0x07, 0x08, 0x70, 0x08, 0x07},   // Y
    {0x61, 0x51, 0x49, 0x45, 0x43},   // Z
    {0x00, 0x7F, 0x41, 0x41, 0x00},   // [
    {0x55, 0xAA, 0x55, 0xAA, 0x55},   // Backslash (Checker pattern)
    {0x00, 0x41, 0x41, 0x7F, 0x00},   // ]
    {0x04, 0x02, 0x01, 0x02, 0x04},   // ^
    {0x40, 0x40, 0x40, 0x40, 0x40},   // _
    {0x00, 0x03, 0x05, 0x00, 0x00},   // `
    {0x20, 0x54, 0x54, 0x54, 0x78},   // a
    {0x7F, 0x48, 0x44, 0x44, 0x38},   // b
    {0x38, 0x44, 0x44, 0x44, 0x20},   // c
    {0x38, 0x44, 0x44, 0x48, 0x7F},   // d
    {0x38, 0x54, 0x54, 0x54, 0x18},   // e
    {0x08, 0x7E, 0x09, 0x01, 0x02},   // f
    {0x18, 0xA4, 0xA4, 0xA4, 0x7C},   // g
    {0x7F, 0x08, 0x04, 0x04, 0x78},   // h
    {0x00, 0x44, 0x7D, 0x40, 0x00},   // i
    {0x40, 0x80, 0x84, 0x7D, 0x00},   // j
    {0x7F, 0x10, 0x28, 0x44, 0x00},   // k
    {0x00, 0x41, 0x7F, 0x40, 0x00},   // l
    {0x7C, 0x04, 0x18, 0x04, 0x78},   // m
    {0x7C, 0x08, 0x04, 0x04, 0x78},   // n
    {0x38, 0x44, 0x44, 0x44, 0x38},   // o
    {0xFC, 0x24, 0x24, 0x24, 0x18},   // p
    {0x18, 0x24, 0x24, 0x18, 0xFC},   // q
    {0x7C, 0x08, 0x04, 0x04, 0x08},   // r
    {0x48, 0x54, 0x54, 0x54, 0x20},   // s
    {0x04, 0x3F, 0x44, 0x40, 0x20},   // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C},   // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C},   // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C},   // w
    {0x44, 0x28, 0x10, 0x28, 0x44},   // x
    {0x1C, 0xA0, 0xA0, 0xA0, 0x7C},   // y
    {0x44, 0x64, 0x54, 0x4C, 0x44},   // z
    {0x00, 0x10, 0x7C, 0x82, 0x00},   // {
    {0x00, 0x00, 0xFF, 0x00, 0x00},   // |
    {0x00, 0x82, 0x7C, 0x10, 0x00},   // }
    {0x00, 0x06, 0x09, 0x09, 0x06}    // ~ (Degrees)
};

/****************************************************************************
 * Name: OLED_sh1106_ResetDcInit
 *
 * Details : This function Initializes and Configures the Reset and DC Pin
 ****************************************************************************/
static int OLED_sh1106_ResetDcInit( void )
{
  int ret = 0;
    
  //do while(false) loop to break if any error
  do
  {
    
    /* Register the Reset GPIO */
    
    //Checking the Reset GPIO is valid or not
    if( gpio_is_valid( SH1106_RST_PIN ) == false )
    {
      pr_err("Reset GPIO %d is not valid\n", SH1106_RST_PIN);
      ret = -1;
      break;
    }
    
    //Requesting the Reset GPIO
    if( gpio_request( SH1106_RST_PIN, "SH1106_RST_PIN" ) < 0 )
    {
      pr_err("ERROR: Reset GPIO %d request\n", SH1106_RST_PIN);
      ret = -1;
      break;
    }
    
    //configure the Reset GPIO as output
    gpio_direction_output( SH1106_RST_PIN, 1 );
    
    /* Register the DC GPIO */
    
    //Checking the DC GPIO is valid or not
    if( gpio_is_valid( SH1106_DC_PIN ) == false )
    {
      pr_err("DC GPIO %d is not valid\n", SH1106_DC_PIN);
      gpio_free( SH1106_RST_PIN );   // free the reset GPIO
      ret = -1;
      break;
    }
    
    //Requesting the DC GPIO
    if( gpio_request( SH1106_DC_PIN, "SH1106_DC_PIN" ) < 0 )
    {
      pr_err("ERROR: DC GPIO %d request\n", SH1106_DC_PIN);
      gpio_free( SH1106_RST_PIN );   // free the reset GPIO
      ret = -1;
      break;
    }
    
    //configure the Reset GPIO as output
    gpio_direction_output( SH1106_DC_PIN, 1 );
    
  } while( false );
  
  //pr_info("DC Reset GPIOs init Done!\n");
  return( ret );
}

/****************************************************************************
 * Name: OLED_SH1106_ResetDcDeInit
 *
 * Details : This function De-initializes the Reset and DC Pin
 ****************************************************************************/
static void OLED_SH1106_ResetDcDeInit( void )
{
  gpio_free( SH1106_RST_PIN );   // free the reset GPIO
  gpio_free( SH1106_DC_PIN );    // free the DC GPIO
}

/****************************************************************************
 * Name: OLED_SH1106_setRst
 *
 * Details : This function writes the value to the Reset GPIO
 * 
 * Argument: 
 *            value - value to be set ( 0 or 1 )
 * 
 ****************************************************************************/
static void OLED_SH1106_setRst( uint8_t value )
{
  gpio_set_value( SH1106_RST_PIN, value );
}

/****************************************************************************
 * Name: OLED_SH1106_setDc
 *
 * Details : This function writes the value to the DC GPIO
 * 
 * Argument: 
 *            value - value to be set ( 0 or 1 )
 * 
 ****************************************************************************/
static void OLED_SH1106_setDc( uint8_t value )
{
  gpio_set_value( SH1106_DC_PIN, value );
}

/****************************************************************************
 * Name: OLED_SH1106_Write
 *
 * Details : This function sends the command/data to the Display
 *
 * Argument: is_cmd
 *              true  - if we need to send command
 *              false - if we need to send data
 *           value
 *              value to be transmitted
 ****************************************************************************/
static int OLED_SH1106_Write( bool is_cmd, uint8_t data )
{
  int     ret = 0;
  uint8_t pin_value;

  if( is_cmd )
  {
    //DC pin has to be low, if this is command.
    pin_value = 0u;
  }
  else
  {
    //DC pin has to be high, if this is data.
    pin_value = 1u;
  }
  
  OLED_SH1106_setDc( pin_value );
  
  //pr_info("Writing 0x%02X \n", data);
  
  //send the byte
  ret = OLED_spi_write( data );
  
  return( ret );
}

/****************************************************************************
 * Name: OLED_SH1106_SetCursor
 *
 * Details : This function is specific to the SSD_1306 OLED.
 *
 * Argument:
 *              lineNo    -> Line Number
 *              cursorPos -> Cursor Position
 * 
 ****************************************************************************/
void OLED_SH1106_SetCursor( uint8_t lineNo, uint8_t cursorPos )
{
  /* Move the Cursor to specified position only if it is in range */
  if((lineNo <= SH1106_MAX_LINE) && (cursorPos < SH1106_MAX_SEG))
  {
    SH1106_LineNum   = lineNo;                 // Save the specified line number
    SH1106_CursorPos = cursorPos;              // Save the specified cursor position
    
    OLED_SH1106_Write(true, 0x21);              // cmd for the column start and end address
    OLED_SH1106_Write(true, cursorPos);         // column start addr
    OLED_SH1106_Write(true, SH1106_MAX_SEG-1); // column end addr
    OLED_SH1106_Write(true, 0x22);              // cmd for the page start and end address
    OLED_SH1106_Write(true, lineNo);            // page start addr
    OLED_SH1106_Write(true, SH1106_MAX_LINE);  // page end addr
  }
}

/****************************************************************************
 * Name: OLED_SH1106_GoToNextLine
 *
 * Details : This function is specific to the SSD_1306 OLED and move the cursor 
 *           to the next line.
 ****************************************************************************/
void OLED_SH1106_GoToNextLine( void )
{
  /*
  ** Increment the current line number.
  ** roll it back to first line, if it exceeds the limit. 
  */
  SH1106_LineNum++;
  SH1106_LineNum = (SH1106_LineNum & SH1106_MAX_LINE);
  OLED_SH1106_SetCursor(SH1106_LineNum,0); /* Finally move it to next line */
}

/****************************************************************************
 * Name: OLED_SH1106_PrintChar
 *
 * Details : This function is specific to the SSD_1306 OLED and sends 
 *           the single char to the OLED.
 * 
 * Arguments:
 *           c   -> character to be written
 * 
 ****************************************************************************/
void OLED_SH1106_PrintChar( unsigned char c )
{
  uint8_t data_byte;
  uint8_t temp = 0;
  
  /*
  ** If we character is greater than segment len or we got new line charcter
  ** then move the cursor to the new line
  */ 
  if( (( SH1106_CursorPos + SH1106_FontSize ) >= SH1106_MAX_SEG ) ||
      ( c == '\n' )
  )
  {
    OLED_SH1106_GoToNextLine();
  }
  
  // print charcters other than new line
  if( c != '\n' )
  {
  
    /*
    ** In our font array (SH1106_font), space starts in 0th index.
    ** But in ASCII table, Space starts from 32 (0x20).
    ** So we need to match the ASCII table with our font table.
    ** We can subtract 32 (0x20) in order to match with our font table.
    */
    c -= 0x20;  //or c -= ' ';
    do
    {
      data_byte= SH1106_font[c][temp];         // Get the data to be displayed from LookUptable
      OLED_SH1106_Write(false, data_byte);  // write data to the OLED
      SH1106_CursorPos++;
      
      temp++;
      
    } while ( temp < SH1106_FontSize);
    
    OLED_SH1106_Write(false, 0x00);         //Display the data
    SH1106_CursorPos++;
  }
}

/****************************************************************************
 * Name: OLED_SH1106_String
 *
 * Details : This function is specific to the SSD_1306 OLED and sends 
 *           the string to the OLED.
 * 
 * Arguments:
 *           str   -> string to be written
 * 
 ****************************************************************************/
void OLED_SH1106_String(char *str)
{
  while( *str )
  {
    OLED_SH1106_PrintChar(*str++);
  }
}


/****************************************************************************
 * Name: OLED_SH1106_InvertDisplay
 *
 * Details : This function is specific to the SSD_1306 OLED and 
 *           inverts the display.
 * 
 * Arguments:
 *           need_to_invert   -> true  - invert display
 *                               false - normal display 
 * 
 ****************************************************************************/
void OLED_SH1106_InvertDisplay(bool need_to_invert)
{
  if(need_to_invert)
  {
    OLED_SH1106_Write(true, 0xA7); // Invert the display
  }
  else
  {
    OLED_SH1106_Write(true, 0xA6); // Normal display
  }
}

/****************************************************************************
 * Name: OLED_SH1106_SetBrightness
 *
 * Details : This function is specific to the SSD_1306 OLED and 
 *           sets the brightness of  the display.
 * 
 * Arguments:
 *           brightnessValue   -> brightness value ( 0 - 255 )
 * 
 ****************************************************************************/
void OLED_SH1106_SetBrightness(uint8_t brightnessValue)
{
    OLED_SH1106_Write(true, 0x81);            // Contrast command
    OLED_SH1106_Write(true, brightnessValue); // Contrast value (default value = 0x7F)
}

/****************************************************************************
 * Name: OLED_SH1106_StartScrollHorizontal
 *
 * Details : This function is specific to the SSD_1306 OLED and 
 *           Scrolls the data right/left in horizontally.
 * 
 * Arguments:
 *           is_left_scroll   -> true  - left horizontal scroll
 *                               false - right horizontal scroll
 *           start_line_no    -> Start address of the line to scroll 
 *           end_line_no      -> End address of the line to scroll 
 * 
 ****************************************************************************/
void OLED_SH1106_StartScrollHorizontal( bool is_left_scroll,
                                        uint8_t start_line_no,
                                        uint8_t end_line_no
                                      )
{
  if(is_left_scroll)
  {
    // left horizontal scroll
    OLED_SH1106_Write(true, 0x27);
  }
  else
  {
    // right horizontal scroll 
    OLED_SH1106_Write(true, 0x26);
  }
  
  OLED_SH1106_Write(true, 0x00);            // Dummy byte (dont change)
  OLED_SH1106_Write(true, start_line_no);   // Start page address
  OLED_SH1106_Write(true, 0x00);            // 5 frames interval
  OLED_SH1106_Write(true, end_line_no);     // End page address
  OLED_SH1106_Write(true, 0x00);            // Dummy byte (dont change)
  OLED_SH1106_Write(true, 0xFF);            // Dummy byte (dont change)
  OLED_SH1106_Write(true, 0x2F);            // activate scroll
}

/****************************************************************************
 * Name: OLED_SH1106_StartScrollVerticalHorizontal
 *
 * Details : This function is specific to the SSD_1306 OLED and 
 *           Scrolls the data in vertically and right/left horizontally
 *           (Diagonally).
 * 
 * Arguments:
 *      is_vertical_left_scroll -> true  - vertical and left horizontal scroll
 *                                 false - vertical and right horizontal scroll
 *      start_line_no           -> Start address of the line to scroll 
 *      end_line_no             -> End address of the line to scroll 
 *      vertical_area           -> Area for vertical scroll (0-63)
 *      rows                    -> Number of rows to scroll vertically 
 * 
 ****************************************************************************/
void OLED_SH1106_StartScrollVerticalHorizontal( 
                                                bool is_vertical_left_scroll,
                                                uint8_t start_line_no,
                                                uint8_t end_line_no,
                                                uint8_t vertical_area,
                                                uint8_t rows
                                              )
{
  
  OLED_SH1106_Write(true, 0xA3);            // Set Vertical Scroll Area
  OLED_SH1106_Write(true, 0x00);            // Check datasheet
  OLED_SH1106_Write(true, vertical_area);   // area for vertical scroll
  
  if(is_vertical_left_scroll)
  {
    // vertical and left horizontal scroll
    OLED_SH1106_Write(true, 0x2A);
  }
  else
  {
    // vertical and right horizontal scroll 
    OLED_SH1106_Write(true, 0x29);
  }
  
  OLED_SH1106_Write(true, 0x00);            // Dummy byte (dont change)
  OLED_SH1106_Write(true, start_line_no);   // Start page address
  OLED_SH1106_Write(true, 0x00);            // 5 frames interval
  OLED_SH1106_Write(true, end_line_no);     // End page address
  OLED_SH1106_Write(true, rows);            // Vertical scrolling offset
  OLED_SH1106_Write(true, 0x2F);            // activate scroll
}

/****************************************************************************
 * Name: OLED_SH1106_DeactivateScroll
 *
 * Details : This function disables the scroll.
 ****************************************************************************/
void OLED_SH1106_DeactivateScroll( void )
{
  OLED_SH1106_Write(true, 0x2E); // Deactivate scroll
}

/****************************************************************************
 * Name: OLED_SH1106_fill
 *
 * Details : This function fills the data to the Display
 ****************************************************************************/
void OLED_SH1106_fill( uint8_t data )
{
  // 8 pages x 128 segments x 8 bits of data
  unsigned int total  = ( SH1106_MAX_SEG * (SH1106_MAX_LINE + 1) );
  unsigned int i      = 0;
  
  //Fill the Display
  for(i = 0; i < total; i++)
  {
    OLED_SH1106_Write(false, data);
  }
}

/****************************************************************************
 * Name: OLED_SH1106_ClearDisplay
 *
 * Details : This function clears the Display
 ****************************************************************************/
void OLED_SH1106_ClearDisplay( void )
{
  //Set cursor
  OLED_SH1106_SetCursor(0,0);
  
  OLED_SH1106_fill( 0x00 );
}

/****************************************************************************
 * Name: OLED_SH1106_PrintLogo
 *
 * Details : This function prints the EmbeTronicX Logo
 ****************************************************************************/
void OLED_SH1106_PrintLogo( void )
{
  int i;
  
  //Set cursor
  OLED_SH1106_SetCursor(0,0);
  
  for( i = 0; i < ( SH1106_MAX_SEG * (SH1106_MAX_LINE + 1) ); i++ )
  {
    OLED_SH1106_Write(false, OLED_logo[i]);
  }
}

/****************************************************************************
 * Name: OLED_SH1106_DisplayInit
 *
 * Details : This function Initializes the Display
 ****************************************************************************/
int OLED_SH1106_DisplayInit(void)
{
  int ret = 0;
  
  //Initialize the Reset and DC GPIOs
  ret = OLED_sh1106_ResetDcInit();
  
  if( ret >= 0 )
  {
    //Make the RESET Line to 0
    OLED_SH1106_setRst( 0u );
    msleep(100);                          // delay
    //Make the DC Line to 1
    OLED_SH1106_setRst( 1u );
    msleep(100);                          // delay
    
    /*
    ** Commands to initialize the SSD_1306 OLED Display
    */
    OLED_SH1106_Write(true, 0xAE); // Entire Display OFF
    OLED_SH1106_Write(true, 0xD5); // Set Display Clock Divide Ratio and Oscillator Frequency

    OLED_SH1106_Write(true, 0x80); // Default Setting for Display Clock Divide Ratio and Oscillator Frequency that is recommended
    OLED_SH1106_Write(true, 0xA8); // Set Multiplex Ratio

    OLED_SH1106_Write(true, 0x3F); // 64 COM lines

    OLED_SH1106_Write(true, 0xD3); // Set display offset
    OLED_SH1106_Write(true, 0x00); // 0 offset

    OLED_SH1106_Write(true, 0x40); // Set first line as the start line of the display

    OLED_SH1106_Write(true, 0x8D); // Charge pump
    OLED_SH1106_Write(true, 0x14); // Enable charge dump during display on

    OLED_SH1106_Write(true, 0x20); // Set memory addressing mode
    OLED_SH1106_Write(true, 0x00); // Horizontal addressing mode

    OLED_SH1106_Write(true, 0xA1); // Set segment remap with column address 127 mapped to segment 0

    OLED_SH1106_Write(true, 0xC8); // Set com output scan direction, scan from com63 to com 0

    OLED_SH1106_Write(true, 0xDA); // Set com pins hardware configuration
    OLED_SH1106_Write(true, 0x12); // Alternative com pin configuration, disable com left/right remap

    OLED_SH1106_Write(true, 0x81); // Set contrast control
    OLED_SH1106_Write(true, 0x80); // Set Contrast to 128

    OLED_SH1106_Write(true, 0xD9); // Set pre-charge period
    OLED_SH1106_Write(true, 0xF1); // Phase 1 period of 15 DCLK, Phase 2 period of 1 DCLK

    OLED_SH1106_Write(true, 0xDB); // Set Vcomh deselect level
    OLED_SH1106_Write(true, 0x20); // Vcomh deselect level ~ 0.77 Vcc

    OLED_SH1106_Write(true, 0xA4); // Entire display ON, resume to RAM content display
    OLED_SH1106_Write(true, 0xA6); // Set Display in Normal Mode, 1 = ON, 0 = OFF

    OLED_SH1106_Write(true, 0x2E); // Deactivate scroll

    OLED_SH1106_Write(true, 0xAF); // Display ON in normal mode
    
    // Clear the display
    OLED_SH1106_ClearDisplay();
  }
    
  return( ret );
}

/****************************************************************************
 * Name: OLED_SH1106_DisplayDeInit
 *
 * Details : This function De-initializes the Display
 ****************************************************************************/
void OLED_SH1106_DisplayDeInit(void)
{
  OLED_SH1106_ResetDcDeInit();  //Free the Reset and DC GPIO
}
module_init(oled_init);
module_exit(oled_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("OLED driver");
MODULE_VERSION("0.1");
