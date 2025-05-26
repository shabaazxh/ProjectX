## Vulkan Renderer
A simple Vulkan renderer with Forward and Deferred shading.
Deferred is the default and most developed. Use Keys: 9 and 0 to switch between Forward and Deferred.

This renderer is for me to experiment with various graphics techniques.

## Rendering features: 
* Physically Based Shading (GGX)
* Screen-Space Reflections (SSR)
* Screen-Space Ambient Occlusion (HBAO)
* Bloom (Gaussian Blur)

## Debug visuals (Requires enabling Forward renderer)
* Mip-map visual
* Partial Derivative visual 
* Linear depth
* Mesh density

<table>
    <tr>
        <td><img src="showcase/linear_depth.png", alt="Linear Depth", width="300"/></td>
	<td><img src="showcase/mip_visual.png", alt="Mip Visual", width="300"/></td>
	<td><img src="showcase/overdraw.png", alt="Overdraw visual", width="300"/></td>
	<td><img src="showcase/overshading.png", alt="Overshading", width="300"/></td>
	<td><img src="showcase/pd.png", alt="Partial Derivative visual", width="300"/></td>
    </tr>
</table>


## TODO:
* Indirect lighting solution 
* Bloom : (Next Generation Post Processing in Call of Duty)
