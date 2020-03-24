# wrk_tcp
基于wrk二次开发的tcp压测工具，测试脚本为Lua

# 基本使用
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

