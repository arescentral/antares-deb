NINJA=ninja -C out/cur

all:
	@$(NINJA)

test: all
	cd libpng-1.6.28 && ../out/cur/pngtest

clean:
	@$(NINJA) -t clean

distclean:
	rm -Rf out/
	rm -f build/lib/scripts/gn

.PHONY: all test clean distclean
