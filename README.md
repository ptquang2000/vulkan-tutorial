# Hello Triangle

## Build

### Arch Linux

```bash
cmake -S . -B bin -G Ninja
cmake --build bin
```

### Windows

Install VulkanSDK from https://sdk.lunarg.com/sdk/download/1.4.321.1/windows/vulkansdk-windows-X64-1.4.321.1.exe

```powershell
cmake -S . -B bin -G "Visual Studio 17 2022"
cmake --build bin
```

## Vulkan

> creating and submit commands in parallel
consistent shader compilation
unify graphic and compute functionality

### 1. Instance & Physical device

> Describe app and API extensions
Device's properties

### 2. Logical device & Queue families

> Describe physical device's features
Specify queue families for draw cmd, memory op

### 3. Window surface & Swap chain

> interact with window manager
provide which image to render or present

### 4. Image view

> image view refer part view of an image
framebuffer refer color, depth, stencil of an image

### 5. Dynamic rendering

> since 1.3, specify render attachment during cmd recording

### 6. Graphic pipeline

> specify which render pass is used by which render target in pipeline
recreate graphic pipeline in advance

### 7. Command pools & Command buffers

> don't need create for each image with dynamic rendering

### 8. Main loop

> acquire image -> select cmd buffer -> submit graphic queue -> present image

### Summary
1. vulkan instance based on windows system extension:
    - select physical device (gpu)
    - get graphics and present queue from gpu
    - create logical device

2. surface:
    - create swapchain, swapchain view 
        + surface width + height -> extent 
        + gpu -> format (color); present mode
        
3. pipeline: (describe how gpu tasks)
    - compile shader -> shader module (gpu binary)
    - describe gpu task
        + input assembly (connect vertex)
        + shader stages
        + dynamic state (runtime variable)
        + viewport (screen view)
        + rasterizer (divide into chunk)
        + multisampling
        + colorblending

4. buffers: data packet between cpu and gpu
    - command buffers: gpu instruction and data binding
        + bind pipeline
        + bind vertex buffer
        + bind descriptor sets

5. vertex buffers:
    - vertex shader input
    - staging buffer -> copy to cpu
    - vertex buffer -> copy from cpu to gpu 

6. index buffers:
    - reuse vertex

7. uniform buffers:
    - mutable variable during vertex shader stage.
    - bind uniform buffer to buffer descriptor

8. texture image:
    - copy image binaries data to buffer with transfer layout
    - create image view and sampler then bind them to image descriptor

9. depth buffering:
    - create depth image for render command's depth attachment
    - describe depth testing in pipeline's rasterizer

10. descriptor:
    - layout: reference the *immutable* data in shader
    - pool: holds descriptor set
    - set: instruct the binding for pipelines
