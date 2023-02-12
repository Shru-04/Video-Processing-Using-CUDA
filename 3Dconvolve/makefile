CC=gcc
CFLAGS=-c -Wall
LDFLAGS:=-lm

NVCC        = /usr/local/cuda/bin/nvcc
NVCC_FLAGS  = -g
LD_FLAGS    = -lcudart
EXE         = ppm
OBJ         = ppm.o


default: parallel

ppm.o: ppm.cu ppmKernel.cu
	$(NVCC) -c -o $@ ppm.cu $(NVCC_FLAGS)

parallel: $(OBJ)
	$(NVCC) $(OBJ) -o par_3D_conv $(LD_FLAGS)

clean:
	rm -rf *.o par_3D_conv ../outfiles/tmp*
