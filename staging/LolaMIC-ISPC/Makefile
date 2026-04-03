TARGET   = LolaMIC-ISPC.so

SRC_DIR    = ./src
MAKE_DIR   = ../

#DO NOT EDIT BELOW

include $(MAKE_DIR)/include_$(COMPILER).mk

CXXFLAGS  += 
INCLUDES  += -I./src/includes -I../RabbitHelper/src/includes
DEFINES   += 
LIBS      +=../RabbitHelper/rabbitHelper.a  -ldl
LFLAGS    += 

#CONFIGURE BUILD SYSTEM
BUILD_DIR  = ./$(COMPILER)
Q         ?= @

VPATH     = $(SRC_DIR)
OBJ      = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o,$(wildcard $(SRC_DIR)/*.c))
OBJ      += $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o,$(wildcard $(SRC_DIR)/*.cpp))
#OBJ      += $(patsubst $(SRC_DIR)/%.pas, $(BUILD_DIR)/%.o,$(wildcard $(SRC_DIR)/*.pas))
OBJ      += $(patsubst $(SRC_DIR)/%.ispc, $(BUILD_DIR)/%.o,$(wildcard $(SRC_DIR)/*.ispc))

CPPFLAGS := $(CPPFLAGS) $(DEFINES) $(INCLUDES) 

$(TARGET): $(BUILD_DIR) $(OBJ) 
	@echo "===>  LINKING  $@"
	$(Q)$(CXX) $(LFLAGS) -o $(TARGET) $(OBJ) $(LIBS)

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


$(BUILD_DIR)/%.o:  %.ispc
	@echo "===>  COMPILE  $@"
ifdef MIC
	$(ISPC) $< --emit-c++ --target=generic-16 -o $(BUILD_DIR)/Lola.cpp \
		--c++-include-file=knc.h
	$(CC) -S -I includes $(CFLAGS) $(ANSI_CFLAGS) $(CPPFLAGS) \
		$(BUILD_DIR)/Lola.cpp -o $(BUILD_DIR)/Lola.s
	$(CC) -c -I includes $(CFLAGS) $(ANSI_CFLAGS) $(CPPFLAGS) \
		$(BUILD_DIR)/Lola.cpp -o $@
else
	$(Q)$(ISPC) $(ISPCFLAGS)  $< -o $@
	$(Q)$(ISPC) --emit-asm $(ISPCFLAGS)  $< -o $(BUILD_DIR)/$*.s
endif


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



