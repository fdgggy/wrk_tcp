local proto = require "proto_util"
local config = require "config"
local util = require "util"

local M = class("login_client")

M.STATE_LOGIN = 1
M.STATE_LOGIN_RES= 2

M.STATE_NONE = 3

M.login_web_data = {}

function M:ctor(fd)
    self.fd = fd
    self.status = self.STATE_LOGIN
    self.state_func = {
        self.login,
    }
    self.msg_handle = {
        ["login_rsp"] = self.login_rsp,
    }
end

function M:start(account)
    self.login_web_data = account
    proto.init()
end

function M:login()
    self.status = self.STATE_LOGIN_RES
    return proto.encode_msg("login", { userid=self.login_web_data.userid, token=self.login_web_data.token, app_ver="1.0", hotfix_ver=0 })
end

function M:login_rsp(msg)
    if msg.code == 0 then
        self.status = self.STATE_NONE
    else
        print("login_rsp failed, code:"..msg.code)
    end
end

---------------------------------------------------------------
function M:writeable()
    if self.status == self.STATE_NONE then
        return "", -1, true
    end

    local handle = self.state_func[self.status]
    if handle then
        return handle(self)
    else
        return "", -1
    end
end

function M:readable(type, proto_id, client_id, session, proto_msg)
    if proto_id == 9500 or proto_id == 125 then
        return -1
    end
    local proto, msg = proto.decode_msg(type, proto_id, client_id, session, proto_msg)
    print("proto:"..proto)

    local handle = self.msg_handle[proto]
    if handle then
        handle(self, msg)
        return 0
    else
        return -1
    end
end

return M

