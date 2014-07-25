CXX := g++
CPPFLAGS := -I. -Wall
CXXFLAGS := -Os

AR := ar
LDFLAGS := -s
LDLIBS := -lutil -lz

RM := rm

SRCS := \
  apt-pkg/acquire-item.cc \
  apt-pkg/acquire-method.cc \
  apt-pkg/acquire-worker.cc \
  apt-pkg/acquire.cc \
  apt-pkg/algorithms.cc \
  apt-pkg/aptconfiguration.cc \
  apt-pkg/cachefile.cc \
  apt-pkg/cachefilter.cc \
  apt-pkg/cacheset.cc \
  apt-pkg/cdrom.cc  \
  apt-pkg/cdromutl.cc \
  apt-pkg/clean.cc \
  apt-pkg/cmndline.cc \
  apt-pkg/configuration.cc \
  apt-pkg/crc-16.cc \
  apt-pkg/debindexfile.cc \
  apt-pkg/deblistparser.cc \
  apt-pkg/debmetaindex.cc \
  apt-pkg/debrecords.cc \
  apt-pkg/debsrcrecords.cc \
  apt-pkg/debsystem.cc \
  apt-pkg/debversion.cc \
  apt-pkg/depcache.cc \
  apt-pkg/dpkgpm.cc \
  apt-pkg/edsp.cc \
  apt-pkg/error.cc \
  apt-pkg/fileutl.cc \
  apt-pkg/gpgv.cc \
  apt-pkg/hashes.cc \
  apt-pkg/hashsum.cc \
  apt-pkg/indexcopy.cc \
  apt-pkg/indexfile.cc \
  apt-pkg/indexrecords.cc \
  apt-pkg/init.cc \
  apt-pkg/install-progress.cc \
  apt-pkg/md5.cc \
  apt-pkg/mmap.cc \
  apt-pkg/netrc.cc \
  apt-pkg/orderlist.cc \
  apt-pkg/packagemanager.cc \
  apt-pkg/pkgcache.cc \
  apt-pkg/pkgcachegen.cc \
  apt-pkg/pkgrecords.cc \
  apt-pkg/pkgsystem.cc \
  apt-pkg/policy.cc \
  apt-pkg/progress.cc \
  apt-pkg/sha1.cc \
  apt-pkg/sha2_internal.cc \
  apt-pkg/sourcelist.cc \
  apt-pkg/srcrecords.cc \
  apt-pkg/strutl.cc \
  apt-pkg/tagfile.cc \
  apt-pkg/update.cc \
  apt-pkg/upgrade.cc \
  apt-pkg/vendor.cc \
  apt-pkg/vendorlist.cc \
  apt-pkg/version.cc \
  apt-pkg/versionmatch.cc \
  apt-private/acqprogress.cc \
  apt-private/private-cachefile.cc \
  apt-private/private-cacheset.cc \
  apt-private/private-download.cc \
  apt-private/private-install.cc \
  apt-private/private-main.cc \
  apt-private/private-output.cc \
  main.cc

OBJS := $(subst .cc,.o,$(SRCS))
PROG := apt-resolve-dep

all: $(PROG)

clean:
	$(RM) -f $(PROG) $(OBJS)

$(PROG): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

MAKEFLAGS += --no-builtin-rules
.SUFFIXES:
