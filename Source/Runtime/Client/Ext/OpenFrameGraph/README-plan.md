# OpenFrameGraph
OpenFrameGraph (OpenFG or OFG) will be an open-source, general-purpose frame graph + RHI combo solution, acting as a drop-in solution for any modern 2D or 3D game engine needing to do rendering against
modern graphics APIs like D3D12/Vulkan/Metal and their NDA cousins (GNMx, NVM, etc.) without needing to be a complete pain in the ass.

Plan is to use a plugin-based architecture where each RHI implements itself as a plugin on the backend (D3D12/Vulkan/Metal coming stock, though Vulkan likely taking priority with D3D12 hot on its ass),
making it as easy as possible to add new backends as needed.

Same thing might be done for another library I want to create, OpenVFS (virtual file system), which would do the same thing but for virtual file systems.