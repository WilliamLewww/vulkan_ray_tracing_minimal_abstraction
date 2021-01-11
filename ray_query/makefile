GLFW_PATH = /usr/local/glfw/glfw-3.3.2
VULKAN_SDK_PATH = /usr/local/vulkan/1.2.162.0/x86_64

EXEC = vulkan-c.out

SRCS := $(wildcard src/*.c)
OBJS := $(notdir $(SRCS:%.c=%.o))

SHADER_SRCS := $(wildcard src/shaders/*)
SHADER_OBJS := $(notdir $(SHADER_SRCS:%=%.spv))

CFLAGS =-I$(VULKAN_SDK_PATH)/include -I$(GLFW_PATH)/include
LDFLAGS =-L$(VULKAN_SDK_PATH)/lib -L$(GLFW_PATH)/build/src -lGL -lglfw3 -lX11 -lXrandr -lpthread -ldl -lm -lvulkan

$EXEC: $(OBJS) $(SHADER_OBJS)
	gcc $(CFLAGS) -o bin/$(EXEC) build/*.o $(LDFLAGS)

%.o: src/%.c
	gcc $(CFLAGS) -c $^ -o build/$@

%.spv: src/shaders/%
	glslc --target-env=vulkan1.2 $^ -o bin/$@
