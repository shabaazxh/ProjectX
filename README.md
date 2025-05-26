## ProjectX - Vulkan Renderer
A simple Vulkan renderer with Forward and Deferred shading.
Deferred is the default and most developed. Use Keys: 9 and 0 to switch between Forward and Deferred.

This renderer is for me to experiment with various graphics techniques.

![Hero Image](showcase/hero.png)

## Rendering features: 
* Physically Based Shading (GGX)
* Screen-Space Reflections (SSR)
* Screen-Space Ambient Occlusion (HBAO)
* Bloom (Gaussian Blur)

## Debug visuals (Requires enabling Forward renderer)

| ![](showcase/linear_depth.png) | ![](showcase/mip_visual.png) | ![](showcase/overdraw.png) | ![](showcase/overshading.png) | ![](showcase/pd.png)  | ![](showcase/mesh_density.png) |
|:-----------------------------:|:----------------------------:|:---------------------------:|:-----------------------------:|:---------------------:| :-----------------------------:|
| Linear Depth                  | Mip Visual                   | Overdraw                    | Overshading                   | Partial Derivative    | Mesh Density                   |

## TODO:
* Indirect lighting solution 
* Cascaded Shadow Maps
* Finish Volumetric Fog
* Bloom : (Next Generation Post Processing in Call of Duty)
