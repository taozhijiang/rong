DEBUG ?= 1
CXX = g++
CXXFLAGS = -g -O0 -std=c++0x
SHELL = /bin/bash
VERSION = 1.0
EXTRAFLAGS  =  -I../source/Client/include -I../source/ -I../deps/xtra_rhelz.x/include/ -I../deps/xtra_rhelz.x/include/google -I../deps/roo 
EXTRAFLAGS += -L../deps/xtra_rhelz.x/libs/google/protobuf-2.5.0 -L../deps/xtra_rhelz.x/libs/google/ -L../deps/xtra_rhelz.x/libs/boost ../build/lib/libClient.a ../build/lib/libRoo.a -lglog_syslog -lboost_system -lboost_thread -lboost_chrono -lprotoc -lprotobuf -lrt -lpthread -lconfig++

#vpath %.cpp ./
TARGET_DIR=./bin

src = $(filter-out main.cpp, $(notdir $(wildcard *.cpp)))
exe = $(src:%.cpp=$(TARGET_DIR)/%)

all : $(exe)
.PHONY : all


$(exe) : $(TARGET_DIR)/%: %.cpp
	@test -d $(TARGET_DIR) || mkdir $(TARGET_DIR)
	$(CXX) -MMD $(CXXFLAGS) $< -o $@ $(EXTRAFLAGS) 

#check header for obj reconstruction
-include $(OBJDIR)/*.d

.PHONY : clean 
clean :	
	-rm -fr $(OBJDIR)
	-rm -fr $(TARGET_DIR)
