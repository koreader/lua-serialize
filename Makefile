all : serialize

linux : serialize.so

mac : libserialize.dylib

serialize.so : serialize.c
	gcc -Wall -g -o $@ -fPIC --shared $^ -llua

libserialize.dylib : serialize.c
	gcc -Wall -g -o $@ -fPIC --shared $^ -llua.5.3  -I/usr/local/include/lua5.3

serialize : serialize.c
	gcc -Wall -g -o $@.dll --shared $^ -I/usr/local/include -L/usr/local/bin -llua53
