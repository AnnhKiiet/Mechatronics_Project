/****************************************************************************//**
*  \file       SH1106.h
*
*  \details    SH1106 related macros (SPI)
*
*  \author     EmbeTronicX
*
*  \Tested with Linux raspberrypi 5.10.27-v7l-embetronicx-custom+
*
*******************************************************************************/

#define SPI_BUS_NUM             (  0 )    // SPI 1

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