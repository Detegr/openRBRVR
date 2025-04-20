RBRDIR = I:\\RBR2
VSEXE  = C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\devenv.exe

release: compile_commands.json
	@zig build --release=fast --prefix $(RBRDIR) --prefix-exe-dir Plugins --prefix-lib-dir Plugins

debug: compile_commands.json
	@zig build --prefix $(RBRDIR) --prefix-exe-dir Plugins --prefix-lib-dir Plugins

debugger:
	@start "" debug.bat "$(VSEXE)" "$(RBRDIR)"

compile_commands.json:
	@zig build cdb

distclean:
	rm -rf .zig-cache

.PHONY: debug release debugger run distclean
