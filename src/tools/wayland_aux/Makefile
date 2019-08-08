THIS_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
XDGSHELL := $(THIS_DIR)/../../third_party/wayland-protocols/src/unstable/xdg-shell/xdg-shell-unstable-v6.xml $(wildcard *.py)

all: x.in.svg x.de.svg l.in.svg l.de.svg fuzz_actions.proto
	@echo $(XDG_SHELL)

fuzz_actions.proto: $(XDGSHELL)
	python main.py --spec $< -t proto > $@

%.in.svg: %.in.gv
	fdp -Tsvg $< -o $@

%.de.svg: %.de.gv
	dot -Tsvg $< -o $@

x.in.gv: $(XDGSHELL)
	python main.py --spec $< -t interfaces -x > $@

l.in.gv: $(XDGSHELL)
	python main.py --spec $< -t interfaces > $@

x.de.gv: $(XDGSHELL)
	python main.py --spec $< -t deps -x > $@

l.de.gv: $(XDGSHELL)
	python main.py --spec $< -t deps > $@

clean:
	rm -f *.svg *.gv
	rm -rf __pycache__
	rm -f fuzz_actions.proto
