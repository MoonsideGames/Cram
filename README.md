This is Cram, a texture packing system in C.

About Cram
----------------
Cram is a portable C texture packing system intended for use in games, particularly 2D sprite games. Texture switching is an expensive operation, especially on low-end GPUs, so for performance it is imperative to pack sprites into textures to enable sprite batching.

Cram uses the maximal rectangles algorithm with the best area fit heuristic to pack your images. It automatically de-duplicates images to save space.

Cram ships with a default command line interface implemented in C, but if you wish you can configure CMake to build a shared library which will allow you to bind its essential functions to another language.

Command Line Usage
-----
```sh
Usage: cramcli input_dir output_dir atlas_name [--padding padding_value] [--notrim] [--dimension max_dimension]
```

Cram CLI expects input images to be in PNG format and will output a PNG and a JSON metadata file that you can use to properly display the images in your game. Cram will recursively walk all the subdirectories of `input_dir` to generate your texture atlas.

Padding is set to 0 by default. If you need to use linear filtering, set padding to at least 1. If you need to use texture compression, set padding to at least 4.

Trimming is on by default. Use `--notrim` if for some weird reason you want it off.

Max dimension value is set to 8192 by default since that is a common max texture size for basically every GPU out there. Use `--dimension [max_dimension]` to override this maximum.

API Usage
---------
If you would rather use Cram to build your own tool, you must configure CMake with BUILD_SHARED_LIBS set to ON so the library can be called. You can also set BUILD_CLI to OFF. See `cram.h` for the API; basically you are going to initialize the packing context, add the image files you want to pack, and call the pack function. Then you can get the pixel and packed image metadata. The pixel data is just RGBA32 colors - you are responsible for how you wish to write the pixel data to a file, and also for how you wish to serialize and deserialize the packer metadata. Make sure to call `Cram_Destroy` once you are done to free the native memory allocations.

Dependencies
------------
Cram depends on the C runtime.

libCram uses `stb_ds` for image hashing, and `stb_image` for image loading.

The CLI uses `stb_image_write` to output PNG images, and a portable `dirent.h` for a Windows-compatible dirent implemention.

Building Cram
-------------------
For *nix platforms, use CMake:

	$ mkdir build/
	$ cd build
	$ cmake ../
	$ make

For Windows, use CMake to generate a visualc project.

License
-------
Cram is licensed under the zlib license. See LICENSE for details.
