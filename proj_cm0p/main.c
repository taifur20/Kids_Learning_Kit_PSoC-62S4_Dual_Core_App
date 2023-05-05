/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for CM0+ in the the CapSense on CM0+ 
*              Application for ModusToolbox.
*
* Related Document: See README.md
*
*
*******************************************************************************/
#include "cy_pdl.h"
#include "cybsp.h"
#include "cycfg.h"
#include "cycfg_capsense.h"
#include "led.h"
#include "ipc_communication.h"


/*******************************************************************************
* Macros
*******************************************************************************/
#define CAPSENSE_INTR_PRIORITY  (3u)
#define EZI2C_INTR_PRIORITY     (2u)

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
static uint32_t initialize_capsense(void);
static void process_touch(void);
static void initialize_capsense_tuner(void);
static void capsense_isr(void);
static void capsense_callback(cy_stc_active_scan_sns_t *);
static void ezi2c_isr(void);

/*******************************************************************************
* Global Variables
*******************************************************************************/
volatile bool capsense_scan_complete = false;
cy_stc_scb_ezi2c_context_t ezi2c_context;

volatile uint8_t msg_cmd = 0;

ipc_msg_t ipc_msg = {              /* IPC structure to be sent to CM4  */
    .client_id  = IPC_CM0_TO_CM4_CLIENT_ID,
    .cpu_status = 0,
    .intr_mask   = USER_IPC_PIPE_INTR_MASK,
    .cmd        = IPC_CMD_STATUS,
    .value      = 0
};

/****************************************************************************
* Functions Prototypes
*****************************************************************************/
void cm0p_msg_callback(uint32_t *msg);


int main(void)
{
    cy_rslt_t result;

    /* Init the IPC communication for CM0+ */
    setup_ipc_communication_cm0();

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Register callback to handle messages from CM4 */

    Cy_IPC_Pipe_RegisterCallback(CY_IPC_EP_CYPIPE_CM0_ADDR,
                                     cm0p_msg_callback,
                                     IPC_CM4_TO_CM0_CLIENT_ID);

    /* Enable CM4.*/
    Cy_SysEnableCM4(CY_CORTEX_M4_APPL_ADDR);

    initialize_led();
    initialize_capsense_tuner();
    
    /* Initialize CapSense */
    result = initialize_capsense();

    if (CYRET_SUCCESS != result)
    {
        /* Halt the CPU if CapSense initialization failed */
        CY_ASSERT(0);
    }

    /* Initiate first scan */
    Cy_CapSense_ScanAllWidgets(&cy_capsense_context);


    for (;;)
    {
        if (capsense_scan_complete)
        {
            /* Process all widgets */
            Cy_CapSense_ProcessAllWidgets(&cy_capsense_context);

            /* Process touch input */
            process_touch();

            /* Establishes synchronized operation between the CapSense
             * middleware and the CapSense Tuner tool.
             */
            Cy_CapSense_RunTuner(&cy_capsense_context);

            /* Initiate next scan */
            Cy_CapSense_ScanAllWidgets(&cy_capsense_context);

            capsense_scan_complete = false;

         }


    }
}

/*******************************************************************************
* Function Name: initialize_capsense
********************************************************************************
* Summary:
*  This function initializes the CapSense and configure the CapSense
*  interrupt.
 * Parameters:
 *  none
 * 
 * Return
 *  uint32_t status - status of operation 
 *
*******************************************************************************/
static uint32_t initialize_capsense(void)
{
    uint32_t status = CYRET_SUCCESS;

    /* CapSense interrupt configuration parameters */
    static const cy_stc_sysint_t capSense_intr_config =
    {
        .intrSrc = NvicMux3_IRQn,
        .cm0pSrc = csd_interrupt_IRQn,
        .intrPriority = CAPSENSE_INTR_PRIORITY,
    };

    /* Capture the CSD HW block and initialize it to the default state. */
    status = Cy_CapSense_Init(&cy_capsense_context);
    if (CYRET_SUCCESS != status)
    {
        return status;
    }

    /* Initialize CapSense interrupt */
    Cy_SysInt_Init(&capSense_intr_config, capsense_isr);
    NVIC_ClearPendingIRQ(capSense_intr_config.intrSrc);
    NVIC_EnableIRQ(capSense_intr_config.intrSrc);

    /* Initialize the CapSense firmware modules. */
    status = Cy_CapSense_Enable(&cy_capsense_context);
    if (CYRET_SUCCESS != status)
    {
        return status;
    }

    /* Assign a callback function to indicate end of CapSense scan. */
    status = Cy_CapSense_RegisterCallback(CY_CAPSENSE_END_OF_SCAN_E,
            capsense_callback, &cy_capsense_context);
    if (CYRET_SUCCESS != status)
    {
        return status;
    }

    return status;
}

/*******************************************************************************
* Function Name: capsense_isr
********************************************************************************
* Summary:
*  Wrapper function for handling interrupts from CapSense block.
*
* Parameters:
*  none
* 
* Return
*  none
*
*******************************************************************************/
static void capsense_isr(void)
{
    Cy_CapSense_InterruptHandler(CYBSP_CSD_HW, &cy_capsense_context);
}

/*******************************************************************************
* Function Name: ezi2c_isr
********************************************************************************
* Summary:
*  Wrapper function for handling interrupts from EZI2C block.
*
*******************************************************************************/
static void ezi2c_isr(void)
{
    Cy_SCB_EZI2C_Interrupt(CYBSP_EZI2C_HW, &ezi2c_context);
}

/*******************************************************************************
* Function Name: capsense_callback()
********************************************************************************
* Summary:
*  This function sets a flag to indicate end of a CapSense scan.
*
* Parameters:
*  cy_stc_active_scan_sns_t* : pointer to active sensor details.
*
* Return
*  none
*
*******************************************************************************/
static void capsense_callback(cy_stc_active_scan_sns_t * ptrActiveScan)
{
    capsense_scan_complete = true;
}

/*******************************************************************************
* Function Name: process_touch
********************************************************************************
* Summary:
*  Gets the details of touch position detected, processes the touch input
*  and updates the LED status.
*
* Parameters:
*  none
* 
* Return
*  none
*
*******************************************************************************/
static void process_touch(void)
{
    uint32_t button0_status;
    uint32_t button1_status;
    cy_stc_capsense_touch_t *slider_touch_info;
    uint16_t slider_pos;
    uint8_t slider_touch_status;
    bool led_update_req = false;

    static uint32_t button0_status_prev;
    static uint32_t button1_status_prev;
    static uint16_t slider_pos_prev;
    static led_data_t led_data = {LED_ON, LED_MAX_BRIGHTNESS};

    /* Get button 0 status */
    button0_status = Cy_CapSense_IsSensorActive(
                                CY_CAPSENSE_BUTTON0_WDGT_ID,
                                CY_CAPSENSE_BUTTON0_SNS0_ID,
                                &cy_capsense_context);

    /* Get button 1 status */
    button1_status = Cy_CapSense_IsSensorActive(
                                CY_CAPSENSE_BUTTON1_WDGT_ID,
                                CY_CAPSENSE_BUTTON0_SNS0_ID,
                                &cy_capsense_context);
    
    /* Get slider status */
    slider_touch_info = Cy_CapSense_GetTouchInfo(
        CY_CAPSENSE_LINEARSLIDER0_WDGT_ID, &cy_capsense_context);
    slider_touch_status = slider_touch_info->numPosition;
    slider_pos = slider_touch_info->ptrPosition->x;

    /* Detect new touch on Button0 */
    if((0u != button0_status) &&
       (0u == button0_status_prev))
    {
        /* Turn USER LED ON */
        led_data.state = LED_ON;
        led_update_req = true;

        ipc_msg.value = 0;
        //send value zero to cm4 on capsense button zero press
        Cy_IPC_Pipe_SendMessage(CY_IPC_EP_CYPIPE_CM4_ADDR,
        		                CY_IPC_EP_CYPIPE_CM0_ADDR,
                            	(uint32_t *) &ipc_msg, NULL);

    }

    /* Detect new touch on Button1 */
    if((0u != button1_status) &&
       (0u == button1_status_prev))
    {
        /* Turn the USER LED off */
        led_data.state = LED_OFF;
        led_update_req = true;

        ipc_msg.value = 1;
        //send value 1 to cm4 on capsense button zero press
        Cy_IPC_Pipe_SendMessage(CY_IPC_EP_CYPIPE_CM4_ADDR,
                		        CY_IPC_EP_CYPIPE_CM0_ADDR,
                                (uint32_t *) &ipc_msg, NULL);

    }

    /* Detect the new touch on slider */
    if ((0 != slider_touch_status) &&
        (slider_pos != slider_pos_prev))
    {
        led_data.brightness = (slider_pos * 100)
                / cy_capsense_context.ptrWdConfig[CY_CAPSENSE_LINEARSLIDER0_WDGT_ID].xResolution;
        led_update_req = true;

        ipc_msg.value = slider_pos+2; //added to so that it doesn't mixed with button press
        //sends slider position on cm4
        Cy_IPC_Pipe_SendMessage(CY_IPC_EP_CYPIPE_CM4_ADDR,
                        	    CY_IPC_EP_CYPIPE_CM0_ADDR,
                                (uint32_t *) &ipc_msg, NULL);

    }

    /* Update the LED state if requested */
    if (led_update_req)
    {
        update_led_state(&led_data);
    }

    /* Update previous touch status */
    button0_status_prev = button0_status;
    button1_status_prev = button1_status;
    slider_pos_prev = slider_pos;

}

/*******************************************************************************
* Function Name: initialize_capsense_tuner
********************************************************************************
* Summary:
*  Initializes interface between Tuner GUI and PSoC 6 MCU.
*
* Parameters:
*  none
* 
* Return
*  none
*
*******************************************************************************/
static void initialize_capsense_tuner(void)
{
    /* EZI2C interrupt configuration structure */
    static const cy_stc_sysint_t ezi2c_intr_config =
    {
        .intrSrc = NvicMux2_IRQn,
        .cm0pSrc = CYBSP_EZI2C_IRQ,
        .intrPriority = EZI2C_INTR_PRIORITY,
    };

    /* Initialize EZI2C */
    Cy_SCB_EZI2C_Init(CYBSP_EZI2C_HW, &CYBSP_EZI2C_config, &ezi2c_context);

    /* Initialize and enable EZI2C interrupts */
    Cy_SysInt_Init(&ezi2c_intr_config, ezi2c_isr);
    NVIC_EnableIRQ(ezi2c_intr_config.intrSrc);

    /* Set up communication data buffer to CapSense data structure to be exposed
     * to I2C master at primary slave address request.
     */
    Cy_SCB_EZI2C_SetBuffer1(CYBSP_EZI2C_HW, (uint8 *)&cy_capsense_tuner,
        sizeof(cy_capsense_tuner), sizeof(cy_capsense_tuner),
        &ezi2c_context);

    /* Enable EZI2C block */
    Cy_SCB_EZI2C_Enable(CYBSP_EZI2C_HW);

}

/*******************************************************************************
* Function Name: cm0p_msg_callback
********************************************************************************
* Summary:
*   Callback function to execute when receiving a message from CM4 to CM0+.
*
* Parameters:
*   msg: message received
*
*******************************************************************************/
void cm0p_msg_callback(uint32_t *msg)
{
    ipc_msg_t *ipc_recv_msg;

    if (msg != NULL)
    {
        /* Cast the message received to the IPC structure */
        ipc_recv_msg = (ipc_msg_t *) msg;

        /* Extract the command to be processed in the main loop */
        msg_cmd = ipc_recv_msg->cmd;
    }
}

/* [] END OF FILE */
