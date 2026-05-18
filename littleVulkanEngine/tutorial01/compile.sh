glslangValidator -V shaders/simple_shader.vert -o shaders/simple_shader.vert.spv
glslangValidator -V shaders/simple_shader.frag -o shaders/simple_shader.frag.spv

# #!/bin/zsh

# # loop through vert and frag file
# for i in *.{vert,frag}; do
#   echo "Processing: " "$i" "${i}.spv";
#   glslc "$i" -o "${i}.spv";
# done