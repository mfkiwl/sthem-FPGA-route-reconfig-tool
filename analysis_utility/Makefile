#export QMAKE = qmake
export QMAKE = qmake CONFIG+=debug
#export QMAKE = /opt/Qt/5.12.0/gcc_64/bin/qmake CONFIG+=debug
export CLANGPP = clang++
export LLVM_CONFIG = llvm-config

.PHONY: all
all:
	mkdir -p bin
	mkdir -p analysis_tool/build
	cd analysis_tool/build && $(QMAKE) ..
	cd analysis_tool/build && $(MAKE)
	cp analysis_tool/build/analysis_tool bin
	cd tulipp_source_tool && $(MAKE)
	cp tulipp_source_tool/tulipp_source_tool bin
	cd llvm_ir_parser && $(MAKE)
	cp llvm_ir_parser/llvm_ir_parser bin
	cd wrapper && $(MAKE)
	cp wrapper/tulipp* wrapper/wrapper wrapper/toolsettings.sh bin

.PHONY : clean
clean :
	rm -rf analysis_tool/build
	rm -rf bin
	cd tulipp_source_tool && $(MAKE) clean
	cd llvm_ir_parser && $(MAKE) clean
