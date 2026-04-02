# RabbitCT

3D Volume reconstruction from CT images using backprojection

To build:

1. Edit Makefile and include\_<COMPILER>.mk to your needs.
2. make

To clean objects:
make clean

To clean all:
make distclean

Look into run-bench to see how RabbitRunner is called.

Example of usage:

./run-bench LolaOMPSSE-V3 4

Pinning is done using likwid-pin. It must be in your path. Of course
you can pin by other means, just adapt run-bench to your needs.

To turn on checking of the results comment in the appropriate line in
run-bench.

The same for outputting the final volume data to be viewed with a RAW image application.
