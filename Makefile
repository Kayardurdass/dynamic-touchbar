# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: uanglade <uanglade@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/09/16 02:15:35 by uanglade          #+#    #+#              #
#    Updated: 2026/03/18 02:49:17 by uanglade         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

CXX := g++
CXXFLAGS := -g3
DFLAGS	= -MMD -MP -MF $(@:.o=.d)

INCS := ./vk-bootstrap/src/ \
		/usr/include/libdrm

BUILD_DIR := ./obj
SRC_DIR := ./src
SRCS := $(wildcard $(SRC_DIR)/*.cpp)

SHADER_SRC := ./shaders/triangle.frag ./shaders/triangle.vert
SHADER_OBJ := $(addsuffix .spv,$(SHADER_SRC))

OBJS = $(addprefix $(BUILD_DIR)/,$(SRCS:%.cpp=%.o))
DEPS = $(addprefix $(BUILD_DIR)/,$(SRCS:%.cpp=%.d))

NAME := dynamic-touchbar

# Rules
all: $(NAME)

-include $(DEPS)

$(NAME): $(OBJS) $(SHADER_OBJ)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS) -L./vk-bootstrap/ -lvk-bootstrap -lvulkan -ldrm

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DFLAGS) $(INCS:%=-I%) -o $@ -c $<

%.spv: %
	glslang -V $<  -o $@


fclean: clean
	rm -f $(NAME)

clean:
	rm -f $(OBJ)
	rm -f $(SHADER_OBJ)

re: fclean all

.PHONY: all clean fclean re
