/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for CM4 in the the Dual CPU IPC Pipes
*              Application for ModusToolbox.
*
* Related Document: See README.md
*
*******************************************************************************/

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "mtb_hx8347.h"
#include "GUI.h"
//#include "BUTTON.h"
#include "ipc_communication.h"

#include <stdio.h>

//for SD
#include "fatfs_sd.h"
#include "ff.h"
#include "diskio.h"


/*******************************************************************************
* Macros
*******************************************************************************/
/* SPI baud rate in Hz */
#define SPI_FREQ_HZ                (10000000UL)
/* Delay of 1000ms between commands */
#define CMD_TO_CMD_DELAY           (1000UL)
/* SPI transfer bits per frame */
#define BITS_PER_FRAME             (8)


cy_rslt_t result;
cyhal_spi_t mSPI;

void handle_error(uint32_t status)
{
    if (status != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
}

/****************************************************************************
* Functions Prototypes
*****************************************************************************/
void cm4_msg_callback(uint32_t *msg);
void number_screen(void);
void display_number(int digit, int color);
void draw_symbol(int digit);
void draw_triangle();

void menu_screen();
void display_a(void);
void display_b(void);
void display_c(void);
void display_d(void);
void display_e(void);
void display_f(void);

/****************************************************************************
* Global Variables
*****************************************************************************/

/* Message variables */
volatile bool msg_flag = false;
volatile uint32_t msg_value;


extern GUI_CONST_STORAGE GUI_BITMAP bma_apple;
extern GUI_CONST_STORAGE GUI_BITMAP bma;
extern GUI_CONST_STORAGE GUI_BITMAP bmball;
extern GUI_CONST_STORAGE GUI_BITMAP bmb;

int main(void)
{
    cy_rslt_t result;

    /* Init the IPC communication for CM4 */
    setup_ipc_communication_cm4();

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    handle_error(result);

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    /* Register the Message Callback */

	Cy_IPC_Pipe_RegisterCallback(CY_IPC_EP_CYPIPE_CM4_ADDR,
	                                 cm4_msg_callback,
	                                 IPC_CM0_TO_CM4_CLIENT_ID);

	result = cyhal_spi_init(&mSPI,CYBSP_SPI_MOSI,CYBSP_SPI_MISO,CYBSP_SPI_CLK,
	                                    NC,NULL,BITS_PER_FRAME,
	                                    CYHAL_SPI_MODE_11_MSB,false);
	handle_error(result);

	result = cyhal_spi_set_frequency(&mSPI, SPI_FREQ_HZ);
	handle_error(result);

	//SD card chip select pin
	cyhal_gpio_init(CYBSP_D5, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false);

	//read the size of the sd card
	uint32_t size = cardSize();
	printf("Card size: %d\r\n\n", size);

    GUI_Init();
    menu_screen();
    //cyhal_system_delay_ms(5000);
    //number_screen();

    //GUI_SetFont(GUI_FONT_32B_1);
    //GUI_SetTextAlign(GUI_TA_CENTER);
    //GUI_DispStringAt("Hello World", GUI_GetScreenSizeX()/2,GUI_GetScreenSizeY()/2 - GUI_GetFontSizeY()/2);
    //cyhal_system_delay_ms(2000);
    //start_screen();
    //menu_screen();
    //cyhal_system_delay_ms(5000);
    //GUI_Clear();

    //GUI_DrawBitmap(&bma_apple, 220, 112);
    //GUI_DrawBitmap(&bma, 0, 0);
    //cyhal_system_delay_ms(5000);
    //GUI_Clear();
    //GUI_SetBkColor(GUI_WHITE);
    //GUI_DrawBitmap(&bmball, 220, 141);
    //GUI_DrawBitmap(&bmb, 0, 0);

    int menu = 0, number = 0;
    for (;;)
    {
        //cyhal_syspm_sleep();

        /* Check if a message was received from CM0+ */
        if (msg_flag)
        {
            msg_flag = false;
            int value = (unsigned int) msg_value;
            if(value==1) {
            	if(menu==0) number = 1;
            	menu++;
            	if(menu > 25) menu = 25;
            }
            else if(value==0) {
            	if(menu==0){
            		menu = 3;
            	    number = 0;
            	}
            	menu--;
            	if(menu < 0) menu = 0;
            }
            else if(value > 1){
            	value = value/10;
            	menu = value;
            }
            /* Print random number received from CM0+ */
            //printf("Number value = %d\n\r", number);
            if(menu == 0) menu_screen();
            else if(menu == 1) number_screen();
            else if(menu == 2) number?display_number(1, 3):display_a();
            else if(menu == 3) number?display_number(2, 1):display_b();
            else if(menu == 4) number?display_number(3, 2):display_c();
            else if(menu == 5) number?display_number(4, 4):display_d();
            else if(menu == 6) number?display_number(5, 5):display_e();
            else if(menu == 7) number?display_number(6, 6):display_f();
        }


    }
}

/*******************************************************************************
* Function Name: cm4_msg_callback
********************************************************************************
* Summary:
*   Callback function to execute when receiving a message from CM0+ to CM4.
*
* Parameters:
*   msg: message received
*
*******************************************************************************/
void cm4_msg_callback(uint32_t *msg)
{
    ipc_msg_t *ipc_recv_msg;

    if (msg != NULL)
    {
        /* Cast received message to the IPC message structure */
        ipc_recv_msg = (ipc_msg_t *) msg;

        /* Extract the message value */
        msg_value = ipc_recv_msg->value;

        /* Set message flag */
        msg_flag = true;
    }

}




void display_a(void){
	GUI_Clear();
	GUI_DrawBitmap(&bma_apple, 220, 112);
	GUI_DrawBitmap(&bma, 0, 0);
}

void display_b(void){
	GUI_Clear();
	GUI_DrawBitmap(&bmball, 220, 141);
    GUI_DrawBitmap(&bmb, 0, 0);
}

void display_c(void){
	;
}

void display_d(void){
	;
}

void display_e(void){
	;
}

void display_f(void){
	;
}




/*
void start_screen(void) {
  GUI_SetBkColor(GUI_BLACK);
  GUI_Clear();
  GUI_SetColor(GUI_WHITE);
  GUI_SetFont(&GUI_Font32B_ASCII);
  //GUI_SetFont(&GUI_Font32B_1);
  GUI_DispStringHCenterAt("Kids Learning Kit", 160, 20);

  BUTTON_Handle dButton, cButton;
  GUI_SetFont(&GUI_Font8x16);
  GUI_DispStringHCenterAt("Touch a Capsense Button to Start", 160, 140);

  dButton = BUTTON_Create(20, 180, 100, 50, GUI_ID_OK, WM_CF_SHOW);
  cButton = BUTTON_Create(200, 180, 100, 50, GUI_ID_OK, WM_CF_SHOW);

  //BUTTON_SetFont(dButton, &GUI_Font20B_ASCII);
  //BUTTON_SetFont(dButton, &GUI_Font20B_1);
  BUTTON_SetFont(dButton, &GUI_Font20_ASCII);
  BUTTON_SetText(dButton, "Numbers");
  BUTTON_SetFont(cButton, &GUI_Font20_ASCII);
  BUTTON_SetText(cButton, "Alphabets");

  while (GUI_WaitKey() != GUI_ID_OK);

  //BUTTON_Delete(hButton);
  //GUI_ClearRect(0, 50, 319, 239);
  //GUI_Delay(1000);
}
*/

void menu_screen(){
	GUI_SetBkColor(GUI_BLACK);
	GUI_Clear();
	GUI_SetColor(GUI_WHITE);
	GUI_SetFont(&GUI_Font32B_ASCII);
    //GUI_SetFont(&GUI_Font32B_1);
	GUI_DispStringHCenterAt("Kids Learning Kit", 160, 20);
	GUI_SetFont(&GUI_Font8x16);
	GUI_DispStringHCenterAt("Touch a Capsense Button to Start", 160, 120);
	//GUI_SetColor(GUI_RED);
    GUI_FillCircle(70, 175, 30);
    GUI_FillCircle(240, 175, 30);
    GUI_SetColor(GUI_BLUE);
    GUI_FillCircle(70, 175, 25);
    GUI_FillCircle(240, 175, 25);
    GUI_SetColor(GUI_WHITE);
    GUI_SetFont(&GUI_Font20_ASCII);
    GUI_DispStringHCenterAt("Numbers", 70, 210);
    GUI_DispStringHCenterAt("Alphabets", 240, 210);

}


void number_screen(void){
   GUI_SetBkColor(GUI_BLACK);
   GUI_Clear();
   GUI_SetColor(GUI_WHITE);
   GUI_SetFont(&GUI_Font32B_ASCII);
   //GUI_SetFont(&GUI_Font32B_1);
   //GUI_SetFont(&GUI_Font);
   GUI_DispStringHCenterAt("Numbers", 160, 20);

   GUI_SetFont(&GUI_Font8x16);
   GUI_DispStringHCenterAt("Use Slider (>>>) to Change Color", 160, 100);
   GUI_SetFont(&GUI_Font8x16);
   GUI_DispStringHCenterAt("Touch Capsense Button to Start", 160, 120);

   GUI_FillCircle(70, 175, 30);
   GUI_FillCircle(240, 175, 30);
   GUI_SetColor(GUI_BLUE);
   GUI_FillCircle(70, 175, 25);
   GUI_FillCircle(240, 175, 25);
   GUI_SetColor(GUI_WHITE);
   GUI_SetFont(&GUI_Font20_ASCII);
   GUI_DispStringHCenterAt("Next", 70, 210);
   GUI_DispStringHCenterAt("Back", 240, 210);

}

void display_number(int digit, int color){
   GUI_SetBkColor(GUI_BLACK);
   GUI_Clear();
   switch(color){
   case 0:
	   GUI_SetColor(GUI_WHITE);
	   break;
   case 1:
   	   GUI_SetColor(GUI_RED);
   	   break;
   case 2:
   	   GUI_SetColor(GUI_BLUE);
   	   break;
   case 3:
   	   GUI_SetColor(GUI_YELLOW);
   	   break;
   case 4:
   	   GUI_SetColor(GUI_GREEN);
   	   break;
   case 5:
   	   GUI_SetColor(GUI_MAGENTA);
   	   break;
   case 6:
       GUI_SetColor(GUI_GRAY);
       break;
   case 7:
       GUI_SetColor(GUI_CYAN);
       break;
   }
   GUI_SetFont(&GUI_FontD80);
   GUI_DispDecAt(digit, 30, 30, 1);
   //GUI_SetColor(GUI_BLUE);

   draw_symbol(digit);

}

void draw_symbol(int digit){
	switch(digit+1){
	   case 0:
		   GUI_SetColor(GUI_WHITE);
		   break;
	   case 1:
	   	   GUI_SetColor(GUI_RED);
	   	   break;
	   case 2:
	   	   GUI_SetColor(GUI_BLUE);
	   	   break;
	   case 3:
	   	   GUI_SetColor(GUI_YELLOW);
	   	   break;
	   case 4:
	   	   GUI_SetColor(GUI_GREEN);
	   	   break;
	   case 5:
	   	   GUI_SetColor(GUI_MAGENTA);
	   	   break;
	   case 6:
	       GUI_SetColor(GUI_CYAN);
	       break;
	   }
	switch(digit){
	   case 0:
		   //GUI_DrawCircle(10, 50, 20);
		   break;
	   case 1:
		   GUI_FillCircle(200, 160, 45);
		   GUI_SetColor(GUI_RED);
		   GUI_FillCircle(180, 140, 5);
		   GUI_FillCircle(220, 140, 5);
		   GUI_FillEllipse(200, 160, 7, 5);
	   	   break;
	   case 2:
		   draw_triangle();
	   	   break;
	   case 3:
		   GUI_FillRect(50, 140, 110, 200);
		   GUI_FillRect(130, 140, 190, 200);
		   GUI_FillRect(210, 140, 270, 200);
	   	   break;
	   case 4:
		   GUI_FillCircle(40, 170, 35);
		   GUI_FillCircle(120, 170, 35);
		   GUI_FillCircle(200, 170, 35);
		   GUI_FillCircle(280, 170, 35);
	   	   break;
	   case 5:
		   GUI_FillRect(130, 70, 190, 130);
		   GUI_FillRect(210, 70, 270, 130);
		   GUI_FillRect(130, 150, 190, 210);
		   GUI_FillRect(210, 150, 270, 210);
		   GUI_FillRect(50, 150, 110, 210);
	   	   break;
	   }

}

void draw_triangle(){
	const GUI_POINT aPoints[] = {
			      { 40, 20}, //x1, y1
			      { 0, 20},//x1, y2
			      { 20, 0} //
			      };
	GUI_POINT aEnlargedPoints[GUI_COUNTOF(aPoints)];
	GUI_EnlargePolygon(aEnlargedPoints, aPoints, GUI_COUNTOF(aPoints), 3 * 5);
	//GUI_FillPolygon(aPoints, GUI_COUNTOF(aPoints), 140, 110);
	GUI_FillPolygon(aEnlargedPoints, GUI_COUNTOF(aPoints), 90, 160);
	GUI_FillPolygon(aEnlargedPoints, GUI_COUNTOF(aPoints), 220, 160);
}


/* [] END OF FILE */
