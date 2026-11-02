# Optional data-generation tools

The application build consumes the complete source bundles and the checked-in collision assets. These scripts are development tools, not runtime dependencies.

They require the exact-build SchemaExplorer JSON, the locally installed game resources decoded with Source2Viewer, the current hero catalogue, and English localization. Paths in the PE-analysis/catalogue scripts currently refer to this development PC. Supply matching inputs and adjust those paths before regenerating another build. The supplied verification JSON records the current inputs and signatures; it is not a substitute for the schema/resource files.

`extract_collision.py input_PHYS_text output.rawtri` also produces primitive and collision-group metadata. Compile the bundled `collision.cpp` with `ACE_COLLISION_BAKER` to build a BVH from the raw triangle/primitive files and per-group masks. Do not reuse collision data or layouts across unverified game revisions.
