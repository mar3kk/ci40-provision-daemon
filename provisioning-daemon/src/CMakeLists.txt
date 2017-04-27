# Add executable targets
########################
file (GLOB src *.c*)
ADD_EXECUTABLE(provisioning_daemon_appd ${src})

# Add library targets
#####################
FIND_LIBRARY(LIB_LMC libletmecreate_core.so ${STAGING_DIR}/usr/lib)
FIND_LIBRARY(LIB_CONFIG libconfig.so ${STAGING_DIR}/usr/lib)
FIND_LIBRARY(LIB_GLIB libglib-2.0.so ${STAGING_DIR}/usr/lib)
TARGET_LINK_LIBRARIES(provisioning_daemon_appd ${LIB_LMC} ${LIB_CONFIG} ${LIB_GLIB} crypto ubus ubox json-c blobmsg_json)

# Add install targets
######################
INSTALL(TARGETS provisioning_daemon_appd RUNTIME DESTINATION bin)
