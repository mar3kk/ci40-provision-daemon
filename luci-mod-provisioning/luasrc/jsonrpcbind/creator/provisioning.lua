#!/usr/bin/lua

module("luci.jsonrpcbind.creator.provisioning", package.seeall)
local creatorProvisioning = require "creator.provisioning"

function isProvisioningDaemonRunning()
    return creatorProvisioning.isProvisioningDaemonRunning()
end

function startProvisioningDaemon()
    return creatorProvisioning.startProvisionignDaemon()
end

function stopProvisioningDaemon()
    return creatorProvisioning.stopProvisioningDaemon()
end

function provisioningDaemonState()
    return creatorProvisioning.getProvisioningDaemonState()
end

function selectClicker(clickerID)
    return creatorProvisioning.selectClicker(clickerID)
end

function startProvisioning()
    return creatorProvisioning.startProvisioning()
end
