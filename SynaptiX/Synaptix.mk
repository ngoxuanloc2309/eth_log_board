
APPFILES = 
# APPFILES += ${SYNAPTIXDIR}/apps/ThingsBoard-device/thingsboard-client.c 
APPFILES += ${SYNAPTIXDIR}/apps/app.c
APPFILES += ${SYNAPTIXDIR}/apps/ota/ota.c
APPFILES += ${SYNAPTIXDIR}/apps/button/button.c
APPFILES += ${SYNAPTIXDIR}/apps/settings/app_settings.c  
APPFILES += ${SYNAPTIXDIR}/apps/sntp/sntp_app.c
APPFILES += ${SYNAPTIXDIR}/apps/shell/shell_tcp.c
APPFILES += ${SYNAPTIXDIR}/apps/shell/shell_serial.c
APPFILES += ${SYNAPTIXDIR}/apps/shell/cli_shell_command.c
APPFILES += ${SYNAPTIXDIR}/apps/transparent/transparent.c
APPFILES += ${SYNAPTIXDIR}/apps/rf_app/rf_app.c
APPFILES += ${SYNAPTIXDIR}/apps/mqtt/mqtt_app.c
APPFILES += ${SYNAPTIXDIR}/apps/sx_485_io/sx_485_io.c
APPFILES += ${SYNAPTIXDIR}/apps/sx_420mA/sx_420mA.c
APPFILES += ${SYNAPTIXDIR}/apps/sx_metter/sx_metter.c
APPFILES += ${SYNAPTIXDIR}/apps/sx_ths/sx_ths.c
APPFILES += ${SYNAPTIXDIR}/apps/function/function.c
APPFILES += ${SYNAPTIXDIR}/apps/filesystem/file_io.c
APPFILES += ${SYNAPTIXDIR}/apps/modbus_slave/mbs_tcp.c
APPFILES += ${SYNAPTIXDIR}/apps/modbus_slave/mbs_usb.c
APPFILES += ${SYNAPTIXDIR}/apps/http_server/http_server.c

SERVICESFILES = ${SYNAPTIXDIR}/services/cJSON/cJSON_Utils.c \
		    	${SYNAPTIXDIR}/services/cJSON/cJSON.c \
				${SYNAPTIXDIR}/services/cqueue/cqueue.c \
 				${SYNAPTIXDIR}/services/logger/logger.c \
				${SYNAPTIXDIR}/services/shell/cli_shell.c \
				${SYNAPTIXDIR}/services/socket_server/socketserver.c \
				${SYNAPTIXDIR}/services/socket_client/socketclient.c \
				${SYNAPTIXDIR}/services/tcp_server/tcp_server.c \
				${SYNAPTIXDIR}/services/tcp_client/tcp_client.c \
				${SYNAPTIXDIR}/services/udp_server/udp_server.c \
				${SYNAPTIXDIR}/services/udp_client/udp_client.c\
				${SYNAPTIXDIR}/services/zigbee/zigbee.c\
				${SYNAPTIXDIR}/services/nanomodbus/nanomodbus.c\
				${SYNAPTIXDIR}/services/thingsboard/thingsboard-client.c \
				${SYNAPTIXDIR}/services/linklist/linklist.c \
				${SYNAPTIXDIR}/services/rpc/rpc.c \
				${SYNAPTIXDIR}/services/littlefs/lfs_util.c \
				${SYNAPTIXDIR}/services/littlefs/lfs.c \
				${SYNAPTIXDIR}/services/modbus/functions/mbfunccoils.c \
				${SYNAPTIXDIR}/services/modbus/functions/mbfuncdiag.c \
				${SYNAPTIXDIR}/services/modbus/functions/mbfuncdisc.c \
				${SYNAPTIXDIR}/services/modbus/functions/mbfuncholding.c \
				${SYNAPTIXDIR}/services/modbus/functions/mbfuncinput.c \
				${SYNAPTIXDIR}/services/modbus/functions/mbfuncother.c \
				${SYNAPTIXDIR}/services/modbus/functions/mbutils.c \
				${SYNAPTIXDIR}/services/modbus/functions/mb.c \
				${SYNAPTIXDIR}/services/fatfs/App/fatfs.c \
				${SYNAPTIXDIR}/services/fatfs/Target/user_diskio.c \
				${SYNAPTIXDIR}/services/fatfs/src/option/syscall.c \
				${SYNAPTIXDIR}/services/fatfs/src/diskio.c \
				${SYNAPTIXDIR}/services/fatfs/src/ff_gen_drv.c \
				${SYNAPTIXDIR}/services/fatfs/src/ff.c \
				${SYNAPTIXDIR}/services/fatfs/src/sram_diskio.c \
				${SYNAPTIXDIR}/services/mmb_serial/mmb_serial.c \
				${SYNAPTIXDIR}/services/sx_dev/sx_dev.c
UTILSFILES  =   ${SYNAPTIXDIR}/utils/str2hex.c

BOARDFILES = ${SYNAPTIXDIR}/board/board.c \
			 ${SYNAPTIXDIR}/board/spif/spif.c \
			 ${SYNAPTIXDIR}/board/qspif/w25q_mem.c

SYNAPTIXFILES = ${BOARDFILES} \
				${APPFILES} \
				${SERVICESFILES} \
				${UTILSFILES}