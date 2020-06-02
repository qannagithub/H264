LIBPATH= ../avc-src-0.14/avc/build/obj/
#CC = gcc
CC=arm-linux-gcc
#VPATH = src:$(LIBPATH)
server_arm:server_arm.o convert.o $(LIBPATH)*.o
	$(CC)  -static -o server_arm $^ -lm
convert.o:convert.h
	$(CC) -c convert.c
.c.o:
clean:
	@rm -f *.o 
	@rm server_arm					
clean1: 
	@rm -f $(LIBPATH)*.o
