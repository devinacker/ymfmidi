#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

TARGET		:=	ymfm-test
BUILD       :=  obj
SOURCES		:=	src ymfm/src
INCLUDES	:=	$(SOURCES) include

CFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))

CFLAGS	:=	-Wall \
			`pkg-config --cflags sdl2` \
			-Wno-sign-compare

CXXFLAGS	= $(CFLAGS) -std=c++14

ASFLAGS	:=	$(ARCH)
LDFLAGS	:=	`pkg-config --libs sdl2` \
			-Wl,-rpath=. 

ifeq ($(DEBUG),1)
  CFLAGS  += -O0 -g
  LDFLAGS += -g
else
  CFLAGS  += -O2
  LDFLAGS += -s
endif

ifeq ($(OS),Windows_NT)
  LDFLAGS += -mconsole
endif

OUTPUT	:=	$(CURDIR)/$(TARGET)

OFILES	:=	$(addprefix $(BUILD)/, $(CPPFILES:.cpp=.o) $(CFILES:.c=.o))

INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir))
CFLAGS   += $(INCLUDE)
CXXFLAGS += $(INCLUDE)

.PHONY: clean

#---------------------------------------------------------------------------------
$(OUTPUT):	$(OFILES)
#---------------------------------------------------------------------------------
	@echo linking $(notdir $@)
	@$(CXX) -o $@ $^ $(LDFLAGS) 

#---------------------------------------------------------------------------------
$(BUILD)/%.o: %.cpp
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

#---------------------------------------------------------------------------------
$(BUILD)/%.o: %.c
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET) $(OFILES)
 
