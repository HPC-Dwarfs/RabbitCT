CC  = icc
CXX  = icpc
AS  = as
PAS = ./perl/AsmGen.pl 

ANSI_CFLAGS  =# -strict-ansi
ANSI_CFLAGS += -std=c99

CFLAGS   =  -O3   -Wno-format -vec-report=0 -openmp $(MIC)
CXXFLAGS =  -O3   -Wno-format -vec-report=0 $(MIC)
ASFLAGS  = # -g -gdwarf-2
CPPFLAGS =
LFLAGS   = -g -ldl -openmp $(MIC)
DEFINES  = -D_GNU_SOURCE
#enable this option to build likwid-bench with marker API for likwid-perfCtr
#DEFINES  += -DPERFMON
#DEFINES += -DLIKWID_PERFMON

INCLUDES = -I../includes/
LIBS     =  ../RabbitHelper/rabbitHelper.a -lm #-L/home/hpc/unrz/unrz278/new-likwid/mic/lib/ -llikwid
