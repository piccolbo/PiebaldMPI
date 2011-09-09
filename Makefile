REXEC = R
RCOMMAND = CMD
RBUILD = build
RINSTALL = INSTALL
RCHECK = check
RPDF = Rd2dvi
TARGET = PiebaldMPI_0.1.0.tar.gz

# subdirectories
RSOURCE = R
RDOCUMENTS = man
RDATA = data

# file types
RDFILES =$(wildcard man/*.Rd)
RFILES = $(wildcard R/*.R)

help:
	@echo "Please use \`make <target>' where <target> is one of"
	@echo ""	
	@echo "BUILDS"
	@echo ""
	@echo "  build         create an OpenMx binary for unix systems (no cross-compilation)"
	@echo ""		
	@echo "INSTALL"
	@echo ""	
	@echo "  install       build and install OpenMx on this machine"
	@echo ""
	@echo "CLEANING"
	@echo ""	
	@echo "  clean      remove all files from the build directory"
	@echo "  veryclean  remove all files from the build directory and all *~ files"

internal-build: build/$(TARGET)

build/$(TARGET): $(RFILES) $(RDFILES)
	cd $(RBUILD); $(REXEC) $(RCOMMAND) $(RBUILD) ..

common-build: clean internal-build
	cd $(RBUILD); $(REXEC) $(RCOMMAND) $(RINSTALL) --build $(TARGET)

common-build32: clean internal-build
	cd $(RBUILD); $(REXEC) --arch i386 $(RCOMMAND) $(RINSTALL) --build $(TARGET)

common-build64: clean internal-build
	cd $(RBUILD); $(REXEC) --arch x86_64 $(RCOMMAND) $(RINSTALL) --build $(TARGET)

common-buildppc: clean internal-build
	cd $(RBUILD); $(REXEC) --arch ppc $(RCOMMAND) $(RINSTALL) --build $(TARGET)

build32: common-build32

build64: common-build64

buildppc: common-buildppc

build: common-build

srcbuild: clean internal-build

winbuild: common-build

winbuild-biarch:
	cd $(RBUILD); $(REXEC) $(RCOMMAND) $(RINSTALL) --force-biarch --build $(TARGET)
	
install: clean internal-build
	cd $(RBUILD); $(REXEC) $(RCOMMAND) $(RINSTALL) $(TARGET)

check: internal-build
	cd $(RBUILD); $(REXEC) $(RCOMMAND) $(RCHECK) $(TARGET)

rproftest:
	$(REXEC) --vanilla --slave < $(RPROFTESTFILE)

testdocs:
	$(DOCTESTGEN)
	$(REXEC) --vanilla --slave < $(DOCTESTFILE)

test:
	$(REXEC) --vanilla --slave --cpus=$(CPUS) < $(TESTFILE)
	
nightly:
	$(REXEC) --vanilla --slave < $(NIGHTLYFILE)	

failtest:
	$(REXEC) --vanilla --slave < $(FAILTESTFILE)

memorytest:
	$(REXEC) -d "valgrind --tool=memcheck --leak-check=full --suppressions=inst/tools/OpenMx.supp --quiet" --vanilla --slave < $(MEMORYTESTFILE)

clean:
	rm -rf $(RBUILD)/*
	rm -rf models/passing/temp-files/*
	rm -rf models/failing/temp-files/*

veryclean: clean
	find . -name "*~" -exec rm -rf '{}' \;
