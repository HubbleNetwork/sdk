SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR ?= $(abspath ../../../../../../..)
HUBBLE_NETWORK_SDK ?= $(abspath ../../../../)
NAME = ble-beacon

# Build directory for all artifacts
BUILD_DIR ?= build

include $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/imports.mak

CC = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"
LNK = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"

SYSCONFIG_GUI_TOOL = $(dir $(SYSCONFIG_TOOL))sysconfig_gui$(suffix $(SYSCONFIG_TOOL))
SYSCFG_CMD_STUB = $(SYSCONFIG_TOOL) --compiler ticlang --product $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/.metadata/product.json
SYSCFG_GUI_CMD_STUB = $(SYSCONFIG_GUI_TOOL) --compiler ticlang --product $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/.metadata/product.json
SYSCFG_FILES := $(shell $(SYSCFG_CMD_STUB) --listGeneratedFiles --listReferencedFiles --output $(BUILD_DIR) ble-beacon.syscfg)

SYSCFG_C_FILES = $(filter %.c,$(SYSCFG_FILES))
SYSCFG_H_FILES = $(filter %.h,$(SYSCFG_FILES))
SYSCFG_OPT_FILES = $(filter %.opt,$(SYSCFG_FILES))

# Enable verbose output by setting VERBOSE=1
V := @
ifeq ($(VERBOSE), 1)
  V :=
endif

OBJECTS = $(addprefix $(BUILD_DIR)/, \
	  app_main.obj \
	  app_hubble_ble_adv.obj \
	  common_Services_dev_info_dev_info_service.obj \
	  common_BLEAppUtil_bleapputil_task.obj \
	  common_BLEAppUtil_bleapputil_init.obj \
	  common_BLEAppUtil_bleapputil_process.obj \
	  common_BLEAppUtil_bleapputil_stack_callbacks.obj \
	  common_lib_opt_ctrl_opt_adv_nconn.obj \
	  common_Startup_osal_icall_ble.obj \
	  common_Startup_rom_init.obj \
	  common_Drivers_NV_crc.obj \
	  common_Drivers_NV_nvocmp.obj \
	  common_iCall_icall_cc23x0.obj \
	  common_iCall_icall_user_config.obj \
	  common_iCall_icall_POSIX.obj \
	  common_config_ble_user_config.obj \
	  common_Profiles_simple_gatt_simple_gatt_profile.obj \
	  common_BLE_SysStat_blesysstat.obj \
	  $(patsubst %.c,%.obj,$(notdir $(SYSCFG_C_FILES))))


CFLAGS += -I. \
	  -I../.. \
	  -Isrc/ \
	  -I$(BUILD_DIR) \
	  "-I$(HUBBLE_NETWORK_SDK)/include" \
	  "-I$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source" \
	  "-I$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti" \
	  "-I$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/common/cc26xx" \
	  "-I$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/posix/ticlang" \
	  "-I$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/third_party/freertos/include" \
	  "-I$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/third_party/freertos/portable/GCC/ARM_CM0" \
	  "-I$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/kernel/freertos" \
	  $(addprefix @,$(SYSCFG_OPT_FILES)) \
	  "@$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/ble/stack_util/config/build_components.opt" \
	  "@$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/ble/stack_util/config/factory_config.opt" \
	  -DICALL_NO_APP_EVENTS \
	  -mthumb \
	  -mlittle-endian \
	  -std=gnu99 \
	  -ffunction-sections \
	  -g \
	  -Oz \
	  -DCC23X0 \
	  -DNVOCMP_NWSAMEITEM=1 \
	  -DNVOCMP_NVPAGES=6 \
	  -DFREERTOS \
	  -DNVOCMP_POSIX_MUTEX \
	  -Wunused-function \
	  -gdwarf-3 \
	  -mcpu=cortex-m0plus \
	  -march=thumbv6m \
	  -mfloat-abi=soft \
	  -Wno-pointer-sign

LFLAGS += -Wl,--diag_wrap=off \
	  -Wl,--display_error_number \
	  -Wl,-x \
	  -Wl,-c \
	  -Wl,-m,$(BUILD_DIR)/$(NAME).map \
	  -Wl,--rom_model \
	  -Wl,--warn_sections \
	  -L$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source \
	  -L$(TICLANG_ARMCOMPILER)/lib \
	  $(BUILD_DIR)/ti_utils_build_linker.cmd.genlibs \
	  $(BUILD_DIR)/cc2340_freertos.cmd \
	  -llibc.a

all: $(BUILD_DIR) postbuild

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@cp cc2340_freertos.cmd $(BUILD_DIR)

.PHONY: postbuild
postbuild: $(BUILD_DIR)/$(NAME).out
	$(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/tools/common/crc_tool/crc_tool patch-image --elf $(BUILD_DIR)/$(NAME).out --symbol-prefix ti_utils_build_GenMap_sym_CRC_CCFG -o $(BUILD_DIR)/$(NAME).out
	$(TICLANG_ARMCOMPILER)/bin/tiarmobjcopy -O ihex $(BUILD_DIR)/$(NAME).out $(BUILD_DIR)/$(NAME).hex

.INTERMEDIATE: syscfg
$(SYSCFG_FILES): syscfg
	@ echo generation complete

syscfg: ble-beacon.syscfg $(BUILD_DIR)
	@ echo Generating configuration files...
	$(V) $(SYSCFG_CMD_STUB) --output $(BUILD_DIR) $<


# Helpful hint that the user needs to use a standalone SysConfig installation
$(SYSCONFIG_GUI_TOOL):
	$(error $(dir $(SYSCONFIG_TOOL)) does not contain the GUI framework \
        necessary to launch the SysConfig GUI.  Please set SYSCONFIG_TOOL \
        (in your SDK's imports.mak) to a standalone SysConfig installation \
        rather than one inside CCS)

syscfg-gui: ble-beacon.syscfg $(SYSCONFIG_GUI_TOOL)
	@ echo Opening SysConfig GUI
	$(V) $(SYSCFG_GUI_CMD_STUB) $<


define C_RULE
$(BUILD_DIR)/$(basename $(notdir $(1))).obj: $(1) $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $$@
	$(V) $(CC) $(CFLAGS) -c $$< -o $$@
endef

$(foreach c_file,$(SYSCFG_C_FILES),$(eval $(call C_RULE,$(c_file))))


# HubbleNetwork SDK

include $(HUBBLE_NETWORK_SDK)/port/freertos/hubblenetwork-sdk.mk


OBJECTS += $(BUILD_DIR)/app_hubble_ble_ti.obj $(patsubst %.c,$(BUILD_DIR)/%.obj,$(notdir $(HUBBLENETWORK_SDK_SOURCES)))

define HUBBLE_RULE
$(BUILD_DIR)/$(basename $(notdir $(1))).obj: $(1) $(BUILD_DIR)
	@ echo Building $$@
	$(V) $(CC) $(CFLAGS) $(HUBBLENETWORK_SDK_FLAGS) -c $$< -o $$@
endef

$(foreach c_file,$(HUBBLENETWORK_SDK_SOURCES),$(eval $(call HUBBLE_RULE,$(c_file))))

$(BUILD_DIR)/app_hubble_ble_ti.obj: src/hubble_ble_ti.c $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) $(HUBBLENETWORK_SDK_FLAGS) -c $< -o $@

# HubbleNetwork SDK END

$(BUILD_DIR)/app_main.obj: src/main.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) $(HUBBLENETWORK_SDK_FLAGS) -c $< -o $@


$(BUILD_DIR)/common_BLE_SysStat_blesysstat.obj: $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/ble/stack_util/health_toolkit/src/ble_sys_stat.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/common_Services_dev_info_dev_info_service.obj: $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/ble/services/dev_info/src/dev_info_service.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/common_BLEAppUtil_bleapputil_task.obj: $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/ble/app_util/framework/src/bleapputil_task.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/common_BLEAppUtil_bleapputil_init.obj: $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/ble/app_util/framework/src/bleapputil_init.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/common_BLEAppUtil_bleapputil_process.obj: $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/ble/app_util/framework/src/bleapputil_process.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/common_BLEAppUtil_bleapputil_stack_callbacks.obj: $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/ble/app_util/framework/src/bleapputil_stack_callbacks.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/common_lib_opt_ctrl_opt_adv_nconn.obj: $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/ble/stack_util/lib_opt/src/ctrl_opt_adv_nconn.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@


$(BUILD_DIR)/common_Startup_osal_icall_ble.obj: src/common/Startup/osal_icall_ble.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/common_Startup_rom_init.obj: $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/ble/stack_util/lib_opt/rom_init.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/common_Drivers_NV_crc.obj: $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/common/nv/crc.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/common_Drivers_NV_nvocmp.obj: $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/common/nv/nvocmp.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/common_iCall_icall_cc23x0.obj: $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/ble/stack_util/icall/app/src/icall_cc23x0.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/common_iCall_icall_user_config.obj: $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/ble/stack_util/icall/app/src/icall_user_config.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/common_config_ble_user_config.obj: $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/ble/app_util/config/src/ble_user_config.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/app_hubble_ble_adv.obj: src/hubble_ble_adv.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) $(HUBBLENETWORK_SDK_FLAGS) -c $< -o $@

$(BUILD_DIR)/common_Profiles_simple_gatt_simple_gatt_profile.obj: $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/ble/profiles/simple_gatt/src/simple_gatt_profile.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/common_iCall_icall_POSIX.obj: $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/source/ti/ble/stack_util/icall/app/src/icall_POSIX.c $(SYSCFG_H_FILES) $(BUILD_DIR)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(NAME).out: $(OBJECTS)
	@ echo linking $@
	$(V) $(LNK) -Wl,-u,_c_int00 $(OBJECTS)  $(LFLAGS) -o $@

clean:
	@ echo Cleaning...
	$(V) $(RM) -rf $(BUILD_DIR) > $(DEVNULL) 2>&1
	$(V) $(RM) $(call SLASH_FIXUP,$(SYSCFG_FILES)) > $(DEVNULL) 2>&1
