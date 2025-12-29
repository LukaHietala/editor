#!/usr/bin/env lua
-- Prints hidden chars for debugging purposes

local RED = "\27[31m"
local RST = "\27[0m"

local escapes = {
	["\n"] = RED .. "\\n" .. RST .. "\n",
	["\t"] = RED .. "\\t" .. RST .. "\t",
	["\r"] = RED .. "\\r" .. RST .. "\r",
    -- TODO: add more if necessary
}

local input = arg[1] and io.open(arg[1], "r") or io.stdin

for line in input:lines("L") do
	local out = line:gsub(".", function(c)
	if escapes[c] then return escapes[c] end

	local b = c:byte()
	if b < 32 or b > 126 then
		return string.format(RED .. "\\x%02X" .. RST, b)
	end
		return c
	end)
	io.write(out)
end
