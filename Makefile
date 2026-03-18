# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: uanglade <uanglade@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/09/16 02:15:35 by uanglade          #+#    #+#              #
#    Updated: 2026/03/18 02:37:18 by uanglade         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

CXX := g++
CXXFLAGS := -g3
SRC := ./main.cpp ./Backend.cpp ./Vulkan.cpp
SHADER_SRC := ./shaders/triangle.frag ./shaders/triangle.vert
SHADER_OBJ := $(addsuffix .spv,$(SHADER_SRC))
INCS := -I./vk-bootstrap/src/ -I/usr/include/libdrm
OBJ := $(SRC:.cpp=.o)
NAME := dynamic-touchbar

# Rules
all: $(NAME)

$(NAME): $(OBJ) $(SHADER_OBJ)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJ) -L./vk-bootstrap/ -lvk-bootstrap -lvulkan -ldrm

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCS) -c $< -o $@

%.spv: %
	glslang -V $<  -o $@


fclean: clean
	rm -f $(NAME)

clean:
	rm -f $(OBJ)
	rm -f $(SHADER_OBJ)

re: fclean all

.PHONY: all clean fclean re
