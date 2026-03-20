# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: uanglade <uanglade@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/09/16 02:15:35 by uanglade          #+#    #+#              #
#    Updated: 2026/03/20 16:05:24 by uanglade         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

CXX := g++
CXXFLAGS := -g3
DFLAGS	= -MMD -MP -MF $(@:.o=.d)

INCS := ./_deps/vk-bootstrap/src \
		/usr/include/libdrm \
		./_deps/clay

BUILD_DIR := ./obj
SRC_DIR := ./src
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
LIB_DIR := ./_deps/vk-bootstrap/build/
LDD_FLAGS := -lvk-bootstrap -lvulkan -ldrm

SHADER_SRC := ./shaders/triangle.frag ./shaders/triangle.vert
SHADER_OBJ := $(addsuffix .spv,$(SHADER_SRC))

OBJS = $(addprefix $(BUILD_DIR)/,$(SRCS:%.cpp=%.o))
DEPS = $(addprefix $(BUILD_DIR)/,$(SRCS:%.cpp=%.d))

NAME := dynamic-touchbar

DEPS_DIR := _deps
VKBOOTSTRAP_DIR := $(DEPS_DIR)/vk-bootstrap
VKBOOTSTRAP_NAME := $(DEPS_DIR)/vk-bootstrap/build/libvk-bootstrap.a
VKBOOTSTRAP_URL := https://github.com/charles-lunarg/vk-bootstrap
VKBOOTSTRAP_VERSION := v1.4.344

CLAY_DIR := $(DEPS_DIR)/clay
CLAY_URL := https://github.com/nicbarker/clay.git
CLAY_VERSION := v0.14

# Rules
all: $(NAME)

-include $(DEPS)

$(NAME): $(OBJS) $(SHADER_OBJ)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS) $(LIB_DIR:%=-L%) $(LDD_FLAGS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DFLAGS) $(INCS:%=-I%) -o $@ -c $<

%.spv: %
	glslang -V $<  -o $@


fclean: clean
	rm -f $(NAME)

clean:
	rm -f $(OBJS)
	rm -f $(SHADER_OBJ)

re: fclean all

$(VKBOOTSTRAP_DIR):
	mkdir -p _deps
	cd _deps; \
	git clone --depth=1 --branch=$(VKBOOTSTRAP_VERSION) $(VKBOOTSTRAP_URL)

$(VKBOOTSTRAP_NAME): setup-vkbootstrap

setup-vkbootstrap: $(VKBOOTSTRAP_DIR)
	cd $(VKBOOTSTRAP_DIR); \
	mkdir build -p ; \
	cd build; \
	cmake ..; \
	make -j

$(CLAY_DIR):
	mkdir -p _deps
	cd _deps; \
	git clone --depth=1 --branch=$(CLAY_VERSION) $(CLAY_URL)

setup-clay: $(CLAY_DIR)


.PHONY: all clean fclean re
