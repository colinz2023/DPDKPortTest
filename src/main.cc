#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "dpdk_import.h"
#include "dpdk.h"

#include "access_cmdline.h"
#include "utils.h"
#include "sfrb.h"

#define CMD_PROMPT "port_test > "
#define PORT_TEST_VERSION "2_r4"

#define Likely(x) (__builtin_expect(!!(x), 1))
#define Unlikely(x) (__builtin_expect(!!(x), 0))

extern bool PreparePacket(rte_mbuf* m, int, int);

struct EthStats {
  struct timeval tv;
  struct rte_eth_stats iostats;
};

struct EthStatsArray {
  EthStats eth_stat[8];
  EthStatsArray() { memset(&eth_stat, 0, sizeof(eth_stat)); }
};

struct EthRate {
  double iMpps;
  double oMpps;
  double iMbps;
  double oMbps;
  void Clear() {
    iMpps = 0.0;
    oMpps = 0.0;
    iMbps = 0.0;
    oMbps = 0.0;
  }
};

bool gStopRunning = false;
DpdkRte* gDpdkRte = NULL;

Sfrb<EthStatsArray>* gEthStatRbPtr = NULL;
static uint64_t gStartTime = 0;

static uint64_t GetTimeUpNow() {
  struct timespec tpstart;
  clock_gettime(CLOCK_MONOTONIC, &tpstart);
  return tpstart.tv_sec;
}

static void PrintUseTime(uint64_t time) {
  int hour = time / 3600;
  int minute = (time - hour * 3600) / 60;
  int second = time % 60;
  printf("程序运行距今: %d小时 %d分 %d秒\n", hour, minute, second);
}

int CalaRate(EthStatsArray* est1, EthStatsArray* est0, EthRate* er, int i) {
  uint64_t vs0 =
      static_cast<int64_t>(est0->eth_stat[i].tv.tv_sec) * 1000 * 1000 +
      est0->eth_stat[i].tv.tv_usec;
  uint64_t vs1 =
      static_cast<int64_t>(est1->eth_stat[i].tv.tv_sec) * 1000 * 1000 +
      est1->eth_stat[i].tv.tv_usec;

  er->Clear();

  if (vs1 <= vs0) {
    return -1;
  }

  uint64_t d_vs = vs1 - vs0;
  uint64_t d_ipackets =
      est1->eth_stat[i].iostats.ipackets - est0->eth_stat[i].iostats.ipackets;
  uint64_t d_opackets =
      est1->eth_stat[i].iostats.opackets - est0->eth_stat[i].iostats.opackets;
  uint64_t d_ibytes =
      est1->eth_stat[i].iostats.ibytes - est0->eth_stat[i].iostats.ibytes;
  uint64_t d_obytes =
      est1->eth_stat[i].iostats.obytes - est0->eth_stat[i].iostats.obytes;

  er->iMpps = static_cast<double>(d_ipackets) / d_vs;
  er->oMpps = static_cast<double>(d_opackets) / d_vs;
  er->iMbps = static_cast<double>(d_ibytes) / d_vs;
  er->oMbps = static_cast<double>(d_obytes) / d_vs;
  return 0;
}

void PortStatReset() {
  for (int i = 0; i < gDpdkRte->port_num; i++) {
    rte_eth_stats_reset(i);
  }
}

std::string PortStat() {
  int i = 0;
  struct rte_eth_stats iostats;
  struct rte_eth_link link;
  std::string result;
  EthStatsArray esa0;
  EthStatsArray esa5;
  EthRate eth_rate;

  gEthStatRbPtr->GetOffset(&esa0, 0);
  gEthStatRbPtr->GetOffset(&esa5, 6);

  for (i = 0; i < gDpdkRte->port_num; i++) {
    uint8_t port_id = gDpdkRte->GetPort(i)->PortId();
    int ret = rte_eth_stats_get(port_id, &iostats);
    if (ret != 0) return "";
    CalaRate(&esa0, &esa5, &eth_rate, i);
    result += FormatStr(
        "port %-3d \n"
        "         ipackets  %-11llu\topackets  %-11llu\n"
        "         ibytes    %-11llu\tobytes    %-11llu\n"
        "         ierrors   %-11llu\toerrors   %-11llu\n"
        "         rx_nombuf %-11llu\timissed   %-11llu\n"
        "         iMpps     %-5.5f\toMpps     %-5.5f\n"
        "         iMbps     %-5.5f\toMbps     %-5.5f\n",
        port_id, iostats.ipackets, iostats.opackets, iostats.ibytes,
        iostats.obytes, iostats.ierrors, iostats.oerrors, iostats.rx_nombuf,
        iostats.imissed, eth_rate.iMpps, eth_rate.oMpps, eth_rate.iMbps * 8,
        eth_rate.oMbps * 8);
    rte_eth_link_get_nowait(port_id, &link);
    result += FormatStr("         link      %-11s\t",
                        (link.link_speed == ETH_LINK_DOWN) ? "down" : "up");
    result +=
        FormatStr("speed     %u %s\n", (link.link_speed > ETH_SPEED_NUM_100M)
                                           ? link.link_speed / 1000
                                           : link.link_speed,
                  (link.link_speed > ETH_SPEED_NUM_100M) ? "Gbps" : "Mbps");
  }
  return result;
}

static int stat_slave(__attribute__((unused)) void* ptr_data) {
  EthStatsArray esa;
  while (!gStopRunning) {
    usleep(300 * 1000);
    for (int i = 0; i < gDpdkRte->port_num; i++) {
      uint8_t port_id = gDpdkRte->GetPort(i)->PortId();
      rte_eth_stats_get(port_id, &esa.eth_stat[i].iostats);
      gettimeofday(&esa.eth_stat[i].tv, NULL);
      gEthStatRbPtr->Set(&esa);
    }
  }
  return 0;
}

#define PKT_READ_SIZE 256
static int cap_slave(__attribute__((unused)) void* ptr_data) {
  struct rte_mbuf* pkts[PKT_READ_SIZE];
  uint8_t port_id = 0;
  uint16_t rx_pkts = 0;

  int core_id = rte_lcore_id();
  int ring_num = core_id % gDpdkRte->CapPortNum();

  for (;;) {
    if (Unlikely(gStopRunning)) {
      break;
    }

    for (int p = 0; p < gDpdkRte->port_num; p++) {
      port_id = gDpdkRte->GetPort(p)->PortId();
      rx_pkts = rte_eth_rx_burst(port_id, ring_num, pkts, PKT_READ_SIZE);
      // && port_id == 0
      if (Unlikely(gDpdkRte->IfSelfSend())) {
        uint16_t sent = 0;
        while (sent < rx_pkts) {
          sent +=
              rte_eth_tx_burst(port_id, ring_num, &pkts[sent], rx_pkts - sent);
        }
      } else {
        for (uint16_t i = 0; i < rx_pkts; i++) {
          // PreparePacket(pkts[i], port_id, ring_num);
          rte_pktmbuf_free(pkts[i]);
        }
      }
    }
  }
  printf("%s, core_id = %d\n", "cap_slave exit", core_id);
  return 0;
}

static void help() {
  printf(
      "支持以下命令:\n"
      "version 显示程序版本 \n"
      "uptime  程序运行时长 \n"
      "state    端口状态 \n"
      "clear    clear \n"
      "exit    退出程序\n");
}

static void version() {
  printf(
      "dpdk version: %s\n"
      "tool version: %s \n",
      rte_version(), PORT_TEST_VERSION);
}

static void CmdLineProcess() {
  CmdLine cmd_line(CMD_PROMPT, ".test_history");
  cmd_line.addHints("exit");
  cmd_line.addHints("state");
  cmd_line.addHints("help");
  cmd_line.addHints("version");
  cmd_line.addHints("uptime");
  cmd_line.addHints("reset_state");
  cmd_line.addHints("clear");

  while (1) {
    if (unlikely(gStopRunning)) {
      break;
    }
    std::string cmd = cmd_line.readLine();
    if (cmd.size() > 0) {
      if (cmd == "exit") {
        gStopRunning = true;
        break;
      } else if (cmd == "state") {
        printf("%s\n", PortStat().c_str());
      } else if (cmd == "help" || cmd == "?") {
        help();
      } else if (cmd == "version") {
        version();
      } else if (cmd == "uptime") {
        PrintUseTime(GetTimeUpNow() - gStartTime);
      } else if (cmd == "reset_state" || cmd == "clear") {
        PortStatReset();
      } else {
        printf("不支持\n");
      }
      printf(CMD_PROMPT);
      fflush(stdout);
    }
  }
}

int main(int argc, char* argv[]) {
  gDpdkRte = DpdkRte::Instance();
  gDpdkRte->RteInit(argc, argv);
  gDpdkRte->PortsInit();

  gEthStatRbPtr = new Sfrb<EthStatsArray>(30);

  uint32_t id_core = rte_lcore_id();
  for (int c = 0; c < gDpdkRte->CapPortNum(); c++) {
    id_core = rte_get_next_lcore(id_core, 1, 1);
    rte_eal_remote_launch(cap_slave, NULL, id_core);
  }

  id_core = rte_get_next_lcore(id_core, 1, 1);
  rte_eal_remote_launch(stat_slave, NULL, id_core);

  gStartTime = GetTimeUpNow();

  gDpdkRte->StartPorts();
  CmdLineProcess();

  RTE_LCORE_FOREACH_SLAVE(id_core) {
    if (rte_eal_wait_lcore(id_core) < 0) {
      return -1;
    }
  }
  delete gDpdkRte;
  delete gEthStatRbPtr;
  return 0;
}
