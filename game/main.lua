local proto = require "protocol.proto"
local netpack = require "netpack"
local config = require "config"
local http = require("socket.http")
local proto_util = require "proto_util"

local M = {}
local count = 0
local identify
local accounts ={}

local clients = {}

local queue

function M.get_account()
    count = count + 1
    return "wrkplayer"..identify.."_"..count
end

function M.init(id, connections)
    identify = id
    local start = os.time()

    for i=1, connections do
        local account = M.get_account()
        local url = string.format(config.loginMsg, config.loginAddr, account, "pw123456",config.appID, "Tourist", "", "mac", 1001)
        local result = http.request(url)
        local res = proto_util.decode_json_msg(result)
        if tonumber(res.code) ~= 0 then
            print("result:"..result)
            print("web_login failed res code:"..res.code)
            util.dumptable(res)
        end
        accounts[count] = res
    end
    local endt = os.time()
    print("login web thread id:"..id.." delta time:"..(endt-start).." "..#accounts)

    proto.register(1, "test")
    math.randomseed(os.time())
end

function M.connected(fd)
    if clients[fd] then
        print("lua connected error, exist the fd:"..(fd or 0))
        return 
    end
    local client = require(config.procedure)
    clients[fd] = client.new(fd)
    clients[fd]:start(table.remove(accounts))
end

local overcount = 0
function M.writeable(fd)
    local client = clients[fd]
    if not client then
        print("lua writeable failed, not exist the fd:"..(fd or 0))
        return
    end

    local data, proto_id, over = client:writeable()
    if over then
        overcount = overcount + 1 
        -- print("identify:"..identify.." over connect:"..overcount)
    end
    if proto_id == -1 then
        return data, 0
    end
    local msg, sz = netpack.packmsg(1, proto_id, 16, 0, data)
    return msg, sz
end

function M.readable(fd, buffer, sz)
    local MSG = {}

    local function dispatch_msg(fd, msg, sz)
        local type, proto_id, client_id, session, proto_msg = netpack.unpackmsg(msg, sz)

        local client = clients[fd]
        if not client then
            print("lua readable failed, not exist the fd:"..(fd or 0))
            return
        end
        local result = client:readable(type, proto_id, client_id, session, proto_msg)
        if result == 0 then
            return 0, 1
        end

        return -1, 1
    end

	MSG.data = dispatch_msg

    local function dispatch_queue()
        -- print("more data")
        local readhandle = -1
        local result = 0
        for fd, msg, sz in netpack.pop, queue do
            local res1, res2 = dispatch_msg(fd, msg, sz)
            if res1 == 0 then
                readhandle = 0
            end
            result = result + res2
        end
        return readhandle, result
    end
    MSG.more = dispatch_queue

    local q, type, fd, msg, sz = netpack.filter(queue, buffer, sz, fd)
    queue = q

    if type == "data" then
        return MSG[type](fd, msg, sz)
    elseif type == "more" then
        return MSG[type]()
    else
        -- print("chai bao:"..(type or ""))
        --拆包
        return -2, 0
    end
end

return M



