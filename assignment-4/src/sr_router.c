/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"

/*---------------------------------------------------------------------
* Method: sr_init(void)
* Scope:  Global
*
* Initialize the routing subsystem
*
*---------------------------------------------------------------------*/
void sr_init(struct sr_instance *sr)
{
	/* REQUIRES */
	assert(sr);

	/* Initialize cache and cache cleanup thread */
	sr_arpcache_init(&(sr->cache));

	pthread_attr_init(&(sr->attr));
	pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
	pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
	pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
	pthread_t thread;

	pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);

	/* Add initialization code here! */

} /* -- sr_init -- */

/*---------------------------------------------------------------------
* Method: ip_black_list(struct sr_ip_hdr *iph)
* Scope:  Local
*
* This method is called each time the sr_handlepacket() is called.
* Block IP addresses in the blacklist and print the log.
* - Format : "[IP blocked] : <IP address>"
* - e.g.) [IP blocked] : 10.0.2.100
*
*---------------------------------------------------------------------*/
int ip_black_list(struct sr_ip_hdr *iph)
{
	int blk = 0;
	char ip_blacklist[20] = "10.0.2.0"; /* DO NOT MODIFY */
	char mask[20] = "255.255.255.0"; /* DO NOT MODIFY */		/* to leave only network IP (remove 4th sector nums) */ 
	/**************** fill in code here *****************/

	struct in_addr b_addr, m_addr, ip_src, ip_dst;
	
	inet_aton(ip_blacklist, &b_addr);
	inet_aton(mask, &m_addr);
	
	ip_src.s_addr = iph->ip_src;
	ip_dst.s_addr = iph->ip_dst;

	/* subnet masking */ 
	if ((ip_src.s_addr & m_addr.s_addr) == (b_addr.s_addr & m_addr.s_addr) ||
		(ip_dst.s_addr & m_addr.s_addr) == (b_addr.s_addr & m_addr.s_addr)) {
		
		blk = 1;

		if ((ip_src.s_addr & m_addr.s_addr) == (b_addr.s_addr & m_addr.s_addr))
		{
			printf("[IP blocked] : %s\n", inet_ntoa(ip_src));
		}
		else
		{
			printf("[IP blocked] : %s\n", inet_ntoa(ip_dst));
		}
	}

	/****************************************************/
	return blk;
}
/*---------------------------------------------------------------------
* Method: sr_handlepacket(uint8_t* p,char* interface)
* Scope:  Global
*
* This method is called each time the router receives a packet on the
* interface.  The packet buffer, the packet length and the receiving
* interface are passed in as parameters. The packet is complete with
* ethernet headers.
*
* Note: Both the packet buffer and the character's memory are handled
* by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
* packet instead if you intend to keep it around beyond the scope of
* the method call.
*
*---------------------------------------------------------------------*/
void sr_handlepacket(struct sr_instance *sr,
					 uint8_t *packet /* lent */,
					 unsigned int len,
					 char *interface /* lent */)
{

	/* REQUIRES */
	assert(sr);
	assert(packet);
	assert(interface);

    /*
        We provide local variables used in the reference solution.
        You can add or ignore local variables.
    */
	uint8_t *new_pck;	  /* new packet */					/* will use to calloc & free new packets to send */ 
	unsigned int new_len; /* length of new_pck */

	unsigned int len_r; /* length remaining, for validation */
	uint16_t checksum;	/* checksum, for validation */

	struct sr_ethernet_hdr *e_hdr0, *e_hdr; /* Ethernet headers */				/* given headers has 0 at the back of their names */ 
	struct sr_ip_hdr *i_hdr0, *i_hdr;		/* IP headers */
	struct sr_arp_hdr *a_hdr0, *a_hdr;		/* ARP headers */
	struct sr_icmp_hdr *ic_hdr0, *ic_hdr;	/* ICMP header */					/* *ic_hdr added (used for packets to send) */ 
	struct sr_icmp_t0_hdr *ict0_hdr;		/* ICMP type0 header */
	struct sr_icmp_t3_hdr *ict3_hdr;		/* ICMP type3 header */
	struct sr_icmp_t11_hdr *ict11_hdr;		/* ICMP type11 header */

	struct sr_if *ifc;			  /* router interface */
	uint32_t ipaddr;			  /* IP address */
	struct sr_rt *rtentry;		  /* routing table entry */
	struct sr_arpentry *arpentry; /* ARP table entry in ARP cache */
	struct sr_arpreq *arpreq;	  /* request entry in ARP cache */
	struct sr_packet *en_pck;	  /* encapsulated packet in ARP cache */

	/* validation */
	if (len < sizeof(struct sr_ethernet_hdr))
		return;
	len_r = len - sizeof(struct sr_ethernet_hdr);
	e_hdr0 = (struct sr_ethernet_hdr *)packet; /* e_hdr0 set */

	/* IP packet arrived */
	if (e_hdr0->ether_type == htons(ethertype_ip))
	{
		/* validation */
		if (len_r < sizeof(struct sr_ip_hdr))
			return;

		len_r = len_r - sizeof(struct sr_ip_hdr);
		i_hdr0 = (struct sr_ip_hdr *)(((uint8_t *)e_hdr0) + sizeof(struct sr_ethernet_hdr)); /* i_hdr0 set */

		if (i_hdr0->ip_v != 0x4)
			return;

		checksum = i_hdr0->ip_sum;
		i_hdr0->ip_sum = 0;
		if (checksum != cksum(i_hdr0, sizeof(struct sr_ip_hdr)))
			return;
		i_hdr0->ip_sum = checksum;

		/* check destination */
		for (ifc = sr->if_list; ifc != NULL; ifc = ifc->next)
		{
			if (i_hdr0->ip_dst == ifc->ip)
				break;
		}

		/* check ip black list */
		if (ip_black_list(i_hdr0))
		{
			/* Drop the packet */
			return;
		}

		/* destined to router interface */
		if (ifc != NULL)
		{
			/* with ICMP */
			if (i_hdr0->ip_p == ip_protocol_icmp)
			{
				/* validation */
				if (len_r < sizeof(struct sr_icmp_hdr))
					return;

				ic_hdr0 = (struct sr_icmp_hdr *)(((uint8_t *)i_hdr0) + sizeof(struct sr_ip_hdr)); /* ic_hdr0 set */

				/* echo request type */
				if (ic_hdr0->icmp_type == 0x08)			/* echo request = 8, reply = 0 */ 
				{
					/* generate ICMP echo reply packet*/
					new_len = sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_ip_hdr) + sizeof(struct sr_icmp_t0_hdr);
					new_pck = (uint8_t *) calloc(1, new_len);

					/* validation */
					checksum = ic_hdr0->icmp_sum;
					ic_hdr0->icmp_sum = 0;
					if (checksum != cksum(ic_hdr0, len - sizeof(struct sr_ethernet_hdr) - sizeof(struct sr_ip_hdr)))
						return;
					ic_hdr0->icmp_sum = checksum;

					/**************** fill in code here *****************/				

					memcpy(new_pck, packet, new_len);	/* copy given packet (ease of use) */ 

					/* ICMP header*/
					ic_hdr = (struct sr_icmp_hdr *)(new_pck + sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_ip_hdr));		/* mostly same as give packet (memcpy). */ 
                    ic_hdr->icmp_type = 0x00;		/* type now reply. */ 
                    ic_hdr->icmp_code = 0x00;		/* code is 0 (in request, but just in case (not in pdf)). */ 
                    ic_hdr->icmp_sum = 0;			/* need to initalize for cksum function. */ 
                    ic_hdr->icmp_sum = cksum(ic_hdr, new_len - sizeof(struct sr_ethernet_hdr) - sizeof(struct sr_ip_hdr));
					
					/* IP header */
					i_hdr = (struct sr_ip_hdr *)(new_pck + sizeof(struct sr_ethernet_hdr));
                    i_hdr->ip_dst = i_hdr0->ip_src;		/* now will reply to past sender. */ 
                    i_hdr->ip_src = ifc->ip;			/* now I become sender. */ 
					i_hdr->ip_ttl = INIT_TTL;			/* new packet = set to INIT_TTL (255). */ 
                    i_hdr->ip_sum = 0;					/* same reason as above. */ 
                    i_hdr->ip_sum = cksum(i_hdr, sizeof(struct sr_ip_hdr));

					/**************** fill in code here *****************/
					/* refer routing table */
					rtentry = sr_findLPMentry(sr->routing_table, i_hdr->ip_dst);		/* longest prefix match entry (reply = past sender now receiver) -> exactly one (one dest, gw, interface etc) */ 
					/* routing table hit */
					if (rtentry != NULL)
					{
						/**************** fill in code here *****************/

						/* Ethernet header */
						e_hdr = (struct sr_ethernet_hdr *)new_pck;


						/**************** fill in code here *****************/
						ifc = sr_get_interface(sr, rtentry->interface);						/* interface = char list = many */ 
						memcpy(e_hdr->ether_shost, ifc->addr, ETHER_ADDR_LEN);				/* current MAC */ 
						arpentry = sr_arpcache_lookup(&(sr->cache), (rtentry->gw.s_addr != 0 && rtentry->gw.s_addr != rtentry->dest.s_addr) ? rtentry->gw.s_addr : i_hdr->ip_dst);	/* use ip -> get mac = rtentry->gw.s_addr (this gateway IP), (gateway = next hop), (in case 0.0.0.0) */ 
						if (arpentry != NULL)
						{
							/**************** fill in code here *****************/
							/* Ethernet header */
							memcpy(e_hdr->ether_dhost, arpentry->mac, ETHER_ADDR_LEN); 		/* next hop's MAC */ 

							/* send */
							sr_send_packet(sr, new_pck, new_len, rtentry->interface);
							free(arpentry);
							/**************** fill in code here *****************/
						}
						else
						{
							/* queue */	
							arpreq = sr_arpcache_queuereq(&(sr->cache), (rtentry->gw.s_addr != 0 && rtentry->gw.s_addr != rtentry->dest.s_addr) ? rtentry->gw.s_addr : i_hdr->ip_dst, new_pck, new_len, rtentry->interface);
							sr_arpcache_handle_arpreq(sr, arpreq);
						}
					}

					/* done */
					free(new_pck);
					return;
				}

				/* other types */
				else
					return;
			}
			/* with TCP or UDP */
			else if (i_hdr0->ip_p == ip_protocol_tcp || i_hdr0->ip_p == ip_protocol_udp)
			{
				/* validation */
				if (len_r + sizeof(struct sr_ip_hdr) < ICMP_DATA_SIZE)
					return;

				/* generate ICMP port unreachable packet */
				new_len = sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_ip_hdr) + sizeof(struct sr_icmp_t3_hdr);
				new_pck = (uint8_t *) calloc(1, new_len);
				
				/**************** fill in code here *****************/			

				ifc = sr_get_interface(sr, interface);

				/* ICMP header */
				ict3_hdr = (struct sr_icmp_t3_hdr *)(new_pck + sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_ip_hdr));
                ict3_hdr->icmp_type = 0x03; 
                ict3_hdr->icmp_code = 0x03; 
                ict3_hdr->icmp_sum = 0;
                ict3_hdr->unused = 0;
                ict3_hdr->next_mtu = 0;

				memcpy(ict3_hdr->data, i_hdr0, ICMP_DATA_SIZE);		/* add original IP header + 8 bytes to data field */ 
                ict3_hdr->icmp_sum = cksum(ict3_hdr, sizeof(struct sr_icmp_t3_hdr));

				/* IP header */
				i_hdr = (struct sr_ip_hdr *)(new_pck + sizeof(struct sr_ethernet_hdr));
                memcpy(i_hdr, i_hdr0, sizeof(struct sr_ip_hdr));
                
                i_hdr->ip_len = htons(sizeof(struct sr_ip_hdr) + sizeof(struct sr_icmp_t3_hdr));
                i_hdr->ip_p = ip_protocol_icmp;
                i_hdr->ip_dst = i_hdr0->ip_src; 		/* source = where error happened */ 
                i_hdr->ip_src = ifc->ip;
                i_hdr->ip_ttl = INIT_TTL;
                i_hdr->ip_sum = 0;
                i_hdr->ip_sum = cksum(i_hdr, sizeof(struct sr_ip_hdr));

				rtentry = sr_findLPMentry(sr->routing_table, i_hdr->ip_dst);
                
                if (rtentry != NULL) 
                {

					/* Ethernet header */
					e_hdr = (struct sr_ethernet_hdr *)new_pck;
                    ifc = sr_get_interface(sr, rtentry->interface);
                    memcpy(e_hdr->ether_shost, ifc->addr, ETHER_ADDR_LEN);			/* MAC in between = way out, this source not where error started */ 
                    e_hdr->ether_type = htons(ethertype_ip);
					
					arpentry = sr_arpcache_lookup(&(sr->cache), (rtentry->gw.s_addr != 0 && rtentry->gw.s_addr != rtentry->dest.s_addr) ? rtentry->gw.s_addr : i_hdr->ip_dst);
                    if (arpentry != NULL) 
					{
						/* send */
						memcpy(e_hdr->ether_dhost, arpentry->mac, ETHER_ADDR_LEN);
                        sr_send_packet(sr, new_pck, new_len, rtentry->interface);
                        free(arpentry);
                    } 
					
					else 
					{
						/* queue */
						arpreq = sr_arpcache_queuereq(&(sr->cache), (rtentry->gw.s_addr != 0 && rtentry->gw.s_addr != rtentry->dest.s_addr) ? rtentry->gw.s_addr : i_hdr->ip_dst, new_pck, new_len, rtentry->interface);
                        sr_arpcache_handle_arpreq(sr, arpreq);
                    }
                }
						
				/* done */
				free(new_pck);

				/*****************************************************/
				return;
			}
			/* with others */
			else
				return;
		}
		/* destined elsewhere, forward */
		else
		{
			/* refer routing table */
			rtentry = sr_findLPMentry(sr->routing_table, i_hdr0->ip_dst);

			/* routing table hit */
			if (rtentry != NULL)
			{
				/* check TTL expiration */
				if (i_hdr0->ip_ttl == 1)
				{
					/**************** fill in code here *****************/

					/* validation */
					if (len < sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_ip_hdr) + 8)
                        return;

					/* generate ICMP time exceeded packet */
					new_len = sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_ip_hdr) + sizeof(struct sr_icmp_t11_hdr);
                    new_pck = (uint8_t *)calloc(1, new_len);
					ifc = sr_get_interface(sr, interface);

					/* ICMP header */
					ict11_hdr = (struct sr_icmp_t11_hdr *)(new_pck + sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_ip_hdr));
                    ict11_hdr->icmp_type = 0x0b; 
                    ict11_hdr->icmp_code = 0x00; 
                    ict11_hdr->icmp_sum = 0;
                    ict11_hdr->unused = 0;

					memcpy(ict11_hdr->data, i_hdr0, ICMP_DATA_SIZE);
                    ict11_hdr->icmp_sum = cksum(ict11_hdr, sizeof(struct sr_icmp_t11_hdr));

					/* IP header */
					i_hdr = (struct sr_ip_hdr *)(new_pck + sizeof(struct sr_ethernet_hdr));
                    memcpy(i_hdr, i_hdr0, sizeof(struct sr_ip_hdr));

					i_hdr->ip_len = htons(sizeof(struct sr_ip_hdr) + sizeof(struct sr_icmp_t11_hdr));
                    i_hdr->ip_p = ip_protocol_icmp;
                    i_hdr->ip_dst = i_hdr0->ip_src; 
                    i_hdr->ip_src = ifc->ip;
                    i_hdr->ip_ttl = INIT_TTL;
                    i_hdr->ip_sum = 0;
                    i_hdr->ip_sum = cksum(i_hdr, sizeof(struct sr_ip_hdr));

					/* Ethernet header */
					e_hdr = (struct sr_ethernet_hdr *)new_pck;
                    e_hdr->ether_type = htons(ethertype_ip);

					rtentry = sr_findLPMentry(sr->routing_table, i_hdr->ip_dst);
                    arpentry = rtentry != NULL ? sr_arpcache_lookup(&(sr->cache), (rtentry->gw.s_addr != 0 && rtentry->gw.s_addr != rtentry->dest.s_addr) ? rtentry->gw.s_addr : i_hdr->ip_dst) : NULL;
                    
                    if (rtentry != NULL && arpentry != NULL) 
                    {
						/* send */
						ifc = sr_get_interface(sr, rtentry->interface);
                        memcpy(e_hdr->ether_shost, ifc->addr, ETHER_ADDR_LEN);
                        memcpy(e_hdr->ether_dhost, arpentry->mac, ETHER_ADDR_LEN);
                        sr_send_packet(sr, new_pck, new_len, rtentry->interface);
                        free(arpentry);
                    } 
                    else if (rtentry != NULL && arpentry == NULL) 
                    {
						/* queue */
						ifc = sr_get_interface(sr, rtentry->interface);
                        memcpy(e_hdr->ether_shost, ifc->addr, ETHER_ADDR_LEN);
                        arpreq = sr_arpcache_queuereq(&(sr->cache), (rtentry->gw.s_addr != 0 && rtentry->gw.s_addr != rtentry->dest.s_addr) ? rtentry->gw.s_addr : i_hdr->ip_dst, new_pck, new_len, rtentry->interface);
                        sr_arpcache_handle_arpreq(sr, arpreq);
                    }

					/* done */
					free(new_pck);
					
					/*****************************************************/
					return;
				}
				/* TTL not expired */
				else {
					/**************** fill in code here *****************/			

					/* forwarding = packet going through = modify little (src, dst) */ 

					/* handle TTL here */ 
					i_hdr0->ip_ttl--; 
                    i_hdr0->ip_sum = 0;
                    i_hdr0->ip_sum = cksum(i_hdr0, sizeof(struct sr_ip_hdr));

					/* set src MAC addr */
					ifc = sr_get_interface(sr, rtentry->interface);
                    memcpy(e_hdr0->ether_shost, ifc->addr, ETHER_ADDR_LEN);

					/* refer ARP table */
					arpentry = sr_arpcache_lookup(&(sr->cache), (rtentry->gw.s_addr != 0 && rtentry->gw.s_addr != rtentry->dest.s_addr) ? rtentry->gw.s_addr : i_hdr0->ip_dst);
					
					if (arpentry != NULL) {
						/* set dst MAC addr */
						memcpy(e_hdr0->ether_dhost, arpentry->mac, ETHER_ADDR_LEN);
                        free(arpentry);

						/* decrement TTL */
						/* handled it above. */ 

						/* forward */
						sr_send_packet(sr, packet, len, rtentry->interface);
                    }
                    else {
						/* queue */
                        arpreq = sr_arpcache_queuereq(&(sr->cache), (rtentry->gw.s_addr != 0 && rtentry->gw.s_addr != rtentry->dest.s_addr) ? rtentry->gw.s_addr : i_hdr0->ip_dst, packet, len, rtentry->interface);
                        sr_arpcache_handle_arpreq(sr, arpreq);
                    }

					/*****************************************************/
					/* done */
					return;
				}
			}
			/* routing table miss */
			else
			{
				/**************** fill in code here *****************/

				/* validation */
				if (len < sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_ip_hdr) + 8)
                    return;

				/* generate ICMP net unreachable packet */
				new_len = sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_ip_hdr) + sizeof(struct sr_icmp_t3_hdr);
                new_pck = (uint8_t *)calloc(1, new_len);
				ifc = sr_get_interface(sr, interface);

				/* ICMP header */
				ict3_hdr = (struct sr_icmp_t3_hdr *)(new_pck + sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_ip_hdr));
                ict3_hdr->icmp_type = 0x03; 
                ict3_hdr->icmp_code = 0x00; 
                ict3_hdr->icmp_sum = 0;
                ict3_hdr->unused = 0;
                ict3_hdr->next_mtu = 0;
				memcpy(ict3_hdr->data, i_hdr0, ICMP_DATA_SIZE);
                ict3_hdr->icmp_sum = cksum(ict3_hdr, sizeof(struct sr_icmp_t3_hdr));

				/* IP header */
				i_hdr = (struct sr_ip_hdr *)(new_pck + sizeof(struct sr_ethernet_hdr));
                memcpy(i_hdr, i_hdr0, sizeof(struct sr_ip_hdr));
                i_hdr->ip_len = htons(sizeof(struct sr_ip_hdr) + sizeof(struct sr_icmp_t3_hdr));
                i_hdr->ip_p = ip_protocol_icmp;
                i_hdr->ip_dst = i_hdr0->ip_src; 
                i_hdr->ip_src = ifc->ip;
                i_hdr->ip_ttl = INIT_TTL;
                i_hdr->ip_sum = 0;
                i_hdr->ip_sum = cksum(i_hdr, sizeof(struct sr_ip_hdr));

				/* Ethernet header */
				e_hdr = (struct sr_ethernet_hdr *)new_pck;
                e_hdr->ether_type = htons(ethertype_ip);

				rtentry = sr_findLPMentry(sr->routing_table, i_hdr->ip_dst);
                arpentry = rtentry != NULL ? sr_arpcache_lookup(&(sr->cache), (rtentry->gw.s_addr != 0 && rtentry->gw.s_addr != rtentry->dest.s_addr) ? rtentry->gw.s_addr : i_hdr->ip_dst) : NULL;
                
                if (rtentry != NULL && arpentry != NULL) 
                {				
					/* send */
					ifc = sr_get_interface(sr, rtentry->interface);
                    memcpy(e_hdr->ether_shost, ifc->addr, ETHER_ADDR_LEN);
                    memcpy(e_hdr->ether_dhost, arpentry->mac, ETHER_ADDR_LEN);
                    sr_send_packet(sr, new_pck, new_len, rtentry->interface);
                    free(arpentry);
                } 
                else if (rtentry != NULL && arpentry == NULL)
                {					
					/* queue */
					ifc = sr_get_interface(sr, rtentry->interface);
                    memcpy(e_hdr->ether_shost, ifc->addr, ETHER_ADDR_LEN);
                    arpreq = sr_arpcache_queuereq(&(sr->cache), (rtentry->gw.s_addr != 0 && rtentry->gw.s_addr != rtentry->dest.s_addr) ? rtentry->gw.s_addr : i_hdr->ip_dst, new_pck, new_len, rtentry->interface);
                    sr_arpcache_handle_arpreq(sr, arpreq);
                }

				/* done */
				free(new_pck);
				
				/*****************************************************/
				return;
			}
		}
	}
	/* ARP packet arrived */
	else if (e_hdr0->ether_type == htons(ethertype_arp))
	{

		/* validation */
		if (len_r < sizeof(struct sr_arp_hdr))
			return;

		a_hdr0 = (struct sr_arp_hdr *)(((uint8_t *)e_hdr0) + sizeof(struct sr_ethernet_hdr)); /* a_hdr0 set */

		/* destined to me */
		ifc = sr_get_interface(sr, interface);
		if (a_hdr0->ar_tip == ifc->ip)		/* target ip addr == interface ip (destination) meaning */ 
		{
			/* request code */
			if (a_hdr0->ar_op == htons(arp_op_request))			/* opcode = request opcode check meaning */ 
			{
				/**************** fill in code here *****************/			

				/* generate reply */
				new_len = sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_arp_hdr);		/* ARP packet, diff from ICMP packet. */ 
                new_pck = (uint8_t *)calloc(1, new_len);									/* use calloc like ICMP packet. */ 
                if (new_pck == NULL) return;

				/* ARP header */
                a_hdr = (struct sr_arp_hdr *)(new_pck + sizeof(struct sr_ethernet_hdr));
                memcpy(a_hdr, a_hdr0, sizeof(struct sr_arp_hdr));		/* mostly same arguments */ 
                a_hdr->ar_op  = htons(arp_op_reply);	/* now reply, not request */ 
				
				/* sender = me (cur router) */ 
                memcpy(a_hdr->ar_sha, ifc->addr, ETHER_ADDR_LEN);		/* MAC */ 
                a_hdr->ar_sip = ifc->ip;								/* IP */ 

				/* receiver = past sender (who requested) */ 
                memcpy(a_hdr->ar_tha, a_hdr0->ar_sha, ETHER_ADDR_LEN);		/* MAC */ 
                a_hdr->ar_tip = a_hdr0->ar_sip;								/* IP */ 

				/* Ethernet header */
                e_hdr = (struct sr_ethernet_hdr *)new_pck;
                memcpy(e_hdr->ether_dhost, e_hdr0->ether_shost, ETHER_ADDR_LEN);	/* dst MAC = past sender */ 
                memcpy(e_hdr->ether_shost, ifc->addr, ETHER_ADDR_LEN);				/* src MAC = me */ 
                e_hdr->ether_type = htons(ethertype_arp);							/* type = arp */ 

				/* send */
				sr_send_packet(sr, new_pck, new_len, interface);	/* from sr_vns_comm.c, sr, new_pck, interface...? (meaning of borrowed) */ 

				/* done */
				free(new_pck);			/* free new_pck (made it here, not given = should manage it) */ 
				
				/*****************************************************/
				return;
			}

			/* reply code */
			else if (a_hdr0->ar_op == htons(arp_op_reply))		/* got reply */ 
			{
				/**************** fill in code here *****************/			

				/* pass info to ARP cache */
				arpreq = sr_arpcache_insert(&(sr->cache), a_hdr0->ar_sha, a_hdr0->ar_sip);		/* arpreq != NULL == list of all penders. */ 

				/* pending request exist */
				if (arpreq != NULL) {

					/* find all penders (indent clue) */ 
                    for (en_pck = arpreq->packets; en_pck != NULL; en_pck = en_pck->next)		/* en_pck from above. */ 
                    {
                        /* set dst MAC addr */
                        e_hdr = (struct sr_ethernet_hdr *)(en_pck->buf);				/* buf = raw ethernet frame = can do this. */ 
                        memcpy(e_hdr->ether_dhost, a_hdr0->ar_sha, ETHER_ADDR_LEN);		/* replied source = dest for penders. */ 

                        /* decrement TTL except for self-generated packets */
                        /* No need here (TTL in IP, this ARP). */ 

                        /* send */
                        sr_send_packet(sr, en_pck->buf, en_pck->len, en_pck->iface);	/* raw ethernet frame = think packet, front = ethernet header structure. */ 
                    }

					/* done */
					sr_arpreq_destroy(&(sr->cache), arpreq);		/* need to do memory management (sorted out penders = not needed anymore = free mem) */ 
					
					/*****************************************************/
					return;
				}
				/* no exist */
				else
					return;
			}

			/* other codes */
			else
				return;
		}

		/* destined to others */
		else
			return;
	}

	/* other packet arrived */
	else
		return;

} /* end sr_ForwardPacket */

struct sr_rt *sr_findLPMentry(struct sr_rt *rtable, uint32_t ip_dst)
{
	struct sr_rt *entry, *lpmentry = NULL;
	uint32_t mask, lpmmask = 0;

	ip_dst = ntohl(ip_dst);

	/* scan routing table */
	for (entry = rtable; entry != NULL; entry = entry->next)
	{
		mask = ntohl(entry->mask.s_addr);
		/* longest match so far */
		if ((ip_dst & mask) == (ntohl(entry->dest.s_addr) & mask) && mask > lpmmask)
		{
			lpmentry = entry;
			lpmmask = mask;
		}
	}

	return lpmentry;
}
