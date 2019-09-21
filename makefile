mirror: main.o functions.o iheader.h
	gcc main.o functions.o -o mirror
main.o: main.c iheader.h
	gcc -c main.c -o main.o
functions.o: functions.c iheader.h
	gcc -c functions.c -o functions.o
clean:
	rm main.o functions.o
