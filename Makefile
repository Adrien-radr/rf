.PHONY: tags lib rf clean post_build

all: tags rf lib post_build

# LIBRARY Defines
GLFW_INCLUDE=ext/glfw/include
GLEW_INCLUDE=ext/glew/include
CJSON_INCLUDE=ext/cjson
STB_INCLUDE=ext/
GLEW_LIB=ext/glew
CJSON_LIB=ext/cjson


SRC_DIR=src/
SRCS= \
	render.cpp \
	utils.cpp \
	log.cpp \
	context.cpp \
	ui.cpp
OBJ_DIR=obj/

INCLUDE_FLAGS=-I$(SRC_DIR) -Iext/ -I$(GLEW_INCLUDE) -I$(GLFW_INCLUDE) -I$(CJSON_INCLUDE)

##################################################
# NOTE - WINDOWS BUILD
##################################################
ifeq ($(OS),Windows_NT) # MSVC
GLEW_OBJECT=ext/glew/glew.obj
GLEW_TARGET=ext/glew/glew.lib
STB_TARGET=ext/stb.lib
STB_OBJECT=ext/stb.obj
CJSON_OBJECT=ext/cjson/cjson.obj
CJSON_TARGET=ext/cjson/cJSON.lib
GLFW_LIB=ext/glfw/build/x64/Release

OBJS=$(patsubst %.cpp,$(OBJ_DIR)%.obj,$(SRCS))

CC=cl /nologo
STATICLIB=lib /nologo
LINK=/link /MACHINE:X64 -subsystem:console,5.02 /INCREMENTAL:NO /ignore:4204
GENERAL_CFLAGS=-Gm- -EHa- -GR- -EHsc
CFLAGS=-MT $(GENERAL_CFLAGS) /W1 -I$(SRC_DIR)
DLL_CFLAGS=-MD -DLIBEXPORT $(GENERAL_CFLAGS)
DEBUG_FLAGS=-DDEBUG -Zi -Od -W1 -wd4100 -wd4189 -wd4514
RELEASE_FLAGS=-O2 -Oi
VERSION_FLAGS=$(DEBUG_FLAGS)

LIB_FLAGS=/LIBPATH:ext /LIBPATH:$(GLEW_LIB) /LIBPATH:$(GLFW_LIB) /LIBPATH:$(CJSON_LIB) stb.lib cjson.lib libglfw3.lib glew.lib opengl32.lib user32.lib shell32.lib gdi32.lib

TARGET=bin/rf.lib
PDB_TARGET=bin/rf.pdb


$(GLEW_TARGET): 
	@$(CC) $(CFLAGS) $(RELEASE_FLAGS) -DGLEW_STATIC -I$(GLEW_INCLUDE) -c ext/glew/src/glew.c -Fo$(GLEW_OBJECT)
	@$(STATICLIB) $(GLEW_OBJECT) -OUT:$(GLEW_TARGET)
	@rm $(GLEW_OBJECT)

$(CJSON_TARGET):
	@$(CC) $(CFLAGS) $(RELEASE_FLAGS) -I$(CJSON_INCLUDE) -c ext/cjson/cjson.c -Fo$(CJSON_OBJECT)
	@$(STATICLIB) $(CJSON_OBJECT) -OUT:$(CJSON_TARGET)
	@rm $(CJSON_OBJECT)

$(STB_TARGET):
	@$(CC) -O2 -Oi -c ext/stb.cpp -Fo$(STB_OBJECT)
	@$(STATICLIB) $(STB_OBJECT) -OUT:$(STB_TARGET)
	@rm $(STB_OBJECT)

$(OBJ_DIR)%.obj: $(SRC_DIR)%.cpp
	@$(CC) $(CFLAGS) $(VERSION_FLAGS) $(INCLUDE_FLAGS) -DGLEW_STATIC -c $< -Fo$@

rf: $(STB_TARGET) $(GLEW_TARGET) $(CJSON_TARGET) $(OBJS)
	@$(CC) $(CFLAGS) $(VERSION_FLAGS) $(INCLUDE_FLAGS) -DGLEW_STATIC $(OBJS) $(LINK) $(LIB_FLAGS) /OUT:$(TARGET) /PDB:$(PDB_TARGET)

##################################################
# NOTE - LINUX BUILD
##################################################
else # GCC
GLEW_OBJECT=ext/glew/glew.o
GLEW_TARGET=ext/glew/libglew.a
GLFW_LIB=ext/glfw/build/src
CJSON_OBJECT=ext/cjson/cJSON.o
CJSON_TARGET=ext/cjson/libcJSON.a
STB_TARGET=ext/libstb.a
STB_OBJECT=ext/stb.o

OBJS=$(patsubst %.cpp,$(OBJ_DIR)%.o,$(SRCS))

CC=g++
CFLAGS=-Wno-unused-variable -Wno-unused-parameter -Wno-write-strings -D_CRT_SECURE_NO_WARNINGS -std=c++11 -pedantic
DEBUG_FLAGS=-g -DDEBUG -Wall -Wextra
RELEASE_FLAGS=-O2
VERSION_FLAGS=$(DEBUG_FLAGS)

LIB_FLAGS=-Lext/ -L$(GLEW_LIB) -L$(GLFW_LIB) -L$(CJSON_LIB) -lstb -lcJSON -lcjson -lglfw3 -lglew \
		  -lGL -lX11 -lXinerama -lXrandr -lXcursor -lm -ldl -lpthread

TARGET=bin/librf.a

$(GLEW_TARGET): 
	@echo "AR $(GLEW_TARGET)"
	@$(CC) $(CFLAGS) $(RELEASE_FLAGS) -DGLEW_STATIC -I$(GLEW_INCLUDE) -c ext/glew/src/glew.c -o $(GLEW_OBJECT)
	@ar rcs $(GLEW_TARGET) $(GLEW_OBJECT)
	@rm $(GLEW_OBJECT)

$(CJSON_TARGET):
	@echo "AR $(CJSON_TARGET)"
	@$(CC) $(CFLAGS) $(RELEASE_FLAGS) -I$(CJSON_INCLUDE) -c ext/cjson/cJSON.c -o $(CJSON_OBJECT)
	@ar rcs $(CJSON_TARGET) $(CJSON_OBJECT) 
	@rm $(CJSON_OBJECT)

$(STB_TARGET):
	@echo "AR $(STB_TARGET)"
	@$(CC) -O3 -fno-strict-aliasing -c ext/stb.cpp -o $(STB_OBJECT)
	@ar rcs $(STB_TARGET) $(STB_OBJECT)
	@rm $(STB_OBJECT)

$(OBJ_DIR)%.o: $(SRC_DIR)%.cpp
	@echo "CC $@"
	@$(CC) $(CFLAGS) $(VERSION_FLAGS) $(INCLUDE_FLAGS) -DGLEW_STATIC -c $< -o $@

rf: $(STB_TARGET) $(GLEW_TARGET) $(CJSON_TARGET) $(OBJS)
	@echo "CC $(TARGET)"
	#$(CC) $(CFLAGS) $(VERSION_FLAGS) -shared -fPIC -DGLEW_STATIC $(OBJS) $(INCLUDE_FLAGS) $(LIB_FLAGS) -o $(TARGET)
	ar rcs $(TARGET) $(OBJS)

endif
#$(error OS not compatible. Only Win32 and Linux for now.)

##################################################
##################################################

post_build:
	@rm -f *.lib *.exp

clean:
	rm $(OBJS)
	rm $(TARGET)

clean_ext:
	rm $(CJSON_TARGET)
	rm $(GLEW_TARGET)

tags:
	@ctags --c++-kinds=+p --fields=+iaS --extra=+q $(SRC_DIR)*.cpp $(SRC_DIR)*.h $(LIB_INCLUDES)
