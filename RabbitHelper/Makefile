TARGET     = rabbitHelper.a
SRC_DIR    = ./src
MAKE_DIR   = ../

#DO NOT EDIT BELOW

include $(MAKE_DIR)/include_$(COMPILER).mk

CFLAGS    +=
INCLUDES  += -I./src/includes
DEFINES   +=
LIBS      +=
LFLAGS    +=

#CONFIGURE BUILD SYSTEM
BUILD_DIR  = ./$(COMPILER)
Q         ?= @

VPATH     = $(SRC_DIR)
OBJ      += $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o,$(wildcard $(SRC_DIR)/*.c))
OBJ      += $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o,$(wildcard $(SRC_DIR)/*.cpp))
OBJ      += $(patsubst $(SRC_DIR)/%.pas, $(BUILD_DIR)/%.o,$(wildcard $(SRC_DIR)/*.pas))

CPPFLAGS := $(CPPFLAGS) $(DEFINES) $(INCLUDES) 

$(TARGET): $(BUILD_DIR) $(OBJ) 
	@echo "===>  CREATE STATIC LIB  $(STATIC_TARGET_LIB)"
	${Q}${AR} -rv $(TARGET) $(OBJ)

tags:
	@echo "===>  GENERATE  TAGS"
	${Q}ctags -R

$(BUILD_DIR):
	@mkdir $(BUILD_DIR)


#PATTERN RULES
$(BUILD_DIR)/%.o:  %.c
	@echo "===>  COMPILE  $@"
	${Q}$(CC) -c  $(CFLAGS) $(ANSI_CFLAGS) $(CPPFLAGS) $< -o $@
	${Q}$(CC) $(CPPFLAGS) -MT $(@:.d=.o) -MM  $< > $(BUILD_DIR)/$*.d

$(BUILD_DIR)/%.o:  %.cpp
	@echo "===>  COMPILE  $@"
	$(Q)$(CXX) -c  $(CXXFLAGS)  $(CPPFLAGS) $< -o $@
	$(Q)$(CXX) $(CPPFLAGS) -MT $(@:.d=.o) -MM  $< > $(BUILD_DIR)/$*.d

$(BUILD_DIR)/%.o:  %.pas
	@echo "===>  ASSEMBLE  $@"
	$(Q)$(PAS) -i x86-64 -o $(BUILD_DIR)/$*.s $<  '$(DEFINES)'
	$(Q)$(AS) $(ASFLAGS)  $(BUILD_DIR)/$*.s -o $@

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



