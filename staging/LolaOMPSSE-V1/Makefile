#===========================================================================
#
#     Filename:  Makefile
#
#     Description:  Central Makefile
#
#     Version:  <VERSION>
#     Created:  <DATE>
#
#     Author:  Jan Treibig (jt), jan.treibig@gmail.com
#     Company:  RRZE Erlangen
#     Project:  likwid
#     Copyright:  Copyright (c) 2010, Jan Treibig
#
#     This program is free software; you can redistribute it and/or modify
#     it under the terms of the GNU General Public License, v2, as
#     published by the Free Software Foundation
#    
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
#    
#     You should have received a copy of the GNU General Public License
#     along with this program; if not, write to the Free Software
#     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#===========================================================================

TARGET   = LolaOMPSSE-V1.so

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
Q         ?= @

VPATH     = $(SRC_DIR)
OBJ       = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o,$(wildcard $(SRC_DIR)/*.c))
OBJ      += $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o,$(wildcard $(SRC_DIR)/*.cpp))
OBJ      += $(patsubst $(SRC_DIR)/%.pas, $(BUILD_DIR)/%.o,$(wildcard $(SRC_DIR)/*.pas))

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



