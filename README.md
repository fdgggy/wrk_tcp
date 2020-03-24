# wrk_tcp
基于wrk二次开发的tcp压测工具，测试脚本为Lua

## 编译
mac平台make macosx, Linux平台make linux

## 基本用法
项目根目录config.lua文件配置相关信息

```
local config = {
    connections = 1,  --连接数
    duration = 5,     --压测持续时间
    threads = 1,      --线程数
    timeout = 8000,   --超时时间(毫秒)
    host = "127.0.0.1",
    port = 17793,
    url = "http://127.0.0.1:17788",
}
具体交互测试逻辑请看game文件夹示例

```
输出:

    Running 5s test skynet
        1 threads and actually 1 connections
        Thread Stats       Avg       Min       Max
        Latency       16.11 ms    16.11 ms    16.11 ms
        1 requests 3 responses in 5061.40 ms, 9.49 kb read
    Requests/sec:      0.20
    Responses/sec:      0.59
    Transfer/sec:       1.88 kb

## 提示:
客户端运行wrk_tcp的机器必须有足够数量的临时端口和可重用且能快速回收time-wait状态的scoket，具体参考如下：

在/etc/sysctl.conf加入以下内容
```
# 系统级别最大打开文件
fs.file-max = 100000

# 单用户进程最大文件打开数
fs.nr_open = 100000

# 是否重用, 快速回收time-wait状态的tcp连接
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_tw_recycle = 1

# 单个tcp连接最大缓存byte单位
net.core.optmem_max = 8192

# 可处理最多孤儿socket数量，超过则警告，每个孤儿socket占用64KB空间
net.ipv4.tcp_max_orphans = 10240

# 最多允许time-wait数量
net.ipv4.tcp_max_tw_buckets = 10240

# 从客户端发起的端口范围,默认是32768 61000，则只能发起2w多连接，改为一下值，可一个IP可发起差不多6.4w连接。
net.ipv4.ip_local_port_range = 1024 65535
```

在/etc/security/limits.conf加入以下内容
```
# 最大不能超过fs.nr_open值, 分别为单用户进程最大文件打开数，soft指软性限制,hard指硬性限制
* soft nofile 100000
* hard nofile 100000
root soft nofile 100000
root hard nofile 100000
```

服务端设置全连接队列大小应该大于并发连接数，全连接队列取值：min(backlog,/proc/sys/net/core/somaxconn)，其中
backlog为listen时传入，后者为可配置，具体配置如下：

在/etc/sysctl.conf加入以下内容

```
# 系统最大文件打开数
fs.file-max = 20000000

# 单个用户进程最大文件打开数
fs.nr_open = 20000000

# 全连接队列长度,默认128
net.core.somaxconn = 10240
# 半连接队列长度，当使用sysncookies无效，默认128
net.ipv4.tcp_max_syn_backlog = 16384
net.ipv4.tcp_syncookies = 0

# 网卡数据包队列长度  
net.core.netdev_max_backlog = 41960

# time-wait 最大队列长度
net.ipv4.tcp_max_tw_buckets = 300000

# time-wait 是否重新用于新链接以及快速回收
net.ipv4.tcp_tw_reuse = 1  
net.ipv4.tcp_tw_recycle = 1
# tcp报文探测时间间隔, 单位s
net.ipv4.tcp_keepalive_intvl = 30
# tcp连接多少秒后没有数据报文时启动探测报文
net.ipv4.tcp_keepalive_time = 900
# 探测次数
net.ipv4.tcp_keepalive_probes = 3

# 保持fin-wait-2 状态多少秒
net.ipv4.tcp_fin_timeout = 15  

# 最大孤儿socket数量,一个孤儿socket占用64KB,当socket主动close掉,处于fin-wait1, last-ack
net.ipv4.tcp_max_orphans = 131072  

# 每个套接字所允许得最大缓存区大小
net.core.optmem_max = 819200

# 默认tcp数据接受窗口大小
net.core.rmem_default = 262144  
net.core.wmem_default = 262144  
net.core.rmem_max = 16777216  
net.core.wmem_max = 16777216
# tcp栈内存使用第一个值内存下限, 第二个值缓存区应用压力上限, 第三个值内存上限, 单位为page,通常为4kb
net.ipv4.tcp_mem = 786432 4194304 8388608
# 读, 第一个值为socket缓存区分配最小字节, 第二个，第三个分别被rmem_default, rmem_max覆盖
net.ipv4.tcp_rmem = 4096 4096 4206592
# 写, 第一个值为socket缓存区分配最小字节, 第二个，第三个分别被wmem_default, wmem_max覆盖
net.ipv4.tcp_wmem = 4096 4096 4206592
```

在/etc/security/limits.conf加入一下内容
```
# End of file
root      soft    nofile          2100000
root      hard    nofile          2100000
*         soft    nofile          2100000
*         hard    nofile          2100000
```

重启使上述内容生效,说使用命令sysctl -p

