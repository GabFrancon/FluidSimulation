# IISPH
This is the code repository of a project from a Computer Graphics course at Télécom Paris (IGR205), implementing the [Implicit Incompressible SPH](https://cg.informatik.uni-freiburg.de/publications/2013_TVCG_IISPH.pdf) approach and a [surface reconstruction](https://www.cs.ubc.ca/~rbridson/docs/zhu-siggraph05-sandfluid.pd) method to make effective and efficient fluid simulation.

The code supports multi-threading on CPU using the openMP API. It also contains a basic render engine for visualization purpose, using the Vulkan graphics API. You can find the full report of the project [here](resources/Report.pdf).

<br>

## Results
For each scenario, you can see the particle simulation and surface reconstruction side by side, rendered with our basic Vulkan engine. We also present the simulation incorporated in a scene and rendered with Cycles (Blender's physic-based engine) for nicer results.

### Breaking dam scenario
![alt text](./results/gif/breaking_dam.gif)
![alt text](./results/gif/call_of_the_wolf.gif)

### Drop and splash scenario
![alt text](./results/gif/drop_and_splash.gif)
![alt text](./results/gif/drop_on_the_beach.gif)

### Fluid flow scenario
![alt text](./results/gif/fluid_flow.gif)
![alt text](./results/gif/glass_of_friendship.gif)
