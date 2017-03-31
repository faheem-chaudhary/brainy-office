#include "sys_network_thread.h"
#include "commons.h"
#include "cloud_medium1_adapter.h"
#include "libemqtt.h"
#include "libemqtt_netx_impl.h"

ULONG g_ip_address;
ULONG g_network_mask;
ULONG g_dns_ip [ 2 ];

ULONG sys_network_thread_wait = 10;

void setMacAddress ( nx_mac_address_t *p_mac_config );

#define NETWORK_STATUS_CHECK_MASK (NX_IP_LINK_ENABLED | NX_IP_ADDRESS_RESOLVED | NX_IP_INTERFACE_LINK_ENABLED | NX_IP_INITIALIZE_DONE)

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
        { // Wait for init to finish.
            ULONG status;
            applicationErrorTrap (
                    nx_ip_interface_status_check ( &g_ip, 0, NX_IP_LINK_ENABLED, &status, NX_WAIT_FOREVER ) );
        }

        { // DHCP Setup
            applicationErrorTrap ( nx_dhcp_start ( &g_dhcp_client ) );
        }

        { // Sanity check for IP Address Verification
            ULONG status;
            applicationErrorTrap ( nx_ip_status_check ( &g_ip, NX_IP_ADDRESS_RESOLVED, &status, TX_WAIT_FOREVER ) );
        }

        { // IP Address Setup
            applicationErrorTrap ( nx_ip_address_get ( &g_ip, &g_ip_address, &g_network_mask ) );
        }

        { // DNS Setup
            UINT dns_ip_str_size = sizeof ( g_dns_ip );
            applicationErrorTrap (
                    nx_dhcp_user_option_retrieve ( &g_dhcp_client, NX_DHCP_OPTION_DNS_SVR, (UCHAR *) g_dns_ip,
                                                   &dns_ip_str_size ) );
            {
                applicationErrorTrap ( nx_dns_server_add ( &g_dns_client, g_dns_ip [ 0 ] ) );

                if ( dns_ip_str_size > 4 ) // Get the secondary DNS IP, if available
                {
                    applicationErrorTrap ( nx_dns_server_add ( &g_dns_client, g_dns_ip [ 1 ] ) );
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

        while ( 1 ) // actual thread body
        {
            // TODO: Actual thread body code goes here
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

void setMacAddress ( nx_mac_address_t *p_mac_config )
{
    //  REA's Vendor MAC range: 00:30:55:xx:xx:xx
    fmi_unique_id_t id;
    g_fmi.p_api->uniqueIdGet ( &id );

    p_mac_config->nx_mac_address_h = 0x0030;
    p_mac_config->nx_mac_address_l = ( ( 0x55000000 ) | ( id.unique_id [ 0 ] & ( 0x00FFFFFF ) ) );
}

