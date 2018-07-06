#ifndef DPDK_PORT_H_
#define DPDK_PORT_H_

#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <map>

#include "dpdk_import.h"

typedef struct rte_mempool* RteMemPoolPtr;
typedef struct rte_eth_dev_info RteEthDevInfo;
typedef struct rte_eth_conf RteEthConf;

#define MBUF_CACHE_SIZE 512

class DpdkPort {
 public:
  DpdkPort(uint8_t port_id, uint16_t rx_rings, uint16_t tx_rings);
  ~DpdkPort();

  int Setup(RteMemPoolPtr, bool rss);
  int Start();
  void Stop();
  void Close();

  uint8_t PortId() { return port_id_; }

  int SocketId() { return socket_id_; }

  int CoreId() { return core_id_; }

  int RxRings() { return rx_rings_; }

  int TxRings() { return tx_rings_; }

  int RxDesc() { return num_rxdesc_; }

  int TxDesc() { return num_txdesc_; }

 private:
  uint8_t port_id_;
  int socket_id_;
  int core_id_;
  std::string dev_name_;
  int rx_rings_;
  int tx_rings_;
  int num_rxdesc_;
  int num_txdesc_;
  RteEthDevInfo dev_info_;
  RteEthConf port_conf_;
};

class DpdkRte {
 public:
  ~DpdkRte();

  static DpdkRte* Instance();

  int RteInit(int argc, char* argv[]);

  void PrintInfo();

  int PortsInit();

  int CapPortNum() { return cap_core_num_; }

  DpdkPort* GetPort(size_t i) { return ports_[i]; }

  void StartPorts();

  void StopPorts();

  void ClosePorts();

  bool IfSelfSend() { return self_sent_; }

 public:
  int core_num;

  int port_num;

 private:
  int cap_core_num_;
  int mbuf_size_;
  bool rss_;
  bool self_sent_;

 private:
  DpdkRte() {}
  DpdkRte& operator=(const DpdkRte&) = delete;
  DpdkRte(const DpdkRte&) = delete;

 private:
  static DpdkRte* rte_;
  static std::mutex mutex_;

 private:
  std::vector<DpdkPort*> ports_;
  std::map<uint8_t, RteMemPoolPtr> ports_mempools_;
};

#endif
