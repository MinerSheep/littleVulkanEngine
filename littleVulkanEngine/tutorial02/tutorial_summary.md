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

5_2 - **Command buffers overview**, primary command buffers are submitted to a queue for execution and can call 2ndary cmd buffers
We create a commandBuffer pool, similar to a ThreadPool so that we can reuse allocated data
Each commandBuffer is recorded and attributed to a frameBuffer so there's no resource confusion, it has 2 clear values for color and depth
We bind these commandBuffers to the graphicsPipeline, and each drawFrame, request the commandBuffer w the next frameBuffer

6 - **Create vertex buffer**, we use a model that takes Vertex structs as vertices to create its vertexBuffer and memory
We map the vertexBuffer to its memory and then memcpy it into a data ptr for us to use
When creating the model, we just specify the vertices in a constructor list, now the model can be reused for binding and drawing with the command buffer

7 - **Adding color output to vertex shader**, including in vec3 color; out vec3 fragColor;
We can send color data based on a vertice to a frag shader.  If a position is not exactly on a vertice, it takes a barycentric coordinate between the vertices colors.
In order to utilize this in code, all we have to do is alter the Vertex class within our model to include a color glm::vec3.  This way whenever we initialize a vertex, include the color.
offsetof is a SUPER COOL macro you can use to determine byte offset of member variable within a class so use it.

8 - **Resizing window means recreating swap chain & viewport** - This happens because swap chain works with a const window size
Callback listens to GLFW Window changes
Instead of initializing the swapchain and command buffer at startup, we check if its dirty inside the DrawFrame function
Using a dynamic viewport so that the graphics pipeline is no longer dependent  (on the swapchain's dimensions)

RenderPass is a BLUEPRINT for the GraphicsPipeline to know what to expect from a FrameBuffer

9 - **Push constants and animations** - Allows us to communicate data to both shaders, problem is that its a CONST size & shared between both shaders
When defining push constant range in CREATE PIPELINE...
Stage flags - Which shaders will utilize push constant data
Offset - where does data for this stage begin
Size - How many bytes of data are we reading

Record data SimplePushConstantData push{}; and submit using vkCmdPushConstants --- IMPORTANT: PUSHDATA SHOULD BE 16 BYTE ALIGNED
Shaders read x y - - / r g b -

Add a frame counter integer to the push constant offset and it makes an animation!

10 - **Rendering game objects** - Storing translation, rotation, scale within entities, we can pass these to vulkan for rendering
vec4(push.transform * position + push.offset, 0.0, 1.0); where push.transform is the scale * rotation,  offset is the translation