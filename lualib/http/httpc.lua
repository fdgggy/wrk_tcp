local httpc = {}

local function request(method, host, path, header, content)
	local header_content = ""
    if header then
		if not header.host then
			header.host = host
		end
		for k,v in pairs(header) do
			header_content = string.format("%s%s:%s\r\n", header_content, k, v)
		end
	else
		header_content = string.format("host:%s\r\n",host)
	end

	if content then
		return string.format("%s %s HTTP/1.1\r\n%scontent-length:%d\r\n\r\n%s", method, path, header_content, #content, content)
	else
		return string.format("%s %s HTTP/1.1\r\n%scontent-length:0\r\n\r\n", method, path, header_content)
	end
end

function httpc.get(host, path)
	return request("GET", host, path)
end

function httpc.post(host, path, header, body)
	return request("POST", host, path, header, body)
end

return httpc
