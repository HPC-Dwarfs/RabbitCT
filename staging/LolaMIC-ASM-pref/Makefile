TARGET   = LolaMIC-ASM-pref.so

SRC_DIR    = ./src
MAKE_DIR   = ../

#DO NOT EDIT BELOW

include $(MAKE_DIR)/include_$(COMPILER).mk

CFLAGS    +=
LFLAGS    +=
INCLUDES  += -I./src/includes -I../includes -I../RabbitHelper/src/includes
DEFINES   += 
LIBS      +=../RabbitHelper/rabbitHelper.a  -ldl

#CONFIGURE BUILD SYSTEM
BUILD_DIR  = ./$(COMPILER)
#Q         ?= @

VPATH     = $(SRC_DIR)
OBJ       = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o,$(wildcard $(SRC_DIR)/*.c))
OBJ      += $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o,$(wildcard $(SRC_DIR)/*.cpp))
OBJ      += $(patsubst $(SRC_DIR)/%.S, $(BUILD_DIR)/%.o,$(wildcard $(SRC_DIR)/*.S))

CPPFLAGS := $(CPPFLAGS) $(DEFINES) $(INCLUDES) 

$(TARGET): $(BUILD_DIR) $(OBJ) 
	@echo "===>  LINKING  $@"
	$(Q)$(CC) $(LFLAGS) -o $(TARGET) $(OBJ) $(LIBS)

tags:
	@echo "===>  GENERATE  TAGS"
	$(Q)ctags -R

$(BUILD_DIR):
	@mkdir $(BUILD_DIR)


#PATTERN RULES
$(BUILD_DIR)/%.o:  %.c
	@echo "===>  COMPILE  $@"
	$(Q)$(CC) -c  $(CFLAGS) $(ANSI_CFLAGS) $(CPPFLAGS) $< -o $@
	$(Q)$(CC) $(CPPFLAGS) -MT $(@:.d=.o) -MM  $< > $(BUILD_DIR)/$*.d

$(BUILD_DIR)/%.o:  %.cpp
	@echo "===>  COMPILE  $@"
	$(Q)$(CXX) -c  $(CXXFLAGS)  $(CPPFLAGS) $< -o $@
	$(Q)$(CXX) $(CPPFLAGS) -MT $(@:.d=.o) -MM  $< > $(BUILD_DIR)/$*.d

$(BUILD_DIR)/%.o:  %.S
	@echo "===>  ASSEMBLE  $@"
	$(Q)$(AS) $(ASFLAGS) $< -o $@

ifeq ($(findstring $(MAKECMDGOALS),clean),)
-include $(OBJ:.o=.d)
endif

.PHONY: clean distclean

clean:
	@echo "===>  CLEAN"
	@rm -rf $(BUILD_DIR)

distclean: clean
	@echo "===>  DIST CLEAN"
	@rm -f $(TARGET)



