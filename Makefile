basic: src/basic.c lib
	clang -Isrc src/basic.c -lcdns -Lbuild -o build/cdns-basic

lib: src/cdns.c src/cdns.h
	clang -Isrc src/cdns.c -c -o build/cdns.o
	ar rcs build/libcdns.a build/cdns.o
doc:
	doxygen
run-basic: basic
	build/cdns-basic
lint: src/basic.c src/cdns.c src/cdns.h
	cpplint src/basic.c src/cdns.c src/cdns.h

clean:
	rm -rf build
	mkdir build