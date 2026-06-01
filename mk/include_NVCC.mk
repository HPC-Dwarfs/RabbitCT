CC = gcc

# CUDA compiler
NVCC = nvcc

# Linker: use nvcc so it can resolve CUDA runtime symbols
LD = $(NVCC)

ifeq ($(strip $(ENABLE_OPENMP)),true)
OPENMP   = -Xcompiler "-fopenmp"
endif

VERSION  = --version
CFLAGS   = -O3 -std=c++17 $(CUDA_ARCH) $(OPENMP)
LFLAGS   = $(OPENMP) -lm $(CUDA_ARCH)

DEFINES  +=  -DENABLE_CUDA -D_GNU_SOURCE -DRUNTIME_BACKEND_IS_CUDA
INCLUDES  =
LIBS      = -lcudart -lm
