tdm: main.c
	gcc main.c -g -o tdm

XDG_CONFIG_HOME ?= ${HOME}/.config

.PHONY: install uninstall config
config:
	mkdir -p "$(XDG_CONFIG_HOME)"
	cp example.conf "$(XDG_CONFIG_HOME)/tdm.conf"

install:
	cp tdm /usr/local/bin/tdm

uninstall:
	rm /usr/local/bin/tdm
