#!/usr/bin/lua
module("luci.controller.creator.rpc_provisioning", package.seeall)

function index()

	local rpc = node("rpc")

	rpc.notemplate = true

	entry({"rpc", "creator_provisioning"}, call("rpc_creator"))

end

function rpc_creator()
	local creator  = require "luci.jsonrpcbind.creator.provisioning"
	local jsonrpc = require "luci.creator.jsonrpc"
	local http    = require "luci.http"
	local ltn12   = require "luci.ltn12"

	http.prepare_content("application/json")
	ltn12.pump.all(jsonrpc.handle(creator, http.source()), http.write)
end
