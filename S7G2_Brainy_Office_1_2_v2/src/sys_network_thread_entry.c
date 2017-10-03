#include "sys_network_thread.h"
#include "commons.h"
#include "ether_phy.h"

/// -------------------------------------------------------- ///
///   SECTION: Macro Definitions                             ///
#define NETWORK_STATUS_CHECK_MASK       (NX_IP_LINK_ENABLED | NX_IP_ADDRESS_RESOLVED | NX_IP_INTERFACE_LINK_ENABLED | NX_IP_INITIALIZE_DONE)
#define NETWORK_ETHERNET_PHY_CHANNEL    1

/// --  END OF: Macro Definitions -------------------------  ///

/// -------------------------------------------------------- ///
///   SECTION: Global/extern Variable Declarations           ///
///                        -- None --                        ///

/// --  END OF: Global/extern Variable Declarations -------- ///

/// -------------------------------------------------------- ///
///   SECTION: Local Type Definitions                        ///
///                        -- None --                        ///

/// --  END OF: Local Type Definitions --------------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Variable Declarations     ///
static ULONG ip_address;
static ULONG network_mask;
static ULONG dns_ip [ 2 ];
static nx_mac_address_t *p_mac_address = NULL;
static ULONG sys_network_thread_wait = 10;

/// --  END OF: Static (file scope) Variable Declarations -- ///

/// -------------------------------------------------------- ///
///   SECTION: Global Function Declarations                  ///
void sys_network_thread_entry ( void );
void setMacAddress ( nx_mac_address_t *p_mac_config );

/// --  END OF: Global Function Declarations --------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Function Declarations     ///
///                        -- None --                        ///

/// --  END OF: Static (file scope) Function Declarations -- ///

/* System Network (Ethernet) Thread entry function */
void sys_network_thread_entry ( void )
{
//    unsigned long networkStatus = 0;
    uint8_t sys_network_thread_id = getUniqueThreadId ();

    sf_message_header_t * message;
    ssp_err_t msgStatus;

    // nx_ip_link_status_change_notify_set
    // nx_ip_address_change_notify

    while ( 1 )
    {
        // Wait for physical Ehternet connector to get plugged in
        while ( Phy_GetLinkStatus ( NETWORK_ETHERNET_PHY_CHANNEL ) != R_PHY_OK )
        {
            tx_thread_sleep ( sys_network_thread_wait );
        }

        { // Wait for init to finish.
            ULONG status;
            handleError ( nx_ip_interface_status_check ( &g_ip, 0, NX_IP_LINK_ENABLED, &status, NX_WAIT_FOREVER ) );
        }

        { // DHCP Setup
            handleError ( nx_dhcp_start ( &g_dhcp_client ) );
        }

        { // Sanity check for IP Address Verification
            ULONG status;
            handleError ( nx_ip_status_check ( &g_ip, NX_IP_ADDRESS_RESOLVED, &status, TX_WAIT_FOREVER ) );
        }

        { // IP Address Setup
            handleError ( nx_ip_address_get ( &g_ip, &ip_address, &network_mask ) );
        }

        { // DNS Setup
            UINT dns_ip_str_size = sizeof ( dns_ip );
            handleError (
                    nx_dhcp_user_option_retrieve ( &g_dhcp_client, NX_DHCP_OPTION_DNS_SVR, (UCHAR *) dns_ip,
                                                   &dns_ip_str_size ) );
            {
                handleError ( nx_dns_server_add ( &g_dns_client, dns_ip [ 0 ] ) );

                if ( dns_ip_str_size > 4 ) // Get the secondary DNS IP, if available
                {
                    handleError ( nx_dns_server_add ( &g_dns_client, dns_ip [ 1 ] ) );
                }
            }
        }

        { // Test dns lookup
            ULONG server_ip_address;
            nx_dns_host_by_name_get ( &g_dns_client, (UCHAR *) "google.com", &server_ip_address, TX_WAIT_FOREVER );

//            char destinationIpAddress [ 80 ];
//            sprintf ( destinationIpAddress, "google.com = %d.%d.%d.%d", (int) ( server_ip_address >> 24 ),
//                      (int) ( server_ip_address >> 16 ) & 0xFF, (int) ( server_ip_address >> 8 ) & 0xFF,
//                      (int) ( server_ip_address ) & 0xFF );
        }

        postSystemEventMessage ( sys_network_thread_id, SF_MESSAGE_EVENT_SYSTEM_NETWORK_AVAILABLE );

        while ( Phy_GetLinkStatus ( NETWORK_ETHERNET_PHY_CHANNEL ) == R_PHY_OK ) // actual thread body
        {
//            nx_ip_interface_status_check ( &g_ip, 0, NETWORK_STATUS_CHECK_MASK, &networkStatus, 0 );
//
//            if ( ( networkStatus & NETWORK_STATUS_CHECK_MASK ) != NETWORK_STATUS_CHECK_MASK )
//            {
//                break;
//            }

            msgStatus = messageQueuePend ( &sys_network_thread_message_queue, (void **) &message,
                                           sys_network_thread_wait );

            if ( msgStatus == SSP_SUCCESS )
            {
                if ( message->event_b.class_code == SF_MESSAGE_EVENT_CLASS_SYSTEM )
                {
                    //TODO: anything related to System Message Processing goes here
                    if ( message->event_b.code == SF_MESSAGE_EVENT_SYSTEM_BUTTON_S4_PRESSED )
                    {
                        logInfo ( "Button %s is pressed", "S4" );
                    }
                    else if ( message->event_b.code == SF_MESSAGE_EVENT_SYSTEM_BUTTON_S5_PRESSED )
                    {
                        logDebug ( "Button S-5 is pressed with code: %d ... rebooting now!!!",
                                   SF_MESSAGE_EVENT_SYSTEM_BUTTON_S5_PRESSED );
                        UINT testError = 3;
                        handleError ( testError );
                    }
                }

                messageQueueReleaseBuffer ( (void **) &message );
            }
        }

        postSystemEventMessage ( sys_network_thread_id, SF_MESSAGE_EVENT_SYSTEM_NETWORK_DISCONNECTED );

        nx_dns_server_remove_all (&g_dns_client);
        nx_dhcp_stop (&g_dhcp_client);
        nx_dhcp_reinitialize ( &g_dhcp_client );
    }
}

__attribute__((no_instrument_function))
void setMacAddress ( nx_mac_address_t *p_mac_config )
{
    //  REA's Vendor MAC range: 00:30:55:xx:xx:xx
    fmi_unique_id_t id;
    g_fmi.p_api->uniqueIdGet ( &id );
    ULONG lowerHalfMac = ( ( 0x55000000 ) | ( id.unique_id [ 0 ] & ( 0x00FFFFFF ) ) );

    p_mac_config->nx_mac_address_h = 0x0030;
    p_mac_config->nx_mac_address_l = lowerHalfMac;

    p_mac_address = p_mac_config;
}

