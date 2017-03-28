cmake_minimum_required(VERSION 3.5)
# Paths
########
include_directories(${STAGING_DIR} ${STAGING_DIR}/usr/include/glib-2.0)
ADD_SUBDIRECTORY(luci-mod-provisioning)
ADD_SUBDIRECTORY(provisioning-daemon)
ADD_SUBDIRECTORY(usr)

