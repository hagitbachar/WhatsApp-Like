cc = g++
flags = -Wextra -Wall -Wvla -std=c++11 -pthread -g
files_to_tar = README Makefile whatsappClient.cpp whatsappServer.cpp whatsappio.cpp whatsappio.h
errors=yes
tar_name = ex4.tar

all: whatsappServer whatsappClient 

whatsappServer.o: whatsappServer.cpp 
	$(cc) $(flags) -c $^ -o $@

whatsappClient.o: whatsappClient.cpp
	$(cc) $(flags) -c $^ -o $@

whatsappio.o: whatsappio.cpp whatsappio.h
	$(cc) $(flags) -c whatsappio.cpp

whatsappServer: whatsappServer.o whatsappio.o
	$(cc) $(flags) $^ -o $@

whatsappClient: whatsappClient.o whatsappio.o
	$(cc) $(flags) $^ -o $@

clean:
	rm -f *.o whatsappServer whatsappClient

tar:
	$@ -cvf $(tar_name) $(files_to_tar)

.PHONY: clean all
