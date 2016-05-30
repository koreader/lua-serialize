all : serialize

linux : serialize.so

linuxlib: libserialize.a

serialize.so : serialize.cpp
	g++ -std=c++11 -Wall -g -o $@ -fPIC --shared $^

#serialize.so : serialize.c
#	gcc -Wall -g -o $@ -fPIC --shared $^

libserialize.a : serialize.o
	ar rcu $@ $^
	ranlib $@

serialize.o : serialize.c
	#g++ -std=c++11 -O2 -Wall -c -o $@ $^
	gcc -Wall -g -c -o $@ $^
serialize : serialize.c
	gcc -Wall -g -o $@.dll --shared $^ -I/usr/local/include -L/usr/local/bin -llua53

clean :
	rm -f serialize.so serialize.dll serialize.o libserialize.a
