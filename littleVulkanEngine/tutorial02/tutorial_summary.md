0 - **making window**, use GLFW_NO_API to tell it to disable GLFW API so we can use vulkan

1 - **making pipeline**, ifstream(std::ios::ate) means seek to EOF immediately, std::ios::binary disables unwanted text conversions
Careful about releasing device before the pipeline is destroyed

2 - **Making devices and COMPILING SHADERS** USING glslangValidator -V simple_shader.vert -o simple_shader.vert.spv
vector<char> can become vector<int> with reinterpret_cast

3 - **define pipeline info**, like how the topology is triangle_list to draw triangles from vertices, we define the viewport that renderer sees, rasterization which defines how objects are defined (front and back), color blending, depth testing
we are also making the shaderStages and putting that in the pipelineInfo
This creates the graphics pipeline, however we are missing PIPELINE LAYOUT & RENDER PASS

5 - **Swap chain logic**, keeps multiple frame buffers for the GPU to work on, renders the most recent frame buffer
Can define PresentMode to MAILBOX, IMMEDIATE, OR FIFO.
Fixed issue where viewportInfo from PipelineConfigInfo referenced local struct variables like viewport and scissor
^ THAT leads to an issue where silent shallow copying occurs and new objects reference viewport & scissor from the old object   
In FirstApp, we create PIPELINE LAYOUT & RENDER PASS, along with changing pipelineConfig to a std::unique_ptr