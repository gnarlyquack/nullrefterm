EXENAME := nullrefterm
DEBUGGER := lldb

.PHONY: build
build:
	./build.bash

.PHONY: run
run:
	../build/$(EXENAME)

.PHONY: debug
debug:
	$(DEBUGGER) ../build/$(EXENAME)
