NINJA=build/lib/bin/ninja -C out/cur

all:
	@$(NINJA)

test: all
	cd libpng-1.6.28 && ../out/cur/pngtest

clean:
	@$(NINJA) -t clean

distclean:
	rm -Rf out/

.PHONY: all test clean distclean
