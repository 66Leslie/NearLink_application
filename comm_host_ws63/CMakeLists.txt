set(SOURCES_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/comm_host_ws63.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wifi_sta_connect_ws63.c
    ${CMAKE_CURRENT_SOURCE_DIR}/udp_server_ws63.c
    ${CMAKE_CURRENT_SOURCE_DIR}/oled_ssd1306_ws63.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_bsp_nfc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_client.c
)

set(SOURCES "${SOURCES}" ${SOURCES_LIST} PARENT_SCOPE)
