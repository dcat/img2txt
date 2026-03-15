#OPTIM   = -march=native -O3
CFLAGS  = -g -Iinc ${OPTIM}
LDFLAGS = -lm

all: img2txt
