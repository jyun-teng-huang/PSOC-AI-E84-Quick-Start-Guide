/*******************************************************************************
* File Name: main.c
*
* Description: This source file contains the main routine for non-secure
*              application in the CM33 CPU for the BLE Find Me Example
*              for ModusToolbox.
*
* Related Document: See README.md
*

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "wiced_bt_stack.h"  		/* Bluetooth stack API */
#include "wiced_memory.h"			/* WICED memory control*/
#include "cybt_platform_trace.h"	/* BT trace/debug */
#include "cyabs_rtos_impl.h"		/* RTOS abstraction */
#include <FreeRTOS.h>				/* FreeRTOS 核心定義 */
#include <task.h>					/* FreeRTOS Task API */
#include <queue.h>					/* FreeRTOS Queue API */
#include <string.h>					/* C 標準字串/記憶體函式 */
#include "stdio.h"					/* printf 等標準輸出入函式 */
#include "cycfg_gatt_db.h"			/* GATT database 自動產生*/
#include "cycfg_bt_settings.h"		/* BT stack 自動產生 */
#include "cycfg_gap.h"				/* GAP/advertising 自動產生 */
#include "wiced_bt_dev.h"			/* BT device API */
#include "cybsp_bt_config.h"		/* BSP BT 硬體設定 */
#include "retarget_io_init.h"		/* UART printf 初始化 */
#include "app_bt_utils.h"			/* 範例BT工具函式*/
#include "cy_time.h"				/* 時間/RTC/CLIB support 相關*/

/*******************************************************************************
* Macros
*******************************************************************************/
/* IAS Alert Levels */
#define IAS_ALERT_LEVEL_LOW                  (0U)
#define IAS_ALERT_LEVEL_MID                  (1U)
#define IAS_ALERT_LEVEL_HIGH                 (2U)

/* App boot address for CM55 project */
#define CM55_APP_BOOT_ADDR                   (CYMEM_CM33_0_m55_nvm_START + \
                                              CYBSP_MCUBOOT_HEADER_SIZE)

/* The timeout value in microseconds used to wait for CM55 core to be booted */
#define CM55_BOOT_WAIT_TIME_USEC             (10U)
#define SCAN_RESPONSE_NO_OF_ELEMENTS         (1U)

/* Enabling or disabling a MCWDT requires a wait time of upto 2 CLK_LF cycles
 * to come into effect. This wait time value will depend on the actual CLK_LF
 * frequency set by the BSP
 */
#define LPTIMER_0_WAIT_TIME_USEC             (62U)

/* Define the LPTimer interrupt priority number. '1' implies highest priority */
#define APP_LPTIMER_INTERRUPT_PRIORITY       (1U)
#define CONNECTION_ID_INIT_VALUE             (0U)
#define IAS_ALERT_LEVEL_INDEX                (0U)
#define DUTY_CYCLE_ZERO                      (0U)
#define DUTY_CYCLE_FIFTY                     (5000U)
#define DUTY_CYCLE_HUNDRED                   (10000U)
#define EXT_ATTR_INIT_VAL                    (0U)
#define CONN_ID_INIT_VAL                     (0U)
#define HANDLE_INIT_VAL                      (0U)
#define LENGTH_INIT_VAL                      (0U)
#define NOT_FILLED                           (0U)
#define BDA_INIT_VAL                         (0U)

/*******************************************************************************
* Variable Definitions
*******************************************************************************/
/* RTC HAL object */
mtb_hal_rtc_t rtc_obj;

/* This enumeration combines the advertising, connection states from two different
 * callbacks to maintain the status in a single state variable
 */
typedef enum
{
    APP_BT_ADV_OFF_CONN_OFF,
    APP_BT_ADV_ON_CONN_OFF,
    APP_BT_ADV_OFF_CONN_ON
} app_bt_adv_conn_mode_t;

static uint16_t                  bt_connection_id = CONNECTION_ID_INIT_VALUE;
static app_bt_adv_conn_mode_t    app_bt_adv_conn_state = APP_BT_ADV_OFF_CONN_OFF;

/* LPTimer HAL object */
static mtb_hal_lptimer_t lptimer_obj;

#if (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP)

/* PWM_1 Context for SysPm Callback */
mtb_syspm_tcpwm_deepsleep_context_t PWM1dscontext =
{
    .channelNum = CYBSP_PWM_LED_CTRL_1_NUM,
};

/* PWM_2 Context for SysPm Callback */
mtb_syspm_tcpwm_deepsleep_context_t PWM2dscontext =
{
    .channelNum = CYBSP_PWM_LED_CTRL_2_NUM,
};

/* SysPm callback parameter structure for PWM_1 */
static cy_stc_syspm_callback_params_t PWM1DSParams =
{
        .context   = &PWM1dscontext,
        .base      = CYBSP_PWM_LED_CTRL_1_HW
};

/* SysPm callback parameter structure for PWM_2 */
static cy_stc_syspm_callback_params_t PWM2DSParams =
{
        .context   = &PWM2dscontext,
        .base      = CYBSP_PWM_LED_CTRL_2_HW
};

/* SysPm callback structure for PWM_1 */
static cy_stc_syspm_callback_t PWM1DeepSleepCallbackHandler =
{
    .callback           = &mtb_syspm_tcpwm_deepsleep_callback,
    .skipMode           = CY_SYSPM_SKIP_CHECK_FAIL | CY_SYSPM_SKIP_CHECK_READY,
    .type               = CY_SYSPM_DEEPSLEEP,
    .callbackParams     = &PWM1DSParams,
    .prevItm            = NULL,
    .nextItm            = NULL,
    .order              = SYSPM_CALLBACK_ORDER
};

/* SysPm callback structure for PWM_2 */
static cy_stc_syspm_callback_t PWM2DeepSleepCallbackHandler =
{
    .callback           = &mtb_syspm_tcpwm_deepsleep_callback,
    .skipMode           = CY_SYSPM_SKIP_CHECK_FAIL | CY_SYSPM_SKIP_CHECK_READY,
    .type               = CY_SYSPM_DEEPSLEEP,
    .callbackParams     = &PWM2DSParams,
    .prevItm            = NULL,
    .nextItm            = NULL,
    .order              = SYSPM_CALLBACK_ORDER
};

#endif /*CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP) */


/*******************************************************************************
* Function Prototypes
********************************************************************************/
typedef void (*pfn_free_buffer_t)(uint8_t *);
wiced_bt_dev_status_t wiced_bt_ble_set_raw_scan_response_data
(uint8_t num_elem, wiced_bt_ble_advert_elem_t *p_data);

/*******************************************************************************
* Function Definitions
*******************************************************************************/

/*******************************************************************************
* Function Name: ias_led_update
********************************************************************************
* Summary:
*   This function updates the IAS alert level LED state based on LE
*   advertising/connection state.
*******************************************************************************/
static void ias_led_update(void)
{
    /* Update LED based on IAS alert level only when the device is connected */
    if(APP_BT_ADV_OFF_CONN_ON == app_bt_adv_conn_state)
    {
        /* Update LED state based on IAS alert level. LED OFF for low level,
         * LED blinking for mid level, and LED ON for high level
         */
        switch(app_ias_alert_level[IAS_ALERT_LEVEL_INDEX])
        {
            case IAS_ALERT_LEVEL_LOW:
                Cy_TCPWM_PWM_SetCompare0(CYBSP_PWM_LED_CTRL_2_HW,
                        CYBSP_PWM_LED_CTRL_2_NUM,DUTY_CYCLE_ZERO);
                break;

            case IAS_ALERT_LEVEL_MID:
                Cy_TCPWM_PWM_SetCompare0(CYBSP_PWM_LED_CTRL_2_HW,
                        CYBSP_PWM_LED_CTRL_2_NUM,DUTY_CYCLE_FIFTY);
                break;

            case IAS_ALERT_LEVEL_HIGH:
                Cy_TCPWM_PWM_SetCompare0(CYBSP_PWM_LED_CTRL_2_HW,
                        CYBSP_PWM_LED_CTRL_2_NUM,DUTY_CYCLE_HUNDRED);
                break;

            default:
                /* Consider any other level as High alert level */
                Cy_TCPWM_PWM_SetCompare0(CYBSP_PWM_LED_CTRL_2_HW,
                        CYBSP_PWM_LED_CTRL_2_NUM,DUTY_CYCLE_HUNDRED);
                break;
        }
    }
    else
    {
        /* In case of disconnection, turn off the IAS LED */
        Cy_TCPWM_PWM_SetCompare0(CYBSP_PWM_LED_CTRL_2_HW,
                CYBSP_PWM_LED_CTRL_2_NUM,DUTY_CYCLE_ZERO);
    }
}

/*******************************************************************************
* Function Name: le_app_set_value
********************************************************************************
* Summary:
*   This function handles writing to the attribute handle in the GATT database
*   using the data passed from the BT stack. The value to write is stored in a
*   buffer whose starting address is passed as one of the function parameters
*
* Parameters:
*  attr_handle - GATT attribute handle
*  p_val       - Pointer to LE GATT write request value
*  len         - length of GATT write request
*
* Return:
*   wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e
*                           in wiced_bt_gatt.h
*
*********************************************************************************/
static wiced_bt_gatt_status_t le_app_set_value(uint16_t attr_handle,
                                                uint8_t *p_val,
                                                uint16_t len)
{
    wiced_bool_t isHandleInTable = WICED_FALSE;
    wiced_bool_t validLen = WICED_FALSE;
    wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_INVALID_HANDLE;

    /* Check for a matching handle entry */
    for (int i = EXT_ATTR_INIT_VAL; i < app_gatt_db_ext_attr_tbl_size; i++)
    {
        if (app_gatt_db_ext_attr_tbl[i].handle == attr_handle)
        {
            /* Detected a matching handle in external lookup table */
            isHandleInTable = WICED_TRUE;

            /* Check if the buffer has space to store the data */
            validLen = (app_gatt_db_ext_attr_tbl[i].max_len >= len);
            if (validLen)
            {
                /* Value fits within the supplied buffer; copy over the value */
                app_gatt_db_ext_attr_tbl[i].cur_len = len;
                memcpy(app_gatt_db_ext_attr_tbl[i].p_data, p_val, len);
                gatt_status = WICED_BT_GATT_SUCCESS;

                /* Add code for any action required when this attribute is written.
                 * In this case, we update the IAS led based on the IAS alert
                 * level characteristic value
                 */

                switch ( attr_handle )
                {
                    case HDLC_IAS_ALERT_LEVEL_VALUE:
                        printf("Alert Level = %d\n",
                                app_ias_alert_level[IAS_ALERT_LEVEL_INDEX]);
                        ias_led_update();
                        break;

                    /* The application is not going to change its GATT DB,
                     * So this case is not handled
                     */
                    case HDLD_GATT_SERVICE_CHANGED_CLIENT_CHAR_CONFIG:
                        gatt_status = WICED_BT_GATT_SUCCESS;
                        break;
                }
            }
            else
            {
                /* Value to write does not meet size constraints */
                gatt_status = WICED_BT_GATT_INVALID_ATTR_LEN;
            }

            break;
        }
    }
    if (!isHandleInTable)
    {
        switch ( attr_handle )
        {
            default:
                /* The write operation was not performed for the indicated handle */
                printf("Write Request to Invalid Handle: 0x%x\n", attr_handle);
                gatt_status = WICED_BT_GATT_WRITE_NOT_PERMIT;
                break;
        }
    }

    return gatt_status;
}

/*******************************************************************************
* Function Name: adv_led_update
********************************************************************************
* Summary:
*  This function updates the advertising LED state based on LE advertising/
*  connection state.
*******************************************************************************/
void adv_led_update(void)
{
    /* Update LED state based on LE advertising/connection state.
     * LED OFF for no advertisement/connection, LED blinking for advertisement
     * state, and LED ON for connected state
     */
    switch(app_bt_adv_conn_state)
    {
        case APP_BT_ADV_OFF_CONN_OFF:
            Cy_TCPWM_PWM_SetCompare0(CYBSP_PWM_LED_CTRL_1_HW,
                    CYBSP_PWM_LED_CTRL_1_NUM,DUTY_CYCLE_ZERO);
            break;

        case APP_BT_ADV_ON_CONN_OFF:
            Cy_TCPWM_PWM_SetCompare0(CYBSP_PWM_LED_CTRL_1_HW,
                    CYBSP_PWM_LED_CTRL_1_NUM,DUTY_CYCLE_FIFTY);
            break;

        case APP_BT_ADV_OFF_CONN_ON:
            Cy_TCPWM_PWM_SetCompare0(CYBSP_PWM_LED_CTRL_1_HW,
                    CYBSP_PWM_LED_CTRL_1_NUM,DUTY_CYCLE_HUNDRED);
            break;

        default:
            /* LED OFF for unexpected states */
            Cy_TCPWM_PWM_SetCompare0(CYBSP_PWM_LED_CTRL_1_HW,
                    CYBSP_PWM_LED_CTRL_1_NUM,DUTY_CYCLE_ZERO);
            break;
    }
}

/*******************************************************************************
* Function Name: le_app_write_handler
********************************************************************************
* Summary:
*   This function handles Write Requests received from the client device
*
* Parameters:
*  conn_id          Connection ID
*  opcode           LE GATT request type opcode
*  p_write_req      Pointer to LE GATT write request
*  len_req          Length of data requested
*  *p_error_handle  Pointer to error handle
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e
*  in wiced_bt_gatt.h
*
********************************************************************************/
static wiced_bt_gatt_status_t le_app_write_handler(uint16_t conn_id,
        wiced_bt_gatt_opcode_t opcode, wiced_bt_gatt_write_req_t *p_write_req,
        uint16_t len_req, uint16_t *p_error_handle)
{
    wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_INVALID_HANDLE;
    *p_error_handle = p_write_req->handle;

    /* Attempt to perform the Write Request */
    gatt_status = le_app_set_value(p_write_req->handle, p_write_req->p_val,
                               p_write_req->val_len);
    if( WICED_BT_GATT_SUCCESS != gatt_status )
    {
        printf("WARNING: GATT set attr status 0x%x\n", gatt_status);
    }
    else
    {
        if(GATT_REQ_WRITE == opcode)
        {
            wiced_bt_gatt_server_send_write_rsp(conn_id, opcode,
                    p_write_req->handle);
        }
    }

    return (gatt_status);
}

/*******************************************************************************
* Function Name : le_app_find_by_handle
* ******************************************************************************
* Summary :
*  Find attribute description by handle
*
* Parameter:
*  handle - handle to look up
*
* Return:
*  gatt_db_lookup_table_t - pointer containing handle data
*
*******************************************************************************/
static gatt_db_lookup_table_t  *le_app_find_by_handle(uint16_t handle)
{
    int i;
    for (i = EXT_ATTR_INIT_VAL; i < app_gatt_db_ext_attr_tbl_size; i++)
    {
        if (handle == app_gatt_db_ext_attr_tbl[i].handle )
        {
            break;
        }
    }

    return (&app_gatt_db_ext_attr_tbl[i]);
}

/*******************************************************************************
* Function Name: le_app_read_handler
********************************************************************************
* Summary:
*   This function handles Read Requests received from the client device
*
* Parameters:
*  conn_id          Connection ID
*  opcode           LE GATT request type opcode
*  p_read_req       Pointer to read request containing the handle to read
*  len_req          Length of data requested
*  *p_error_handle  Pointer to error handle
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e
*  in wiced_bt_gatt.h
*
*******************************************************************************/
static wiced_bt_gatt_status_t le_app_read_handler( uint16_t conn_id,
             wiced_bt_gatt_opcode_t opcode, wiced_bt_gatt_read_t *p_read_req,
             uint16_t len_req, uint16_t *p_error_handle)
{
    gatt_db_lookup_table_t *puAttribute;
    int  attr_len_to_copy;
    uint8_t *from;
    int to_send;
    wiced_bt_gatt_status_t status = WICED_BT_GATT_SUCCESS;

    *p_error_handle = p_read_req->handle;
    puAttribute = le_app_find_by_handle(p_read_req->handle);
    if ( NULL == puAttribute )
    {
        status = WICED_BT_GATT_INVALID_HANDLE;
    }
    else
    {
        attr_len_to_copy = puAttribute->cur_len;
        if (p_read_req->offset >= puAttribute->cur_len)
        {
            status = WICED_BT_GATT_INVALID_OFFSET;
        }
        else
        {
            to_send = MIN(len_req, attr_len_to_copy - p_read_req->offset);
            from = ((uint8_t *)puAttribute->p_data) + p_read_req->offset;
            status = wiced_bt_gatt_server_send_read_handle_rsp(conn_id, opcode,
                    to_send, from, NULL);
        }
    }

    return status;
}

/*******************************************************************************
* Function Name: le_app_connect_handler
********************************************************************************
* Summary:
*  This callback function handles connection status changes.
*
* Parameters:
*  wiced_bt_gatt_connection_status_t *p_conn_status  : Pointer to data that has
*                                                       connection details
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e
*                          in wiced_bt_gatt.h
*
*******************************************************************************/
static wiced_bt_gatt_status_t le_app_connect_handler
             (wiced_bt_gatt_connection_status_t *p_conn_status)
{
    wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_SUCCESS ;
    if ( NULL != p_conn_status )
    {
        if ( p_conn_status->connected )
        {
            /* Device has connected */
            printf("Connected : BDA " );
            print_bd_address(p_conn_status->bd_addr);
            printf("Connection ID '%d' \n", p_conn_status->conn_id );

            /* Store the connection ID */
            bt_connection_id = p_conn_status->conn_id;

            /* Update the adv/conn state */
            app_bt_adv_conn_state = APP_BT_ADV_OFF_CONN_ON;
        }
        else
        {
            /* Device has disconnected */
            printf("Disconnected : BDA " );
            print_bd_address(p_conn_status->bd_addr);
            printf("Connection ID '%d', Reason '%s'\n", p_conn_status->conn_id,
                    get_bt_gatt_disconn_reason_name(p_conn_status->reason) );

            /* Set the connection id to zero to indicate disconnected state */
            bt_connection_id = CONN_ID_INIT_VAL;

            /* Restart the advertisements */
            wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH,
                    BLE_ADDR_PUBLIC, NULL);

            /* Update the adv/conn state */
            app_bt_adv_conn_state = APP_BT_ADV_ON_CONN_OFF;

            /* Turn Off the IAS LED on a disconnection */
            ias_led_update();
        }

        /* Update Advertisement LED to reflect the updated state */
        adv_led_update();
        gatt_status = WICED_BT_GATT_ERROR;
    }

    return gatt_status;
}

/*******************************************************************************
* Function Name: app_free_buffer
********************************************************************************
* Summary:
*  This function frees up the memory buffer.
*
* Parameters:
*  uint8_t *p_data: Pointer to the buffer to be free
*
* Return:
*  None
*******************************************************************************/
static void app_free_buffer(uint8_t *p_buf)
{
    vPortFree(p_buf);
}

/*******************************************************************************
* Function Name: app_alloc_buffer
********************************************************************************
* Summary:
*  This function allocates a memory buffer.
*
* Parameters:
*  int len: Length to allocate
*
* Return:
*  None
*******************************************************************************/
static void* app_alloc_buffer(int len)
{
    return pvPortMalloc(len);
}

/*******************************************************************************
* Function Name: app_bt_gatt_req_read_by_type_handler
********************************************************************************
* Function Description:
*  Process read-by-type request from peer device
*
* Parameters:
*  conn_id          Connection ID
*  opcode           LE GATT request type opcode
*  p_read_req       Pointer to read request containing the handle to read
*  len_requested    Length of data requested
*  *p_error_handle  Pointer to error handle
*
* Return:
*  wiced_bt_gatt_status_t  LE GATT status
*
*******************************************************************************/
static wiced_bt_gatt_status_t
app_bt_gatt_req_read_by_type_handler(uint16_t conn_id,
                                     wiced_bt_gatt_opcode_t opcode,
                                     wiced_bt_gatt_read_by_type_t *p_read_req,
                                     uint16_t len_requested,
                                     uint16_t *p_error_handle)
{
    gatt_db_lookup_table_t *puAttribute;
    uint16_t last_handle = HANDLE_INIT_VAL;
    uint16_t attr_handle = p_read_req->s_handle;
    uint8_t *p_rsp = app_alloc_buffer(len_requested);
    uint8_t pair_len = LENGTH_INIT_VAL;
    int used_len = LENGTH_INIT_VAL;
    wiced_bt_gatt_status_t status = WICED_BT_GATT_SUCCESS;
    if (NULL == p_rsp)
    {
        printf("No memory, len_requested: %d!!\r\n",len_requested);
        status = WICED_BT_GATT_INSUF_RESOURCE;
    }
    else
    {
        /* Read by type returns all attributes of the specified type,
         * between the start and end handles
         */
        while (WICED_TRUE)
        {
            *p_error_handle = attr_handle;
            last_handle = attr_handle;
            attr_handle = wiced_bt_gatt_find_handle_by_type(attr_handle,
                                                            p_read_req->e_handle,
                                                           &p_read_req->uuid);
            if (HANDLE_INIT_VAL == attr_handle )
                break;
            if ( NULL == (puAttribute = le_app_find_by_handle(attr_handle)))
            {
                printf("found type but no attribute for %d \r\n",last_handle);
                status = WICED_BT_GATT_INVALID_HANDLE;
                break;
            }
            int filled = wiced_bt_gatt_put_read_by_type_rsp_in_stream(p_rsp +
                         used_len, len_requested - used_len, &pair_len,
                         attr_handle, puAttribute->cur_len, puAttribute->p_data);
            if (NOT_FILLED == filled)
            {
                break;
            }
            used_len += filled;

            /* Increment starting handle for next search to one past current */
            attr_handle++;
        }

        if (LENGTH_INIT_VAL == used_len)
        {
           printf("attr not found  start_handle: 0x%04x  end_handle: 0x%04x  "
                   "Type: 0x%04x\r\n", p_read_req->s_handle, p_read_req->e_handle,
                   p_read_req->uuid.uu.uuid16);
            status = WICED_BT_GATT_INVALID_HANDLE;
        }
        else
        {
            /* Send the response */
            status = wiced_bt_gatt_server_send_read_by_type_rsp(conn_id,
                    opcode, pair_len, used_len, p_rsp, (void *)app_free_buffer);
        }
    }
    if (p_rsp != NULL)
    {
        app_free_buffer(p_rsp);
    }

    return status;
}

/*******************************************************************************
* Function Name: le_app_server_handler
********************************************************************************
* Summary:
*   This function handles GATT server events from the BT stack.
*
* Parameters:
*  p_attr_req:      Pointer to LE GATT connection status
*  *p_error_handle  Pointer to error handle
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e
*                          in wiced_bt_gatt.h
*
********************************************************************************/
static wiced_bt_gatt_status_t le_app_server_handler
                  (wiced_bt_gatt_attribute_request_t *p_attr_req,
                   uint16_t *p_error_handle)
{
    wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_SUCCESS;
    switch ( p_attr_req->opcode )
    {
        case GATT_REQ_READ:
        case GATT_REQ_READ_BLOB:

             /* Attribute read request */
            gatt_status = le_app_read_handler(p_attr_req->conn_id,
                                              p_attr_req->opcode,
                                              &p_attr_req->data.read_req,
                                              p_attr_req->len_requested,
                                              p_error_handle);
             break;
        case GATT_REQ_WRITE:
        case GATT_CMD_WRITE:

             /* Attribute write request */
            gatt_status = le_app_write_handler(p_attr_req->conn_id,
                                               p_attr_req->opcode,
                                               &p_attr_req->data.write_req,
                                               p_attr_req->len_requested,
                                               p_error_handle );

             break;
        case GATT_REQ_MTU:
            gatt_status = wiced_bt_gatt_server_send_mtu_rsp(p_attr_req->conn_id,
                                                       p_attr_req->data.remote_mtu,
                                                       CY_BT_RX_PDU_SIZE);
             break;
        case GATT_HANDLE_VALUE_NOTIF:
                    printf("Notfication send complete\n");
             break;
        case GATT_REQ_READ_BY_TYPE:
            gatt_status = app_bt_gatt_req_read_by_type_handler(p_attr_req->conn_id,
                            p_attr_req->opcode, &p_attr_req->data.read_by_type,
                            p_attr_req->len_requested, p_error_handle);
             break;

        default:
                printf("ERROR: Unhandled GATT Connection Request case: %d\n",
                        p_attr_req->opcode);
                gatt_status = WICED_BT_GATT_ERROR;
                break;
    }

    return gatt_status;
}

/******************************************************************************
* Function Name: le_app_gatt_event_callback
*******************************************************************************
* Summary:
*   This function handles GATT events from the BT stack.
*
* Parameters:
*   wiced_bt_gatt_evt_t event                : LE GATT event code of one byte
*                                              length
*   wiced_bt_gatt_event_data_t *p_event_data : Pointer to LE GATT event structures
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e
*                          in wiced_bt_gatt.h
*
*******************************************************************************/
static wiced_bt_gatt_status_t le_app_gatt_event_callback
            (wiced_bt_gatt_evt_t event, wiced_bt_gatt_event_data_t *p_event_data)
{
    wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_SUCCESS;
    wiced_bt_gatt_attribute_request_t *p_attr_req =
            &p_event_data->attribute_request;
    uint16_t error_handle = HANDLE_INIT_VAL;

    /* Call the appropriate callback function based on the GATT event type,
     * and pass the relevant event
     * parameters to the callback function
     */
    switch ( event )
    {
        case GATT_CONNECTION_STATUS_EVT:
            gatt_status = le_app_connect_handler(&p_event_data->connection_status);
            break;

        case GATT_ATTRIBUTE_REQUEST_EVT:
            gatt_status = le_app_server_handler(p_attr_req,
                                                &error_handle );
            if(WICED_BT_GATT_SUCCESS != gatt_status)
            {
               wiced_bt_gatt_server_send_error_rsp(p_attr_req->conn_id,
                    p_attr_req->opcode, error_handle, gatt_status);
            }
            break;

        case GATT_GET_RESPONSE_BUFFER_EVT:
            /* GATT buffer request, typically sized to max of bearer mtu - 1 */
            p_event_data->buffer_request.buffer.p_app_rsp_buffer =
            app_alloc_buffer(p_event_data->buffer_request.len_requested);
            p_event_data->buffer_request.buffer.p_app_ctxt =
                    (void *)app_free_buffer;
            gatt_status = WICED_BT_GATT_SUCCESS;
            break;

        case GATT_APP_BUFFER_TRANSMITTED_EVT:
            /* GATT buffer transmitted event,
             * check \ref wiced_bt_gatt_buffer_transmitted_t
             */
            {
            pfn_free_buffer_t pfn_free = (pfn_free_buffer_t)p_event_data
                    ->buffer_xmitted.p_app_ctxt;

            /* If the buffer is dynamic, the context will point to a
             * function to free it.
             */
            if (pfn_free)
            {
                pfn_free(p_event_data->buffer_xmitted.p_app_data);
            }
            gatt_status = WICED_BT_GATT_SUCCESS;
            }
            break;
        default:
            gatt_status = WICED_BT_GATT_ERROR;
               break;
    }

    return gatt_status;
}

/*******************************************************************************
* Function Name: le_app_init
********************************************************************************
* Summary:
*  This function handles application level initialization tasks and is called
*  from the BT management callback once the LE stack enabled event
*  (BTM_ENABLED_EVT) is triggered.
*  This function is executed in the BTM_ENABLED_EVT management callback.
*******************************************************************************/

static void le_app_init(void)
{

#if (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP)

    /* SysPm callback registration for PWMs */
    Cy_SysPm_RegisterCallback(&PWM1DeepSleepCallbackHandler);
    Cy_SysPm_RegisterCallback(&PWM2DeepSleepCallbackHandler);

#endif

    cy_rslt_t cy_result = CY_RSLT_SUCCESS;
    cy_rslt_t result = CY_TCPWM_SUCCESS;
    wiced_result_t wiced_result = WICED_BT_SUCCESS;
    wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_SUCCESS;
    printf("\n***********************************************\n");
    printf("**Discover device with \"Find Me Target\" name*\n");
    printf("***********************************************\n\n");

    /* Initialize the PWM used for USER LEDs */

    /* LED 1 */
    /* Initialize the TCPWM block */
    result = Cy_TCPWM_PWM_Init(CYBSP_PWM_LED_CTRL_2_HW,
            CYBSP_PWM_LED_CTRL_2_NUM,
            &CYBSP_PWM_LED_CTRL_2_config);

    /* PWM init failed. Stop program execution */
    if(CY_TCPWM_SUCCESS != result)
    {
        printf("Failed to initialize PWM for LED1.\r\n");
        handle_app_error();
    }

    /* Enable the TCPWM block */
    Cy_TCPWM_PWM_Enable(CYBSP_PWM_LED_CTRL_2_HW,
            CYBSP_PWM_LED_CTRL_2_NUM);

    /* Start the PWM */
    Cy_TCPWM_TriggerStart_Single(CYBSP_PWM_LED_CTRL_2_HW,
            CYBSP_PWM_LED_CTRL_2_NUM);

    /* LED 2 */
    /* Initialize the TCPWM block */
    result = Cy_TCPWM_PWM_Init(CYBSP_PWM_LED_CTRL_1_HW,
            CYBSP_PWM_LED_CTRL_1_NUM,
            &CYBSP_PWM_LED_CTRL_1_config);

    /* PWM init failed. Stop program execution */
    if(CY_TCPWM_SUCCESS != result)
    {
        printf("Failed to initialize PWM for LED2.\r\n");
        handle_app_error();
    }

    /* Enable the TCPWM block */
    Cy_TCPWM_PWM_Enable(CYBSP_PWM_LED_CTRL_1_HW,
            CYBSP_PWM_LED_CTRL_1_NUM);

    /* Start the PWM */
    Cy_TCPWM_TriggerStart_Single(CYBSP_PWM_LED_CTRL_1_HW,
            CYBSP_PWM_LED_CTRL_1_NUM);
    wiced_bt_set_pairable_mode(FALSE, FALSE);

    /* Set Advertisement Data */
    cy_result= wiced_bt_ble_set_raw_advertisement_data
            (CY_BT_ADV_PACKET_DATA_SIZE, cy_bt_adv_packet_data);
    if (WICED_SUCCESS != cy_result)
    {
        printf("Set ADV data failed\n");
    }

    /* Set raw response packet data */
    cy_result=wiced_bt_ble_set_raw_scan_response_data
            (SCAN_RESPONSE_NO_OF_ELEMENTS,cy_bt_scan_resp_packet_data);
    if (WICED_SUCCESS != cy_result)
    {
        printf("Set response data failed\n");
    }
    /* Register with BT stack to receive GATT callback */
    gatt_status = wiced_bt_gatt_register(le_app_gatt_event_callback);
    printf("GATT event Handler registration status: %s \n",
            get_bt_gatt_status_name(gatt_status));

    /* Initialize GATT Database */
    gatt_status = wiced_bt_gatt_db_init(gatt_database,
            gatt_database_len, NULL);
    printf("GATT database initialization status: %s \n",
            get_bt_gatt_status_name(gatt_status));

    /* Start Undirected LE Advertisements on device startup. */
    wiced_result = wiced_bt_start_advertisements
            (BTM_BLE_ADVERT_UNDIRECTED_HIGH, BLE_ADDR_PUBLIC, NULL);

    /* Failed to start advertisement. Stop program execution */
    if (WICED_BT_SUCCESS != wiced_result)
    {
        printf("failed to start advertisement! \n");
        handle_app_error();
    }
}

/*******************************************************************************
* Function Name: app_bt_management_callback
********************************************************************************
* Summary:
*   This is a Bluetooth stack event handler function to receive management
*   events from the LE stack and process as per the application.
*
* Parameters:
*   wiced_bt_management_evt_t event : LE event code of one byte length
*   wiced_bt_management_evt_data_t *p_event_data: Pointer to LE management
*                                                 event structures
*
* Return:
*  wiced_result_t: Error code from WICED_RESULT_LIST or BT_RESULT_LIST
*
*******************************************************************************/
wiced_result_t app_bt_management_callback(wiced_bt_management_evt_t event,
                                wiced_bt_management_evt_data_t *p_event_data)
{
    wiced_result_t wiced_result = WICED_BT_SUCCESS;
    wiced_bt_device_address_t bda = {BDA_INIT_VAL};
    wiced_bt_ble_advert_mode_t *p_adv_mode = NULL;
    switch (event)
    {
        case BTM_ENABLED_EVT:		

            /* Bluetooth Controller and Host Stack Enabled */
            if (WICED_BT_SUCCESS == p_event_data->enabled.status)
            {
                wiced_bt_set_local_bdaddr((uint8_t *)cy_bt_device_address,
                        BLE_ADDR_PUBLIC);
                wiced_bt_dev_read_local_addr(bda);
                printf("Local Bluetooth Address: ");
                print_bd_address(bda);

                /* Perform application-specific initialization */
                le_app_init();
            }
            else
            {
                printf( "Bluetooth Disabled \n" );
            }

            break;

        case BTM_BLE_ADVERT_STATE_CHANGED_EVT:

            /* Advertisement State Changed */
            p_adv_mode = &p_event_data->ble_advert_state_changed;
            printf("Advertisement State Change: %s\n",
                    get_bt_advert_mode_name(*p_adv_mode));
            if (BTM_BLE_ADVERT_OFF == *p_adv_mode)
            {
                /* Advertisement Stopped */
                printf("Advertisement stopped\n");

                /* Check connection status after advertisement stops */
                if(CONNECTION_ID_INIT_VALUE == bt_connection_id)
                {
                    app_bt_adv_conn_state = APP_BT_ADV_OFF_CONN_OFF;
                }
                else
                {
                    app_bt_adv_conn_state = APP_BT_ADV_OFF_CONN_ON;
                }
            }
            else
            {
                /* Advertisement Started */
                printf("Advertisement started\n");
                app_bt_adv_conn_state = APP_BT_ADV_ON_CONN_OFF;
            }

            /* Update Advertisement LED to reflect the updated state */
            adv_led_update();
            break;

        case BTM_BLE_CONNECTION_PARAM_UPDATE:
			/* BLE 連線參數設定 */
            printf("Connection parameter update status:%d, Connection Interval: "
                    "%d, Connection Latency: %d, Connection Timeout: %d\n",
                    p_event_data->ble_connection_param_update.status,
                    p_event_data->ble_connection_param_update.conn_interval,
                    p_event_data->ble_connection_param_update.conn_latency,
                    p_event_data->ble_connection_param_update.supervision_timeout);
            break;

        default:
            printf("Unhandled Bluetooth Management Event: 0x%x %s\n",
                    event, get_btm_event_name(event));
            break;
    }
    
    return wiced_result;
}

/*******************************************************************************
* Function Name: lptimer_interrupt_handler
********************************************************************************
* Summary:
* Interrupt handler function for LPTimer instance.
*******************************************************************************/
static void lptimer_interrupt_handler(void)
{
    mtb_hal_lptimer_process_interrupt(&lptimer_obj);
}

/*******************************************************************************
* Function Name: setup_tickless_idle_timer
********************************************************************************
* Summary:
*  1. This function first configures and initializes an interrupt for LPTimer.
*  2. Then it initializes the LPTimer HAL object to be used in the RTOS
*     tickless idle mode implementation to allow the device enter deep sleep
*     when idle task runs. LPTIMER_0 instance is configured for CM33 CPU.
*  3. It then passes the LPTimer object to abstraction RTOS library that
*     implements tickless idle mode.
*******************************************************************************/
static void setup_tickless_idle_timer(void)
{
    /* Interrupt configuration structure for LPTimer */
    cy_stc_sysint_t lptimer_intr_cfg =
    {
        .intrSrc = CYBSP_CM33_LPTIMER_0_IRQ,
        .intrPriority = APP_LPTIMER_INTERRUPT_PRIORITY
    };

    /* Initialize the LPTimer interrupt and specify the interrupt handler. */
    cy_en_sysint_status_t interrupt_init_status =
         Cy_SysInt_Init(&lptimer_intr_cfg,
         lptimer_interrupt_handler);

    /* LPTimer interrupt initialization failed. Stop program execution. */
    if(CY_SYSINT_SUCCESS != interrupt_init_status)
    {
        handle_app_error();
    }

    /* Enable NVIC interrupt. */
    NVIC_EnableIRQ(lptimer_intr_cfg.intrSrc);

    /* Initialize the MCWDT block */
    cy_en_mcwdt_status_t mcwdt_init_status =
         Cy_MCWDT_Init(CYBSP_CM33_LPTIMER_0_HW,
                       &CYBSP_CM33_LPTIMER_0_config);

    /* MCWDT initialization failed. Stop program execution. */
    if(CY_MCWDT_SUCCESS != mcwdt_init_status)
    {
        handle_app_error();
    }

    /* Enable MCWDT instance */
    Cy_MCWDT_Enable(CYBSP_CM33_LPTIMER_0_HW,
                    CY_MCWDT_CTR_Msk,
                    LPTIMER_0_WAIT_TIME_USEC);

    /* Setup LPTimer using the HAL object and desired configuration as defined
     * in the device configurator.
     */
    cy_rslt_t result = mtb_hal_lptimer_setup(&lptimer_obj,
                                             &CYBSP_CM33_LPTIMER_0_hal_config);

    /* LPTimer setup failed. Stop program execution. */
    if(CY_RSLT_SUCCESS != result)
    {
        handle_app_error();
    }

   /* Pass the LPTimer object to abstraction RTOS library that implements
    * tickless idle mode
    */
    cyabs_rtos_set_lptimer(&lptimer_obj);
}

/*******************************************************************************
* Function Name: setup_clib_support
********************************************************************************
* Summary:
*    1. This function configures and initializes the Real-Time Clock (RTC).
*    2. It then initializes the RTC HAL object to enable CLIB support library
*       to work with the provided Real-Time Clock (RTC) module.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void setup_clib_support(void)
{
    /* RTC Initialization */
    Cy_RTC_Init(&CYBSP_RTC_config);
    Cy_RTC_SetDateAndTime(&CYBSP_RTC_config);

    /* Initialize the ModusToolbox CLIB support library */
    mtb_clib_support_init(&rtc_obj);
}

/*******************************************************************************
* Function Name : main
********************************************************************************
* Summary:
*  Entry point to the application. Set device configuration and start
*  BT stack initialization.  The actual application initialization will happen
*  when stack reports that BT device is ready.
*******************************************************************************/
int main(void)
{
    cy_rslt_t cy_result;
    wiced_result_t wiced_result;

    /* Initialize the board support package */
    cy_result = cybsp_init();							/* 初始化板子硬體與BSP */
    if (CY_RSLT_SUCCESS != cy_result)					
    {
        handle_app_error();								/* 錯誤處理流程 */
    }

    /* Initialize retarget-io middleware */
    init_retarget_io();									/* 初始化 UART printf */

    /* Setup CLIB support library. */
    setup_clib_support();

    /* Setup the LPTimer instance for CM33 CPU. */
    setup_tickless_idle_timer();

    /* Clear UART Terminal Window*/
    printf("\x1b[2J\x1b[;H");
    /* Display header information*/
    printf("************* PSOC Edge MCU : Bluetooth LE Findme "
            "Profile **************\n\n");

    /* Enable global interrupts */
    __enable_irq();
    
    /* Initialize BT Stack */
    wiced_result = wiced_bt_stack_init (app_bt_management_callback,
               &cy_bt_cfg_settings);											/* 藍芽設備名稱 */

    /* Check if stack initialization was successful */
    if( WICED_BT_SUCCESS == wiced_result)
    {
        printf("Bluetooth Stack Initialization Successful \n");
        printf("Wait few seconds... Patch is getting downloaded. \n");
    }
    else
    {
        printf("Bluetooth Stack Initialization failed!! \n");
        handle_app_error();
    }

   /* Enable CM55. CM55_APP_BOOT_ADDR must be updated if CM55
    * memory layout is changed.
    */
    Cy_SysEnableCM55( MXCM55,CM55_APP_BOOT_ADDR,
           CM55_BOOT_WAIT_TIME_USEC);

    /* Start the FreeRTOS scheduler */
    vTaskStartScheduler();

    /* Should never get here */
    handle_app_error();
}


/* END OF FILE [] */
