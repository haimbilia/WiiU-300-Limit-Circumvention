#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/wut/share/wut_rules

TARGET      := MockTitle
BUILD       := build
SOURCES     := source
DATA        :=
INCLUDES    :=
ICON        := assets/icon.png
TV_SPLASH   := assets/tv-splash.png
DRC_SPLASH  := assets/drc-splash.png

CFLAGS      := -g -Wall -O2 -ffunction-sections $(MACHDEP)
CFLAGS      += $(INCLUDE) -D__WIIU__ -D__WUT__
CXXFLAGS    := $(CFLAGS)
ASFLAGS     := -g $(ARCH)
LDFLAGS     := -g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map)
LIBS        := -lwut
LIBDIRS     := $(PORTLIBS) $(WUT_ROOT)

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

export LD       := $(CC)
export OFILES   := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export APP_NAME := Title Limit Test
export APP_SHORTNAME := Limit Test
export APP_AUTHOR := Wii U Menu Title Limit project
export APP_ICON := $(TOPDIR)/$(ICON)
export APP_TV_SPLASH := $(TOPDIR)/$(TV_SPLASH)
export APP_DRC_SPLASH := $(TOPDIR)/$(DRC_SPLASH)

.PHONY: $(BUILD) clean all
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@rm -fr $(BUILD) $(TARGET).wuhb $(TARGET).rpx $(TARGET).elf $(TARGET).map
else
.PHONY: all
DEPENDS := $(OFILES:.o=.d)
all: $(OUTPUT).wuhb
$(OUTPUT).wuhb: $(OUTPUT).rpx
$(OUTPUT).rpx: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)
endif
