![](http://static.creatordev.io/logo.png)

# Provision Daemon for ci40 board.

Visit us at [forum.creatordev.io](http://forum.creatordev.io) for support and discussion


This project provides daemon which provides PSK and configuration needed by constrained device to be provisioned on [Device Server](http://creatordev.io/). This code cooperates with [Onboarding Scripts](https://github.com/CreatorDev/ci40-onboarding-scripts) and [Provision Library](https://github.com/CreatorDev/contiki-provisioning-library) which are separate project, more information about whole solution can be find [here](http://properLinkHere)

## Build status

Master
[![Build Status](https://travis-ci.org/CreatorDev/ci40-provision-daemon.svg?branch=master)](https://github.com/CreatorDev/ci40-provision-daemon)


Dev
[![Build Status](https://travis-ci.org/CreatorDev/ci40-provision-daemon.svg?branch=dev)](https://github.com/CreatorDev/ci40-provision-daemon)


## Pre requirements
To have this daemon working in proper way you have to ensure following things:

  * Board has configured wifi or cable connection, so internet is reachable.
  * Onboarding scripts are installed.
  * User finished onboarding procedure (account data are stored in ci40)
  * Both Mikro-E Clicker and ci40 board are using this same 6LowPan channel and PANID.

# Installation
When using Creator Image this package should be already installed. If you want to install it manually preferred way is to use opkg
`opkg install provisioning-daemon`.

## Configuration
Most of daemon configuration is done through config file located at `/etc/config/provisioning_daemon` however few of switches can be passed as an argument:

```
-v level - Set log level 1-5.
-l path - Set path which includes directory + file name in which logs should be stored.
-c path - Set path for configuration file.
-d - Run as daemon.
-r - Same as setting REMOTE_PROVISION_CTRL to true
```

Options which can be set in configuration file:

```
#URI at which Device Server is located.
BOOTSTRAP_URI="coaps://deviceserver.creatordev.io:15684"

#TCP port on which daemon is awaiting for connections.
PORT=49300

#Turns on/off possibility to control provision process through uBus commands.
#Default value is false
REMOTE_PROVISION_CTRL=true/false

#Turns on/off possibility to control provision process through buttons on ci40.
#Default value is true
LOCAL_PROVISION_CTRL=true/false

#This is address of default gateway. This value should be passed to method uip_ds6_defrt_add on constrained device, it will be passed to constrained device as one of config parameters.
#Maximum length 100 characters
DEFAULT_ROUTE_URI="..."

#This is address of DNS server. This value should be passed to method uip_nameserver_update on constrained device, it will be passed to constrained device as one of config parameters.
#Maximum length 100 characters
DNS_SERVER="..."

#This is name which should be used by constrained device. It should be used when initializing Awa client, it will be passed to constrained device as one of config parameters.
#Maximum length is 24 characters (including generated parts)
#To add dynamic part of name you can use following arguments:
# {t} - timestamp in alpha-numeric encoded form
# {i} - last parts of constrained device ip address
ENDPOINT_NAME_PATTERN="<static name>{t}{i}"

#Sets logging level of daemon. Following values are valid:
# 1 - FATAL ERROR
# 2 - ERROR
# 3 - WARNING
# 4 - INFO
# 5 - DEBUG (VERBOSE)
#default value is 3
LOG_LEVEL=3
```

## Usage with buttons on ci40
After setting value of `LOCAL_PROVISION_CTRL` to `1` it's possible to control provisioning process directly from ci40 without need to connect any terminal or application. For this purpose daemon will use on board LEDs and buttons. Here is description of process.
  * When any constrained device connects to Provision Daemon LED is turned on. So if for example three clickers connect, then three LEDs will turn on.
  * One of LEDs will slowly blink, this indicate that clicker is selected, you can also see that second LED on constrained device is turned on.
  * By pressing button 1 on ci40 you can switch selection. If you have several connected clickers you should notice that after changing LED on ci40, also LEDs on Clickers will turn on or off. At one moment only one clicker can be selected.
  * By pressing button 2 you requesting start of provisioning process with selected clicker. Currently selected LED on ci40 will start to blink rapidly.
  * If all LEDS started to blink simultaneously this indicate that Provisioning daemon encountered some critical error, unfortunately you need to look into log to see what happen. Some of most common problems are:
    * Lack of internet connectivity
    * Problems with account (no onboarding before provisioning?)
    * Problems with Device Server communication (timeout, can't be reached)

## Usage with Mobile application
To work with mobile application your smartphone needs to be in this same network as ci40 board. If in your config file parameter `REMOTE_PROVISION_CTRL` is set to `1`. You will be able to control process of provisioning from application. Please refer to documentation of project [Android Onboard App](https://github.com/CreatorDev/android-provisioning-onboard-app) for more information.

## Contributing
If you have a contribution to make please follow the processes laid out in [contributor guide](CONTRIBUTING.md).
