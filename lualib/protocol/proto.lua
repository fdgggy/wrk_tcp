local parser = require "sprotoparser"
local core = require "sproto.core"
local sproto = require "sproto"

local loader = {}

function loader.register(index, ...)
	local proto_name = {...}
	local proto_text = ""

	for i = 1, #proto_name do
		local filename = "./lualib/protocol/"..proto_name[i]..".proto"
		local f = assert(io.open(filename), "Can't open sproto file")
		local data = f:read("a")
		f:close()
		proto_text = proto_text..data
	end

	local ok, sp = pcall(parser.parse, proto_text)
	if not ok then
	    error("sproto parse fail in "..table.concat(proto_name, ",").." , "..sp)
	    return
	end

	local csp = core.newproto(sp)
	core.saveproto(csp, index)
end

function loader.load(index)
	local csp = core.loadproto(index)
	--  no __gc in metatable
	return sproto.sharenew(csp)
end

return loader