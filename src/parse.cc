#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>

bool PreparePacket(rte_mbuf* m, int port_id, int ring_id) {
  char* pkt_data = rte_ctrlmbuf_data(m);
  uint16_t* eth_type = (uint16_t*)(pkt_data + 2 * ETHER_ADDR_LEN);
  m->l2_len = 2 * ETHER_ADDR_LEN;
  ipv4_hdr* ipv4 = NULL;
  tcp_hdr* tcp = NULL;

  while (*eth_type == rte_cpu_to_be_16(ETHER_TYPE_VLAN) ||
         *eth_type == rte_cpu_to_be_16(ETHER_TYPE_QINQ)) {
    eth_type += 2;
    m->l2_len += 4;
  }
  m->l2_len += 2;

  uint8_t ip_proto = 0;
  switch (rte_cpu_to_be_16(*eth_type)) {
    case ETHER_TYPE_IPv4: {
      ipv4 = (ipv4_hdr*)(eth_type + 1);
      m->l3_len = 4 * (ipv4->version_ihl & 0x0F);
      ip_proto = ipv4->next_proto_id;
      break;
    }
    case ETHER_TYPE_IPv6: {
      m->l3_len = sizeof(ipv6_hdr);  //
      ip_proto = ((ipv6_hdr*)(eth_type + 1))->proto;
      break;
    }
    default: {
      printf("%s\n", "Packet is not IP - not supported");
      return false;
    }
  }
  // If it's not TCP or UDP packet - skip it
  switch (ip_proto) {
    case IPPROTO_TCP: {
      tcp = rte_pktmbuf_mtod_offset(m, tcp_hdr*, m->l2_len + m->l3_len);
      m->l4_len = 4 * ((tcp->data_off & 0xF0) >> 4);

      printf(
          "port_id=%02d, ring_num=%02d, hash=%x, sip=%u, dip=%u, sport=%d, "
          "dport=%d\n",
          port_id, ring_id, m->hash.rss, ipv4->src_addr, ipv4->dst_addr,
          tcp->src_port, tcp->dst_port);

      break;
    }
    case IPPROTO_UDP: {
      m->l4_len = sizeof(udp_hdr);  //
      break;
    }
    default: { return false; }
  }

  return true;
}