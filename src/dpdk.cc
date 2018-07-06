#include <getopt.h>

#include "dpdk.h"

#include <string>

DpdkRte* DpdkRte::rte_ = nullptr;
std::mutex DpdkRte::mutex_;

#define RSS_KEY_SIZE 52
static uint8_t rss_sym_key[RSS_KEY_SIZE] = {
    0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d,
    0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
    0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d,
    0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
    0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a};

DpdkRte::~DpdkRte() { ClosePorts(); }

DpdkRte* DpdkRte::Instance() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (rte_ == nullptr) {
    rte_ = new DpdkRte;
  }
  return rte_;
}

int DpdkRte::RteInit(int argc, char* argv[]) {
  int ret = rte_eal_init(argc, argv);
  if (ret < 0) rte_exit(EXIT_FAILURE, "Invalid EAL parameters\n");
  cap_core_num_ = 1;
  mbuf_size_ = 0;
  rss_ = false;
  self_sent_ = false;
  core_num = rte_lcore_count();
  port_num = rte_eth_dev_count();
  argc -= ret;
  argv += ret;

  int opt;
  while ((opt = getopt(argc, argv, "b:c:rs")) != -1) {
    switch (opt) {
      case 'b':
        mbuf_size_ = atoi(optarg);
        break;
      case 'c':
        cap_core_num_ = atoi(optarg);
        break;
      case 'r':
        rss_ = true;
        break;
      case 's':
        self_sent_ = true;
        break;
      default: /* '?' */
        rte_exit(EXIT_FAILURE, "Invalid EAL parameters\n");
    }
  }
  return ret;
}

int DpdkRte::PortsInit() {
  char pool_name[256] = {0};
  for (int i = 0; i < port_num; i++) {
    DpdkPort* port = new DpdkPort(i, cap_core_num_, cap_core_num_);
    ports_.push_back(port);
    uint8_t socketid = port->SocketId();
    if (ports_mempools_.count(socketid) == 0) {
      unsigned size = (port->RxRings() * port->RxDesc() +
                       port->TxRings() * port->TxDesc()) +
                      MBUF_CACHE_SIZE * (cap_core_num_ + 1);
      size += mbuf_size_;
      printf("pktmbuf_pool size = %u, mbuf_size_=%d, cap_core_num_= %d\n", size,
             mbuf_size_, cap_core_num_);
      snprintf(pool_name, sizeof pool_name, "%s_%d", "DpdkRte_MBUF_POOL", i);
      RteMemPoolPtr mbuf_pool =
          rte_pktmbuf_pool_create(pool_name, size, MBUF_CACHE_SIZE, 0,
                                  RTE_MBUF_DEFAULT_BUF_SIZE, socketid);
      if (mbuf_pool == NULL) {
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
      }
      ports_mempools_[socketid] = mbuf_pool;
    }
  }

  for (std::vector<DpdkPort*>::iterator x = ports_.begin(); x != ports_.end();
       x++) {
    int rt = (*x)->Setup(ports_mempools_[(*x)->SocketId()], rss_);
    if (rt != 0) {
      rte_exit(EXIT_FAILURE, "fail to Setup \n");
    }
  }
  return 0;
}

void DpdkRte::StartPorts() {
  for (std::vector<DpdkPort*>::iterator x = ports_.begin(); x != ports_.end();
       x++) {
    (*x)->Start();
  }
}

void DpdkRte::StopPorts() {
  for (std::vector<DpdkPort*>::iterator x = ports_.begin(); x != ports_.end();
       x++) {
    (*x)->Stop();
  }
}

void DpdkRte::ClosePorts() {
  for (std::vector<DpdkPort*>::iterator x = ports_.begin(); x != ports_.end();
       x++) {
    (*x)->Close();
  }
}

void DpdkRte::PrintInfo() {
  printf("core_num = %d\n", core_num);
  printf("port_num = %d\n", port_num);
}

DpdkPort::DpdkPort(uint8_t port_id, uint16_t rx_rings, uint16_t tx_rings)
    : port_id_(port_id),
      rx_rings_(rx_rings),
      tx_rings_(tx_rings),
      num_rxdesc_(512),
      num_txdesc_(512) {
  char dev_name[128];
  port_conf_.rxmode.mq_mode = ETH_MQ_RX_RSS;
  port_conf_.rx_adv_conf.rss_conf.rss_key = nullptr;  // use defaut DPDK-key
  // port_conf_.rx_adv_conf.rss_conf.rss_hf = ETH_RSS_IP | ETH_RSS_TCP |
  // ETH_RSS_UDP;
  port_conf_.rx_adv_conf.rss_conf.rss_hf = ETH_RSS_PROTO_MASK;
  // Tune tx
  port_conf_.txmode.mq_mode = ETH_MQ_TX_NONE;

  if (rte_eth_dev_is_valid_port(port_id_) == 0) {
    rte_exit(EXIT_FAILURE, "fail to get rte_eth_dev_is_valid_port \n");
  }

  if (rte_eth_dev_get_name_by_port(port_id_, dev_name) != 0) {
    rte_exit(EXIT_FAILURE, "fail to get rte_eth_dev_get_name_by_port\n");
  }
  dev_name_ = std::string(dev_name);

  socket_id_ = rte_eth_dev_socket_id(port_id_);
  core_id_ = rte_lcore_id();
  rte_eth_dev_info_get(1, &dev_info_);
  printf("port_id = %d, socket_id_ = %d\n", port_id, socket_id_);
}

DpdkPort::~DpdkPort() {}

int DpdkPort::Setup(RteMemPoolPtr mbuf_pool, bool rss) {
  int ret = 0;
  if (rss) {
    printf("rss set--------------------------------------\n");
    port_conf_.rxmode.mq_mode = ETH_MQ_RX_RSS;
    port_conf_.rxmode.jumbo_frame = 1;
    port_conf_.rxmode.max_rx_pkt_len = 2048;
    port_conf_.rx_adv_conf.rss_conf.rss_key = rss_sym_key;
    port_conf_.rx_adv_conf.rss_conf.rss_key_len = RSS_KEY_SIZE;
    port_conf_.rx_adv_conf.rss_conf.rss_hf =
        ETH_RSS_IP | ETH_RSS_UDP | ETH_RSS_TCP | ETH_RSS_SCTP;
    port_conf_.txmode.mq_mode = ETH_MQ_TX_NONE;
  }
  ret = rte_eth_dev_configure(port_id_, rx_rings_, tx_rings_, &port_conf_);
  if (ret) {
    printf("%s\n", "rte_eth_dev_configure");
    return ret;
  }

  for (int q = 0; q < rx_rings_; q++) {
    ret = rte_eth_rx_queue_setup(port_id_, q, num_rxdesc_, socket_id_, nullptr,
                                 mbuf_pool);
    if (ret) {
      printf("%s\n", "rte_eth_rx_queue_setup");
      return ret;
    }
  }

  for (int q = 0; q < tx_rings_; q++) {
    ret = rte_eth_tx_queue_setup(port_id_, q, num_txdesc_, socket_id_, nullptr);
    if (ret) {
      printf("%s\n", "rte_eth_tx_queue_setup");
      return ret;
    }
  }

  rte_eth_promiscuous_enable(port_id_);
  rte_eth_allmulticast_enable(port_id_);
  return 0;
}

int DpdkPort::Start() {
  rte_eth_dev_stop(port_id_);
  int ret = rte_eth_dev_start(port_id_);
  if (ret) {
    rte_exit(EXIT_FAILURE, "Cannot start port %d \n", port_id_);
  }
  return 0;
}

void DpdkPort::Stop() { rte_eth_dev_stop(port_id_); }

void DpdkPort::Close() {
  rte_eth_dev_stop(port_id_);
  rte_eth_dev_close(port_id_);
}