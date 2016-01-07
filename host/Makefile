
all: libml libbdf


libml: usb.c color.c color.h
	gcc -shared -fPIC -std=gnu11 -Wall -lm -o libml.so -g usb.c color.c -lusb-1.0 

libbdf: bdf.c font.c font.h color.c color.h
	gcc -shared -fPIC -std=gnu11 -Wall -lm -o libbdf.so -g bdf.c font.c color.c

clean:
	rm libbdf.so libml.so
