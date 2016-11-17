# Add executable targets
########################
ADD_EXECUTABLE(provisioning_daemon_appd provisioning_daemon.c clicker.c led.c connection_manager.c processing_queue.c clicker_sm.c utils.c ubus_agent.c provision_history.c)
# Add library targets
#####################
FIND_LIBRARY(LIB_LMC libletmecreate_core.so ${STAGING_DIR}/usr/lib)
FIND_LIBRARY(LIB_CONFIG libconfig.so ${STAGING_DIR}/usr/lib)
TARGET_LINK_LIBRARIES(provisioning_daemon_appd ${LIB_LMC} ${LIB_CONFIG} crypto ubus ubox json-c blobmsg_json)

# Add install targets
######################
INSTALL(TARGETS provisioning_daemon_appd RUNTIME DESTINATION bin)
