CC  = gcc
CXX = g++
AS  = as
PAS = ./perl/AsmGen.pl 
ISPC = ispc

ANSI_CFLAGS  = -ansi
ANSI_CFLAGS += -std=c99
#ANSI_CFLAGS += -pedantic
#ANSI_CFLAGS += -Wextra

CFLAGS   =  -O2 -std=gnu99 -Wno-format  -fpic -fopenmp -g
CXXFLAGS =  -O2 -Wno-format  -fpic -fopenmp -g
ASFLAGS  = -g -gdward-2
CPPFLAGS = 
ISPCFLAGS =
LFLAGS   =  -shared  -g -fopenmp
DEFINES  = -D_GNU_SOURCE -DX86
DEFINES   += -DMAX_NUM_THREADS=128
DEFINES   += -DVECTORSIZE=8

INCLUDES = -I../includes
LIBS     =


