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

11 - **Refactoring** - Strip out implementation from first_app into renderer component and system 

12 - **3d Rendering** - Switching to a 3d rendering system requires affine transformations (which are when you add a column to the matrix with 1 at the bottom, the 1 represents a point i.e. an offset translation)
We update our matrix computation to use glm's library and also to change vertex input in model (+ its attribute descriptions)
Adjusting the shaders again, we can make each face of the cube a different color
We are currently using INTRINSIC rotations which means rotation axis change based on ordering (L -> R   y -> x -> z)
Doing it backwards makes it EXTRINSIC meaning rotation axis doesn't change apparently?  (I don't get this)

13 - **Homogenous Coordinates** - To build an orthographic perspective, we use a frustrum shape
This computes the height of a 3d object on screen by taking ny/z  where n is distance viewer to screen and z is distance screen to obj
In vertex shader code, gl_Position = push.transform * vec4(position, 1.0);, the w component divides the output by 1.0
To replace it with z, we just need to alter the last row of the identity matrix to be 0,0,1,0 instead (this will take the z component and use it for our w, ignoring the 1.0)

Add orthographic projection computation by multiplying
push.transform = camera.getProjection() * obj.transform.mat4();
Currently this will scale the object based on window size, to remove this, make orthographic viewing aspect ratio = window aspect ratio

14 - **Camera Transform** - In order to view the world through the lens of a movable camera, every object should have a relative position to the camera
Currently the camera is static, we do the model transform, perspective projection transform onto cam, and map to viewport.
However, if camera is dynamic, we can move every object in the world relative to the camera by positioning the camera at origin.
Shift everything with the camera, then do the perspective projection.

15 - **Game Loop Timing** - Game Loops require i/o to be consistent across monitors with different refresh rates
On top of other systems like AI, Physics, and Audio, they all need to match the timing of the renderer
We can do this for now by making a camera controller and passing in dt during the application run function
dt can be obtained using std::chrono's high resolution clock

**To see renditions of the engine from before here, open tutorial 2 folder in littleVulkanEngine**

16 - **Index & Staging Buffers (maybe issue here)** - Rendered objects like Squares are made up of triangles and they share vertices to make those triangles
6 faces splits into 12 triangles = 12 * 3 = 36 vertices, 30 of which being duplicates (causes memory abuse for more complex models)
This is why we use an index buffer, to tell the GPU to render index {0,1,2} of vertices list {0,1,2,3,4,5}

To make memory as fast as it can be, we need to set VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT which is not compatible with mutable buffer data
Therefore, we will use a staging buffer, which is what our buffer is now, but we send to the buffer and then delete the stagingBuffer
This works best with static models, dynamic models will negate any performance gain we get from this

17 - **Adding libraries and loading 3d models** - Add libraries to the project using libs folder and -Ilibs in Makefile
pass in a filepath to tinyobj::LoadObj, we can read the shape.mesh.indices for lines attributed to the vertex, texcoord, & normal

However, we are not using Index Buffer during this to reduce model memory consumption, it's not as straightforward here
Since Waveform objs, they use separate vertices for the position, normal, and texcoord.  We need all these vertices grouped together
(I did this by keeping track of unique vertices in a map
putting each element in indexbuffer 
towards the unique vertice index)

18 - **Adding lighting** - Lighting is a very complicated formula to calculate, but for diffuse lighting, it can be simplified
Diffuse lighting is calculated using ||a||||b||cos 0 which, when both vectors are normalized, turns out to cos 0 (capped from 1 to 0)
Therefore we can use this to calculate the light % are every point on the model in regards to a light source

We will start by using a skylight which is only a direction, no position
Also if we scale the obj unevenly, it can look wrong.  This can be fixed with forced uniform scaling, OR by passing in the normal matrix which we do here

This tutorial also shows how to export from Blender in order to achieve smooth vs flat shading (different visual effect, smooth is cheaper)

19 - **Uniform buffers** - Uniform buffers are used to replace push constants in shaders.  They work the same as Vertex buffers.
Its important to note that 16 KB is how large you can go for mobile devices, must abide by the device's minOffsetAlignment.
We also need to align by the device's nonCoherentAtomSize for incoherent buffers (where we have 2 buffers in one that are offsetted)
This is because the host and device memory have a min size they can sync between
I fixed this by taking the max value between minOffsetAlignment & nonCoherentAtomSize though it can also be solved with lcm or with separate uniform buffers.

20 - **Descriptors** - Descriptors point to buffer data for the render pipeline to use.  They must be organized into SETS.
At application start, we must define the Descriptor Set Layout.  When allocating the Set, we must allocate from a Descriptor Pool.

We use a custom builder class that **returns a reference to itself so we can chain initialization**.
When we write to each buffer/image, we can use the writer class.  Pass in the descriptorInfo() for each buffer.

Descriptor Pools are given a fixed size and can only allocate so many descriptors of a given type.  For example, globalPool is given maxSets = MAX_FRAMES_IN_FLIGHT and poolSize = (VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)

This allows allocate 2 uniform buffer descriptors, but can't allocate 2 more uniform buffer descriptors unless it is specified during initialization (addPoolSize(TYPE_UNIFORM_BUFFER), 4);

FINALLY, in our SimpleRenderSystem, before we do rendering for all gameObjects, call vkCmdBindDescriptorSets to bind descriptor set ONCE and then it is reused for all gameObjects that frame.

Important to note that if a descriptorSet is overwritten, every descriptorSet that comes after also needs to be rewritten!

There is A LOT going on in this one so definitely study later

21 - **Point Lights** - point lighting hits vertices at different angles compared to a global skylights
The intensity of the light is defined as intensity / dist^2
You can make it more artistic by adding constant, linear, and quadratic scaling to the denominator

Making point lights work with our object means we need worldPosition for the object which is the modelMatrix * position
float attenuation = 1.0 / dot(directionToLight, directionToLight); // distance squared
^ multiply this by colorLight.xyz * colorLight.w (its intensity)

I ran into an issue where the window would not launch and it said WARN: COPY MODE on the window title
The issue behind this had nothing to do with the code, but rather the wsl driver malfunctioning
An easy fix was to do wsl --shutdown before rebooting wsl and the program

22 - **Fragment Lighting** - currently the floor looks weird, and thats because it only has 4 vertices
The color for the floor is interpolating between the color of the 4 vertices at each corner

**A Vulkan Story** - If Vulkan rendering is taking a long time and you notice damage to the PCI (motherboard to gpu bus)
Likely its because buffers are being stored and rendered through the cpu
The biggest giveaway for this is to keep in mind that GPU excels at rendering a million objects. CPU doesnt

24 - **Billboards** - A billboard is an object like in Baldis Basics, it always faces the camera and can be used for light objects
We can make a new shader for this that draws 2d images to the screen by using gl_vertexIndex with an offset for each vertex
The point light system draws 6 vertice indexes
It uses the point light shader which gets the ubo's lightPosition value and draws EACH VERTEX on an offset around the position
To make it into a circle, we can use the point's distance to discard; any value that is > 1.0 (radius)