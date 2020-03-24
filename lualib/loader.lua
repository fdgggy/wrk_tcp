
package.path = "lualib/?.lua;game/?.lua;"..package.path
package.cpath = "luaclib/?.so;"..package.cpath

require "class"

local main = require "main"
return main

