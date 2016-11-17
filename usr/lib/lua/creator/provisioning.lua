#!/usr/bin/lua

module("creator.provisioning", package.seeall)

local ubus = require "ubus"
local creatorUtils = require "creator.utils"
local conn = ubus.connect();

function isProvisioningDaemonRunning()
    local res, status = conn:call("provisioning-daemon", "getState", {})
    if (status == nil) then
        return true
    end
    return false
end

function startProvisionignDaemon()
    luci.sys.exec("/usr/bin/provisioning_daemon_appd -d -r")
    os.execute("sleep 1")
    if  (isProvisioningDaemonRunning()) then
        return true
    end
    error({code = 1, message = "Unable to start provisioning daemon.", data = nil})
end

function stopProvisioningDaemon()
    luci.sys.exec("killall -9 provisioning_daemon_appd")
    if isProvisioningDaemonRunning() then
        error({code = 1, message = "Unable to stop provisioning daemon.", data = nil})
    else
        return true
    end
end

function getProvisioningDaemonState()
    local res, status = conn:call("provisioning-daemon", "getState", {})
    if (status == nil) then
        return res
    elseif (status == 4) then
        error({code = status, message = "Provisioning daemon is not running or has been started without '-r' param.", data = nil})
    else
        error({code = status, message = "ubus error", data = nil})
    end

end

function startProvisioning()
    local res, status = conn:call("provisioning-daemon", "startProvision", {})
    if (status == nil) then
        return true
    end
    error({code = 1, message = "Unable to start provisioning daemon.", data = nil})
end

function selectClicker(clickerID)
    local res, status = conn:call("provisioning-daemon", "select", {clickerID = clickerID})
    if (status == nil) then
        return true
    elseif (status == 4) then
        error({code = status, message = "Provisioning daemon is not running or has been started without '-r' param.", data = nil})
    elseif (status == 2) then
        error({code = status, message = "Invalid clickerID. Selected clicker may no longer be connected.", data = nil})
    else
        error({code = status, message = "ubus error", data = nil})
    end

    return false

end
