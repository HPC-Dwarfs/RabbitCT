CC   = nvcc
LD = $(CC)

ifeq ($(ENABLE_OPENMP),true)
OPENMP   = -Xcompiler "-fopenmp"
endif

ENABLE_CUDA = true

VERSION  = --version
CFLAGS   = -O3 -std=c++17 $(OPENMP) -g
LFLAGS   = $(OPENMP)
DEFINES  = -DENABLE_CUDA -D_GNU_SOURCE
INCLUDES =
LIBS     = -lm
