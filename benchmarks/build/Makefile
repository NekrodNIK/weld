
default:	build

clean:
	rm -rf Makefile objs

.PHONY:	default clean

build:
	$(MAKE) -f objs/Makefile

install:
	$(MAKE) -f objs/Makefile install

modules:
	$(MAKE) -f objs/Makefile modules

upgrade:
	/usr/local/nginx/sbin/nginx -t

	kill -USR2 `cat /dev/null`
	sleep 1
	test -f /dev/null.oldbin

	kill -QUIT `cat /dev/null.oldbin`

.PHONY:	build install modules upgrade
