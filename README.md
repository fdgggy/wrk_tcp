# wrk_tcp
基于wrk二次开发的tcp压测工具，测试脚本为Lua

## 基本用法
项目根目录配置相关信息

```c
local config = {
    connections = 1,
    duration = 5,
    threads = 1,
    timeout = 8000,  
    host = "127.0.0.1",
    port = 17793,
    url = "http://127.0.0.1:17788",
}
```
### connections 连接数
### duration 压测持续时间
### threads 线程数
### timeout 超时时间(毫秒)

输出：
Running 5s test skynet
  1 threads and actually 1 connections
  Thread Stats       Avg       Min       Max
    Latency       16.11 ms    16.11 ms    16.11 ms
  1 requests 3 responses in 5061.40 ms, 9.49 kb read
Requests/sec:      0.20
Responses/sec:      0.59
Transfer/sec:       1.88 kb
