#
# Raspberry Pi RP2350 specific settings
##############################################################################
COMMON_VPATH += $(PLATFORM_PATH)/$(PLATFORM_KEY)/vendors/$(MCU_FAMILY)

ADEFS += -DCRT0_VTOR_INIT=1 \
         -DCRT0_EXTRA_CORES_NUMBER=0 \
         -DCRT0_INIT_VECTORS=1

CFLAGS += -DNDEBUG

EXTRAINCDIRS += $(PLATFORM_PATH)/$(PLATFORM_KEY)/vendors/$(MCU_FAMILY)
