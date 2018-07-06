## 网卡型号

MCX4121A-ACAT
```
15b3  Mellanox Technologies
    1015  MT27710 Family [ConnectX-4 Lx]
            15b3 0003  Stand-up ConnectX-4 Lx EN, 25GbE dual-port SFP28, PCIe3.0 x8, MCX4121A-ACAT

```
## 系统
centos 7.3.1611

## 注意
* 是否需要UIO/igb_uio等内核模块?
    不需要, 不能使用dpdk-init将网卡绑定为igb_uio模块
* 大页内存的使用有什么区别?
```
echo 1024> /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages
echo 1024> /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
```
* 需要什么其他内核模块?
    需要, 通过Mellanox_OFED安装
* 网卡如何使用RSS?
    无需额外设置, 应该与i40e相同
* 如何turning Mlx网卡?
    参考`mlnx_tuning_scripts_package`包
    
## 网卡操作

```
[root@localhost mxl5_test]# cat /sys/class/net/ens1f1/settings/hfunc
Operational hfunc: xor
Supported hfuncs: xor toeplitz

[root@localhost mxl5_test]# ethtool --show-rxfh ens1f1
RSS hash key:
2a:e7:34:ef:fa:33:d2:d9:e9:8c:49:c9:88:83:76:ef:ea:5f:62:c2:58:ce:41:0f:6f:f2:f8:4b:aa:23:73:e5:f9:7c:0e:73:a0:e7:88:ce

```

支持rss
```
echo toeplitz > /sys/class/net/ens1f1/settings/hfunc
```

## OFED

```
ofed_info -s
rpm -qa | grep rdma
```

## mlx5 DPDK编译
下载Linux驱动    
~~wget http://www.mellanox.com/downloads/ofed/MLNX_EN-4.3-1.0.1.0/mlnx-en-4.3-1.0.1.0-rhel7.4-x86_64.tgz~~

下载这个 MLNX_OFED_LINUX-4.3-1.0.1.0-rhel7.3-x86_64.tgz

DPDK官网参考    
[http://doc.dpdk.org/guides/nics/mlx5.html](http://doc.dpdk.org/guides-16.11/nics/mlx5.html)    
[http://doc.dpdk.org/guides-16.11/nics/mlx5.html](http://doc.dpdk.org/guides-16.11/nics/mlx5.html)    

## 应用编译
```
linkFlags  += "-Wl,-lrte_pmd_mlx5 -Wl, -libverbs"
```

## 问题
#### 1.dpdk编译错误, MLX5_RSS_HF_MASK 未声明.
dpdk-16.11.6/drivers/net/mlx5/mlx5_ethdev.c:661:34: 错误：**‘MLX5_RSS_HF_MASK** 未声明(在此函数内第一次使用)
dpdk-16.11 dpdk-16.11.4可以编译通过, dpdk-16.11.5之后不能编译通过.

可能的解决方法:

选择正确的Mellanox  OFED版本.

不同的OFED版本对应的功能可能不能, 需要查看release说明, 并实际测试.
```      
dpdk16.11 -> Mellanox OFED3.4        NOK
dpdk17.11 -> Mellanox OFED4.2        OK
dpdk18    -> Mellanox OFED4.2/4.3    OK
```

建议使用: dpdk-17.11.3, 配合Mellanox OFED4.2

> ./mlnxofedinstall --upstream-libs  --dpdk

#### 2.应该下载MLNX_OFED包

## 参考
[https://community.mellanox.com/docs/DOC-2489](https://community.mellanox.com/docs/DOC-2489)    
[http://doc.dpdk.org/guides/nics/mlx5.html](http://doc.dpdk.org/guides-16.11/nics/mlx5.html)    
[http://doc.dpdk.org/guides-16.11/nics/mlx5.html](http://doc.dpdk.org/guides-16.11/nics/mlx5.html)   
https://www.openfabrics.org/index.php/openfabrics-software.html  
http://www.mellanox.com/page/products_dyn?product_family=26&mtag=linux    
https://www.kernel.org/doc/Documentation/vm/hugetlbpage.txt    



