#include "queue.h"
#include "lib.h"
#include "protocols.h"
#include <string.h>
#include <arpa/inet.h>


int arp_table_length, rtable_length;
struct route_table_entry *rtable;
struct arp_entry *arp_table;

 struct route_table_entry *get_best_route(uint32_t ip_dest, struct route_table_entry *rtable) {
 	int vizitat = 0;
 	struct route_table_entry* best = NULL; 
	for(int i = 0; i < rtable_length; i++){
		if((ip_dest & ntohl(rtable[i].mask)) == ntohl(rtable[i].prefix)){
			vizitat = 1;
			if(best == NULL) {
				best = &rtable[i];
			}
			else {
				if(ntohl(best->mask) < ntohl(rtable[i].mask)){
					best = &rtable[i];
				}
			}
		}
	}

	if (vizitat == 1) {
		return best;
	} else {
		return NULL;
	}
}

struct arp_entry *get_mac_entry(uint32_t ip_dest, struct arp_entry *arp_table) {
	for(int i = 0; i < arp_table_length; i++){
		if(ntohl(arp_table[i].ip) == ntohl(ip_dest)){
			return &arp_table[i];
		}
	}
	return NULL;
}


int main(int argc, char *argv[])
{
	char buf[MAX_PACKET_LEN];
	
	struct arp_entry *arp_table = malloc(10 * sizeof(struct arp_entry));
	DIE(arp_table == NULL, "memory");
	struct route_table_entry *rtable = malloc(100000 * sizeof(struct route_table_entry));
	DIE(rtable == NULL, "memory");
	rtable_length = read_rtable(argv[1], rtable);
	arp_table_length = parse_arp_table("arp_table.txt", arp_table);

	// Do not modify this line
	init(argc - 2, argv + 2);

	/* Note that packets received are in network order,
		any header field which has more than 1 byte will need to be conerted to
		host order. For example, ntohs(eth_hdr->ether_type). The oposite is needed when
		sending a packet on the link, */

	while (1) {

		int interface;
		size_t len;
		struct in_addr aux;

		interface = recv_from_any_link(buf, &len);
		DIE(interface < 0, "recv_from_any_links");

		struct ether_header *eth_hdr = (struct ether_header *) buf;
		struct iphdr *ip_hdr = (struct iphdr *)(buf + sizeof(struct ether_header));

		/*Pas 1: Verificare daca pachetul este destinat router-ului.*/
		inet_aton(get_interface_ip(interface), &aux);
		if (aux.s_addr == ip_hdr->daddr) {
			continue;
		}

		/*Pas 2: Verificare checksum*/
		const uint16_t checks = ip_hdr->check;
		ip_hdr->check = 0;
		ip_hdr->check = htons(checksum((uint16_t*)ip_hdr, sizeof(struct iphdr)));

		if (ip_hdr->check != checks) 
		{
			continue ;
		}

		/*Pas 3: Verificare TTL.*/
		if (ip_hdr->ttl < 1) {
			continue;
		}
		ip_hdr->ttl -- ;

		/*Pas 4: Gasire ruta*/
		struct route_table_entry *best_route = get_best_route(ntohl(ip_hdr->daddr), rtable);
		if (best_route == NULL) {
			continue;
		}

		/*Pas 5: Update checksum*/
		ip_hdr->check = 0;
		ip_hdr->check = htons(checksum((uint16_t*)ip_hdr, sizeof(struct iphdr)));

		/*Pas 6: Rescriere adrese fizice.*/
		get_interface_mac(best_route->interface, eth_hdr->ether_shost);

		struct arp_entry *mac_entry = get_mac_entry(best_route->next_hop, arp_table); 
		if(mac_entry == NULL){
			continue;
		}
		memcpy(eth_hdr->ether_dhost, mac_entry->mac, sizeof(eth_hdr->ether_dhost));
		
		/*Pas 7: Trimitere pachet pe ruta gasita.*/
		send_to_link(best_route->interface, buf, len);
	}
}

