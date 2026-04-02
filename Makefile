TARGETS   = RabbitHelper
TARGETS  := $(TARGETS) RabbitRunner
ifdef MIC
TARGETS  := $(TARGETS) LolaMIC-OMP-scalar
TARGETS  := $(TARGETS) LolaMIC-OMP-autovec
TARGETS  := $(TARGETS) LolaMIC-OMP-pragmasimd
TARGETS  := $(TARGETS) LolaMIC-INTRIN
TARGETS  := $(TARGETS) LolaMIC-ASM
TARGETS  := $(TARGETS) LolaMIC-ISPC
else
TARGETS  := $(TARGETS) LolaOMPSSE-V1
TARGETS  := $(TARGETS) LolaOMPSSE-V2
TARGETS  := $(TARGETS) LolaOMPAVX
TARGETS  := $(TARGETS) LolaOMPAVX2-gather
TARGETS  := $(TARGETS) LolaOMPAVX2-pairwise
TARGETS  := $(TARGETS) LolaISPC-sse2-i32x4
TARGETS  := $(TARGETS) LolaISPC-sse2-i32x8
TARGETS  := $(TARGETS) LolaISPC-avx1.1-i32x8
TARGETS  := $(TARGETS) LolaISPC-avx1.1-i32x16
TARGETS  := $(TARGETS) LolaISPC-avx2-i32x8
TARGETS  := $(TARGETS) LolaISPC-avx2-i32x16
TARGETS  := $(TARGETS) LolaISPC-avx-tasks
TARGETS  := $(TARGETS) LolaOMP-scalar-SSE
TARGETS  := $(TARGETS) LolaOMP-scalar-AVX
TARGETS  := $(TARGETS) LolaOMP-scalar-AVX2
TARGETS  := $(TARGETS) LolaOMP-autovec-SSE
TARGETS  := $(TARGETS) LolaOMP-autovec-AVX
TARGETS  := $(TARGETS) LolaOMP-autovec-AVX2
TARGETS  := $(TARGETS) LolaOMP-pragmasimd-SSE
TARGETS  := $(TARGETS) LolaOMP-pragmasimd-AVX
TARGETS  := $(TARGETS) LolaOMP-pragmasimd-AVX2
endif

export COMPILER=ICC

all:  $(TARGETS) end


$(TARGETS): 
	@echo ""
	@echo "********************************"
	@echo " Build $@ "
	@echo "********************************"
	$(MAKE) -C $@ $(MAKECMDGOALS)

end:
	@echo ""
	@echo "********************************"
	@echo ' RabbitRunnerCT build complete! '
	@echo "********************************"


clean: $(TARGETS)



distclean: $(TARGETS)
	@echo "===>  DIST CLEAN"
	@rm -f *.dat*


#.SILENT:

.PHONY: clean distclean  $(TARGETS)

.NOTPARALLEL:

