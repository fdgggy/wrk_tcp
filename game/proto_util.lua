local proto  = require ("protocol.proto")
local httpc = require ("http.httpc")
local json = require "cjson"
local util = require "util"

local pb

local M = {}

function M.init()
    pb = proto.load(1)
    if not pb then
        print("can not load proro config")
        return
    end
end

local function escape(s)
	return (string.gsub(s, "([^A-Za-z0-9_])", function(c)
		return string.format("%%%02X", string.byte(c))
	end))
end

-- local host = "127.0.0.1:17788";
-- local path = "/test_wrk?a=11";
function M.encode_get(host, path)
    local request = httpc.get(host, path)
    return request, #request
end

function M.encode_post(host, path, form)
    local header = {
		["content-type"] = "application/x-www-form-urlencoded"
    }
    local body = {}
	for k,v in pairs(form) do
		table.insert(body, string.format("%s=%s",escape(k),escape(v)))
    end
    
    local request = httpc.post(host, path, header, table.concat(body , "&"))

    return request, #request
end

function M.encode_post_json(host, path, form)
    local header = {
		["content-type"] = "application/json; charset=utf-8;"
    }

    local body = json.encode(form)
    local request = httpc.post(host, path, header, body)

    return request, #request
end

function M.decode_json_msg(body)
    return json.decode(body)
end

function M.encode_msg(proto_name, msg)
    local data, proto_id = pb:request_encode(proto_name, msg)
    return data, proto_id
end

function M.decode_msg(type, proto_id, client_id, session, proto_msg)
    local proto_name, msg_params
    if type > 0 then
        msg_params, proto_name = pb:request_decode(proto_id, proto_msg)
        proto_name = proto_name.."_req"
    else
        msg_params, proto_name = pb:response_decode(proto_id, proto_msg)
        proto_name = proto_name.."_rsp"
    end

    -- print("type:"..type.." proto_id:"..proto_id.." proto_name:"..proto_name)
    -- util.dumptable(msg_params)

    return proto_name, msg_params
end

return M
